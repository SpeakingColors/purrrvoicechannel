#!/usr/bin/env python3
"""Reads the tour data of the Purr Voice Channel mockup straight from its DOM and draws the manual pictures.
Run:  python3 tools/shoot_tour.py        (needs: pip install playwright pillow; playwright install chromium)
Writes docs/tour/tour.json (controls, ranges, info-bubble texts, displays), docs/tour/window.png (numbered pink
badges on controls, lettered teal badges on displays), one crop per panel and one picture per display view.
How the data is read: the page attaches a tooltip to every control with a pointerenter listener. The script records
those elements, fires the event for each one and reads the bubble text the page itself shows. Knob ranges are read by
pressing Arrow keys on the knob and reading the value text, so nothing is typed in by hand."""
import json, os, re, sys
from playwright.sync_api import sync_playwright
from PIL import Image, ImageDraw, ImageFont

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(root, "docs", "tour")
MOCK = "file://" + os.path.join(root, "docs", "mockup", "index.html")
os.makedirs(OUT, exist_ok=True)

INIT = """
window.__ptr = [];
const _add = EventTarget.prototype.addEventListener;
EventTarget.prototype.addEventListener = function (t, f, o) {
  if (t === 'pointerenter' && this instanceof Element) window.__ptr.push(this);
  return _add.call(this, t, f, o);
};
const _st = window.setTimeout;
window.setTimeout = function (f, d, ...a) { return _st(f, d === 1000 ? 0 : d, ...a); };
"""

COLLECT = """
async () => {
  const tipEl = document.querySelector('.tip');
  const seen = new Set(), out = [];
  const secOf = el => { const s = el.closest('section'); return s ? (s.id || '') : ''; };
  for (const el of window.__ptr) {
    if (seen.has(el) || tipEl.contains(el)) continue; seen.add(el);
    const r = el.getBoundingClientRect(); if (r.width < 4 || r.height < 4) continue;
    el.dispatchEvent(new PointerEvent('pointerenter', { bubbles: false }));
    await new Promise(res => setTimeout(res, 8));
    if (tipEl.hidden) continue;
    const lab = tipEl.querySelector('b'); const label = lab ? lab.textContent : '';
    const text = tipEl.textContent.slice(label.length);
    tipEl.hidden = true;
    const kind = el.classList.contains('kw') ? 'knob' : el.tagName === 'LABEL' ? 'menu' : el.tagName === 'BUTTON' ? 'button' : 'display';
    const o = { label, tip: text, kind, cls: el.className.toString(), section: secOf(el), id: el.id || '', r: [r.x, r.y, r.width, r.height] };
    if (kind === 'knob') {
      const k = el.querySelector('.knob'), kv = el.querySelector('.kv');
      const lab2 = el.querySelector('.kl'); if (lab2) o.label = lab2.textContent;
      o.def = kv.textContent;
      const fire = (key, n) => { for (let i = 0; i < n; i++) k.dispatchEvent(new KeyboardEvent('keydown', { key, bubbles: true })); };
      fire('ArrowDown', 70); o.min = kv.textContent; fire('ArrowUp', 140); o.max = kv.textContent; fire('Home', 1);
    }
    if (kind === 'menu') { const s = el.querySelector('select'); if (s) o.options = [...s.options].map(x => x.textContent); }
    out.push(o);
  }
  const secs = [...document.querySelectorAll('section.sec')].map(s => { const r = s.getBoundingClientRect(); const h = s.querySelector('h3'); return { id: s.id, title: h ? h.firstChild.textContent.trim() : s.id, sub: h && h.querySelector('small') ? h.querySelector('small').textContent : '', r: [r.x, r.y, r.width, r.height] }; });
  return { controls: out, sections: secs, W: innerWidth, H: document.documentElement.scrollHeight };
}
"""

