#!/usr/bin/env node
/**
 * panel_preview_server.js — [EN] Offline preview of the ESP web panel.
 * [FA] پیش‌نمایش آفلاین پنل وب ESP.
 *
 * [EN] Serves the REAL panel HTML extracted from esp_link_panel.ino and
 *      simulates the STM32 behind it: a full charge cycle
 *      OFF -> BULK -> ABSORB -> FLOAT -> (sag) -> REENTRY -> BULK ...,
 *      the v1.14 charge-stage graph, parameter writes with the REAL clamp
 *      windows, and the telemetry JSON in the exact /t shape the ESP sends.
 *      No hardware needed:  node tools/panel_preview_server.js  ->  http://localhost:3000
 *      (binds 0.0.0.0 so it also works inside a sandboxed preview).
 * [FA] همان HTML واقعی پنل را از esp_link_panel.ino بیرون می‌کشد و STM32
 *      را پشت آن شبیه‌سازی می‌کند: یک چرخهٔ کامل شارژ
 *      خاموش → بالک → ابزورب → شناور → (افت) → بازگشت → بالک...،
 *      نمودار مراحل v1.14، ثبت پارامتر با همان پنجره‌های گیرهٔ واقعی، و
 *      JSON تله‌متری دقیقاً به شکل /t روی ESP. بدون سخت‌افزار:
 *      node tools/panel_preview_server.js  →  http://localhost:3000
 */

"use strict";

const http = require("http");
const fs = require("fs");
const path = require("path");

/* ---------- extract the real panel HTML from the .ino ---------- */
const inoPath = path.join(__dirname, "..", "esp_link_panel", "esp_link_panel.ino");
const ino = fs.readFileSync(inoPath, "utf8");
const html = ino.split('R"HTML(', 2)[1].split(')HTML";', 2)[0];

/* banner + auto-open the charge tab (injected into the served page ONLY) */
const inject = `<script>(function(){
var b=document.createElement('div');
b.style.cssText='position:fixed;top:0;left:0;right:0;z-index:99;background:#3a2b06;color:#ffd970;font:13px Vazirmatn,sans-serif;padding:6px 12px;text-align:center;border-bottom:1px solid #6b5206;direction:rtl';
b.textContent='پیش‌نمایش آفلاین — مقادیر از شبیه‌ساز STM32 می‌آیند، نه برد واقعی';
document.body.appendChild(b);document.body.style.paddingTop='32px';
var t2=document.querySelector('nav button[data-t="2"]');if(t2)t2.click();
})();</script></body></html>`;
const page = html.replace("</body></html>", inject);
if (page === html) page = html + inject; /* fallback: append */

/* ---------- simulated STM32 ---------- */
const P = [8, 8, 1046, 1303, 0, 0, 0, 3, 10, 0, 0, 1, 1, 500, 500, 0, 0, 0, 0, 0,
           14400, 14300, 14600, 13500, 12800, 650, 50];

function clampParam(id, v) {
    const a = P[20];
    const f = P[23];
    const im = P[25];
    switch (id) {
        case 20: return Math.min(14600, Math.max(11000, v));
        case 21: return Math.min(a - 50, Math.max(a - 500, v));
        case 22: return Math.min(Math.min(a + 400, 14750), Math.max(a + 100, v));
        case 23: return Math.min(a - 300, Math.max(9000, v));
        case 24: return Math.min(f - 300, Math.max(8000, v));
        case 25: return Math.min(900, Math.max(100, v));
        case 26: return Math.min(Math.min(300, im), Math.max(10, v));
        case 7:  return Math.min(15, Math.max(1, v));
        case 8:  return Math.min(300, Math.max(1, v));
        case 13: case 14: case 16: case 18: return Math.min(500, Math.max(0, v));
        case 15: case 17: case 19: return Math.min(1, Math.max(0, v));
        case 11: case 12: return Math.min(1, Math.max(0, v));
        case 0: case 1: return Math.min(255, Math.max(0, v));
        case 2: case 3: return Math.min(3000, Math.max(100, v));
        case 4: case 5: case 6: return Math.min(5000, Math.max(-5000, v));
        case 9: case 10: return Math.min(999, Math.max(0, v));
        default: return v;
    }
}

/* one battery channel: firmware-like state machine, demo time-scale */
function mkChannel() {
    return { state: 0, v: 12400, i: 0, duty: 0, soak: 0, taper: 0, off: 0, floatT: 0 };
}
const ch = [mkChannel(), mkChannel()];
let seq = 1, frames = 0;
const SIM_MS = 100;
let ms = 0;

function stepChannel(c) {
    const prof = { enter: P[21], absorb: P[20], float: P[23], reentry: P[24], imax: P[25], taper: P[26] };
    switch (c.state) {
        case 0: /* OFF: connection-stability gate (demo: 3 s) */
            c.i = 0; c.duty = 0;
            c.off += SIM_MS;
            if (c.off >= 3000) { c.state = 1; c.soak = 0; c.taper = 0; }
            break;
        case 1: /* BULK: constant current, voltage rising */
            c.duty = Math.min(c.duty + 2, 320);
            c.i = 620 + Math.round(25 * Math.sin(ms / 700));
            c.v += 80; /* demo-speed rise (~80 mV/s) */
            if (c.v >= prof.enter) { c.state = 2; c.soak = 0; c.taper = 0; }
            break;
        case 2: /* ABSORB: voltage hold at absorb, current tapering */
            c.duty = Math.max(30, c.duty - 4);
            c.v = prof.absorb + Math.round(14 * Math.sin(ms / 900));
            c.i = Math.max(prof.taper + 2, 400 - Math.floor(c.soak / 40)); /* smooth decay ~14 s */
            c.soak += SIM_MS;
            if (c.i < prof.taper) c.taper += SIM_MS; else c.taper = 0;
            if ((c.soak > 8000 && c.taper > 2500) || c.soak > 20000) { c.state = 3; c.floatT = 0; }
            break;
        case 3: /* FLOAT: hold float, then sag to reentry -> BULK (cycle) */
            c.duty = 40;
            c.floatT += SIM_MS;
            if (c.floatT < 2000) { c.v += (prof.float - c.v) * 0.12; c.i = 40; }
            else { c.v -= 45; c.i = 2; } /* demo-speed self-discharge */
            if (c.v <= prof.reentry) { c.state = 1; c.soak = 0; c.taper = 0; }
            break;
        default: c.state = 0;
    }
}

