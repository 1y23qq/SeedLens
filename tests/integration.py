from pathlib import Path
import csv
import hashlib
import json
import struct
import subprocess
import time
import zlib

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'build-native/SeedLens.exe'
OUT = ROOT / 'build-native/verification'
OUT.mkdir(parents=True, exist_ok=True)
checks = []

def center_pixel(path):
    image=path.read_bytes();position=8;compressed=bytearray();width=height=bpp=0
    while position<len(image):
        length=struct.unpack('>I',image[position:position+4])[0]
        kind=image[position+4:position+8];data=image[position+8:position+8+length];position+=length+12
        if kind==b'IHDR':
            width,height=struct.unpack('>II',data[:8])
            assert data[8]==8 and data[9] in (2,6)
            bpp=4 if data[9]==6 else 3
        elif kind==b'IDAT':compressed.extend(data)
    raw=zlib.decompress(compressed);stride=width*bpp;previous=bytearray(stride);rows=[]
    for y in range(height):
        start=y*(stride+1);kind=raw[start];row=bytearray(raw[start+1:start+stride+1])
        for i in range(stride):
            left=row[i-bpp] if i>=bpp else 0;up=previous[i];diagonal=previous[i-bpp] if i>=bpp else 0
            if kind==1:predict=left
            elif kind==2:predict=up
            elif kind==3:predict=(left+up)//2
            elif kind==4:
                p=left+up-diagonal;distances=(abs(p-left),abs(p-up),abs(p-diagonal))
                predict=(left,up,diagonal)[distances.index(min(distances))]
            else:assert kind==0;predict=0
            row[i]=(row[i]+predict)&255
        rows.append(row);previous=row
    return tuple(rows[height//2][width//2*bpp:width//2*bpp+3])


def run(label, args, expected=0):
    start = time.perf_counter()
    result = subprocess.run([str(EXE), *map(str, args)], capture_output=True, timeout=120)
    if result.returncode != expected:
        raise AssertionError(f'{label}: exit {result.returncode}; {result.stderr.decode("utf-8", "replace")}')
    checks.append({'check': label, 'seconds': round(time.perf_counter() - start, 4)})


run('embedded executable self-test', ['--self-test', '--output', OUT / 'self-test.json'])
assert json.loads((OUT / 'self-test.json').read_text())['ok']
run('latest Java reference biome', ['--seed', '-3829811542736183482', '--query', 68148, 77, 80990, '--output', OUT / 'java.json'])
assert json.loads((OUT / 'java.json').read_text())['biome'] == 'Dappled Forest'
run('CLI options are order independent', ['--seed', -1, '--edition', 'bedrock', '--query', -17, 64, -17, '--output', OUT / 'negative.json'])
assert json.loads((OUT / 'negative.json').read_text())['seed'] == '-1'
run('oversized numeric seed is rejected', ['--seed', '9223372036854775808', '--query', 0, 64, 0], 1)
run('unavailable version is rejected', ['--version', '999', '--query', 0, 64, 0], 1)
run('oversized image is rejected', ['--export', OUT / 'invalid.png', '--width', 100000], 1)
run('Bedrock large-biome mode is rejected', ['--edition', 'bedrock', '--large-biomes', '--query', 0, 64, 0], 1)

for edition in ('java', 'bedrock'):
    for dimension in ('overworld', 'nether', 'end'):
        path = OUT / f'{edition}-{dimension}.png'
        run(f'{edition} {dimension} standalone PNG', ['--edition', edition, '--seed', '8675309', '--dimension', dimension,
            '--export', path, '--width', 512, '--height', 384, '--scale', 4])
        data = path.read_bytes()
        assert data[:8] == b'\x89PNG\r\n\x1a\n' and struct.unpack('>II', data[16:24]) == (512, 384)
        kind = {'overworld': 'village', 'nether': 'nether-fortress', 'end': 'end-city'}[dimension]
        csv_path = OUT / f'{edition}-{dimension}.csv'
        run(f'{edition} {dimension} standalone structure CSV', ['--edition', edition, '--seed', 8675309,
            '--dimension', dimension, '--scan', csv_path, '--kind', kind, '--radius', 4096])
        rows = list(csv.DictReader(csv_path.open(encoding='utf-8-sig', newline='')))
        assert rows and all(abs(int(r['x'])) <= 4096 and abs(int(r['z'])) <= 4096 for r in rows)
        if dimension == 'end':
            assert any(int(row['flags']) & 32 for row in rows), 'Expected some End cities with ships'

run('Java Nether fossils with terrain checks', ['--seed', 8675309, '--dimension', 'nether', '--scan', OUT / 'fossils.csv',
    '--kind', 'nether-fossil', '--radius', 512])
rows = list(csv.DictReader((OUT / 'fossils.csv').open(encoding='utf-8-sig', newline='')))
assert rows and all(32 < int(row['y']) <= 125 for row in rows)
run('Java structure version gate', ['--version', '1.8.9', '--scan', OUT / 'absent.csv', '--kind', 'trial-chambers'], 1)
run('Unicode export path and text seed', ['--seed', '森のシード', '--export', OUT / '日本語.png', '--width', 64, '--height', 64])
for label,args,expected in [
    ('Chunkbase Dappled Forest RGB',['--seed','-3829811542736183482','--at',68148,80990,'--y',77,'--scale',1],(154,63,53)),
    ('Chunkbase Nether Wastes RGB',['--version','1.18.2','--seed',12345,'--dimension','nether','--at',0,0,'--y',0,'--scale',1],(191,59,59)),
    ('Chunkbase End Highlands RGB',['--seed','856461443495910397','--dimension','end','--at',-371136,-64,'--scale',16],(195,189,137)),
]:
    path=OUT/(label.replace(' ','-')+'.png')
    run(label,[*args,'--export',path,'--width',3,'--height',3])
    assert center_pixel(path)==expected,(label,center_pixel(path),expected)
report = {'exe_bytes': EXE.stat().st_size, 'sha256': hashlib.sha256(EXE.read_bytes()).hexdigest(), 'checks': checks}
(OUT / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(f'PASS: {len(checks)} executable integration checks; {report["exe_bytes"]} bytes')
