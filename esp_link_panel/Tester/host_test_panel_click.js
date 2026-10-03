/**
 * [EN] Behavioural test for the panel's click-to-edit operating table.
 *
 *      Every other check in this repository reads the panel as TEXT. That
 *      catches a missing field and it catches a renamed id, but it cannot
 *      catch an editor that opens and then gets wiped by the next telemetry
 *      tick, or an Escape key that saves, or a clamp the user never sees.
 *      Those are the failures this file exists for: it loads the real
 *      generated page in a DOM, clicks the real cells and asserts on what
 *      actually happens.
 *
 *      Optional by design. jsdom is not a dependency of this project and the
 *      firmware build must never need node, so a missing jsdom SKIPS (exit 0)
 *      with an instruction instead of failing. Run it with:
 *
 *          npm install --no-save jsdom
 *          node esp_link_panel/Tester/host_test_panel_click.js
 *
 * [FA] تست رفتاری جدول عملکردِ کلیک-و-ویرایش پنل.
 *
 *      هر چک دیگری در این مخزن پنل را به‌صورت «متن» می‌خواند. آن روش نبودِ یک
 *      فیلد یا تغییر نام یک شناسه را می‌گیرد، ولی نمی‌تواند ویرایشگری را بگیرد
 *      که باز می‌شود و تلمتری بعدی پاکش می‌کند، یا کلید Esc که به‌جای لغو ذخیره
 *      کند، یا گیره‌ای که کاربر هرگز نمی‌بیندش. این فایل برای همان خرابی‌هاست:
 *      صفحهٔ واقعی تولیدشده را در یک DOM بار می‌کند، روی خانه‌های واقعی کلیک
 *      می‌کند و روی آنچه واقعاً رخ می‌دهد ادعا می‌گذارد.
 *
 *      عمداً اختیاری است. jsdom وابستگی این پروژه نیست و بیلد فرم‌ور هرگز نباید
 *      به node نیاز پیدا کند، پس نبودن jsdom یعنی SKIP (خروج ۰) همراه با
 *      دستور نصب، نه شکست.
 */

"use strict";

const fs = require("fs");
const path = require("path");

let JSDOM;
try {
    ({ JSDOM } = require("jsdom"));
} catch (e) {
    console.log("SKIP: jsdom not installed.");
    console.log("      npm install --no-save jsdom");
    console.log("      node esp_link_panel/Tester/host_test_panel_click.js");
    process.exit(0);
}

const PAGE = path.join(__dirname, "..", "panel_preview.html");
let failures = 0;
let checks = 0;

function check(cond, what, detail) {
    checks++;
    if (cond) {
        console.log("  ok   " + what);
    } else {
        failures++;
        console.log("  FAIL " + what + (detail ? "  [" + detail + "]" : ""));
    }
}

/* [EN] The limit ids the user ordered to be panel-settable.
   [FA] شناسه‌هایی که کاربر دستور داد از پنل تنظیم‌شدنی باشند. */
const LIMIT_IDS = [];
for (let i = 93; i <= 107; i++) { LIMIT_IDS.push(i); }

function seedParams() {
    const p = {};
    for (let i = 0; i < 108; i++) { p[i] = 0; }
    Object.assign(p, {
        13: 500, 14: 500, 20: 14400, 21: 14300, 22: 14600, 23: 13500,
        24: 12800, 25: 650, 26: 50, 35: 950, 36: 14850,
        93: 3600000, 94: 100, 95: 600000, 96: 60000, 97: 8, 98: 700,
        99: 32, 100: 14800, 101: 100, 102: 500, 103: 10, 104: 15000,
        105: 3000, 106: 3000, 107: 500
    });
    return p;
}

