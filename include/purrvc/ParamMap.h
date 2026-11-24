// Sets an engine parameter from its mockup id and a number (booleans 0/1; tube = index; tvolt/eqpos/emode as 0/1).
#pragma once
#include "VoiceDsp.h"
#include <string>
namespace purrvc {
inline bool setById(Params& T, const std::string& id, double v) {
    bool b = v > 0.5;
#define D(n) if (id == #n) { T.n = v; return true; }
#define B(n) if (id == #n) { T.n = b; return true; }
    D(gpos) B(pad) D(imp) B(ph) B(lc) D(tcol) D(tvoice) B(eqbyp) D(lg) D(lf) D(lq) D(m1g) D(m1f) D(m1q) D(m2g) D(m2f) D(m2q) D(hg) D(hf) D(hq)
    D(thr) D(ratio) D(ds) D(dsf) D(att) D(rel) B(eon) D(ethr) D(out) B(byp) D(dsthr) B(dsw) B(dsl) D(erat) D(eatt) D(ehys) B(elook) D(ekhp) D(eklp) D(tvar) B(tnoise)
    D(itrim) D(makeup) B(amk) D(mix) D(knee) D(schp) D(link) B(arel) D(erange) D(erel) D(ehold) B(lim) B(match) B(delta)
#undef D
#undef B
    if (id == "tvolt") { T.tvoltHigh = b; return true; }
    if (id == "eqpos") { T.eqPost = b; return true; }
    if (id == "emode") { T.gate = b; return true; }
    if (id == "tube") { T.tube = (int)v; return true; }
    return false;
}
}
