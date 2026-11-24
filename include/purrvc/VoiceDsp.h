// Purrr Voice Channel: audio engine. Line by line port of VoiceDSP() in docs/mockup/index.html (the mockup IS the plugin).
// Keep both in sync: tests/parity.mjs runs the mockup engine in node and this engine on the same input and compares.
// Pure C++17, no JUCE, real-time safe after construction (no allocation, no locks in process()).
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

namespace purrvc {

constexpr int kNumTubes = 10;
// order = the mockup's TUBES list (and the plugin's Tube menu)
static const char* const kTubeIds[kNumTubes]   = { "au7", "au7a", "at7", "at7a", "ax7", "mw", "tsol", "jj", "sov", "ax7a" };
static const char* const kTubeNames[kNumTubes] = { "Kit Cat 12AU7", "Tabby Tom 12AU7 aged", "Tuxedo 12AT7", "Old Tux 12AT7 aged", "Gold Cat 12AX7",
                                                   "Meowlard 12AX7", "Bengal 12AX7", "Black Cat 12AX7", "Snowy 12AX7", "Alley Cat 12AX7 aged" };
constexpr int kTubeAx7 = 4;

struct Params {           // same names and defaults as the engine's T object in the mockup
    double gpos = 0; bool pad = false; double imp = 1200; bool ph = false, lc = false; bool tvoltHigh = false;
    double tcol = 60, tvoice = 100; bool eqPost = false, eqbyp = false;
    double lg = 0, lf = 150, lq = 0.7, m1g = 0, m1f = 400, m1q = 0.9, m2g = 0, m2f = 3500, m2q = 0.9, hg = 0, hf = 15000, hq = 0.7;
    double thr = -20, ratio = 1, ds = 0, dsf = 6000, att = 10, rel = 400; bool eon = false; double ethr = -50; bool gate = false;
    double out = 0; bool byp = false; int tube = kTubeAx7; double dsthr = -32; bool dsw = false, dsl = false;
    double erat = 1.5, eatt = 2, ehys = 4; bool elook = false; double ekhp = 20, eklp = 20000, tvar = 50; bool tnoise = false;
    double itrim = 0, makeup = 0; bool amk = false; double mix = 100, knee = 6, schp = 0, link = 100; bool arel = false;
    double erange = 60, erel = 40, ehold = 0; bool lim = true, match = false, delta = false;
};

struct Stats { double inL = 0, inR = 0, outL = 0, outR = 0, rms = 0, gr = 0, eg = 0, tubeK = 0, dgr = 0, gdb = -120, lgr = 0,
               shIn = -120, shOut = -120, off = 0, liIn = -120, liOut = -120, mk = 0; int toggles = 0, gopen = 1, lat = 0; };

using Coef = std::array<double, 5>;

namespace detail {
struct Koren { double mu, ex, kg1, kp, kvb; };
struct TubeSpec { int k; double B, RL, Vg; int ag; double ls, hs, fc, sag, gt; int nz; };
// k: 0 ax7, 1 au7, 2 at7 (Koren sets). Same table as TSP in the mockup.
static const Koren KP[3] = { { 100, 1.4, 1060, 600, 300 }, { 21.5, 1.3, 1180, 84, 300 }, { 60, 1.35, 590, 300, 300 } };
static const TubeSpec TSP[kNumTubes] = {
    { 1, 250, 10000, -8.5, 0, -1, 1.5, 24000, 0, -1.5, 4 },     // au7
    { 1, 250, 10000, -6.5, 1, 3, -4, 11000, 0.2, -1.2, 3 },     // au7a
    { 2, 250, 22000, -2.5, 0, 1, 2, 21000, 0.05, 0.3, 4 },      // at7
    { 2, 250, 22000, -1.7, 1, 3.5, -5, 9500, 0.3, -0.8, 3 },    // at7a
    { 0, 250, 100000, -1.8, 0, 2.5, 3, 16000, 0.25, 0.5, 5 },   // ax7
    { 0, 300, 100000, -1.3, 0, 4, -4, 11000, 0.4, 1.5, 5 },     // mw
    { 0, 250, 150000, -1.5, 0, 3, 5, 22000, 0.1, 1, 4 },        // tsol
    { 0, 250, 100000, -2.2, 0, 3.5, -5.5, 12000, 0.25, 0, 4 },  // jj
    { 0, 300, 220000, -2.6, 0, -1, 4, 22000, 0.02, -0.3, 4 },   // sov
    { 0, 250, 100000, -1.1, 1, 3.5, -7, 7500, 0.6, -1.2, 2 } }; // ax7a
constexpr int XM = 16, NT = 4096; constexpr double HH = 2.0 * XM / NT, IH = 1.0 / HH, KIN = 1.0;
struct Plan { Koren p; double B, RL, Vg; };
struct Table { std::vector<double> f, F; double I0 = 0, g = 0; };

inline double koren(const Koren& p, double Vg, double Vp) {
    double e1 = (Vp / p.kp) * std::log(1 + std::exp(p.kp * (1 / p.mu + Vg / std::sqrt(p.kvb + Vp * Vp))));
    return e1 > 0 ? 2 * std::pow(e1, p.ex) / p.kg1 : 0; }
inline Plan planFor(int id, bool hi) { const TubeSpec& sp = TSP[id]; const Koren& k = KP[sp.k];
    Plan pl; pl.p = { k.mu * (sp.ag ? 0.88 : 1), k.ex, k.kg1 * (sp.ag ? 1.35 : 1), k.kp, k.kvb }; pl.B = sp.B * (hi ? 1.35 : 1); pl.RL = sp.RL; pl.Vg = sp.Vg; return pl; }
inline double solveI(const Plan& pl, double vg, double I0) {
    double lo = 0, hi = pl.B / pl.RL, I = std::min(std::max(I0, lo), hi);
    for (int q = 0; q < 60; q++) { double h = I - koren(pl.p, vg, pl.B - I * pl.RL); if (h > 0) hi = I; else lo = I; double nI = (lo + hi) / 2; if (hi - lo < 1e-13) { I = nI; break; } I = nI; }
    return I; }
inline Table buildTable(int id, bool hi) {
    Plan pl = planFor(id, hi); Table tb; tb.f.assign(NT + 1, 0); tb.F.assign(NT + 1, 0);
    tb.I0 = solveI(pl, pl.Vg, 0.001); double dd = 0.005;
    tb.g = (solveI(pl, pl.Vg + dd, tb.I0) - solveI(pl, pl.Vg - dd, tb.I0)) * pl.RL / (2 * dd);
    double I = tb.I0;
    for (int i = 0; i <= NT; i++) { double x = -XM + i * HH, vin = x * KIN, vg = pl.Vg + (vin < 0 ? vin : 0.4 * std::tanh(vin / 0.4)); I = solveI(pl, vg, I);
        tb.f[i] = (I - tb.I0) * pl.RL / tb.g / KIN; if (i > 0) tb.F[i] = tb.F[i - 1] + HH * (tb.f[i - 1] + tb.f[i]) / 2; }
    return tb; }
// all 20 tables (10 tubes x normal / high voltage), built once per process, shared by every instance
inline const std::vector<Table>& tables() { static const std::vector<Table> t = [] { std::vector<Table> v; v.reserve(2 * kNumTubes);
    for (int i = 0; i < kNumTubes; i++) { v.push_back(buildTable(i, false)); v.push_back(buildTable(i, true)); } return v; }(); return t; }
inline const Table& tab(int id, bool hi) { return tables()[2 * id + (hi ? 1 : 0)]; }

struct HB { int M, K; std::vector<int> nz; std::vector<double> hn; };
inline double bessel(double x) { double s = 1, t = 1; for (int k = 1; k < 40; k++) { double q = x / (2 * k); t *= q * q; s += t; } return s; }
inline HB halfband(int M, double beta) { const double PI = 3.14159265358979323846; int K = (M - 1) / 2; std::vector<double> h(M); double sum = 0;
    for (int i = 0; i < M; i++) { int k0 = i - K; double v0 = k0 == 0 ? 0.5 : (k0 % 2 == 0 ? 0 : std::sin(PI * k0 / 2) / (PI * k0)), r0 = (double)k0 / K;
        h[i] = v0 * bessel(beta * std::sqrt(std::max(0.0, 1 - r0 * r0))) / bessel(beta); sum += h[i]; }
    HB H{ M, K, {}, {} }; for (int i = 0; i < M; i++) { h[i] /= sum; if (std::fabs(h[i]) > 1e-12) { H.nz.push_back(i); H.hn.push_back(h[i]); } } return H; }
struct Ring { std::vector<double> b; int p = 0, M = 0; void init(int m) { M = m; b.assign(2 * m, 0); p = 0; } void clear() { std::fill(b.begin(), b.end(), 0); }
    void push(double v) { b[p] = v; b[p + M] = v; p = p + 1 == M ? 0 : p + 1; }
    double dot(const HB& H) const { double s = 0; const double* bb = b.data() + p; for (size_t q = 0; q < H.nz.size(); q++) s += H.hn[q] * bb[H.nz[q]]; return s; } };
inline double bqs(const Coef& q, double* z, double v) { double y = q[0] * v + z[0]; z[0] = q[1] * v - q[3] * y + z[1]; z[1] = q[2] * v - q[4] * y; return y; }
} // namespace detail

class VoiceDsp {
public:
    static constexpr int SUB = 16, LMASK = 1023;
    explicit VoiceDsp(double sampleRate) { prepare(sampleRate); }
    Params T;

