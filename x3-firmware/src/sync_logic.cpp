#include "sync_logic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mergeDoneMarks(TaskList& fresh, const TaskList& local) {
  int carried = 0;
  for (int i = 0; i < fresh.count; ++i) {
    Task& t = fresh.items[i];
    if (t.done) continue;
    for (int j = 0; j < local.count; ++j) {
      const Task& l = local.items[j];
      if (l.done && strcmp(l.text, t.text) == 0 && strcmp(l.time, t.time) == 0) {
        t.done = true;
        ++carried;
        break;
      }
    }
  }
  return carried;
}

size_t base64Encode(const uint8_t* in, size_t len, char* out, size_t outSize) {
  static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const size_t need = 4 * ((len + 2) / 3) + 1;
  if (outSize < need) return 0;
  size_t o = 0;
  for (size_t i = 0; i < len; i += 3) {
    const uint32_t a = in[i];
    const uint32_t b = i + 1 < len ? in[i + 1] : 0;
    const uint32_t c = i + 2 < len ? in[i + 2] : 0;
    const uint32_t v = (a << 16) | (b << 8) | c;
    out[o++] = kAlphabet[(v >> 18) & 63];
    out[o++] = kAlphabet[(v >> 12) & 63];
    out[o++] = i + 1 < len ? kAlphabet[(v >> 6) & 63] : '=';
    out[o++] = i + 2 < len ? kAlphabet[v & 63] : '=';
  }
  out[o] = '\0';
  return o;
}

bool jsonStringField(const char* json, const char* key, char* out, size_t outSize) {
  char pattern[48];
  snprintf(pattern, sizeof pattern, "\"%s\"", key);
  const char* p = strstr(json, pattern);
  if (p == nullptr) return false;
  p += strlen(pattern);
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
  if (*p++ != ':') return false;
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
  if (*p++ != '"') return false;
  size_t o = 0;
  while (*p && *p != '"') {
    char c = *p++;
    if (c == '\\' && (*p == '"' || *p == '\\')) c = *p++;
    if (o + 1 >= outSize) return false;
    out[o++] = c;
  }
  if (*p != '"') return false;
  out[o] = '\0';
  return true;
}

namespace {

// Appends a JSON string literal (with escaping) to out at *o.
bool putJsonString(const char* s, char* out, size_t outSize, size_t* o) {
  auto put = [&](char c) {
    if (*o + 1 >= outSize) return false;
    out[(*o)++] = c;
    return true;
  };
  if (!put('"')) return false;
  for (; *s; ++s) {
    const unsigned char c = static_cast<unsigned char>(*s);
    if (c == '"' || c == '\\') {
      if (!put('\\') || !put(static_cast<char>(c))) return false;
    } else if (c < 0x20) {
      char esc[8];
      snprintf(esc, sizeof esc, "\\u%04x", c);
      for (const char* e = esc; *e; ++e)
        if (!put(*e)) return false;
    } else if (!put(static_cast<char>(c))) {
      return false;
    }
  }
  return put('"');
}

bool putRaw(const char* s, char* out, size_t outSize, size_t* o) {
  for (; *s; ++s) {
    if (*o + 1 >= outSize) return false;
    out[(*o)++] = *s;
  }
  return true;
}

}  // namespace

bool buildPutBody(const char* message, const char* base64, const char* sha, const char* branch, char* out,
                  size_t outSize) {
  size_t o = 0;
  const bool ok = putRaw("{\"message\":", out, outSize, &o) && putJsonString(message, out, outSize, &o) &&
                  putRaw(",\"content\":", out, outSize, &o) && putJsonString(base64, out, outSize, &o) &&
                  putRaw(",\"branch\":", out, outSize, &o) && putJsonString(branch, out, outSize, &o) &&
                  (sha == nullptr || sha[0] == '\0' ||
                   (putRaw(",\"sha\":", out, outSize, &o) && putJsonString(sha, out, outSize, &o))) &&
                  putRaw("}", out, outSize, &o);
  if (!ok) return false;
  out[o] = '\0';
  return true;
}

size_t buildDeviceStateFile(const TaskList& tasks, const Library* lib, const char* stamp, char* out,
                            size_t outSize) {
  const int n = snprintf(out, outSize, "# X3 cihaz durumu. guncellendi: %s\n", stamp);
  if (n < 0 || static_cast<size_t>(n) >= outSize) return 0;
  size_t o = static_cast<size_t>(n) + tasks.serialize(out + n, outSize - n);
  if (lib == nullptr) return o;
  for (int i = 0; i < lib->count; ++i) {
    const BookInfo& b = lib->books[i];
    char when[20] = "-";
    if (b.lastRead >= 200001010000ull && b.lastRead < 210001010000ull) {
      const uint64_t v = b.lastRead;
      snprintf(when, sizeof when, "%04u-%02u-%02u %02u:%02u", static_cast<unsigned>(v / 100000000ull),
               static_cast<unsigned>(v / 1000000ull % 100), static_cast<unsigned>(v / 10000ull % 100),
               static_cast<unsigned>(v / 100 % 100), static_cast<unsigned>(v % 100));
    }
    const char* state = b.finished ? "bitti" : (b.lastRead != 0 ? "okunuyor" : "baslanmadi");
    const int w = snprintf(out + o, outSize - o, "# kitap | %%%u | %s | %s | %s\n", b.finished ? 100u : b.percent,
                           state, when, b.file);
    if (w < 0 || o + static_cast<size_t>(w) >= outSize) {
      out[o] = '\0';  // drop the line that does not fit
      break;
    }
    o += static_cast<size_t>(w);
  }
  return o;
}

