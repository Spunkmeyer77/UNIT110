#!/usr/bin/env python3
# Builds the UI images in Assets/ from the design renders (1.0.5: the clean "pristine" renders).
#   python3 tools/make_assets.py <front.png> <interior.png> <knob.png>
# front    = front panel, 3072 x 2048, no text, plain keys, grey knob circles, empty LCD
# interior = the open case, 3072 x 2048, no board text, bare gold pads where the trimmers go
# knob     = one knob seen from above, pointer at 12 o'clock, on white
# Both views come out 3004 x 1844 (2 x the 1502 x 922 design) with the case exactly where the panel is, both edges level.
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

K = 2                                  # assets are stored at 2x the design size (sharp on Retina / large windows)
UP = 1.25 * K * 1536 / 3072            # the 3072 renders scaled to K x the 1920 layout everything was measured on
CROP = tuple(v * K for v in (195, 233, 1697, 1155))   # in that frame
PANEL_BOX = tuple(v * K for v in (205, 257, 1684, 1126))   # the panel's outline there (top / mean bottom)
CASE_BOX = None                        # measured from the interior render
TOP, BOTTOM = 24.0 * K, 893.0 * K      # level edges after straightening



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


def edge_fit(img):
    a = np.asarray(img.convert("RGB")).astype(int).sum(-1) > 90
    xs = np.arange(60 * K, a.shape[1] - 60 * K, 10)
    top = [np.where(a[:, x])[0].min() for x in xs]; bot = [np.where(a[:, x])[0].max() for x in xs]
    pt, pb = np.polyfit(xs, top, 1), np.polyfit(xs, bot, 1)
    return (pt[0], pt[1]), (pb[0], pb[1])


def outline(img):
    a = np.asarray(img.convert("RGB")).astype(int).sum(-1) > 60
    ys, xs = np.where(a)
    return xs.min(), ys.min(), xs.max(), ys.max()


def up(path):
    im = Image.open(path).convert("RGB")
    return im.resize((round(im.width * UP), round(im.height * UP)), Image.LANCZOS)


# ---- front panel
f = up(front)
fc = f.crop(CROP)
PANEL_FIT = edge_fit(fc)
print("panel edges", PANEL_FIT)
straighten(fc, PANEL_FIT).save(os.path.join(OUT, "panel.png"), optimize=True)

# ---- interior: the case stretched onto the panel's outline (it comes out level already)
im = up(interior)
cb, pb = outline(im), PANEL_BOX
sx = (pb[2] - pb[0]) / (cb[2] - cb[0]); sy = (pb[3] - pb[1]) / (cb[3] - cb[1])
big = im.resize((round(im.width * sx), round(im.height * sy)), Image.LANCZOS)
canvas = Image.new("RGB", f.size, (0, 0, 0))
canvas.paste(big, (round(pb[0] - cb[0] * sx), round(pb[1] - cb[1] * sy)))
board = canvas.crop(CROP)
board = straighten(board, edge_fit(board))

# board print: group names over the boxes, setting names under them, pads + names for the extra trimmers
d = ImageDraw.Draw(board)
ink = (240, 243, 236)
fh, fl, fs = ImageFont.truetype(FONT_B, 18 * K), ImageFont.truetype(FONT_BC, 11 * K), ImageFont.truetype(FONT_BC, 10 * K)
X0 = [114 + 195.7 * g for g in range(7)]   # first trimmer pad of each group (design px); 50 px apart; y = 802
D = lambda x, y: (x * K, y * K)
names = ["ACC", "BD", "SD", "OH", "CH", "CY", "CP"]
for g, x0 in enumerate(X0):
    d.text(D (x0 + 50, 754), names[g], font=fh, fill=ink, anchor="mm")
    for i, t in enumerate(["DRIVE", "DECAY", "LEVEL"] if g == 0 else ["TUNE", "DECAY", "LEVEL"]):
        d.text(D (x0 + 50 * i, 827), t, font=fl, fill=ink, anchor="mm")
for g, t in [(1, "ATTACK"), (2, "SNAPPY"), (5, "TONE"), (6, "TAIL")]:
    x = X0[g] + 112
    d.ellipse((*D (x - 9, 754 - 9), *D (x + 9, 754 + 9)), fill=(226, 180, 92), outline=(170, 128, 60), width=K)
    d.text(D (x, 736), t, font=fs, fill=ink, anchor="mm")
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
Image.fromarray(ka, "RGBA").resize((400, 400), Image.LANCZOS).save(os.path.join(OUT, "knob.png"), optimize=True)
print("assets written to", OUT)
