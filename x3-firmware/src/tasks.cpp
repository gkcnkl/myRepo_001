#include "tasks.h"

#include <string.h>

const char kSampleTasksFile[] =
    "# Günün brifingi. Her satır bir görev; [x] tamamlanmış demek.\n"
    "# Ayrıntılar: README, \"brifing.txt\" bölümü.\n"
    "baslik: Bugün\n"
    "alt: Örnek dosya, SD karttan düzenle\n"
    "## Günün akışı\n"
    "[ ] 09:00-09:30 | Sabah e-postalarını kontrol et\n"
    "[ ] 10:00-11:00 | Raporun ilk taslağını yaz\n"
    "[ ] 12:30-13:00 | Öğle yürüyüşü\n"
    "[ ] 18:00-19:00 | Kitaptan bir bölüm oku\n"
    "## Bugün due\n"
    "[ ] Şirket toplantısına hazırlan\n";

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

void trim(const char*& s, size_t& n) {
  while (n > 0 && (*s == ' ' || *s == '\t')) { ++s; --n; }
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) --n;
}

// Copies at most size-1 bytes without splitting a UTF-8 sequence.
void copyText(char* dst, size_t size, const char* src, size_t n) {
  if (n >= size) {
    n = size - 1;
    while (n > 0 && (static_cast<unsigned char>(src[n]) & 0xC0) == 0x80) --n;
  }
  memcpy(dst, src, n);
  dst[n] = '\0';
}

// Splits "a | b" at the first " | "; returns false when there is none.
bool splitBar(const char* s, size_t n, size_t& leftLen, const char*& right, size_t& rightLen) {
  for (size_t i = 0; i + 2 < n; ++i) {
    if (s[i] == ' ' && s[i + 1] == '|' && s[i + 2] == ' ') {
      leftLen = i;
      right = s + i + 3;
      rightLen = n - i - 3;
      return true;
    }
  }
  return false;
}

// Matches "key:" (any of the given spellings) and returns the trimmed value.
bool keyValue(const char* line, size_t n, const char* const* keys, const char*& value, size_t& valueLen) {
  for (const char* const* k = keys; *k; ++k) {
    const size_t kl = strlen(*k);
    if (n > kl && strncmp(line, *k, kl) == 0 && line[kl] == ':') {
      value = line + kl + 1;
      valueLen = n - kl - 1;
      trim(value, valueLen);
      return true;
    }
  }
  return false;
}

const char* const kEyebrowKeys[] = {"ust", "üst", nullptr};
const char* const kTitleKeys[] = {"baslik", "başlık", nullptr};
const char* const kSubtitleKeys[] = {"alt", nullptr};
const char* const kIndicatorKeys[] = {"gosterge", "gösterge", nullptr};

}  // namespace

