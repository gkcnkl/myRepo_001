#pragma once
// Two-way sync with a private GitHub repository over the home Wi-Fi:
// brifing.txt (written by the Sabah Brifingi routine) comes down and is
// merged with the done marks made on the device; cihaz.txt (the device's
// view) goes up. Also sets the RTC from NTP. Device only (Wi-Fi, TLS).

#include <stddef.h>
#include <stdint.h>

#include "clock.h"
#include "tasks.h"

struct SyncSettings {
  char ssid[33] = "";
  char pass[65] = "";
  char token[128] = "";
  char repo[64] = "gkcnkl/x3-sync";
  char branch[32] = "main";
  bool configured() const { return ssid[0] && token[0] && repo[0]; }
};

// Settings live in NVS (Preferences namespace "x3sync").
void loadSyncSettings(SyncSettings& s);
void saveSyncSettings(const SyncSettings& s);

// Minutes since 2000 of the last successful sync (0 = never), in NVS.
uint32_t lastSyncMinutes();

struct SyncOutcome {
  bool ok = false;        // briefing pulled (or absent) and state pushed
  bool changed = false;   // `tasks` was replaced with the merged download
  bool clockSet = false;
  char message[64] = "";  // short Turkish status for the screen
};

// Runs a full sync. `tasks` is the device's briefing (merged in place);
// `setClock` writes the RTC. Leaves Wi-Fi off. Blocks for a few seconds.
SyncOutcome runSync(const SyncSettings& s, TaskList& tasks, bool (*setClock)(const LocalTime&));
