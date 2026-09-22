#include "google_calendar.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <ctype.h>
#include "app_config.h"
#include "secrets.h"

// =========================================================================
//  Google Trust Services "GTS Root R1" -- the CA that signs both
//  accounts.google.com and www.googleapis.com's certificates. Public PKI
//  material, safe to hardcode; fetched from https://pki.goog/repo/certs/gtsr1.pem.
// =========================================================================
static const char GTS_ROOT_R1_PEM[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIFVzCCAz+gAwIBAgINAgPlk28xsBNJiGuiFzANBgkqhkiG9w0BAQwFADBHMQsw\n"
"CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU\n"
"MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAw\n"
"MDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp\n"
"Y2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEBAQUA\n"
"A4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaMf/vo\n"
"27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vXmX7w\n"
"Cl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7zUjw\n"
"TcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0Pfybl\n"
"qAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtcvfaH\n"
"szVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4Zor8\n"
"Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUspzBmk\n"
"MiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOORc92\n"
"wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYWk70p\n"
"aDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+DVrN\n"
"VjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgFlQID\n"
"AQABo0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4E\n"
"FgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBAJ+qQibb\n"
"C5u+/x6Wki4+omVKapi6Ist9wTrYggoGxval3sBOh2Z5ofmmWJyq+bXmYOfg6LEe\n"
"QkEzCzc9zolwFcq1JKjPa7XSQCGYzyI0zzvFIoTgxQ6KfF2I5DUkzps+GlQebtuy\n"
"h6f88/qBVRRiClmpIgUxPoLW7ttXNLwzldMXG+gnoot7TiYaelpkttGsN/H9oPM4\n"
"7HLwEXWdyzRSjeZ2axfG34arJ45JK3VmgRAhpuo+9K4l/3wV3s6MJT/KYnAK9y8J\n"
"ZgfIPxz88NtFMN9iiMG1D53Dn0reWVlHxYciNuaCp+0KueIHoI17eko8cdLiA6Ef\n"
"MgfdG+RCzgwARWGAtQsgWSl4vflVy2PFPEz0tv/bal8xa5meLMFrUKTX5hgUvYU/\n"
"Z6tGn6D/Qqc6f1zLXbBwHSs09dR2CQzreExZBfMzQsNhFRAbd03OIozUhfJFfbdT\n"
"6u9AWpQKXCBfTkBdYiJ23//OYb2MI3jSNwLgjt7RETeJ9r/tSQdirpLsQBqvFAnZ\n"
"0E6yove+7u7Y/9waLd64NnHi/Hm3lCXRSHNboTXns5lndcEZOitHTtNCjv0xyBZm\n"
"2tIMPNuzjsmhDYAPexZ3FL//2wmUspO8IFgV6dtxQ/PeEMMA3KgqlbbC1j+Qa3bb\n"
"bP6MvPJwNQzcmRk13NfIRmPVNnGuV/u3gm3c\n"
"-----END CERTIFICATE-----\n";

// =========================================================================
//  RFC3339 -> UTC epoch, without touching the system timezone.
//
//  Parses "YYYY-MM-DDTHH:MM:SS[.fff](Z|+HH:MM|-HH:MM)" and computes the
//  epoch directly via Howard Hinnant's days_from_civil formula rather than
//  mktime()/timegm() -- avoids any dependency on libc TZ state, which this
//  app never sets (see app_config.h: everything is compared as raw UTC
//  epoch seconds, so no local timezone is needed anywhere).
// =========================================================================
static long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= (m <= 2);
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);                       // [0, 399]
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;    // [0, 365]
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;             // [0, 146096]
  return era * 146097L + (long)doe - 719468L;                      // days since 1970-01-01
}

static bool parseRfc3339(const char *s, time_t *outEpoch) {
  int year, mon, day, hh, mm, ss;
  if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &mon, &day, &hh, &mm, &ss) != 6)
    return false;

  const char *p = s + 19;    // just past "YYYY-MM-DDTHH:MM:SS"
  if (*p == '.') { p++; while (isdigit((unsigned char)*p)) p++; }

  long offsetSec = 0;
  if (*p == '+' || *p == '-') {
    int sign = (*p == '-') ? -1 : 1;
    int offH = 0, offM = 0;
    if (sscanf(p + 1, "%2d:%2d", &offH, &offM) != 2) return false;
    offsetSec = sign * (offH * 3600L + offM * 60L);
  }
  // else 'Z' (or anything else) -- treat as UTC, offset 0.

  long days = daysFromCivil(year, (unsigned)mon, (unsigned)day);
  time_t utc = (time_t)days * 86400L + hh * 3600L + mm * 60L + ss;
  *outEpoch = utc - offsetSec;
  return true;
}

