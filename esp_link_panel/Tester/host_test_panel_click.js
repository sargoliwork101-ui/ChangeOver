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
 *      v1.30 (user order): the CHART is the editor. The report table that
 *      later sat beside it was deleted wholesale on a follow-up order
 *      (v1.36), so this file now also pins its ABSENCE - a table that quietly
 *      crept back would pass every grep.
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
/* [EN] v1.32 (user order: "I wanted to click ON the chart, not have you write
        it below it"): the chip strip is gone. These are annotated inside the
        SVG, next to the line each one acts on.
   [FA] نوار تراشه حذف شد. این‌ها داخل خود SVG و کنار همان خطی که رویش اثر
        می‌گذارند یادداشت می‌شوند. */
const ANNOTATED = [93, 95, 96, 97, 98, 99, 101, 102, 103, 104, 105, 106, 107];
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
        /* [EN] draw() rather than qgraph() directly: TAB defaults to 0, the
                chargers page, so this also proves the redraw is not gated on
                the settings tab. A mount that is never drawn is a container
                that is present, empty and permanently silent.
           [FA] به‌جای صدازدن مستقیم qgraph از draw استفاده می‌شود: TAB پیش‌فرض
                ۰ است یعنی صفحهٔ شارژرها، پس این ثابت می‌کند بازرسم به تب
                تنظیمات مشروط نیست. */
        w.draw(w.D);

        const anchor = (i) => d.querySelector('[data-i="' + i + '"]');
        const pop = () => d.getElementById("evpop");
        const field = () => { const p = pop(); return p ? p.querySelector("input") : null; };
        const open = (i) => {
            anchor(i).dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
            return field();
        };
        const key = (el, k) =>
            el.dispatchEvent(new w.KeyboardEvent("keydown", { key: k, bubbles: true }));
        const P0 = d.getElementById("p0");
        const P2 = d.getElementById("p2");

        /* --- 0. v1.36 (user follow-up order: the operating table was not
                 liked - delete it entirely). The chargers page carries just
                 the two charger cards now; the chart keeps its one home in
                 Settings > charge filter and PID.
                 The redraw gate narrowed with that move, so the chart is
                 only drawn
                 while that sub-tab is open - every chart assertion below has
                 to GO there rather than assume the landing page holds a
                 copy. Third reversal of this preference, so the assertions
                 pin placement and single-sourcing, never a head count. --- */
        const gotoChart = () => {
            [...d.querySelectorAll("nav button")][2].click();
            [...d.querySelectorAll("#sbt button")][0].click();
        };
        gotoChart();
        const CHART = d.getElementById("s0");
        check(P0.querySelectorAll(".qgm").length === 0,
              "the chart is gone from the chargers page, as ordered",
              "it was removed on the user's order; a mount here is a relapse");
        check(P0.querySelector("#ctb") === null,
              "the operating table is GONE from the panel, as ordered",
              "the user had it deleted wholesale; any table here is a relapse");
        const mounts = [...d.querySelectorAll(".qgm")];
        check(mounts.length === 1,
              "the chart has exactly one home, so no two copies can disagree");
        check(mounts.every(m => m.querySelectorAll("svg").length === 1),
              "the mount is actually drawn into",
              "a mount outside the redraw gate stays an empty box forever");
        check(CHART.contains(mounts[0]),
              "that home is the charge/filter/PID sub-tab of settings");
        /* v1.35 (user order: "write those times underneath so the charts do
           not get so crowded - do the same for the gains"). This demanded the
           chip strip stay DELETED. Inverted, not removed: the strip must now
           exist under EVERY chart and must actually render. */
        const SRC = require("fs").readFileSync(
            require("path").join(__dirname, "..", "plink_panel.h"), "utf8");
        const EVN = {};
        {
            const m = SRC.match(/const EVN=\{([\s\S]*?)\};/);
            if (m) for (const r of m[1].matchAll(/(\d+):'([^']+)'/g))
                EVN[+r[1]] = r[2];
        }
        const HELPS = [...SRC.matchAll(
            />!<span class="it">([\s\S]*?)<\/span><\/button>/g)]
            .map(x => x[1]).filter(h => h.includes("Taper sustain"));
        /* ---- v1.36 (user order): the summary tables were ordered deleted
           wholesale, and that absence is what this file now pins - a table
           that quietly crept back under a new name is the relapse to catch. */
        check(d.getElementById("ctb") === null && !/function ctab/.test(SRC),
              "the summary tables stay GONE - no mount, no renderer",
              "the user rejected the table outright; bringing one back is not a tidy-up, it is disobedience");
        const strips = [...d.querySelectorAll(".qgcm")];
        check(strips.length === mounts.length,
              "a chip strip is mounted under every chart");
        check(strips.every(x => x.querySelectorAll(".evc").length > 0),
              "every chip strip actually renders chips",
              "an empty strip is a setting the user can no longer reach");
        check(/querySelectorAll\('\.qgcm'\)/.test(SRC),
              "the chip strips are filled by class, not by id lookup",
              "an id lookup fills only the first and lets copies disagree");
        check(P2.querySelectorAll(".qgm svg").length === 1,
              "the settings tab carries the chart again, as asked");

        /* --- 0b. v1.33: the duplicate q20..q26 form is gone, so there is no
                 mirrored input to keep in step and no "typed but not applied"
                 state. What must still hold is that a chart edit reaches the
                 board - and, since v1.36, that nothing else shadows it. --- */
        for (let i = 20; i < 27; i++) {
            check(d.getElementById("q" + i) === null,
                  "no duplicate input box for id " + i,
                  "the chart is the only place these are set");
        }
        const lbl20 = CHART.querySelector('.qgm [data-i="20"]');
        lbl20.dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
        const i20 = field();
        i20.value = "14500";
        key(i20, "Enter");
        check(CHART.querySelector('.qgm [data-i="20"]')
               .textContent.indexOf("پیش‌نمایش") < 0,
              "no phantom 'preview' is announced after committing",
              "a dashed preview means a pending change; there is none");
        check(d.getElementById("ctb") === null,
              "and there is no table that could follow anything");
        w.D.p[20] = 14400;
        w.draw(w.D);

        console.log("panel click-to-edit behaviour (the chart edits; no table anywhere)");
        console.log("=".repeat(68));

        /* --- 1. the user order: every limit still has an editor, and it is
                 on the chart card --- */
        for (const id of LIMIT_IDS) {
            check(anchor(id) !== null,
                  "id " + id + " still has an editor somewhere",
                  "the standing order is that every limit stays settable");
        }
        for (const id of ON_VOLT_AXIS.concat(ON_CURR_AXIS)) {
            check(CHART.querySelector('.qgm [data-i="' + id + '"]') !== null,
                  "id " + id + " is drawn on the chart itself",
                  "it is a voltage or a current, so it belongs on an axis");
        }
        /* The off-axis values moved OUT of the SVG and into the chips. Each
           one must be reachable there, and must no longer be inside the plot
           - checking only the first half would let a duplicate survive in
           both places, which is the two-places-to-be-wrong bug again. */
        for (const id of ANNOTATED) {
            const chip = CHART.querySelector('.qgcm [data-i="' + id + '"]');
            check(chip !== null,
                  "id " + id + " is reachable as a chip under the chart",
                  "it is neither a voltage nor a current, so it has no axis");
            check(CHART.querySelector('.qgm svg [data-i="' + id + '"]') === null,
                  "id " + id + " no longer crowds the plot itself",
                  "left in both places it would be editable twice");
            const nm = EVN[id];
            check(nm && chip && chip.closest(".evc").textContent.includes(nm),
                  "chip " + id + " is labelled with its variable name " + nm);
        }
        /* the help behind "!" must define every one of those names */
        for (const id of ANNOTATED) {
            const nm = EVN[id];
            check(HELPS.every(h => h.includes(nm)),
                  "the ! help explains the name " + nm,
                  "user order: say what each variable name means");
        }

        /* --- 2. the table stays deleted and edits live on the chart --- */
        check(d.getElementById("ctb") === null,
              "the operating table stays deleted, as ordered",
              "one place to write means one place to be wrong - and that place is the chart");

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
        const last = sent[sent.length - 1];
        check(last && last[0] === 93 && last[1] === 0,
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
        const cutoff = [...CHART.querySelectorAll(".qgm text")]
            .filter(t => t.textContent.indexOf("Hard cutoff") >= 0);
        check(cutoff.length === 1 && !cutoff[0].hasAttribute("data-i"),
              "the 15 V measurement ceiling stays read-only",
              "it is the board's valid-range limit, not a setting");

        /* --- 10. v1.33: the current ceilings were opened so a parallel pack
                 can be charged. The bench LUTs are only fitted to ~631 mA, so
                 the chart must SHOW where measured data ends - otherwise the
                 freedom is real but silent, and a trip set at 2 A looks as
                 trustworthy as one set at 600 mA. --- */
        const edge = () => [...CHART.querySelectorAll(".qgm line")]
            .filter(l => l.getAttribute("stroke-dasharray") === "2 4");
        /* The first draft of this check asserted the edge is HIDDEN at
           default. Running it proved the opposite is the honest answer: the
           factory 950 mA trip has ALWAYS been past the 631 mA fitted edge,
           so hiding the line at default would hide a fact that was already
           true - it just had never been drawn. The assertion was corrected
           to match reality rather than the code being bent to match it.
           نسخهٔ اول این چک ادعا می‌کرد خط در حالت پیش‌فرض پنهان است. اجرا نشان
           داد عکسش صادقانه است: تریپ کارخانه‌ای ۹۵۰ همیشه بالای لبهٔ ۶۳۱ بوده.
           پس ادعا با واقعیت درست شد، نه کد با ادعا. */
        check(edge().length === 1,
              "the calibration edge is drawn even at the factory setting",
              "the default 950 mA trip already sits past the 631 mA fit");
        check(Number(edge()[0].getAttribute("x1")) <
              Number(CHART.querySelector('.qgm [data-i="35"]').getAttribute("x")),
              "the edge sits left of the factory trip");

        const lbl35 = CHART.querySelector('.qgm [data-i="35"]');
        lbl35.dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
        const i35 = field();
        check(Number(i35.max) >= 3000,
              "the hard-fault editor must accept a parallel-pack current",
              "max offered = " + i35.max);
        i35.value = "2400";
        key(i35, "Enter");
        check(sent.some(x => x[0] === 35 && x[1] === 2400),
              "a 2400 mA hard fault is actually sent",
              JSON.stringify(sent.slice(-1)));

        w.D.p[35] = 2400;
        w.draw(w.D);
        check(edge().length === 1,
              "the calibration edge appears once the axis passes fitted data",
              "going above the bench fit must be visible, not hidden");
        const xEdge = Number(edge()[0].getAttribute("x1"));
        /* v1.37: the Persian left labels can be displaced by the collision
           pass, and a displaced red label draws a small red CONNECTOR TICK -
           also a <line stroke="#ff6873">. The pin below must mean the trip
           LINE, so it now asks for the full-height vertical (y1=12), which
           only a threshold can be. */
        const xTrip = Number(
            [...CHART.querySelectorAll(".qgm line")]
                .filter(l => l.getAttribute("stroke") === "#ff6873" &&
                             l.getAttribute("y1") === "12")
                .map(l => Number(l.getAttribute("x1")))
                .filter(v => !isNaN(v)).pop());
        check(xEdge < xTrip,
              "the edge is drawn to the LEFT of a trip set beyond it",
              "x(edge)=" + xEdge + " x(trip)=" + xTrip);
        w.D.p[35] = 950;
        w.draw(w.D);

        /* ---- the chart must FOLLOW applied telemetry (the table that used
           to be cross-checked here was deleted wholesale on the user's order,
           so the still-live risk is a chart label going stale after an edit) */
        const chartLabel = id => {
            const el = CHART.querySelector('.qgm svg [data-i="' + id + '"]');
            return el ? el.textContent.trim() : "";
        };
        /* the gate only draws the chart while its sub-tab is open, and the
           tab walk above left us elsewhere - go back before redrawing, or
           the chart label would be read stale and the test would pass for
           the wrong reason */
        gotoChart();
        w.D.p[25] = 1200;
        w.D.p[35] = 2400;
        w.draw(w.D);
        check(chartLabel(25).includes("1200"),
              "editing the bulk current moves the chart label",
              "chart=" + chartLabel(25));
        check(chartLabel(35).includes("2400"),
              "editing the hard fault moves the chart label",
              "chart=" + chartLabel(35));
        w.D.p[25] = 650;
        w.D.p[35] = 950;
        w.draw(w.D);
        check(chartLabel(25).includes("650"),
              "and it follows the value back down again");

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
