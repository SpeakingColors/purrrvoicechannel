#!/usr/bin/env python3
"""Builds docs/Purrr_Voice_Channel_manual.pdf from docs/manual.txt and docs/tour/tour.json (made by tools/shoot_tour.py).
Usage: python3 tools/make_manual.py [out.pdf]     Needs: pip install reportlab pillow"""
import io, json, os, sys
from PIL import Image
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak, KeepTogether, CondPageBreak, Image as RLImage

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "docs", "Purrr_Voice_Channel_manual.pdf")
T = json.load(open(os.path.join(root, "docs/tour/tour.json")))
win = Image.open(os.path.join(root, "docs/tour/window.png")).convert("RGB")
sec = {}; cur = None
for line in open(os.path.join(root, "docs/manual.txt"), encoding="utf-8").read().splitlines():
    if line.startswith("# "): cur = line[2:].strip(); sec[cur] = []
    elif cur: sec[cur].append(line)
paras = lambda k: [p.strip() for p in "\n".join(sec.get(k, [])).split("\n\n") if p.strip()]
bullets = lambda k: [l[2:].strip() for l in sec.get(k, []) if l.startswith("- ")]

BG, PANEL = colors.HexColor("#15161a"), colors.HexColor("#22242b")
TEAL, PINK, INK, DIM = colors.HexColor("#02ebba"), colors.HexColor("#ff7bb1"), colors.HexColor("#e8e8ee"), colors.HexColor("#a9adb8")
st = lambda n, **kw: ParagraphStyle(n, fontName=kw.pop("font", "Helvetica"), **kw)
H1 = st("h1", fontSize=26, leading=30, textColor=TEAL, font="Helvetica-Bold")
H2 = st("h2", fontSize=14, leading=18, textColor=TEAL, font="Helvetica-Bold", spaceBefore=10, spaceAfter=4)
B = st("b", fontSize=10, leading=14, textColor=INK); SM = st("s", fontSize=8.5, leading=11.5, textColor=DIM)
CELL = st("c", fontSize=8, leading=10, textColor=INK); CELLB = st("cb", fontSize=8.5, leading=11, textColor=PINK, font="Helvetica-Bold")
CELLT = st("ct", fontSize=8.5, leading=11, textColor=TEAL, font="Helvetica-Bold")
esc = lambda t: t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

def img(im, wmm=170, maxh=150):
    b = io.BytesIO(); im.save(b, "PNG"); b.seek(0); w, h = im.size
    s = min(wmm * mm / w, maxh * mm / h); return RLImage(b, width=w * s, height=h * s)
def crop_rect(r, pad=8):
    return win.crop((max(0, int(r[0] - pad)), max(0, int(r[1] - pad)), min(win.size[0], int(r[0] + r[2] + pad)), min(win.size[1], int(r[1] + r[3] + pad))))
def table(rows, widths):
    t = Table(rows, colWidths=[w * mm for w in widths])
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BACKGROUND", (0, 0), (-1, -1), PANEL), ("LINEBELOW", (0, 0), (-1, -1), 0.5, BG), ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4), ("LEFTPADDING", (0, 0), (-1, -1), 5)]))
    return t

ctl = [c for c in T["controls"] if "n" in c]; disp = [c for c in T["controls"] if "letter" in c]
story = [Paragraph("Purrr Voice Channel", H1), Paragraph("Manual . Studio 87 . free and open source (AGPL-3.0-or-later) . github.com/SpeakingColors/purrrvoicechannel", SM), Spacer(1, 6)]
for k in ("Intro", "Status"):
    story.append(Paragraph(k if k != "Intro" else "What it is", H2))
    for p in paras(k): story += [Paragraph(esc(p), B), Spacer(1, 4)]
if bullets("Install"):
    story.append(Paragraph("Install and remove", H2))
    for t in bullets("Install"): story.append(Paragraph("&bull; " + esc(t), B))
story.append(Paragraph("Quick guide (from the Guide button)", H2))
for line in T["dialogs"]["Guide"].splitlines()[1:]:
    line = line.strip()
    if not line or line == "CLOSE": continue
    story.append(Paragraph(esc(line), st("g", fontSize=9, leading=12, textColor=TEAL, font="Helvetica-Bold", spaceBefore=5) if line.isupper() else B))
