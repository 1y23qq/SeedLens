from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.cache/font-runtime'))
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
from fontTools import subset

source=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'.cache/reference/NotoSansJP-variable.ttf'
ui=''.join(p.read_text(encoding='utf-8') for p in (ROOT/'src').glob('*') if p.suffix in ('.cpp','.h','.c'))
basic=set(range(0x20,0x250))|set(range(0x2000,0x2070))|set(range(0x3000,0x3100))|set(range(0xff00,0xfff0))|{0xfffd}
common=set()
for lead in range(0xa1,0xd0):
    for trail in range(0xa1,0xff):
        try: common.update(ord(c) for c in bytes([lead,trail]).decode('euc_jp'))
        except UnicodeDecodeError: pass
for weight,style,chars in ((400,'Regular',basic|common|set(map(ord,ui))),(600,'SemiBold',basic|set(map(ord,ui)))):
    font=instantiateVariableFont(TTFont(source),{'wght':weight},inplace=False)
    font['hhea'].ascent=font['OS/2'].sTypoAscender
    font['hhea'].descent=font['OS/2'].sTypoDescender
    font['hhea'].lineGap=0
    for record in font['name'].names:
        if record.nameID in (1,16):value='Noto Sans JP'
        elif record.nameID in (2,17):value=style
        elif record.nameID==4:value='Noto Sans JP '+style
        elif record.nameID==6:value='NotoSansJP-'+style
        else:continue
        record.string=value.encode(record.getEncoding())
    options=subset.Options()
    options.layout_features=['*']
    options.name_IDs=[0,1,2,3,4,5,6,13,14,16,17]
    sub=subset.Subsetter(options=options)
    sub.populate(unicodes=chars)
    sub.subset(font)
    path=ROOT/f'assets/fonts/NotoSansJP-{style}.ttf'
    font.save(path)
    print(style,len(font.getBestCmap()),path.stat().st_size,'bytes')
