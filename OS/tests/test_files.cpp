#include "file_browser.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <unistd.h>
int main(){
 auto root=std::filesystem::temp_directory_path()/("freslest_files_"+std::to_string(getpid()));
 std::filesystem::create_directories(root/"games/nes");std::ofstream(root/"note.txt")<<std::string(1800,'Q');std::ofstream(root/"games/nes/TEST.NES")<<"NES data";std::ofstream(root/"bad.txt",std::ios::binary).write("x\0y",3);
 FileBrowser browser(root.c_str());assert(browser.refresh()&&browser.count()==3);assert(browser.entry(0)->directory);
 assert(browser.openDirectory(0)&&browser.openDirectory(0));char out[FileBrowser::TEXT_PAGE+1];bool more=false;assert(!browser.textPage(0,0,out,sizeof(out),more));assert(browser.parent()&&browser.parent());
 assert(!browser.removeEntry(0)&&std::filesystem::exists(root/"games/nes/TEST.NES"));assert(browser.refresh());assert(!browser.textPage(1,0,out,sizeof(out),more));
 assert(browser.textPage(2,0,out,sizeof(out),more)&&strlen(out)==768&&more);assert(browser.textPage(2,1,out,sizeof(out),more)&&strlen(out)==768&&more);assert(browser.textPage(2,2,out,sizeof(out),more)&&strlen(out)==264&&!more);assert(!browser.textPage(2,999999,out,sizeof(out),more));
 assert(browser.removeEntry(2)&&!std::filesystem::exists(root/"note.txt"));assert(browser.openDirectory(0)&&browser.openDirectory(0)&&browser.removeEntry(0));assert(browser.parent()&&browser.removeEntry(0)&&browser.parent()&&browser.removeEntry(0));assert(!browser.parent());std::filesystem::remove_all(root);
 puts("PASS: real nested file operations, nonempty folder protection, binary rejection, bounded text pages and explicit deletion");
}
