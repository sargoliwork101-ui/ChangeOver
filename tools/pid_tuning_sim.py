#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
[EN] Charge-PID tuning simulator - a faithful transcript of
     func__Charger_PidStep (charger.c) plus the flyback+lead-acid plant it
     drives. It exists so a coefficient can be judged by MEASUREMENT before
     it is ever flashed: the user's order of 2026-09-29 was "calibrate it
     yourself from the tables for the first time so I can optimise it
     later", and this is the tool that makes "optimise it later" cheap.

     It mirrors C semantics deliberately: truncate-toward-zero division,
     int32 milli-permille integral with a carried remainder, whole-permille
     PWM output. A floating-point model would hide exactly the quantisation
     effects (P-term ripple, output hysteresis, backstop granularity) that
     every hard-won constant in charger.h exists to tame.

     Run:  python3 tools/pid_tuning_sim.py            # default report
           python3 tools/pid_tuning_sim.py --help

[FA] شبیه‌ساز تیون PID شارژ - ترجمهٔ وفادار func__Charger_PidStep به‌همراه
     مدل فلای‌بک و باتری سرب-اسید. برای این است که هر ضریب پیش از فلش‌شدن
     با «اندازه‌گیری» قضاوت شود: دستور کاربر در ۲۰۲۶-۰۹-۲۹ این بود که
     «خودت بر اساس جدول‌ها کالیبره کن تا بعداً اگر خواستم بهینه‌اش کنم»، و
     این همان ابزاری است که «بعداً بهینه کردن» را ارزان می‌کند.

     عمداً معنای زبان C را آینه می‌کند: تقسیم با قطع اعشار به سمت صفر،
     انتگرال میلی‌پرمیل صحیح با باقی‌ماندهٔ حمل‌شونده، و خروجی پرمیل صحیح.
     مدل اعشاری دقیقاً همان اثرهای کوانتیزاسیون را پنهان می‌کرد (ریپل جملهٔ
     P، هیسترزیس خروجی، دانه‌بندی پشتیبان) که هر ثابت سخت‌به‌دست‌آمده در
     charger.h برای رام کردنشان وجود دارد.
