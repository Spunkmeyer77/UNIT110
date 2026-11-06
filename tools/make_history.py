#!/usr/bin/env python3
# Builds docs/UNIT110-History.pdf, the update history (needs: pip install reportlab).
#   python3 tools/make_history.py
# Add new versions at the top of HISTORY. No dates.
import os, re
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.lib.units import mm
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, KeepTogether

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = re.search(r"project\(UNIT110 VERSION ([0-9.]+)\)", open(os.path.join(ROOT, "CMakeLists.txt")).read()).group(1)
OUT = os.path.join(ROOT, "docs", "UNIT110-History.pdf")

HISTORY = [
    ("1.0.3", [
        "Sharp panel: all lettering, numbers, key legends and icons are now drawn as vector graphics, crisp at any window size.",
        "Spelling: BASS DRUM, DC.",
        "The TEMPO knob is now SWING (STRAIGHT to SHUFFLE, 50-75 %). MIDI export swings too.",
        "UNIT 110 always follows the DAW tempo; START / STOP with the DAW stopped also use it. In the standalone app the tempo is set by "
        "dragging the TEMPO box on the LCD.",
        "Mute with SHIFT + drum key, solo with Option (Alt) + drum key; the LCD shows M / S.",
        "The drum keys' right-click menu now holds only Own output. Right-click menus are larger.",
        "Checked: runs on macOS 10.13 and later (11 on Apple silicon), including macOS 12.",
    ]),
    ("1.0.2", [
        "New parameter-lock screen: double-click SHIFT and the LCD shows one drum's locks for one setting (tune, decay, level or the "
        "drum's extra) as a bar per step. Drag to set, double-click to clear. Double-click SHIFT again to go back.",
        "Copy pattern / Paste pattern in the LCD's right-click menu (steps and locks).",
        "New front panel and circuit board artwork, now exactly the same size: opening the case looks like lifting the lid.",
        "The ON-OFF POWER lettering also opens the case; inside, the three big black chips close it, as well as the screws.",
        "Bigger case screws on the front panel.",
    ]),
    ("1.0.1", [
        "Fixed: the plug-in and app did not open on macOS 12 (and other versions older than the build machine). They now run on macOS 10.13 and later.",
    ]),
    ("1.0.0", [
        "First release of UNIT 110 by Transistor Labs.",
        "Hyperrealistic front panel: photographed keys that sink when pressed, turning knobs, a working POWER switch and a live LCD.",
        "The LCD shows all 7 rows (accent + 6 drums) on a 16-step grid, the playing step, the mode, the pattern and the tempo. "
        "Click steps to edit them; right-click for MIDI export, copy and clear.",
        "Panel modes like the original: SHIFT + 1..5 = PLAY SONG, PLAY PATTERN, WRITE SONG, WRITE STEP, WRITE TAP; "
        "SHIFT + 6 = 12/16 steps, SHIFT + 7 = clear pattern, SHIFT + 8 = clear song.",
        "32 patterns in 4 banks (A-D) of 8, chosen with the A|B|C|D key and the number keys.",
        "START / STOP run the sequencer on the TEMPO knob while the host is stopped.",
        "New BALANCE knob: drums (bass drum, snare, clap) against metal (hats, cymbal).",
        "Open the case with a screw in a bottom corner: the circuit board has trimmers for Tune, Decay and Level of every drum, "
        "Attack, Snappy, Tone and Tail, and the accent's Drive, Decay and Level.",
        "Sound engine from TRoss 110 0.3.3.",
    ]),
]

INK = colors.HexColor("#1e1f21")
BLUE = colors.HexColor("#3f7f9e")
GREY = colors.HexColor("#6b6e6a")
title = ParagraphStyle("t", fontName="Helvetica-Bold", fontSize=24, leading=29, textColor=INK, spaceAfter=4)
sub = ParagraphStyle("s", fontName="Helvetica", fontSize=10.5, leading=14, textColor=GREY, spaceAfter=14)
ver = ParagraphStyle("v", fontName="Helvetica-Bold", fontSize=13, leading=17, textColor=BLUE, spaceBefore=10, spaceAfter=4)
item = ParagraphStyle("i", fontName="Helvetica", fontSize=9.8, leading=13.6, textColor=INK, leftIndent=12, bulletIndent=2, spaceAfter=3)


def footer(c, doc):
    c.saveState()
    c.setFont("Helvetica", 8)
    c.setFillColor(GREY)
    c.drawString(18 * mm, 10 * mm, "UNIT 110 update history  ·  Transistor Labs  ·  ticcoross@gmail.com")
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()


s = [Paragraph('UNIT <font color="#3f7f9e">110</font>  update history', title),
     Paragraph(f"Current version {VERSION}. Newest first.", sub)]
for v, items in HISTORY:
    s.append(KeepTogether([Paragraph(f"Version {v}", ver)] + [Paragraph(t, item, bulletText="•") for t in items]))
    s.append(Spacer(1, 4))
SimpleDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=18 * mm, bottomMargin=20 * mm,
                  title=f"UNIT 110 Update History {VERSION}", author="Transistor Labs").build(s, onFirstPage=footer, onLaterPages=footer)
print("written", OUT)
