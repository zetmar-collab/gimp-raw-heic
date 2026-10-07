# Compare gimp-raw-heic output with LibRaw's dcraw_emu reference
# (dcraw_emu -w -4 -T -o 1 -q 3 = camera WB, linear 16-bit sRGB, AHD).
# Run inside gimp-console, see tests/test_load.py for the command line.
import os, struct
from gi.repository import Gimp, Gio, Gegl

ref_dir = os.environ.get("RAWHEIC_REF", os.path.join("build", "ref"))
samples = os.environ.get("RAWHEIC_SAMPLES", "samples")
lines = []

def channel_means(image, fmt):
    layer = image.get_layers()[0]
    buf = layer.get_buffer()
    w, h = buf.get_extent().width, buf.get_extent().height
    scale = 1.0 / 8
    rect = Gegl.Rectangle.new(0, 0, int(w * scale), int(h * scale))
    data = buf.get(rect, scale, fmt, Gegl.AbyssPolicy.CLAMP)
    n = len(data) // 6
    vals = struct.unpack("<%dH" % (n * 3), data[: n * 6])
    return [sum(vals[c::3]) / n for c in range(3)]

for ref in sorted(os.listdir(ref_dir)):
    if not ref.endswith(".tiff"):
        continue
    raw = ref[:-5]
    ours = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,
                          Gio.File.new_for_path(os.path.abspath(os.path.join(samples, raw))))
    theirs = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,
                            Gio.File.new_for_path(os.path.abspath(os.path.join(ref_dir, ref))))
    # read stored code values of both (ours: linear u16, ref: u16 as stored)
    a = channel_means(ours, "RGB u16")
    b = channel_means(theirs, "R'G'B' u16" if "NON_LINEAR" in str(theirs.get_precision()) or "PERCEPTUAL" in str(theirs.get_precision()) else "RGB u16")
    diff = [100.0 * (x - y) / max(y, 1) for x, y in zip(a, b)]
    lines.append("%-24s size %dx%d vs %dx%d  ours %s  ref %s  diff%% %s" % (
        raw, ours.get_width(), ours.get_height(), theirs.get_width(), theirs.get_height(),
        ["%.0f" % v for v in a], ["%.0f" % v for v in b], ["%+.2f" % d for d in diff]))
    ours.delete()
    theirs.delete()

out = os.path.join("build", "test-out", "compare.txt")
with open(out, "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
print("\n".join(lines))
