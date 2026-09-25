# Leaderboard server

The game's original leaderboard is unreachable and always will be. This is a
replacement that costs nothing to run.

## Why the original is gone

The game did not use HTTP. It connected to int13's **"Hub Server"** at a
hardcoded IP, asked it which address was serving leaderboards that day, and
then spoke **NWT** — their own binary packet protocol, obfuscated on the wire.
The strings are still in the binary:

```
LEADERBOARD: Connected to the Hub Server.
LEADERBOARD: Service available @%ip:%d.
NWT: Incorrect host name %s, expected 'xxx.xxx.xxx.xxx'
```

Those were int13's addresses a decade ago; they belong to someone else now.
Reviving it would mean reversing an undocumented obfuscated protocol *and*
hosting raw TCP, which almost no free tier offers.

## What replaces it

None of that is necessary, because the engine exposes a public hook:

```c
LEADERBOARD_SetScoreReceivedCallback(handle, callback, user)
```

The callback it registers — `onReceiveScore` — is what actually fills in the
ranking screen. So the port does the networking itself over ordinary HTTPS and
calls that function with the answer. **The guest never opens a socket.**

`onReceiveScore`'s thirteen arguments were recovered from the log lines it
prints for each one (`"World best: %d (%s)"`, `"Country rank: %d (%s)"` …),
which agree exactly with its mangled parameter types:

```c
onReceiveScore(board, yourBest,
               worldRank,   worldBest,   worldBestName,
               countryRank, countryBest, countryBestName,
               cityRank,    cityBest,    cityBestName,
               countryName, cityName, SHOGUN*)
```

The JSON below is that signature, field for field, so the client does no
mapping.

## Why Cloudflare Workers

The game wants **World / Country / City** rankings, and geo-IP is the expensive
part of that. Workers give you `request.cf.country` and `request.cf.city` on
every request, free — and because it comes from the edge rather than the
client, it cannot be spoofed by whoever is holding the phone.

Free tier: 100,000 requests/day, 100,000 KV reads, 1,000 KV writes. A player
generates a handful of writes per session.

Alternatives, if you would rather: **Supabase** (real SQL ranking, no geo),
**Deno Deploy** / **Val Town** (simplest, no geo or KV), **Firebase RTDB**.
Any of them work — the client only needs one JSON endpoint.

## Deploy

```bash
npm create cloudflare@latest -- shogun-leaderboard
cd shogun-leaderboard
cp /path/to/server/worker.js src/index.js
cp /path/to/server/wrangler.toml .

npx wrangler kv namespace create SCORES     # paste the id into wrangler.toml
npx wrangler deploy
```

Then point the game at it, without rebuilding:

```bash
adb shell "echo https://shogun-leaderboard.YOURNAME.workers.dev/v1/score > \
  /sdcard/Android/data/dev.admiralgallade.shogun64/files/leaderboard.txt"
```

Or set `DEFAULT_ENDPOINT` in `Leaderboard.java` and rebuild. **Empty means the
leaderboard stays offline**, which is the default.

## API

`POST /v1/score`

```json
{ "board": "total", "name": "AdmiralGallade", "score": 7279 }
```

`board` is one of `total`, `level1`, `level2`, `level3`, `level4` — the five
the engine asks for. A `score` of `0` reads the standings without recording
anything, which is what opening the leaderboard screen does.

```json
{
  "board": "total",
  "yourBest": 7279,
  "worldRank": 2,   "worldBest": 99999, "worldBestName": "Rival",
  "countryRank": 1, "countryBest": 7279, "countryBestName": "AdmiralGallade",
  "cityRank": 1,    "cityBest": 7279,   "cityBestName": "AdmiralGallade",
  "countryName": "IN", "cityName": "Bengaluru"
}
```

`GET /v1/health` returns `{"ok":true,...}`.

## Tests

`test.mjs` runs the Worker against an in-memory stand-in for KV — no account,
no network:

```bash
node test.mjs
```

It covers ranking across scopes, that improving your own score does not
duplicate you, that boards are independent, that geo comes from the edge, and
that bad input is rejected.

## Notes

- Names are clamped to 31 printable characters, because that is the limit the
  engine copies them with.
- There is no authentication. Anyone who finds the URL can post a score. For a
  private leaderboard that is usually fine; if it matters, add a shared secret
  header and check it in the Worker.
