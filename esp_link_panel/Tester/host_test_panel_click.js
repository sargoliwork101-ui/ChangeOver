/**
 * [EN] Behavioural test for the panel's click-to-edit values.
 *
 *      Every other check in this repository reads the panel as TEXT. That
 *      catches a missing field and it catches a renamed id, but it cannot
 *      catch an editor that opens and then gets wiped by the next telemetry
 *      tick, or an Escape key that saves, or a clamp the user never sees.
 *      Those are the failures this file exists for: it loads the real
 *      generated page in a DOM, clicks the real labels and asserts on what
 *      actually happens.
 *
 *      v1.30 (user order): the CHART is the editor and the operating table
 *      only reports. So this file also pins the direction of that split -
 *      a table that quietly became editable again would pass every grep.
 *
 *      Optional by design. jsdom is not a dependency of this project and the
 *      firmware build must never need node, so a missing jsdom SKIPS (exit 0)
 *      with an instruction instead of failing. Run it with:
 *
 *          npm install --no-save jsdom
 *          node esp_link_panel/Tester/host_test_panel_click.js
 *
 * [FA] تست رفتاری مقادیر کلیک-و-ویرایش پنل.
 *
 *      هر چک دیگری در این مخزن پنل را به‌صورت «متن» می‌خواند. آن روش نبودِ یک
 *      فیلد یا تغییر نام یک شناسه را می‌گیرد، ولی نمی‌تواند ویرایشگری را بگیرد
 *      که باز می‌شود و تلمتری بعدی پاکش می‌کند، یا کلید Esc که به‌جای لغو ذخیره
 *      کند، یا گیره‌ای که کاربر هرگز نمی‌بیندش.
 *
 *      نسخهٔ ۱.۳۰ (دستور کاربر): نمودار ویرایشگر است و جدول عملکرد فقط گزارش
 *      می‌دهد. پس این فایل جهتِ همان تقسیم را هم میخ می‌کند - جدولی که بی‌صدا
 *      دوباره ویرایش‌پذیر شود، از هر grep سالم رد می‌شود.
 *
 *      عمداً اختیاری است. jsdom وابستگی این پروژه نیست و بیلد فرم‌ور هرگز نباید
 *      به node نیاز پیدا کند، پس نبودن jsdom یعنی SKIP (خروج ۰).
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

/* [EN] The ids the user ordered to be settable from the panel, split by where
        the chart can honestly draw them.
   [FA] شناسه‌هایی که کاربر دستور داد از پنل تنظیم‌شدنی باشند، بر حسب اینکه
        نمودار کجا می‌تواند صادقانه رسمشان کند. */
