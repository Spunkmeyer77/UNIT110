#!/usr/bin/env python3
# Builds docs/UNIT110-Manual.pdf (needs: pip install reportlab).
#   python3 tools/make_manual.py
# Pictures: docs/panel.png and docs/pcb.png (editor snapshots written by the plug-in test).
import os, re
from reportlab.lib.pagesizes import A4
from reportlab.lib import colors
from reportlab.lib.units import mm
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Image, PageBreak, KeepTogether)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = re.search(r"project\(UNIT110 VERSION ([0-9.]+)\)", open(os.path.join(ROOT, "CMakeLists.txt")).read()).group(1)
OUT = os.path.join(ROOT, "docs", "UNIT110-Manual.pdf")

INK = colors.HexColor("#1e1f21")
BLUE = colors.HexColor("#3f7f9e")
GREY = colors.HexColor("#6b6e6a")
LIGHT = colors.HexColor("#eef1ea")

body = ParagraphStyle("body", fontName="Helvetica", fontSize=9.6, leading=13.6, textColor=INK, spaceAfter=6)
small = ParagraphStyle("small", parent=body, fontSize=8.6, leading=11.6, spaceAfter=0)
h1 = ParagraphStyle("h1", fontName="Helvetica-Bold", fontSize=15, leading=19, textColor=INK, spaceBefore=10, spaceAfter=7, keepWithNext=1)
h2 = ParagraphStyle("h2", fontName="Helvetica-Bold", fontSize=11, leading=14, textColor=BLUE, spaceBefore=9, spaceAfter=4, keepWithNext=1)
title = ParagraphStyle("title", fontName="Helvetica-Bold", fontSize=34, leading=38, textColor=INK)
sub = ParagraphStyle("sub", fontName="Helvetica", fontSize=11, leading=15, textColor=GREY, spaceAfter=14)
cell = ParagraphStyle("cell", parent=small)
cellb = ParagraphStyle("cellb", parent=small, fontName="Helvetica-Bold")


def table(rows, widths):
    data = [[Paragraph(str(c), cellb if i == 0 or j == 0 else cell) for j, c in enumerate(r)] for i, r in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), LIGHT),
        ("LINEBELOW", (0, 0), (-1, 0), 0.8, BLUE),
        ("LINEBELOW", (0, 1), (-1, -1), 0.3, colors.HexColor("#cfd3cb")),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
        ("LEFTPADDING", (0, 0), (-1, -1), 5), ("RIGHTPADDING", (0, 0), (-1, -1), 5),
    ]))
    return t


def P(t): return Paragraph(t, body)


def footer(canv, doc):
    canv.saveState()
    canv.setFont("Helvetica", 8)
    canv.setFillColor(GREY)
    canv.drawString(18 * mm, 10 * mm, f"UNIT 110 {VERSION}  ·  Transistor Labs")
    canv.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    canv.setStrokeColor(BLUE); canv.setLineWidth(0.6)
    canv.line(18 * mm, 13.5 * mm, A4[0] - 18 * mm, 13.5 * mm)
    canv.restoreState()


W = A4[0] - 36 * mm
s = []
s += [Spacer(1, 14 * mm), Paragraph('UNIT <font color="#3f7f9e">110</font>', title),
      Paragraph(f"Transistor Labs rhythm composer  ·  Owner's manual  ·  version {VERSION}", sub)]
img = os.path.join(ROOT, "docs", "panel.png")
if os.path.exists(img):
    s += [Image(img, width=W, height=W * 853 / 1455), Spacer(1, 8)]
s += [P("UNIT 110 is a drum machine you play like the hardware: press its keys, turn its knobs, read its LCD, and open the "
        "case to reach the trimmers inside. Six analog-style voices - bass drum, snare, hand clap, closed hat, open hat "
        "and cymbal - a global accent, and a 16-step sequencer with 32 patterns and a song mode. It runs as an Audio Unit, "
        "VST3 or LV2 plug-in and as a standalone app on macOS, Windows and Linux.")]

s += [PageBreak(), Paragraph("1  System requirements", h1)]
s += [table([
    ["System", "Needs"],
    ["macOS", "macOS 10.13 or later; Apple silicon or Intel (universal build). A host for AU or VST3 (Ableton Live, Logic Pro, "
              "GarageBand, Bitwig, Reaper...), or the standalone app."],
    ["Windows", "Windows 10 or 11, 64-bit. A VST3 host, or the standalone app."],
    ["Linux", "x86_64 with X11, FreeType and ALSA or JACK. A VST3 or LV2 host (Bitwig, Reaper, Ardour...), or the standalone app."],
    ["Screen", "The window opens at 1091 x 640 and can be resized from 50 % to 120 %."],
    ["CPU / memory", "Light: one instance uses a few percent of one core; about 30 MB of memory."],
], [28 * mm, W - 28 * mm])]