static void formatRfc3339Utc(time_t t, char *out, size_t outLen) {
  struct tm tmVal;
  gmtime_r(&t, &tmVal);
  strftime(out, outLen, "%Y-%m-%dT%H:%M:%SZ", &tmVal);
}

// Percent-encodes everything outside the RFC 3986 "unreserved" set. Used for
// POST body fields (client secret / refresh token) and the ':' in our own
// fixed-format timestamps -- simple and correct is worth more here than
// fast, this runs at most a few times a minute.
static String urlEncode(const char *s) {
  String out;
  char buf[4];
  for (const char *p = s; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      snprintf(buf, sizeof(buf), "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

// =========================================================================
//  Access token cache
// =========================================================================
static String s_accessToken;
static time_t s_accessTokenExpiry = 0;   // epoch; 0 == never fetched

bool gcalEnsureAccessToken() {
  time_t now = time(nullptr);
  if (s_accessToken.length() && now < s_accessTokenExpiry - GCAL_TOKEN_REFRESH_MARGIN_S)
    return true;

  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setCACert(GTS_ROOT_R1_PEM);
  HTTPClient https;
  if (!https.begin(client, "https://oauth2.googleapis.com/token")) return false;
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "client_id=" + urlEncode(GOOGLE_CLIENT_ID) +
                "&client_secret=" + urlEncode(GOOGLE_CLIENT_SECRET) +
                "&refresh_token=" + urlEncode(GOOGLE_REFRESH_TOKEN) +
                "&grant_type=refresh_token";

  int code = https.POST(body);
  if (code != HTTP_CODE_OK) {
    Serial.printf("[GCal] token refresh failed, HTTP %d\n", code);
    https.end();
    return false;
  }
  String resp = https.getString();
  https.end();

  JsonDocument doc;
  if (deserializeJson(doc, resp)) return false;

  const char *at = doc["access_token"] | (const char *)nullptr;
  long expiresIn = doc["expires_in"] | 0;
  if (!at || !*at) return false;

  s_accessToken = at;
  s_accessTokenExpiry = now + (time_t)expiresIn;
  return true;
}

bool gcalFetchAgenda(CalEvent *current, bool *hasCurrent,
                     CalEvent *next, bool *hasNext) {
  *hasCurrent = false;
  *hasNext = false;

  if (!gcalEnsureAccessToken()) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  time_t now = time(nullptr);
  time_t until = now + (time_t)GCAL_LOOKAHEAD_SECONDS;
  char timeMin[24], timeMax[24];
  formatRfc3339Utc(now, timeMin, sizeof(timeMin));
  formatRfc3339Utc(until, timeMax, sizeof(timeMax));

  String url = "https://www.googleapis.com/calendar/v3/calendars/" GCAL_CALENDAR_ID
               "/events?singleEvents=true&orderBy=startTime&maxResults=" +
               String(GCAL_MAX_EVENTS) +
               "&timeMin=" + urlEncode(timeMin) +
               "&timeMax=" + urlEncode(timeMax);

  WiFiClientSecure client;
  client.setCACert(GTS_ROOT_R1_PEM);
  HTTPClient https;
  if (!https.begin(client, url)) return false;
  https.addHeader("Authorization", "Bearer " + s_accessToken);

  int code = https.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[GCal] events fetch failed, HTTP %d\n", code);
    https.end();
    return false;
  }
  String payload = https.getString();
  https.end();

  // Keep only the fields this app actually looks at -- bounds memory use
  // regardless of how much else Google's response includes.
  JsonDocument filter;
  filter["items"][0]["summary"] = true;
  filter["items"][0]["status"] = true;
  filter["items"][0]["start"]["dateTime"] = true;
  filter["items"][0]["end"]["dateTime"] = true;

  JsonDocument doc;
  if (deserializeJson(doc, payload, DeserializationOption::Filter(filter))) return false;

  for (JsonObject item : doc["items"].as<JsonArray>()) {
    const char *status = item["status"] | "confirmed";
    if (strcmp(status, "cancelled") == 0) continue;

    // All-day events carry start/end as a "date", not "dateTime" -- the
    // filter above only keeps "dateTime", so such events simply have
    // neither field present here and are skipped.
    const char *startStr = item["start"]["dateTime"] | (const char *)nullptr;
    const char *endStr   = item["end"]["dateTime"]   | (const char *)nullptr;
    if (!startStr || !endStr) continue;

    time_t start, end;
    if (!parseRfc3339(startStr, &start)) continue;
    if (!parseRfc3339(endStr, &end)) continue;

    const char *summary = item["summary"] | "(no title)";

    if (!*hasCurrent && start <= now && now < end) {
      current->title = summary;
      current->start = start;
      current->end = end;
      *hasCurrent = true;
    } else if (!*hasNext && start > now) {
      next->title = summary;
      next->start = start;
      next->end = end;
      *hasNext = true;
      break;   // results are start-time ordered -- nothing past this matters
    }
  }
  return true;
}
