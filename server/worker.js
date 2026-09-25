/**
 * Shogun 64 leaderboard — a Cloudflare Worker.
 *
 * The original game talked to int13's "Hub Server" over NWT, their own
 * obfuscated binary packet protocol: connect to a hardcoded IP, ask which
 * IP:port is serving leaderboards today, then speak NWT to that. Both are long
 * gone, the addresses belong to someone else now, and almost no free tier
 * offers raw TCP anyway.
 *
 * None of that needs reviving. The engine exposes
 *
 *     LEADERBOARD_SetScoreReceivedCallback(handle, callback, user)
 *
 * and the callback it registers, onReceiveScore, is what actually populates
 * the ranking table the UI reads. So the port does the networking itself over
 * ordinary HTTPS and hands the result to that callback. The guest never opens
 * a socket. This file is the other end.
 *
 * Why Cloudflare specifically: the game wants World / Country / City rankings,
 * and geo-IP is the expensive part of that. Workers hand you request.cf.country
 * and request.cf.city on every request, free.
 *
 * Deploy:
 *   npm create cloudflare@latest -- shogun-leaderboard
 *   # replace src/index.js with this file, then:
 *   npx wrangler kv namespace create SCORES
 *   # put the returned id in wrangler.toml, then:
 *   npx wrangler deploy
 *
 * Free tier: 100k requests/day, 100k KV reads, 1k KV writes. A single player
 * generates a handful of writes per session.
 */

const BOARDS = new Set(['total', 'level1', 'level2', 'level3', 'level4']);
const MAX_NAME = 31;          // the engine copies names with a 31-char limit
const UNKNOWN = '--';

/** The engine's name buffers are fixed size and it renders them raw. */
function cleanName(s) {
  if (typeof s !== 'string') return UNKNOWN;
  const t = s.replace(/[^\x20-\x7e]/g, '').trim().slice(0, MAX_NAME);
  return t.length ? t : UNKNOWN;
}

/** One KV key per board per scope, holding the top entries for that scope. */
const key = (board, scope) => `b:${board}:${scope}`;

async function readBoard(env, board, scope) {
  const raw = await env.SCORES.get(key(board, scope), 'json');
  return Array.isArray(raw) ? raw : [];
}

/**
 * Merge a score into a board, keeping one entry per player and the list
 * sorted. Capped so a KV value can never grow without bound.
 */
function merge(list, name, score, cap = 100) {
  const i = list.findIndex((e) => e.n === name);
  if (i >= 0) {
    if (list[i].s >= score) return list;   // not an improvement
    list[i].s = score;
  } else {
    list.push({ n: name, s: score });
  }
  list.sort((a, b) => b.s - a.s);
  return list.slice(0, cap);
}

/** Rank is 1-based; a player not present ranks after everyone we know of. */
function rankOf(list, name) {
  const i = list.findIndex((e) => e.n === name);
  return i >= 0 ? i + 1 : list.length + 1;
}

function best(list) {
  return list.length ? { n: list[0].n, s: list[0].s } : { n: UNKNOWN, s: 0 };
}

async function submit(request, env) {
  let body;
  try {
    body = await request.json();
  } catch {
    return json({ error: 'bad json' }, 400);
  }

  const board = String(body.board || 'total');
  if (!BOARDS.has(board)) return json({ error: 'unknown board' }, 400);

  const name = cleanName(body.name);
  const score = Math.max(0, Math.min(Number(body.score) | 0, 0x7fffffff));

  // Geo comes from the edge, not the client, so it cannot be spoofed by
  // whoever is holding the phone.
  const cf = request.cf || {};
  const country = cleanName(cf.country || UNKNOWN);
  const city = cleanName(cf.city || UNKNOWN);

  const scopes = ['world', `c:${country}`, `t:${country}:${city}`];
  const lists = await Promise.all(scopes.map((s) => readBoard(env, board, s)));

  // Only record a real play. A score of 0 still reads the boards, which is
  // what the game does when it opens the leaderboard screen.
  if (score > 0) {
    for (let i = 0; i < scopes.length; i++) {
      lists[i] = merge(lists[i], name, score);
    }
    await Promise.all(
      scopes.map((s, i) =>
        env.SCORES.put(key(board, s), JSON.stringify(lists[i]))));
  }

  const [w, c, t] = lists;
  const yourBest = Math.max(
    score, ...w.filter((e) => e.n === name).map((e) => e.s), 0);

  // Field-for-field what onReceiveScore takes, so the client does no mapping.
  return json({
    board,
    yourBest,
    worldRank: rankOf(w, name),   worldBest: best(w).s,   worldBestName: best(w).n,
    countryRank: rankOf(c, name), countryBest: best(c).s, countryBestName: best(c).n,
    cityRank: rankOf(t, name),    cityBest: best(t).s,    cityBestName: best(t).n,
    countryName: country,
    cityName: city,
  });
}

function json(obj, status = 200) {
  return new Response(JSON.stringify(obj), {
    status,
    headers: { 'content-type': 'application/json; charset=utf-8' },
  });
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (url.pathname === '/v1/score' && request.method === 'POST') {
      return submit(request, env);
    }
    if (url.pathname === '/v1/health') {
      return json({ ok: true, boards: [...BOARDS] });
    }
    return json({ error: 'not found' }, 404);
  },
};