"""

import argparse
import math
import random
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHARGER_H = ROOT / "Firmware" / "Modules" / "Charger" / "charger.h"


# --------------------------------------------------------------------------
# Constants are READ FROM charger.h, never duplicated here. If the firmware
# changes and this file is not updated, the simulation follows the firmware
# instead of silently reporting numbers for a build that no longer exists.
# ثابت‌ها از charger.h خوانده می‌شوند و اینجا تکرار نمی‌شوند.
# --------------------------------------------------------------------------
def _hdr(name, default=None):
    m = re.search(r"#define\s+%s\s+(\d+)u" % name, CHARGER_H.read_text(encoding="utf-8"))
    if m:
        return int(m.group(1))
    if default is not None:
        return default
    raise SystemExit("constant %s not found in charger.h" % name)


ABSORB_MV = _hdr("CHG_ABSORB_MV")
BULK_IMAX_MA = _hdr("CHG_BULK_CURRENT_MAX_MA")
DUTY_MAX = _hdr("CHG_DUTY_MAX_PERMILLE")
HARD_FAULT_MA = _hdr("CHG_CURRENT_HARD_FAULT_MA")
MARGIN_MA = _hdr("CHG_PID_CURRENT_MARGIN_MA")
ERRCLAMP = _hdr("CHG_PID_ERROR_CLAMP", 4000)
SCALE = _hdr("CHG_PID_DUTY_SCALE", 1000)
PERIOD_MS = _hdr("CHG_PID_PERIOD_MS")
HYST = _hdr("CHG_PID_OUTPUT_HYST_MILLI")
BACKSTOP_MV = _hdr("CHG_PID_BACKSTOP_MV")
GAIN_I = _hdr("CHG_PID_BACKSTOP_GAIN_I")
GAIN_V = _hdr("CHG_PID_BACKSTOP_GAIN_V")
VOLT_FILTER_N = _hdr("CHG_PID_VOLT_FILTER_N")
MAX_STEP = _hdr("CHG_PID_MAX_STEP_PERMILLE")
KP_DIV = _hdr("CHG_PID_KP_DIV", 1)
KI_DIV = _hdr("CHG_PID_KI_DIV", 1000)
KD_DIV = _hdr("CHG_PID_KD_DIV", 1)

FACTORY = [
    tuple(_hdr("CHG_PID_%s_%s" % (loop, f))
          for f in ("KP", "KI", "KD", "UP_RATE", "DOWN_RATE"))
    for loop in ("CURRENT", "VOLTAGE")
]


def cdiv(a, b):
    """[EN] C integer division: truncates toward zero, unlike Python's floor.
       [FA] تقسیم صحیح زبان C: به سمت صفر قطع می‌کند، برخلاف پایتون."""
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


class Plant:
    """[EN] Flyback in DCM feeding a lead-acid half-pack.
           i = 230.4 x (D/1000)^2 / V  (the DCM transfer measured on the
           bench), a series resistance, and an exponential gassing sink that
           is what actually ends the charge. voc integrates the net current.
       [FA] فلای‌بک در DCM که نیم-پک سرب-اسید را تغذیه می‌کند."""

    def __init__(self, voc=12200.0, R=0.15, I0=50.0, C=2000.0, noise=0.0, seed=1):
        self.voc, self.R, self.I0, self.C, self.noise = voc, R, I0, C, noise
        self.k = 0
        # [EN] Real ADC noise is broadband, not a tone. An earlier version of
        #      this file used a single sine, and the duty-reversal counts it
        #      produced were chaotic enough to reverse a design decision -
        #      use a seeded PRNG and average several seeds instead.
        # [FA] نویز واقعی ADC پهن‌باند است نه تک‌تن. نسخهٔ قبلی این فایل یک
        #      سینوس داشت و شمارش تغییر جهتش آن‌قدر آشوبناک بود که می‌توانست
        #      یک تصمیم طراحی را برعکس کند - به‌جایش PRNG با seed و میانگین
        #      چند seed.
        self.rng = random.Random(seed)

    def step(self, duty, dt):
        v = self.voc
        for _ in range(4):                       # settle the implicit loop
            i = max(0.0, 230.4 * (duty / 1000.0) ** 2 / (v / 1000.0)) * 1000.0
            v = self.voc + i * self.R
        igas = self.I0 * math.exp((v - 14400.0) / 150.0)
        self.voc += ((i - igas) / 1000.0) * dt / self.C * 1000.0
        self.k += 1
        n = self.rng.uniform(-self.noise, self.noise) if self.noise else 0.0
        return v + n, i                          # measured, true current


class Pid:
    """[EN] One channel of func__Charger_PidStep.
       `rows` is [(Kp,Ki,Kd,up,down)] - 2 rows = the shipped CC/CV design
       (current loop + voltage loop), 3 rows = the retired v1.23 variant
       that split the voltage row at the setpoint, 1 row = a single PID used
       for everything (the architectures compared by --compare).
       [FA] یک کانال از func__Charger_PidStep."""

    def __init__(self, rows, imax=BULK_IMAX_MA):
        self.rows = rows
        self.imax = imax
        self.I = 0
        self.irem = 0
        self.le = 0
        self.lv = True
        self.applied = 0
        self.ceil = DUTY_MAX * SCALE

    def seed(self, duty):
        self.applied = duty
        self.I = duty * SCALE
        self.irem = 0
        self.le = 0
        self.lv = True

    def _rowset(self, vmv, target):
        """Pick the gain rows. With one row every loop shares it; with two,
        row 0 is current and row 1 is voltage; with three, the voltage row
        switches at the setpoint."""
        if len(self.rows) == 1:
            return self.rows[0], self.rows[0]
        if len(self.rows) == 2:
            return self.rows[0], self.rows[1]
        return self.rows[0], (self.rows[1] if vmv < target else self.rows[2])

    def step(self, vmv, ima, target, dt_ms, raw_mv=None, raw_ma=None,
             ceil_permille=DUTY_MAX, current_loop=True):
        rawv = vmv if raw_mv is None else raw_mv
        rawi = ima if raw_ma is None else raw_ma
        ceil = ceil_permille * SCALE
        applied_milli = self.applied * SCALE

        # ---- hard backstops, on the RAW samples (charger.c order) ----
        if rawi > self.imax:
            cut = int((rawi - self.imax) * GAIN_I)
            ceil = min(ceil, applied_milli - cut) if applied_milli > cut else 0
        if rawv > BACKSTOP_MV:
            cut = int((rawv - BACKSTOP_MV) * GAIN_V)
            ceil = min(ceil, applied_milli - cut) if applied_milli > cut else 0
        ceil = max(0, ceil)
        self.ceil = ceil

        crow, vrow = self._rowset(vmv, target)
        cKp, cKi, cKd, cUp, cDn = crow
        vKp, vKi, vKd, vUp, vDn = vrow

        ev = max(-ERRCLAMP, min(ERRCLAMP, int(target) - int(vmv)))
        ei = max(-ERRCLAMP, min(ERRCLAMP, (self.imax - MARGIN_MA) - int(ima)))
        pv = cdiv(vKp * ev, KP_DIV)
        pi = cdiv(cKp * ei, KP_DIV)
        dv = cdiv(vKd * (ev - self.le), KD_DIV) if self.lv else 0
        di = cdiv(cKd * (ei - self.le), KD_DIV) if not self.lv else 0

        if not current_loop:
            # single-loop architecture: voltage only, the current limit is
            # left entirely to the hard backstop above
            usev = True
        else:
            usev = (pv + dv) <= (pi + di)

        if usev:
            e, gi, up, dn, pp, dd = ev, vKi, vUp, vDn, pv, dv
        else:
            e, gi, up, dn, pp, dd = ei, cKi, cUp, cDn, pi, di

        rate = cdiv(gi * e, KI_DIV)
        rate = max(-dn, min(up, rate))
        num = rate * dt_ms + self.irem
        st = cdiv(num, 1000)
        self.irem = num - st * 1000
        self.I += st
        if self.I < 0:
            self.I, self.irem = 0, 0
        if self.I > ceil:
            self.I, self.irem = ceil, 0

        dem = max(0, min(ceil, self.I + pp + dd))

        prev = self.applied
        ap = prev
        dev = dem - applied_milli
        if dev >= HYST or dev <= -HYST:
            ap = (dem + SCALE // 2) // SCALE
        ap = max(prev - MAX_STEP, min(prev + MAX_STEP, ap))   # symmetric cap
        ap = min(ap, ceil // SCALE)                           # absolute cap
        self.applied, self.le, self.lv = ap, e, usev
        return ap


def charge(rows, hours=10.0, seed_duty=10, current_loop=True, **kw):
    """[EN] Run a charge. Returns [(t, v_true, i, duty)].
       [FA] یک شارژ را اجرا می‌کند."""
    pl = Plant(**kw)
    pid = Pid(rows)
    pid.seed(seed_duty)
    d, t, log, S = seed_duty, 0.0, [], None
    dt = PERIOD_MS / 1000.0
    while t < hours * 3600:
        v, i = pl.step(d, dt)
        vi = round(v)
        if S is None:
            S = vi * VOLT_FILTER_N
        S = S - (S // VOLT_FILTER_N) + vi          # leaky-integrator prefilter
        d = pid.step(S // VOLT_FILTER_N, round(i), ABSORB_MV, PERIOD_MS,
                     raw_mv=vi, raw_ma=round(i), current_loop=current_loop)
        log.append((t, v, i, d))
        t += dt
    return log


def reversals(duties):
    r, prev = 0, 0
    for k in range(1, len(duties)):
        dd = duties[k] - duties[k - 1]
        if dd:
            sg = 1 if dd > 0 else -1
            if prev and sg != prev:
                r += 1
            prev = sg
    return r


def score(log):
    v = [r[1] for r in log]
    i = [r[2] for r in log]
    d = [r[3] for r in log]
    hold = log[int(len(log) * 0.8):]
    bulk = [x for x in log if 1200 < x[0] < 5400]
    tset = next((r[0] for r in log if r[1] >= ABSORB_MV), None)
    return dict(
        peak=max(v), imax=max(i), rev=reversals(d), tset=tset,
        holdlo=min(x[1] for x in hold), holdhi=max(x[1] for x in hold),
        holderr=max(abs(x[1] - ABSORB_MV) for x in hold),
        bulklo=min((x[2] for x in bulk), default=0),
        bulkhi=max((x[2] for x in bulk), default=0),
        endduty=d[-1])


SCENARIOS = {
    "nominal": {},
    "flat 11.0 V start": dict(voc=11000.0),
    "0.05 ohm short leads": dict(R=0.05),
    "0.30 ohm long/worn": dict(R=0.30),
    "new pack (I0=10)": dict(I0=10.0),
    "worn pack (I0=200)": dict(I0=200.0),
    "small pack (C=800)": dict(C=800.0),
    "big pack (C=4000)": dict(C=4000.0),
    "sensor noise 15 mV": dict(noise=15.0),
    "sensor noise 40 mV": dict(noise=40.0),
}


def report(rows, title, hours=10.0, current_loop=True):
    print("=" * 78)
    print(title)
    print("=" * 78)
    hdr = ("%-22s %9s %8s %7s %8s %20s %16s"
           % ("scenario", "peak V", "peak I", "rev", "t14.4V", "absorb hold", "bulk I"))
    print(hdr)
    worst_v = worst_i = 0.0
    worst_err = 0.0
    for nm, kw in SCENARIOS.items():
        s = score(charge(rows, hours=hours, current_loop=current_loop, **kw))
        worst_v, worst_i = max(worst_v, s["peak"]), max(worst_i, s["imax"])
        worst_err = max(worst_err, s["holderr"])
        t = ("%.0fmin" % (s["tset"] / 60)) if s["tset"] else "n/a"
        print("%-22s %9.1f %8.1f %7d %8s %9.1f..%-9.1f %7.1f..%-7.1f"
              % (nm, s["peak"], s["imax"], s["rev"], t,
                 s["holdlo"], s["holdhi"], s["bulklo"], s["bulkhi"]))
    print("-" * 78)
    print("worst peak %.1f mV / %.1f mA | worst hold error %.1f mV"
          % (worst_v, worst_i, worst_err))
    print("14.8 V backstop respected: %s | 950 mA hard fault avoided: %s"
          % ("YES" if worst_v <= BACKSTOP_MV else "NO",
             "YES" if worst_i <= HARD_FAULT_MA else "NO"))
    print()
    return worst_v, worst_i, worst_err


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--hours", type=float, default=10.0, help="simulated hours per run")
    ap.add_argument("--rows", type=str, default=None,
                    help="custom gains, e.g. '12,1600,0,1000,1000;150,600,0,30,1000;150,18000,0,10,1000'")
    ap.add_argument("--compare", action="store_true",
                    help="compare 1-row / 2-row / 3-row architectures")
    args = ap.parse_args()

    print("constants read from charger.h: absorb %d mV, bulk %d mA, ceiling %d permille,"
          % (ABSORB_MV, BULK_IMAX_MA, DUTY_MAX))
    print("hysteresis %d, prefilter N=%d, mis-tune cap %d permille, backstops %d mA / %d mV"
          % (HYST, VOLT_FILTER_N, MAX_STEP, BULK_IMAX_MA, BACKSTOP_MV))
    print()

    if args.rows:
        rows = [tuple(int(x) for x in r.split(",")) for r in args.rows.split(";")]
        report(rows, "CUSTOM GAINS %s" % (rows,), hours=args.hours,
               current_loop=(len(rows) > 1))
        return

    report(FACTORY, "FACTORY CALIBRATION (two loops, as shipped)", hours=args.hours)


if __name__ == "__main__":
    main()
