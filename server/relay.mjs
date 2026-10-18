import http from 'node:http';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const host = '127.0.0.1';
const port = Number(process.env.JEV_RELAY_PORT || 8787);
const apiKey = process.env.TYPESAFE_API_KEY || '';
const mock = process.env.JEV_MOCK === '1';
const apiUrl = process.env.TYPESAFE_API_URL || 'https://api.typesafe.ai/v1/systemone';
// Pin a versioned model so results stay comparable; 'jev-latest' may move between test runs.
const model = process.env.JEV_MODEL || 'jev-1.13.0';
const names = new Set([
  'guard_lantern', 'inspect_bell', 'inspect_tracks', 'inspect_last_seen',
  'watch_exit', 'watch_torii_exit', 'watch_log_exit', 'follow_visible_clue', 'wait_and_watch',
]);
const descriptions = {
  guard_lantern: 'Remain at or return to the shrine lantern and guard it carefully.',
  inspect_bell: 'Go to the bell to find what made it ring, while remaining alert for other evidence.',
  inspect_tracks: 'Examine the most recently observed footprints and follow their visible direction.',
  inspect_last_seen: 'Investigate the place where the fox or lantern light was last actually seen.',
  watch_exit: 'Watch the forest fork leading toward the fox den after entering the forest.',
  watch_torii_exit: 'Watch the torii exit on the west side of the shrine.',
  watch_log_exit: 'Watch the fallen-log exit on the east side of the shrine.',
  follow_visible_clue: 'Approach a currently visible fox or carried light, with a warning before contact.',
  wait_and_watch: 'Stay still briefly and observe before choosing a route.',
};

function validPoint(point) {
  return point && typeof point === 'object' &&
    Number.isFinite(point.x) && Number.isFinite(point.y) &&
    point.x >= 0 && point.x <= 1672 && point.y >= 0 && point.y <= 941;
}
function validAge(value) { return Number.isFinite(value) && value >= 0 && value <= 36000; }