function main() {
    const dom = new JSDOM(fs.readFileSync(PAGE, "utf8"), {
        runScripts: "dangerously", pretendToBeVisual: true,
        url: "http://localhost/"
    });
    const w = dom.window;
    const d = w.document;

    setTimeout(() => {
        const sent = [];
        /* [EN] Intercept the write path: this test is about the panel, not
                about the link. The board echo is simulated by writing the
                value straight back, which is what a real accepted SET_PARAM
                looks like from the page's point of view.
           [FA] مسیر نوشتن قطع می‌شود: موضوع این تست پنل است نه لینک. */
        w.send = (id, v) => { sent.push([id, v]); if (w.D && w.D.p) { w.D.p[id] = v; } };
        w.D = {
            p: seedParams(), t: new Array(25).fill(0),
            q: 0, q2: 0, q3: 0, q4: 0, fl: 0, on: 1
        };
        w.ctab();

        const cell = (i) => d.querySelector('#ctb .ev[data-i="' + i + '"]');
        const open = (i) => {
            cell(i).dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
            return d.querySelector("#ctb input.evi");
        };
        const key = (el, k) =>
            el.dispatchEvent(new w.KeyboardEvent("keydown", { key: k, bubbles: true }));

        console.log("panel click-to-edit behaviour");
        console.log("=".repeat(68));

        /* --- 1. every ordered id is actually on screen and clickable --- */
        for (const id of LIMIT_IDS) {
            check(cell(id) !== null,
                  "id " + id + " is drawn as an editable cell",
                  "the user ordered every limit settable from the panel");
        }

        /* --- 2. values are formatted for humans but edited as raw units --- */
        check(cell(93).textContent.trim() === "1 ساعت",
              "a millisecond value is shown in human units",
              cell(93).textContent);
        check(cell(20).textContent.trim() === "14.40 V",
              "a millivolt value is shown as volts", cell(20).textContent);

        let inp = open(93);
        check(inp !== null, "clicking a value opens an input in its place");
        check(inp.value === "3600000",
              "the input carries the RAW value, not the pretty one",
              "typing 1 into a field showing '1 hour' must not mean 1 ms");
        check(inp.getAttribute("min") === "0" &&
              inp.getAttribute("max") === "21600000",
              "the input advertises the firmware's own window",
              inp.getAttribute("min") + ".." + inp.getAttribute("max"));

        /* --- 3. Enter commits --- */
        inp.value = "0";
        key(inp, "Enter");
        check(sent.length === 1 && sent[0][0] === 93 && sent[0][1] === 0,
              "Enter sends the typed value", JSON.stringify(sent));
        check(cell(93).textContent.trim() === "بدون سقف",
              "zero renders as 'no ceiling', not as '0 ms'",
              cell(93).textContent);

        /* --- 4. Escape must not write. A cancel key that saves is worse
                  than no cancel key, because the user believes they backed
                  out of a safety limit. --- */
        const before = sent.length;
        const inp2 = open(99);
        inp2.value = "7";
        key(inp2, "Escape");
        check(sent.length === before, "Escape writes nothing");
        check(cell(99).textContent.trim() === "32",
              "Escape leaves the displayed value untouched",
              cell(99).textContent);

        /* --- 5. the regression this design is most likely to hit: the table
                  re-renders roughly once a second from telemetry, and a naive
                  implementation deletes the field the user is typing into. --- */
        const inp3 = open(97);
        w.ctab();
        w.ctab();
        const survivor = d.querySelector("#ctb input.evi");
        check(survivor !== null,
              "an open editor survives a telemetry re-render",
              "a ~1 s tick must not delete the field under the user's fingers");
        if (survivor) {
            survivor.value = "3";
            key(survivor, "Enter");
            check(sent[sent.length - 1][0] === 97 &&
                  sent[sent.length - 1][1] === 3,
                  "the survivor still commits correctly");
        }

        /* --- 6. a clamp must be visible. The board silently clamping while
                  the panel shows what you typed is how a safety ceiling ends
                  up believed-set and not set. --- */
        w.send = (id, v) => { sent.push([id, v]); w.D.p[id] = Math.min(v, 14800); };
        const inp4 = open(100);
        inp4.value = "20000";
        key(inp4, "Enter");
        const c100 = cell(100);
        check(c100.className.indexOf("cl") >= 0,
              "a clamped value is marked on screen");
        check(c100.textContent.trim() === "14.80 V",
              "the cell shows the APPLIED value, not the requested one",
              c100.textContent);
        check((c100.getAttribute("title") || "").indexOf("20000") >= 0,
              "the marker explains what was asked for",
              c100.getAttribute("title"));

        /* --- 7. the separate card is gone (user order) and the derived
                  read-only cell is not clickable --- */
        check(d.getElementById("q93") === null &&
              d.getElementById("q107") === null,
              "the old separate limits card is gone");
        const ro = d.querySelector("#ctb .ev.ro");
        check(ro !== null && ro.textContent.indexOf("%") >= 0,
              "the derived duty ceiling is shown read-only", ro && ro.textContent);
        const roSent = sent.length;
        if (ro) { ro.dispatchEvent(new w.MouseEvent("click", { bubbles: true })); }
        check(d.querySelector("#ctb input.evi") === null &&
              sent.length === roSent,
              "clicking the read-only cell opens nothing");

        console.log("=".repeat(68));
        if (failures === 0) {
            console.log("ALL " + checks + " PANEL CLICK TESTS PASSED");
        } else {
            console.log(failures + " of " + checks + " FAILED");
        }
        dom.window.close();
        process.exit(failures === 0 ? 0 : 1);
    }, 700);
}

main();