def main():
    with sync_playwright() as p:
        b = p.chromium.launch()
        pg = b.new_page(viewport={"width": 2200, "height": 1100})
        errs = []; pg.on("console", lambda m: errs.append(m.text) if m.type == "error" else None)
        pg.add_init_script(INIT)
        pg.goto(MOCK); pg.wait_for_timeout(1500)
        data = pg.evaluate(COLLECT)
        # display views: click each view tab and shoot the display area
        pl0 = [c for c in data["controls"] if c["label"] == "Play"]
        if pl0: pg.mouse.click(pl0[0]["r"][0] + pl0[0]["r"][2] / 2, pl0[0]["r"][1] + pl0[0]["r"][3] / 2)
        pg.mouse.move(5, 5); pg.wait_for_timeout(2500)
        views = []
        tabs = [c for c in data["controls"] if c["kind"] == "button" and c["section"] == "" and c["r"][1] < 120 and c["r"][1] > 60]
        disp = pg.evaluate("(()=>{const r=document.getElementById('disp').getBoundingClientRect();return [r.x,r.y,r.width,r.height]})()")
        for t in tabs:
            pg.mouse.click(t["r"][0] + t["r"][2] / 2, t["r"][1] + t["r"][3] / 2); pg.wait_for_timeout(1800)
            key = re.sub(r"\W+", "_", t["label"].strip().lower()).strip("_")
            pg.screenshot(path=os.path.join(OUT, "view_%s.png" % key), clip={"x": disp[0], "y": disp[1], "width": disp[2], "height": disp[3]})
            views.append({"key": key, "label": t["label"], "tip": t["tip"]})
        texts = {}
        for name in ("Guide", "About & Support", "Settings"):
            bt = [c for c in data["controls"] if c["label"] == name]
            if not bt: continue
            pg.mouse.click(bt[0]["r"][0] + 10, bt[0]["r"][1] + 10); pg.wait_for_timeout(400)
            texts[name] = pg.evaluate("""() => { const hs = [...document.querySelectorAll('h2')].filter(h => h.getBoundingClientRect().width > 0);
              const h = hs[0]; if (!h) return ''; return h.parentElement.innerText; }""")
            pg.keyboard.press("Escape"); pg.wait_for_timeout(200)
        data["dialogs"] = texts
        first = tabs[0]; pg.mouse.click(first["r"][0] + 10, first["r"][1] + 10); pg.wait_for_timeout(400)
        # start the test voice so the displays are alive (the same as pressing Play), wait, then shoot
        pg.mouse.move(5, 5); pg.wait_for_timeout(2500)
        pg.evaluate("document.querySelector('.tip').hidden = true; window.scrollTo(0,0)")
        pg.screenshot(path=os.path.join(OUT, "_raw.png"))
        pg.screenshot(path=os.path.join(root, "docs", "screenshot.png"))
        data["views"] = views; data["disp"] = disp; data["errors"] = errs
        b.close()
    DISPLAY_CLS = {"vu": "VU meter", "grm": "Gain reduction meter", "mcv": "Input and output level meters"}
    DISPLAY_LABELS = {"Voltage change": "Voltage change", "Loudness readout": "Loudness readout", "Window 100 %": "Window size read-out", "Preview rig": "Preview rig label", "Gold Cat": "Tube name and description"}
    pan = data["sections"]; pan.sort(key=lambda s: s["r"][0]); ids = [s["id"] for s in pan]
    ctl, disps = [], []
    for c in data["controls"]:
        if c["label"].strip() == "Resize": continue          # window resize handles: not a control of the plugin
        cls = c["cls"].split()[0] if c["cls"] else ""
        if cls in DISPLAY_CLS: c["label"] = DISPLAY_CLS[cls]; c["kind"] = "display"; disps.append(c)
        elif c["label"] in DISPLAY_LABELS: c["label"] = DISPLAY_LABELS[c["label"]]; c["kind"] = "display"; disps.append(c)
        else:
            if c["kind"] == "display": c["kind"] = "switch"      # segmented switches and selectors built from divs
            y = c["r"][1]
            c["panel"] = c["section"] if c["section"] in ids else ("header" if y < 70 else "views" if y < 140 else "rig")
            ctl.append(c)
    order = {i: n for n, i in enumerate(ids + ["header", "views", "rig"])}
    ctl.sort(key=lambda c: (order[c["panel"]], round(c["r"][1] / 30), c["r"][0]))
    for n, c in enumerate(ctl, 1): c["n"] = n
    disps.sort(key=lambda c: (round(c["r"][1] / 40), c["r"][0]))
    for i, d in enumerate(disps): d["letter"] = chr(65 + i)
    data["controls"] = ctl + disps
    data["groups"] = [{"id": s["id"], "title": s["title"], "sub": s["sub"]} for s in pan] + [{"id": "header", "title": "Header: presets and windows", "sub": ""}, {"id": "views", "title": "Display views", "sub": ""}, {"id": "rig", "title": "Preview rig", "sub": "not part of the plugin"}]
    json.dump(data, open(os.path.join(OUT, "tour.json"), "w"), indent=1)
    # draw badges
    im = Image.open(os.path.join(OUT, "_raw.png")).convert("RGB")
    d = ImageDraw.Draw(im)
    try: f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 14)
    except Exception: f = ImageFont.load_default()
    def badge(cx, cy, t, fill):
        d.ellipse([cx - 11, cy - 11, cx + 11, cy + 11], fill=fill, outline=(22, 22, 56))
        w = d.textlength(t, font=f); d.text((cx - w / 2, cy - 9), t, fill=(22, 22, 56), font=f)
    for c in ctl:
        x, y, w, h = c["r"]; badge(x + 8, y + 8, str(c["n"]), (255, 123, 177))
    for c in disps:
        x, y, w, h = c["r"]; badge(x + w - 14, y + 14, c["letter"], (2, 235, 186))
    im.save(os.path.join(OUT, "window.png"), optimize=True)
    for s in pan:
        x, y, w, h = s["r"]; im.crop((int(x) - 4, int(y) - 4, int(x + w) + 4, int(y + h) + 4)).save(os.path.join(OUT, "panel_%s.png" % s["id"]), optimize=True)
    os.remove(os.path.join(OUT, "_raw.png"))
    print("controls", len(ctl), "displays", len(disps), "views", [v["key"] for v in views], "errors", errs)

main()