/* battery current -> ADC raw counts via the v1.13 power model (rough inverse) */
function rawFromCurrent(iBat, vMv) {
    const p = iBat * vMv / 1000; /* mW */
    let chainMa;
    if (p > 800) chainMa = p / 14.9;
    else if (p > 60) chainMa = p / 9.0;
    else chainMa = p / 3.4;
    return 8 + Math.round(chainMa / 1.144096);
}

let SNAP = null;

/* [EN] The sim runs on a REAL 100 ms ticker (like the STM32 measurement
 *      task), independent of browser polling; /t just snapshots it.
 * [FA] شبیه‌سازی روی تیکر واقعی ۱۰۰ms (مثل تسک اندازه‌گیری STM32) مستقل
 *      از poll مرورگر جلو می‌رود؛ /t فقط عکس لحظه را می‌دهد. */
setInterval(() => { SNAP = telemetry(); }, SIM_MS);

function telemetry() {
    stepChannel(ch[0]);
    stepChannel(ch[1]);
    const vlow = ch[1].v, vhigh = ch[0].v;
    const t = new Array(20).fill(0);
    for (let k = 0; k < 2; k++) {
        const b = k === 0 ? 0 : 7, c = ch[k];
        t[b + 0] = rawFromCurrent(c.i, k === 0 ? vhigh : vlow);
        t[b + 1] = Math.round(t[b + 0] * 8.7767);
        t[b + 2] = c.i;
        t[b + 3] = c.i;
        t[b + 4] = c.i;
        t[b + 5] = c.duty;
        t[b + 6] = c.state;
    }
    t[14] = 24100;                 /* input, one reading per run (bench rule) */
    t[15] = vlow + vhigh;          /* 24 V pack */
    t[16] = vlow + 150 + Math.round(0.47 * ch[1].i); /* V12 sense before comp */
    t[17] = vlow;                  /* battery low, compensated */
    t[18] = vhigh;                 /* battery high */
    t[19] = 0;                     /* fault mask */
    seq += 1; frames += 1; ms += SIM_MS;
    return { on: 1, age: 40, seq, fl: 7, n: frames, q: 0, ka: 800, t, p: P.slice() };
}

/* ---------- HTTP server ---------- */
const PORT = process.env.PORT ? Number(process.env.PORT) : 3000;
const server = http.createServer((req, res) => {
    const url = new URL(req.url, "http://x");
    const send = (code, type, body) => {
        res.writeHead(code, { "Content-Type": type, "Cache-Control": "no-store" });
        res.end(body);
    };
    if (req.method === "GET" && url.pathname === "/") {
        return send(200, "text/html; charset=utf-8", page);
    }
    if (req.method === "GET" && url.pathname === "/f.css") {
        return send(200, "text/css", "/* preview: system fonts */\n");
    }
    if (req.method === "GET" && url.pathname === "/t") {
        return send(200, "application/json", JSON.stringify(SNAP || telemetry()));
    }
    if (req.method === "POST" && url.pathname === "/s") {
        const id = Number(url.searchParams.get("id"));
        const v = Number(url.searchParams.get("v"));
        if (id >= 0 && id < 27 && Number.isFinite(v)) {
            P[id] = clampParam(id, v); /* clamped exactly like the firmware */
            if (id >= 20) {
                /* v1.14d: whole-set re-clamp in dependency order, like
                 * Charger_ClampProfile - so the preview zones move exactly
                 * as the real board's would after each write. */
                for (const pid of [20, 21, 22, 23, 24, 25, 26]) P[pid] = clampParam(pid, P[pid]);
            }
        }
        return send(200, "application/json", '{"_s":200}');
    }
    if (req.method === "POST" && url.pathname === "/m") {
        return send(200, "application/json", '{"_s":200}');
    }
    if (req.method === "GET" && url.pathname === "/m") {
        const s = new Array(20).fill(0), lo = new Array(20).fill(0),
              hi = new Array(20).fill(0), la = new Array(20).fill(0);
        const d = telemetry();
        for (let k = 0; k < 20; k++) { s[k] = d.t[k]; lo[k] = d.t[k]; hi[k] = d.t[k]; la[k] = d.t[k]; }
        return send(200, "application/json", JSON.stringify({ _s: 200, n: 1, s, lo, hi, la }));
    }
    if (req.method === "GET" && url.pathname === "/benchlog") {
        return send(200, "text/csv; charset=utf-8",
            "# [EN] offline preview: no bench log. / [FA] پیش‌نمایش آفلاین: فایل بنچ ندارد.\n");
    }
    if (req.method === "POST" && (url.pathname === "/benchlog/add" || url.pathname === "/benchlog/clear")) {
        return send(200, "application/json", '{"_s":200}');
    }
    return send(404, "text/plain", "not found");
});

server.listen(PORT, "0.0.0.0", () => {
    console.log(`panel preview: http://0.0.0.0:${PORT}  (charge tab: BULK->ABSORB->FLOAT->reentry cycle, ~1 min loop)`);
});
