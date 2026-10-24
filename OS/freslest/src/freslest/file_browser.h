#pragma once
#include <stddef.h>
class FileBrowser {
public:
  static constexpr unsigned MAX_ENTRIES = 128, NAME_SIZE = 65, PATH_SIZE = 256,
                            TEXT_PAGE = 768;
  struct Entry {
    char name[NAME_SIZE];
    size_t bytes;
    bool directory;
  };
  explicit FileBrowser(const char *mount = "/littlefs") : mount_(mount) {}
  void selectMount(const char *mount);
  bool refresh();
  bool openDirectory(unsigned);
  bool parent();
  bool removeEntry(unsigned);
  bool isText(unsigned) const;
  bool textPage(unsigned, unsigned, char *, size_t, bool &);
  const Entry *entry(unsigned i) const {
    return i < count_ ? &entries_[i] : nullptr;
  }
  unsigned count() const { return count_; }
  const char *path() const { return path_; }
  const char *error() const { return error_; }
  bool truncated() const { return truncated_; }

private:
  const char *mount_;
  char path_[PATH_SIZE] = "/", error_[96] = {};
  Entry entries_[MAX_ENTRIES]{};
  unsigned count_ = 0;
  bool truncated_ = false;
  bool fullPath(const char *, char *, size_t) const;
  bool fail(const char *);
};
