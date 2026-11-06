#!/usr/bin/env python3
# Builds the UI images in Assets/ from the design renders.
#   python3 tools/make_assets.py <front.png> <interior.png> <knob.png>
# front    = front panel with the knobs removed and the LCD empty
# interior = the open case with the trimmers removed
# knob     = one knob seen from above, pointer at 12 o'clock, on white
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

# ---- front panel: crop to the unit (keeping the power switch tab on the top edge)
Image.open(front).convert("RGB").crop((220, 245, 1675, 1098)).save(os.path.join(OUT, "panel.png"), optimize=True)

# ---- interior: crop, remove the old group names and labels, print the new ones
im = Image.open(interior).convert("RGB")
a = np.asarray(im).copy()
H, W = a.shape[:2]
mask = np.zeros((H, W), np.uint8)
lum = a.astype(int).sum(-1) / 3
sat = a.astype(int).max(-1) - a.astype(int).min(-1)
first_pad = [293 + 196 * g for g in range(7)]
for g, x0 in enumerate(first_pad):
    cx = x0 + 52
    # group name above the boxes
    y0, y1, xa, xb = 958, 994, cx - 50, cx + 50
    sub = (lum[y0:y1, xa:xb] > 150) & (sat[y0:y1, xa:xb] < 70)
    mask[y0:y1, xa:xb] |= sub.astype(np.uint8)
# the TUNE / DECAY / LEVEL row under the boxes: everything that isn't board green, except the grey case clips
y0, y1 = 1093, 1121
band = a[y0:y1, :, :].astype(int)
sub = (band[..., 1] - band[..., 0]) < 60
for xa, xb in [(0, 281), (357, 395), (839, 878), (1016, 1049), (1470, 1508), (1588, W)]:
    sub[:, xa:xb] = False
sub[1115 - y0:, 900:1050] = False
mask[y0:y1, :] |= sub.astype(np.uint8)
mask = cv2.dilate(mask, np.ones((5, 5), np.uint8))
a = cv2.inpaint(a, mask * 255, 5, cv2.INPAINT_TELEA)
im = Image.fromarray(a)
d = ImageDraw.Draw(im)
ink = (236, 240, 232)
fh = ImageFont.truetype(FONT_B, 21)
fl = ImageFont.truetype(FONT_BC, 12)
names = ["ACC", "BD", "SD", "OH", "CH", "CY", "CP"]
for g, x0 in enumerate(first_pad):
    d.text((x0 + 52, 976), names[g], font=fh, fill=ink, anchor="mm")
    labels = ["DRIVE", "DECAY", "LEVEL"] if g == 0 else ["TUNE", "DECAY", "LEVEL"]
    for i, t in enumerate(labels):
        d.text((x0 + 52 * i, 1075), t, font=fl, fill=ink, anchor="mm")
# service trimmers for the extra controls (drawn by the plug-in), labelled like board print
fs = ImageFont.truetype(FONT_BC, 11)
for g, t in [(1, "ATTACK"), (2, "SNAPPY"), (5, "TONE"), (6, "TAIL")]:
    x = first_pad[g] + 112
    d.ellipse((x - 11, 966 - 11, x + 11, 966 + 11), fill=(196, 132, 84), outline=(150, 100, 60))
    d.text((x, 943), t, font=fs, fill=ink, anchor="mm")
im.crop((181, 109, 1683, 1174)).save(os.path.join(OUT, "pcb.png"), optimize=True)

# ---- knob: cut the round cap out of the white background
k = Image.open(knob).convert("RGB")
cx, cy, r = 800, 822, 444
k = k.crop((cx - r - 4, cy - r - 4, cx + r + 4, cy + r + 4))
n = k.size[0]
yy, xx = np.mgrid[0:n, 0:n]
dist = np.hypot(xx - n / 2 + 0.5, yy - n / 2 + 0.5)
alpha = np.clip((r - dist) * 1.5 + 1, 0, 1)
ka = np.dstack([np.asarray(k), (alpha * 255).astype(np.uint8)])
Image.fromarray(ka, "RGBA").resize((300, 300), Image.LANCZOS).save(os.path.join(OUT, "knob.png"), optimize=True)
print("assets written to", OUT)
