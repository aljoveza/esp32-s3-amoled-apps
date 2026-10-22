#pragma once
#include <Arduino.h>

// =========================================================================
//  Google Calendar — OAuth token refresh + "current/next event" fetch.
//
//  Talks to accounts.google.com and www.googleapis.com directly over TLS
//  using a long-lived refresh token (see secrets.h / SETUP.md) -- there is
//  no backend server involved. Every failure here is a normal, expected
//  runtime state (no Wi-Fi yet, Google unreachable, token expired) and is
//  reported back via a return value, never by blocking or crashing --
//  same philosophy as audio.h/battery.h in src/common.
// =========================================================================

struct CalEvent {
  String title;
  time_t start;   // UTC epoch seconds
  time_t end;     // UTC epoch seconds
};

// Makes sure a non-expired access token is cached, refreshing it via the
// stored refresh token if there isn't one yet or it's about to expire.
// Requires Wi-Fi to already be connected. Safe to call every time before a
// fetch; it only actually hits the network when a refresh is due.
bool gcalEnsureAccessToken();

// Fetches the agenda for [now, now + GCAL_LOOKAHEAD_SECONDS) and picks out
// the current event (start <= now < end) and the next one after it, if any.
// All-day and cancelled events are skipped entirely. Calls
// gcalEnsureAccessToken() itself. Returns false only on a network/parse
// failure -- *hasCurrent/*hasNext false is a normal "nothing right now"
// result, not a failure.
bool gcalFetchAgenda(CalEvent *current, bool *hasCurrent,
                     CalEvent *next, bool *hasNext);
