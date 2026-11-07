/**
 * [EN] Behavioural test for the short English tags the history card prints in
 *      its event column.
 *      Why a file of its own: the labels are JavaScript inside a PROGMEM string,
 *      so neither the C++ host test nor the Python audit can call them - they can
 *      only see that the characters exist. v1.123 (user order: "replace every
 *      event state with one or two English words that keep the same meaning")
 *      turned the Persian sentences into tags, and this file is what keeps them
 *      honest: the boot bit still is not an update row, the tag stays short, and
 *      no tag may fall back to Persian.
 *      This reads the real function out of plink_panel.h and calls it with a
 *      handful of rows. No jsdom needed: only the label logic is exercised.
 *      Run: node ESP_Calibration/Tester/host_test_panel_labels.js
 * [FA] تست برچسب‌های کوتاهِ انگلیسیِ ستون «اتفاق» در کارت تاریخچه. برچسب‌ها داخل
 *      یک رشتهٔ PROGMEM هستند، پس نه تست C++ و نه ممیزی پایتون نمی‌توانند صدایشان
 *      بزنند. نسخهٔ ۱٫۱۲۳ (دستور کاربر) جمله‌های فارسی را به تگ انگلیسی تبدیل
 *      کرد و این فایل نگهبان آن است: تگ کوتاه بماند و هیچ‌وقت فارسی نشود.
 */
"use strict";
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const PANEL = path.join(__dirname, "..", "plink_panel.h");
const SOURCE = fs.readFileSync(PANEL, "utf8");

let checks = 0;
let failures = 0;
function check(cond, what, detail) {
    checks++;
    if (cond) {
        console.log("  ok   " + what);
    } else {
        failures++;
        console.log("  FAIL " + what + (detail ? "   [" + detail + "]" : ""));
    }
}

/* [EN] Cut the function out of the page with brace matching: a regex that
        stops at the first "}" would cut it in half. */
function extractFunction(name) {
    const start = SOURCE.indexOf("function " + name + "(");
    if (start < 0) {
        throw new Error("function " + name + " not found in plink_panel.h");
    }
    let depth = 0;
    let opened = false;
    for (let i = start; i < SOURCE.length; i++) {
        const c = SOURCE[i];
        if (c === "{") {
            depth++;
            opened = true;
        } else if (c === "}") {
            depth--;
            if (opened && depth === 0) {
                return SOURCE.slice(start, i + 1);
            }
        }
    }
    throw new Error("unbalanced braces around " + name);
}

const sandbox = {
    EVT_ST: ["off", "bulk", "absorb", "float", "bring-up", "jit wait",
             "input wait", "final fault", "no batt", "manual", "thermal"],
    ST: {},
    EVT_FL: [],
    EVT_FLAG: [],
    P: [],
    evtFaults: function () { return ""; },
    evtFlags: function () { return ""; },
    evtTime: function () { return "00:00:00"; },
};
vm.createContext(sandbox);
vm.runInContext(extractFunction("evtRowText"), sandbox);
const evtRowText = sandbox.evtRowText;

function row(over) {
    const base = {
        chg: 0, note: 0, mains: 1, c1: 0, c2: 0, fault: 0, flags2: 0, fl: 0,
        dm: 0, v24: 26400, vlow: 13200, vhigh: 13200, imbal: 0,
        vin: 24000, v12: 12000, i1: 0, i2: 0, d1: 0, d2: 0, pct: 80, epoch: 1794000000,
    };
    return Object.assign(base, over);
}

console.log("panel history tags / تگ‌های تاریخچهٔ پنل");

const FA_RANGE = /[\u0600-\u06FF]/;
function tag(over) { return evtRowText(row(over)).txt; }

/* 1. a switch-on row: one word, and NOT the update tag */
check(tag({ chg: 1, note: 0 }) === "Boot",
      "a switch-on row is tagged 'Boot'", tag({ chg: 1, note: 0 }));

/* 2. the update row is not a switch-on (v1.115 rule, kept by the tag) */
{
    const t = tag({ chg: 1 | 4096, note: 0 });
    check(t.indexOf("Boot") < 0, "update row: it is not tagged as a switch-on", t);
    check(t.indexOf("FW update") >= 0, "update row: it says the firmware was updated", t);
}

/* 3. the danger-band row is not a switch-on either */
{
    const t = tag({ chg: 1, note: 1 });
    check(t.indexOf("Boot") < 0, "danger row: it is not tagged as a switch-on", t);
    check(t.indexOf("Low-batt back") >= 0,
          "danger row: it says the last run stopped at the cut line", t);
}

/* 4. an update row without the boot bit keeps its own tag */
check(tag({ chg: 4096, note: 0 }).indexOf("FW update") >= 0,
      "update-only row: still says the firmware was updated", tag({ chg: 4096, note: 0 }));

/* 5. v1.117 day-start marker */
check(tag({ chg: 0, note: 7 }).indexOf("Day start") >= 0,
      "day start: the first row of a day file says the file was created",
      tag({ chg: 0, note: 7 }));

/* 6. v1.96 heartbeat */
check(tag({ chg: 0, note: 5 }).indexOf("Heartbeat") >= 0,
      "heartbeat: the periodic row still says the panel is alive",
      tag({ chg: 0, note: 5 }));

/* 7. v1.123 (user order): every state tag is ENGLISH, never a Persian sentence */
{
    const noisy = tag({ chg: 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | 512 | 1024 | 2048 | 4096,
                        note: 4, mains: 0, c1: 7, c2: 10, fault: 1023, flags2: 127,
                        fl: 63, dm: 3, v24: 20000 });
    check(!FA_RANGE.test(noisy),
          "every state tag is English, no Persian character left in the event column", noisy);
    const extra = [tag({ chg: 0, note: 6 }), tag({ chg: 0, note: 0 }),
                   tag({ chg: 2, note: 2, mains: 0 }),
                   tag({ chg: 128, note: 2 }), tag({ chg: 128, note: 0 })];
    check(extra.every(x => !FA_RANGE.test(x)),
          "and the rows with no change bits are English too", extra.join(" | "));
}

/* 8. the order said "one or two words": no tag may grow back into a sentence */
{
    const long = [];
    for (const rowTest of [{ chg: 1, note: 0 }, { chg: 2, mains: 1 }, { chg: 4, c1: 5 },
                           { chg: 16, fault: 1 }, { chg: 32, flags2: 2 },
                           { chg: 512, dm: 3 }, { chg: 256, fl: 33 }, { chg: 0, note: 4 }]) {
        long.push(tag(rowTest));
    }
    const worst = long.flatMap(x => x.split(" · ")).map(x => x.split(/\s+/).length).reduce((a, b) => Math.max(a, b), 0);
    check(worst <= 3,
          "every tag stays at one or two words (three with its colon prefix)",
          worst + " words: " + long.join(" | "));
}

/* 9. the charging phases are English in the log, and none falls back to #n */
{
    const out = [];
    for (let p = 0; p <= 10; p++) out.push(tag({ chg: 4, c1: p }));
    check(out.every(x => x.indexOf("#") < 0 && !FA_RANGE.test(x)),
          "all eleven charger phases have a short English name in the log",
          out.join(" | "));
    check(out[2].indexOf("absorb") >= 0,
          "phase 2 reads 'CH1 absorb', the same word the card shows", out[2]);
}

console.log("");
if (failures > 0) {
    console.log(failures + " of " + checks + " PANEL LABEL CHECKS FAILED");
    process.exit(1);
}
console.log("ALL " + checks + " PANEL LABEL CHECKS PASSED");
