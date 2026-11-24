// Offline render: render <sr> <in.f32> <out.f32> [id=value ...]   (stereo interleaved float32, blocks of 128)
#include "purrvc/ParamMap.h"
#include <cstdio>
#include <vector>
#include <string>
int main(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: render sr in.f32 out.f32 [id=v...]\n"); return 2; }
    double sr = std::atof(argv[1]); FILE* f = std::fopen(argv[2], "rb"); if (!f) return 3;
    std::vector<float> in; float buf[4096]; size_t r; while ((r = std::fread(buf, 4, 4096, f)) > 0) in.insert(in.end(), buf, buf + r); std::fclose(f);
    purrvc::VoiceDsp d(sr);
    for (int i = 4; i < argc; i++) { std::string a = argv[i]; auto p = a.find('='); if (p == std::string::npos || !purrvc::setById(d.T, a.substr(0, p), std::atof(a.c_str() + p + 1))) { std::fprintf(stderr, "bad param %s\n", argv[i]); return 4; } }
    size_t n = in.size() / 2; std::vector<float> L(n), R(n), oL(n), oR(n), out(2 * n);
    for (size_t i = 0; i < n; i++) { L[i] = in[2 * i]; R[i] = in[2 * i + 1]; }
    for (size_t s = 0; s < n; s += 128) { int m = (int)std::min<size_t>(128, n - s); d.process(&L[s], &R[s], &oL[s], &oR[s], m); }
    for (size_t i = 0; i < n; i++) { out[2 * i] = oL[i]; out[2 * i + 1] = oR[i]; }
    FILE* o = std::fopen(argv[3], "wb"); std::fwrite(out.data(), 4, out.size(), o); std::fclose(o);
    std::printf("latency %d\n", d.latency()); return 0;
}
