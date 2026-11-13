#include "sync.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <SDCardManager.h>
#include <WiFi.h>
#include <time.h>

#include <new>

#include "github_roots.h"
#include "sync_logic.h"

namespace {

constexpr const char* kNamespace = "x3sync";
constexpr uint32_t kWifiTimeoutMs = 15000;
constexpr uint32_t kNtpTimeoutMs = 6000;
constexpr long kTurkeyOffsetSec = 3 * 3600;  // UTC+3 all year
constexpr size_t kMaxFile = 8192;
constexpr const char* kBooksDir = "/kitaplar";
constexpr const char* kStateDir = "/kitaplar/.x3";
constexpr const char* kManifestPath = "/kitaplar/.x3/esitlenen.txt";
constexpr const char* kDownloadTmp = "/kitaplar/.x3/indirme.tmp";
constexpr int kMaxRemoteBooks = 30;
constexpr int kMaxDownloadsPerSync = 5;
constexpr uint32_t kMaxBookBytes = 8u * 1024 * 1024;
constexpr size_t kMaxListing = 40 * 1024;

uint32_t hashText(const char* s) {
  uint32_t h = 2166136261u;
  for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
  return h;
}

void stamp(char* out, size_t n, const LocalTime& t) {
  snprintf(out, n, "%04u-%02u-%02u %02u:%02u", t.year, t.month, t.day, t.hour, t.minute);
}

// Opens a contents-API request for `path` (percent-encoded here).
bool beginGithub(HTTPClient& http, NetworkClientSecure& client, const SyncSettings& s, bool get, const char* path,
                 const char* accept) {
  client.setCACert(kGithubRootCAs);  // verify api.github.com: the token stays private
  client.setTimeout(15);
  char encoded[300];
  if (!urlEncodePath(path, encoded, sizeof encoded)) return false;
  char url[420];
  snprintf(url, sizeof url, "https://api.github.com/repos/%s/contents/%s%s%s", s.repo, encoded, get ? "?ref=" : "",
           get ? s.branch : "");
  if (!http.begin(client, url)) return false;
  char auth[160];
  snprintf(auth, sizeof auth, "Bearer %s", s.token);
  http.addHeader("Authorization", auth);
  http.addHeader("Accept", accept);
  http.addHeader("User-Agent", "x3-bugun");
  http.addHeader("X-GitHub-Api-Version", "2022-11-28");
  return true;
}

// One HTTPS request to the contents API. Returns the HTTP status (negative on
// a connection error) and the body in `body`.
int github(const SyncSettings& s, const char* method, const char* path, const char* accept, const char* payload,
           String& body) {
  NetworkClientSecure client;
  HTTPClient http;
  if (!beginGithub(http, client, s, strcmp(method, "GET") == 0, path, accept)) return -1;
  int code;
  if (strcmp(method, "PUT") == 0) {
    http.addHeader("Content-Type", "application/json");
    code = http.sendRequest("PUT", reinterpret_cast<uint8_t*>(const_cast<char*>(payload)), strlen(payload));
  } else {
    code = http.GET();
  }
  body = code > 0 ? http.getString() : String();
  http.end();
  return code;
}

const char* httpMessage(int code) {
  if (code == 401) return "GitHub anahtarı geçersiz";
  if (code == 403) return "GitHub erişim izni yok";
  if (code == 404) return "Depo ya da dal bulunamadı";
  if (code < 0) return "GitHub'a bağlanılamadı";
  return "GitHub hatası";
}

// Streams one book from the repository to /kitaplar/<name> (through a temp
// file, so a broken download never replaces a good book).
bool downloadBook(const SyncSettings& s, const RemoteBook& r) {
  char path[120];
  snprintf(path, sizeof path, "kitaplar/%s", r.name);
  NetworkClientSecure client;
  HTTPClient http;
  if (!beginGithub(http, client, s, true, path, "application/vnd.github.raw+json")) return false;
  http.setTimeout(20000);
  const int code = http.GET();
  bool ok = false;
  if (code == 200) {
    SdMan.remove(kDownloadTmp);
    FsFile f = SdMan.open(kDownloadTmp, O_WRONLY | O_CREAT | O_TRUNC);
    if (f) {
      const int expected = http.getSize();
      const int written = http.writeToStream(&f);
      f.close();
      ok = written > 0 && (expected < 0 || written == expected) && (r.size == 0 || static_cast<uint32_t>(written) == r.size);
    }
    if (ok) {
      char target[120];
      snprintf(target, sizeof target, "%s/%s", kBooksDir, r.name);
      SdMan.remove(target);
      ok = SdMan.rename(kDownloadTmp, target);
    }
    if (!ok) SdMan.remove(kDownloadTmp);
  }
  http.end();
  Serial.printf("[x3] book %s: http %d %s\n", r.name, code, ok ? "ok" : "failed");
  return ok;
}

// Removes a book that came from the repository, with its state and page cache.
void removeSyncedBook(const char* name) {
  char path[120];
  snprintf(path, sizeof path, "%s/%s", kBooksDir, name);
  SdMan.remove(path);
  BookInfo b;
  if (bookFromFileName(name, 0, b)) {
    snprintf(path, sizeof path, "%s/%s", kStateDir, b.id);
    SdMan.removeDir(path);
  }
  Serial.printf("[x3] book %s removed (gone from the repository)\n", name);
}

// Mirrors the repository's kitaplar/ folder into /kitaplar: new or changed
// books come down, books that came from the repository and are gone there
// are removed. Books uploaded over Wi-Fi are never touched.
struct BookSyncResult {
  int added = 0;
  int removed = 0;
  int failed = 0;
  bool listed = false;
};

BookSyncResult syncBooks(const SyncSettings& s) {
  BookSyncResult res;
  RemoteBook* remote = new (std::nothrow) RemoteBook[kMaxRemoteBooks];
  if (remote == nullptr) return res;
  int count = 0;
  bool complete = false;  // the listing holds every book (safe to remove the rest)
  {
    String body;
    const int code = github(s, "GET", "kitaplar", "application/vnd.github+json", nullptr, body);
    if (code == 200 && body.length() > 0 && body.length() < kMaxListing) {
      count = parseDirListing(body.c_str(), remote, kMaxRemoteBooks);
      complete = count < kMaxRemoteBooks;
      res.listed = true;
    } else if (code == 404) {
      complete = true;  // no kitaplar/ folder: no books from the repository
      res.listed = true;
    }
    Serial.printf("[x3] kitaplar/: http %d, %d books\n", code, count);
  }
  if (!res.listed) {
    delete[] remote;
    return res;
  }

  SdMan.ensureDirectoryExists(kStateDir);
  const String old = SdMan.readFile(kManifestPath);
  String fresh;
  int downloads = 0;
  for (int i = 0; i < count; ++i) {
    const RemoteBook& r = remote[i];
    char oldSha[44] = "";
    const bool known = manifestFind(old.c_str(), r.name, oldSha, sizeof oldSha);
    char path[120];
    snprintf(path, sizeof path, "%s/%s", kBooksDir, r.name);
    const bool present = SdMan.exists(path);
    if (known && present && strcmp(oldSha, r.sha) == 0) {
      fresh += String(r.sha) + " " + r.name + "\n";
      continue;
    }
    bool ok = false;
    if (r.size <= kMaxBookBytes && downloads < kMaxDownloadsPerSync) {
      ++downloads;
      ok = downloadBook(s, r);
    }
    if (ok) {
      ++res.added;
      fresh += String(r.sha) + " " + r.name + "\n";
    } else {
      ++res.failed;
      if (known) fresh += String(oldSha) + " " + r.name + "\n";  // still ours; retried next time
    }
  }
  if (complete) {
    const char* c = old.c_str();
    char name[80], sha[44];
    while (manifestNext(c, name, sizeof name, sha, sizeof sha)) {
      bool inRepo = false;
      for (int i = 0; i < count && !inRepo; ++i) inRepo = strcmp(remote[i].name, name) == 0;
      if (!inRepo) {
        removeSyncedBook(name);
        ++res.removed;
      }
    }
  } else {
    // Partial listing: keep remembering books we could not see this time.
    const char* c = old.c_str();
    char name[80], sha[44];
    while (manifestNext(c, name, sizeof name, sha, sizeof sha)) {
      if (!manifestFind(fresh.c_str(), name, nullptr, 0)) fresh += String(sha) + " " + name + "\n";
    }
  }
  if (fresh != old) SdMan.writeFile(kManifestPath, fresh);
  delete[] remote;
  return res;
}

}  // namespace

