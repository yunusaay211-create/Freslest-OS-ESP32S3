"""Decode a NEW 6.16 panic with the matching included OS ELF (not a 6.13/6.14 trace)."""
from pathlib import Path
import argparse,re,shutil,subprocess
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('addresses',nargs='+',help='0x420... addresses or quoted backtrace')
a=p.parse_args();addresses=[]
for item in a.addresses:
 # Retain PC side of PC:SP pairs, not stack pointers.
 addresses += re.findall(r'0x[0-9a-fA-F]+(?::0x[0-9a-fA-F]+)?',item)
addresses=[s.split(':')[0] for s in addresses]
tool=shutil.which('xtensa-esp32s3-elf-addr2line') or shutil.which('xtensa-esp-elf-addr2line')
if not tool:raise SystemExit('Run from an ESP-IDF command prompt (Xtensa toolchain required).')
if not addresses:raise SystemExit('No program-counter addresses found.')
subprocess.run([tool,'-aipfC','-e',str(r/'debug/freslest_os_s3-6.16.elf'),*addresses],check=True)