    void prepare(double sampleRate) {
        using namespace detail;
        sr = sampleRate; osr = sr * 4; detail::tables();
        H1 = halfband(47, 7); H2 = halfband(17, 6); LAT = H1.K + H2.K / 2;
        LN = std::max(8, (int)std::lround(0.0015 * sr));
        for (auto& c : C) { c.u1.init(47); c.d1.init(47); c.u2.init(17); c.d2.init(17); }
        twStep = 1 / (0.03 * sr);
        { double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196, K = std::tan(PI * f0 / sr), Vh = std::pow(10, G / 20), Vb = std::pow(Vh, 0.4996667741545416), a0 = 1 + K / Q + K * K;
          KS1 = { (Vh + Vb * K / Q + K * K) / a0, 2 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0 };
          f0 = 38.13547087602444; Q = 0.5003270373238773; K = std::tan(PI * f0 / sr); a0 = 1 + K / Q + K * K; KS2 = { 1, -2, 1, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0 }; }
        subLen = (int)std::lround(0.1 * sr); stA = 1 - std::exp(-1 / (1.0 * sr)); offA = 1 - std::exp(-SUB / (0.8 * sr));
        dtR = std::exp(-1 / (0.005 * sr)); NL = std::min((int)std::lround(0.002 * sr), 900 - 40);
        detR = std::exp(-1 / (0.0015 * sr)); lcCoef = std::exp(-2 * PI * 100 / sr); swA = 1 - std::exp(-1 / (0.004 * sr));
        bmA = 1 - std::exp(-1 / (0.008 * sr)); dkA = 1 - std::exp(-1 / (0.005 * sr)); smA = 1 - std::exp(-SUB / (0.02 * sr)); kA = 1 - std::exp(-SUB / (5.5 * sr));
        lrc = 1 - std::exp(-1 / (0.08 * sr)); sgK = 1 - std::exp(-1 / (0.05 * sr));
        emA = 1 - std::pow(1 - 0.0015, 48000 / sr); ceR = std::pow(0.9997, 48000 / sr); dcR = std::pow(0.9995, 48000 / sr);
        for (int q = 1; q <= 3; q++) { double t = q / 4.0, sm = 0; for (int k = -3; k <= 4; k++) { double x = k - t, sn = std::fabs(x) < 1e-9 ? 1 : std::sin(PI * x) / (PI * x), w = 0.5 + 0.5 * std::cos(PI * x / 4.5); TPC[q - 1][k + 3] = sn * w; sm += sn * w; } for (int k = 0; k < 8; k++) TPC[q - 1][k] /= sm; }
        curId = kTubeAx7; nxtId = -1;   // like the mockup: starts on Gold Cat and glides to T.tube in 30 ms tw = 0; tubeK = 0; kEff = 0; lastKE = -1; pendB = true;
        reset();
    }
    int latency() const { return LAT + LN; }           // samples (plus NL while gate lookahead is on)
    int latencyNow() const { return LAT + LN + (c[CLK] > 0.0005 ? NL : 0); }

