import worker from './worker.js';

// Minimal in-memory stand-in for a KV namespace.
const store = new Map();
const env = { SCORES: {
  async get(k, t) { const v = store.get(k); return v == null ? null : (t === 'json' ? JSON.parse(v) : v); },
  async put(k, v) { store.set(k, v); },
}};

const post = (body, cf) => new Request('https://x/v1/score', {
  method: 'POST', body: JSON.stringify(body),
  headers: { 'content-type': 'application/json' },
});
// Request.cf is not settable in plain node, so wrap.
const call = async (body, cf) => {
  const r = post(body); Object.defineProperty(r, 'cf', { value: cf });
  const res = await worker.fetch(r, env);
  return { status: res.status, body: await res.json() };
};

const IN = { country: 'IN', city: 'Bengaluru' };
const US = { country: 'US', city: 'Austin' };

let fails = 0;
const chk = (name, cond, got) => {
  if (cond) console.log('  PASS', name);
  else { console.log('  FAIL', name, '->', JSON.stringify(got)); fails++; }
};

console.log('=== health ===');
let h = await worker.fetch(new Request('https://x/v1/health'), env);
chk('health ok', h.status === 200, await h.clone().json());

console.log('=== first submission ===');
let a = await call({ board: 'total', name: 'AdmiralGallade', score: 7279 }, IN);
chk('rank 1 everywhere', a.body.worldRank === 1 && a.body.countryRank === 1 && a.body.cityRank === 1, a.body);
chk('geo from edge', a.body.countryName === 'IN' && a.body.cityName === 'Bengaluru', a.body);
chk('yourBest echoes', a.body.yourBest === 7279, a.body);

console.log('=== a higher score from another country ===');
let b = await call({ board: 'total', name: 'Rival', score: 99999 }, US);
chk('rival tops world', b.body.worldRank === 1 && b.body.worldBest === 99999, b.body);
chk('rival own country rank 1', b.body.countryRank === 1 && b.body.countryName === 'US', b.body);

console.log('=== original player re-reads (score 0 = read-only) ===');
let c = await call({ board: 'total', name: 'AdmiralGallade', score: 0 }, IN);
chk('now world rank 2', c.body.worldRank === 2, c.body);
chk('still country rank 1', c.body.countryRank === 1, c.body);
chk('world best is rival', c.body.worldBestName === 'Rival' && c.body.worldBest === 99999, c.body);
chk('yourBest preserved', c.body.yourBest === 7279, c.body);

console.log('=== improving your own score does not duplicate you ===');
await call({ board: 'total', name: 'AdmiralGallade', score: 50000 }, IN);
let d = await call({ board: 'total', name: 'AdmiralGallade', score: 0 }, IN);
chk('single entry, improved', d.body.yourBest === 50000 && d.body.worldRank === 2, d.body);

console.log('=== boards are independent ===');
let e = await call({ board: 'level1', name: 'AdmiralGallade', score: 10 }, IN);
chk('level1 separate', e.body.worldRank === 1 && e.body.worldBest === 10, e.body);

console.log('=== validation ===');
let f = await call({ board: 'nope', name: 'x', score: 1 }, IN);
chk('unknown board rejected', f.status === 400, f);
let g = await call({ board: 'total', name: '\u0000\u0001bad\u007f', score: 5 }, IN);
chk('name sanitised', /^[\x20-\x7e]+$/.test(g.body.worldBestName), g.body);
let i2 = await call({ board: 'total', name: 'NoGeo', score: 5 }, {});
chk('missing geo falls back', i2.body.countryName === '--' && i2.body.cityName === '--', i2.body);

console.log(fails ? `\n${fails} FAILURE(S)` : '\nall checks passed');
process.exit(fails ? 1 : 0);
