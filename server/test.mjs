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
    knowledge: {
      bellHeard: true, tracksSeen: false, foxVisible: false,
      theftKnown: false, falseAlarm: false, hasLastSeen: false,
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
      bellHeard: true, tracksSeen: false, foxVisible: false,
      theftKnown: false, previousBellInvestigationEmpty: false,
      lastSeenArea: 'shrine', lastTracksArea: 'shrine',
      bellPosition: {x: 350, y: 500},
    },
  });
  assert.equal(mockChoice(normalized), 'inspect_bell');
});

test('J-01..05: local substitute selects only from observed clues', () => {
  const cases = [
    [{}, ['guard_lantern', 'wait_and_watch'], 'guard_lantern'],
    [{bellHeard: true, bellPosition: {x: 350, y: 500}}, ['guard_lantern', 'inspect_bell'], 'inspect_bell'],
    [{tracksSeen: true, lastTracks: {x: 550, y: 550}}, ['guard_lantern', 'inspect_tracks'], 'inspect_tracks'],
    [{bellHeard: true, bellPosition: {x: 350, y: 500}, tracksSeen: true,
      lastTracks: {x: 550, y: 550}}, ['guard_lantern', 'inspect_bell', 'inspect_tracks'], 'inspect_tracks'],
    [{bellHeard: true, bellPosition: {x: 350, y: 500}, falseAlarm: true},
     ['guard_lantern', 'inspect_bell'], 'guard_lantern'],
    [{theftKnown: true}, ['guard_lantern', 'watch_exit'], 'watch_exit'],
    [{foxVisible: true}, ['guard_lantern', 'follow_visible_clue'], 'follow_visible_clue'],
  ];
  for (const [knowledge, legalIntents, expected] of cases) {
    const normalized = normalize({...fixture(), legalIntents,
      knowledge: {...fixture().knowledge, ...knowledge}});
    assert.equal(mockChoice(normalized), expected);
  }
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
  assert.deepEqual(await health.json(), {ready: true, mode: 'mock'});
  const response = await post(fixture());
  assert.equal(response.status, 200);
  assert.deepEqual(await response.json(),
    {attemptId: 2, decisionId: 7, revision: 3, choice: 'inspect_bell'});
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
    {...fixture(), knowledge: {...fixture().knowledge, bellHeard: 'true'}},
    {...fixture(), knowledge: {...fixture().knowledge, bellPosition: {x: -1, y: 500}}},
    {...fixture(), knowledge: {...fixture().knowledge, bellPosition: {x: 350}}},
    {...fixture(), knowledge: {...fixture().knowledge, hasLastSeen: true}},
    {...fixture(), knowledge: {...fixture().knowledge, tracksSeen: true}},
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
const jevRelayUrl = upstreamReady.then(() => new Promise((resolveReady, rejectReady) => {
  jevRelay = spawn(process.execPath, ['relay.mjs'], {
    cwd: import.meta.dirname,
    env: {...process.env, JEV_MOCK: '', JEV_RELAY_PORT: '0', TYPESAFE_API_KEY: 'test-only-key',
          TYPESAFE_API_URL: `http://127.0.0.1:${upstream.address().port}/v1/systemone`},
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  let output = '';
  const timeout = setTimeout(() => rejectReady(new Error(`jev relay did not start: ${output}`)), 5000);
  jevRelay.stdout.on('data', chunk => {
    output += chunk;
    const match = output.match(/http:\/\/127\.0\.0\.1:(\d+)/);
    if (match) { clearTimeout(timeout); resolveReady(`http://127.0.0.1:${match[1]}`); }
  });
  jevRelay.stderr.on('data', chunk => { output += chunk; });
  jevRelay.on('exit', code => { clearTimeout(timeout); rejectReady(new Error(`jev relay exit ${code}: ${output}`)); });
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
  assert.deepEqual(await response.json(), {attemptId: 2, decisionId: 7, revision: 3, choice: 'inspect_bell'});
  assert.equal(upstreamCalls.length, 1);
  const sent = upstreamCalls[0];
  assert.equal(sent.authorization, 'Bearer test-only-key');
  assert.deepEqual(sent.body.state.observed_facts, normalize(request).observations);
  assert.deepEqual(Object.keys(sent.body.questions.guardian_intent.criteria), request.legalIntents);
  const text = JSON.stringify(sent.body);
  assert.doesNotMatch(text, /hiddenFoxPosition|secretNestLocation|attemptId|decisionId|test-only-key/);
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
