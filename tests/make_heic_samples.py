"""Generate HEIC test files with known content using libheif's heif-enc.

Creates in samples/heic/:
  sdr8.heic      8-bit sRGB photo (from a RAW test preview) with thumbnail
  sdr10.heic     10-bit sRGB gradient
  alpha.heic     8-bit with alpha channel
  pq.heic        10-bit PQ (BT.2100) test chart, lossless
  hlg.heic       10-bit HLG (BT.2100) test chart, lossless
The PQ/HLG charts contain grey patches at scene-linear 0.18, 1.0 (SDR
reference white, 203 nits) and 4.0 (812 nits); see PATCHES below.
Usage: python tests/make_heic_samples.py [preview.png]
"""
import math, os, struct, subprocess, sys, zlib

OUT = os.path.join("samples", "heic")
W, H = 512, 256
PATCHES = [0.18, 1.0, 4.0]      # linear values, left to right, top half


def write_png(path, w, h, rows, channels, bits):
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    ctype = {3: 2, 4: 6}[channels]
    raw = b"".join(b"\0" + r for r in rows)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, bits, ctype, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def pq_encode(lin):                      # lin: 1.0 = 203 nits
    m1, m2 = 2610 / 16384, 2523 / 4096 * 128
    c1, c2, c3 = 3424 / 4096, 2413 / 4096 * 32, 2392 / 4096 * 32
    y = max(lin * 203.0 / 10000.0, 0.0)
    p = y ** m1
    return ((c1 + c2 * p) / (1 + c3 * p)) ** m2


def hlg_encode(lin):                     # lin: 1.0 = HLG reference white
    a, b, c = 0.17883277, 0.28466892, 0.55991073
    ref = (math.exp((0.75 - c) / a) + b) / 12.0
    E = max(lin * ref, 0.0)
    return math.sqrt(3 * E) if E <= 1 / 12 else a * math.log(12 * E - b) + c


def chart_rows(encode):
    rows = []
    for y in range(H):
        row = bytearray()
        for x in range(W):
            if y < H // 2:
                lin = PATCHES[min(x * len(PATCHES) // W, len(PATCHES) - 1)]
                rgb = (lin, lin, lin)
            else:                           # ramp 0 .. 8 (0 .. 1624 nits)
                v = 8.0 * x / (W - 1)
                rgb = (v, v * 0.5, v * 0.25)
            for c in rgb:
                row += struct.pack(">H", int(round(min(max(encode(c), 0.0), 1.0) * 65535)))
        rows.append(bytes(row))
    return rows


def run(args):
    print(" ".join(args))
    subprocess.run(args, check=True)


def main():
    os.makedirs(OUT, exist_ok=True)
    tmp = os.path.join(OUT, "_tmp")
    os.makedirs(tmp, exist_ok=True)

    # PQ / HLG charts: primaries 9 (BT.2020), transfer 16 / 18, lossless
    for name, enc, tc in (("pq", pq_encode, 16), ("hlg", hlg_encode, 18)):
        png = os.path.join(tmp, name + ".png")
        write_png(png, W, H, chart_rows(enc), 3, 16)
        run(["heif-enc", "-L", "-b", "10", "--colour_primaries", "9",
             "--transfer_characteristic", str(tc), "--matrix_coefficients", "0",
             "-o", os.path.join(OUT, name + ".heic"), png])

    # 10-bit SDR gradient
    rows = []
    for y in range(H):
        row = bytearray()
        for x in range(W):
            v = x / (W - 1)
            for c in (v, 1 - v, y / (H - 1)):
                row += struct.pack(">H", int(c * 65535))
        rows.append(bytes(row))
    png = os.path.join(tmp, "sdr10.png")
    write_png(png, W, H, rows, 3, 16)
    run(["heif-enc", "-b", "10", "-q", "90", "--transfer_characteristic", "13",
         "-o", os.path.join(OUT, "sdr10.heic"), png])

    # 8-bit with alpha: a disc on transparent background
    rows = []
    for y in range(H):
        row = bytearray()
        for x in range(W):
            d = math.hypot(x - W / 2, y - H / 2)
            a = 255 if d < H * 0.4 else 0
            row += bytes((200, 60, 30, a))
        rows.append(bytes(row))
    png = os.path.join(tmp, "alpha.png")
    write_png(png, W, H, rows, 4, 8)
    run(["heif-enc", "-q", "90", "-o", os.path.join(OUT, "alpha.heic"), png])

    # 8-bit photo with embedded thumbnail
    photo = sys.argv[1] if len(sys.argv) > 1 else os.path.join("build", "test-out", "Fujifilm-X-T3.png")
    if os.path.exists(photo):
        run(["heif-enc", "-q", "80", "-t", "256", "-o", os.path.join(OUT, "sdr8.heic"), photo])

    for f in os.listdir(tmp):
        os.remove(os.path.join(tmp, f))
    os.rmdir(tmp)


if __name__ == "__main__":
    main()