s += [Paragraph("2  Installing", h1)]
s += [table([
    ["System", "File", "What it installs"],
    ["macOS", f"UNIT110-{VERSION}-macOS.pkg", "Audio Unit, VST3 and the standalone app. Click Customize to skip any of them."],
    ["Windows", f"UNIT110-{VERSION}-Windows-Setup.exe", "VST3 (Common Files\\VST3) and the standalone app."],
    ["Linux", f"UNIT110-{VERSION}-Linux-x86_64.tar.gz", "Unpack and run ./install.sh: VST3 to ~/.vst3, LV2 to ~/.lv2, app to ~/.local/bin."],
], [22 * mm, 66 * mm, W - 88 * mm])]
s += [Paragraph("2.1  macOS security warning", h2),
      P("The macOS installer is <b>not notarised</b> by Apple, so the first time you open it macOS says it \"cannot be opened "
        "because it is from an unidentified developer\" (or that Apple could not check it for malicious software). To install:"),
      P("1. Double-click the .pkg once and close the warning.<br/>"
        "2. Open <b>System Settings > Privacy &amp; Security</b>, scroll down to the message about UNIT110 and click <b>Open Anyway</b>.<br/>"
        "3. Confirm with your password and run the installer.<br/>"
        "Alternative: right-click (Control-click) the .pkg in Finder, choose <b>Open</b>, then <b>Open</b> again."),
      P("On Windows, SmartScreen may warn about the installer: click <b>More info > Run anyway</b>."),
      P("<b>Ableton Live:</b> Settings > Plug-Ins: turn on \"Use Audio Units v2\" (Mac) and/or VST3, then Rescan. UNIT 110 is under "
        "Plug-Ins > Audio Units (or VST3) > Transistor Labs. Drop it on a MIDI track.")]

s += [Paragraph("3  The front panel", h1),
      P("Everything works with the mouse, just like the real keys. Resize the window from its bottom-right corner. "
        "Click the TRANSISTOR LABS logo for the About window.")]
s += [Paragraph("3.1  Knobs and switch", h2)]
s += [table([
    ["Control", "What it does"],
    ["TEMPO", "40-250 BPM. Sets the speed when UNIT 110 runs on its own (START / STOP). When the host plays, the host tempo rules."],
    ["BALANCE", "Drums against metal. Turn left and the hats and cymbal fade; turn right and the bass drum, snare and clap fade. Centre = both full."],
    ["ACCENT", "How hard accented steps hit: a short swell on the whole machine that also drives it into grit."],
    ["VOLUME", "Main output level (5 = -14 dB, 10 = +6 dB)."],
    ["POWER", "Click the switch on the top edge. Off = the LCD goes dark and UNIT 110 is silent."],
    ["Knobs", "Drag up/down. The value shows while you drag; double-click resets."],
], [26 * mm, W - 26 * mm])]
s += [Paragraph("3.2  Keys", h2)]
s += [table([
    ["Key", "What it does"],
    ["SHIFT", "Click it, then the next key does its upper (printed) function. Holding the computer's Shift key while clicking works too."],
    ["A|B|C|D", "Steps through the 4 banks of 8 patterns (A1-D8). With SHIFT, or in WRITE STEP: I-II, switches between steps 1-8 and 9-16."],
    ["1 - 8", "Selects pattern 1-8 of the current bank. While playing, the new pattern starts at the next step 1 (the LCD shows NEXT)."],
    ["START / STOP", "Runs and stops the sequencer at the TEMPO knob while the host is stopped. When the host plays, UNIT 110 follows it."],
    ["Drum keys", "Play the drum. ACCENT arms an accent for the next drum you hit (the key stays down). Right-click a drum key for Mute, "
                  "Solo and Own output."],
], [26 * mm, W - 26 * mm])]
s += [Paragraph("3.3  SHIFT functions", h2)]
s += [table([
    ["SHIFT +", "Function", "What it does"],
    ["1", "PLAY SONG", "Plays the song (the list of patterns built in WRITE SONG), one pattern per bar, locked to the song position."],
    ["2", "PLAY PATTERN", "Plays the selected pattern over and over (the default)."],
    ["3", "WRITE SONG", "Number keys add patterns of the current bank to the end of the song. O.HIHAT (ENTER) adds the playing pattern; "
                       "C.HIHAT (D.C.) takes back the last measure. Up to 32 measures."],
    ["4", "WRITE STEP", "Press ACCENT or a drum key to choose the row (marked on the LCD), then number keys 1-8 switch its steps on and off. "
                       "A|B|C|D (I-II) switches to steps 9-16."],
    ["5", "WRITE TAP", "Record live: drum keys and MIDI notes played while the sequencer runs go into the nearest step. "
                      "ACCENT before a hit, or velocity above 100, also sets the accent."],
    ["6", "12 / 16", "Pattern length 12 (shuffle, triplets) or 16 steps."],
    ["7", "PATTERN CLEAR", "Clears the selected pattern."],
    ["8", "MEASURE RESET", "Clears the song."],
], [16 * mm, 32 * mm, W - 48 * mm])]

