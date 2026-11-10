#include "tasks.h"

#include <string.h>

const char kSampleTasksFile[] =
    "# Bugünün görevleri. Her satır bir görev.\n"
    "# [x] ile başlayan satırlar tamamlanmış sayılır.\n"
    "[ ] Sabah e-postalarını kontrol et\n"
    "[ ] Raporun ilk taslağını yaz\n"
    "[ ] Öğle yürüyüşü: 20 dakika\n"
    "[ ] Şirket toplantısına hazırlan\n"
    "[ ] Kitaptan bir bölüm oku\n";

int TaskList::doneCount() const {
  int n = 0;
  for (int i = 0; i < count; ++i) n += items[i].done ? 1 : 0;
  return n;
}

namespace {

bool startsWith(const char* s, size_t len, const char* prefix) {
  const size_t n = strlen(prefix);
  return len >= n && strncmp(s, prefix, n) == 0;
}

}  // namespace

void TaskList::parse(const char* data, size_t len) {
  count = 0;
  size_t pos = 0;
  // Skip a UTF-8 BOM (Windows Notepad adds one).
  if (len >= 3 && static_cast<unsigned char>(data[0]) == 0xEF && static_cast<unsigned char>(data[1]) == 0xBB &&
      static_cast<unsigned char>(data[2]) == 0xBF) {
    pos = 3;
  }
  while (pos < len && count < kMax) {
    size_t end = pos;
    while (end < len && data[end] != '\n') ++end;
    const char* line = data + pos;
    size_t n = end - pos;
    pos = end + 1;

    while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t')) --n;
    while (n > 0 && (*line == ' ' || *line == '\t')) { ++line; --n; }
    if (n == 0 || *line == '#') continue;

    bool done = false;
    if (startsWith(line, n, "- ")) { line += 2; n -= 2; }
    if (startsWith(line, n, "[x]") || startsWith(line, n, "[X]")) { done = true; line += 3; n -= 3; }
    else if (startsWith(line, n, "[ ]")) { line += 3; n -= 3; }
    while (n > 0 && *line == ' ') { ++line; --n; }
    if (n == 0) continue;

    Task& t = items[count++];
    if (n >= Task::kMaxText) {
      n = Task::kMaxText - 1;
      // Don't cut a UTF-8 sequence in half.
      while (n > 0 && (static_cast<unsigned char>(line[n]) & 0xC0) == 0x80) --n;
    }
    memcpy(t.text, line, n);
    t.text[n] = '\0';
    t.done = done;
  }
}

size_t TaskList::serialize(char* buf, size_t bufSize) const {
  if (bufSize == 0) return 0;
  size_t o = 0;
  auto put = [&](const char* s) {
    while (*s && o + 1 < bufSize) buf[o++] = *s++;
  };
  put("# Bugünün görevleri. Her satır bir görev.\n");
  put("# [x] ile başlayan satırlar tamamlanmış sayılır.\n");
  for (int i = 0; i < count; ++i) {
    put(items[i].done ? "[x] " : "[ ] ");
    put(items[i].text);
    put("\n");
  }
  buf[o] = '\0';
  return o;
}

void TaskList::loadSample() { parse(kSampleTasksFile, strlen(kSampleTasksFile)); }
