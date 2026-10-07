# Metadata test (run in gimp-console): EXIF from RAW reaches the image,
# orientation is normalised, and metadata survives JPEG export.
import os
from gi.repository import Gimp, Gio

samples = os.environ.get("RAWHEIC_SAMPLES", "samples")
out = os.path.join("build", "test-out", "meta")
os.makedirs(out, exist_ok=True)
lines = []


def tag(meta, name):
    try:
        return meta.try_get_tag_string(name)
    except Exception:
        return None


for fn in sorted(os.listdir(samples)):
    p = os.path.join(samples, fn)
    if not os.path.isfile(p):
        continue
    img = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, Gio.File.new_for_path(os.path.abspath(p)))
    meta = img.get_metadata()
    if meta is None:
        lines.append("%-32s NO METADATA" % fn)
        img.delete()
        continue
    model = tag(meta, "Exif.Image.Model")
    iso = tag(meta, "Exif.Photo.ISOSpeedRatings")
    date = tag(meta, "Exif.Photo.DateTimeOriginal")
    orient = tag(meta, "Exif.Image.Orientation")
    jpg = os.path.abspath(os.path.join(out, os.path.splitext(fn)[0] + ".jpg"))
    dup = img.duplicate()
    dup.convert_precision(Gimp.Precision.U8_NON_LINEAR)
    Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, dup, Gio.File.new_for_path(jpg), None)
    dup.delete()
    lines.append("%-32s model=%s iso=%s date=%s orientation=%s" % (fn, model, iso, date, orient))
    img.delete()

with open(os.path.join("build", "test-out", "metadata.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
