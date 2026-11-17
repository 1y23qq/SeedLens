from pathlib import Path
import hashlib
import io
import json
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.cache/svg-runtime'))
from PIL import Image

CLIENT_SHA1 = 'e877b6a07acd633fb3bb475002175cec036e7b87'
CLIENT_URL = f'https://piston-data.mojang.com/v1/objects/{CLIENT_SHA1}/client.jar'


def main():
    client = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / '.cache/reference/minecraft-26.3.jar'
    if hashlib.sha1(client.read_bytes()).hexdigest() != CLIENT_SHA1:
        raise RuntimeError('Expected the official Minecraft Java 26.3 client resources.')
    archive = zipfile.ZipFile(client)
    sources = {}

    def texture(path):
        full = 'assets/minecraft/textures/' + path + '.png'
        raw = archive.read(full)
        sources[path] = {'path': full, 'sha256': hashlib.sha256(raw).hexdigest()}
        return Image.open(io.BytesIO(raw)).convert('RGBA')

    def flat(path):
        im = texture(path)
        return im.crop((0, 0, im.width, im.width)).resize((24, 24), Image.Resampling.NEAREST)

    def item(path):
        return flat('item/' + path)

    def block(path):
        return flat('block/' + path)

    def head(path, height=10, width=8, nose=True):
        im = texture('entity/' + path)
        front = im.crop((8, 8, 8 + width, 8 + height))
        front = front.resize((width * 2, height * 2), Image.Resampling.NEAREST)
        out = Image.new('RGBA', (24, 24))
        out.alpha_composite(front, ((24-front.width)//2, (24-front.height)//2))
        if nose:
            n = im.crop((26, 2, 28, 6)).resize((4, 8), Image.Resampling.NEAREST)
            out.alpha_composite(n, (10, 11))
        return out

    def chest():
        im = texture('entity/chest/normal')
        out = Image.new('RGBA', (14, 15))
        out.paste(im.crop((14, 14, 28, 19)), (0, 0))
        out.paste(im.crop((14, 33, 28, 43)), (0, 5))
        out.alpha_composite(im.crop((1, 1, 3, 5)), (6, 3))
        result = Image.new('RGBA', (24, 24))
        result.alpha_composite(out.resize((22, 23), Image.Resampling.NEAREST), (1, 0))
        return result

    def witch():
        im=texture('entity/witch/witch')
        result=Image.new('RGBA',(24,24))
        result.alpha_composite(im.crop((8,8,16,18)).resize((14,16),Image.Resampling.NEAREST),(5,8))
        result.alpha_composite(im.crop((26,2,28,6)).resize((4,6),Image.Resampling.NEAREST),(10,15))
        result.alpha_composite(im.crop((10,74,20,76)).resize((22,3),Image.Resampling.NEAREST),(1,7))
        result.alpha_composite(im.crop((7,83,14,87)).resize((14,6),Image.Resampling.NEAREST),(5,2))
        result.alpha_composite(im.crop((4,91,8,95)).resize((7,4),Image.Resampling.NEAREST),(9,0))
        return result

    sprites = [
        head('villager/villager'),
        block('chiseled_sandstone'),
        block('mossy_cobblestone'),
        witch(),
        block('furnace_front'),
        block('prismarine_bricks'),
        head('illager/evoker'),
        head('illager/pillager'),
        block('nether_portal'),
        block('nether_portal'),
        item('soul_lantern'),
        chest(),
        block('rail'),
        block('end_portal_frame_top'),
        item('oak_boat'),
        block('chiseled_stone_bricks'),
        item('brush'),
        block('trial_spawner_side_active'),
        item('campfire'),
        head('blaze/blaze', 8, 8, False),
        head('piglin/piglin', 8, 10, False),
        item('bone'),
        block('purpur_block'),
        item('ender_pearl'),
        block('end_stone'),
        item('water_bucket'),
        block('amethyst_cluster'),
        block('spawner'),
        block('stone'),
        block('lava_still'),
        item('bone'),
        head('zombie_villager/zombie_villager'),
        item('elytra'),
        item('compass_16'),
        item('slime_ball'),
    ]
    atlas = Image.new('RGBA', (256, 160))
    for i, image in enumerate(sprites):
        atlas.alpha_composite(image, ((i % 8)*32+4, (i//8)*32+4))
    atlas.save(ROOT / 'assets/minecraft-markers.png', optimize=True)
    metadata = {'minecraft': '26.3', 'client_sha1': CLIENT_SHA1, 'client_url': CLIENT_URL,
                'copyright': 'Minecraft textures © Mojang AB / Microsoft. Not covered by the SeedLens MIT license.',
                'sources': sources}
    (ROOT / 'assets/minecraft/sources.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
    (ROOT / 'src/minecraft_icons.h').write_text('''#pragma once
#include "backend.h"
namespace seedlens::minecraft {
inline int sprite_index(int kind, unsigned flags = 0) {
    if (kind == SL_VILLAGE && (flags & SL_ABANDONED)) return 31;
    if (kind == SL_END_CITY && (flags & SL_END_SHIP)) return 32;
    if (kind == -1) return 33;
    return kind >= 0 && kind < SL_KIND_COUNT ? kind : 33;
}
inline constexpr int columns = 8, width = 256, height = 160, cell = 32, inset = 4, content = 24;
}
''', encoding='utf-8')
    print(f'Packed {len(sprites)} Minecraft texture markers: {(ROOT / "assets/minecraft-markers.png").stat().st_size} bytes')


if __name__ == '__main__':
    main()
