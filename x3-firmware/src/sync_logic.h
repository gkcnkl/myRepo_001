#pragma once
// Hardware-independent parts of the GitHub sync (see sync.cpp):
// merging done marks, base64 and the small JSON bits of the contents API.
#include <stddef.h>
#include <stdint.h>

#include "tasks.h"

// Marks tasks in `fresh` (just downloaded) as done when the same task
// (time + text) is done in `local`. Done marks are never lost; returns how
// many tasks were carried over.
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

// What the device writes back: a stamp line, then the briefing as it knows it.
size_t buildDeviceStateFile(const TaskList& tasks, const char* stamp, char* out, size_t outSize);
