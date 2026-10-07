# HEIC tests for gimp-raw-heic (run inside gimp-console, see test_load.py).
import os
from gi.repository import Gimp, Gio

d = os.path.join("samples", "heic")
pdb = Gimp.get_pdb()
lines = []
EXPECT = [0.18, 1.0, 4.0]


def info(img):
    prof = img.get_color_profile() or img.get_effective_color_profile()
    lay = img.get_layers()[0]
    return "%dx%d %s %s alpha=%s" % (img.get_width(), img.get_height(),
                                    str(img.get_precision()).split(".")[-1].split(" ")[0],
                                    prof.get_label(), lay.has_alpha())


def patches(img):
    lay = img.get_layers()[0]
    w, h = img.get_width(), img.get_height()
    out = []
    for i in range(3):
        c = lay.get_pixel(int((i + 0.5) * w / 3), h // 4)
        r, g, b, a = c.get_rgba()
        out.append(g)
    return out


def load_with(fn, mode):
    proc = pdb.lookup_procedure("file-raw-heic-heif-load")
    cfg = proc.create_config()
    cfg.set_property("run-mode", Gimp.RunMode.NONINTERACTIVE)
    cfg.set_property("file", Gio.File.new_for_path(os.path.abspath(os.path.join(d, fn))))
    cfg.set_property("hdr-mode", mode)
    r = proc.run(cfg)
    return r.index(1) if r.index(0) == Gimp.PDBStatusType.SUCCESS else None


for fn in sorted(os.listdir(d)):
    path = os.path.abspath(os.path.join(d, fn))
    img = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(path))
    if not img:
        lines.append("FAIL %s" % fn)
        continue
    line = "OK   %-12s %s" % (fn, info(img))
    if fn in ("pq.heic", "hlg.heic"):
        p = patches(img)
        line += "  patches=%s expected=%s" % (["%.3f" % v for v in p], EXPECT)
    lines.append(line)
    img.delete()

for mode in ("sdr", "builtin"):
    img = load_with("pq.heic", mode)
    if img:
        lines.append("MODE %-8s pq.heic -> %s" % (mode, info(img)))
        img.delete()
    else:
        lines.append("MODE %-8s FAIL" % mode)

thumb = pdb.lookup_procedure("file-raw-heic-heif-load-thumb")
for fn in sorted(os.listdir(d)):
    cfg = thumb.create_config()
    cfg.set_property("file", Gio.File.new_for_path(os.path.abspath(os.path.join(d, fn))))
    cfg.set_property("thumb-size", 128)
    r = thumb.run(cfg)
    if r.index(0) == Gimp.PDBStatusType.SUCCESS:
        lines.append("THUMB %-12s %dx%d (full %dx%d)" % (fn, r.index(1).get_width(),
                     r.index(1).get_height(), r.index(2), r.index(3)))
        r.index(1).delete()
    else:
        lines.append("THUMB %-12s FAIL" % fn)

with open(os.path.join("build", "test-out", "heic.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
print("\n".join(lines))
