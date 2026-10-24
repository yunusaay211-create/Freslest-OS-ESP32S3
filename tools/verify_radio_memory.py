"""Verify radio PSRAM placement in the final OS ELF/map (needs pyelftools)."""
from pathlib import Path
from collections import Counter
from elftools.elf.elffile import ELFFile
import argparse, json, re
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--elf',type=Path,default=r/'OS/build/freslest_os_s3.elf')
p.add_argument('--map',type=Path)
p.add_argument('--output',type=Path)
a=p.parse_args();mp=a.map or a.elf.with_suffix('.map')
config=(r/'OS/sdkconfig').read_text().splitlines()
for name in ['CONFIG_SPIRAM_USE_MALLOC','CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY','CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP','CONFIG_BT_ALLOCATION_FROM_SPIRAM_FIRST']:
 assert name+'=y' in config,name
with a.elf.open('rb') as f:
 elf=ELFFile(f);sec=elf.get_section_by_name('.ext_ram.bss');assert sec and sec['sh_size']>0
 lo,hi=sec['sh_addr'],sec['sh_addr']+sec['sh_size'];symbols={s.name:s for s in elf.get_section_by_name('.symtab').iter_symbols()}
 examples={}
 for name in ['btm_cb','l2cb','gatt_cb','tcp_active_pcbs']:
  s=symbols[name];assert lo<=s['st_value']<hi
  examples[name]={'address':hex(s['st_value']),'size':s['st_size'],'section':elf.get_section(s['st_shndx']).name}
 for name in ['field_eof','descriptor_error']:
  assert elf.get_section(symbols[name]['st_shndx']).name=='.iram0.text'
counts=Counter()
for line in mp.read_text().splitlines():
 m=re.search(r'(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+(\S+\.a)\(',line)
 if m and lo<=int(m[1],16)<hi:counts[Path(m[3]).name]+=int(m[2],16)
for lib in ['libbt.a','liblwip.a','libnet80211.a','libpp.a','libwpa_supplicant.a']:assert counts[lib]>0,lib
result={'external_BSS_start':hex(lo),'external_BSS_end':hex(hi),'external_BSS_bytes':hi-lo,'external_BSS_by_archive_without_alignment_padding':dict(counts),'examples':examples,'note':'Linked static placement, not measured live dynamic allocations. SDK Wi-Fi/lwIP and BT heap routes prefer PSRAM with internal fallback. Controller/DMA/FreeRTOS internal allocations remain.'}
if a.output:a.output.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
