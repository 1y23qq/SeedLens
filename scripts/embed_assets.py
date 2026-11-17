from pathlib import Path
import argparse

ROOT=Path(__file__).resolve().parents[1]

def generate(output):
    resources={200:ROOT/'THIRD_PARTY_NOTICES.txt',300:ROOT/'assets/minecraft-markers.png',
               401:ROOT/'assets/fonts/NotoSansJP-Regular.bin',402:ROOT/'assets/fonts/NotoSansJP-SemiBold.bin'}
    lines=['#include "platform.h"','namespace seedlens {']
    for key,path in resources.items():
        data=path.read_bytes()
        lines.append(f'alignas(4) static const uint8_t resource_{key}[]={{')
        lines.extend(','.join(map(str,data[i:i+48]))+',' for i in range(0,len(data),48))
        lines.append('};')
    lines+=['std::span<const uint8_t> embedded_resource(int id){','switch(id){']
    lines.extend(f'case {key}:return resource_{key};' for key in resources)
    lines+=['default:return {};','}','}','}']
    value='\n'.join(lines)+'\n'
    output.parent.mkdir(parents=True,exist_ok=True)
    if not output.exists() or output.read_text(encoding='utf-8')!=value:output.write_text(value,encoding='utf-8')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('output',type=Path);args=parser.parse_args();generate(args.output)
