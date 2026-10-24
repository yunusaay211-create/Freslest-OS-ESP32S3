#include "file_browser.h"
#include <cctype>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
void FileBrowser::selectMount(const char *mount) {
  mount_=mount;path_[0]='/';path_[1]=0;error_[0]=0;count_=0;truncated_=false;
}
bool FileBrowser::fail(const char *text) {
  snprintf(error_, sizeof(error_), "%s", text);
  return false;
}
bool FileBrowser::fullPath(const char *name, char *out, size_t size) const {
  if (name && (!*name || strchr(name, '/') || !strcmp(name, ".") ||
               !strcmp(name, "..")))
    return false;
  int n = snprintf(out, size, "%s%s%s%s", mount_, path_,
                   name && strcmp(path_, "/") ? "/" : "", name ? name : "");
  return n >= 0 && (size_t)n < size;
}
bool FileBrowser::refresh() {
  count_ = 0;
  truncated_ = false;
  error_[0] = 0;
  char full[PATH_SIZE + 64];
  if (!fullPath(nullptr, full, sizeof(full)))
    return fail("Path too long");
  DIR *d = opendir(full);
  if (!d)
    return fail("Cannot read folder. Files kept.");
  while (auto *item = readdir(d)) {
    if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, ".."))
      continue;
    if (strlen(item->d_name) >= NAME_SIZE || count_ == MAX_ENTRIES) {
      truncated_ = true;
      continue;
    }
    struct stat st{};
    if (!fullPath(item->d_name, full, sizeof(full)) || stat(full, &st)) {
      truncated_ = true;
      continue;
    }
    if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))
      continue;
    Entry &e = entries_[count_++];
    memcpy(e.name, item->d_name, strlen(item->d_name) + 1);
    e.directory = S_ISDIR(st.st_mode);
    e.bytes = e.directory ? 0 : st.st_size;
  }
  closedir(d);
  for (unsigned i = 1; i < count_; ++i) {
    Entry value = entries_[i];
    unsigned j = i;
    while (j) {
      const Entry &old = entries_[j - 1];
      bool before = value.directory != old.directory
                        ? value.directory
                        : strcmp(value.name, old.name) < 0;
      if (!before)
        break;
      entries_[j] = old;
      --j;
    }
    entries_[j] = value;
  }
  return true;
}
bool FileBrowser::openDirectory(unsigned i) {
  const Entry *e = entry(i);
  if (!e || !e->directory)
    return fail("Select a folder");
  char next[PATH_SIZE], old[PATH_SIZE];
  int n = snprintf(next, sizeof(next), "%s%s%s", path_,
                   strcmp(path_, "/") ? "/" : "", e->name);
  if (n < 0 || (size_t)n >= sizeof(next))
    return fail("Path too long");
  strcpy(old, path_);
  strcpy(path_, next);
  if (refresh())
    return true;
  strcpy(path_, old);
  refresh();
  return fail("Cannot open folder");
}
bool FileBrowser::parent() {
  if (!strcmp(path_, "/"))
    return false;
  char *slash = strrchr(path_, '/');
  if (slash == path_)
    path_[1] = 0;
  else
    *slash = 0;
  return refresh();
}
bool FileBrowser::removeEntry(unsigned i) {
  const Entry *e = entry(i);
  if (!e)
    return fail("File unavailable");
  char full[PATH_SIZE + 64];
  if (!fullPath(e->name, full, sizeof(full)))
    return fail("Invalid path");
  if ((e->directory ? rmdir(full) : unlink(full)) != 0)
    return fail("Delete failed. Folder must be empty.");
  return refresh();
}
bool FileBrowser::isText(unsigned i) const {
  const Entry *e = entry(i);
  if (!e || e->directory)
    return false;
  const char *ext = strrchr(e->name, '.');
  if (!ext)
    return true;
  char lower[12];
  size_t n = strlen(ext);
  if (n >= sizeof(lower))
    return false;
  for (size_t j = 0; j <= n; j++)
    lower[j] = (char)tolower((unsigned char)ext[j]);
  const char *types[] = {".txt", ".md",  ".log", ".csv", ".json",
                         ".ini", ".cfg", ".c",   ".cpp", ".h"};
  for (auto *t : types)
    if (!strcmp(t, lower))
      return true;
  return false;
}
bool FileBrowser::textPage(unsigned i, unsigned page, char *out,
                           size_t capacity, bool &more) {
  more = false;
  if (!out || capacity < TEXT_PAGE + 1)
    return fail("Preview buffer too small");
  out[0] = 0;
  if (!isText(i))
    return fail("Binary file: use Details");
  char full[PATH_SIZE + 64];
  if (!fullPath(entry(i)->name, full, sizeof(full)))
    return fail("Invalid path");
  FILE *f = fopen(full, "rb");
  if (!f)
    return fail("Cannot open file");
  unsigned char check[256];
  size_t n = fread(check, 1, sizeof(check), f);
  for (size_t j = 0; j < n; j++)
    if (check[j] < 32 && check[j] != '\n' && check[j] != '\r' &&
        check[j] != '\t') {
      fclose(f);
      return fail("Binary file: use Details");
    }
  if (page > entry(i)->bytes / TEXT_PAGE ||
      fseek(f, (long)((size_t)page * TEXT_PAGE), SEEK_SET)) {
    fclose(f);
    return fail("Page unavailable");
  }
  n = fread(out, 1, TEXT_PAGE, f);
  bool bad = ferror(f);
  more = (size_t)(page + 1) * TEXT_PAGE < entry(i)->bytes;
  fclose(f);
  out[n] = 0;
  if (bad)
    return fail("Read failed");
  for (size_t j = 0; j < n; j++)
    if ((unsigned char)out[j] >= 127 ||
        (out[j] < 32 && out[j] != '\n' && out[j] != '\r' && out[j] != '\t'))
      out[j] = '?';
  return true;
}