    void reset() {
        toggles = 0; stp0 = 0;
        for (auto& o : C) { o.u1.clear(); o.d1.clear(); o.u2.clear(); o.d2.clear(); o.xp = o.fp = o.lp = o.dcx = o.dcy = o.lc = o.sg = o.dv = o.dg = 0; o.em = o.ce = 0;
            std::memset(o.bz, 0, sizeof o.bz); std::memset(o.vz, 0, sizeof o.vz); std::memset(o.kz, 0, sizeof o.kz); std::memset(o.z, 0, sizeof o.z); std::memset(o.tz, 0, sizeof o.tz);
            std::memset(o.hz, 0, sizeof o.hz); std::memset(o.kin, 0, sizeof o.kin); std::memset(o.kout, 0, sizeof o.kout);
            std::fill(std::begin(o.dl), std::end(o.dl), 0.0); std::fill(std::begin(o.gdl), std::end(o.gdl), 0.0); std::fill(std::begin(o.lb), std::end(o.lb), 0.0); o.dp = 0; o.lv = o.g1 = o.g2 = 0; }
        inited = false; lvE = 0; hold2 = 0; smMix = 1; smDsw = 0; smDsl = 0; gp = 0; eg = 0; gateOpen = true;
        std::fill(std::begin(gg), std::end(gg), 1.0); std::fill(std::begin(rr), std::end(rr), 1.0); sumR = LN; Rprev = 1; sIn = sOut = off = 0; resetLoud(); lastTone = 9;
    }
    void resetLoudness() { resetLoud(); }
    void setTubeK(double k) { tubeK = k; }   // jump the voltage glide (used when a preset or project loads)

    // transfer curve of a tube at the given voltage mix (for the tube view)
    static double xfer(int id, double k, double x) { double lf, lff; detail::Table const& a = detail::tab(id, false); lk(a, x, lf, lff); double r = lf * (1 - k);
        if (k > 0.001) { lk(detail::tab(id, true), x, lf, lff); r += k * lf; } return r; }
    static detail::TubeSpec spec(int id) { return detail::TSP[std::clamp(id, 0, kNumTubes - 1)]; }

    // Vicanek matched biquad: type 0 peak, 1 low shelf, 2 high shelf; returns [b0,b1,b2,a1,a2]
    static Coef design(int type, double f, double g, double Q, double fs) {
        f = std::min(std::max(f, 10.0), 0.45 * fs); Q = std::max(Q, 0.05);
        double A = std::pow(10, g / 40), w0 = 2 * PI * f / fs, wp, zeta, A2 = A * A;
        if (type == 0) { wp = w0; zeta = 1 / (2 * A * Q); } else if (type == 1) { wp = w0 / std::sqrt(A); zeta = 1 / (2 * Q); } else { wp = w0 * std::sqrt(A); zeta = 1 / (2 * Q); }
        double e = std::exp(-zeta * wp), a1, a2 = e * e;
        if (zeta < 1) a1 = -2 * e * std::cos(wp * std::sqrt(1 - zeta * zeta)); else a1 = -2 * e * std::cosh(wp * std::sqrt(zeta * zeta - 1));
        auto mag2 = [&](double fr) { double O = fr / f, O2 = O * O, n, d;
            if (type == 0) { n = (1 - O2) * (1 - O2) + A2 * O2 / (Q * Q); d = (1 - O2) * (1 - O2) + O2 / (A2 * Q * Q); return n / d; }
            if (type == 1) { n = (A - O2) * (A - O2) + A * O2 / (Q * Q); d = (1 - A * O2) * (1 - A * O2) + A * O2 / (Q * Q); return A2 * n / d; }
            n = (1 - A * O2) * (1 - A * O2) + A * O2 / (Q * Q); d = (A - O2) * (A - O2) + A * O2 / (Q * Q); return A2 * n / d; };
        double A0 = (1 + a1 + a2) * (1 + a1 + a2), A1 = (1 - a1 + a2) * (1 - a1 + a2), A2c = -4 * a2, H0 = mag2(0), Hn = mag2(fs / 2), B0 = A0 * H0, B1 = A1 * Hn, s0 = std::sqrt(B0), s1 = std::sqrt(B1), W = (s0 + s1) / 2;
        auto fit = [&](double fm) { double wm = 2 * PI * fm / fs, ph0 = std::pow(std::cos(wm / 2), 2), ph1 = std::pow(std::sin(wm / 2), 2), ph2 = 4 * ph0 * ph1, Dm = A0 * ph0 + A1 * ph1 + A2c * ph2,
            B2 = (mag2(fm) * Dm - B0 * ph0 - B1 * ph1) / ph2, b0 = (W + std::sqrt(std::max(0.0, W * W + B2))) / 2; return Coef{ b0, (s0 - s1) / 2, -B2 / (4 * b0), a1, a2 }; };
        auto errOf = [&](const Coef& cf) { double m = 0; for (int q = 1; q <= 9; q++) { double fr = q * 0.05 * fs, w = 2 * PI * fr / fs, cw = std::cos(w), sw = std::sin(w);
            double nr = cf[0] + cf[1] * cw + cf[2] * (2 * cw * cw - 1), ni = -(cf[1] * sw + cf[2] * 2 * sw * cw), dr = 1 + cf[3] * cw + cf[4] * (2 * cw * cw - 1), di = -(cf[3] * sw + cf[4] * 2 * sw * cw);
            double dbv = 10 * std::log10((nr * nr + ni * ni) / (dr * dr + di * di + 1e-30)) - 10 * std::log10(mag2(fr)); if (std::fabs(dbv) > m) m = std::fabs(dbv); } return m; };
        Coef best = fit(f);
        if (f > 0.2 * fs) { Coef c2 = fit(0.25 * fs), c3 = fit(0.33 * fs); double e1 = errOf(best), e2 = errOf(c2), e3 = errOf(c3); if (e2 < e1 && e2 <= e3) best = c2; else if (e3 < e1) best = c3; }
        return best;
    }
    // Impedance as a tone control: z -1 (150 Ohm) .. +1 (3.4 kOhm)
    static double toneZ(double imp) { double z = imp >= 1200 ? std::log(imp / 1200) / std::log(3400.0 / 1200) : std::log(imp / 1200) / std::log(1200.0 / 150); return std::max(-1.0, std::min(1.0, z)); }