void loadSyncSettings(SyncSettings& s) {
  Preferences p;
  if (!p.begin(kNamespace, true)) return;
  p.getString("ssid", s.ssid, sizeof s.ssid);
  p.getString("pass", s.pass, sizeof s.pass);
  if (p.isKey("ssid2")) p.getString("ssid2", s.ssid2, sizeof s.ssid2);
  if (p.isKey("pass2")) p.getString("pass2", s.pass2, sizeof s.pass2);
  p.getString("token", s.token, sizeof s.token);
  if (p.isKey("repo")) p.getString("repo", s.repo, sizeof s.repo);
  if (p.isKey("branch")) p.getString("branch", s.branch, sizeof s.branch);
  p.end();
}

void saveSyncSettings(const SyncSettings& s) {
  Preferences p;
  if (!p.begin(kNamespace, false)) return;
  p.putString("ssid", s.ssid);
  p.putString("pass", s.pass);
  p.putString("ssid2", s.ssid2);
  p.putString("pass2", s.pass2);
  p.putString("token", s.token);
  p.putString("repo", s.repo);
  p.putString("branch", s.branch);
  p.end();
}

uint32_t lastSyncMinutes() {
  Preferences p;
  if (!p.begin(kNamespace, true)) return 0;
  const uint32_t v = p.getUInt("last", 0);
  p.end();
  return v;
}

