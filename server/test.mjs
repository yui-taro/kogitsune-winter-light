import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import http from 'node:http';
import {resolve} from 'node:path';
import {after, test} from 'node:test';
import {mockChoice, normalize} from './relay.mjs';

function fixture() {
  return {
    attemptId: 2, decisionId: 7, revision: 3,
    legalIntents: ['guard_lantern', 'inspect_bell', 'wait_and_watch'],
    guardian: {scene: 'shrine', form: 'stone', currentIntent: 'guard_lantern',
               position: {x: 1005, y: 470}},
    knowledge: {
      bellHeard: true, tracksSeen: false, tracksPending: false, foxVisible: false, lightVisible: false,
      theftKnown: false, falseAlarm: false, hasLastSeen: false,
      bellCount: 1, bellAge: 0.5, watchCompleted: false,
      lastSeenInForest: false, tracksSeenInForest: false,
      bellPosition: {x: 350, y: 500},
    },
  };
}

test('J-07: normalize forwards observed facts only', () => {
  const request = fixture();
  request.hiddenFoxPosition = {x: 800, y: 500};
  request.knowledge.secretNestLocation = {x: 10, y: 20};
  request.knowledge.lastSeen = {x: 800, y: 500};
  request.knowledge.lastTracks = {x: 800, y: 500};
  const normalized = normalize(request);
  assert.deepEqual(normalized, {
    attemptId: 2, decisionId: 7, revision: 3,
    legalIntents: request.legalIntents,
    observations: {
      bellHeard: true, tracksSeen: false, tracksPending: false, foxVisible: false, lightVisible: false,
      theftKnown: false, previousBellInvestigationEmpty: false,
      bellCount: 1, bellAge: 0.5, watchCompleted: false,
      lastSeenArea: 'shrine', lastTracksArea: 'shrine',
      bellPosition: {x: 350, y: 500},
    },
    guardian: request.guardian,
  });
  assert.equal(mockChoice(normalized), 'inspect_bell');
});

test('J-01..05: local substitute selects only from observed clues', () => {
  const cases = [
    [{}, ['guard_lantern', 'wait_and_watch'], 'guard_lantern'],
    [{bellHeard: true, bellPosition: {x: 350, y: 500}}, ['guard_lantern', 'inspect_bell'], 'inspect_bell'],
    [{bellHeard: false, tracksSeen: true, tracksPending: true, lastTracks: {x: 550, y: 550}},
     ['guard_lantern', 'inspect_tracks'], 'inspect_tracks'],
    [{bellHeard: true, bellPosition: {x: 350, y: 500}, tracksSeen: true, tracksPending: true,
      lastTracks: {x: 550, y: 550}}, ['guard_lantern', 'inspect_bell', 'inspect_tracks'], 'inspect_tracks'],
    [{bellHeard: false, tracksSeen: true, tracksPending: false, lastTracks: {x: 550, y: 550}},
     ['guard_lantern', 'wait_and_watch'], 'guard_lantern'],
    [{bellHeard: true, bellPosition: {x: 350, y: 500}, falseAlarm: true},
     ['guard_lantern'], 'guard_lantern'],
    [{theftKnown: true}, ['guard_lantern', 'watch_torii_exit', 'watch_log_exit'], 'watch_log_exit'],
    [{foxVisible: true}, ['guard_lantern', 'follow_visible_clue'], 'follow_visible_clue'],
    [{lightVisible: true}, ['guard_lantern', 'follow_visible_clue'], 'follow_visible_clue'],
  ];
  for (const [knowledge, legalIntents, expected] of cases) {
    const normalized = normalize({...fixture(), legalIntents,
      knowledge: {...fixture().knowledge, ...knowledge,
        bellCount: knowledge.bellHeard === false ? 0 : 1,
        ...(knowledge.tracksSeen ? {tracksAge: 0.2} : {}),
        ...(knowledge.hasLastSeen ? {lastSeenAge: 0.2} : {})}});
    assert.equal(mockChoice(normalized), expected);
  }
});

