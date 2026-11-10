#include "sync.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
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

uint32_t hashText(const char* s) {
  uint32_t h = 2166136261u;
  for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
  return h;
}

void stamp(char* out, size_t n, const LocalTime& t) {
  snprintf(out, n, "%04u-%02u-%02u %02u:%02u", t.year, t.month, t.day, t.hour, t.minute);
}

// One HTTPS request to the contents API. Returns the HTTP status (negative on
// a connection error) and the body in `body`.
int github(const SyncSettings& s, const char* method, const char* path, const char* accept, const char* payload,
           String& body) {
  NetworkClientSecure client;
  client.setCACert(kGithubRootCAs);  // verify api.github.com: the token stays private
  client.setTimeout(15);
  HTTPClient http;
  char url[200];
  snprintf(url, sizeof url, "https://api.github.com/repos/%s/contents/%s%s%s", s.repo, path,
           strcmp(method, "GET") == 0 ? "?ref=" : "", strcmp(method, "GET") == 0 ? s.branch : "");
  if (!http.begin(client, url)) return -1;
  char auth[160];
  snprintf(auth, sizeof auth, "Bearer %s", s.token);
  http.addHeader("Authorization", auth);
  http.addHeader("Accept", accept);
  http.addHeader("User-Agent", "x3-bugun");
  http.addHeader("X-GitHub-Api-Version", "2022-11-28");
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

}  // namespace

void loadSyncSettings(SyncSettings& s) {
  Preferences p;
  if (!p.begin(kNamespace, true)) return;
  p.getString("ssid", s.ssid, sizeof s.ssid);
  p.getString("pass", s.pass, sizeof s.pass);
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

SyncOutcome runSync(const SyncSettings& s, TaskList& tasks, bool (*setClock)(const LocalTime&)) {
  SyncOutcome out;
  if (!s.configured()) {
    snprintf(out.message, sizeof out.message, "Eşitleme ayarlanmadı");
    return out;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(s.ssid, s.pass);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < kWifiTimeoutMs) delay(100);
  if (WiFi.status() != WL_CONNECTED) {
    snprintf(out.message, sizeof out.message, "Ev Wi-Fi'ına bağlanılamadı");
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

  // 2) Push cihaz.txt when the device's view changed since the last push.
  char* state = static_cast<char*>(malloc(kMaxFile));
  char* b64 = static_cast<char*>(malloc(kMaxFile * 4 / 3 + 8));
  char* put = static_cast<char*>(malloc(kMaxFile * 4 / 3 + 512));
  bool pushed = false;
  if (state != nullptr && b64 != nullptr && put != nullptr) {
    tasks.serialize(state, kMaxFile);
    const uint32_t h = hashText(state);
    Preferences p;
    p.begin(kNamespace, true);
    const uint32_t lastHash = p.getUInt("pushed", 0);
    p.end();
    if (h == lastHash) {
      pushed = true;  // nothing new to tell the routine
    } else {
      char when[24] = "saat bilinmiyor";
      if (haveTime) stamp(when, sizeof when, now);
      const size_t n = buildDeviceStateFile(tasks, when, state, kMaxFile);
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
    if (haveTime) {
      snprintf(out.message, sizeof out.message, "Eşitlendi %02u:%02u", now.hour, now.minute);
      Preferences p;
      p.begin(kNamespace, false);
      p.putUInt("last", minutesSince2000(now));
      p.end();
    } else {
      snprintf(out.message, sizeof out.message, "Eşitlendi");
    }
  }
  return out;
}
