"""
osciloscopio.py - measures the ESC signal from an oscilloscope CSV export.

PWM:
    pulse width, period/frequency and the throttle it represents.
DShot (150/300):
    bit period, T0H, T1H (vs. the spec), frame length, frame rate and
    full decoding of every frame (11-bit value, telemetry bit, CRC check).

Usage:
    python osciloscopio.py capture.csv
    python osciloscopio.py capture.csv --threshold 2.5 --channel 1

Accepted CSV layouts (auto-detected):
    * Rigol DS1000Z style: header "X,CH1,...,Start,Increment" (sample index)
    * time,voltage columns (Siglent, Keysight, Rigol DHO, Owon, Hantek...)
    * Tektronix TBS/TDS: metadata in columns 0-2, time and voltage in 3-4
Use --time-col / --volt-col (0-based) if the auto-detection picks the
wrong columns.

DShot spec values: Betaflight docs, https://betaflight.com/docs/development/API/Dshot
"""
import argparse
import csv
import os
import sys

import numpy as np

DSHOT_SPECS = {   # name: (bit period us, T1H us, T0H us)
    'DShot150': (6.667, 5.000, 2.500),
    'DShot300': (3.333, 2.500, 1.250),
}
ANALOG_SPECS = {  # name: (min us, max us)
    'PWM':        (1000.0, 2000.0),
}


