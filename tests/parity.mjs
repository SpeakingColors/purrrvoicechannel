// Parity test: the mockup engine (VoiceDSP in docs/mockup/index.html) and the C++ engine on the same input and settings.
// Run: node tests/parity.mjs <render binary>      Prints the worst difference per case in dB below full scale.
import fs from "fs"; import { execFileSync } from "child_process";
const html = fs.readFileSync(new URL("../docs/mockup/index.html", import.meta.url), "utf8");
const a = html.indexOf("function VoiceDSP(sr) {"), b = html.indexOf("\n}\n", a);
const VoiceDSP = new Function(html.slice(a, b + 2) + "\nreturn VoiceDSP;")();
const TUBES = ["au7", "au7a", "at7", "at7a", "ax7", "mw", "tsol", "jj", "sov", "ax7a"];
const BIN = process.argv[2] || "/tmp/vc_render";
// test signal: 3 s, tones + noise bursts with gaps (gate), an S-like hiss burst (de-esser), a loud section (limiter)
function signal(sr) { const n = Math.round(3 * sr), L = new Float32Array(n), R = new Float32Array(n); let z = 1;
  const rnd = () => { z = (z * 1103515245 + 12345) >>> 0; return z / 4294967296 * 2 - 1; };
  for (let i = 0; i < n; i++) { const t = i / sr, gate = (t % 0.5) < 0.32 ? 1 : 0.003, loud = t > 2.2 ? 3.5 : 1;
    const v = 0.15 * Math.sin(2 * Math.PI * 180 * t) + 0.08 * Math.sin(2 * Math.PI * 1170 * t + 0.3) + 0.04 * Math.sin(2 * Math.PI * 3300 * t);
    const hiss = (t > 1.2 && t < 1.5) ? 0.12 * rnd() : 0.01 * rnd();
    L[i] = (v + hiss) * gate * loud; R[i] = (0.9 * v + hiss * 0.8) * gate * loud; } return [L, R]; }
const base = { gpos: 20, pad: 0, imp: 1200, ph: 0, lc: 0, tvolt: 0, tcol: 50, tvoice: 100, tvar: 30, tnoise: 0, tube: 4, eqpos: 0, eqbyp: 0,
  lg: 0, lf: 150, lq: 0.7, m1g: 0, m1f: 400, m1q: 0.9, m2g: 0, m2f: 3500, m2q: 0.9, hg: 0, hf: 15000, hq: 0.7, thr: -20, ratio: 1, att: 10, rel: 400, arel: 0,
  itrim: 0, makeup: 0, amk: 0, knee: 6, schp: 80, mix: 100, link: 100, lim: 1, ds: 0, dsf: 6000, dsthr: -32, dsw: 0, dsl: 0, eon: 0, emode: 0, ethr: -80,
  erat: 1.5, erange: 60, eatt: 1, ehold: 0, erel: 40, ehys: 4, elook: 0, ekhp: 80, eklp: 20000, out: 0, byp: 0, match: 0, delta: 0 };
const cases = [
  ["defaults", {}],
  ["heavy colour + voicing 300 + variation 100", { tcol: 100, tvoice: 300, tvar: 100, gpos: 45 }],
  ["high voltage, Alley Cat", { tvolt: 1, tube: 9, tcol: 70 }],
  ["comp + de-ess + auto release + makeup", { ratio: 6, thr: -30, att: 2, rel: 150, arel: 1, amk: 1, ds: 70, dsf: 7000 }],
  ["gate + EQ post + low cut + invert", { eon: 1, emode: 1, ethr: -40, eqpos: 1, lg: 4, m1g: -3, m2g: 5, hg: 6, hf: 5000, lc: 1, ph: 1 }],
  ["expander + lookahead + pad + impedance", { eon: 1, ethr: -45, elook: 1, pad: 1, gpos: 80, imp: 150 }],
  ["Kit Cat, out -6, mix 50", { tube: 0, out: -6, mix: 50, ratio: 3 }],
];
const toArgs = (p) => Object.entries(p).map(([k, v]) => k + "=" + v);
let worst = -999;
for (const sr of [48000, 44100]) {
  const [L, R] = signal(sr), n = L.length, inter = new Float32Array(2 * n);
  for (let i = 0; i < n; i++) { inter[2 * i] = L[i]; inter[2 * i + 1] = R[i]; }
  fs.writeFileSync("/tmp/pvc_in.f32", Buffer.from(inter.buffer));
  for (const [name, over] of cases) {
    const p = Object.assign({}, base, over), d = VoiceDSP(sr), tp = {};
    for (const k in p) tp[k] = p[k];
    tp.tube = TUBES[p.tube]; tp.tvolt = p.tvolt ? "high" : "normal"; tp.eqpos = p.eqpos ? "post" : "pre"; tp.emode = p.emode ? "gate" : "exp";
    for (const k of ["pad", "ph", "lc", "tnoise", "eqbyp", "arel", "amk", "lim", "dsw", "dsl", "eon", "elook", "byp", "match", "delta"]) tp[k] = !!p[k];
    TUBES.forEach((id) => d.xfer(id, 1, 0));            // build every tube table first (the C++ engine builds them all up front)
    d.set(tp);
    const oL = new Float32Array(n), oR = new Float32Array(n);
    for (let s = 0; s < n; s += 128) { const m = Math.min(128, n - s); d.process(L.subarray(s, s + m), R.subarray(s, s + m), oL.subarray(s, s + m), oR.subarray(s, s + m), m); }
    execFileSync(BIN, [String(sr), "/tmp/pvc_in.f32", "/tmp/pvc_out.f32", ...toArgs(p)]);
    const c = new Float32Array(fs.readFileSync("/tmp/pvc_out.f32").buffer.slice(0));
    let md = 0, pk = 0; for (let i = 0; i < n; i++) { md = Math.max(md, Math.abs(c[2 * i] - oL[i]), Math.abs(c[2 * i + 1] - oR[i])); pk = Math.max(pk, Math.abs(oL[i])); }
    const db = 20 * Math.log10(md + 1e-30); worst = Math.max(worst, db);
    console.log(`${sr} ${name.padEnd(45)} max diff ${db.toFixed(1)} dBFS (peak out ${(20 * Math.log10(pk)).toFixed(1)} dBFS)`);
  }
}
console.log("worst", worst.toFixed(1), "dBFS"); process.exit(worst < -100 ? 0 : 1);