function normalize(input) {
  if (!input || !Number.isSafeInteger(input.attemptId) || input.attemptId < 1 ||
      !Number.isSafeInteger(input.decisionId) || input.decisionId < 1 ||
      !Number.isSafeInteger(input.revision) || input.revision < 1 ||
      !Array.isArray(input.legalIntents) || input.legalIntents.length < 1 ||
      input.legalIntents.length > names.size ||
      new Set(input.legalIntents).size !== input.legalIntents.length ||
      !input.legalIntents.every(name => names.has(name))) throw new Error('invalid decision');
  const k = input.knowledge;
  if (!k || typeof k !== 'object') throw new Error('invalid knowledge');
  const keys = ['bellHeard', 'tracksSeen', 'tracksPending', 'foxVisible', 'lightVisible', 'theftKnown', 'falseAlarm',
                'hasLastSeen', 'lastSeenInForest', 'tracksSeenInForest'];
  if (!keys.every(key => typeof k[key] === 'boolean')) throw new Error('invalid knowledge flags');
  if (k.hasLastSeen && !validPoint(k.lastSeen)) throw new Error('invalid last seen');
  if (k.tracksSeen && !validPoint(k.lastTracks)) throw new Error('invalid tracks');
  if (k.bellHeard && !validPoint(k.bellPosition)) throw new Error('invalid bell');
  if (!Number.isSafeInteger(k.bellCount) || k.bellCount < 0 || k.bellCount > 10000 ||
      typeof k.watchCompleted !== 'boolean' ||
      k.bellHeard !== (k.bellCount > 0) || (k.tracksPending && !k.tracksSeen) ||
      (k.bellHeard && !validAge(k.bellAge)) ||
      (k.tracksSeen && !validAge(k.tracksAge)) ||
      (k.hasLastSeen && !validAge(k.lastSeenAge))) throw new Error('invalid clue history');
  const guardian = input.guardian;
  if (!guardian || !['shrine', 'forest'].includes(guardian.scene) ||
      !['stone', 'awakening', 'animal', 'returning'].includes(guardian.form) ||
      !names.has(guardian.currentIntent) || !validPoint(guardian.position))
    throw new Error('invalid guardian');
  const legal = new Set(input.legalIntents);
  if ((legal.has('follow_visible_clue') && !k.foxVisible && !k.lightVisible) ||
      (legal.has('inspect_last_seen') && !k.hasLastSeen) ||
      (legal.has('inspect_tracks') && !k.tracksPending) ||
      (legal.has('inspect_bell') && (!k.bellHeard || k.falseAlarm)) ||
      (['watch_exit', 'watch_torii_exit', 'watch_log_exit'].some(name => legal.has(name)) && !k.theftKnown) ||
      (legal.has('watch_exit') && guardian.scene !== 'forest') ||
      ((legal.has('watch_torii_exit') || legal.has('watch_log_exit')) && guardian.scene !== 'shrine'))
    throw new Error('invalid legal intents');
  // Construct the state afresh so arbitrary client fields never reach Jev.
  const observations = {
    bellHeard: k.bellHeard,
    tracksSeen: k.tracksSeen,
    tracksPending: k.tracksPending,
    foxVisible: k.foxVisible,
    lightVisible: k.lightVisible,
    theftKnown: k.theftKnown,
    previousBellInvestigationEmpty: k.falseAlarm,
    bellCount: k.bellCount,
    watchCompleted: k.watchCompleted,
    lastSeenArea: k.lastSeenInForest ? 'forest' : 'shrine',
    lastTracksArea: k.tracksSeenInForest ? 'forest' : 'shrine',
  };
  if (k.hasLastSeen) { observations.lastSeen = k.lastSeen; observations.lastSeenAge = k.lastSeenAge; }
  if (k.tracksSeen) { observations.lastTracks = k.lastTracks; observations.tracksAge = k.tracksAge; }
  if (k.bellHeard) { observations.bellPosition = k.bellPosition; observations.bellAge = k.bellAge; }
  return {
    attemptId: input.attemptId,
    decisionId: input.decisionId,
    revision: input.revision,
    legalIntents: input.legalIntents,
    observations,
    guardian: {scene: guardian.scene, form: guardian.form,
               currentIntent: guardian.currentIntent, position: guardian.position},
  };
}

function mockChoice(request) {
  const k = request.observations;
  const legal = new Set(request.legalIntents);
  if ((k.foxVisible || k.lightVisible) && legal.has('follow_visible_clue')) return 'follow_visible_clue';
  if (k.theftKnown && k.lastSeen && legal.has('inspect_last_seen')) return 'inspect_last_seen';
  if (k.tracksPending && legal.has('inspect_tracks')) return 'inspect_tracks';
  if (k.bellHeard && !k.previousBellInvestigationEmpty && legal.has('inspect_bell')) return 'inspect_bell';
  if (k.theftKnown && legal.has('watch_exit')) return 'watch_exit';
  if (k.theftKnown && legal.has('watch_torii_exit') && legal.has('watch_log_exit'))
    return Math.hypot(request.guardian.position.x - 160, request.guardian.position.y - 735) <
           Math.hypot(request.guardian.position.x - 1480, request.guardian.position.y - 345) ?
           'watch_torii_exit' : 'watch_log_exit';
  return legal.has('guard_lantern') ? 'guard_lantern' : request.legalIntents[0];
}

export {normalize, mockChoice};

