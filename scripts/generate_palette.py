from pathlib import Path
import json,re

ROOT=Path(__file__).resolve().parents[1]
data=json.loads((ROOT/'assets/chunkbase-biome-colors.json').read_text())
ids={
    'Ocean':0,'Plains':1,'Desert':2,'Windswept Hills':3,'Forest':4,'Taiga':5,'Swamp':6,'River':7,
    'Nether Wastes':8,'The End':9,'Frozen Ocean':10,'Frozen River':11,'Snowy Plains':12,'Snowy Mountains':13,
    'Mushroom Fields':14,'Mushroom Fields Shore':15,'Beach':16,'Desert Hills':17,'Wooded Hills':18,'Taiga Hills':19,
    'Mountain Edge':20,'Jungle':21,'Jungle Hills':22,'Sparse Jungle':23,'Deep Ocean':24,'Stony Shore':25,
    'Snowy Beach':26,'Birch Forest':27,'Birch Forest Hills':28,'Dark Forest':29,'Snowy Taiga':30,'Snowy Taiga Hills':31,
    'Old Growth Pine Taiga':32,'Giant Tree Taiga Hills':33,'Windswept Forest':34,'Savanna':35,'Savanna Plateau':36,
    'Badlands':37,'Wooded Badlands':38,'Badlands Plateau':39,'Small End Islands':40,'End Midlands':41,
    'End Highlands':42,'End Barrens':43,'Warm Ocean':44,'Lukewarm Ocean':45,'Cold Ocean':46,'Deep Warm Ocean':47,
    'Deep Lukewarm Ocean':48,'Deep Cold Ocean':49,'Deep Frozen Ocean':50,'Sunflower Plains':129,'Desert Lakes':130,
    'Windswept Gravelly Hills':131,'Flower Forest':132,'Taiga Mountains':133,'Swamp Hills':134,'Ice Spikes':140,
    'Modified Jungle':149,'Modified Jungle Edge':151,'Old Growth Birch Forest':155,'Tall Birch Hills':156,
    'Dark Forest Hills':157,'Snowy Taiga Mountains':158,'Old Growth Spruce Taiga':160,'Giant Spruce Taiga Hills':161,
    'Gravelly Mountains+':162,'Windswept Savanna':163,'Shattered Savanna Plateau':164,'Eroded Badlands':165,
    'Modified Wooded Badlands Plateau':166,'Modified Badlands Plateau':167,'Bamboo Jungle':168,'Bamboo Jungle Hills':169,
    'Soul Sand Valley':170,'Crimson Forest':171,'Warped Forest':172,'Basalt Deltas':173,'Dripstone Caves':174,
    'Lush Caves':175,'Meadow':177,'Grove':178,'Snowy Slopes':179,'Frozen Peaks':180,'Jagged Peaks':181,
    'Stony Peaks':182,'Deep Dark':183,'Mangrove Swamp':184,'Cherry Grove':185,'Pale Garden':186,
    'Sulfur Caves':187,'Dappled Forest':188,'Ice Caves':189
}
ids['Wooded Mountains']=34
rows=[];missing=[]
for entry in data['colors']:
    name=re.sub(r'(pre-1\.18|unused|26\.4\+)$','',entry['name'])
    if name not in ids:missing.append(name);continue
    rgb=list(map(int,re.findall(r'\d+',entry['color'])))
    rows.append((ids[name],*rgb,'pre-1.18' in entry['name'],name))
if missing:raise RuntimeError('Unmapped legend entries: '+str(missing))
header='#pragma once\nnamespace seedlens {\nstruct PaletteEntry {int id,r,g,b;bool legacy;};\ninline constexpr PaletteEntry reference_palette[] = {\n'
header+=''.join(f'    {{{i},{r},{g},{b},{str(legacy).lower()}}},\n' for i,r,g,b,legacy,name in sorted(rows))
header+='};\n}\n'
(ROOT/'src/biome_palette.h').write_text(header,encoding='utf-8')
print('Generated',len(rows),'verified biome colours')
