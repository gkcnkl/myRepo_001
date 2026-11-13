#pragma once
// Hardware-independent parts of the GitHub sync (see sync.cpp):
// merging done marks, base64 and the small JSON bits of the contents API.
#include <stddef.h>
#include <stdint.h>

#include "library.h"
#include "tasks.h"

// Carries the device's marks into `fresh` (just downloaded): a task (same
// time + text) done or cancelled in `local` stays so. A task done on the
// server stays done. Returns how many marks were carried over.
int mergeDoneMarks(TaskList& fresh, const TaskList& local);

// Standard base64 (no line breaks). Returns the output length, or 0 when
// `outSize` is too small (it needs 4 * ceil(len / 3) + 1 bytes).
size_t base64Encode(const uint8_t* in, size_t len, char* out, size_t outSize);

// Finds "key":"value" in a flat JSON object and copies the value (no
// unescaping beyond \" and \\). Returns false when missing or too long.
bool jsonStringField(const char* json, const char* key, char* out, size_t outSize);

// Body for PUT /repos/{repo}/contents/{path}: message, base64 content,
// branch and (when updating) the file's current sha.
bool buildPutBody(const char* message, const char* base64, const char* sha, const char* branch, char* out,
                  size_t outSize);

// What the device writes back: a stamp line, the briefing as it knows it and
// (when `lib` is given) one comment line per book with its reading state:
//   # kitap | %42 | okunuyor | 2026-11-11 07:02 | <file name>
// ("bitti" once the last page was reached, "baslanmadi" before the first read).
// The task parser skips '#' lines, so the file still reads as a briefing.
size_t buildDeviceStateFile(const TaskList& tasks, const Library* lib, const char* stamp, char* out,
                            size_t outSize);

// Picks which saved network to join from a Wi-Fi scan: the saved one seen
// with the strongest signal. `saved` holds up to 2 names ("" = unused);
// returns its index, or -1 when none of them is in the scan.
int pickNetwork(const char* const saved[2], const char* const seen[], const int rssi[], int seenCount);

// --- Books from the repository's kitaplar/ folder ------------------------------

struct RemoteBook {
  char name[80];
  char sha[44];
  uint32_t size;
};

// A repository file name the device may store: a readable book (.epub/.txt),
// no path parts, no hidden or control characters, fits BookInfo::file.
bool safeBookName(const char* name);

// Reads a contents-API directory listing (JSON array) and keeps the files
// that are safe book names. Returns how many were stored in `out`.
int parseDirListing(const char* json, RemoteBook* out, int max);

// Percent-encodes a repository path for a URL (keeps '/' and unreserved).
// Returns false when `out` is too small.
bool urlEncodePath(const char* in, char* out, size_t outSize);

// The list of books that came from the repository, on the SD card as
// "<sha> <name>" lines. Finds `name` and copies its sha (when `sha` is set).
bool manifestFind(const char* manifest, const char* name, char* sha, size_t shaSize);
// Walks the manifest: copies the next line's name (and sha); returns false at the end.
bool manifestNext(const char*& cursor, char* name, size_t nameSize, char* sha, size_t shaSize);
