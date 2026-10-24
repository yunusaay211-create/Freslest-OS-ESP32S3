#include "browser_html.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace web_lite {
namespace {
bool copy(char *out, size_t cap, const char *s) {
  size_t n = strlen(s);
  if (!cap || n >= cap)
    return false;
  memcpy(out, s, n + 1);
  return true;
}
bool prefix(const char *s, const char *p) { return !strncmp(s, p, strlen(p)); }
unsigned entityValue(const char *s) {
  if (!strcmp(s, "amp"))
    return '&';
  if (!strcmp(s, "lt"))
    return '<';
  if (!strcmp(s, "gt"))
    return '>';
  if (!strcmp(s, "quot"))
    return '"';
  if (!strcmp(s, "apos") || !strcmp(s, "#39"))
    return '\'';
  if (!strcmp(s, "nbsp"))
    return ' ';
  if (*s == '#') {
    char *end;
    unsigned long n = strtoul(s + (s[1] == 'x' || s[1] == 'X' ? 2 : 1), &end,
                              s[1] == 'x' || s[1] == 'X' ? 16 : 10);
    if (!*end && n > 0 && n <= 0x10ffff)
      return n;
  }
  return 0;
}
char ascii(unsigned c) {
  if (c < 128)
    return c >= 32 ? (char)c : ' ';
  switch (c) {
  case 0xe7:
    return 'c';
  case 0xc7:
    return 'C';
  case 0x11f:
    return 'g';
  case 0x11e:
    return 'G';
  case 0x131:
    return 'i';
  case 0x130:
    return 'I';
  case 0xf6:
    return 'o';
  case 0xd6:
    return 'O';
  case 0x15f:
    return 's';
  case 0x15e:
    return 'S';
  case 0xfc:
    return 'u';
  case 0xdc:
    return 'U';
  case 0x2018:
  case 0x2019:
    return '\'';
  case 0x201c:
  case 0x201d:
    return '"';
  case 0x2013:
  case 0x2014:
    return '-';
  case 0xa0:
    return ' ';
  default:
    return '?';
  }
}
bool attr(const char *tag, const char *wanted, char *out, size_t cap) {
  out[0] = 0;
  const char *p = tag;
  while (*p && !isspace((unsigned char)*p))
    ++p;
  while (*p) {
    while (isspace((unsigned char)*p) || *p == '/')
      ++p;
    if (!*p)
      break;
    const char *start = p;
    while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != '/')
      ++p;
    size_t len = p - start;
    while (isspace((unsigned char)*p))
      ++p;
    const char *v = "";
    size_t n = 0;
    if (*p == '=') {
      ++p;
      while (isspace((unsigned char)*p))
        ++p;
      char q = (*p == '\'' || *p == '"') ? *p++ : 0;
      v = p;
      while (*p && (q ? *p != q : !isspace((unsigned char)*p)))
        ++p;
      n = p - v;
      if (q && *p)
        ++p;
    }
    bool match = len == strlen(wanted);
    for (size_t i = 0; match && i < len; i++)
      match = tolower((unsigned char)start[i]) == wanted[i];
    if (match) {
      size_t k = 0;
      for (size_t i = 0; i < n && k + 1 < cap; i++) {
        if (v[i] == '&') {
          const char *semi =
              (const char *)memchr(v + i, ';', std::min((size_t)18, n - i));
          if (semi) {
            char e[20];
            size_t el = semi - v - i - 1;
            memcpy(e, v + i + 1, el);
            e[el] = 0;
            unsigned c = entityValue(e);
            if (c && c < 128) {
              out[k++] = (char)c;
              i = semi - v;
              continue;
            }
          }
        }
        out[k++] = v[i];
      }
      out[k] = 0;
      return true;
    }
    if (p == start)
      ++p;
  }
  return false;
}
bool encoded(char *out, size_t cap, size_t &n, const char *s) {
  const char *h = "0123456789ABCDEF";
  for (; *s; s++) {
    unsigned char c = *s;
    bool simple = isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
    size_t add = simple ? 1 : 3;
    if (n + add >= cap)
      return false;
    if (simple)
      out[n++] = c;
    else {
      out[n++] = '%';
      out[n++] = h[c >> 4];
      out[n++] = h[c & 15];
    }
  }
  out[n] = 0;
  return true;
}
} // namespace
void Document::reset(const char *s) {
  memset(this, 0, sizeof(*this));
  copy(url, sizeof(url), s);
  lineCount = 1;
  for (auto &l : lines)
    l.link = -1;
}
bool validUrl(const char *s) {
  size_t n = strnlen(s, URL);
  size_t p = prefix(s, "https://") ? 8 : prefix(s, "http://") ? 7 : 0;
  if (!p || n <= p || n >= URL)
    return false;
  const char *end = s + p;
  while (*end && *end != '/' && *end != '?' && *end != '#') {
    if (*end == '@')
      return false;
    ++end;
  }
  if (end == s + p || s[p] == ':' || s[p] == '.')
    return false;
  for (size_t i = 0; i < n; i++)
    if ((unsigned char)s[i] <= 32 || (unsigned char)s[i] == 127 || s[i] == '\\')
      return false;
  return true;
}
bool resolveUrl(const char *base, const char *reference, char *out,
                size_t cap) {
  while (isspace((unsigned char)*reference))
    ++reference;
  char ref[URL];
  size_t n = strnlen(reference, URL);
  while (n && isspace((unsigned char)reference[n - 1]))
    --n;
  if (n >= sizeof(ref))
    return false;
  memcpy(ref, reference, n);
  ref[n] = 0;
  if (char *h = strchr(ref, '#'))
    *h = 0;
  char candidate[URL]{};
  if (prefix(ref, "http://") || prefix(ref, "https://")) {
    if (!copy(candidate, sizeof(candidate), ref))
      return false;
  } else {
    if (!validUrl(base))
      return false;
    const char *scheme = strstr(base, "://");
    size_t origin = scheme - base + 3;
    while (base[origin] && base[origin] != '/' && base[origin] != '?' &&
           base[origin] != '#')
      ++origin;
    if (prefix(ref, "//")) {
      if ((size_t)(scheme - base + 1) + strlen(ref) >= sizeof(candidate))
        return false;
      memcpy(candidate, base, scheme - base + 1);
      strcat(candidate, ref);
    } else {
      const char *colon = strchr(ref, ':'), *slash = strchr(ref, '/');
      if (colon && (!slash || colon < slash))
        return false;
      size_t keep = origin;
      if (!ref[0]) {
        keep = strcspn(base, "#");
      } else if (ref[0] == '?') {
        keep = strcspn(base, "?#");
      } else if (ref[0] != '/') {
        keep = strcspn(base, "?#");
        while (keep > origin && base[keep - 1] != '/')
          --keep;
      }
      if (keep + strlen(ref) + 2 >= sizeof(candidate))
        return false;
      memcpy(candidate, base, keep);
      candidate[keep] = 0;
      if (ref[0] && ref[0] != '/' && ref[0] != '?' && keep == origin)
        strcat(candidate, "/");
      strcat(candidate, ref);
    }
  }
  if (!validUrl(candidate))
    return false;
  // Normalize dot path segments without touching queries or percent escapes.
  const char *scheme = strstr(candidate, "://");
  size_t start = scheme - candidate + 3;
  while (candidate[start] && candidate[start] != '/' && candidate[start] != '?')
    ++start;
  char normalized[URL];
  memcpy(normalized, candidate, start);
  size_t used = start;
  const char *p = candidate + start;
  while (*p && *p != '?') {
    if (*p != '/')
      break;
    const char *next = p + 1;
    while (*next && *next != '/' && *next != '?')
      ++next;
    size_t len = next - p - 1;
    if (len == 1 && p[1] == '.') {
    } else if (len == 2 && p[1] == '.' && p[2] == '.') {
      while (used > start && normalized[used - 1] != '/')
        --used;
      if (used > start)
        --used;
    } else {
      memcpy(normalized + used, p, len + 1);
      used += len + 1;
    }
    p = next;
  }
  if (*p == '?') {
    size_t tail = strlen(p);
    memcpy(normalized + used, p, tail);
    used += tail;
  }
  normalized[used] = 0;
  return copy(out, cap, normalized);
}
bool formUrl(const Document &d, unsigned index, char *out, size_t cap) {
  if (index >= d.formCount || d.forms[index].post ||
      !copy(out, cap, d.forms[index].action))
    return false;
  size_t n = strlen(out);
  bool query = strchr(out, '?');
  for (unsigned i = 0; i < d.fieldCount; i++) {
    auto &f = d.fields[i];
    if (f.form != index || !f.name[0])
      continue;
    if (n + 2 >= cap)
      return false;
    out[n++] = query ? '&' : '?';
    query = true;
    if (!encoded(out, cap, n, f.name) || n + 2 >= cap)
      return false;
    out[n++] = '=';
    if (!encoded(out, cap, n, f.value))
      return false;
  }
  out[n] = 0;
  return validUrl(out);
}
Parser::Parser(Document &doc, const char *url) : d(doc) { d.reset(url); }
void Parser::newline() {
  if (!column)
    return;
  column = 0;
  if (d.lineCount < LINES) {
    ++d.lineCount;
    d.lines[d.lineCount - 1].link = currentLink;
  } else
    d.truncated = true;
}
void Parser::text(unsigned code) {
  char c = ascii(code);
  if (inTitle) {
    if (titleLength + 1 < sizeof(d.title)) {
      d.title[titleLength++] = c;
      d.title[titleLength] = 0;
    }
    return;
  }
  if (inHead)
    return;
  if (c == ' ' && (!column || d.lines[d.lineCount - 1].text[column - 1] == ' '))
    return;
  if (d.truncated)
    return;
  if (column >= COLS)
    newline();
  if (d.truncated)
    return;
  auto &line = d.lines[d.lineCount - 1];
  line.link = currentLink;
  line.text[column++] = c;
  line.text[column] = 0;
}
void Parser::emitEntity() {
  entity[entityLength] = 0;
  unsigned c = entityValue(entity);
  if (c)
    text(c);
  else {
    text('&');
    for (unsigned i = 0; i < entityLength; i++)
      text(entity[i]);
    text(';');
  }
  inEntity = false;
  entityLength = 0;
}
void Parser::character(char ch) {
  unsigned char c = ch;
  if (utfLeft) {
    if ((c & 0xc0) != 0x80) {
      utfLeft = 0;
      text('?');
    } else {
      utfCode = (utfCode << 6) | (c & 63);
      if (!--utfLeft)
        text(utfCode);
      return;
    }
  }
  if (c >= 0xc2 && c <= 0xf4) {
    utfLeft = c < 0xe0 ? 1 : c < 0xf0 ? 2 : 3;
    utfCode = c & ((1u << (6 - utfLeft)) - 1);
    return;
  }
  text(c >= 128 ? '?' : c);
}
int Parser::link(LinkKind kind, const char *url, unsigned index) {
  if (d.linkCount >= LINKS) {
    d.truncated = true;
    return -1;
  }
  auto &l = d.links[d.linkCount];
  l.kind = kind;
  l.index = index;
  if (url && !copy(l.url, sizeof(l.url), url))
    return -1;
  return d.linkCount++;
}
void Parser::tagDone() {
  tag[tagLength] = 0;
  char name[20];
  unsigned n = 0;
  const char *p = tag;
  bool close = *p == '/';
  if (close)
    ++p;
  while (*p && !isspace((unsigned char)*p) && *p != '/' && n + 1 < sizeof(name))
    name[n++] = tolower((unsigned char)*p++);
  name[n] = 0;
  if (!strcmp(name, "head")) {
    inHead = !close;
    return;
  }
  if (!strcmp(name, "title")) {
    inTitle = !close;
    return;
  }
  if (!close && (!strcmp(name, "script") || !strcmp(name, "style"))) {
    if (!strcmp(name, "script"))
      d.scripts = true;
    strcpy(raw,!strcmp(name,"script")?"</script":"</style");
    rawAt = 0;
    rawMode = true;
    return;
  }
  if (inHead)
    return;
  if (!strcmp(name, "a")) {
    newline();
    if (close)
      currentLink = -1;
    else {
      char href[URL], url[URL];
      currentLink =
          attr(tag, "href", href, sizeof(href)) && resolveUrl(d.url, href, url)
              ? link(Anchor, url)
              : -1;
    }
    return;
  }
  if (!strcmp(name, "form")) {
    newline();
    if (close)
      currentForm = -1;
    else if (d.formCount < FORMS) {
      currentForm = d.formCount++;
      auto &f = d.forms[currentForm];
      char action[URL], method[16];
      attr(tag, "action", action, sizeof(action));
      if (!resolveUrl(d.url, action, f.action))
        copy(f.action, sizeof(f.action), d.url);
      attr(tag, "method", method, sizeof(method));
      for (char *c = method; *c; c++)
        *c = tolower((unsigned char)*c);
      f.post = !strcmp(method, "post");
    }
    return;
  }
  if (!close && !strcmp(name, "input") && currentForm >= 0) {
    char type[24], name[48], value[192];
    attr(tag, "type", type, sizeof(type));
    for (char *c = type; *c; c++)
      *c = tolower((unsigned char)*c);
    attr(tag, "name", name, sizeof(name));
    attr(tag, "value", value, sizeof(value));
    if (!strcmp(type, "submit") || !strcmp(type, "button")) {
      newline();
      currentLink = link(Submit, nullptr, currentForm);
      const char *s = value[0] ? value : "Submit";
      while (*s)
        text(*s++);
      newline();
      currentLink = -1;
      return;
    }
    if (!name[0] || d.fieldCount == FIELDS || !strcmp(type, "checkbox") ||
        !strcmp(type, "radio") || !strcmp(type, "file"))
      return;
    unsigned index = d.fieldCount++;
    auto &f = d.fields[index];
    copy(f.name, sizeof(f.name), name);
    copy(f.value, sizeof(f.value), value);
    f.form = currentForm;
    f.hidden = !strcmp(type, "hidden");
    f.secret = !strcmp(type, "password");
    if (!f.hidden) {
      newline();
      currentLink = link(Field, nullptr, index);
      text('[');
      for (char *c = name; *c; c++)
        text(*c);
      text(']');
      text(' ');
      if (f.secret) {
        text('*');
        text('*');
        text('*');
      } else
        for (char *c = value; *c; c++)
          text(*c);
      newline();
      currentLink = -1;
    }
    return;
  }
  if (!close && !strcmp(name, "button") && currentForm >= 0) {
    newline();
    currentLink = link(Submit, nullptr, currentForm);
    return;
  }
  if (close && !strcmp(name, "button")) {
    newline();
    currentLink = -1;
    return;
  }
  if (!close && (!strcmp(name, "img") || !strcmp(name, "video") ||
                 !strcmp(name, "source"))) {
    bool video = strcmp(name, "img");
    char src[URL], url[URL], alt[96];
    if (!attr(tag, "src", src, sizeof(src)) || !resolveUrl(d.url, src, url))
      return;
    newline();
    currentLink = link(video ? Video : Image, url);
    const char *label = video ? "[Video: unsupported codec] " : "[Image] ";
    while (*label)
      text(*label++);
    if (attr(tag, "alt", alt, sizeof(alt)))
      for (char *c = alt; *c; c++)
        text(*c);
    newline();
    currentLink = -1;
    d.video |= video;
    return;
  }
  if (!strcmp(name, "br") || !strcmp(name, "p") || !strcmp(name, "div") ||
      !strcmp(name, "li") || !strcmp(name, "tr") || !strcmp(name, "h1") ||
      !strcmp(name, "h2") || !strcmp(name, "h3") || !strcmp(name, "section") ||
      !strcmp(name, "article"))
    newline();
}
void Parser::feed(const char *data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    char c = data[i];
    if (comment) {
      commentTail = ((commentTail << 8) | (unsigned char)c) & 0xffffff;
      if (commentTail == 0x2d2d3e) {
        comment = false;
        commentTail = 0;
      }
      continue;
    }
    if (rawMode) {
      char lower = tolower((unsigned char)c);
      size_t n = strlen(raw);
      if (rawAt == n) {
        if (c == '>') {
          rawMode = false;
          rawAt = 0;
        } else if (!isspace((unsigned char)c))
          rawAt = 0;
      } else if (lower == raw[rawAt])
        ++rawAt;
      else
        rawAt = lower == '<' ? 1 : 0;
      continue;
    }
    if (inTag) {
      if (!quote && c == '>') {
        if (!tagOverflow)
          tagDone();
        inTag = false;
        tagLength = 0;
        continue;
      }
      if (c == '\'' || c == '"') {
        if (!quote)
          quote = c;
        else if (quote == c)
          quote = 0;
      }
      if (tagLength + 1 < sizeof(tag))
        tag[tagLength++] = c;
      else
        tagOverflow = true;
      if (tagLength == 3 && !memcmp(tag, "!--", 3)) {
        comment = true;
        inTag = false;
        tagLength = 0;
      }
      continue;
    }
    if (inEntity) {
      if (c == ';') {
        emitEntity();
        continue;
      }
      if (entityLength + 1 < sizeof(entity) &&
          ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '#')) {
        entity[entityLength++] = c;
        continue;
      }
      text('&');
      for (unsigned j = 0; j < entityLength; j++)
        character(entity[j]);
      inEntity = false;
      entityLength = 0;
    }
    if (c == '<') {
      inTag = true;
      tagLength = 0;
      quote = 0;
      tagOverflow = false;
    } else if (c == '&') {
      inEntity = true;
      entityLength = 0;
    } else
      character(c);
  }
}
void Parser::plain(const char *data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (data[i] == '\n')
      newline();
    else
      character(data[i]);
  }
}
void Parser::finish() {
  if (inEntity) {
    text('&');
    for (unsigned i = 0; i < entityLength; i++)
      character(entity[i]);
  }
  while (d.lineCount > 1 && !d.lines[d.lineCount - 1].text[0])
    --d.lineCount;
  if (!d.title[0])
    copy(d.title, sizeof(d.title), "Web page");
}
} // namespace web_lite
