# Re-develop + non-destructive filter test (run in gimp-console).
import os, struct
from gi.repository import Gimp, Gio, Gegl

pdb = Gimp.get_pdb()
src = os.path.abspath(os.path.join("samples", "Canon-EOS_6D.cr2"))
xcf = os.path.abspath(os.path.join("build", "test-out", "redevelop.xcf"))
lines = []


def mean(layer):
    buf = layer.get_buffer()
    e = buf.get_extent()
    s = 1 / 16
    data = buf.get(Gegl.Rectangle.new(0, 0, int(e.width * s), int(e.height * s)), s,
                   "RGB float", Gegl.AbyssPolicy.CLAMP)
    n = len(data) // 12
    v = struct.unpack("<%df" % (n * 3), data[: n * 12])
    return [round(sum(v[c::3]) / n, 4) for c in range(3)]


def save(img, path):
    Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, img, Gio.File.new_for_path(path), None)


def load(path):
    return Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(path))


def redevelop(img, **props):
    proc = pdb.lookup_procedure("plug-in-raw-heic-redevelop")
    cfg = proc.create_config()
    cfg.set_property("run-mode", Gimp.RunMode.NONINTERACTIVE)
    cfg.set_property("image", img)
    for k, v in props.items():
        cfg.set_property(k.replace("_", "-"), v)
    return proc.run(cfg).index(0)


img = load(src)
lines.append("loaded: layers=%d mean=%s" % (len(img.get_layers()), mean(img.get_layers()[0])))
save(img, xcf)
img.delete()

img = load(xcf)
p = img.get_parasite("gimp-raw-heic-source")
lines.append("xcf reopened: parasite=%s layer-marker=%s" % (
    p is not None, img.get_layers()[0].get_parasite("gimp-raw-heic-layer") is not None))
before = mean(img.get_layers()[0])

st = redevelop(img, wb_mode="manual", wb_temperature=3200.0, exposure=1.0,
               redevelop_mode="replace")
after = mean(img.get_layers()[0])
lines.append("replace (3200K, +1EV): status=%s layers=%d before=%s after=%s" % (
    st, len(img.get_layers()), before, after))

st = redevelop(img, half_size=True, redevelop_mode="new-layer")
lay = img.get_layers()
lines.append("new-layer (half size): status=%s layers=%d top=%s %dx%d image=%dx%d" % (
    st, len(lay), lay[0].get_name(), lay[0].get_width(), lay[0].get_height(),
    img.get_width(), img.get_height()))

par = img.get_parasite("gimp-raw-heic-source")
txt = bytes(par.get_data()).decode("utf-8", "replace")
lines.append("parasite now: " + " ".join(l for l in txt.splitlines()
                                          if l.split("=")[0] in ("wb-mode", "wb-temperature", "exposure", "half-size")))

proc = pdb.lookup_procedure("plug-in-raw-heic-quick-adjust")
cfg = proc.create_config()
cfg.set_property("run-mode", Gimp.RunMode.NONINTERACTIVE)
cfg.set_property("image", img)
cfg.set_core_object_array("drawables", [img.get_layers()[-1]])
cfg.set_property("exposure", 0.5)
cfg.set_property("temperature-from", 5000.0)
cfg.set_property("shadows", 30.0)
cfg.set_property("add-levels", True)
cfg.set_property("add-curves", True)
st = proc.run(cfg).index(0)
filters = img.get_layers()[-1].get_filters()
lines.append("quick-adjust: status=%s filters=%s" % (st, [f.get_name() for f in filters]))

save(img, xcf)
img.delete()
img = load(xcf)
filters = img.get_layers()[-1].get_filters()
lines.append("after XCF save/reload: filters=%s ops=%s" % (
    [f.get_name() for f in filters], [f.get_operation_name() for f in filters]))
st = redevelop(img, exposure=-1.0)
lines.append("redevelop after reload: status=%s" % st)

# image without RAW parasite must fail cleanly
plain = Gimp.Image.new(32, 32, Gimp.ImageBaseType.RGB)
plain.insert_layer(Gimp.Layer.new(plain, "x", 32, 32, Gimp.ImageType.RGB_IMAGE, 100,
                                  Gimp.LayerMode.NORMAL), None, 0)
lines.append("non-RAW image: status=%s" % redevelop(plain))

with open(os.path.join("build", "test-out", "redevelop.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
