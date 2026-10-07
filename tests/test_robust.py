# Thumbnails + damaged-file robustness for gimp-raw-heic (run in gimp-console).
# Creates truncated / bit-flipped copies of each sample in build/fuzz and
# checks that loading fails cleanly (error status) instead of crashing.
import os, random, time
from gi.repository import Gimp, Gio

samples = os.environ.get("RAWHEIC_SAMPLES", "samples")
fuzz_dir = os.path.join("build", "fuzz")
os.makedirs(fuzz_dir, exist_ok=True)
pdb = Gimp.get_pdb()
lines = []
random.seed(1234)

thumb = pdb.lookup_procedure("file-raw-heic-raw-load-thumb")
for fn in sorted(f for f in os.listdir(samples) if os.path.isfile(os.path.join(samples, f))):
    path = os.path.abspath(os.path.join(samples, fn))
    cfg = thumb.create_config()
    cfg.set_property("file", Gio.File.new_for_path(path))
    cfg.set_property("thumb-size", 256)
    t0 = time.time()
    r = thumb.run(cfg)
    dt = time.time() - t0
    st = r.index(0)
    if st == Gimp.PDBStatusType.SUCCESS:
        img = r.index(1)
        lines.append("THUMB OK   %-32s %.2fs thumb %dx%d  full %dx%d" % (
            fn, dt, img.get_width(), img.get_height(), r.index(2), r.index(3)))
        img.delete()
    else:
        lines.append("THUMB FAIL %-32s %s" % (fn, st))

def try_load(path):
    try:
        img = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(path))
        if img:
            s = "loaded %dx%d" % (img.get_width(), img.get_height())
            img.delete()
            return s
        return "error (None)"
    except Exception as e:
        return "error: " + str(e).splitlines()[0][:90]

for fn in sorted(f for f in os.listdir(samples) if os.path.isfile(os.path.join(samples, f))):
    data = open(os.path.join(samples, fn), "rb").read()
    base, ext = os.path.splitext(fn)
    variants = {
        "trunc10": data[: len(data) // 10],
        "trunc50": data[: len(data) // 2],
        "header":  data[:4096],
        "zero":    b"",
    }
    flipped = bytearray(data)
    for _ in range(200):
        i = random.randrange(len(flipped))
        flipped[i] = random.randrange(256)
    variants["bitflip"] = bytes(flipped)
    for name, blob in variants.items():
        p = os.path.abspath(os.path.join(fuzz_dir, "%s-%s%s" % (base, name, ext)))
        with open(p, "wb") as f:
            f.write(blob)
        lines.append("FUZZ %-40s %s" % (os.path.basename(p), try_load(p)))
        os.remove(p)

# the plug-in must still be alive and working after all of that
ok = try_load(os.path.abspath(os.path.join(samples, sorted(f for f in os.listdir(samples) if os.path.isfile(os.path.join(samples, f)))[0])))
lines.append("AFTER-FUZZ sanity load: " + ok)

with open(os.path.join("build", "test-out", "robust.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
print("\n".join(lines))