    const Stats& process(const float* iL, const float* iR, float* oL, float* oR, int n) {
        double pkIL = 0, pkIR = 0, pkOL = 0, pkOR = 0, rms = 0, grMax = 0, egMax = 0, dgMax = 0; lgrMax = 0;
        const float* ii[2] = { iL, iR };
        for (int s = 0; s < n; s += SUB) {
            step(); int e = std::min(n, s + SUB);
            for (int m = s; m < e; m++) {
                den = -den; double ramp = (double)(m - s + 1) / SUB; double a0 = std::fabs((double)iL[m]), a1 = std::fabs((double)iR[m]); if (a0 > pkIL) pkIL = a0; if (a1 > pkIR) pkIR = a1;
                sgn += ((T.ph ? -1 : 1) - sgn) * swA; lcm += ((T.lc ? 1 : 0) - lcm) * swA; eqm += ((T.eqbyp ? 0 : 1) - eqm) * swA;
                double dkT = 1; if (T.eqPost != curPost) { dkT = 0; if (dk < 0.02) { curPost = T.eqPost; std::memset(C[0].z, 0, sizeof C[0].z); std::memset(C[1].z, 0, sizeof C[1].z); } }
                dk += (dkT - dk) * dkA; smMix += (c[MIX] - smMix) * dkA; smDsw += (c[DSW] - smDsw) * dkA; smDsl += (c[DSL] - smDsl) * dkA;
                // preamp: gain, noise, supply sag, tube (4x, ADAA), colour, voicing, DC, low cut, impedance tone, invert
                for (int k = 0; k < 2; k++) {
                    Ch& ci = C[k]; double xin = ii[k][m]; x0[k] = xin; ci.dl[ci.dp] = xin; nzs = nzs * 1664525u + 1013904223u;
                    double v = xin * (gin0 + (gin - gin0) * ramp) + c[TN] * (((double)nzs / 2147483648.0) - 1) + den;
                    ci.sg += (std::fabs(v) - ci.sg) * sgK; v /= 1 + 2 * sagA * c[VM] * ci.sg;
                    ci.u1.push(v); double ua = 2 * ci.u1.dot(H1); ci.u1.push(0); double ub = 2 * ci.u1.dot(H1);
                    for (int r = 0; r < 2; r++) { double w = r ? ub : ua; ci.u2.push(w); double p0 = 2 * ci.u2.dot(H2); ci.u2.push(0); double p1 = 2 * ci.u2.dot(H2);
                        ci.d2.push(tubeStage(ci, p0)); double back = ci.d2.dot(H2); ci.d2.push(tubeStage(ci, p1)); if (r) ub = back; else ua = back; }
                    ci.d1.push(ua); v = ci.d1.dot(H1); ci.d1.push(ub);
                    double tmE = c[CC] * v * v / (std::fabs(v) + 0.02); ci.em += (tmE - ci.em) * emA; v += tmE - ci.em;
                    double avS = std::fabs(v), ceN = ci.ce; ceN = avS > ceN ? avS : ceN * ceR; ci.ce = ceN;
                    if (c[GT2] > 0.001) { double ySat = ceN * std::tanh(v / (0.35 * ceN + 1e-9)); v += c[GT2] * (ySat - v); }
                    v = detail::bqs(vc[3], ci.vz + 6, detail::bqs(vc[2], ci.vz + 4, detail::bqs(vc[1], ci.vz + 2, detail::bqs(vc[0], ci.vz, v))));
                    double dcy = v - ci.dcx + dcR * ci.dcy; ci.dcx = v; ci.dcy = dcy; v = dcy;
                    ci.lc = (1 - lcCoef) * v + lcCoef * ci.lc; v = v - lcm * ci.lc;
                    v = detail::bqs(tc2[1], ci.tz + 2, detail::bqs(tc2[0], ci.tz, v));
                    x[k] = v * sgn;
                }
                if (!curPost) for (int k = 0; k < 2; k++) { double q0 = x[k]; x[k] = q0 + eqm * (eq(C[k], q0) - q0); }
                double gi = it0 + (it1 - it0) * ramp; x[0] *= gi; x[1] *= gi;
                // expander / gate
                double kf0 = detail::bqs(gkL, C[0].kz + 2, detail::bqs(gkH, C[0].kz, x[0])), kf1 = detail::bqs(gkL, C[1].kz + 2, detail::bqs(gkH, C[1].kz, x[1])), lv = std::max(std::fabs(kf0), std::fabs(kf1));
                lvE = lv > lvE ? lv : lvE * dtR; double ldb = 20 * std::log10(lvE + 1e-9), tg = 0, ethr = c[ETHR]; ldbLast = ldb;
                if (T.eon) {
                    int hs = T.ehold > 0 ? (int)std::lround(T.ehold * 0.001 * sr) : 0;
                    if (T.gate) {
                        if (gateOpen) { openT++; if (ldb < ethr - c[EHYS]) { if (hold > 0) hold--; else { gateOpen = false; toggles++; } } else hold = hs > 0 ? hs : (int)std::lround(std::min(0.25, std::max(0.02, openT / sr * 0.25)) * sr); }
                        else { openT = 0; if (ldb > ethr) { gateOpen = true; toggles++; } }
                        tg = gateOpen ? 0 : c[ERNG];
                    } else {
                        gateOpen = true; tg = ldb < ethr ? std::min(c[ERNG], (c[ERAT] - 1) * (ethr - ldb)) : 0;
                        if (hs > 0) { if (ldb >= ethr) hold2 = hs; else if (hold2 > 0) { hold2--; if (tg > eg) tg = eg; } }
                    }
                    eg += (tg - eg) * (tg > eg ? gClose : gOpen);
                } else { eg += (0 - eg) * gOpen; gateOpen = true; }
                C[0].gdl[gp] = x[0]; C[1].gdl[gp] = x[1];
                if (c[CLK] > 0.0005) { double dg0 = C[0].gdl[(gp - NL) & 1023], dg1 = C[1].gdl[(gp - NL) & 1023]; x[0] += c[CLK] * (dg0 - x[0]); x[1] += c[CLK] * (dg1 - x[1]); }
                gp = (gp + 1) & 1023;
                double egl = lin(-eg); x[0] *= egl; x[1] *= egl; if (eg > egMax) egMax = eg;
                // compressor / limiter
                bool cOn = c[RATIO] > 1.0005; double mkg = mk0 + (mk1 - mk0) * ramp, ref = 0, W = c[KNEE], half = W / 2;
                for (int k = 0; k < 2; k++) { Ch& ci = C[k]; double xk = x[k], dx = xk + c[HPM] * (detail::bqs(hpc, ci.hz, xk) - xk); double dt = std::fabs(dx); ci.lv = dt > ci.lv ? dt : ci.lv * detR; if (ci.lv > ref) ref = ci.lv; }
                for (int k = 0; k < 2; k++) {
                    Ch& ci = C[k]; double xk2 = x[k], lvk = ci.lv + c[LINK] * (ref - ci.lv), grk, ycomp;
                    if (cOn) {
                        double dd = 20 * std::log10(lvk + 1e-9), over = dd - c[THR], tr = 0;
                        if (over > half) tr = over * slope; else if (over > -half && W > 0.01) tr = slope * (over + half) * (over + half) / (2 * W);
                        ci.g1 += (tr - ci.g1) * (tr > ci.g1 ? aA : rA); ci.g2 += (ci.g1 - ci.g2) * (ci.g1 > ci.g2 ? sAt : sRl);
                        grk = ci.g1 + c[ARM] * (smax(ci.g1, ci.g2) - ci.g1);
                        ycomp = xk2 * lin(-grk) * mkg; ycomp = xk2 + smMix * (ycomp - xk2);
                    } else { ci.g1 += (0 - ci.g1) * rA; ci.g2 += (ci.g1 - ci.g2) * sRl; grk = ci.g1; ycomp = xk2 + smMix * (xk2 * mkg - xk2); }
                    if (grk > grMax) { grMax = grk; }
                    yy[k] = ycomp;
                }
                // de-esser
                double bd0 = detail::bqs(bpc, C[0].bz, yy[0]), bd1 = detail::bqs(bpc, C[1].bz, yy[1]), ab0 = std::fabs(bd0), ab1 = std::fabs(bd1);
                C[0].dv = ab0 > C[0].dv ? ab0 : C[0].dv * dvR; C[1].dv = ab1 > C[1].dv ? ab1 : C[1].dv * dvR;
                double dvm = std::max(C[0].dv, C[1].dv), bdb = 20 * std::log10(dvm + 1e-9);
                if (bdb > -75) denv += (bdb - denv) * (bdb > denv ? denvA : denvR);
                double dsv = c[DS], dcap = dsv <= 0.25 ? 24 * dsv : dsv <= 0.75 ? 6 + 16 * (dsv - 0.25) : 14 + 24 * (dsv - 0.75);
                double dover = bdb - (denv + 9 - 11 * dsv), dtg = dsv > 0.001 ? std::min(dcap, std::max(0.0, dover)) : 0;
                C[0].dg += (dtg - C[0].dg) * (dtg > C[0].dg ? dgA : dgR); double dgl = lin(-C[0].dg); if (C[0].dg > dgMax) dgMax = C[0].dg;
                double o0 = smDsw * yy[0] * (dgl - 1) + yy[0] - (1 - smDsw) * (1 - dgl) * bd0, o1 = smDsw * yy[1] * (dgl - 1) + yy[1] - (1 - smDsw) * (1 - dgl) * bd1;
                yy[0] = o0 + smDsl * (bd0 - o0); yy[1] = o1 + smDsl * (bd1 - o1);
                if (curPost) { double p0e = yy[0], p1e = yy[1]; yy[0] = p0e + eqm * (eq(C[0], p0e) - p0e); yy[1] = p1e + eqm * (eq(C[1], p1e) - p1e); }
                double g2 = (gout0 + (gout - gout0) * ramp) * dk; yy[0] *= g2; yy[1] *= g2;
                // loudness (K weighted)
                double eI = 0, eO = 0; for (int kk = 0; kk < 2; kk++) { double ki = kf(x0[kk], C[kk].kin), ko = kf(yy[kk], C[kk].kout); eI += ki * ki; eO += ko * ko; }
                sIn += (eI - sIn) * stA; sOut += (eO - sOut) * stA; sbI += eI; sbO += eO;
                if (++sbN >= subLen) { for (int q = 0; q < 3; q++) { rI[q] = rI[q + 1]; rO[q] = rO[q + 1]; } rI[3] = sbI; rO[3] = sbO; sbI = 0; sbO = 0; sbN = 0;
                    if (++nb >= 4) { binAdd(cI, sI, (rI[0] + rI[1] + rI[2] + rI[3]) / (4.0 * subLen)); binAdd(cO, sO, (rO[0] + rO[1] + rO[2] + rO[3]) / (4.0 * subLen)); } }
                // Before / After, delta
                double tA = T.byp ? (T.delta ? 1 : 0) : 1, tD2 = T.byp ? (T.delta ? -1 : 1) : (T.delta ? -1 : 0); cA += (tA - cA) * bmA; cD += (tD2 * lin(c[GMDB]) - cD) * bmA;
                double d0 = C[0].dl[(C[0].dp + 1024 - LAT) & 1023], d1 = C[1].dl[(C[1].dp + 1024 - LAT) & 1023];
                if (c[CLK] > 0.0005) { d0 += c[CLK] * (C[0].dl[(C[0].dp + 1024 - LAT - NL) & 1023] - d0); d1 += c[CLK] * (C[1].dl[(C[1].dp + 1024 - LAT - NL) & 1023] - d1); }
                yy[0] = cA * yy[0] + cD * d0; yy[1] = cA * yy[1] + cD * d1;
                C[0].dp = (C[0].dp + 1) & 1023; C[1].dp = (C[1].dp + 1) & 1023;
                // lookahead true-peak safety limiter, ceiling -1.2 dBFS
                double pk = 0, p;
                for (int kq = 0; kq < 2; kq++) { double* lb = C[kq].lb; lb[lbP] = yy[kq]; int b0 = lbP - 4; p = std::fabs(lb[b0 & LMASK]); if (p > pk) pk = p;
                    for (int ph = 0; ph < 3; ph++) { double acc = 0; for (int tk = 0; tk < 8; tk++) acc += TPC[ph][tk] * lb[(b0 - 3 + tk) & LMASK]; p = std::fabs(acc); if (p > pk) pk = p; } }
                double gnow = (T.lim && pk > CEIL) ? CEIL / pk : 1; gg[lbP] = gnow; double Mn = 1; for (int qq = 0; qq < LN; qq++) { double gv = gg[(lbP - qq) & LMASK]; if (gv < Mn) Mn = gv; }
                double Rn = Rprev + (1 - Rprev) * lrc; if (Mn < Rn) Rn = Mn; Rprev = Rn; sumR += Rn - rr[(lbP - LN) & LMASK]; rr[lbP] = Rn; double G = sumR / LN; if (1 - G > lgrMax) lgrMax = 1 - G;
                for (int kq = 0; kq < 2; kq++) oy[kq] = C[kq].lb[(lbP - LN) & LMASK] * G;
                lbP = (lbP + 1) & LMASK;
                if (!std::isfinite(oy[0]) || !std::isfinite(oy[1])) { reset(); oy[0] = oy[1] = 0; }   // never pass NaN or inf to the host
                oL[m] = (float)oy[0]; oR[m] = (float)oy[1]; double b0 = std::fabs(oy[0]), b1 = std::fabs(oy[1]); if (b0 > pkOL) pkOL = b0; if (b1 > pkOR) pkOR = b1;
                rms += oy[0] * oy[0] + oy[1] * oy[1]; wChg = false;
            }
        }
        st.inL = pkIL; st.inR = pkIR; st.outL = pkOL; st.outR = pkOR; st.rms = std::sqrt(rms / (2.0 * std::max(1, n))); st.gr = T.byp ? 0 : grMax; st.eg = T.byp ? 0 : egMax;
        st.tubeK = tubeK; st.toggles = toggles; st.dgr = dgMax; st.gdb = ldbLast; st.gopen = gateOpen ? 1 : 0; st.lgr = lgrMax > 0 ? -20 * std::log10(1 - lgrMax) : 0;
        st.lat = latencyNow(); st.mk = c[MKDB] + c[AMKM] * autoMk(); st.liIn = integ(cI, sI); st.liOut = integ(cO, sO);
        return st;
    }
    const Stats& stats() const { return st; }

private:
    static constexpr double PI = 3.14159265358979323846, CEIL = 0.87096358995608;
    enum Key { GDB, ODB, IMZ, LG, LF, LQ, M1G, M1F, M1Q, M2G, M2F, M2Q, HG, HF, HQ, THR, RATIO, LATT, LREL, DS, LDSF, DSTHR, DSL, DSW, COL, VM, CC, GT2, FC,
               VLS, VHS, VLM, VPR, ITDB, MKDB, MIX, KNEE, LSCH, LINK, ETHR, ERNG, ERAT, EHYS, LEREL, LEATT, LKH, LKL, CLK, EONM, TN, AMKM, HPM, ARM, GMDB, NKEYS };
    struct Ch { detail::Ring u1, d1, u2, d2; double xp = 0, fp = 0, lp = 0, dcx = 0, dcy = 0, lc = 0, sg = 0, dv = 0, dg = 0, lv = 0, g1 = 0, g2 = 0, em = 0, ce = 0;
        double vz[8]{}, bz[2]{}, kz[4]{}, z[8]{}, tz[4]{}, hz[2]{}, kin[4]{}, kout[4]{}; double dl[1024]{}, gdl[1024]{}, lb[1024]{}; int dp = 0; };