const ON_VOLT_AXIS = [20, 21, 22, 23, 24, 36, 100];
const ON_CURR_AXIS = [25, 26, 35, 94];
const AS_CHIPS = [93, 95, 96, 97, 98, 99, 101, 102, 103, 104, 105, 106, 107];
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
        /* [EN] Intercept the write path: this test is about the panel, not the
                link. Echoing the value back is what an accepted SET_PARAM
                looks like from the page's point of view.
           [FA] مسیر نوشتن قطع می‌شود: موضوع این تست پنل است نه لینک. */
        w.send = (id, v) => { sent.push([id, v]); if (w.D && w.D.p) { w.D.p[id] = v; } };
        w.D = {
            p: seedParams(), t: new Array(25).fill(0),
            q: 0, q2: 0, q3: 0, q4: 0, fl: 0, on: 1
        };
        w.qgraph();
        w.ctab();

        const anchor = (i) => d.querySelector('[data-i="' + i + '"]');
        const pop = () => d.getElementById("evpop");
        const field = () => { const p = pop(); return p ? p.querySelector("input") : null; };
        const open = (i) => {
            anchor(i).dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
            return field();
        };
        const key = (el, k) =>
            el.dispatchEvent(new w.KeyboardEvent("keydown", { key: k, bubbles: true }));

        console.log("panel click-to-edit behaviour (chart edits, table reports)");
        console.log("=".repeat(68));

        /* --- 1. the user order: every limit still has an editor, and it is
                 on the chart card, not in the table --- */
        for (const id of LIMIT_IDS) {
            check(anchor(id) !== null,
                  "id " + id + " still has an editor somewhere",
                  "the standing order is that every limit stays settable");
        }
        for (const id of ON_VOLT_AXIS.concat(ON_CURR_AXIS)) {
            const el = d.querySelector('#qg [data-i="' + id + '"]');
            check(el !== null, "id " + id + " is drawn on the chart itself",
                  "it is a voltage or a current, so it belongs on an axis");
        }
        for (const id of AS_CHIPS) {
            check(d.querySelector('#qgc [data-i="' + id + '"]') !== null,
                  "id " + id + " is a chip under the chart",
                  "a time or a gain has no honest position on these axes");
        }

        /* --- 2. the table reports and nothing more (user order) --- */
        check(d.querySelectorAll("#ctb [data-i]").length === 0,
              "the operating table holds NO editable value",
              "one place to write means one place to be wrong");
        check(d.querySelectorAll("#ctb .evv").length > 0,
              "the operating table still shows the values");

        /* --- 3. the chart labels carry the number, not just a name --- */
        check(/14\.40V/.test(anchor(20).textContent),
              "a voltage label shows its value", anchor(20).textContent);
        check(/650mA/.test(anchor(25).textContent),
              "a current label shows its value", anchor(25).textContent);
        /* [EN] Reading the label text proves nothing here: it says "950mA"
                whether the line is drawn at 950 or pinned to the right edge
                by a too-small IMAX. Mutation testing caught exactly that -
                the assertion has to be about the POSITION.
           [FA] خواندن متن برچسب اینجا چیزی ثابت نمی‌کند: چه خط روی ۹۵۰ رسم
                شود چه IMAXِ کوچک آن را به لبهٔ راست بچسباند، متن همان
                «950mA» است. موتیشن‌تست دقیقاً همین را گرفت - ادعا باید دربارهٔ
                «جای» خط باشد. */
        check(/950mA/.test(anchor(35).textContent),
              "the hard fault label shows its value", anchor(35).textContent);
        const X1 = 742;   /* the plot's right edge, from qgraph() */
        const x35 = parseFloat(anchor(35).getAttribute("x"));
        const x25 = parseFloat(anchor(25).getAttribute("x"));
        check(x35 > x25,
              "950 mA sits to the right of the 650 mA bulk ceiling",
              "x35=" + x35 + " x25=" + x25);
        check(x35 <= X1 - 10,
              "the hard fault line is INSIDE the plot, not clipped to the edge",
              "x=" + x35 + " with the right edge at " + X1 +
              " - IMAX must cover every current threshold it draws");

        /* --- 4. clicking opens a floating editor with the RAW value --- */
        let inp = open(93);
        check(inp !== null, "clicking a value opens the editor");
        check(inp.value === "3600000",
              "the editor carries the RAW value, not the pretty one",
              "typing 1 into a field reading '1 hour' must not mean 1 ms");
        check(inp.getAttribute("min") === "0" &&
              inp.getAttribute("max") === "21600000",
              "the editor advertises the firmware's own window",
              inp.getAttribute("min") + ".." + inp.getAttribute("max"));

        /* --- 5. Enter commits --- */
        inp.value = "0";
        key(inp, "Enter");
        check(sent.length === 1 && sent[0][0] === 93 && sent[0][1] === 0,
              "Enter sends the typed value", JSON.stringify(sent));
        check(pop() === null, "committing closes the editor");
        check(anchor(93).textContent.indexOf("بدون سقف") >= 0,
              "zero renders as 'no ceiling', not as '0 ms'",
              anchor(93).textContent);

        /* --- 6. Escape must not write. A cancel key that saves is worse than
                 no cancel key: the user believes they backed out of changing
                 a safety limit. --- */
        const before = sent.length;
        const inp2 = open(99);
        inp2.value = "7";
        key(inp2, "Escape");
        check(sent.length === before, "Escape writes nothing");
        check(pop() === null, "Escape closes the editor");
        check(anchor(99).textContent.indexOf("32") >= 0,
              "Escape leaves the displayed value untouched",
              anchor(99).textContent);

        /* --- 7. what happens under an open editor when telemetry keeps
                 arriving. The field itself lives on <body>, so a redraw was
                 never going to delete it - mutation testing proved that
                 assertion was vacuous. The property that IS load-bearing is
                 that the page freezes while you type: the value you are
                 editing must not be rewritten under the open editor, and the
                 anchor you aimed at must not move. --- */
        const inp3 = open(97);
        w.D.p[97] = 42;                 /* the board reports something new */
        w.qgraph();
        w.ctab();
        const survivor = field();
        check(survivor !== null, "the editor is still open");
        check(survivor.value === "8",
              "what you typed is not overwritten by an incoming telemetry value",
              survivor.value);
        check(anchor(97).textContent.indexOf("42") < 0,
              "the frozen page does not swap the value under the open editor",
              anchor(97).textContent);
        if (survivor) {
            survivor.value = "3";
            key(survivor, "Enter");
            check(sent[sent.length - 1][0] === 97 &&
                  sent[sent.length - 1][1] === 3,
                  "committing after the freeze still sends the typed value");
            check(anchor(97).textContent.indexOf("3") >= 0,
                  "and the page resumes updating once the editor closes",
                  anchor(97).textContent);
        }

        /* --- 8. a clamp must be visible. The board clamping while the panel
                 keeps showing what was typed is how a ceiling ends up
                 believed-set and not set. --- */
        w.send = (id, v) => { sent.push([id, v]); w.D.p[id] = Math.min(v, 14800); };
        const inp4 = open(100);
        inp4.value = "20000";
        key(inp4, "Enter");
        const a100 = anchor(100);
        check(a100.textContent.indexOf("14.80") >= 0,
              "the label shows the APPLIED value, not the requested one",
              a100.textContent);
        check((a100.getAttribute("fill") || "") === "#f7c13c",
              "a clamped chart label is recoloured so the clamp is visible",
              a100.getAttribute("fill"));

        /* --- 9. the hard constant is not offered as a control --- */
        const cutoff = [...d.querySelectorAll("#qg text")]
            .filter(t => t.textContent.indexOf("قطع سخت") >= 0);
        check(cutoff.length === 1 && !cutoff[0].hasAttribute("data-i"),
              "the 15 V measurement ceiling stays read-only",
              "it is the board's valid-range limit, not a setting");

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