SyncOutcome runSync(const SyncSettings& s, TaskList& tasks, const Library* lib, bool (*setClock)(const LocalTime&)) {
  SyncOutcome out;
  if (!s.configured()) {
    snprintf(out.message, sizeof out.message, "Eşitleme ayarlanmadı");
    return out;
  }

  // Join whichever saved network is around (home or phone), strongest first.
  WiFi.mode(WIFI_STA);
  const char* saved[2] = {s.ssid, s.ssid2};
  const char* pass[2] = {s.pass, s.pass2};
  int pick = -1;
  const int found = WiFi.scanNetworks(false, true);
  if (found > 0) {
    const int n = found > 24 ? 24 : found;
    String names[24];
    const char* seen[24];
    int rssi[24];
    for (int i = 0; i < n; ++i) {
      names[i] = WiFi.SSID(i);
      seen[i] = names[i].c_str();
      rssi[i] = WiFi.RSSI(i);
    }
    pick = pickNetwork(saved, seen, rssi, n);
  }
  WiFi.scanDelete();
  // Not in the scan (hidden network, or the scan failed): try them in order.
  int order[2] = {pick, -1};
  if (pick < 0) {
    order[0] = s.ssid[0] ? 0 : 1;
    order[1] = (order[0] == 0 && s.ssid2[0]) ? 1 : -1;
  }
  bool joined = false;
  wl_status_t last = WL_IDLE_STATUS;
  for (int k = 0; k < 2 && !joined && order[k] >= 0; ++k) {
    WiFi.begin(saved[order[k]], pass[order[k]]);
    const uint32_t start = millis();
    while ((last = WiFi.status()) != WL_CONNECTED && millis() - start < kWifiTimeoutMs) delay(100);
    joined = last == WL_CONNECTED;
    if (joined) Serial.printf("[x3] joined %s\n", saved[order[k]]);
    else WiFi.disconnect();
  }
  if (!joined) {
    if (pick < 0 && found >= 0) {
      snprintf(out.message, sizeof out.message, "Kayıtlı Wi-Fi bulunamadı");
    } else if (last == WL_CONNECT_FAILED || last == WL_NO_SSID_AVAIL) {
      snprintf(out.message, sizeof out.message, "Wi-Fi şifresi yanlış olabilir");
    } else {
      snprintf(out.message, sizeof out.message, "Wi-Fi'a bağlanılamadı");
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return out;
  }

  // Clock from NTP (Türkiye: UTC+3, no daylight saving).
  configTime(kTurkeyOffsetSec, 0, "pool.ntp.org", "time.google.com");
  LocalTime now;
  bool haveTime = false;
  const uint32_t ntpStart = millis();
  while (millis() - ntpStart < kNtpTimeoutMs) {
    const time_t t = time(nullptr);
    if (t > 1700000000) {
      struct tm tmv;
      localtime_r(&t, &tmv);
      now.year = static_cast<uint16_t>(tmv.tm_year + 1900);
      now.month = static_cast<uint8_t>(tmv.tm_mon + 1);
      now.day = static_cast<uint8_t>(tmv.tm_mday);
      now.hour = static_cast<uint8_t>(tmv.tm_hour);
      now.minute = static_cast<uint8_t>(tmv.tm_min);
      now.second = static_cast<uint8_t>(tmv.tm_sec);
      haveTime = isPlausible(now);
      break;
    }
    delay(100);
  }
  if (haveTime && setClock != nullptr) out.clockSet = setClock(now);

  // 1) Pull brifing.txt and merge the device's done marks into it.
  String body;
  int code = github(s, "GET", "brifing.txt", "application/vnd.github.raw+json", nullptr, body);
  if (code == 200 && body.length() > 0 && body.length() < kMaxFile) {
    // Heap, not static: static RAM is the reader's budget.
    TaskList* fresh = new (std::nothrow) TaskList();
    if (fresh != nullptr) {
      fresh->parse(body.c_str(), body.length());
      mergeDoneMarks(*fresh, tasks);
      tasks = *fresh;
      delete fresh;
      out.changed = true;
    }
  } else if (code != 404) {
    snprintf(out.message, sizeof out.message, "%s (%d)", httpMessage(code), code);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return out;
  }
  body = String();

  // 2) Books: mirror the repository's kitaplar/ folder (needs the SD card).
  if (lib != nullptr) {
    const BookSyncResult books = syncBooks(s);
    out.booksAdded = books.added;
    out.booksRemoved = books.removed;
    out.booksFailed = books.failed;
  }

  // 3) Push cihaz.txt when the device's view changed since the last push.
  char* state = static_cast<char*>(malloc(kMaxFile));
  char* b64 = static_cast<char*>(malloc(kMaxFile * 4 / 3 + 8));
  char* put = static_cast<char*>(malloc(kMaxFile * 4 / 3 + 512));
  bool pushed = false;
  if (state != nullptr && b64 != nullptr && put != nullptr) {
    // The hash covers the briefing and the books' reading state (no stamp).
    const size_t stateLen = buildDeviceStateFile(tasks, lib, "", state, kMaxFile);
    const uint32_t h = stateLen > 0 ? hashText(state) : 0;
    Preferences p;
    p.begin(kNamespace, true);
    const uint32_t lastHash = p.getUInt("pushed", 0);
    p.end();
    if (h == lastHash) {
      pushed = true;  // nothing new to tell the routine
    } else {
      char when[24] = "saat bilinmiyor";
      if (haveTime) stamp(when, sizeof when, now);
      const size_t n = buildDeviceStateFile(tasks, lib, when, state, kMaxFile);
      String meta;
      char sha[48] = "";
      const int metaCode = github(s, "GET", "cihaz.txt", "application/vnd.github+json", nullptr, meta);
      if (metaCode == 200) jsonStringField(meta.c_str(), "sha", sha, sizeof sha);
      meta = String();
      if ((metaCode == 200 || metaCode == 404) &&
          base64Encode(reinterpret_cast<const uint8_t*>(state), n, b64, kMaxFile * 4 / 3 + 8) > 0 &&
          buildPutBody("X3: cihaz durumu", b64, sha, s.branch, put, kMaxFile * 4 / 3 + 512)) {
        String resp;
        code = github(s, "PUT", "cihaz.txt", "application/vnd.github+json", put, resp);
        pushed = code == 200 || code == 201;
        if (pushed) {
          p.begin(kNamespace, false);
          p.putUInt("pushed", h);
          p.end();
        } else {
          snprintf(out.message, sizeof out.message, "Gönderilemedi: %s (%d)", httpMessage(code), code);
        }
      } else if (metaCode != 200 && metaCode != 404) {
        snprintf(out.message, sizeof out.message, "%s (%d)", httpMessage(metaCode), metaCode);
      }
    }
  } else {
    snprintf(out.message, sizeof out.message, "Eşitleme için bellek yetmedi");
  }
  free(state);
  free(b64);
  free(put);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  if (pushed) {
    out.ok = true;
    char extra[32] = "";
    if (out.booksAdded > 0) snprintf(extra, sizeof extra, " · %d yeni kitap", out.booksAdded);
    else if (out.booksFailed > 0) snprintf(extra, sizeof extra, " · %d kitap inmedi", out.booksFailed);
    if (haveTime) {
      snprintf(out.message, sizeof out.message, "Eşitlendi %02u:%02u%s", now.hour, now.minute, extra);
      Preferences p;
      p.begin(kNamespace, false);
      p.putUInt("last", minutesSince2000(now));
      p.end();
    } else {
      snprintf(out.message, sizeof out.message, "Eşitlendi%s", extra);
    }
  }
  return out;
}