story.append(PageBreak())
story += [Paragraph("Interface tour", H1), Paragraph("Pink numbers mark the controls and match the tables on the next pages; teal letters mark displays and meters. The picture is the design mockup with the test voice playing, not a plugin window.", SM), Spacer(1, 4), img(win)]
story.append(Paragraph("Signal flow", H2))
sig = [l.strip() for l in T["dialogs"]["Guide"].splitlines() if l.strip().startswith("Input, preamp")]
story.append(Paragraph(esc(sig[0] if sig else ""), B))
story.append(Paragraph("Displays and meters", H2))
story.append(table([[Paragraph(d["letter"], CELLT), Paragraph(esc(d["label"]), CELLB), Paragraph(esc(d["tip"]), CELL)] for d in disp], [8, 42, 120]))
def ctl_rows(items):
    rows = []
    for c in items:
        if c["kind"] == "knob": rg = "%s to %s, default %s" % (c["min"], c["max"], c["def"])
        elif c.get("options"): rg = "choices: " + ", ".join(c["options"])
        else: rg = {"button": "button", "switch": "switch", "menu": "menu"}.get(c["kind"], c["kind"])
        rows.append([Paragraph(str(c["n"]), CELLB), Paragraph(esc(c["label"] or "(unnamed)"), CELLB), Paragraph(esc(rg), CELL), Paragraph(esc(c["tip"]), CELL)])
    return rows
for g in T["groups"]:
    items = [c for c in ctl if c["panel"] == g["id"]]
    if not items: continue
    xs = [c["r"] for c in items]
    sr = next((s["r"] for s in T["sections"] if s["id"] == g["id"]), None)
    if sr is None: sr = [min(r[0] for r in xs), min(r[1] for r in xs), max(r[0] + r[2] for r in xs) - min(r[0] for r in xs), max(r[1] + r[3] for r in xs) - min(r[1] for r in xs)]
    title = g["title"] + ((" (" + g["sub"] + ")") if g["sub"] else "")
    story += [CondPageBreak(100 * mm), Paragraph(esc(title), H2), img(crop_rect(sr), 170, 62), Spacer(1, 4), table(ctl_rows(items), [8, 24, 38, 100])]
story += [PageBreak(), Paragraph("Display views", H1), Paragraph("The tabs under the header switch the big display. Each picture is taken without the badges.", SM)]
for v in T["views"]:
    story.append(KeepTogether([Paragraph(esc(v["label"]), H2), img(Image.open(os.path.join(root, "docs/tour/view_%s.png" % v["key"])).convert("RGB"), 170, 62), Paragraph(esc(v["tip"]), B)]))
story.append(CondPageBreak(70 * mm)); story.append(Paragraph("Under the hood", H2))
for t in bullets("Under the hood"): story.append(Paragraph("&bull; " + esc(t), B))
for name in ("Settings", "About & Support"):
    txt = [l.strip() for l in T["dialogs"][name].splitlines()[1:] if l.strip() and l.strip() not in ("CLOSE",)]
    story.append(Paragraph(esc(name), H2)); story.append(Paragraph(esc(" / ".join(txt)), SM))
def page(c, d):
    c.saveState(); c.setFillColor(BG); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(TEAL); c.rect(0, A4[1] - 3 * mm, A4[0], 3 * mm, stroke=0, fill=1)
    c.setFont("Helvetica", 8); c.setFillColor(DIM); c.drawString(20 * mm, 10 * mm, "Purrr Voice Channel manual (design mockup)"); c.drawRightString(190 * mm, 10 * mm, "page %d" % d.page); c.restoreState()
SimpleDocTemplate(out, pagesize=A4, leftMargin=20 * mm, rightMargin=20 * mm, topMargin=14 * mm, bottomMargin=18 * mm, title="Purrr Voice Channel manual", author="Studio 87").build(story, onFirstPage=page, onLaterPages=page)
print("wrote", out)