s += [Paragraph("4  The LCD", h1),
      P("The upper part is the pattern: 7 rows (ACCENT, BASS DR, SNARE, CLAP, C.HIHAT, O.HIHAT, CYMBAL) by 16 steps. "
        "The playing step is shown reversed in the number row. Steps beyond the length are greyed out."),
      P("<b>Click</b> a step to switch it on or off, or drag along a row to paint. <b>Right-click</b> the LCD for: export the pattern, all "
        "patterns in a row, every pattern as its own file, or the song as MIDI; copy the pattern to another slot; clear the pattern or the song. "
        "MIDI files use 16th notes on channel 10, accented steps at velocity 127."),
      P("The lower boxes show the MODE (and short messages), the PATTERN (bank letter + number, NEXT for a queued one) and the TEMPO, "
        "the length and whether the host (HOST) or UNIT 110 itself (RUN) is playing.")]

s += [Paragraph("5  Inside the case", h1),
      P("Click either <b>screw in the bottom corners</b> of the front panel to open the unit. The circuit board has a service trimmer for "
        "every sound setting. Drag a trimmer up/down; double-click resets it. Click a corner screw on the board, right-click, or press Esc "
        "to close the case.")]
img = os.path.join(ROOT, "docs", "pcb.png")
if os.path.exists(img):
    s += [Image(img, width=W, height=W * 853 / 1455), Spacer(1, 6)]
s += [table([
    ["Trimmer", "What it does"],
    ["TUNE", "Pitch, ±12 semitones. On the hats and cymbal it moves the whole metallic cluster."],
    ["DECAY", "Length of the sound. Middle is the classic timing; fully left a quarter as long, fully right four times as long. "
              "The bass drum's decay also follows its tuning: tune it up and it gets shorter."],
    ["LEVEL", "The drum's level, -inf to +6 dB."],
    ["ACC: DRIVE", "How much an accent pushes the machine into grit."],
    ["ACC: DECAY", "How long the accent swell lasts."],
    ["ACC: LEVEL", "Accent amount (the same setting as the ACCENT knob on the front)."],
    ["ATTACK (BD)", "The click at the front of the kick."],
    ["SNAPPY (SD)", "Noise snap against the drum body."],
    ["TONE (CY)", "Bright attack shimmer (right) against the darker ringing body (left)."],
    ["TAIL (CP)", "Level of the clap's room tail after the burst of hits."],
], [28 * mm, W - 28 * mm])]

s += [Paragraph("6  MIDI and outputs", h1)]
s += [table([
    ["Drum", "MIDI notes (C1 = 36)"],
    ["Bass drum", "35, 36"], ["Snare", "37, 38, 40"], ["Hand clap", "39"], ["Closed hat", "42, 44"], ["Open hat", "46"],
    ["Cymbal", "49, 51, 52, 53, 55, 57"],
], [40 * mm, W - 40 * mm])]
s += [Spacer(1, 4),
      P("Velocities up to 100 scale the level; above 100 also fires the accent."),
      P("<b>Outputs (plug-in):</b> main stereo plus one mono output per drum. Right-click a drum key and choose <b>Own output</b> to take "
        "that drum out of the main mix and send it to its own output (in Live: make an audio track, Audio From = the UNIT 110 track, "
        "pick the drum's output, Monitor In). The standalone app has only the main output."),
      P("All 32 patterns, the song, the mode and every knob and trimmer are saved with your project.")]

s += [Paragraph("7  Specifications", h1)]
s += [table([
    ["", ""],
    ["Voices", "6: bass drum, snare, hand clap, closed hat, open hat, cymbal; plus global accent"],
    ["Sequencer", "16 steps (12 or 16), 7 rows, 32 patterns (4 banks x 8), song of up to 32 measures, host sync, own clock, step and tap writing"],
    ["Outputs", "Main stereo + 6 mono drum outputs (plug-in)"],
    ["Formats", "AU, VST3 (macOS); VST3 (Windows); VST3, LV2 (Linux); standalone app on all three"],
    ["Sample rates", "Any host rate (44.1-192 kHz)"],
], [30 * mm, W - 30 * mm])]
s += [Spacer(1, 8), P("Store: <b>https://ticcoross.gumroad.com/</b>  ·  Questions and feedback: <b>ticcoross@gmail.com</b>")]

doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=18 * mm, bottomMargin=20 * mm,
                        title=f"UNIT 110 Manual {VERSION}", author="Transistor Labs", subject="UNIT 110 owner's manual")
doc.build(s, onFirstPage=footer, onLaterPages=footer)
print("wrote", OUT)