test('exit choice follows the guardian position', () => {
  const request = fixture();
  request.guardian.position = {x: 200, y: 735};
  request.knowledge.theftKnown = true;
  request.legalIntents = ['watch_torii_exit', 'watch_log_exit'];
  assert.equal(mockChoice(normalize(request)), 'watch_torii_exit');
});

const child = spawn(process.execPath, ['relay.mjs'], {
  cwd: import.meta.dirname,
  env: {...process.env, JEV_MOCK: '1', JEV_RELAY_PORT: '0', TYPESAFE_API_KEY: ''},
  stdio: ['ignore', 'pipe', 'pipe'],
});
after(() => child.kill());

let childOutput = '';
let baseUrl;
const listening = new Promise((resolveReady, rejectReady) => {
  const timeout = setTimeout(() => rejectReady(new Error(`relay did not start: ${childOutput}`)), 5000);
  child.stdout.on('data', chunk => {
    childOutput += chunk;
    const match = childOutput.match(/http:\/\/127\.0\.0\.1:(\d+)/);
    if (match) {
      clearTimeout(timeout);
      baseUrl = `http://127.0.0.1:${match[1]}`;
      resolveReady();
    }
  });
  child.stderr.on('data', chunk => { childOutput += chunk; });
  child.on('error', error => { clearTimeout(timeout); rejectReady(error); });
  child.on('exit', code => {
    if (!baseUrl) { clearTimeout(timeout); rejectReady(new Error(`relay exit ${code}: ${childOutput}`)); }
  });
});

async function post(body) {
  await listening;
  return fetch(`${baseUrl}/decision`, {
    method: 'POST', headers: {'Content-Type': 'application/json'},
    body: typeof body === 'string' ? body : JSON.stringify(body),
  });
}

test('V4: relay preserves request identity and returns only one legal choice', async () => {
  await listening;
  const health = await fetch(`${baseUrl}/health`);
  assert.equal(health.status, 200);
  assert.deepEqual(await health.json(), {ready: true, mode: 'mock', login: false});
  const response = await post(fixture());
  assert.equal(response.status, 200);
  assert.deepEqual(await response.json(),
    {attemptId: 2, decisionId: 7, revision: 3, choice: 'inspect_bell', mode: 'mock'});
  assert.equal(response.headers.get('cache-control'), 'no-store');
});

test('V4: relay rejects malformed identity, evidence, candidates, and oversized bodies', async () => {
  const bad = [
    {...fixture(), attemptId: 0},
    {...fixture(), decisionId: 1.5},
    {...fixture(), revision: -1},
    {...fixture(), legalIntents: []},
    {...fixture(), legalIntents: ['inspect_bell', 'inspect_bell']},
    {...fixture(), legalIntents: ['teleport_to_nest']},
    {...fixture(), legalIntents: ['follow_visible_clue']},
    {...fixture(), legalIntents: ['inspect_tracks']},
    {...fixture(), legalIntents: ['watch_exit']},
    {...fixture(), legalIntents: ['watch_log_exit']},
    {...fixture(), guardian: {...fixture().guardian, currentIntent: 'teleport_to_nest'}},
    {...fixture(), guardian: {...fixture().guardian, position: {x: -1, y: 470}}},
    {...fixture(), knowledge: {...fixture().knowledge, bellAge: -1}},
    {...fixture(), knowledge: {...fixture().knowledge, bellCount: 0}},
    {...fixture(), knowledge: {...fixture().knowledge, bellHeard: 'true'}},
    {...fixture(), knowledge: {...fixture().knowledge, bellPosition: {x: -1, y: 500}}},
    {...fixture(), knowledge: {...fixture().knowledge, bellPosition: {x: 350}}},
    {...fixture(), knowledge: {...fixture().knowledge, hasLastSeen: true}},
    {...fixture(), knowledge: {...fixture().knowledge, tracksSeen: true}},
    {...fixture(), knowledge: {...fixture().knowledge, tracksPending: true}},
    {...fixture(), knowledge: {...fixture().knowledge, tracksSeen: true, tracksPending: false,
                                lastTracks: {x: 550, y: 550}}, legalIntents: ['inspect_tracks']},
    '{broken JSON',
    JSON.stringify(fixture()) + ' '.repeat(17000),
  ];
  for (const request of bad) {
    const response = await post(request);
    assert.equal(response.status, 400, JSON.stringify(request).slice(0, 120));
    assert.deepEqual(await response.json(), {error: 'invalid request'});
  }
  const missingRoute = await fetch(`${baseUrl}/missing`);
  assert.equal(missingRoute.status, 404);
});

