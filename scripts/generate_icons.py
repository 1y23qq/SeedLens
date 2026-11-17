from pathlib import Path
import math
import struct
import sys
import urllib.request
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.cache/svg-runtime'))
from svg.path import parse_path, Move, Close
from PIL import Image, ImageDraw

NAMES = ['map', 'layers', 'search', 'map-pin', 'bookmark', 'image', 'download', 'copy', 'check', 'x', 'plus', 'minus', 'crosshair', 'locate-fixed', 'house', 'pyramid', 'landmark', 'castle', 'flag', 'door-open', 'building-2', 'gem', 'pickaxe', 'ship', 'waves', 'shovel', 'vault', 'tent', 'bone', 'mountain', 'droplets', 'box', 'flame', 'shuffle', 'settings-2', 'info', 'chevron-down', 'arrow-up-right', 'circle-help', 'compass', 'grid-2x2', 'maximize', 'keyboard', 'folder-open', 'sun', 'moon', 'arrow-right', 'chevron-left', 'chevron-right', 'refresh-cw']
REVISION = 'f8feea7a02ea461c9f2a4b4d851512aae6ec933f'


def paths_for(root):
    paths = []
    for el in root:
        tag = el.tag.split('}')[-1]
        if tag == 'path':
            current = []
            for segment in parse_path(el.attrib['d']):
                if isinstance(segment, Move):
                    if current:
                        paths.append(current)
                    current = [(segment.end.real, segment.end.imag)]
                else:
                    steps = 1 if segment.__class__.__name__ in ('Line', 'Close') else 14
                    for i in range(1, steps + 1):
                        point = segment.point(i / steps)
                        current.append((point.real, point.imag))
            if current:
                paths.append(current)
        elif tag in ('circle', 'ellipse'):
            cx, cy = float(el.get('cx', 0)), float(el.get('cy', 0))
            rx, ry = float(el.get('r', el.get('rx', 0))), float(el.get('r', el.get('ry', 0)))
            paths.append([(cx + math.cos(i * math.tau / 36) * rx, cy + math.sin(i * math.tau / 36) * ry) for i in range(37)])
        elif tag in ('polyline', 'polygon'):
            values = el.attrib['points'].replace(',', ' ').split()
            pts = [(float(values[i]), float(values[i + 1])) for i in range(0, len(values), 2)]
            if tag == 'polygon':
                pts.append(pts[0])
            paths.append(pts)
        elif tag == 'line':
            paths.append([(float(el.get('x1', 0)), float(el.get('y1', 0))), (float(el.get('x2', 0)), float(el.get('y2', 0)))])
        elif tag == 'rect':
            x, y, w, h = (float(el.get(a, 0)) for a in ('x', 'y', 'width', 'height'))
            r = min(float(el.get('rx', 0)), w / 2, h / 2)
            if r:
                points = []
                for cx, cy, start in ((x+w-r,y+r,-90),(x+w-r,y+h-r,0),(x+r,y+h-r,90),(x+r,y+r,180)):
                    points += [(cx + r*math.cos(math.radians(start+i*90/8)), cy + r*math.sin(math.radians(start+i*90/8))) for i in range(9)]
                paths.append(points + [points[0]])
            else:
                paths.append([(x,y),(x+w,y),(x+w,y+h),(x,y+h),(x,y)])
    return paths


def main():
    folder = ROOT / 'assets/lucide'
    folder.mkdir(parents=True, exist_ok=True)
    all_paths = {}
    for name in NAMES:
        path = folder / (name + '.svg')
        if not path.exists():
            source = {'building-2': 'building', 'circle-help': 'circle-question-mark', 'waves': 'waves-horizontal'}.get(name, name)
            urllib.request.urlretrieve(f'https://raw.githubusercontent.com/lucide-icons/lucide/{REVISION}/icons/{source}.svg', path)
        all_paths[name] = paths_for(ET.parse(path).getroot())
    license_path = folder / 'LICENSE'
    if not license_path.exists():
        urllib.request.urlretrieve(f'https://raw.githubusercontent.com/lucide-icons/lucide/{REVISION}/LICENSE', license_path)
    output = ['#pragma once', '#include <cstring>', 'namespace seedlens::lucide {', 'struct Path { const float *xy; int count; };', 'struct Icon { const char *name; const Path *paths; int count; };']
    for name, paths in all_paths.items():
        symbol = name.replace('-', '_')
        for i, points in enumerate(paths):
            output.append(f'inline const float {symbol}_{i}[] = {{' + ','.join(f'{v:.4f}f' for p in points for v in p) + '};')
        output.append(f'inline const Path {symbol}_paths[] = {{' + ','.join(f'{{{symbol}_{i},{len(p)}}}' for i,p in enumerate(paths)) + '};')
    output.append('inline const Icon icons[] = {')
    output += [f'{{"{name}",{name.replace("-", "_")}_paths,{len(paths)}}},' for name,paths in all_paths.items()]
    output += ['};', 'inline const Icon *find(const char *name) { for (const auto &i : icons) if (!std::strcmp(i.name,name)) return &i; return &icons[0]; }', '}']
    (ROOT / 'src/lucide_icons.h').write_text('\n'.join(output) + '\n', encoding='utf-8')
    images = []
    for size in (16,24,32,48,64,128,256):
        scale = 4
        image = Image.new('RGBA', (size*scale, size*scale))
        draw = ImageDraw.Draw(image)
        draw.rounded_rectangle((0,0,size*scale-1,size*scale-1), radius=size*scale*0.22, fill='#202f2b')
        pad = size*scale*0.17
        factor = size*scale*0.66/24
        for points in all_paths['map']:
            pts = [(pad+x*factor,pad+y*factor) for x,y in points]
            width = max(2, round(1.65*factor))
            draw.line(pts, fill='#b8e6c8', width=width, joint='curve')
            for x,y in (pts[0],pts[-1]):
                r=width/2
                draw.ellipse((x-r,y-r,x+r,y+r), fill='#b8e6c8')
        images.append(image.resize((size,size), Image.Resampling.LANCZOS))
    images[-1].save(ROOT / 'assets/SeedLens.ico', format='ICO', append_images=images[:-1], sizes=[(s,s) for s in (16,24,32,48,64,128,256)])
    print(f'Generated {len(all_paths)} Lucide icons and the application icon.')


if __name__ == '__main__':
    main()
