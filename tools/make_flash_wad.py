"""Build the flash WAD without changing the supplied original DOOM.WAD.

Maps, textures, sprites and PCM SFX remain byte-identical. Identical lump
payloads share storage; MUS scores unused by the existing DFPlayer backend
become distinct empty MUS identity tokens. Music still uses the shared MP3s.
"""
from pathlib import Path
import struct,hashlib
ROOT=Path(__file__).resolve().parents[1]
def lumps(data):
 magic,count,at=struct.unpack_from('<4sII',data)
 if magic!=b'IWAD' or not count or at+count*16>len(data):raise ValueError('Invalid IWAD')
 result=[]
 for i in range(count):
  offset,size,name=struct.unpack_from('<II8s',data,at+i*16)
  if offset+size>len(data):raise ValueError('Lump out of bounds')
  result.append((name,data[offset:offset+size],offset))
 return result

def music(name,data):return name.startswith(b'D_') and data.startswith(b'MUS\x1a')
def token(name,index):return struct.pack('<4s6H',b'MUS\x1a',1,16,0,0,0,0)+b'\x60'+name+struct.pack('<I',index)
def verify(original,image):
 before,after=lumps(original),lumps(image);assert len(before)==len(after);offsets=set();count=0
 for index,((name,data,_),(other,payload,offset)) in enumerate(zip(before,after)):
  assert name==other
  if music(name,data):assert payload==token(name,index) and offset not in offsets;offsets.add(offset);count+=1
  else:assert data==payload
 assert len(image)<=0x950000
 return len(before),count

def build():
 original=(ROOT/'Doom/DOOM.WAD').read_bytes();image=bytearray(12);entries=[];seen={}
 for index,(name,data,_) in enumerate(lumps(original)):
  if music(name,data):data=token(name,index)
  key=hashlib.sha256(data).digest();existing=seen.get(key)
  if existing is not None and existing[1]==data:offset=existing[0]
  else:
   image.extend(bytes((-len(image))%4));offset=len(image);image.extend(data);seen[key]=(offset,data)
  entries.append((offset,len(data),name))
 image.extend(bytes((-len(image))%4));at=len(image)
 for entry in entries:image.extend(struct.pack('<II8s',*entry))
 struct.pack_into('<4sII',image,0,b'IWAD',len(entries),at)
 count,tracks=verify(original,image);out=ROOT/'firmware/doom_flash.wad';out.write_bytes(image)
 print(f'PASS: {len(image)} bytes; {count} lumps; {tracks} distinct MP3 identities; all non-music bytes unchanged')
 return out
if __name__=='__main__':build()
