#pragma once
#include <stddef.h>
#include <stdint.h>
namespace web_lite {
constexpr unsigned URL = 384, COLS = 72, LINES = 256, LINKS = 64, FIELDS = 12,
                   FORMS = 6;
enum LinkKind : uint8_t { Anchor, Image, Field, Submit, Video };
struct Line {
  char text[COLS + 1];
  int16_t link;
};
struct Link {
  char url[URL];
  uint8_t kind, index;
};
struct FieldValue {
  char name[48], value[192];
  uint8_t form;
  bool secret, hidden;
};
struct Form {
  char action[URL];
  bool post;
};
struct Document {
  char url[URL], title[80];
  Line lines[LINES];
  Link links[LINKS];
  FieldValue fields[FIELDS];
  Form forms[FORMS];
  uint16_t lineCount, linkCount;
  uint8_t fieldCount, formCount;
  bool truncated, scripts, video;
  void reset(const char *);
};
bool validUrl(const char *);
bool resolveUrl(const char *base, const char *reference, char *out,
                size_t cap = URL);
bool formUrl(const Document &, unsigned index, char *out, size_t cap = URL);
class Parser {
  Document &d;
  char tag[768]{}, entity[20]{}, raw[12]{};
  unsigned tagLength = 0, entityLength = 0, column = 0, titleLength = 0,
           rawAt = 0;
  int currentLink = -1, currentForm = -1;
  char quote = 0;
  bool inTag = false, inEntity = false, inTitle = false, inHead = false,
       comment = false, tagOverflow = false, rawMode = false;
  unsigned commentTail = 0;
  unsigned utfLeft = 0, utfCode = 0;
  void character(char);
  void text(unsigned);
  void newline();
  void tagDone();
  void emitEntity();
  int link(LinkKind, const char *, unsigned = 0);

public:
  Parser(Document &, const char *);
  void feed(const char *, size_t);
  void finish();
  void plain(const char *, size_t);
};
} // namespace web_lite
