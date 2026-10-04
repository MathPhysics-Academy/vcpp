// Drive headless Chrome over CDP: load page, wait for the wasm's own success
// message, record console + exceptions, screenshot, then press scene keys.
// usage: node cdp_run.mjs <label> <serveDir> <httpPort> <cdpPort> <outDir>
import { spawn } from 'node:child_process';
import { writeFileSync, rmSync } from 'node:fs';

const [label, serveDir, httpPort, cdpPort, outDir] = process.argv.slice(2);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const profile = `${outDir}/chrome-prof-cdp-${label}`;
rmSync(profile, { recursive: true, force: true });

const server = spawn('python3', ['-m', 'http.server', httpPort], { cwd: serveDir, stdio: 'ignore' });
const chrome = spawn('/Applications/Google Chrome.app/Contents/MacOS/Google Chrome', [
  '--headless=new', '--enable-unsafe-webgpu', `--remote-debugging-port=${cdpPort}`,
  `--user-data-dir=${profile}`, '--no-first-run', '--no-default-browser-check',
  '--window-size=1280,800', 'about:blank',
], { stdio: 'ignore' });

const cleanup = () => { try { chrome.kill('SIGKILL'); } catch {} try { server.kill('SIGKILL'); } catch {} };
process.on('exit', cleanup);

let wsUrl;
for (let i = 0; i < 60 && !wsUrl; i++) {
  try {
    const list = await (await fetch(`http://127.0.0.1:${cdpPort}/json/list`)).json();
    wsUrl = list.find((t) => t.type === 'page')?.webSocketDebuggerUrl;
  } catch {}
  if (!wsUrl) await sleep(500);
}
if (!wsUrl) { console.log(`[${label}] FAIL: no CDP page target`); process.exit(2); }

const ws = new WebSocket(wsUrl);
await new Promise((r) => ws.addEventListener('open', r));
let nextId = 1;
const pending = new Map();
const events = []; // {t, kind, text}
const t0 = Date.now();
const log = (kind, text) => events.push({ t: ((Date.now() - t0) / 1000).toFixed(1), kind, text });

ws.addEventListener('message', (m) => {
  const msg = JSON.parse(m.data);
  if (msg.id && pending.has(msg.id)) { pending.get(msg.id)(msg); pending.delete(msg.id); return; }
  if (msg.method === 'Runtime.consoleAPICalled') {
    const text = msg.params.args.map((a) => a.value ?? a.description ?? '').join(' ');
    log(msg.params.type, text);
  } else if (msg.method === 'Runtime.exceptionThrown') {
    const d = msg.params.exceptionDetails;
    log('EXCEPTION', d.exception?.description ?? d.text);
  } else if (msg.method === 'Log.entryAdded' && ['error', 'warning'].includes(msg.params.entry.level)) {
    log(`log-${msg.params.entry.level}`, msg.params.entry.text);
  }
});
const send = (method, params = {}) => new Promise((r) => {
  const id = nextId++; pending.set(id, r); ws.send(JSON.stringify({ id, method, params }));
});

await send('Runtime.enable'); await send('Page.enable'); await send('Log.enable');
await send('Page.navigate', { url: `http://localhost:${httpPort}/` });

const has = (s) => events.some((e) => e.text.includes(s));
const deadline = Date.now() + 150000;
while (Date.now() < deadline && !has('Loaded Default Scene') && !events.some((e) => e.kind === 'EXCEPTION')) await sleep(500);
const loaded = has('Loaded Default Scene');
await sleep(3000); // let frames render

const shot = async (name) => {
  const r = await send('Page.captureScreenshot', { format: 'png' });
  if (r.result?.data) writeFileSync(`${outDir}/${name}.png`, Buffer.from(r.result.data, 'base64'));
};
await shot(`cdp_${label}_default`);

const keys = [
  ['Digit1', '1', 49], ['Digit2', '2', 50], ['Digit3', '3', 51], ['Digit4', '4', 52],
  ['Digit5', '5', 53], ['Digit6', '6', 54], ['Digit7', '7', 55], ['Digit8', '8', 56],
  ['Digit9', '9', 57], ['Digit0', '0', 48], ['Minus', '-', 189], ['Equal', '=', 187],
  ['BracketLeft', '[', 219], ['BracketRight', ']', 221],
];
if (loaded) {
  for (const [code, key, vk] of keys) {
    const before = events.length;
    for (const type of ['keyDown', 'keyUp'])
      await send('Input.dispatchKeyEvent', { type, code, key, windowsVirtualKeyCode: vk, text: type === 'keyDown' ? key : undefined });
    await sleep(1500);
    const got = events.slice(before).map((e) => `${e.kind === 'log' ? '' : e.kind + ': '}${e.text}`).join(' | ');
    log('key', `${code} -> ${got || '(no output)'}`);
    if (code === 'Digit9') await shot(`cdp_${label}_labels`);
    if (code === 'Equal') await shot(`cdp_${label}_compound`); // rotates, so compare by eye, not bytes
    if (code === 'BracketLeft') await shot(`cdp_${label}_text3d`);
  }
}

console.log(`===== ${label}: ${loaded ? 'LOADED DEFAULT SCENE' : 'DID NOT REACH DEFAULT SCENE'} =====`);
for (const e of events) console.log(`  [${e.t}s] ${e.kind.padEnd(9)} ${e.text}`);
const exc = events.filter((e) => e.kind === 'EXCEPTION' || e.kind === 'error' || e.kind === 'log-error');
console.log(`  errors/exceptions: ${exc.length}`);
ws.close(); cleanup(); process.exit(0);