    static double lin(double d) { return std::pow(10.0, d / 20); }
    static void lk(const detail::Table& tb, double x, double& LF, double& LFF) { using namespace detail; double t = (x + XM) * IH;
        if (t <= 0) { LF = tb.f[0]; LFF = tb.F[0] + (x + XM) * tb.f[0]; return; } if (t >= NT) { LF = tb.f[NT]; LFF = tb.F[NT] + (x - XM) * tb.f[NT]; return; }
        int i = (int)t; double fr = t - i, a = tb.f[i], d = tb.f[i + 1] - a; LF = a + fr * d; LFF = tb.F[i] + HH * fr * (a + 0.5 * fr * d); }
    Coef rbj(int kind, double f, double Q) const { double w = 2 * PI * f / sr, cs = std::cos(w), al = std::sin(w) / (2 * Q), ia = 1 / (1 + al);
        if (kind == 0) return { (1 + cs) / 2 * ia, -(1 + cs) * ia, (1 + cs) / 2 * ia, -2 * cs * ia, (1 - al) * ia };
        if (kind == 1) return { (1 - cs) / 2 * ia, (1 - cs) * ia, (1 - cs) / 2 * ia, -2 * cs * ia, (1 - al) * ia };
        return { al * ia, 0, -al * ia, -2 * cs * ia, (1 - al) * ia }; }
    void setWeights() {
        wn = 0; int ids[2] = { curId, nxtId }; double wi[2] = { nxtId >= 0 ? 1 - tw : 1, tw }; int nid = nxtId >= 0 ? 2 : 1;
        for (int q = 0; q < nid; q++) { const detail::Table* tn = &detail::tab(ids[q], false); const detail::Table* th = kEff > 0.0005 ? &detail::tab(ids[q], true) : nullptr; double kk = th ? kEff : 0;
            if (wi[q] * (1 - kk) > 0) { wT[wn] = tn; wW[wn++] = wi[q] * (1 - kk); } if (th && wi[q] * kk > 0) { wT[wn] = th; wW[wn++] = wi[q] * kk; } }
        if (!wn) { wT[0] = &detail::tab(kTubeAx7, false); wW[0] = 1; wn = 1; } wChg = true; }
    double fmixF(double xv) const { double F = 0, lf, lff; for (int q = 0; q < wn; q++) { lk(*wT[q], xv, lf, lff); F += wW[q] * lff; } return F; }
    double fmixf(double xv) const { double f = 0, lf, lff; for (int q = 0; q < wn; q++) { lk(*wT[q], xv, lf, lff); f += wW[q] * lf; } return f; }
    double tubeStage(Ch& ci, double v) { double xd = v - ci.xp, y, Fv = fmixF(v), Fp = wChg ? fmixF(ci.xp) : ci.fp;
        if (std::fabs(xd) > 1e-5) y = (Fv - Fp) / xd; else y = fmixf((v + ci.xp) / 2);
        ci.xp = v; ci.fp = Fv; ci.lp += osA * (y - ci.lp); return ci.lp; }
    double eq(Ch& ci, double v) { for (int b = 0; b < 4; b++) v = detail::bqs(eqc[b], ci.z + b * 2, v); return v; }
    static double smax(double a, double b) { return 0.5 * (a + b + std::sqrt((a - b) * (a - b) + 1e-4)); }
    double kf(double v, double* z) const { double y = KS1[0] * v + z[0]; z[0] = KS1[1] * v - KS1[3] * y + z[1]; z[1] = KS1[2] * v - KS1[4] * y;
        double y2 = KS2[0] * y + z[2]; z[2] = KS2[1] * y - KS2[3] * y2 + z[3]; z[3] = KS2[2] * y - KS2[4] * y2; return y2; }
    static void binAdd(double* cnt, double* sum, double ms) { double L = -0.691 + 10 * std::log10(ms + 1e-20); if (L > -70 && L < 10) { int b = (int)std::floor((L + 70) * 10); cnt[b]++; sum[b] += ms; } }
    static double integ(const double* cnt, const double* sum) { double n = 0, s = 0; for (int b = 0; b < 800; b++) { n += cnt[b]; s += sum[b]; } if (!n) return -120;
        double thr = -0.691 + 10 * std::log10(s / n) - 10; int bt = std::max(0, (int)std::ceil((thr + 70) * 10)); n = 0; s = 0; for (int b = bt; b < 800; b++) { n += cnt[b]; s += sum[b]; }
        return n ? -0.691 + 10 * std::log10(s / n) : -120; }
    void resetLoud() { std::fill(std::begin(cI), std::end(cI), 0.0); std::fill(std::begin(sI), std::end(sI), 0.0); std::fill(std::begin(cO), std::end(cO), 0.0); std::fill(std::begin(sO), std::end(sO), 0.0);
        sbI = sbO = 0; sbN = 0; nb = 0; for (int q = 0; q < 4; q++) rI[q] = rO[q] = 0; }
    double autoMk() const { if (c[RATIO] < 1.0005) return 0; double sl = 1 - 1 / c[RATIO], W = c[KNEE], o = 6, half = W / 2; return o > half ? sl * o : (W > 0.01 ? sl * (o + half) * (o + half) / (2 * W) : 0); }