async function runBridge(mode, url) {
  await listening;
  const binary = resolve(import.meta.dirname, '../game/build/test_bridge');
  const output = await new Promise((resolveResult, reject) => {
    const bridgeProcess = spawn(binary, [mode], {
      env: {...process.env, JEV_RELAY_URL: url},
    });
    let stdout = '';
    let stderr = '';
    bridgeProcess.stdout.on('data', chunk => { stdout += chunk; });
    bridgeProcess.stderr.on('data', chunk => { stderr += chunk; });
    bridgeProcess.on('error', reject);
    bridgeProcess.on('close', (code, signal) => code === 0 ? resolveResult(stdout) :
      reject(new Error(`bridge exit ${code ?? signal}: ${stderr}`)));
  });
  assert.match(output, /bridge integration: pass/);
}

test('V4: C++ bridge applies the mock choice', async () => {
  await runBridge('success', `${baseUrl}/decision`);
});

test('J-08: C++ bridge uses local policy after relay failure', async () => {
  await runBridge('fallback', `${baseUrl}/missing`);
});

test('J-08: C++ bridge discards an obsolete response', async () => {
  await runBridge('stale', `${baseUrl}/decision`);
});

// A local stand-in for the Jev API, so relay error handling is tested without a real key.
const upstreamCalls = [];
let upstreamReply = () => [200, {answers: {guardian_intent: {choice: 'inspect_bell'}}}];
const upstream = http.createServer(async (req, res) => {
  let text = '';
  for await (const chunk of req) text += chunk;
  upstreamCalls.push({authorization: req.headers.authorization, body: JSON.parse(text)});
  const [status, payload] = upstreamReply();
  res.writeHead(status, {'Content-Type': 'application/json'});
  res.end(typeof payload === 'string' ? payload : JSON.stringify(payload));
});
const upstreamReady = new Promise(ready => upstream.listen(0, '127.0.0.1', ready));
after(() => upstream.close());

