#include "sync_logic.h"

#include <stdio.h>
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

size_t buildDeviceStateFile(const TaskList& tasks, const char* stamp, char* out, size_t outSize) {
  const int n = snprintf(out, outSize, "# X3 cihaz durumu. guncellendi: %s\n", stamp);
  if (n < 0 || static_cast<size_t>(n) >= outSize) return 0;
  return static_cast<size_t>(n) + tasks.serialize(out + n, outSize - n);
}
