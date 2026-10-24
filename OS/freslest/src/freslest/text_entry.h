#pragma once
#include <cstring>
#include <stddef.h>
#include <stdint.h>
struct TextEntryState {
  char text[384]{};
  unsigned length = 0, cursor = 0, limit = 383, page = 0, focus = 0;
  bool secret = false;
  void begin(const char *value, unsigned maxLength, bool hide) {
    limit = maxLength < sizeof(text) ? maxLength : sizeof(text) - 1;
    secret = hide;
    page = focus = 0;
    length = 0;
    while (value && value[length] && length < limit) {
      text[length] = value[length];
      ++length;
    }
    text[length] = 0;
    cursor = length;
  }
  bool insert(char c) {
    if ((unsigned char)c < 32 || (unsigned char)c > 126 || length >= limit)
      return false;
    memmove(text + cursor + 1, text + cursor, length - cursor + 1);
    text[cursor++] = c;
    ++length;
    return true;
  }
  bool erase(bool before) {
    if (before) {
      if (!cursor)
        return false;
      --cursor;
    } else if (cursor == length)
      return false;
    memmove(text + cursor, text + cursor + 1, length - cursor);
    --length;
    return true;
  }
  void moveCursor(int d) {
    if (d < 0) {
      if (cursor)
        --cursor;
    } else if (cursor < length)
      ++cursor;
  }
  void moveFocus(int x, int y) {
    unsigned col = focus < 40 ? focus % 10 : (focus - 40) * 2,
             row = focus < 40 ? focus / 10 : 4;
    if (y < 0)
      row = (row + 4) % 5;
    if (y > 0)
      row = (row + 1) % 5;
    if (x) {
      if (row == 4) {
        unsigned c = focus >= 40 ? focus - 40 : col / 2;
        focus = 40 + (c + (x > 0 ? 1 : 4)) % 5;
        return;
      }
      col = (col + (x > 0 ? 1 : 9)) % 10;
    }
    focus = row == 4 ? 40 + col / 2 : row * 10 + col;
  }
  char focusedChar() const {
    static const char *keys[] = {"1234567890qwertyuiopasdfghjkl-zxcvbnm._@",
                                 "1234567890QWERTYUIOPASDFGHJKL-ZXCVBNM._@",
                                 "!@#$%^&*()-_=+[]{}<>/\\:;\"'?,.~`|        "};
    return focus < 40 ? keys[page % 3][focus] : 0;
  }
};
enum class TextEntryResult { None, Accepted, Cancelled };
void textEntryBegin(const char *title, const char *value, unsigned maxLength,
                    bool secret = false);
bool textEntryActive();
void textEntryLoop();
TextEntryResult textEntryTakeResult(char *out, size_t size);
void textEntryCancel();