    void step() {
        double tgv = T.tvoltHigh ? 1 : 0; tubeK += (tgv - tubeK) * kA; kEff = tubeK;
        int tube = std::clamp(T.tube, 0, kNumTubes - 1);
        const detail::TubeSpec& sp = detail::TSP[tube];
        double tz = toneZ(T.imp), vr = T.tvar <= 0 ? 0.2 : 0.2 + 4.8 * std::pow(T.tvar / 100, 1.2), ex = vr, colk = T.tcol / 100, vm = std::max(0.0, std::min(3.0, T.tvoice / 100));
        double nzL = std::min(-50.0, -90 + 5 * (5 - sp.nz) * (1 + T.tvar / 100));
        double t[NKEYS];
        t[GDB] = 0.6 * T.gpos - (T.pad ? 20 : 0) + 1.5 * tubeK + std::max(-8.0, std::min(8.0, sp.gt * ex * vm)); t[ODB] = T.out <= -59.5 ? -120 : T.out; t[IMZ] = tz;
        t[LG] = T.lg; t[LF] = T.lf; t[LQ] = std::log(T.lq); t[M1G] = T.m1g; t[M1F] = T.m1f; t[M1Q] = std::log(T.m1q); t[M2G] = T.m2g; t[M2F] = T.m2f; t[M2Q] = std::log(T.m2q);
        t[HG] = T.hg; t[HF] = T.hf; t[HQ] = std::log(T.hq); t[THR] = T.thr; t[RATIO] = T.ratio; t[LATT] = std::log(T.att); t[LREL] = std::log(T.rel); t[DS] = T.ds / 100; t[LDSF] = std::log(T.dsf);
        t[DSTHR] = T.dsthr; t[DSL] = T.dsl ? 1 : 0; t[DSW] = T.dsw ? 1 : 0; t[COL] = colk; t[VM] = vm; t[CC] = 0.7 * colk * colk + 0.1 * colk; t[GT2] = std::max(0.0, std::min(1.0, (colk - 0.4) / 0.6));
        t[ITDB] = T.itrim; t[MKDB] = T.makeup; t[MIX] = T.mix / 100; t[KNEE] = T.knee; t[LSCH] = std::log(std::max(20.0, T.schp > 0 ? T.schp : 100)); t[LINK] = T.link / 100;
        t[ETHR] = T.ethr; t[ERNG] = T.erange; t[ERAT] = T.erat; t[EHYS] = T.ehys; t[LEREL] = std::log(T.erel); t[LEATT] = std::log(T.eatt); t[LKH] = std::log(T.ekhp); t[LKL] = std::log(T.eklp);
        t[CLK] = T.elook ? 1 : 0; t[EONM] = T.eon ? 1 : 0; t[TN] = T.tnoise ? std::pow(10, nzL / 20) * 1.732 : 0; t[AMKM] = T.amk ? 1 : 0; t[HPM] = T.schp > 0 ? 1 : 0; t[ARM] = T.arel ? 1 : 0; t[GMDB] = T.match ? off : 0;
        t[FC] = 24000 + std::max(3500.0 - 24000, (sp.fc * (1 + 0.3 * tubeK) - 24000) * ex * (vm <= 1 ? vm : 1 + 0.2 * (vm - 1)));
        t[VLS] = std::max(-18.0, std::min(18.0, sp.ls * ex * vm)); t[VHS] = std::max(-18.0, std::min(18.0, sp.hs * ex * vm)); t[VLM] = 2.5 * std::max(0.0, vm - 1); t[VPR] = 3 * std::max(0.0, vm - 1);
        if (tube != (nxtId >= 0 ? nxtId : curId)) { if (nxtId >= 0) curId = nxtId; nxtId = tube; tw = 0; }
        sagA = sp.sag * ex;
        bool eqMoved = false, hpMoved = false, keyMoved = false, dsMoved = false;
        for (int k = 0; k < NKEYS; k++) { double d = t[k] - c[k]; if (!inited) c[k] = t[k]; else if (d != 0) { c[k] += d * smA; if (std::fabs(t[k] - c[k]) < 1e-6 * (1 + std::fabs(t[k]))) c[k] = t[k];
            if (k >= LG && k <= HQ) { eqMoved = true; }
            if (k == LSCH) { hpMoved = true; }
            if (k == LKH || k == LKL) { keyMoved = true; }
            if (k == LDSF) { dsMoved = true; } } }
        if (!inited) { eqMoved = hpMoved = keyMoved = dsMoved = true; lcm = T.lc ? 1 : 0; sgn = T.ph ? -1 : 1; eqm = T.eqbyp ? 0 : 1; cA = T.byp ? (T.delta ? 1 : 0) : 1; inited = true; }
        gin0 = gin; gout0 = gout; gin = lin(c[GDB]); gout = lin(c[ODB]); it0 = it1; it1 = lin(c[ITDB]); mk0 = mk1; mk1 = lin(c[MKDB] + c[AMKM] * autoMk());
        if (!stp0) { gin0 = gin; gout0 = gout; it0 = it1; mk0 = mk1; stp0 = 1; }
        if (eqMoved) { eqc[0] = design(1, c[LF], c[LG], std::exp(c[LQ]), sr); eqc[1] = design(0, c[M1F], c[M1G], std::exp(c[M1Q]), sr); eqc[2] = design(0, c[M2F], c[M2G], std::exp(c[M2Q]), sr); eqc[3] = design(2, c[HF], c[HG], std::exp(c[HQ]), sr); }
        if (c[IMZ] != lastTone) { tc2[0] = design(2, 3500, 3 * c[IMZ], 0.6, sr); tc2[1] = design(0, 220, -1.5 * c[IMZ], 0.5, sr); lastTone = c[IMZ]; }
        if (hpMoved) { double fh = std::exp(c[LSCH]), wh = 2 * PI * std::min(fh, 0.45 * sr) / sr, cs = std::cos(wh), al = std::sin(wh) / (2 * 0.7071), ia = 1 / (1 + al); hpc = { (1 + cs) / 2 * ia, -(1 + cs) * ia, (1 + cs) / 2 * ia, -2 * cs * ia, (1 - al) * ia }; }
        double att = std::exp(c[LATT]), rel = std::exp(c[LREL]) * (1 - 0.6 * c[ARM]), erl = std::exp(c[LEREL]), eat = std::exp(c[LEATT]);
        aA = 1 - std::exp(-1 / (att * 0.001 * sr)); rA = 1 - std::exp(-1 / (rel * 0.001 * sr)); slope = 1 - 1 / std::max(1.0, c[RATIO]);
        sAt = 1 - std::exp(-1 / (3 * std::exp(c[LREL]) * 0.001 * sr)); sRl = 1 - std::exp(-1 / (2.5 * std::exp(c[LREL]) * 0.001 * sr));
        gClose = 1 - std::exp(-1 / (erl * 0.001 * sr)); gOpen = 1 - std::exp(-1 / (eat * 0.001 * sr));
        if (keyMoved) { gkH = rbj(0, std::exp(c[LKH]), 0.7071); gkL = rbj(1, std::min(std::exp(c[LKL]), 0.45 * sr), 0.7071); }
        if (dsMoved) bpc = rbj(2, std::min(std::exp(c[LDSF]), 0.45 * sr), 1.4);
        dgA = 1 - std::exp(-1 / (0.0004 * sr)); dgR = 1 - std::exp(-1 / (0.03 * sr)); dvR = std::exp(-1 / (0.002 * sr)); denvA = 1 - std::exp(-1 / (0.3 * sr)); denvR = 1 - std::exp(-1 / (1.5 * sr));
        osA = 1 - std::exp(-2 * PI * std::min(c[FC], osr * 0.45) / osr);
        if (c[VLS] != lastV[0] || c[VHS] != lastV[1] || c[VLM] != lastV[2] || c[VPR] != lastV[3]) { vc[0] = design(1, 160, c[VLS], 0.7, sr); vc[1] = design(2, 5000, c[VHS], 0.7, sr); vc[2] = design(0, 300, c[VLM], 0.8, sr); vc[3] = design(0, 3200, c[VPR], 0.9, sr);
            lastV[0] = c[VLS]; lastV[1] = c[VHS]; lastV[2] = c[VLM]; lastV[3] = c[VPR]; }
        if (nxtId >= 0) { tw += twStep * SUB; if (tw >= 1) { curId = nxtId; nxtId = -1; tw = 0; } }
        if (nxtId >= 0 || tubeK != lastKE || pendB) { setWeights(); lastKE = tubeK; pendB = false; }
        double lI = -0.691 + 10 * std::log10(sIn + 1e-12), lO = -0.691 + 10 * std::log10(sOut + 1e-12);
        if (lI > -60 && lO > -60) { double of = std::max(-24.0, std::min(24.0, lO - lI)); off += (of - off) * offA; }
        st.shIn = lI; st.shOut = lO; st.off = off;
    }

