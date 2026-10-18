// Run deliberately with TYPESAFE_API_KEY set. Sends a small, fixed set of game decisions
// through the real relay and Jev API; never prints the key or raw request bodies.
import {spawn} from 'node:child_process';
import {performance} from 'node:perf_hooks';
import {fileURLToPath} from 'node:url';
import {normalize} from './relay.mjs';

const baseKnowledge = {
  bellHeard: false, bellCount: 0, tracksSeen: false, tracksPending: false,
  foxVisible: false, lightVisible: false, theftKnown: false, falseAlarm: false,
  hasLastSeen: false, lastSeenInForest: false, tracksSeenInForest: false,
  watchCompleted: false,
};

function scenario(name, knowledge, legalIntents, preferred = null, required = false,
                  guardian = {}) {
  return {
    name, preferred, required,
    request: {
      attemptId: 1, decisionId: 1, revision: 1, legalIntents,
      guardian: {scene: 'shrine', form: 'stone', currentIntent: 'guard_lantern',
                 position: {x: 1005, y: 470}, ...guardian},
      knowledge: {...baseKnowledge, ...knowledge},
    },
  };
}

const bell = {bellHeard: true, bellCount: 1, bellAge: 0.2,
              bellPosition: {x: 350, y: 500}};
const tracks = {tracksSeen: true, tracksPending: true, tracksAge: 0.2,
                lastTracks: {x: 880, y: 500}};
const watch = ['watch_torii_exit', 'watch_log_exit'];
const cases = [
  scenario('bell_only', bell, ['guard_lantern', 'inspect_bell', 'wait_and_watch']),
  scenario('tracks_only', tracks, ['guard_lantern', 'inspect_tracks', 'wait_and_watch'],
           'inspect_tracks'),
  scenario('bell_and_tracks', {...bell, ...tracks},
           ['guard_lantern', 'inspect_bell', 'inspect_tracks', 'wait_and_watch'],
           'inspect_tracks'),
  scenario('bell_checked_tracks_remain', {...bell, ...tracks, falseAlarm: true},
           ['guard_lantern', 'inspect_tracks', 'wait_and_watch'], 'inspect_tracks'),
  scenario('theft_seen', {theftKnown: true},
           watch, 'watch_log_exit'),
  scenario('exit_watch_completed', {theftKnown: true, watchCompleted: true},
           ['guard_lantern'], 'guard_lantern'),
  scenario('carried_light_visible', {
    theftKnown: true, lightVisible: true, hasLastSeen: true,
    lastSeen: {x: 1230, y: 470}, lastSeenAge: 0.1,
  }, ['follow_visible_clue'],
  'follow_visible_clue', true),
  scenario('tracks_investigated', {...tracks, tracksPending: false},
           ['guard_lantern', 'wait_and_watch'], 'guard_lantern'),
  scenario('new_tracks_during_return', {...tracks, lastTracks: {x: 850, y: 520}},
           ['guard_lantern', 'inspect_tracks', 'wait_and_watch'], 'inspect_tracks', false,
           {form: 'returning', currentIntent: 'guard_lantern', position: {x: 940, y: 480}}),
];

for (const entry of cases) normalize(entry.request);
if (process.argv.includes('--dry-run')) {
  console.log(`Validated ${cases.length} live Jev scenarios without contacting the API.`);
  process.exit(0);
}
if (!process.env.TYPESAFE_API_KEY) {
  console.error('TYPESAFE_API_KEY is required for the live Jev smoke test.');
  process.exit(2);
}

const relay = spawn(process.execPath, [fileURLToPath(new URL('./relay.mjs', import.meta.url))], {
  env: {...process.env, JEV_MOCK: '', JEV_RELAY_PORT: '0'},
  stdio: ['ignore', 'pipe', 'pipe'],
});
let relayOutput = '';
let stdoutBuffer = '';
let stderrBuffer = '';
const usage = [];
const relayErrors = [];
const ready = new Promise((resolveReady, rejectReady) => {
  const timer = setTimeout(() => rejectReady(new Error('Jev relay did not start')), 5000);
  relay.stdout.on('data', chunk => {
    relayOutput += chunk;
    stdoutBuffer += chunk;
    const lines = stdoutBuffer.split('\n');
    stdoutBuffer = lines.pop();
    for (const line of lines) {
      if (!line.startsWith('{')) continue;
      try {
        const event = JSON.parse(line);
        if (event.event === 'jev_usage') usage.push(event);
      } catch { /* A partial line is harmless; response validation still runs. */ }
    }
    const match = relayOutput.match(/http:\/\/127\.0\.0\.1:(\d+)/);
    if (match) { clearTimeout(timer); resolveReady(`http://127.0.0.1:${match[1]}`); }
  });
  relay.stderr.on('data', chunk => {
    stderrBuffer += chunk;
    const lines = stderrBuffer.split('\n');
    stderrBuffer = lines.pop();
    for (const line of lines) {
      try {
        const event = JSON.parse(line);
        if (event.event === 'jev_error') relayErrors.push(event.reason);
      } catch { /* Only structured, non-secret diagnostics are retained. */ }
    }
  });
  relay.on('error', error => { clearTimeout(timer); rejectReady(error); });
  relay.on('exit', code => {
    if (!relayOutput.includes('Jev relay listening')) {
      clearTimeout(timer);
      rejectReady(new Error(`Jev relay exited before startup (${code})`));
    }
  });
});

try {
  const url = `${await ready}/decision`;
  const durations = [];
  let failures = 0;
  let qualityIssues = 0;
  let attempted = 0;
  for (const [index, entry] of cases.entries()) {
    attempted++;
    entry.request.decisionId = index + 1;
    const started = performance.now();
    let result;
    try {
      const response = await fetch(url, {
        method: 'POST', headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(entry.request), signal: AbortSignal.timeout(6000),
      });
      const payload = await response.json();
      const latencyMs = Math.round(performance.now() - started);
      durations.push(latencyMs);
      const valid = response.status === 200 && payload.mode === 'jev' &&
                    payload.attemptId === 1 && payload.decisionId === index + 1 &&
                    payload.revision === 1 && entry.request.legalIntents.includes(payload.choice);
      const preferredMatched = entry.preferred === null || payload.choice === entry.preferred;
      if (!valid || (entry.required && !preferredMatched)) failures++;
      if (valid && !preferredMatched) qualityIssues++;
      result = {scenario: entry.name, status: response.status, choice: payload.choice ?? null,
                latencyMs, valid, preferred: entry.preferred, preferredMatched};
    } catch (error) {
      failures++;
      result = {scenario: entry.name, error: error.name, valid: false};
    }
    console.log(JSON.stringify(result));
    // Authentication, relay, and transport failures make later scenarios uninformative.
    if (!result.valid && (result.status !== 200 || result.error)) break;
  }
  const sorted = durations.toSorted((a, b) => a - b);
  const percentile = fraction => sorted.length ? sorted[Math.ceil(sorted.length * fraction) - 1] : null;
  console.log(JSON.stringify({summary: true, modelRequested: process.env.JEV_MODEL || 'jev-1.13.0',
    scenariosPlanned: cases.length, scenariosAttempted: attempted,
    failures, qualityIssues, p50Ms: percentile(0.5),
    p95Ms: percentile(0.95), inputTokens: usage.reduce((sum, event) => sum + (event.inputTokens ?? 0), 0),
    usageEvents: usage.length, relayErrors}));
  if (failures) process.exitCode = 1;
} finally {
  relay.kill();
}
