#define FRESLEST_FILES_HOST_TEST 1
#define FRESLEST_INTERNAL_ROOT testInternalRoot
static const char *testInternalRoot;
#include "../freslest/src/freslest/app_files.cpp"
#include <filesystem>
#include <fstream>
#include <unistd.h>
static void clear(){bBack.pressedEdge=bUp.pressedEdge=bDown.pressedEdge=bLeft.pressedEdge=bRight.pressedEdge=bOk.pressedEdge=bFire.pressedEdge=false;usbPointer.click=usbPointer.rightClick=false;}
static void click(int x,int y,bool right=false){clear();usbPointer={x,y,true,false,!right,right};filesLoop(0);clear();}
int main(){
 auto root=std::filesystem::temp_directory_path()/("files_ui_"+std::to_string(getpid()));std::string mount=root.string();std::filesystem::create_directories(root/"games/nes");std::ofstream(root/"note.txt")<<"Keep this note";std::ofstream(root/"games/nes/TEST.NES")<<"NES data";testInternalRoot=mount.c_str();filesEnter();assert(view==DISKS);bOk.pressedEdge=true;filesLoop(0);clear();assert(view==LIST&&browser.count()==2);
 click(40,120,true);assert(view==ACTIONS&&selected==1);click(60,220);assert(view==DELETE_CONFIRM&&!confirmDelete);bOk.pressedEdge=true;filesLoop(0);clear();assert(view==LIST&&std::filesystem::exists(root/"note.txt"));
 click(40,120,true);click(60,220);bBack.pressedEdge=true;filesLoop(0);clear();assert(view==LIST&&std::filesystem::exists(root/"note.txt"));click(40,120);assert(view==TEXT&&!strcmp(preview,"Keep this note"));bBack.pressedEdge=true;filesLoop(0);clear();
 click(40,120,true);click(60,220);click(300,230);assert(view==LIST&&!std::filesystem::exists(root/"note.txt")&&std::filesystem::exists(root/"games/nes/TEST.NES"));click(40,90);assert(!strcmp(browser.path(),"/games"));bBack.pressedEdge=true;filesLoop(0);clear();assert(!strcmp(browser.path(),"/"));bBack.pressedEdge=true;filesLoop(0);clear();assert(view==DISKS);bBack.pressedEdge=true;filesLoop(0);clear();assert(entered==APP_APPS);
 auto usb=root/"usb",usb2=root/"usb2";std::filesystem::create_directories(usb/"docs");std::filesystem::create_directory(usb2);
 std::ofstream(usb/"docs/readme.txt")<<"USB text";std::ofstream(usb/"delete.txt")<<"Delete only USB";std::ofstream(usb2/"second.txt")<<"Second drive";
 std::string usbMount=usb.string(),secondMount=usb2.string();hostUsbRoot=usbMount.c_str();
 filesEnter();assert(view==DISKS&&!hostUsb.present());assert(std::find(tft.words.begin(),tft.words.end(),"D:  USB drive")==tft.words.end());
 hostUsb.state=UsbStorageState::Ready;hostUsb.bytes=16*1024*1024;hostUsb.revision++;filesLoop(1);assert(view==DISKS&&hostUsb.present()&&!usbLocks);assert(std::find(tft.words.begin(),tft.words.end(),"D:  USB drive")!=tft.words.end());
 click(40,170);assert(onUsb&&view==LIST&&browser.count()==2&&!usbLocks);
 click(40,90);assert(!strcmp(browser.path(),"/docs"));click(40,90);assert(view==TEXT&&!strcmp(preview,"USB text"));
 // Removal while previewing a file discards cached entries and returns to Disks.
 hostUsb.state=UsbStorageState::Absent;hostUsb.revision++;filesLoop(2);assert(view==DISKS&&!browser.count()&&!usbLocks);assert(std::find(tft.words.begin(),tft.words.end(),"D:  USB drive")==tft.words.end());
 hostUsbRoot=secondMount.c_str();hostUsb.state=UsbStorageState::Ready;hostUsb.revision++;filesLoop(3);click(40,170);assert(!strcmp(browser.path(),"/")&&browser.count()==1);click(40,90);assert(!strcmp(preview,"Second drive"));
 hostUsbRoot=usbMount.c_str();hostUsb.revision++;filesLoop(4);assert(view==DISKS);click(40,170);
 click(40,120,true);click(60,220);assert(view==DELETE_CONFIRM&&!confirmDelete);click(300,230);
 assert(!std::filesystem::exists(usb/"delete.txt")&&std::filesystem::exists(root/"games/nes/TEST.NES")&&!usbLocks);
 bBack.pressedEdge=true;filesLoop(5);clear();assert(view==DISKS);bFire.pressedEdge=true;filesLoop(6);clear();assert(ejects==1);filesLoop(7);assert(view==DISKS);
 disk=1;bOk.pressedEdge=true;filesLoop(8);clear();assert(retries==1&&view==DISKS);
 hostUsb.state=UsbStorageState::Error;hostUsb.revision++;filesLoop(9);bOk.pressedEdge=true;filesLoop(10);clear();assert(retries==2&&view==DISKS);
 hostPC=true;hostUsb.state=UsbStorageState::Disabled;hostUsb.revision++;filesLoop(11);bFire.pressedEdge=true;filesLoop(12);clear();assert(!hostPC&&modeSwitches==1);
 std::filesystem::remove_all(root);
 puts("PASS: actual Files UI C:/D: Disks, USB folders/text, hot removal during preview, replaced-drive root reset, USB-only confirmed delete, retry/eject/PC-role switch, internal ROM preservation and lock release; original actions/cancel/parent behavior");
}
