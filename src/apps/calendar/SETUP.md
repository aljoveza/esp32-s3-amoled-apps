# Calendar — one-time Google Cloud / OAuth setup

The device authenticates to Google Calendar directly (no backend server),
using a **refresh token** that's minted once, on your computer, and then
baked into the firmware's `secrets.h`. This is a PC-side setup you do once
before the app can show real data — none of it runs on the ESP32.

## 1. Create a Google Cloud project and enable the Calendar API

1. Go to the [Google Cloud Console](https://console.cloud.google.com/) and
   create a new project (or reuse an existing personal one).
2. **APIs & Services → Library** → search **Google Calendar API** → Enable.

## 2. Configure the OAuth consent screen

1. **APIs & Services → OAuth consent screen.**
2. User type: **External** (personal Gmail accounts can't use Internal).
3. Fill in the required app name/support email fields — anything
   reasonable, this is only ever shown to you.
4. Add scope `https://www.googleapis.com/auth/calendar.readonly`.
5. **Publishing status: set it to "In production", not "Testing".** This
   matters concretely: a refresh token minted while the app is in Testing
   status **expires after 7 days**, silently breaking the device a week
   after setup. In production status, a refresh token instead lasts until
   you revoke it or it goes unused for 6 months. Moving to production
   without Google's app verification is fine here — verification is only
   required to *remove* the "unverified app" warning shown during
   authorization, which is expected and fine to click through for your own
   personal single-user app requesting your own data.

## 3. Create an OAuth Client ID

1. **APIs & Services → Credentials → Create Credentials → OAuth client ID.**
2. Application type: **Desktop app.**
3. Note the **Client ID** and **Client Secret** it gives you.

## 4. Mint a refresh token

Run the helper script once, from this directory, with Python 3 (standard
library only — nothing to install):

```bash
python3 tools/get_refresh_token.py --client-id YOUR_CLIENT_ID --client-secret YOUR_CLIENT_SECRET
```

It opens your browser to Google's consent page, catches the redirect on a
local `http://localhost` port, exchanges the authorization code for tokens,
and prints the **refresh token** to your terminal. Approve access for your
own account when prompted (you'll see the "Google hasn't verified this app"
warning from step 2 — click **Advanced → Go to (your app name)** to
continue).

## 5. Fill in `secrets.h`

```bash
cp secrets.h.example secrets.h
```

Edit `secrets.h` and fill in `WIFI_SSID`, `WIFI_PASSWORD`, the
`GOOGLE_CLIENT_ID` / `GOOGLE_CLIENT_SECRET` from step 3, and
`GOOGLE_REFRESH_TOKEN` from step 4. This file is gitignored — it never gets
committed.

Then follow the "Setup" steps in [`README.md`](README.md) to flash it.

## If it stops working later

- **"Calendar auth error" on-device, worked before:** the refresh token was
  revoked or expired. If the consent screen is still in "Testing" status
  (see step 2), this happens on a 7-day cycle — switch it to "In
  production" and re-run step 4.
- **Wrong events, or none at all:** the app reads the **primary** calendar
  of whichever account you authorized in step 4 — make sure that was the
  account whose calendar you meant to show.
