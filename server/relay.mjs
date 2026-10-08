import http from 'node:http';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const host = '127.0.0.1';
const port = Number(process.env.JEV_RELAY_PORT || 8787);
const apiKey = process.env.TYPESAFE_API_KEY || '';
const mock = process.env.JEV_MOCK === '1';
const apiUrl = process.env.TYPESAFE_API_URL || 'https://api.typesafe.ai/v1/systemone';
const names = new Set([
  'guard_lantern', 'inspect_bell', 'inspect_tracks', 'inspect_last_seen',
  'watch_exit', 'follow_visible_clue', 'wait_and_watch',
]);
const descriptions = {
  guard_lantern: 'Remain at or return to the shrine lantern and guard it carefully.',
  inspect_bell: 'Go to the bell to find what made it ring, while remaining alert for other evidence.',
  inspect_tracks: 'Examine the most recently observed footprints and follow their visible direction.',
  inspect_last_seen: 'Investigate the place where the fox or lantern light was last actually seen.',
  watch_exit: 'Watch a shrine exit without assuming which path an unseen fox took.',
  follow_visible_clue: 'Approach a currently visible fox or carried light, with a warning before contact.',
  wait_and_watch: 'Stay still briefly and observe before choosing a route.',
};

function validPoint(point) {
  return point && typeof point === 'object' &&
    Number.isFinite(point.x) && Number.isFinite(point.y) &&
    point.x >= 0 && point.x <= 1672 && point.y >= 0 && point.y <= 941;
}

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
  const keys = ['bellHeard', 'tracksSeen', 'foxVisible', 'theftKnown', 'falseAlarm',
                'hasLastSeen', 'lastSeenInForest', 'tracksSeenInForest'];
  if (!keys.every(key => typeof k[key] === 'boolean')) throw new Error('invalid knowledge flags');
  if (k.hasLastSeen && !validPoint(k.lastSeen)) throw new Error('invalid last seen');
  if (k.tracksSeen && !validPoint(k.lastTracks)) throw new Error('invalid tracks');
  if (k.bellHeard && !validPoint(k.bellPosition)) throw new Error('invalid bell');
  const legal = new Set(input.legalIntents);
  if ((legal.has('follow_visible_clue') && !k.foxVisible) ||
      (legal.has('inspect_last_seen') && !k.hasLastSeen) ||
      (legal.has('inspect_tracks') && !k.tracksSeen) ||
      (legal.has('inspect_bell') && !k.bellHeard) ||
      (legal.has('watch_exit') && !k.theftKnown)) throw new Error('invalid legal intents');
  // Construct the state afresh so arbitrary client fields never reach Jev.
  const observations = {
    bellHeard: k.bellHeard,
    tracksSeen: k.tracksSeen,
    foxVisible: k.foxVisible,
    theftKnown: k.theftKnown,
    previousBellInvestigationEmpty: k.falseAlarm,
    lastSeenArea: k.lastSeenInForest ? 'forest' : 'shrine',
    lastTracksArea: k.tracksSeenInForest ? 'forest' : 'shrine',
  };
  if (k.hasLastSeen) observations.lastSeen = k.lastSeen;
  if (k.tracksSeen) observations.lastTracks = k.lastTracks;
  if (k.bellHeard) observations.bellPosition = k.bellPosition;
  return {
    attemptId: input.attemptId,
    decisionId: input.decisionId,
    revision: input.revision,
    legalIntents: input.legalIntents,
    observations,
  };
}

function mockChoice(request) {
  const k = request.observations;
  const legal = new Set(request.legalIntents);
  if (k.foxVisible && legal.has('follow_visible_clue')) return 'follow_visible_clue';
  if (k.theftKnown && k.lastSeen && legal.has('inspect_last_seen')) return 'inspect_last_seen';
  if (k.tracksSeen && legal.has('inspect_tracks')) return 'inspect_tracks';
  if (k.bellHeard && !k.previousBellInvestigationEmpty && legal.has('inspect_bell')) return 'inspect_bell';
  if (k.theftKnown && legal.has('watch_exit')) return 'watch_exit';
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
    },
    model: 'jev-latest',
    questions: {
      guardian_intent: {
        type: 'choice',
        instructions: 'Choose one legal action now that best fits this guardian\'s duty, curiosity, compassion, caution, and only its observed facts. Never assume an unseen fox position or know why the lantern is missing.',
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
                    revision: request.revision, choice});
  } catch (error) {
    const badInput = error instanceof SyntaxError || /^invalid|^request too large/.test(error.message);
    send(res, badInput ? 400 : 503, {error: badInput ? 'invalid request' : 'decision unavailable'});
  }
});

if (process.argv[1] && fileURLToPath(import.meta.url) === resolve(process.argv[1])) {
  server.listen(port, host, () => {
    process.stdout.write(`Jev relay listening at http://${host}:${server.address().port}\n`);
  });
}
