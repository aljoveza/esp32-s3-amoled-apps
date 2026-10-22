#!/usr/bin/env python3
"""One-time helper: mints a Google OAuth refresh token for the `calendar`
app's secrets.h. Runs entirely on your computer -- nothing here ever runs
on the ESP32. Standard library only, nothing to install.

Usage:
    python3 get_refresh_token.py --client-id ID --client-secret SECRET

See ../SETUP.md for how to get a client ID/secret in the first place.
"""
import argparse
import http.server
import json
import threading
import urllib.parse
import urllib.request
import webbrowser

SCOPE = "https://www.googleapis.com/auth/calendar.readonly"
AUTH_URL = "https://accounts.google.com/o/oauth2/v2/auth"
TOKEN_URL = "https://oauth2.googleapis.com/token"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--client-id", required=True)
    ap.add_argument("--client-secret", required=True)
    ap.add_argument("--port", type=int, default=8765,
                    help="local port for the OAuth redirect (default 8765)")
    args = ap.parse_args()

    redirect_uri = f"http://localhost:{args.port}"
    result = {"code": None}
    done = threading.Event()

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            qs = urllib.parse.urlparse(self.path).query
            params = urllib.parse.parse_qs(qs)
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            if "code" in params:
                result["code"] = params["code"][0]
                self.wfile.write(b"<html><body>Done -- you can close this "
                                 b"tab and go back to the terminal.</body></html>")
            else:
                self.wfile.write(b"<html><body>No authorization code in "
                                 b"the callback -- check the terminal.</body></html>")
            done.set()

        def log_message(self, *a):
            pass   # quiet -- this is a one-shot local helper, not a server

    server = http.server.HTTPServer(("localhost", args.port), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()

    auth_params = {
        "client_id": args.client_id,
        "redirect_uri": redirect_uri,
        "response_type": "code",
        "scope": SCOPE,
        "access_type": "offline",
        "prompt": "consent",   # forces a refresh_token even on a repeat run
    }
    auth_url = f"{AUTH_URL}?{urllib.parse.urlencode(auth_params)}"
    print(f"Opening your browser to authorize:\n{auth_url}\n")
    print("If it doesn't open automatically, paste that URL into a browser yourself.")
    webbrowser.open(auth_url)

    done.wait(timeout=300)
    server.shutdown()

    if not result["code"]:
        print("No authorization code received (timed out or denied). Nothing to do.")
        return

    body = urllib.parse.urlencode({
        "code": result["code"],
        "client_id": args.client_id,
        "client_secret": args.client_secret,
        "redirect_uri": redirect_uri,
        "grant_type": "authorization_code",
    }).encode()

    req = urllib.request.Request(TOKEN_URL, data=body, method="POST",
                                 headers={"Content-Type": "application/x-www-form-urlencoded"})
    try:
        with urllib.request.urlopen(req) as resp:
            token_data = json.loads(resp.read())
    except urllib.error.HTTPError as e:
        print(f"Token exchange failed: HTTP {e.code}\n{e.read().decode()}")
        return

    refresh_token = token_data.get("refresh_token")
    if not refresh_token:
        print("No refresh_token in the response -- this can happen if you've "
              "already authorized this app before without revoking access. "
              "Revoke it at https://myaccount.google.com/permissions and "
              "re-run this script.")
        print(json.dumps(token_data, indent=2))
        return

    print("\nSuccess. Put this in secrets.h:\n")
    print(f'#define GOOGLE_REFRESH_TOKEN  "{refresh_token}"')


if __name__ == "__main__":
    main()
