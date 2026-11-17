import argparse
import concurrent.futures
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
sys.stderr.reconfigure(encoding='utf-8', errors='replace')
ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build-native'
TOOLCHAIN = Path(os.environ.get('SEEDLENS_TOOLCHAIN', ROOT / '.cache/toolchain/w64devkit/bin'))


def tool(name):
    local = TOOLCHAIN / (name + '.exe')
    found = str(local) if local.exists() else shutil.which(name)
    if not found:
        raise RuntimeError('Install w64devkit and set SEEDLENS_TOOLCHAIN to its bin directory.')
    return found


def run(args):
    proc = subprocess.run([str(a) for a in args], cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    if proc.returncode:
        raise RuntimeError(proc.stdout + proc.stderr)
    return proc.stdout


def compile_group(name, files, force_include=None, extra=()):
    out = BUILD / name
    out.mkdir(parents=True, exist_ok=True)
    headers = [p for root in (ROOT / 'src', ROOT / 'third_party') for p in root.rglob('*.h*')]
    newest_header = max([p.stat().st_mtime for p in headers] + [Path(__file__).stat().st_mtime])
    objects, jobs = [], []
    for source in files:
        obj = out / (source.stem + '_' + hashlib.sha1(str(source.relative_to(ROOT)).encode()).hexdigest()[:8] + '.o')
        objects.append(obj)
        if obj.exists() and obj.stat().st_mtime >= max(source.stat().st_mtime, newest_header):
            continue
        args = [tool('gcc' if source.suffix == '.c' else 'g++'), '-c', '-O2', '-fmerge-all-constants', '-ffunction-sections', '-fdata-sections', '-fwrapv', '-fno-strict-aliasing', '-ffp-contract=off', '-DNOMINMAX', '-DWIN32_LEAN_AND_MEAN', '-DUNICODE', '-D_UNICODE', '-D_CRT_SECURE_NO_WARNINGS']
        args += ['-std=gnu11'] if source.suffix == '.c' else ['-std=c++20', '-fno-rtti', '-Wall', '-Wextra']
        args += [f'-ffile-prefix-map={ROOT}=.', f'-ffile-prefix-map={ROOT.as_posix()}=.']
        args += [f'-I{ROOT / p}' for p in ('src', 'third_party/imgui', 'third_party/imgui/backends')]
        if force_include:
            args += ['-include', force_include]
        args += list(extra) + ['-o', obj, source]
        jobs.append(args)
    if jobs:
        print(f'Compiling {name}: {len(jobs)} files', flush=True)
        with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
            futures = [pool.submit(run, args) for args in jobs]
            for future in concurrent.futures.as_completed(futures):
                try:
                    future.result()
                except Exception:
                    for pending in futures:
                        pending.cancel()
                    raise
    return objects


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', action='store_true')
    parser.add_argument('--upstream-tests', action='store_true')
    args = parser.parse_args()
    BUILD.mkdir(exist_ok=True)
    java_root = ROOT / 'third_party/cubiomes_java'
    bedrock_root = ROOT / 'third_party/cubiomes_bedrock'
    if args.probe:
        (ROOT / 'src/bedrock_symbols.h').write_text('#pragma once\n', encoding='utf-8')
    java = [p for p in java_root.rglob('*.c') if p.name != 'tests.c' and 'tests' not in p.relative_to(java_root).parts]
    bedrock = [p for p in bedrock_root.glob('*.c') if p.name != 'tests.c']
    if not args.probe:
        java += [ROOT / 'src/backend_java.c']
        bedrock += [ROOT / 'src/backend_bedrock.c']
    jobj = compile_group('java', java, extra=['-w'])
    bobj = compile_group('bedrock', bedrock, ROOT / 'src/bedrock_symbols.h', ['-w'])
    for archive, objects in ((BUILD / 'java.a', jobj), (BUILD / 'bedrock.a', bobj)):
        archive.unlink(missing_ok=True)
        run([tool('ar'), 'rcs', archive, *objects])
    if args.probe:
        symbols = run([tool('nm'), '-g', '--defined-only', BUILD / 'bedrock.a'])
        names = sorted(name for name in set(re.findall(r'^[0-9a-fA-F]+ [TDBRCG] ([A-Za-z_]\w*)$', symbols, re.M)) if not name.startswith('btree'))
        header = '#pragma once\n' + ''.join(f'#define {name} slb_{name}\n' for name in names)
        (ROOT / 'src/bedrock_symbols.h').write_text(header, encoding='utf-8')
        print(f'World-generation libraries compiled; isolated {len(names)} Bedrock symbols.')
        return
    imgui_root = ROOT / 'third_party/imgui'
    imgui = [imgui_root / s for s in ['imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp', 'backends/imgui_impl_win32.cpp', 'backends/imgui_impl_dx11.cpp']]
    iobj = compile_group('imgui', imgui, extra=['-w', '-Os'])
    core = compile_group('core', [ROOT / 'src/engine.cpp', ROOT / 'src/persistence.cpp'])
    ui = compile_group('app', [ROOT / 'src/main.cpp', ROOT / 'src/ui.cpp', ROOT / 'src/export.cpp'], extra=['-Os'])
    res = BUILD / 'app.o'
    run([tool('windres'), '-Iassets', '-i', ROOT / 'assets/app.rc', '-o', res])
    libs = [BUILD / 'java.a', BUILD / 'bedrock.a']
    system = ['-ld3d11', '-ldxgi', '-ld3dcompiler', '-ldwmapi', '-lcomdlg32', '-lwindowscodecs', '-lole32', '-lshell32', '-luser32', '-lgdi32', '-limm32', '-ladvapi32', '-luuid']
    exe = BUILD / 'SeedLens.exe'
    flags = ['-static', '-s', '-Wl,--gc-sections,--stack,8388608,--no-insert-timestamp']
    run([tool('g++'), *flags, '-municode', '-mwindows', '-o', exe, *ui, *core, *iobj, res, *libs, *system])
    tests = compile_group('tests', [ROOT / 'tests/core_tests.cpp'])
    test_exe = BUILD / 'seedlens_tests.exe'
    run([tool('g++'), *flags, '-o', test_exe, *tests, *core, *libs, '-lole32', '-lshell32', '-luuid'])
    print(run([test_exe]), end='')
    if args.upstream_tests:
        for source in (java_root / 'tests').glob('test_*.c'):
            objs = compile_group('upstream', [source], extra=['-w'])
            binary = BUILD / (source.stem + '.exe')
            run([tool('gcc'), *flags, '-o', binary, *objs, BUILD / 'java.a'])
            print(run([binary]), end='')
    print(f'Built {exe} ({exe.stat().st_size / 1048576:.2f} MiB)', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