# ------------------------------------------------------------------
# CSV reading
# ------------------------------------------------------------------
def _num(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def read_capture(path, time_col=None, volt_col=None, channel=1):
    with open(path, newline='', encoding='utf-8', errors='replace') as f:
        rows = list(csv.reader(f))

    header = [c.strip().lower() for c in rows[0]] if rows else []

    # Rigol DS1000Z: X,CH1,Start,Increment / Sequence,Volt,<start>,<inc>
    if 'increment' in header and 'start' in header:
        start = _num(rows[1][header.index('start')])
        inc = _num(rows[1][header.index('increment')])
        vcol = volt_col if volt_col is not None else channel
        v = np.array([_num(r[vcol]) for r in rows[2:] if len(r) > vcol and _num(r[vcol]) is not None])
        t = start + inc * np.arange(len(v))
        return t, v

    # Tektronix: first rows carry "Record Length", time/voltage in columns 3/4
    tek = any(r and 'record length' in r[0].lower() for r in rows[:20])
    if tek and time_col is None:
        time_col, volt_col = 3, 4

    data = []
    for r in rows:
        vals = [_num(c) for c in r]
        if time_col is not None and volt_col is not None:
            if len(vals) > max(time_col, volt_col) and vals[time_col] is not None and vals[volt_col] is not None:
                data.append((vals[time_col], vals[volt_col]))
            continue
        nums = [i for i, x in enumerate(vals) if x is not None]
        if len(nums) >= 2:
            # generic: time in the first numeric column, channel N after it
            ti = nums[0]
            vi = ti + channel
            if vi < len(vals) and vals[vi] is not None:
                data.append((vals[ti], vals[vi]))
    if len(data) < 10:
        sys.exit("Could not find time/voltage columns. Use --time-col and --volt-col.")
    a = np.array(data)
    return a[:, 0], a[:, 1]


# ------------------------------------------------------------------
# Edge detection with hysteresis
# ------------------------------------------------------------------
def edges(t, v, threshold=None, hyst=0.1):
    if threshold is None:
        lo, hi = np.percentile(v, 2), np.percentile(v, 98)
        threshold = (lo + hi) / 2
        hyst = 0.1 * (hi - lo)
    state = v[0] > threshold
    rises, falls = [], []
    up, dn = threshold + hyst / 2, threshold - hyst / 2
    for i in range(1, len(v)):
        if not state and v[i] > up:
            state = True
            # linear interpolation of the crossing time at the threshold
            rises.append(_cross(t, v, i, threshold))
        elif state and v[i] < dn:
            state = False
            falls.append(_cross(t, v, i, threshold))
    return np.array(rises), np.array(falls), threshold


def _cross(t, v, i, th):
    j = i - 1
    while j > 0 and (v[j] - th) * (v[i] - th) > 0:
        j -= 1
    if v[i] == v[j]:
        return t[i]
    return t[j] + (th - v[j]) * (t[i] - t[j]) / (v[i] - v[j])


def pulses(rises, falls):
    """Returns (rise time, high time, period to next rise) in seconds."""
    out = []
    for k, r in enumerate(rises):
        f = falls[falls > r]
        if len(f) == 0:
            break
        per = rises[k + 1] - r if k + 1 < len(rises) else np.nan
        out.append((r, f[0] - r, per))
    return np.array(out)


# ------------------------------------------------------------------
# Analysis
# ------------------------------------------------------------------
def analyse_analog(p):
    w = p[:, 1] * 1e6
    per = p[:-1, 2] * 1e3
    wm = np.mean(w)
    name = min(ANALOG_SPECS, key=lambda n: 0 if ANALOG_SPECS[n][0] * 0.9 <= wm <= ANALOG_SPECS[n][1] * 1.1
               else abs(np.log(wm / np.mean(ANALOG_SPECS[n]))))
    lo, hi = ANALOG_SPECS[name]
    print(f"\nProtocol (by pulse width): {name}  ({lo:g}-{hi:g} us)")
    print(f"Pulses measured   : {len(w)}")
    print(f"Pulse width       : mean {wm:.3f} us | min {w.min():.3f} | max {w.max():.3f} | std {np.std(w):.3f} us")
    if len(per):
        print(f"Period            : mean {np.mean(per):.4f} ms  -> {1000 / np.mean(per):.1f} Hz")
    print(f"Throttle (uni)    : {(wm - lo) / (hi - lo) * 100:.1f} %")
    print(f"Throttle (bi/3D)  : {(wm - (lo + hi) / 2) / ((hi - lo) / 2) * 100:+.1f} %  (neutral = {(lo + hi) / 2:g} us)")
    return name, None


def decode_frames(p):
    # a frame ends where the gap to the next rise is much longer than a bit
    bit_guess = np.median(p[:-1, 2])
    frames, cur = [], []
    for row in p:
        cur.append(row)
        if np.isnan(row[2]) or row[2] > 2.5 * bit_guess:
            frames.append(np.array(cur))
            cur = []
    result = []
    for fr in frames:
        if len(fr) != 16:
            continue                       # partial frame at the edges of the capture
        bit = np.median(fr[:-1, 2])
        bits = ''.join('1' if h > 0.5625 * bit else '0' for h in fr[:, 1])
        q = int(bits, 2)
        x = q >> 4
        crc_ok = (q & 0xF) == ((x ^ (x >> 4) ^ (x >> 8)) & 0xF)
        result.append(dict(t=fr[0, 0], bits=bits, frame=q, value=q >> 5, telem=(q >> 4) & 1,
                           crc_ok=crc_ok, bit=bit, highs=fr[:, 1],
                           length=fr[-1, 0] + fr[-1, 1] - fr[0, 0], pulses=fr))
    return result


def analyse_dshot(p):
    fr = decode_frames(p)
    if not fr:
        sys.exit("No complete 16-bit frame found. Increase the time window or the sample rate.")
    bit = np.median([f['bit'] for f in fr]) * 1e6
    name = min(DSHOT_SPECS, key=lambda n: abs(DSHOT_SPECS[n][0] - bit))
    sb, s1, s0 = DSHOT_SPECS[name]
    allh = np.concatenate([f['highs'] for f in fr]) * 1e6
    th = 0.5625 * bit
    h1, h0 = allh[allh > th], allh[allh <= th]

    def line(lbl, meas, spec):
        if len(meas) == 0:
            return f"{lbl:<18}: (no bits of this kind in the capture)"
        m = np.mean(meas)
        return f"{lbl:<18}: {m:.4f} us (std {np.std(meas):.4f}) | spec {spec:.3f} us | error {(m - spec) / spec * 100:+.2f} %"

    print(f"\nProtocol (by bit period): {name}  ->  {1e3 / bit:.1f} kbit/s")
    print(line("Bit period", [f['bit'] * 1e6 for f in fr], sb))
    print(line("T1H (bit '1')", h1, s1))
    print(line("T0H (bit '0')", h0, s0))
    print(f"{'Frame length':<18}: {np.mean([f['length'] for f in fr]) * 1e6:.2f} us (16 bits)")
    if len(fr) > 1:
        gaps = np.diff([f['t'] for f in fr]) * 1e3
        print(f"{'Frame interval':<18}: {np.mean(gaps):.4f} ms -> {1 / np.mean(gaps):.3f} kHz")
    ok = sum(f['crc_ok'] for f in fr)
    print(f"{'Frames decoded':<18}: {len(fr)}  (CRC OK: {ok}, CRC error: {len(fr) - ok})\n")

    seen = {}
    for f in fr:
        seen.setdefault(f['frame'], []).append(f)
    print("frame   bits (value | T | CRC)          value  telem  CRC   count")
    for q, lst in seen.items():
        f = lst[0]
        b = f['bits']
        kind = 'stop' if f['value'] == 0 else ('command' if f['value'] < 48 else 'throttle')
        print(f"0x{q:04X}  {b[:11]} {b[11]} {b[12:]}   {f['value']:5d}  {f['telem']:5d}  "
              f"{'OK ' if f['crc_ok'] else 'ERR'}  {len(lst):5d}  ({kind})")
    return name, fr


# ------------------------------------------------------------------
# Plot
# ------------------------------------------------------------------
def plot(t, v, threshold, name, frames, out_png, p):
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(11, 4.2))
    if frames:
        f = frames[0]
        t0 = f['pulses'][0, 0]
        bit = f['bit']
        m = (t >= t0 - 2 * bit) & (t <= t0 + 17 * bit)
        ax.plot((t[m] - t0) * 1e6, v[m], lw=1.2)
        top = np.percentile(v[m], 98)
        for k, (r, h, _) in enumerate(f['pulses']):
            ax.text((r - t0 + bit / 2) * 1e6, top * 1.08, f['bits'][k], ha='center', fontsize=11,
                    color='tab:red' if f['bits'][k] == '1' else 'tab:blue', fontweight='bold')
        for x in range(17):
            ax.axvline(x * bit * 1e6, color='0.85', lw=0.6, zorder=0)
        ax.set_ylim(top=top * 1.25)
        ax.set_title(f"{name}: frame 0x{f['frame']:04X} -> value {f['value']}, telemetry {f['telem']}, "
                     f"CRC {'OK' if f['crc_ok'] else 'ERROR'}  |  bit = {bit * 1e6:.3f} us")
        ax.set_xlabel('Time from frame start (us)')
    else:
        r0 = p[0, 0]
        per = p[0, 2] if not np.isnan(p[0, 2]) else p[0, 1] * 4
        m = (t >= r0 - 0.1 * per) & (t <= r0 + 1.2 * per)
        ax.plot((t[m] - r0) * 1e6, v[m], lw=1.2)
        ax.axvspan(0, p[0, 1] * 1e6, color='tab:orange', alpha=0.15)
        ax.set_title(f"{name}: pulse width {p[0, 1] * 1e6:.2f} us, period {per * 1e3:.3f} ms")
        ax.set_xlabel('Time from rising edge (us)')
    ax.axhline(threshold, color='0.5', ls='--', lw=0.8)
    ax.set_ylabel('Voltage (V)')
    fig.tight_layout()
    fig.savefig(out_png, dpi=150)
    print(f"\nPlot saved: {out_png}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('csv')
    ap.add_argument('--threshold', type=float, help='logic threshold in V (default: middle of the signal)')
    ap.add_argument('--channel', type=int, default=1, help='channel column for Rigol/X,CH files (default 1)')
    ap.add_argument('--time-col', type=int)
    ap.add_argument('--volt-col', type=int)
    ap.add_argument('--no-plot', action='store_true')
    a = ap.parse_args()

    t, v = read_capture(a.csv, a.time_col, a.volt_col, a.channel)
    dt = np.median(np.diff(t))
    print(f"File: {a.csv}")
    print(f"Samples: {len(t)} | sample interval {dt * 1e9:.1f} ns ({1 / dt / 1e6:.1f} MSa/s) | "
          f"window {(t[-1] - t[0]) * 1e3:.3f} ms | signal {np.percentile(v, 2):.2f} V .. {np.percentile(v, 98):.2f} V")

    r, f, th = edges(t, v, a.threshold)
    p = pulses(r, f)
    if len(p) == 0:
        sys.exit("No pulses found.")

    # DShot = bursts of pulses only a few us apart; analog = 1 pulse per >= 250 us
    if len(p) >= 16 and np.nanmedian(p[:, 2]) < 20e-6:
        if dt > 0.1e-6:
            print("WARNING: sample interval > 100 ns; use >= 10 MSa/s for DShot")
        name, frames = analyse_dshot(p)
    else:
        name, frames = analyse_analog(p)

    if not a.no_plot:
        plot(t, v, th, name, frames, os.path.splitext(a.csv)[0] + '_analysis.png', p)


if __name__ == '__main__':
    main()