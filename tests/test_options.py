# Option matrix test (run in gimp-console): every output space, tone curve,
# precision and the processing options must load, carry the right profile
# and keep neutral grey neutral.
import os, struct, time
from gi.repository import Gimp, Gio, Gegl

pdb = Gimp.get_pdb()
src = os.path.abspath(os.path.join("samples", os.environ.get("RAWHEIC_OPT_FILE", "Nikon-D750.nef")))
proc = pdb.lookup_procedure("file-raw-heic-raw-load")
lines = []


def load(**props):
    cfg = proc.create_config()
    cfg.set_property("run-mode", Gimp.RunMode.NONINTERACTIVE)
    cfg.set_property("file", Gio.File.new_for_path(src))
    cfg.set_property("half-size", True)
    for k, v in props.items():
        cfg.set_property(k.replace("_", "-"), v)
    t0 = time.time()
    r = proc.run(cfg)
    return (r.index(1) if r.index(0) == Gimp.PDBStatusType.SUCCESS else None), time.time() - t0


def mean_srgb(img):
    """Mean in linear sRGB, whatever the image space (babl converts)."""
    lay = img.get_layers()[0]
    buf = lay.get_buffer()
    e = buf.get_extent()
    # 1:1 read of a central block (scaled reads of 8-bit buffers return 0)
    w, h = min(512, e.width), min(512, e.height)
    data = buf.get(Gegl.Rectangle.new((e.width - w) // 2, (e.height - h) // 2, w, h), 1.0,
                   "RGB float", Gegl.AbyssPolicy.CLAMP)
    n = len(data) // 12
    v = struct.unpack("<%df" % (n * 3), data[: n * 12])
    g = v[1::3]
    hf = sum(abs(g[i] - g[i + 1]) for i in range(0, n - 1, 3)) / max(1, (n - 1) // 3)
    return [sum(v[c::3]) / n for c in range(3)] + [hf]


def row(label, img, dt, ref=None):
    if not img:
        lines.append("FAIL %-34s" % label)
        return None
    prof = img.get_color_profile() or img.get_effective_color_profile()
    m = mean_srgb(img)
    hf = m.pop()
    ret = list(m)
    dev = " noise=%.5f" % hf
    if ref:
        dev += " max-dev-vs-ref=%.2f%%" % (100 * max(abs(a - b) / max(b, 1e-6) for a, b in zip(m, ref)))
    lines.append("OK   %-34s %5.1fs %-18s %-44s mean(linear sRGB)=%s%s" % (
        label, dt, str(img.get_precision()).split(".")[-1].split(" ")[0],
        prof.get_label(), ["%.4f" % x for x in m], dev))
    img.delete()
    return ret


ref_img, dt = load()
ref = row("default (sRGB linear u16)", ref_img, dt)
ref = ref and ref[:3]

for cs in ("srgb", "adobe", "display-p3", "prophoto", "rec2020"):
    for linear in (True, False):
        img, dt = load(color_space=cs, linear=linear, depth="float")
        row("%s %s float" % (cs, "linear" if linear else "gamma"), img, dt, ref)

for depth in ("u8", "u16", "float"):
    img, dt = load(depth=depth)
    row("depth %s" % depth, img, dt, ref)

cases = [
    ("wb auto", dict(wb_mode="auto")),
    ("wb manual 3200K", dict(wb_mode="manual", wb_temperature=3200.0)),
    ("wb manual 6500K tint+50", dict(wb_mode="manual", wb_temperature=6500.0, wb_tint=50.0)),
    ("wb spot centre", dict(wb_mode="spot")),
    ("exposure +1", dict(exposure=1.0)),
    ("exposure -1", dict(exposure=-1.0)),
    ("highlight unclip", dict(highlight="unclip")),
    ("highlight blend", dict(highlight="blend")),
    ("highlight rebuild 5", dict(highlight="rebuild", rebuild_level=5)),
    ("brightness 2", dict(brightness=2.0)),
    ("auto bright", dict(auto_bright=True)),
    ("denoise 300", dict(denoise=300.0)),
    ("denoise 1000", dict(denoise=1000.0)),
    ("fbdd full", dict(fbdd="full")),
    ("ca correction", dict(ca_red=1.0005, ca_blue=0.9995)),
    ("hot pixels + median 2", dict(hot_pixels=True, median_passes=2)),
    ("no rotate, no crop", dict(auto_rotate=False, camera_crop=False)),
]
for label, props in cases:
    img, dt = load(**props)
    row(label, img, dt)

for dm in ("linear", "vng", "ppg", "ahd", "dcb", "dht", "aahd"):
    img, dt = load(demosaic=dm, half_size=False)
    row("demosaic %s (full size)" % dm, img, dt, None)

with open(os.path.join("build", "test-out", "options.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