int pickNetwork(const char* const saved[2], const char* const seen[], const int rssi[], int seenCount) {
  int best = -1, bestRssi = -1000;
  for (int k = 0; k < 2; ++k) {
    if (saved[k] == nullptr || saved[k][0] == '\0') continue;
    for (int i = 0; i < seenCount; ++i) {
      if (strcmp(seen[i], saved[k]) == 0 && rssi[i] > bestRssi) {
        best = k;
        bestRssi = rssi[i];
      }
    }
  }
  return best;
}

bool safeBookName(const char* name) {
  const size_t n = strlen(name);
  if (n == 0 || n >= sizeof(BookInfo{}.file) || name[0] == '.' || name[0] == ' ') return false;
  for (const char* p = name; *p; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c < 0x20 || c == 0x7f || strchr("/\\:*?\"<>|", c) != nullptr) return false;
  }
  BookInfo probe;
  return bookFromFileName(name, 0, probe);
}

int parseDirListing(const char* json, RemoteBook* out, int max) {
  static const char kName[] = "\"name\"";
  int count = 0;
  const char* p = strstr(json, kName);
  while (p != nullptr && count < max) {
    const char* next = strstr(p + 1, kName);
    // Copy this entry (up to the next "name") so the field lookups stay inside it.
    char entry[1024];
    size_t len = next != nullptr ? static_cast<size_t>(next - p) : strlen(p);
    if (len >= sizeof entry) len = sizeof entry - 1;
    memcpy(entry, p, len);
    entry[len] = '\0';
    RemoteBook& r = out[count];
    char type[12];
    char size[16] = "0";
    if (jsonStringField(entry, "name", r.name, sizeof r.name) && jsonStringField(entry, "sha", r.sha, sizeof r.sha) &&
        jsonStringField(entry, "type", type, sizeof type) && strcmp(type, "file") == 0 && safeBookName(r.name)) {
      const char* s = strstr(entry, "\"size\"");
      if (s != nullptr) {
        s = strchr(s, ':');
        if (s != nullptr) snprintf(size, sizeof size, "%s", s + 1);
      }
      r.size = static_cast<uint32_t>(strtoul(size, nullptr, 10));
      ++count;
    }
    p = next;
  }
  return count;
}

bool urlEncodePath(const char* in, char* out, size_t outSize) {
  static const char kHex[] = "0123456789ABCDEF";
  size_t o = 0;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(in); *p; ++p) {
    const unsigned char c = *p;
    const bool keep = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                      c == '-' || c == '_' || c == '.' || c == '~' || c == '/';
    if (o + (keep ? 1 : 3) >= outSize) return false;
    if (keep) {
      out[o++] = static_cast<char>(c);
    } else {
      out[o++] = '%';
      out[o++] = kHex[c >> 4];
      out[o++] = kHex[c & 15];
    }
  }
  out[o] = '\0';
  return true;
}

bool manifestNext(const char*& cursor, char* name, size_t nameSize, char* sha, size_t shaSize) {
  while (*cursor) {
    const char* line = cursor;
    const char* end = strchr(line, '\n');
    if (end == nullptr) end = line + strlen(line);
    cursor = *end ? end + 1 : end;
    const char* space = static_cast<const char*>(memchr(line, ' ', static_cast<size_t>(end - line)));
    if (space == nullptr) continue;
    const char* nameEnd = end;
    if (nameEnd > space + 1 && nameEnd[-1] == '\r') --nameEnd;
    const size_t shaLen = static_cast<size_t>(space - line);
    const size_t nameLen = static_cast<size_t>(nameEnd - (space + 1));
    if (nameLen == 0 || nameLen >= nameSize || (sha != nullptr && shaLen >= shaSize)) continue;
    memcpy(name, space + 1, nameLen);
    name[nameLen] = '\0';
    if (sha != nullptr) {
      memcpy(sha, line, shaLen);
      sha[shaLen] = '\0';
    }
    return true;
  }
  return false;
}

bool manifestFind(const char* manifest, const char* name, char* sha, size_t shaSize) {
  char n[80], s[44];
  const char* c = manifest;
  while (manifestNext(c, n, sizeof n, s, sizeof s)) {
    if (strcmp(n, name) == 0) {
      if (sha != nullptr) snprintf(sha, shaSize, "%s", s);
      return true;
    }
  }
  return false;
}
