import argparse
import concurrent.futures
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
from embed_assets import generate

ROOT=Path(__file__).resolve().parents[1]
CACHE=Path(os.environ.get('SEEDLENS_MOBILE_CACHE',Path.home()/'.cache/seedlens-mobile'))
BUILD=ROOT/'build-mobile/android'
WINDOWS=sys.platform=='win32'
SUFFIX='.exe' if WINDOWS else ''
sys.stdout.reconfigure(encoding='utf-8',errors='replace')
sys.stderr.reconfigure(encoding='utf-8',errors='replace')

def run(args,env=None):
    result=subprocess.run(list(map(str,args)),cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace')
    if result.returncode:raise RuntimeError(result.stdout+result.stderr)
    return result.stdout

def relative(path):
    path=Path(path)
    return path.relative_to(ROOT) if path.is_relative_to(ROOT) else path

def find_tools():
    sdk=Path(os.environ.get('ANDROID_HOME',os.environ.get('ANDROID_SDK_ROOT',CACHE)))
    if (sdk/'build-tools/36.0.0').exists():
        tools=sdk/'build-tools/36.0.0';android_jar=sdk/'platforms/android-36/android.jar'
    else:
        tools=next((CACHE/'sdk-build-tools-36.0.0').rglob('aapt2.exe')).parent
        android_jar=next((CACHE/'sdk-platforms-android-36').rglob('android.jar'))
    ndk_env=os.environ.get('ANDROID_NDK_HOME') or os.environ.get('ANDROID_NDK_ROOT')
    if ndk_env:ndk=Path(ndk_env)
    elif (CACHE/'ndk/android-ndk-r30').exists():ndk=CACHE/'ndk/android-ndk-r30'
    else:ndk=sorted((sdk/'ndk').iterdir())[-1]
    prebuilt=next((ndk/'toolchains/llvm/prebuilt').iterdir())
    sdl=Path(os.environ.get('SDL_ANDROID_AAR',CACHE/'sdl-android/SDL3-3.4.20.aar'))
    unpack=CACHE/'sdl-aar'
    if not (unpack/'classes.jar').exists():
        unpack.mkdir(parents=True,exist_ok=True)
        with zipfile.ZipFile(sdl) as z:z.extractall(unpack)
    include=next(unpack.rglob('SDL.h')).parent.parent
    return tools,android_jar,prebuilt,unpack,include

def compile_native(abi,prebuilt,sdl,include):
    triples={'arm64-v8a':'aarch64-linux-android','armeabi-v7a':'armv7a-linux-androideabi','x86_64':'x86_64-linux-android'}
    target=triples[abi]+'26'
    clang=prebuilt/'bin'/('clang'+SUFFIX);cxx=prebuilt/'bin'/('clang++'+SUFFIX)
    output=BUILD/abi;output.mkdir(parents=True,exist_ok=True)
    resource=output/'resources.cpp';generate(resource)
    java=ROOT/'third_party/cubiomes_java'
    bedrock=ROOT/'third_party/cubiomes_bedrock'
    files=[(p,False,False) for p in java.rglob('*.c') if p.name!='tests.c' and 'tests' not in p.relative_to(java).parts]
    files += [(ROOT/'src/backend_java.c',False,False)]
    files += [(p,True,False) for p in bedrock.glob('*.c') if p.name!='tests.c']+[(ROOT/'src/backend_bedrock.c',True,False)]
    files += [(ROOT/p,False,False) for p in [
        'src/engine.cpp','src/persistence.cpp','src/export_common.cpp','src/export_sdl.cpp',
        'src/ui.cpp','src/mobile_main.cpp','src/mobile_platform.cpp',
        'third_party/imgui/imgui.cpp','third_party/imgui/imgui_draw.cpp','third_party/imgui/imgui_tables.cpp','third_party/imgui/imgui_widgets.cpp',
        'third_party/imgui/backends/imgui_impl_sdl3.cpp','third_party/imgui/backends/imgui_impl_sdlrenderer3.cpp']]
    files += [(resource,False,False),(ROOT/'tests/core_tests.cpp',False,True)]
    newest=max(p.stat().st_mtime for folder in (ROOT/'src',ROOT/'third_party') for p in folder.rglob('*.h'))
    newest=max(newest,Path(__file__).stat().st_mtime)
    common=['--target='+target,'--sysroot='+str(prebuilt/'sysroot'),'-fPIC','-O2','-fwrapv','-fno-strict-aliasing','-ffp-contract=off',
        '-ffunction-sections','-fdata-sections','-fvisibility=hidden','-DNOMINMAX',
        '-ffile-prefix-map='+str(ROOT)+'=.','-ffile-prefix-map='+ROOT.as_posix()+'=.',
        '-ffile-prefix-map='+str(CACHE)+'=mobile-sdk','-ffile-prefix-map='+CACHE.as_posix()+'=mobile-sdk']
    includes=[ROOT/'src',ROOT/'third_party/imgui',ROOT/'third_party/imgui/backends',include]
    jobs=[];objects=[]
    for source,bed,test in files:
        obj=output/(source.stem+'_'+hashlib.sha1(str(source).encode()).hexdigest()[:8]+'.o');objects.append(obj)
        if obj.exists() and obj.stat().st_mtime>=max(newest,source.stat().st_mtime):continue
        args=[clang if source.suffix=='.c' else cxx,*common,'-std=gnu11' if source.suffix=='.c' else '-std=c++20']
        if source.suffix!='.c':args+=['-fno-rtti']
        args += ['-I'+str(p) for p in includes]
        if bed:args+=['-include',ROOT/'src/bedrock_symbols.h']
        if test:args+=['-Dmain=seedlens_core_tests_main']
        if source.name in ('ui.cpp','export_sdl.cpp') or 'imgui' in source.parts:args+=['-Os']
        args+=['-w','-c',source,'-o',obj];jobs.append(args)
    print(f'Compiling Android {abi}: {len(jobs)} files',flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        for future in concurrent.futures.as_completed([pool.submit(run,args) for args in jobs]):future.result()
    library=next((sdl/'prefab').rglob('android.'+abi+'/libSDL3.so'))
    result=output/'libmain.so'
    run([cxx,*common,'-shared','-static-libstdc++','-Wl,--gc-sections,-z,max-page-size=16384,-soname,libmain.so',
        '-o',result,*objects,library,'-landroid','-llog','-ldl','-lm','-latomic'])
    run([prebuilt/'bin'/('llvm-strip'+SUFFIX),'--strip-unneeded',result])
    return result,library

def sign(apk,tools,destination):
    signing=Path(os.environ.get('SEEDLENS_SIGNING_DIR',CACHE/'signing'))
    signing.mkdir(parents=True,exist_ok=True)
    key=signing/'seedlens-android.jks';password_file=signing/'password.txt'
    if not password_file.exists():password_file.write_text(os.urandom(32).hex(),encoding='utf-8')
    password=password_file.read_text(encoding='utf-8').strip()
    env=dict(os.environ);env['SEEDLENS_SIGNING_PASSWORD']=password
    if not key.exists():
        run(['keytool','-genkeypair','-keystore',key,'-alias','seedlens','-keyalg','RSA','-keysize','4096','-validity','10000',
            '-storepass:env','SEEDLENS_SIGNING_PASSWORD','-keypass:env','SEEDLENS_SIGNING_PASSWORD','-dname','CN=SeedLens, OU=Independent Software, O=1y23qq, C=JP'],env)
    result=destination.resolve();result.parent.mkdir(parents=True,exist_ok=True)
    run(['java','-jar',tools/'lib/apksigner.jar','sign','--ks',key,'--ks-key-alias','seedlens','--ks-pass','env:SEEDLENS_SIGNING_PASSWORD',
        '--key-pass','env:SEEDLENS_SIGNING_PASSWORD','--out',result,apk],env)
    run(['java','-jar',tools/'lib/apksigner.jar','verify','--verbose',result])
    return result

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--abi',nargs='+',choices=['arm64-v8a','armeabi-v7a','x86_64'],default=['arm64-v8a'])
    parser.add_argument('--output',type=Path,default=ROOT/'dist/SeedLens.apk')
    args=parser.parse_args()
    tools,android_jar,prebuilt,sdl,include=find_tools()
    libraries={}
    for abi in args.abi:libraries[abi]=compile_native(abi,prebuilt,sdl,include)
    BUILD.mkdir(parents=True,exist_ok=True)
    res=BUILD/'res';(res/'drawable').mkdir(parents=True,exist_ok=True)
    shutil.copyfile(ROOT/'mobile/android/icon.png',res/'drawable/icon.png')
    compiled=BUILD/'resources.zip';run([tools/('aapt2'+SUFFIX),'compile','--dir',relative(res),'-o',relative(compiled)])
    base=BUILD/'base.apk'
    run([tools/('aapt2'+SUFFIX),'link','-o',relative(base),'-I',android_jar,'--manifest',relative(ROOT/'mobile/android/AndroidManifest.xml'),
        '--auto-add-overlay',relative(compiled)])
    classes=BUILD/'classes';classes.mkdir(exist_ok=True)
    source=ROOT/'mobile/android/src/com/seedlens/app/SeedLensActivity.java'
    classpath=str(android_jar)+os.pathsep+str(sdl/'classes.jar')
    run(['javac','-encoding','UTF-8','-source','8','-target','8','-classpath',classpath,'-d',classes,source])
    dex=BUILD/'dex';dex.mkdir(exist_ok=True)
    run(['java','-cp',tools/'lib/d8.jar','com.android.tools.r8.D8','--release','--min-api','26','--lib',android_jar,
        '--output',dex,sdl/'classes.jar',*classes.rglob('*.class')])
    unsigned=BUILD/'SeedLens-unsigned.apk';shutil.copyfile(base,unsigned)
    with zipfile.ZipFile(unsigned,'a',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for file in dex.glob('classes*.dex'):z.write(file,file.name)
        for abi,(main_lib,sdl_lib) in libraries.items():
            z.write(main_lib,f'lib/{abi}/libmain.so');z.write(sdl_lib,f'lib/{abi}/libSDL3.so')
    aligned=BUILD/'SeedLens-aligned.apk'
    run([tools/('zipalign'+SUFFIX),'-f','4',relative(unsigned),relative(aligned)])
    result=sign(aligned,tools,args.output)
    print('Built signed APK:',result,result.stat().st_size,'bytes',flush=True)

if __name__=='__main__':
    try:main()
    except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)
