/**
 * [EN] Behavioural test for the Persian label the history card prints for a
 *      row that carries the boot bit.
 *      Why a file of its own: the label is JavaScript inside a PROGMEM string,
 *      so neither the C++ host test nor the Python audit can call it - they can
 *      only see that the characters exist. v1.118 (audit item 3) changed what
 *      the boot bit MEANS (since v1.115 a plain switch-on writes no row), so
 *      the sentence had to follow the meaning, and the old sentence had to stay
 *      for files written by v1.114 and earlier.
 *      This reads the real function out of plink_panel.h and calls it with
 *      three rows. No jsdom needed: only the label logic is exercised.
 *      Run: node ESP_Calibration/Tester/host_test_panel_labels.js
 * [FA] تست برچسب فارسی ردیف‌های «روشن شدن» در کارت تاریخچه. برچسب داخل یک
 *      رشتهٔ PROGMEM است، پس نه تست C++ و نه ممیزی پایتون نمی‌توانند صدایش
 *      بزنند. اینجا خودِ تابع از plink_panel.h خوانده و با سه ردیف اجرا می‌شود.
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
    STFA: { 0: "خاموش", 1: "شارژ سریع" },
    ST: { 0: "خاموش", 1: "شارژ سریع" },
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

console.log("panel history labels / برچسب‌های تاریخچهٔ پنل");

/* 1. a real switch-on row from an OLD file (v1.114 and earlier) */
check(evtRowText(row({ chg: 1, note: 0 })).txt.indexOf("پنل روشن شد") === 0,
      "old file: the v1.114 switch-on row still reads 'پنل روشن شد'",
      evtRowText(row({ chg: 1, note: 0 })).txt);

/* 2. the update row: it is NOT a switch-on any more (v1.115) */
{
    const t = evtRowText(row({ chg: 1 | 4096, note: 0 })).txt;
    check(t.indexOf("پنل روشن شد") < 0,
          "update row: it no longer pretends the panel just came on", t);
    check(t.indexOf("بروزرسانی فرم‌ور") >= 0,
          "update row: it says the firmware was updated", t);
}

/* 3. the danger-band row: also not a switch-on */
{
    const t = evtRowText(row({ chg: 1, note: 1 })).txt;
    check(t.indexOf("پنل روشن شد") < 0,
          "danger row: it no longer pretends the panel just came on", t);
    check(t.indexOf("مرز قطع") >= 0,
          "danger row: it says the last run stopped at the cut line", t);
}

/* 4. an update row without the boot bit keeps its own sentence */
check(evtRowText(row({ chg: 4096, note: 0 })).txt.indexOf("بروزرسانی فرم‌ور") >= 0,
      "update-only row: still says the firmware was updated",
      evtRowText(row({ chg: 4096, note: 0 })).txt);

/* 5. v1.117 day-start marker */
check(evtRowText(row({ chg: 0, note: 7 })).txt.indexOf("ایجاد فایل روز") >= 0,
      "day start: the first row of a day file says the file was created",
      evtRowText(row({ chg: 0, note: 7 })).txt);

/* 6. v1.96 heartbeat, so the three labels above cannot quietly eat it */
check(evtRowText(row({ chg: 0, note: 5 })).txt.indexOf("گزارش دوره‌ای") >= 0,
      "heartbeat: the hourly row still says the panel is alive",
      evtRowText(row({ chg: 0, note: 5 })).txt);

console.log("");
if (failures > 0) {
    console.log(failures + " of " + checks + " PANEL LABEL CHECKS FAILED");
    process.exit(1);
}
console.log("ALL " + checks + " PANEL LABEL CHECKS PASSED");
