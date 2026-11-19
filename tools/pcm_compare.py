"""Compare two PCM recordings of the same scene (M07-02, M07-10): the Switch's and the desktop reference's.

    python tools/pcm_compare.py <switch.wav> <reference.wav> [--out report.txt] [--window 10]

Both are upstream's PS2X_AUDIO_DUMP files (16-bit stereo, everything the SPU2 produced). Reports, for each: rate,
channels, length, RMS and peak per channel, clipped samples (|s| = 32767 / 32768) and silent share; then, on loudness
envelopes (RMS per --window ms), the offset that lines the two up best and the envelopes' correlation there, per
channel and with the channels swapped (a channel-order check), and the level difference in dB over the overlap.
The two runs' timing differs (frame rate, load times), so the envelopes, not the samples, are compared; a
sample-exact comparison is not claimed. Standard library only.
"""
import argparse
import array
import math
from pathlib import Path
import sys
import wave


def load(path):
    with wave.open(str(path), "rb") as w:
        rate, channels, width, frames = w.getframerate(), w.getnchannels(), w.getsampwidth(), w.getnframes()
        if width != 2:
            raise SystemExit(f"{path}: {8 * width}-bit samples, expected 16-bit")
        data = array.array("h", w.readframes(frames))
    if sys.byteorder != "little":
        data.byteswap()
    return rate, channels, data


def stats(rate, channels, data):
    out = {"rate": rate, "channels": channels, "seconds": len(data) / channels / rate}
    for c in range(channels):
        s = data[c::channels]
        n = len(s) or 1
        out[f"rms{c}"] = math.sqrt(sum(v * v for v in s) / n)
        out[f"peak{c}"] = max((abs(v) for v in s), default=0)
        out[f"clipped{c}"] = sum(1 for v in s if v >= 32767 or v <= -32768)
        out[f"silent{c}"] = sum(1 for v in s if -8 <= v <= 8) / n
    return out


def envelope(rate, channels, data, channel, window_ms):
    step = max(1, rate * window_ms // 1000)
    s = data[channel::channels]
    return [math.sqrt(sum(v * v for v in s[i:i + step]) / step) for i in range(0, len(s) - step + 1, step)]


def correlation(a, b):
    n = min(len(a), len(b))
    if n < 10:
        return float("nan")
    a, b = a[:n], b[:n]
    ma, mb = sum(a) / n, sum(b) / n
    num = sum((x - ma) * (y - mb) for x, y in zip(a, b))
    den = math.sqrt(sum((x - ma) ** 2 for x in a) * sum((y - mb) ** 2 for y in b))
    return num / den if den else float("nan")


def best_offset(a, b, max_shift):
    """The shift of b against a (in envelope steps; positive: b starts later) with the highest correlation."""
    best = (float("-inf"), 0)
    for shift in range(-max_shift, max_shift + 1):
        x, y = (a, b[shift:]) if shift >= 0 else (a[-shift:], b)
        r = correlation(x, y)
        if not math.isnan(r) and r > best[0]:
            best = (r, shift)
    return best


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("switch", type=Path)
    parser.add_argument("reference", type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--window", type=int, default=10, help="envelope window in ms (default 10)")
    parser.add_argument("--max-shift", type=float, default=60.0, help="largest offset searched, seconds (default 60)")
    args = parser.parse_args()
    lines = []
    loaded = {}
    for name, path in (("switch", args.switch), ("reference", args.reference)):
        rate, channels, data = load(path)
        loaded[name] = (rate, channels, data)
        s = stats(rate, channels, data)
        lines.append(f"{name}: {path.name}: {s['rate']} Hz, {s['channels']} channels, {s['seconds']:.1f} s")
        for c in range(channels):
            lines.append(f"  channel {c}: RMS {s[f'rms{c}']:.1f}, peak {s[f'peak{c}']}, clipped {s[f'clipped{c}']}, "
                         f"silent {100 * s[f'silent{c}']:.1f} %")
    (ra, ca, da), (rb, cb, db) = loaded["switch"], loaded["reference"]
    if (ra, ca) != (rb, cb):
        lines.append(f"FORMAT DIFFERS: switch {ra} Hz / {ca} ch, reference {rb} Hz / {cb} ch")
    else:
        env = {(n, c): envelope(r, ch, d, c, args.window) for n, (r, ch, d) in loaded.items() for c in range(ch)}
        # coarse search on 100 ms envelopes (pure Python: a fine search over a minute of shifts is too slow), then
        # refined on the --window envelopes within 200 ms of it
        coarse = {k: [math.sqrt(sum(v * v for v in e[i:i + 100 // args.window]) / (100 // args.window))
                      for i in range(0, len(e), 100 // args.window)] for k, e in env.items() if k[1] == 0}
        _, coarse_shift = best_offset(coarse[("switch", 0)], coarse[("reference", 0)], int(args.max_shift * 10))
        centre, span = coarse_shift * (100 // args.window), 200 // args.window
        best = (float("-inf"), centre)
        for s in range(centre - span, centre + span + 1):
            a, b = env[("switch", 0)], env[("reference", 0)]
            x, y = (a, b[s:]) if s >= 0 else (a[-s:], b)
            r = correlation(x[:min(len(x), len(y))], y[:min(len(x), len(y))])
            if not math.isnan(r) and r > best[0]:
                best = (r, s)
        shift = best[1]
        lines.append(f"envelope offset {shift * args.window / 1000:+.2f} s (reference against switch), "
                     f"window {args.window} ms")
        for c in range(ca):
            a = env[("switch", c)]
            b = env[("reference", c)]
            x, y = (a, b[shift:]) if shift >= 0 else (a[-shift:], b)
            n = min(len(x), len(y))
            same = correlation(x[:n], y[:n])
            other = env[("reference", 1 - c)] if ca == 2 else b
            ox, oy = (a, other[shift:]) if shift >= 0 else (a[-shift:], other)
            swapped = correlation(ox[:min(len(ox), len(oy))], oy[:min(len(ox), len(oy))])
            mx = math.sqrt(sum(v * v for v in x[:n]) / n) if n else 0.0
            my = math.sqrt(sum(v * v for v in y[:n]) / n) if n else 0.0
            level = 20 * math.log10(mx / my) if mx and my else float("nan")
            mo = math.sqrt(sum(v * v for v in oy[:n]) / n) if n else 0.0
            level_swapped = 20 * math.log10(mx / mo) if mx and mo else float("nan")
            lines.append(f"  channel {c}: envelope correlation {same:.3f} (with the other channel {swapped:.3f}); "
                         f"level switch - reference {level:+.2f} dB (against the other channel {level_swapped:+.2f} dB) "
                         f"over {n * args.window / 1000:.1f} s")
    text = "\n".join(lines) + "\n"
    print(text, end="")
    if args.out:
        args.out.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
