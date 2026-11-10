#include "clock.h"

#include <stdio.h>
#include <string.h>

namespace {

const char* const kDays[] = {"Pazar", "Pazartesi", "Salı", "Çarşamba", "Perşembe", "Cuma", "Cumartesi"};
const char* const kMonths[] = {"Ocak",  "Şubat", "Mart",  "Nisan",  "Mayıs", "Haziran",
                               "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık"};

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int daysInMonth(int y, int m) {
  static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return m == 2 && isLeap(y) ? 29 : d[m - 1];
}

// Reads an unsigned number of 1..maxDigits digits.
bool number(const char*& p, const char* end, int maxDigits, int& out) {
  int n = 0, v = 0;
  while (p < end && *p >= '0' && *p <= '9' && n < maxDigits) {
    v = v * 10 + (*p++ - '0');
    ++n;
  }
  out = v;
  return n > 0;
}

bool expect(const char*& p, const char* end, char c) {
  if (p < end && *p == c) {
    ++p;
    return true;
  }
  return false;
}

}  // namespace

int weekdayOf(int year, int month, int day) {
  // Sakamoto's method.
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) year -= 1;
  return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

bool isPlausible(const LocalTime& t) {
  if (t.year < 2025 || t.year > 2099) return false;
  if (t.month < 1 || t.month > 12) return false;
  if (t.day < 1 || t.day > daysInMonth(t.year, t.month)) return false;
  return t.hour < 24 && t.minute < 60 && t.second < 60;
}

void formatTurkishDate(const LocalTime& t, char* out, size_t outSize) {
  if (outSize == 0) return;
  if (!isPlausible(t)) {
    out[0] = '\0';
    return;
  }
  snprintf(out, outSize, "%s, %d %s", kDays[weekdayOf(t.year, t.month, t.day)], t.day, kMonths[t.month - 1]);
}

bool parseSetTime(const char* data, size_t len, LocalTime& out) {
  const char* p = data;
  const char* end = data + len;
  if (len >= 3 && static_cast<unsigned char>(p[0]) == 0xEF && static_cast<unsigned char>(p[1]) == 0xBB &&
      static_cast<unsigned char>(p[2]) == 0xBF) {
    p += 3;
  }
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;

  int a, b, c;
  if (!number(p, end, 4, a)) return false;
  LocalTime t;
  if (expect(p, end, '-')) {  // 2026-11-10
    if (!number(p, end, 2, b) || !expect(p, end, '-') || !number(p, end, 2, c)) return false;
    t.year = a, t.month = b, t.day = c;
  } else if (expect(p, end, '.')) {  // 10.11.2026
    if (!number(p, end, 2, b) || !expect(p, end, '.') || !number(p, end, 4, c)) return false;
    t.day = a, t.month = b, t.year = c;
  } else {
    return false;
  }
  if (!expect(p, end, ' ') && !expect(p, end, 'T')) return false;
  while (p < end && *p == ' ') ++p;
  int h, m, s = 0;
  if (!number(p, end, 2, h) || !expect(p, end, ':') || !number(p, end, 2, m)) return false;
  if (expect(p, end, ':') && !number(p, end, 2, s)) return false;
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
  if (p != end) return false;
  t.hour = h, t.minute = m, t.second = s;
  if (!isPlausible(t)) return false;
  out = t;
  return true;
}
