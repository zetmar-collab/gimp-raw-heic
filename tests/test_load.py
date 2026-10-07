# Batch test for gimp-raw-heic, run inside gimp-console:
#   gimp-console-3.2.exe -i --batch-interpreter=python-fu-eval \
#       -b "exec(open(r'tests/test_load.py', encoding='utf-8').read())" --quit
# Environment:
#   RAWHEIC_SAMPLES  folder with sample files (default: samples)
#   RAWHEIC_OUT      folder for PNG previews  (default: build/test-out)
import os, sys, time
from gi.repository import Gimp, Gio

samples = os.environ.get("RAWHEIC_SAMPLES", "samples")
out_dir = os.environ.get("RAWHEIC_OUT", os.path.join("build", "test-out"))
only = os.environ.get("RAWHEIC_ONLY", "")
os.makedirs(out_dir, exist_ok=True)

pdb = Gimp.get_pdb()
results = []

def export_preview(image, name):
    dup = image.duplicate()
    dup.convert_precision(Gimp.Precision.U8_NON_LINEAR)
    w, h = dup.get_width(), dup.get_height()
    s = 1024 / max(w, h)
    if s < 1:
        dup.scale(max(1, int(w * s)), max(1, int(h * s)))
    dup.flatten()
    Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, dup,
                   Gio.File.new_for_path(os.path.join(out_dir, name + ".png")), None)
    dup.delete()

for fn in sorted(f for f in os.listdir(samples) if os.path.isfile(os.path.join(samples, f))):
    if only and only not in fn:
        continue
    path = os.path.abspath(os.path.join(samples, fn))
    t0 = time.time()
    try:
        image = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(path))
    except Exception as e:
        image = None
        err = str(e)
    dt = time.time() - t0
    if image is None:
        results.append("FAIL %-32s %.1fs %s" % (fn, dt, err if 'err' in dir() else ""))
        continue
    prof = image.get_color_profile() or image.get_effective_color_profile()
    own = "own" if image.get_color_profile() else "default"
    par = image.get_parasite("gimp-raw-heic-source")
    results.append("OK   %-32s %5.1fs %5dx%-5d %-28s %-52s parasite=%s" % (
        fn, dt, image.get_width(), image.get_height(),
        str(image.get_precision()).split(".")[-1].split(" ")[0],
        (prof.get_label() if prof else "-") + " [" + own + "]", "yes" if par else "no"))
    try:
        export_preview(image, os.path.splitext(fn)[0])
    except Exception as e:
        results.append("     preview export failed: %s" % e)
    image.delete()

with open(os.path.join(out_dir, "results.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(results) + "\n")
print("\n".join(results))
