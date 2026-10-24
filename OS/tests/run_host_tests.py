from pathlib import Path
import subprocess,os,sys
base=Path(__file__).resolve().parent; project=base.parent
build=base/'host_build';build.mkdir(exist_ok=True)
core=project/'components/nofrendo/src';app=project/'freslest/src/freslest'
def run(cmd,**kwargs):subprocess.run(list(map(str,cmd)),check=True,**kwargs)
flags=['-g','-O1','-fsanitize=address','-fno-omit-frame-pointer','-fwrapv','-fcommon','-fno-strict-aliasing','-w','-I'+str(core)]
objects=[]
for i,file in enumerate(core.rglob('*.c')):
 obj=build/f'{i}.o';run(['gcc','-std=gnu17',*flags,'-c',file,'-o',obj]);objects.append(obj)
run(['g++','-std=c++17',*flags,'-I'+str(base/'stubs'),'-I'+str(app),app/'nes_core.cpp',app/'nes_audio.cpp',base/'mock_audio.cpp',base/'test_nes_core.cpp',*objects,'-Wl,--wrap=malloc','-Wl,--wrap=free','-lm','-pthread','-o',build/'test_nes_core'])
rom=next((project/'freslest/data/games/nes').glob('*.nes'))
run([build/'test_nes_core',rom],cwd=build,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
run(['g++','-std=c++17','-g','-fsanitize=address,undefined','-I'+str(app),base/'test_data.cpp','-o',build/'test_data'])
run([build/'test_data'],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

run(['g++','-std=c++17','-g','-fsanitize=address,undefined','-I'+str(base/'stubs'),'-I'+str(app),'-I'+str(core),app/'audio_manager.cpp',app/'nes_audio.cpp',base/'mock_audio.cpp',base/'test_mp3.cpp','-o',build/'test_mp3'])
run([build/'test_mp3'],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

run(['g++','-std=c++17','-g','-fsanitize=address,undefined','-pthread','-I'+str(base/'stubs'),'-I'+str(core),'-I'+str(app),app/'nes_audio.cpp',base/'mock_audio.cpp',base/'test_nes_audio.cpp','-o',build/'test_nes_audio'])
run([build/'test_nes_audio'],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

for name,sources in [('usb_disk',[app/'nes_usb_disk.cpp',base/'test_usb_disk.cpp']),('buttons',[project/'freslest/input.cpp',base/'test_buttons.cpp']),('battery',[base/'test_battery.cpp']),('install_notice',[base/'test_install_notice.cpp']),('input_latched',[base/'test_input_latched.cpp'])]:
 run(['g++','-std=c++17','-DFRESLEST_INPUT_HOST_TEST','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(base/'stubs'),'-I'+str(app),*sources,'-o',build/('test_'+name)])
 run([build/('test_'+name)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(base/'stubs'),'-I'+str(app),base/'test_usb_transfer.cpp',app/'nes_usb_disk.cpp','-pthread','-o',build/'test_usb_transfer'])
run([build/'test_usb_transfer'],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
run([sys.executable,base/'run_usb_scheduler_tests.py'])
run([sys.executable,base/'run_ui_tests.py'])

hid=project.parent/'shared_components/freslest_hid'
run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(app),app/'file_browser.cpp',base/'test_files.cpp','-o',build/'test_files'])
run([build/'test_files'],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(hid/'include'),hid/'freslest_hid_state.c',base/'test_hid_state.c','-o',build/'test_hid_state'])
run([build/'test_hid_state'],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
run(['gcc','-std=gnu17','-g','-fsanitize=address,undefined','-I'+str(hid/'include'),'-c',hid/'freslest_hid_state.c','-o',build/'hid_input.o'])
run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(base/'stubs'),'-I'+str(hid/'include'),base/'test_usb_input.cpp',build/'hid_input.o','-o',build/'test_usb_input'])
run([build/'test_usb_input'],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(base/'stubs'),'-I'+str(app),base/'test_files_ui.cpp',app/'file_browser.cpp','-o',build/'test_files_ui'])
run([build/'test_files_ui'],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
run([sys.executable,base/'run_pointer_tests.py'])

run([sys.executable,base/'run_nes_menu_tests.py'])
run([sys.executable,base/'run_usb_storage_tests.py'])
