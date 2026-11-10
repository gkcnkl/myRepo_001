#pragma once
#include <stddef.h>
#include <stdint.h>

// Date helpers, hardware independent. The RTC keeps local (Türkiye) time.
struct LocalTime {
  uint16_t year = 0;
  uint8_t month = 0;  // 1-12
  uint8_t day = 0;    // 1-31
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
};

// 0 = Sunday .. 6 = Saturday.
int weekdayOf(int year, int month, int day);

// True for a real calendar date in a sane year range (the RTC reads 2000-01-01
// when it was never set).
bool isPlausible(const LocalTime& t);

// "Salı, 10 Kasım" (UTF-8). Empty string when the date is not plausible.
void formatTurkishDate(const LocalTime& t, char* out, size_t outSize);

// Parses the /saat.txt content: "10.11.2026 15:30" or "2026-11-10 15:30"
// (seconds optional, surrounding whitespace / BOM ignored).
bool parseSetTime(const char* data, size_t len, LocalTime& out);