let jevRelay;
let jevRelayOutput = '';
const jevRelayUrl = upstreamReady.then(() => new Promise((resolveReady, rejectReady) => {
  jevRelay = spawn(process.execPath, ['relay.mjs'], {
    cwd: import.meta.dirname,
    env: {...process.env, JEV_MOCK: '', JEV_RELAY_PORT: '0', TYPESAFE_API_KEY: 'test-only-key',
          TYPESAFE_API_URL: `http://127.0.0.1:${upstream.address().port}/v1/systemone`},
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  const timeout = setTimeout(() => rejectReady(new Error(`jev relay did not start: ${jevRelayOutput}`)), 5000);
  jevRelay.stdout.on('data', chunk => {
    jevRelayOutput += chunk;
    const match = jevRelayOutput.match(/http:\/\/127\.0\.0\.1:(\d+)/);
    if (match) { clearTimeout(timeout); resolveReady(`http://127.0.0.1:${match[1]}`); }
  });
  jevRelay.stderr.on('data', chunk => { jevRelayOutput += chunk; });
  jevRelay.on('exit', code => { clearTimeout(timeout); rejectReady(new Error(`jev relay exit ${code}: ${jevRelayOutput}`)); });
}));
after(() => jevRelay?.kill());

async function postJev(body) {
  return fetch(`${await jevRelayUrl}/decision`, {
    method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(body),
  });
}

test('J-07/V4: Jev request carries only normalized observations and legal criteria', async () => {
  upstreamCalls.length = 0;
  upstreamReply = () => [200, {answers: {guardian_intent: {choice: 'inspect_bell'}}}];
  const request = fixture();
  request.hiddenFoxPosition = {x: 800, y: 500};
  request.knowledge.secretNestLocation = {x: 10, y: 20};
  const response = await postJev(request);
  assert.equal(response.status, 200);
  assert.deepEqual(await response.json(), {attemptId: 2, decisionId: 7, revision: 3, choice: 'inspect_bell', mode: 'jev'});
  assert.equal(upstreamCalls.length, 1);
  const sent = upstreamCalls[0];
  assert.equal(sent.authorization, 'Bearer test-only-key');
  assert.deepEqual(sent.body.state.observed_facts, normalize(request).observations);
  assert.deepEqual(sent.body.state.guardian_now, normalize(request).guardian);
  assert.deepEqual(Object.keys(sent.body.questions.guardian_intent.criteria), request.legalIntents);
  const text = JSON.stringify(sent.body);
  assert.doesNotMatch(text, /hiddenFoxPosition|secretNestLocation|attemptId|decisionId|test-only-key/);
});

test('Jev request pins a versioned model and the relay logs reported usage', async () => {
  upstreamCalls.length = 0;
  upstreamReply = () => [200, {model: 'jev-1.13.0', usage: {input_tokens: 321},
                               answers: {guardian_intent: {choice: 'inspect_bell'}}}];
  const response = await postJev(fixture());
  assert.equal(response.status, 200);
  assert.equal(upstreamCalls[0].body.model, 'jev-1.13.0');
  assert.match(jevRelayOutput, /\{"event":"jev_usage","model":"jev-1\.13\.0","inputTokens":321,"today":\d+\}/);
  assert.doesNotMatch(jevRelayOutput, /test-only-key|bellPosition/);
});

test('V4/J-08: Jev 429, 529, malformed, and illegal replies become one unavailable response', async () => {
  const failures = [
    [429, {error: 'rate limited'}],
    [529, {error: 'overloaded'}],
    [401, {error: 'unauthorized'}],
    [422, {error: 'unprocessable'}],
    [200, '{broken JSON'],
    [200, {answers: {}}],
    [200, {answers: {guardian_intent: {choice: 'watch_exit'}}}],
    [200, {answers: {guardian_intent: {choice: 'teleport_to_nest'}}}],
  ];
  for (const failure of failures) {
    upstreamCalls.length = 0;
    upstreamReply = () => failure;
    const response = await postJev(fixture());
    assert.equal(response.status, 503, JSON.stringify(failure));
    assert.deepEqual(await response.json(), {error: 'decision unavailable'});
    // The relay must not hammer the API with rapid retries for the same request.
    assert.equal(upstreamCalls.length, 1, JSON.stringify(failure));
  }
});


// ---- public relay: login, CORS, and limits ----
function startRelay(env) {
  const proc = spawn(process.execPath, ['relay.mjs'], {
    cwd: import.meta.dirname,
    env: {...process.env, JEV_MOCK: '1', JEV_RELAY_PORT: '0', TYPESAFE_API_KEY: '', ...env},
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  after(() => proc.kill());
  let output = '';
  const url = new Promise((resolveReady, rejectReady) => {
    const timeout = setTimeout(() => rejectReady(new Error(`relay did not start: ${output}`)), 5000);
    proc.stdout.on('data', chunk => {
      output += chunk;
      const match = output.match(/http:\/\/127\.0\.0\.1:(\d+)/);
      if (match) { clearTimeout(timeout); resolveReady(`http://127.0.0.1:${match[1]}`); }
    });
    proc.stderr.on('data', chunk => { output += chunk; });
    proc.on('exit', code => { clearTimeout(timeout); rejectReady(new Error(`relay exit ${code}: ${output}`)); });
  });
  return {url, output: () => output, proc};
}

const origin = 'https://yui-taro.github.io';
const secure = startRelay({RELAY_PASSWORDS: 'snow-lantern-01, second-pass-02',
  RELAY_SESSION_SECRET: 'x'.repeat(40), RELAY_ALLOWED_ORIGINS: `${origin}/`,
  RELAY_PER_MINUTE_LIMIT: '2', RELAY_DAILY_LIMIT: '4'});

async function login(password, headers = {}) {
  return fetch(`${await secure.url}/login`, {method: 'POST',
    headers: {'Content-Type': 'application/json', ...headers}, body: JSON.stringify({password})});
}
async function decide(token) {
  return fetch(`${await secure.url}/decision`, {method: 'POST',
    headers: {'Content-Type': 'application/json', ...(token ? {Authorization: `Bearer ${token}`} : {})},
    body: JSON.stringify(fixture())});
}

test('public relay: decisions need a login and the password decides it', async () => {
  const health = await fetch(`${await secure.url}/health`);
  assert.deepEqual(await health.json(), {ready: true, mode: 'mock', login: true});
  assert.equal((await decide()).status, 401);
  assert.equal((await decide('forged.token')).status, 401);
  assert.equal((await login('wrong-password')).status, 401);
  const ok = await login('second-pass-02');
  assert.equal(ok.status, 200);
  const {token} = await ok.json();
  const tampered = token.replace(/^./, c => (c === 'a' ? 'b' : 'a'));
  assert.equal((await decide(tampered)).status, 401);
  const decision = await decide(token);
  assert.equal(decision.status, 200);
  assert.equal((await decision.json()).choice, 'inspect_bell');
  assert.doesNotMatch(secure.output(), /second-pass-02|snow-lantern-01/);
});

test('public relay: only the allowed origin gets CORS headers', async () => {
  const preflight = await fetch(`${await secure.url}/decision`, {method: 'OPTIONS',
    headers: {Origin: origin, 'Access-Control-Request-Method': 'POST'}});
  assert.equal(preflight.status, 204);
  assert.equal(preflight.headers.get('access-control-allow-origin'), origin);
  assert.match(preflight.headers.get('access-control-allow-headers'), /Authorization/);
  const other = await fetch(`${await secure.url}/decision`, {method: 'OPTIONS',
    headers: {Origin: 'https://evil.example', 'Access-Control-Request-Method': 'POST'}});
  assert.equal(other.status, 403);
  assert.equal(other.headers.get('access-control-allow-origin'), null);
  const loginReply = await login('wrong-password', {Origin: origin});
  assert.equal(loginReply.headers.get('access-control-allow-origin'), origin);
});

test('public relay: per-minute and daily limits refuse extra decisions', async () => {
  const {token: first} = await (await login('snow-lantern-01')).json();
  const {token: second} = await (await login('snow-lantern-01')).json();
  // One decision was already used by the earlier test; the daily limit is 4.
  assert.equal((await decide(first)).status, 200);
  assert.equal((await decide(first)).status, 200);
  const perMinute = await decide(first);
  assert.equal(perMinute.status, 429);
  assert.equal((await perMinute.json()).error, 'too many requests');
  assert.equal((await decide(second)).status, 200);
  const daily = await decide(second);
  assert.equal(daily.status, 429);
  assert.equal((await daily.json()).error, 'daily limit reached');
});

test('public relay: repeated wrong passwords are blocked', async () => {
  const relay = startRelay({RELAY_PASSWORDS: 'another-pass-1', RELAY_SESSION_SECRET: 'y'.repeat(40)});
  const url = await relay.url;
  const attempt = password => fetch(`${url}/login`, {method: 'POST',
    headers: {'Content-Type': 'application/json'}, body: JSON.stringify({password})});
  for (let i = 0; i < 10; i++) assert.equal((await attempt('nope-nope')).status, 401);
  assert.equal((await attempt('another-pass-1')).status, 429);
});

test('public relay: weak settings stop the relay from starting', async () => {
  for (const env of [{RELAY_PASSWORDS: 'short', RELAY_SESSION_SECRET: 'z'.repeat(40)},
                     {RELAY_PASSWORDS: 'long-enough-pass', RELAY_SESSION_SECRET: 'short'}]) {
    const relay = startRelay(env);
    await assert.rejects(relay.url, /relay exit 1: relay not started/);
  }
});
