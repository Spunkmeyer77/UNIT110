#!/usr/bin/env python3
# Builds the UI images in Assets/ from the design renders.
#   python3 tools/make_assets.py <front.webp/png> <interior.png> <knob.png>
# front    = front panel, 1920 x 1280, knobs removed, LCD empty
# interior = the open case, trimmers removed (any size: its case is stretched onto the panel's outline)
# knob     = one knob seen from above, pointer at 12 o'clock, on white
# Both views come out the same size (1500 x 908) with the case exactly where the panel is.
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

CROP = (196, 238, 1696, 1146)          # in the 1920 x 1280 front image
PANEL_BOX = (206, 252, 1685, 1136)     # the unit's outline in the front image
CASE_BOX = (192, 219, 1387, 906)       # the case's outline in the interior image


def bbox(img):
    a = np.asarray(img.convert("RGB")).astype(int).sum(-1) > 60
    ys, xs = np.where(a)
    return xs.min(), ys.min(), xs.max(), ys.max()


# ---- front panel
f = Image.open(front).convert("RGB")
f.crop(CROP).save(os.path.join(OUT, "panel.png"), optimize=True)

# ---- interior: stretch the case onto the panel outline, same crop
im = Image.open(interior).convert("RGB")
cb = CASE_BOX
sx = (PANEL_BOX[2] - PANEL_BOX[0]) / (cb[2] - cb[0])
sy = (PANEL_BOX[3] - PANEL_BOX[1]) / (cb[3] - cb[1])
big = im.resize((round(im.width * sx), round(im.height * sy)), Image.LANCZOS)
canvas = Image.new("RGB", f.size, (0, 0, 0))
canvas.paste(big, (round(PANEL_BOX[0] - cb[0] * sx), round(PANEL_BOX[1] - cb[1] * sy)))
board = canvas.crop(CROP)

# repaint the board print: group names and TUNE / DECAY / LEVEL
a = np.asarray(board).copy()
H, W = a.shape[:2]
mask = np.zeros((H, W), np.uint8)
lum = a.astype(int).sum(-1) / 3
sat = a.astype(int).max(-1) - a.astype(int).min(-1)
X0 = [113 + 194.8 * g for g in range(7)]   # first trimmer of each group; trimmers 51 px apart, y = 800
for x0 in X0:
    cx = int(x0 + 51)
    y0, y1, xa, xb = 726, 758, cx - 45, cx + 45
    mask[y0:y1, xa:xb] |= ((lum[y0:y1, xa:xb] > 150) & (sat[y0:y1, xa:xb] < 70)).astype(np.uint8)
y0, y1 = 836, 860
band = a[y0:y1].astype(int)
sub = (band[..., 1] - band[..., 0]) < 60
for xa, xb in [(0, 98), (176, 212), (658, 690), (834, 862), (1282, 1332), (1404, W)]:   # grey case clips / walls
    sub[:, xa:xb] = False
mask[y0:y1] |= sub.astype(np.uint8)
mask = cv2.dilate(mask, np.ones((5, 5), np.uint8))
a = cv2.inpaint(a, mask * 255, 5, cv2.INPAINT_TELEA)
board = Image.fromarray(a)
d = ImageDraw.Draw(board)
ink = (236, 240, 232)
fh, fl, fs = ImageFont.truetype(FONT_B, 18), ImageFont.truetype(FONT_BC, 11), ImageFont.truetype(FONT_BC, 10)
names = ["ACC", "BD", "SD", "OH", "CH", "CY", "CP"]
for g, x0 in enumerate(X0):
    d.text((x0 + 51, 741), names[g], font=fh, fill=ink, anchor="mm")
    for i, t in enumerate(["DRIVE", "DECAY", "LEVEL"] if g == 0 else ["TUNE", "DECAY", "LEVEL"]):
        d.text((x0 + 51 * i, 825), t, font=fl, fill=ink, anchor="mm")
# pads + labels for the four extra trimmers (drawn by the plug-in)
for g, t in [(1, "ATTACK"), (2, "SNAPPY"), (5, "TONE"), (6, "TAIL")]:
    x = X0[g] + 112
    d.ellipse((x - 9, 741 - 9, x + 9, 741 + 9), fill=(196, 132, 84), outline=(150, 100, 60))
    d.text((x, 722), t, font=fs, fill=ink, anchor="mm")
board.save(os.path.join(OUT, "pcb.png"), optimize=True)

# ---- knob: cut the round cap out of the white background
k = Image.open(knob).convert("RGB")
cx, cy, r = 800, 822, 444
k = k.crop((cx - r - 4, cy - r - 4, cx + r + 4, cy + r + 4))
n = k.size[0]
yy, xx = np.mgrid[0:n, 0:n]
alpha = np.clip((r - np.hypot(xx - n / 2 + 0.5, yy - n / 2 + 0.5)) * 1.5 + 1, 0, 1)
ka = np.dstack([np.asarray(k), (alpha * 255).astype(np.uint8)])
Image.fromarray(ka, "RGBA").resize((300, 300), Image.LANCZOS).save(os.path.join(OUT, "knob.png"), optimize=True)
print("assets written to", OUT)