void TaskList::parse(const char* data, size_t len) {
  count = 0;
  indicatorCount = 0;
  sectionCount = 0;
  eyebrow[0] = title[0] = subtitle[0] = '\0';
  int currentSection = -1;

  size_t pos = 0;
  if (len >= 3 && static_cast<unsigned char>(data[0]) == 0xEF && static_cast<unsigned char>(data[1]) == 0xBB &&
      static_cast<unsigned char>(data[2]) == 0xBF) {
    pos = 3;  // UTF-8 BOM (Windows Notepad)
  }
  while (pos < len) {
    size_t end = pos;
    while (end < len && data[end] != '\n') ++end;
    const char* line = data + pos;
    size_t n = end - pos;
    pos = end + 1;
    trim(line, n);
    if (n == 0) continue;

    if (startsWith(line, n, "##")) {
      line += 2;
      n -= 2;
      trim(line, n);
      if (sectionCount < kMaxSections && n > 0) {
        copyText(sections[sectionCount], sizeof sections[0], line, n);
        currentSection = sectionCount++;
      }
      continue;
    }
    if (*line == '#') continue;

    const char* v;
    size_t vn;
    if (keyValue(line, n, kEyebrowKeys, v, vn)) { copyText(eyebrow, sizeof eyebrow, v, vn); continue; }
    if (keyValue(line, n, kTitleKeys, v, vn)) { copyText(title, sizeof title, v, vn); continue; }
    if (keyValue(line, n, kSubtitleKeys, v, vn)) { copyText(subtitle, sizeof subtitle, v, vn); continue; }
    if (keyValue(line, n, kIndicatorKeys, v, vn)) {
      if (indicatorCount >= kMaxIndicators) continue;
      Indicator& ind = indicators[indicatorCount++];
      ind.label[0] = ind.value[0] = ind.detail[0] = '\0';
      size_t l1, rest;
      const char* r;
      if (!splitBar(v, vn, l1, r, rest)) {
        copyText(ind.value, sizeof ind.value, v, vn);
        continue;
      }
      copyText(ind.label, sizeof ind.label, v, l1);
      size_t l2, rest2;
      const char* r2;
      if (splitBar(r, rest, l2, r2, rest2)) {
        copyText(ind.value, sizeof ind.value, r, l2);
        copyText(ind.detail, sizeof ind.detail, r2, rest2);
      } else {
        copyText(ind.value, sizeof ind.value, r, rest);
      }
      continue;
    }

    if (count >= kMax) continue;
    bool done = false;
    if (startsWith(line, n, "- ")) { line += 2; n -= 2; }
    if (startsWith(line, n, "[x]") || startsWith(line, n, "[X]")) { done = true; line += 3; n -= 3; }
    else if (startsWith(line, n, "[ ]")) { line += 3; n -= 3; }
    trim(line, n);
    if (n == 0) continue;

    Task& t = items[count++];
    t.done = done;
    t.section = static_cast<int8_t>(currentSection);
    t.time[0] = '\0';
    size_t tl, rn;
    const char* r;
    if (splitBar(line, n, tl, r, rn) && tl < Task::kMaxTime && rn > 0) {
      copyText(t.time, sizeof t.time, line, tl);
      copyText(t.text, sizeof t.text, r, rn);
    } else {
      copyText(t.text, sizeof t.text, line, n);
    }
  }
}

size_t TaskList::serialize(char* buf, size_t bufSize) const {
  if (bufSize == 0) return 0;
  size_t o = 0;
  auto put = [&](const char* s) {
    while (*s && o + 1 < bufSize) buf[o++] = *s++;
  };
  put("# Günün brifingi. Her satır bir görev; [x] tamamlanmış demek.\n");
  if (eyebrow[0]) { put("ust: "); put(eyebrow); put("\n"); }
  if (title[0]) { put("baslik: "); put(title); put("\n"); }
  if (subtitle[0]) { put("alt: "); put(subtitle); put("\n"); }
  for (int i = 0; i < indicatorCount; ++i) {
    put("gosterge: ");
    put(indicators[i].label);
    put(" | ");
    put(indicators[i].value);
    if (indicators[i].detail[0]) { put(" | "); put(indicators[i].detail); }
    put("\n");
  }
  int section = -1;
  for (int i = 0; i < count; ++i) {
    const Task& t = items[i];
    if (t.section != section && t.section >= 0) {
      section = t.section;
      put("## ");
      put(sections[section]);
      put("\n");
    }
    put(t.done ? "[x] " : "[ ] ");
    if (t.time[0]) { put(t.time); put(" | "); }
    put(t.text);
    put("\n");
  }
  // Keep empty sections so the file round-trips.
  for (int s = 0; s < sectionCount; ++s) {
    bool used = false;
    for (int i = 0; i < count && !used; ++i) used = items[i].section == s;
    if (!used) { put("## "); put(sections[s]); put("\n"); }
  }
  buf[o] = '\0';
  return o;
}

void TaskList::loadSample() { parse(kSampleTasksFile, strlen(kSampleTasksFile)); }