    double sr = 48000, osr = 192000; detail::HB H1, H2; int LAT = 0, LN = 8, NL = 96;
    Ch C[2]; double c[NKEYS]{}; double lcm = 0, sgn = 1, eqm = 1, cA = 1, cD = 0, dk = 1;
    bool inited = false; int toggles = 0, stp0 = 0, hold = 0, openT = 0, hold2 = 0; double tubeK = 0, kEff = 0, lvE = 0, eg = 0, den = 1e-25; bool curPost = false, gateOpen = true;
    double gin0 = 1, gin = 1, gout0 = 1, gout = 1, it0 = 1, it1 = 1, mk0 = 1, mk1 = 1;
    Coef vc[4]{ Coef{1,0,0,0,0}, Coef{1,0,0,0,0}, Coef{1,0,0,0,0}, Coef{1,0,0,0,0} }; double lastV[4]{ 9, 9, 9, 9 }; double sagA = 0;
    Coef bpc{ 1,0,0,0,0 }, eqc[4]{ Coef{1,0,0,0,0}, Coef{1,0,0,0,0}, Coef{1,0,0,0,0}, Coef{1,0,0,0,0} }, tc2[2]{ Coef{1,0,0,0,0}, Coef{1,0,0,0,0} }, hpc{ 1,0,0,0,0 }; double lastTone = 9;
    Stats st;
    int curId = kTubeAx7, nxtId = -1; double tw = 0, twStep = 0; const detail::Table* wT[4]{}; double wW[4]{}; int wn = 0; bool wChg = true; double lastKE = -1; bool pendB = true;
    Coef KS1{}, KS2{};
    double sIn = 0, sOut = 0, off = 0, sbI = 0, sbO = 0; int sbN = 0, nb = 0; double rI[4]{}, rO[4]{}, cI[800]{}, sI[800]{}, cO[800]{}, sO[800]{};
    int subLen = 4800; double stA = 0, offA = 0;
    double aA = 0, rA = 0, slope = 0, osA = 0, gOpen = 0, gClose = 0, sAt = 0, sRl = 0; Coef gkH{ 1,0,0,0,0 }, gkL{ 1,0,0,0,0 };
    double dgA = 0, dgR = 0, dvR = 0; uint32_t nzs = 12345; double dtR = 0; int gp = 0;
    double detR = 0, lcCoef = 0, swA = 0, bmA = 0, dkA = 0, smA = 0, kA = 0, lrc = 0;
    double TPC[3][8]{}; double ldbLast = -120, denv = -70, denvA = 0, denvR = 0, sgK = 0; int lbP = 0; double gg[1024]{}, rr[1024]{}, sumR = 8, Rprev = 1, lgrMax = 0;
    double emA = 0.0015, ceR = 0.9997, dcR = 0.9995;
    double smMix = 1, smDsw = 0, smDsl = 0, x[2]{}, x0[2]{}, yy[2]{}, oy[2]{};
};

} // namespace purrvc
