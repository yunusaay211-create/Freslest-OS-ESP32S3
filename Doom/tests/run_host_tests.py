"""Compile the real audio/gamepad C sources against host API doubles, then run scenarios."""
from pathlib import Path
import os, struct, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
inc=['-I'+str(root.parent/'shared_components/freslest_video/include'),'-I'+str(root.parent/'shared_components/freslest_hid/include'),'-I'+str(root/'tests/stubs'),'-I'+str(root/'main'),'-I'+str(root/'components/prboom/include'),'-I'+str(root/'components/prboom-esp32-compat/include')]
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp)
    wad=(root/'DOOM.WAD').read_bytes()
    _,n,d=struct.unpack_from('<4sII',wad)
    for i in range(n):
        offset,size,name=struct.unpack_from('<II8s',wad,d+i*16)
        if name.rstrip(b'\0')==b'DSPISTOL':
            (tmp/'pistol.raw').write_bytes(wad[offset:offset+size]); break
    else: raise RuntimeError('DSPISTOL not found')
    for name in ['audio','gamepad','lcd']:
        exe=tmp/name
        subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',*inc,str(root/f'tests/test_{name}.c'),*([str(root.parent/'shared_components/freslest_hid/freslest_hid_state.c')] if name=='gamepad' else []),'-o',str(exe)],check=True)
        subprocess.run([str(exe),*([str(tmp/'pistol.raw')] if name=='audio' else [])],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
    subprocess.run(['gcc','-std=gnu17','-fsyntax-only','-DHAVE_CONFIG_H','-DESP_PLATFORM',*inc,str(root/'components/prboom/g_game.c')],check=True)
print('All host source tests and G_BuildTiccmd syntax check passed. ESP-IDF/hardware not tested.')

subprocess.run(['gcc','-std=gnu17','-fsyntax-only','-DHAVE_CONFIG_H','-DESP_PLATFORM',*inc,str(root/'components/prboom/d_main.c')],check=True)
assert '#define DOOM_START_IN_LEVEL 0' in (root/'main/board_config.h').read_text()
assert '#define DOOM_START_IN_MENU 1' in (root/'main/board_config.h').read_text()
print('PASS: menu-first startup syntax and configuration')
