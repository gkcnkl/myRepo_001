#pragma once
// The bookshelf: books found in /kitaplar on the SD card, their reading state
// and the shelf layout (spines side by side). Hardware independent.

#include <stddef.h>
#include <stdint.h>

struct DateStamp {
  uint16_t year = 0;  // 0 = unknown
  uint8_t month = 0;
  uint8_t day = 0;
  bool known() const { return year != 0; }
};

struct BookInfo {
  char file[80];      // file name inside /kitaplar
  char id[12];        // FNV-1a hash of the name: state + cache folder
  char title[96];     // from the book, or derived from the file name
  char author[64];
  uint32_t size;      // bytes, sets the spine thickness
  bool isText;        // .txt instead of .epub
  // Reading state (persisted in /kitaplar/.x3/<id>/durum.txt).
  DateStamp started;  // first opened
  uint64_t lastRead;  // yyyymmddhhmm, 0 = never; sorts the lending card
  uint16_t spine;
  uint32_t charOffset;
  uint8_t percent;
  bool finished;
};

// One readable file name -> BookInfo with defaults (title from the name).
bool bookFromFileName(const char* name, uint32_t size, BookInfo& out);

// Reading state file: "key: value" lines (title, author, started, lastRead,
// spine, char, percent, finished).
void parseBookState(const char* data, size_t len, BookInfo& b);
size_t serializeBookState(const BookInfo& b, char* buf, size_t bufSize);

struct Library {
  static constexpr int kMax = 40;
  BookInfo books[kMax];
  int count = 0;

  // Sorts by title (Turkish-aware enough: byte order after case folding).
  void sortByTitle();
  // Books started but not finished, most recently read first; returns count.
  int inProgress(const BookInfo** out, int max) const;
};

// --- Shelf layout -------------------------------------------------------------
// Items on the shelves are the books plus a trailing "+ Ekle" spine.
struct SpineBox {
  int16_t x, y, w, h;  // y = top; the spine stands on its shelf board
  int shelf;
};

struct ShelfLayout {
  static constexpr int16_t kLeft = 28;
  static constexpr int16_t kRight = 500;
  static constexpr int16_t kTop = 150;         // first shelf's top
  static constexpr int16_t kShelfH = 160;      // spine room + board
  static constexpr int16_t kBoard = 6;         // shelf board thickness
  static constexpr int16_t kMaxSpineH = 144;
  static constexpr int kShelves = 3;           // visible at once
  static constexpr int16_t kGap = 3;
  static constexpr int16_t kAddW = 40;         // the "+ Ekle" spine

  // Spine thickness from the file size; height varies a little per book.
  static int16_t spineWidth(const BookInfo& b);
  static int16_t spineHeight(const BookInfo& b);

  // Places items 0..count (count = the "+" spine) left to right, wrapping to
  // new shelves. Returns the number of shelves used.
  static int place(const Library& lib, SpineBox* boxes, int maxBoxes);
};
