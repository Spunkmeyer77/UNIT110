#!/usr/bin/env python3
# Builds the UI images in Assets/ from the design renders (1.0.5: the clean "pristine" renders).
#   python3 tools/make_assets.py <front.png> <interior.png> <knob.png>
# front    = front panel, 1536 x 1024, no text, plain keys, grey knob circles, empty LCD
# interior = the open case, 1536 x 1024, no board text, bare gold pads where the trimmers go
# knob     = one knob seen from above, pointer at 12 o'clock, on white
# Both views come out 1502 x 922 with the case exactly where the panel is, both edges level.
# Needs: pip install pillow numpy opencv-python
import os, sys
import numpy as np
import cv2
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "Assets")
os.makedirs(OUT, exist_ok=True)
front, interior, knob = sys.argv[1:4]
FONT_B = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONT_BC = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"

UP = 1.25                              # the 1536 renders are scaled to the 1920 layout everything was measured on
CROP = (195, 233, 1697, 1155)          # in that 1920 x 1280 frame
PANEL_BOX = (205, 257, 1684, 1126)     # the panel's outline there (top / mean bottom)
CASE_BOX = (203, 257, 1709, 1129)      # the case's outline in the scaled interior
TOP, BOTTOM = 24.0, 893.0              # level edges after straightening (design pixels)
PANEL_FIT = ((0.000963038, 24.21007), (0.00878241, 886.3991))   # top / bottom edge of the cropped panel: (slope, offset)


def straighten(img, fit):
    a = np.asarray(img.convert("RGB"))
    h, w = a.shape[:2]
    (st, ot), (sb, ob) = fit
    xs = np.arange(w, dtype=np.float32)
    top = ot + st * xs; bot = ob + sb * xs
    ys = np.arange(h, dtype=np.float32)[:, None]
    mapy = (top[None, :] + (ys - TOP) * (bot - top)[None, :] / (BOTTOM - TOP)).astype(np.float32)
    mapx = np.repeat(xs[None, :], h, axis=0)
    return Image.fromarray(cv2.remap(a, mapx, mapy, cv2.INTER_LANCZOS4, borderMode=cv2.BORDER_CONSTANT, borderValue=0))


def up(path):
    im = Image.open(path).convert("RGB")
    return im.resize((round(im.width * UP), round(im.height * UP)), Image.LANCZOS)


# ---- front panel
f = up(front)
straighten(f.crop(CROP), PANEL_FIT).save(os.path.join(OUT, "panel.png"), optimize=True)

# ---- interior: the case stretched onto the panel's outline (it comes out level already)
im = up(interior)
cb, pb = CASE_BOX, PANEL_BOX
sx = (pb[2] - pb[0]) / (cb[2] - cb[0]); sy = (pb[3] - pb[1]) / (cb[3] - cb[1])
big = im.resize((round(im.width * sx), round(im.height * sy)), Image.LANCZOS)
canvas = Image.new("RGB", f.size, (0, 0, 0))
canvas.paste(big, (round(pb[0] - cb[0] * sx), round(pb[1] - cb[1] * sy)))
board = canvas.crop(CROP)

# board print: group names over the boxes, setting names under them, pads + names for the extra trimmers
d = ImageDraw.Draw(board)
ink = (240, 243, 236)
fh, fl, fs = ImageFont.truetype(FONT_B, 18), ImageFont.truetype(FONT_BC, 11), ImageFont.truetype(FONT_BC, 10)
X0 = [114 + 195.7 * g for g in range(7)]   # first trimmer pad of each group; 50 px apart; y = 802
names = ["ACC", "BD", "SD", "OH", "CH", "CY", "CP"]
for g, x0 in enumerate(X0):
    d.text((x0 + 50, 754), names[g], font=fh, fill=ink, anchor="mm")
    for i, t in enumerate(["DRIVE", "DECAY", "LEVEL"] if g == 0 else ["TUNE", "DECAY", "LEVEL"]):
        d.text((x0 + 50 * i, 827), t, font=fl, fill=ink, anchor="mm")
for g, t in [(1, "ATTACK"), (2, "SNAPPY"), (5, "TONE"), (6, "TAIL")]:
    x = X0[g] + 112
    d.ellipse((x - 9, 754 - 9, x + 9, 754 + 9), fill=(226, 180, 92), outline=(170, 128, 60))
    d.text((x, 736), t, font=fs, fill=ink, anchor="mm")
board.save(os.path.join(OUT, "pcb.png"), optimize=True)

# ---- knob: cut the round cap out of the white background
k = Image.open(knob).convert("RGB")
a = np.asarray(k).astype(int)
ys, xs = np.where(a.sum(-1) < 600)
cx, cy = (xs.min() + xs.max()) / 2.0, (ys.min() + ys.max()) / 2.0
r = (xs.max() - xs.min() + ys.max() - ys.min()) / 4.0
k = k.crop((int(cx - r - 4), int(cy - r - 4), int(cx + r + 4), int(cy + r + 4)))
n = k.size[0]
yy, xx = np.mgrid[0:n, 0:n]
alpha = np.clip((r - np.hypot(xx - n / 2 + 0.5, yy - n / 2 + 0.5)) * 1.5 + 1, 0, 1)
ka = np.dstack([np.asarray(k)[:n, :n], (alpha[:k.size[1], :k.size[0]] * 255).astype(np.uint8)])
Image.fromarray(ka, "RGBA").resize((300, 300), Image.LANCZOS).save(os.path.join(OUT, "knob.png"), optimize=True)
print("assets written to", OUT)
