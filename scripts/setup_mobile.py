import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import urllib.request
import zipfile

CACHE=Path(os.environ.get('SEEDLENS_MOBILE_CACHE',Path.home()/'.cache/seedlens-mobile'))
ROOT=Path(__file__).resolve().parents[1]

def fetch(name,url,sha1=None):
    file=CACHE/name
    if not file.exists():
        print('Downloading '+name,flush=True)
        urllib.request.urlretrieve(url,file)
    if sha1 and hashlib.sha1(file.read_bytes()).hexdigest()!=sha1:
        raise RuntimeError('Checksum mismatch: '+name)
    return file

def install(name,url,dest,sha1=None):
    marker=dest/'.seedlens-installed'
    if marker.exists():return
    file=fetch(name,url,sha1)
    print('Extracting '+name,flush=True)
    dest.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(file) as archive:
        base=dest.resolve()
        for item in archive.infolist():
            target=(dest/item.filename).resolve()
            if not target.is_relative_to(base):raise RuntimeError('Unsafe archive path')
        archive.extractall(dest)
    marker.write_text(name,encoding='utf-8')

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--emulator',action='store_true')
    args=parser.parse_args()
    CACHE.mkdir(parents=True,exist_ok=True)
    packages=json.loads((CACHE/'packages.json').read_text(encoding='utf-8'))
    wanted=['platforms;android-36','build-tools;36.0.0','platform-tools']
    if args.emulator:wanted+=['emulator']
    jobs=[
        ('android-ndk-r30-windows.zip','https://dl.google.com/android/repository/android-ndk-r30-windows.zip',CACHE/'ndk','9bf167a1985fa7d4a036186b78f702eab9179408'),
        ('SDL3-3.4.20-android.zip','https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-devel-3.4.20-android.zip',CACHE/'sdl-android',None),
        ('SDL3-3.4.20.zip','https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.zip',CACHE/'sdl-source',None),
    ]
    if args.emulator:
        jobs.append(('aosp-x86_64-35_r02.zip','https://dl.google.com/android/repository/sys-img/android/x86_64-35_r02.zip',CACHE/'system-image','2d857d170c0d1b827149565da34b3383e5306f7f'))
    for package in wanted:
        info=next(p for p in packages if p['package']==package)
        jobs.append((info['url'].rsplit('/',1)[-1],info['url'],CACHE/('sdk-'+package.replace(';','-')),info['checksum']))
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        futures=[pool.submit(install,*job) for job in jobs]
        for f in concurrent.futures.as_completed(futures):f.result()
    print('Mobile tools ready: '+str(CACHE),flush=True)

if __name__=='__main__':main()