async function decide(request) {
  if (mock) return mockChoice(request);
  if (!apiKey) throw new Error('Jev API key is not configured');
  const criteria = Object.fromEntries(request.legalIntents.map(name => [name, descriptions[name]]));
  const body = {
    state: {
      character: {
        role: 'Single shrine guardian: the stone komainu and living mountain dog are one being.',
        duty: 'Protect the one shrine lantern and restore it if it is lost.',
        curiosity: 'Check concrete evidence before chasing an assumption; prefer recent observed clues.',
        compassion: 'Warn and restrain the young fox without injury. Do not attack.',
        caution: 'After an empty investigation, become less eager to repeat the same lure.',
      },
      observed_facts: request.observations,
      guardian_now: request.guardian,
    },
    model,
    questions: {
      guardian_intent: {
        type: 'choice',
        instructions: 'Choose one legal action now that best fits this guardian\'s duty, curiosity, compassion, caution, current action, and recent observations. If the fox or carried lantern light is currently visible, follow_visible_clue takes priority; after losing sight, investigate the last observed place. Investigated footprints are memory, not a fresh clue; return to the shrine post when no active clue remains. A completed watch should lead to a new action. Use only observed facts. Never assume an unseen fox position or know why the lantern is missing.',
        criteria,
      },
    },
  };
  const response = await fetch(apiUrl, {
    method: 'POST',
    headers: {Authorization: `Bearer ${apiKey}`, 'Content-Type': 'application/json'},
    body: JSON.stringify(body),
    signal: AbortSignal.timeout(2500),
  });
  if (!response.ok) throw new Error(`Jev returned HTTP ${response.status}`);
  // A malformed upstream body is a Jev failure, not a client SyntaxError (400).
  const output = await response.json().catch(() => { throw new Error('Jev returned malformed JSON'); });
  const choice = output?.answers?.guardian_intent?.choice;
  // One line per answered request, for cost tracking. Never includes the key or observations.
  process.stdout.write(`${JSON.stringify({event: 'jev_usage', model: output?.model ?? null,
    inputTokens: output?.usage?.input_tokens ?? null})}\n`);
  if (!request.legalIntents.includes(choice)) throw new Error('Jev returned an illegal choice');
  return choice;
}

function send(res, status, payload) {
  const body = JSON.stringify(payload);
  res.writeHead(status, {'Content-Type': 'application/json; charset=utf-8',
                         'Cache-Control': 'no-store', 'Content-Length': Buffer.byteLength(body)});
  res.end(body);
}

const server = http.createServer(async (req, res) => {
  if (req.method === 'GET' && req.url === '/health') {
    send(res, 200, {ready: mock || Boolean(apiKey), mode: mock ? 'mock' : 'jev'});
    return;
  }
  if (req.method !== 'POST' || req.url !== '/decision') {
    send(res, 404, {error: 'not found'});
    return;
  }
  try {
    let text = '';
    for await (const chunk of req) {
      text += chunk;
      if (text.length > 16384) throw new Error('request too large');
    }
    const request = normalize(JSON.parse(text));
    const choice = await decide(request);
    send(res, 200, {attemptId: request.attemptId, decisionId: request.decisionId,
                    revision: request.revision, choice, mode: mock ? 'mock' : 'jev'});
  } catch (error) {
    const badInput = error instanceof SyntaxError || /^invalid|^request too large/.test(error.message);
    if (!badInput) {
      const upstreamStatus = /^Jev returned HTTP (\d{3})$/.exec(error.message)?.[1];
      const reason = upstreamStatus ? `http_${upstreamStatus}` :
        error.name === 'TimeoutError' ? 'timeout' :
        error.message === 'Jev returned malformed JSON' ? 'malformed_response' :
        error.message === 'Jev returned an illegal choice' ? 'illegal_choice' : 'connection_or_response_error';
      process.stderr.write(`${JSON.stringify({event: 'jev_error', reason})}\n`);
    }
    send(res, badInput ? 400 : 503, {error: badInput ? 'invalid request' : 'decision unavailable'});
  }
});

if (process.argv[1] && fileURLToPath(import.meta.url) === resolve(process.argv[1])) {
  server.listen(port, host, () => {
    process.stdout.write(`Jev relay listening at http://${host}:${server.address().port}\n`);
  });
}
