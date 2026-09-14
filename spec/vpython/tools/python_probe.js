const fs = require('fs'), path = require('path');
// SELF = this tree (the committed CSVs); REF = the third-party glowscript clone, outside the repo.
const SELF = path.dirname(__dirname);
const REF = process.env.VPYTHON_REF ||
  path.join(process.env.HOME, 'workspace/math-physics-academy/vpython-reference');
if (!fs.existsSync(REF + '/glowscript/lib/rapydscript/compiler.js')) {
  console.error('no glowscript checkout under ' + REF + ' — set VPYTHON_REF'); process.exit(1);
}
const RS = require(REF + '/glowscript/lib/rapydscript/compiler.js').RapydScript;
const C = RS.create_embedded_compiler();

// EVERY bare alias the runtime installs: `name = ρσ_something`, anywhere on a line.
const rt = fs.readFileSync(REF + '/glowscript/lib/rapydscript/runtime.js', 'utf8');
const alias = new Set();
const re = /\b([A-Za-z_]\w*)\s*=\s*ρσ_(\w+)/g;
let m; while ((m = re.exec(rt)) !== null) if (!m[1].startsWith('ρ')) alias.add(m[1]);
const glow = new Set(fs.readFileSync(SELF + '/catalog/globals.csv', 'utf8')
  .split('\n').slice(1).map(l => l.split(',')[0]).filter(Boolean));
console.log('runtime bare aliases (' + alias.size + '): ' + [...alias].sort().join(' '));
console.log();

const NAMES = ['len','range','print','int','float','str','bool','dict','set','list','tuple','enumerate',
  'reversed','sorted','sum','min','max','abs','round','pow','divmod','zip','map','filter','any','all',
  'type','repr','chr','ord','hex','bin','getattr','hasattr','callable','iter','dir','input','id'];
// Names the compiler WRITES INTO the program text rather than exporting, so they appear in neither
// the runtime aliases nor the glow exports — which is also why catalog/globals.csv misses them:
//   round: GScompiler.js:1254 — function round(num, n=0), Python ndigits, shadows the Math.round export
//   input: GScompiler.js:1268 — inserted only when the source text mentions `input`
// Without this set the probe reports `input` as unavailable and contradicts PYTHON_SUBSET.md.
const injected = new Set(['input', 'round']);
const bad = [];
console.log('name        source(s) of the name');
for (const n of NAMES) {
  const src = [];
  if (alias.has(n)) src.push('RapydScript runtime');
  if (glow.has(n)) src.push('GlowScript global');
  if (injected.has(n)) src.push('compiler-injected');
  if (!src.length) bad.push(n);
  console.log(n.padEnd(11) + ' ' + (src.length ? src.join(' + ') : '*** NOT AVAILABLE'));
}
console.log('\nNOT AVAILABLE in Web VPython: ' + (bad.join(' ') || '(none)'));

console.log('\nre-testing the two syntax failures:');
const T = {
  'lambda a: a+1':        'f = lambda a: a+1',
  'lambda: 1 (no args)':  'f = lambda: 1',
  'def(a): anon':         'f = def(a):\n    return a+1',
  'while/else':           'while False:\n    pass\nelse:\n    pass',
  'for/else':             'for i in range(2):\n    pass\nelse:\n    pass',
};
for (const k in T) {
  try { C.compile(T[k], 't'); console.log('  ' + k.padEnd(22) + ' compiles'); }
  catch (e) { console.log('  ' + k.padEnd(22) + ' NO -> ' + String(e.message||e).split('\n')[0].slice(0,50)); }
}
