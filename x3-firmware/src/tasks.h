#pragma once
#include <stddef.h>

// Today's task list, stored on the SD card as /gorevler.txt (UTF-8):
//   [ ] Not done task
//   [x] Done task
//   plain line          -> not done
//   # comment / blank   -> ignored
struct Task {
  static constexpr size_t kMaxText = 120;
  char text[kMaxText];
  bool done;
};

struct TaskList {
  static constexpr int kMax = 30;
  Task items[kMax];
  int count = 0;

  int doneCount() const;
  // Parses file content (need not be NUL-terminated). Extra tasks are dropped.
  void parse(const char* data, size_t len);
  // Writes the file content; returns bytes written (excluding NUL), truncating
  // at bufSize-1.
  size_t serialize(char* buf, size_t bufSize) const;
  void loadSample();
};

extern const char kSampleTasksFile[];
