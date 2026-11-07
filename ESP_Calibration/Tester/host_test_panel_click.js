/**
 * [EN] Behavioural test for the panel's click-to-edit values.
 *      Every other check in this repository reads the panel as TEXT. That
 *      catches a missing field and it catches a renamed id, but it cannot
 *      catch an editor that opens and then gets wiped by the next telemetry
 *      tick, or an Escape key that saves, or a clamp the user never sees.
 *      Those are the failures this file exists for: it loads the real
 *      generated page in a DOM, clicks the real labels and asserts on what
 *      actually happens.
 *      v1.30 (user order): the CHART is the editor. The report table that
 *      later sat beside it was deleted wholesale on a follow-up order
 *      (v1.36), so this file now also pins its ABSENCE - a table that quietly
 *      crept back would pass every grep.
 *      Optional by design. jsdom is not a dependency of this project and the
 *      firmware build must never need node, so a missing jsdom SKIPS (exit 0)
 *      with an instruction instead of failing. Run it with:
 *          npm install --no-save jsdom
 *          node ESP_Calibration/Tester/host_test_panel_click.js
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
    console.log("      node ESP_Calibration/Tester/host_test_panel_click.js");
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
/*
 * [EN] The ids the user ordered to be settable from the panel, split by where
 * the chart can honestly draw them.
 */
const ON_VOLT_AXIS = [20, 21, 22, 23, 24, 36, 100];
const ON_CURR_AXIS = [25, 26, 35, 94];
/*
 * [EN] v1.32 (user order: "I wanted to click ON the chart, not have you write
 * it below it"): the chip strip is gone. These are annotated inside the
 * SVG, next to the line each one acts on.
 */
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
        /*
 * [EN] Intercept the write path: this test is about the panel, not the
 * link. Echoing the value back is what an accepted SET_PARAM
 * looks like from the page's point of view.
 */
        const stub = (id, v) => { sent.push([id, v]); if (w.D && w.D.p) { w.D.p[id] = v; } };
        w.send = stub;
        /*
 * [EN] v1.52: edits are staged in the global queue instead of being
 * posted field by field, so the queue is the write path now.
 */
        w.qput = stub;
        w.D = {
            p: seedParams(), t: new Array(25).fill(0),
            q: 0, q2: 0, q3: 0, q4: 0, fl: 0, on: 1
        };
        /*
 * [EN] draw() rather than qgraph() directly: TAB defaults to 0, the
 * chargers page, so this also proves the redraw is not gated on
 * the settings tab. A mount that is never drawn is a container
 * that is present, empty and permanently silent.
 */
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
        w.qput = (id, v) => { sent.push([id, v]); w.D.p[id] = Math.min(v, 14800); };
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
        /*
 * The first draft of this check asserted the edge is HIDDEN at
 * default. Running it proved the opposite is the honest answer: the
 * factory 950 mA trip has ALWAYS been past the 631 mA fitted edge,
 * so hiding the line at default would hide a fact that was already
 * true - it just had never been drawn. The assertion was corrected
 * to match reality rather than the code being bent to match it.
 */
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
        /* --- 11. v1.85 (user order): a latched or decided error has to be
                 visible where the locks are, and clicking that lamp must open
                 the settings card which owns the error. The chips are rebuilt
                 by every poll, so each assertion clicks the chip from the
                 CURRENT draw instead of a reference captured earlier. --- */
        {
            w.scrollTo = () => {};
            /*
 * [EN] v1.94 (user order 2026-10-23): the always-on chips above the
 * table are gone; everything that is true right now lives in
 * bits keep their own clickable lamps next to it.
 */
            const lvChip = (txt) => [...d.querySelectorAll("#lv span")]
                .find(sp => sp.textContent.indexOf(txt) >= 0);
            const ledSet = (id) => {
                const el = d.getElementById(id);
                return !!el && el.className.indexOf("set") >= 0;
            };
            const activeTab = () => [...d.querySelectorAll("nav button")]
                .filter(b => b.classList.contains("a")).map(b => b.dataset.t).join();
            const activeSub = () => [...d.querySelectorAll("#sbt button")]
                .filter(b => b.classList.contains("a")).map(b => b.dataset.s).join();
            const shownCard = () => [...d.querySelectorAll("#usel button")]
                .filter(b => b.classList.contains("a")).map(b => b.dataset.u).join();
            /* a healthy board: the strip says so in words */
            w.D.fl = 2; w.D.fl2 = 0; w.D.t[28] = 0; w.D.t[19] = 0;
            w.D.t[6] = 1; w.D.t[13] = 3;
            w.draw(w.D);
            check(!!lvChip("برق شهر: وصل"),
                  "the live strip says the mains is present",
                  d.getElementById("lv") ? d.getElementById("lv").textContent : "no strip");
            check(!!lvChip("کانال ۱: شارژ اصلی") && !!lvChip("کانال ۲: شناور"),
                  "and shows each channel's charging phase in Persian",
                  d.getElementById("lv") ? d.getElementById("lv").textContent : "no strip");
            check(!!lvChip("مسیر باتری: وصل"),
                  "and says the battery path is connected");
            check(!lvChip("استراحت حرارتی") && !lvChip("حکم باتری خراب"),
                  "a healthy board shows no rest and no verdict");
            /* the two spare wire bits of byte 3 drive the two hardest states */
            w.D.fl2 = 32 | 64;
            w.draw(w.D);
            check(!!lvChip("مسیر باتری: قطع"),
                  "wire bit b5 turns the strip to «battery path cut»",
                  d.getElementById("lv") ? d.getElementById("lv").textContent : "no strip");
            check(!!lvChip("استراحت حرارتی"),
                  "wire bit b6 shows the thermal rest");
            w.D.fl2 = 0;
            /* a condemned battery: the LED lights AND the strip says it */
            w.D.t[28] = 1;
            w.draw(w.D); w.uview();
            check(ledSet("asbb9"),
                  "the condemned-battery lamp lights on the fault row");
            check(!!lvChip("حکم باتری خراب"),
                  "and the live strip carries the same verdict in words");
            const deadLed = d.getElementById("asbb9");
            if (deadLed) {
                deadLed.dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
                check(activeTab() === "2" && activeSub() === "1" && shownCard() === "6",
                      "clicking that lamp opens Settings > scenarios on card 6",
                      activeTab() + " | " + activeSub() + " | " + shownCard());
            }
            /* v1.98 (user order): the weak-battery verdict is a latch like the
               others, so it gets its own lamp in the fault row - not only a
               line in the log. */
            w.D.t[28] = 0; w.D.t[19] = 0; w.D.fl2 = 16;
            w.draw(w.D); w.uview();
            check(ledSet("asbb10"),
                  "the weak-battery verdict lights its own lamp in the fault row");
            check(!!lvChip("باتری ضعیف"),
                  "and the same verdict is written in words in the fault text");
            const weakLed = d.getElementById("asbb10");
            if (weakLed) {
                weakLed.dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
                check(activeTab() === "2" && activeSub() === "1" && shownCard() === "8",
                      "clicking it opens Settings > scenarios on card 8",
                      activeTab() + " | " + activeSub() + " | " + shownCard());
            }
            w.D.fl2 = 0;
            w.draw(w.D); w.uview();
            check(!ledSet("asbb10"),
                  "and the lamp goes out when the verdict is cleared");
            /* the charger power stage */
            w.D.t[28] = 0; w.D.t[19] = 128;
            w.draw(w.D); w.uview();
            check(ledSet("asbb7") && !!lvChip("خطای فنی برد"),
                  "the technical fault lights its lamp and its chip");
            const techLed = d.getElementById("asbb7");
            if (techLed) {
                techLed.dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
                check(shownCard() === "7", "and its click shows card 7", shownCard());
            }
            /* the weak-battery verdict keeps its own route into card 8 */
            w.D.t[19] = 0; w.D.fl2 = 16; w.D.p[157] = 1;
            w.draw(w.D);
            const weak = lvChip("حکم باتری ضعیف");
            check(!!weak && weak.className.indexOf("r") >= 0,
                  "fl2 bit 0x10 turns the weak-battery chip red",
                  weak ? weak.className : "no chip");
            check(!!weak && weak.getAttribute("onclick") === "errjmp(8)",
                  "and the chip carries its own route to scenario 8");
            const wbTxt = d.getElementById("wbst");
            check(!!wbTxt && wbTxt.textContent.indexOf("قفل است") >= 0 &&
                  wbTxt.className.indexOf("r") >= 0,
                  "the scenario-8 card states the latch in words, not just a colour",
                  wbTxt ? wbTxt.textContent : "no line");
            if (weak) {
                weak.dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
                check(shownCard() === "8", "and the chip opens card 8", shownCard());
            }
            w.D.fl2 = 0;
            /* a running imbalance episode is amber, the latch is a red lamp */
            w.D.t[19] = 0; w.D.fl2 = 1;
            w.draw(w.D); w.uview();
            const epi = lvChip("عدم‌توازن در جریان");
            check(!!epi && epi.className.indexOf("y") >= 0,
                  "a running imbalance episode is an amber state chip");
            w.D.fl2 = 2;
            w.draw(w.D); w.uview();
            check(ledSet("asbb8"),
                  "the permanent latch keeps its own always-lit red lamp");
            check(!lvChip("عدم‌توازن در جریان"),
                  "and the amber episode chip yields to the permanent lock");
            w.D.fl2 = 0; w.D.t[28] = 0;
            w.draw(w.D); w.uview();
            check(!ledSet("asbb8"),
                  "clearing the lock puts that lamp back out");
        }
        /*
 * [EN] Ctrl+Enter must run the same send-all as the button (spy on the global).
 */
        {
            let sendCalls = 0;
            const realSendAll = w.sendall;
            w.sendall = () => { sendCalls++; };
            d.body.dispatchEvent(new w.KeyboardEvent("keydown", { key: "Enter", ctrlKey: true, bubbles: true, cancelable: true }));
            check(sendCalls === 1, "Ctrl+Enter triggers send-all once");
            d.body.dispatchEvent(new w.KeyboardEvent("keydown", { key: "Enter", bubbles: true }));
            check(sendCalls === 1, "plain Enter does not trigger send-all");
            w.sendall = realSendAll;
        }
        /* --- 12. v1.86 (user order): every latched fault is written on ONE line
                 of the fault box, and the scenario-6 continuous-charge counters
                 (the numbers that decide "battery condemned") are shown live
                 beside it. --- */
        {
            w.D.fl2 = 0; w.D.t[28] = 0; w.D.t[19] = 0;
            w.D.t[29] = 45; w.D.t[30] = 0; w.D.p[125] = 3600000;
            w.draw(w.D);
            const asf = d.getElementById("asf"), dbcnt = d.getElementById("dbcnt");
            check(d.querySelectorAll("#asb5 .leds").length === 1,
                  "all fault lamps share a single LED row");
            check(!!asf && asf.textContent.indexOf("✅ بدون خطای قفل‌شده") >= 0,
                  "a clean fault box stays one short line",
                  asf ? asf.textContent : "no #asf");
            w.D.t[19] = 128 + 2 + 1;   /* bits 0, 1 and 7 */
            w.D.fl2 = 2;               /* permanent imbalance latch */
            w.D.t[28] = 3;             /* both batteries condemned */
            w.draw(w.D);
            const txt = asf ? asf.textContent : "";
            check(txt.indexOf("خطای ADC") >= 0 && txt.indexOf("اضافه‌جریان کانال ۱") >= 0 &&
                  txt.indexOf("خطای فنی برد") >= 0 && txt.indexOf("قفل عدم‌توازن") >= 0 &&
                  txt.indexOf("باتری خراب") >= 0,
                  "every latched fault is written in the box", txt);
            check(txt.indexOf("\n") < 0 && txt.indexOf(" · ") >= 0,
                  "and they all sit on one line, joined by a separator");
            check(txt.indexOf("عدم‌توازن در جریان") < 0,
                  "the episode note yields to the permanent latch");
            check(!!dbcnt && dbcnt.textContent.indexOf("باتری ۱") >= 0 &&
                  dbcnt.textContent.indexOf("باتری ۲") >= 0,
                  "the dead-battery counters are shown for both batteries",
                  dbcnt ? dbcnt.textContent : "no #dbcnt");
            check(!!dbcnt && dbcnt.textContent.indexOf("از") >= 0 &&
                  dbcnt.textContent.indexOf("حکم صادر شد") >= 0,
                  "each counter counts up to its deadline and the verdict is flagged");
            w.D.p[125] = 0;
            w.draw(w.D);
            check(!!dbcnt && dbcnt.textContent.indexOf("خاموش است") >= 0,
                  "a zero deadline switches the counter line off");
            w.D.p[125] = 3600000; w.D.t[19] = 0; w.D.fl2 = 0; w.D.t[28] = 0;
            w.draw(w.D);
        }
        /*
 * [EN] Styled dialogs: the panel's own modal is shown, Enter confirms, Escape cancels.
 * The modal opens one tick after the call (it queues behind any open dialog).
 */
        {
        /* --- v1.95 (user order): the log card at the bottom shows every state
                 and every fault, one line per event, newest first, and each
                 line carries its own colour (red fault, amber warning, green
                 recovery, grey plain change). --- */
        {
            const ep = 1792756800;
            const row = (chg, note, mains, fl, c1, c2, fault, f2, v) =>
                [ep, 12, chg, note, mains, fl, c1, c2, fault, f2, v, v - 40, v + 40, 20].join(",");
            const rows = () => [...w.document.querySelectorAll("#evtlist .evtrow")];
            const txt = (i2) => rows()[i2] ? rows()[i2].textContent : "";
            const tbl = () => w.document.querySelectorAll("#evtlist table.evttbl");
            const ths = () => w.document.querySelectorAll("#evtlist table.evttbl thead th");
            const cells = (i4) => rows()[i4] ? [...rows()[i4].querySelectorAll("td")] : [];
            w.evtRender([
                row(2, 0, 1, 0, 1, 3, 0, 0, 24000),          /* mains back */
                row(16, 0, 1, 0, 1, 3, 8, 0, 24000),         /* a fault */
                row(256, 0, 0, 32, 9, 9, 0, 0, 20600)        /* hand mode */
            ].join("\n"), "");
            check(rows().length === 3,
                  "the log card draws one line per saved event",
                  String(rows().length));
            check(rows()[0] && rows()[0].className.indexOf("lv2") >= 0,
                  "the newest line comes first (hand mode is the last event)",
                  rows()[0] ? rows()[0].className : "none");
            check(rows()[1] && rows()[1].className.indexOf("lv3") >= 0,
                  "a fault line is red",
                  rows()[1] ? rows()[1].className : "none");
            check(rows()[2] && rows()[2].className.indexOf("lv1") >= 0,
                  "a healthy board coming back is green",
                  rows()[2] ? rows()[2].className : "none");
            check(txt(1).indexOf("Fault") >= 0 && txt(1).indexOf("Batt V") >= 0,
                  "the red line names the fault in two English words, not just by colour", txt(1));
            check(cells(1)[2] && cells(1)[2].textContent.trim() === "23.96",
                  "and the lower battery voltage has its own cell",
                  cells(1)[2] ? cells(1)[2].textContent : "none");
            check(txt(0).indexOf("Manual on") >= 0,
                  "the hand-mode step is tagged in English", txt(0));
            check(txt(2).indexOf("Mains on") >= 0,
                  "a mains return is tagged in English too", txt(2));
            /* every charger phase has a Persian name - none may fall back to #n */
            const all = [];
            for (let s1 = 0; s1 <= 10; s1++) all.push(row(4, 0, 0, 0, s1, s1, 0, 0, 24000));
            w.evtRender(all.join("\n"), "");
            check(txt(0).indexOf("#") < 0 && txt(0).indexOf("thermal") >= 0,
                  "every charging phase has a short English name in the log",
                  txt(0));
            /* the condemned-battery verdict has its own column and its own bit */
            w.evtRender("1792756800,12,512,0,0,0,1,3,0,0,24000,23960,24040,20,1", "");
            check(rows()[0] && rows()[0].className.indexOf("lv3") >= 0 &&
                  txt(0).indexOf("Dead: CH1") >= 0,
                  "a condemned-battery verdict is a red line that names the battery",
                  txt(0));
            /*
 * [EN] v1.96: a row now carries the rails, both currents and both
 * duties, the pack voltage step is its own change bit, and a
 * quiet board writes a heartbeat line.
 */
            w.evtRender("1792756800,12,1024,0,1,2,1,3,0,0,24000,23960,24040,20,0,23000,12100,1200,1100,450,300", "");
            check(txt(0).indexOf("V step") >= 0,
                  "a voltage step is a tag of its own", txt(0));
            /* v1.123 (user order): one or two English words per state, and no
               Persian sentence may creep back into that column. */
            check(!/[\u0600-\u06FF]/.test(txt(0)),
                  "the event column holds only English tags", txt(0));
            /*
 * [EN] v1.122 (user order): the history is a TABLE, so two steps can be
 *      compared column by column instead of guessed from a sentence. Same
 *      colours, same theme; the event sentence keeps its own cell.
 */
            // v1.137: compact and legacy rows share the same nine-column view.
            {
                const compact="1792756800,12,1024,0,1,1,1,3,0,0,11900,12100,0,23000,1200,1100,450,300";
                const legacy="1792756800,12,1024,0,1,3,1,3,0,0,24000,11900,12100,200,0,23000,11900,1200,1100,450,300,37";
                const c=w.evtParse(compact),l=w.evtParse(legacy);
                check(JSON.stringify(w.evtCols(c))===JSON.stringify(w.evtCols(l)),
                      "compact and legacy rows show the same battery halves and currents");
                w.evtRender(compact,"v137");
                check(cells(0).length===9 && cells(0)[2].textContent.trim()==="11.90" && cells(0)[3].textContent.trim()==="12.10",
                      "the new row shows lower and upper battery side by side");
                w.evtRender(legacy+"\n"+compact,"mixed");
                check(rows().length===2 && cells(0)[2].textContent===cells(1)[2].textContent && cells(0)[3].textContent===cells(1)[3].textContent,
                      "an upgrade day can mix legacy and compact rows without shifting columns");
                check(![...ths()].some(h=>/پک|نیم|باتری ٪/.test(h.textContent)),
                      "removed pack, imbalance and percent columns are absent");
                check(!w.evtRowText(w.evtParse("1792756800,12,256,0,1,2,0,0,0,0,11900,12100,0,23000,0,0,0,0")).txt.match(/Flags off|Input on/),
                      "removed generic flags and input tags cannot reappear");
                w.evtRender("1792756800,12,1024,0,1,2,1,3,0,0,24000,23960,24040,20,0,23000,12100,1200,1100,450,300", "");
            }
            check(tbl().length === 1,
                  "the history card draws one table, not a stack of lines",
                  String(tbl().length));
            check(ths().length === 9,
                  "the table has one header per column", String(ths().length));
            check(rows()[0] && rows()[0].tagName === "TR" &&
                  cells(0).length === 9,
                  "every step is a row with one cell per column",
                  rows()[0] ? rows()[0].tagName + "/" + cells(0).length : "none");
            check(cells(0)[2] && cells(0)[2].textContent.trim() === "23.96",
                  "the lower battery voltage sits in its own column", cells(0)[2] ? cells(0)[2].textContent : "none");
            check(cells(0)[4] && cells(0)[4].textContent.trim() === "23.00",
                  "and the input rail in the next one", cells(0)[4] ? cells(0)[4].textContent : "none");
            check(cells(0)[5] && cells(0)[5].textContent.trim() === "1.20" &&
                  cells(0)[6] && cells(0)[6].textContent.trim() === "1.10",
                  "both battery currents are comparable cells",
                  (cells(0)[5] ? cells(0)[5].textContent : "?") + "/" + (cells(0)[6] ? cells(0)[6].textContent : "?"));
            check(cells(0)[7] && cells(0)[7].textContent.trim() === "45" &&
                  cells(0)[8] && cells(0)[8].textContent.trim() === "30",
                  "and both commands as a percentage",
                  (cells(0)[7] ? cells(0)[7].textContent : "?") + "/" + (cells(0)[8] ? cells(0)[8].textContent : "?"));
            /* v1.124 (user order): the tags stay one or two words, and the
               explanation lives in a tooltip on the tag itself. */
            {
                const spans = [...cells(0)[1].querySelectorAll(".ep")];
                check(spans.length >= 1 && spans.every(sp => (sp.getAttribute("title") || "").length > 3),
                      "every event tag carries its own explanation as a tooltip",
                      spans.map(sp => sp.textContent + "=" + (sp.getAttribute("title") || "")).join(" | "));
                check(spans.length >= 1 && spans.every(sp => sp.textContent.trim().split(/\s+/).length <= 3),
                      "and the tag itself stays one or two words",
                      spans.map(sp => sp.textContent.trim()).join(" | "));
                const heads = [...ths()];
                check(heads.length === 9 && heads.every(h2 => (h2.getAttribute("title") || "").length > 3),
                      "every column header explains itself on hover too",
                      heads.map(h2 => h2.textContent + "=" + (h2.getAttribute("title") || "")).join(" | "));
                const cardSx = d.querySelector('#evtlist') &&
                      d.querySelector('#evtlist').parentElement.querySelector('.sx');
                check(!!cardSx && cardSx.textContent.trim().split(/\s+/).length <= 20 &&
                      (cardSx.getAttribute("title") || "").length > 20,
                      "the card's own description is one short line, the details are on hover",
                      cardSx ? cardSx.textContent.trim() : "none");
            }
            check(cells(0)[1] && cells(0)[1].querySelectorAll(".ep").length >= 1,
                  "the event tags keep their own cell and their own spans",
                  cells(0)[1] ? cells(0)[1].textContent : "none");
            w.evtRender("1792756800,12,0,5,1,1,3,0,0,0,24000,23960,24040,20,0", "");
            check(txt(0).indexOf("Heartbeat") >= 0 && rows()[0].className.indexOf("lv0") >= 0,
                  "a heartbeat line says the panel is alive, in grey", txt(0));
            /* an old 15-column file still reads, without the new columns */
            w.evtRender("1792756800,12,2,0,1,1,3,0,0,0,24000,23960,24040,20,0", "");
            check(cells(0)[2] && cells(0)[2].textContent.trim() === "23.96" &&
                  cells(0)[4] && cells(0)[4].textContent.trim() === "\u2014",
                  "a day file written before v1.96 still opens, and its missing columns stay empty instead of inventing zeros",
                  (cells(0)[2] ? cells(0)[2].textContent : "?") + "/" + (cells(0)[4] ? cells(0)[4].textContent : "?"));
            /* an empty day says so instead of an empty box */
            w.evtRender("", "");
            check(w.document.getElementById("evtlist").textContent.indexOf("هنوز چیزی ثبت نشده") >= 0,
                  "an empty day says so in words");
            /* v1.99 (user order 2026-10-23): the day labels are Iranian (Jalali)
               dates, and each piece of a line sits in its own span with numbers
               and latin words bidi-isolated, so mixed text cannot tangle. */
            check(w.evtDayFa("20261023").indexOf("آبان") >= 0 &&
                  w.evtDayFa("20261023").indexOf("۱۴۰۵") >= 0,
                  "the day label is an Iranian (Shamsi) date", w.evtDayFa("20261023"));
            check(w.evtDaySh("20261023").indexOf("۱۴۰۵") >= 0 &&
                  w.evtDaySh("20261023").indexOf("-") >= 0,
                  "the download name carries the Iranian date too", w.evtDaySh("20261023"));
            w.evtRender("1792756800,12,2,0,1,1,3,0,0,0,24000,23960,24040,20,0,23000,12100,1200,1100,450,300", "");
            check(rows()[0].querySelectorAll(".ep").length >= 1 &&
                  cells(0).filter(td => td.textContent.trim() !== "" && td.textContent.trim() !== "\u2014").length >= 8,
                  "each message sits in its own span and each measurement in its own cell",
                  rows()[0].querySelectorAll(".ep").length + "/" +
                  cells(0).filter(td => td.textContent.trim() !== "" && td.textContent.trim() !== "\u2014").length);
            /*
 * [EN] v1.125 (user order): clicking a row must show, in phosphor, what changed
 *      against the step BELOW it (the older one) - two steps, one comparison.
 * [FA] نسخهٔ ۱٫۱۲۵ (دستور کاربر): کلیک روی هر ردیف باید با رنگ فسفری نشان دهد
 *      نسبت به پلهٔ پایین‌تر (قدیمی‌تر) چه چیزی عوض شده.
 */
            {
                const newer = "1792756800,12,1024,0,1,2,1,3,0,0,24000,23960,24040,80,0,23000,12100,1200,1100,450,300";
                const older = "1792756200,12,0,5,1,2,1,3,0,0,23800,23760,23840,80,0,23000,12100,1200,1100,450,300";
                w.evtRender(older + "\n" + newer, "");
                const sel = () => d.querySelectorAll("#evtlist tr.sel");
                const chg = (i5) => cells(i5).filter(td => td.classList.contains("chg")).length;
                const top = rows()[0];
                top.dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
                check(sel().length === 1 && top.classList.contains("sel"),
                      "clicking a row selects exactly that row", String(sel().length));
                check(cells(0)[2].classList.contains("chg"),
                      "a value that changed against the step below is highlighted",
                      cells(0)[2].className);
                check(cells(0)[4] && !cells(0)[4].classList.contains("chg"),
                      "a value that did NOT change keeps its normal colour",
                      cells(0)[4] ? cells(0)[4].className : "none");
                check((cells(0)[2].getAttribute("title") || "").indexOf("23.76") >= 0,
                      "the highlighted cell says what the older step had",
                      cells(0)[2].getAttribute("title") || "none");
                /* in this pair only the pack voltage and the battery percent
                   it drives are different: 24.00/23.80, and the same 80 mV
                   imbalance in both steps must NOT light up. */
                check(chg(0) === 2 && cells(0)[2].classList.contains("chg") &&
                      cells(0)[3].classList.contains("chg") &&
                      !cells(0)[4].classList.contains("chg"),
                      "exactly the two battery cells light up, the unchanged input does not",
                      String(chg(0)) + " @ " + cells(0).map(td => td.classList.contains("chg") ? "x" : ".").join(""));
                /* the oldest row has nothing below it: it must not throw or paint */
                rows()[1].dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
                check(sel().length === 1 && rows()[1].classList.contains("sel"),
                      "clicking the oldest row moves the selection to it");
                check(chg(0) === 0 && chg(1) === 0,
                      "and paints nothing, because there is no older step to compare with",
                      chg(0) + "/" + chg(1));
                /*
 * [EN] v1.126 (audit finding): restoring a cell used to write plain text into
 *      it, so the bidi span the page wrapped around every number was destroyed
 *      by the first click and never came back - three spans lost per click.
 *      A selected cell must return to exactly the markup it had.
 * [FA] نسخهٔ ۱٫۱۲۶ (یافتهٔ ممیزی): بازگرداندن خانه با متن ساده انجام می‌شد و
 *      span دوسویه‌ای که دور هر عدد بود را برای همیشه پاک می‌کرد. خانهٔ آزادشده
 *      باید دقیقاً همان مارک‌آپ اولش را پس بگیرد.
 */
                {
                    const pairNew = "1792756800,12,1024,0,1,2,1,3,0,0,24000,23960,24040,20,0,23000,12100,1200,1100,450,300";
                    const pairOld = "1792756200,12,0,5,1,2,1,3,0,0,23800,23760,23840,20,0,23000,12100,1200,1100,450,300";
                    w.evtRender(pairOld + "\n" + pairNew, "");
                    const numericLt = () => rows().reduce((n, r2) => n + [...r2.querySelectorAll("td")].slice(2)
                        .reduce((m, td) => m + td.querySelectorAll(".lt").length, 0), 0);
                    const fresh = numericLt();
                    rows()[0].dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
                    const hot = cells(0)[2];
                    check(hot.classList.contains("chg") && hot.innerHTML.indexOf('class="lt"') >= 0,
                          "a highlighted cell keeps its bidi span around the number",
                          hot.innerHTML);
                    rows()[1].dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
                    check(numericLt() === fresh,
                          "releasing the row gives every bidi span back",
                          fresh + " -> " + numericLt());
                    check(cells(0)[2].innerHTML.indexOf('class="lt"') >= 0 &&
                          cells(0)[2].textContent.trim() === "23.96",
                          "and the released cell is the original markup and the original value again",
                          cells(0)[2].innerHTML + " / " + cells(0)[2].textContent.trim());
                }
                const css2 = [...d.querySelectorAll("style")].map(s2 => s2.textContent).join("\n");
                check(/\.evttbl[^{]*td\.chg\{[^}]*#a3ff3c/i.test(css2) ||
                      /\.evttbl[^{]*td\.chg\{[^}]*#c6ff3c/i.test(css2),
                      "the highlight colour is the phosphor green the owner asked for",
                      (css2.match(/td\.chg\{[^}]*}/) || [""])[0]);
            }
            check(rows()[0].querySelectorAll(".lt").length >= 9,
                  "numbers and latin words run inside their own left-to-right spans",
                  String(rows()[0].querySelectorAll(".lt").length));
        }
        /*
 * [EN] v1.121 (user report 2026-11-03: "the morning rows are gone"): the card
 *      only ever showed the last 30 rows the board serves, so a day with more
 *      rows looked as if everything before the first visible hour had been
 *      deleted. The whole day file is now paged 30 rows at a time, and the
 *      "all rows" key can draw the day in one go. Nothing here is a firmware
 *      change: the page reads the same /evt/dl file the download key saves.
 */
        {
            const ep2 = 1792756800;
            const dayRows = [];
            for (let i2 = 0; i2 < 75; i2++) {
                dayRows.push([ep2 + i2 * 600, 12 + i2, 0, 5, 1, 1, 3, 0, 0, 0,
                              24000, 23960, 24040, 20, 0].join(","));
            }
            const pRows = () => [...w.document.querySelectorAll("#evtlist .evtrow")];
            const pTxt = (i3) => pRows()[i3] ? pRows()[i3].textContent : "";
            const pv = d.getElementById("evtpv");
            const nx = d.getElementById("evtnx");
            const al = d.getElementById("evtal");
            check(!!pv && !!nx && !!al && pv.getAttribute("title").indexOf("قدیمی") >= 0 &&
                  al.textContent.indexOf("همه") >= 0,
                  "the history card carries a previous-page key and a show-all key");
            w.evtDayLoad(dayRows.join("\n"), "");
            check(pRows().length === 30, "the day view draws one page of 30 rows",
                  String(pRows().length));
            check(d.getElementById("evtst").textContent.indexOf("۷۵") >= 0,
                  "the status line says how many rows the WHOLE day has",
                  d.getElementById("evtst").textContent);
            const newest = pTxt(0);
            check(w.evtPageOlder() === true && pRows().length === 30,
                  "the previous-page key moves back one page", String(pRows().length));
            check(pTxt(0) !== newest, "and the rows on screen really changed", pTxt(0));
            w.evtPageOlder();
            check(w.evtPageOlder() === false && pRows().length === 30,
                  "the oldest page stops at the first row of the file",
                  String(pRows().length));
            check(pv.disabled === true,
                  "the previous-page key goes grey on the first page");
            check(w.evtShowAll() === true && pRows().length === 75,
                  "show-all draws every row of the day", String(pRows().length));
            check(w.evtPageNewer() === true && pRows().length === 30,
                  "the next-page key returns to the last page", String(pRows().length));
            /* [EN] v1.129 audit: a key that cannot move must leave the view exactly
                    as it was. The old code cleared evtAll first and then returned
                    false, so the card said "one page" while the screen still showed
                    the whole day - the next press could then undo a page for no
                    reason. Reached the way a reader reaches it: a day is read
                    (last page), he presses show-all, then presses the next key.
               [FA] ممیزی ۱٫۱۲۹: کلیدی که نمی‌تواند تکان بخورد باید نما را
                    دست‌نخورده بگذارد. کد قدیم اول evtAll را پاک می‌کرد و بعد false
                    برمی‌گرداند؛ یعنی کارت می‌گفت «یک صفحه» ولی صفحه هنوز کل روز را
                    نشان می‌داد. همان‌طور که کاربر می‌رسد سنجیده می‌شود: روز خوانده
                    می‌شود (صفحهٔ آخر)، «همهٔ ردیف‌ها» زده می‌شود، بعد کلید بعدی. */
            w.evtDayLoad(dayRows.join("\n"), "");
            w.evtShowAll();
            const allRows = pRows().length, allLine = d.getElementById("evtst").textContent;
            check(allRows === 75 && allLine.indexOf("۷۵") >= 0,
                  "the whole-day view really holds all 75 rows", allRows + " / " + allLine);
            check(w.evtPageNewer() === true && pRows().length === 30,
                  "from the whole-day view the next key gives a page back, it does not do nothing",
                  String(pRows().length));
            check(d.getElementById("evtst").textContent.indexOf("ردیف") >= 0,
                  "and the status line agrees: the card is a page again",
                  d.getElementById("evtst").textContent);
            check(w.evtPageNewer() === false && pRows().length === 30,
                  "a second press, which cannot move, leaves the screen alone",
                  String(pRows().length));
            w.evtShowAll();
            check(w.evtPageOlder() === true && pRows().length === 30,
                  "the previous key also leaves the whole-day view for a page",
                  String(pRows().length));
            w.evtRender(dayRows.slice(0, 3).join("\n"), "");
            check(pv.disabled === false && pRows().length === 3,
                  "a fresh tail view leaves the previous-page key usable",
                  String(pRows().length));
            /* v1.123 (user order): the keys are centred between the rows, and
               the two page keys are arrows (>> and <<) instead of sentences. */
            const pg = d.getElementById("evtpage");
            check(!!pg && pg.querySelectorAll("button").length === 3,
                  "the three page keys live in their own row");
            check(d.getElementById("evtpv").textContent.trim() === "<<" &&
                  d.getElementById("evtnx").textContent.trim() === ">>",
                  "the page keys are the two arrows, the way the owner reads them (v1.125 swapped the glyphs)",
                  d.getElementById("evtpv").textContent.trim() + "/" +
                  d.getElementById("evtnx").textContent.trim());
            check(d.getElementById("evtal").textContent.indexOf("همه") >= 0,
                  "the all-rows key stays between them, in words",
                  d.getElementById("evtal").textContent);
            check(d.getElementById("evtpv").getAttribute("title").indexOf("۳۰") >= 0 &&
                  d.getElementById("evtnx").getAttribute("title").indexOf("۳۰") >= 0,
                  "each arrow carries a Persian tooltip that says what it does",
                  d.getElementById("evtpv").getAttribute("title") + " / " +
                  d.getElementById("evtnx").getAttribute("title"));
            const css = [...d.querySelectorAll("style")].map(s2 => s2.textContent).join("\n");
            const row = (css.match(/#evtpage\{[^}]*}/) || [""])[0].replace(/\s+/g, "");
            check(row.indexOf("display:flex") >= 0 &&
                  row.indexOf("justify-content:center") >= 0 &&
                  row.indexOf("flex-wrap:wrap") >= 0,
                  "the key row is centred and wraps when the panel gets narrow", row);
            check(row.indexOf("33.333%") < 0,
                  "the old thirds rule is gone, the row is centred now", row);
        }
        /*
 * [EN] v1.126: the "N rows were lost" warning must survive the v1.121 move of
 *      the status line into the table renderer - it is the only place the board
 *      admits it could not store a row. The fetch is asynchronous, so this is a
 *      promise the final block waits for before it prints the verdict.
 * [FA] نسخهٔ ۱٫۱۲۶: هشدار «N ردیف از دست رفت» باید بعد از انتقال خط وضعیت به
 *      رندر جدول هم زنده بماند؛ تنها جایی که برد اعتراف می‌کند ردیفی را ذخیره
 *      نکرده همین است. گرفتن داده ناهمگام است، پس این یک Promise است و بلوک
 *      پایانی پیش از چاپ نتیجه منتظرش می‌ماند.
 */
        const lostCheck = (async () => {
            const savedHttp = w.evtHttp;
            w.evtHttp = async () => ({
                ok: true,
                headers: { get: (h) => (h === "X-Log-Lost" ? "4" : (h === "X-Log-Fs" ? "1" : null)) },
                text: async () => "1792756800,12,0,5,1,1,3,0,0,0,24000,23960,24040,20,0"
            });
            await w.evtfetch(0);
            const line = d.getElementById("evtst").textContent;
            check(line.indexOf("از دست رفت") >= 0 && line.indexOf("۴") >= 0,
                  "rows the board could not store are still counted in the status line", line);
            w.evtHttp = savedHttp;
        })();
        /*
 * [EN] v1.127 (user report 2026-11-04: "I click a log row, or page back 30
 *      rows, and every few seconds it jumps away as if it refreshed"). The
 *      10 s tick refetched /evt - which is a 30-row TAIL, never the whole day -
 *      and repainted the card, so the page the reader had turned to, the row he
 *      had picked and the phosphor cells were all thrown away. These checks
 *      pin the three things that must stay true afterwards: the reader's place
 *      survives the tick, the day list still refreshes while it does, and the
 *      plain tail view still takes a genuinely new row.
 *      It runs AFTER the lost-row check, because both drive the same fetch
 *      helper and must not fight over the stub.
 * [FA] نسخهٔ ۱٫۱۲۷ (گزارش کاربر): تیک ۱۰ ثانیه‌ای دوباره /evt را می‌گرفت -
 *      که فقط ۳۰ ردیف آخر است - و کارت را از نو می‌ساخت؛ صفحه‌ای که کاربر
 *      ورق زده بود، ردیف انتخاب‌شده و خانه‌های فسفری از دست می‌رفت. این
 *      بررسی‌ها سه چیز را قفل می‌کنند: جای کاربر در تیک می‌ماند، لیست روزها
 *      همچنان تازه می‌شود، و نمای پیش‌فرض ردیف تازه را می‌گیرد. بعد از بررسی
 *      ردیف‌های ازدست‌رفته اجرا می‌شود تا دو استاب با هم نجنگند.
 */
        const tickCheck = lostCheck.then(async () => {
            const pRows3 = () => [...d.querySelectorAll("#evtlist .evtrow")];
            const stTxt = () => d.getElementById("evtst").textContent;
            const selN = () => d.querySelectorAll("#evtlist tr.sel").length;
            const chgN = () => d.querySelectorAll("#evtlist td.chg").length;
            const ep3 = 1792756800;
            const many = [];
            for (let i4 = 0; i4 < 75; i4++) {
                many.push([ep3 + i4 * 600, 12 + i4, 0, 5, 1, 1, 3, 0, 0, 0,
                           24000, 23960, 24040, 20, 0].join(","));
            }
            const pairOld = "1792756200,12,0,5,1,2,1,3,0,0,23800,23760,23840,20,0,23000,12100,1200,1100,450,300";
            const pairNew = "1792756800,12,1024,0,1,2,1,3,0,0,24000,23960,24040,20,0,23000,12100,1200,1100,450,300";
            const savedHttp = w.evtHttp;
            const savedDays = w.evtDays;
            let daysCalls = 0;
            let fetches = 0;
            /* the board's usual answer: the last 30 rows, nothing more */
            let body = "";
            w.evtDays = async () => { daysCalls++; };
            w.evtHttp = async () => {
                fetches++;
                return {
                    ok: true,
                    headers: { get: (h) => (h === "X-Log-Fs" ? "1" : (h === "X-Log-Lost" ? "0" : null)) },
                    text: async () => body
                };
            };
            /* 1) the reader turned back a page: the tick must not take it away */
            body = many.slice(-30).join("\n");
            w.evtDayLoad(many.join("\n"), "");
            w.evtPageOlder();
            const pText = pRows3()[0].textContent;
            const pLine = stTxt();
            const d0 = daysCalls, f0 = fetches;
            await w.evtTick();
            check(pRows3().length === 30 && pRows3()[0].textContent === pText &&
                  stTxt() === pLine && pLine.indexOf("از ۷۵") >= 0,
                  "the 10 s tick leaves the page the reader turned to exactly where it was",
                  ((pRows3()[0] || { textContent: "no rows" }).textContent.slice(0, 24)) + " / " + stTxt());
            check(daysCalls > d0 && fetches === f0,
                  "and it keeps the day list fresh without refetching the tail at all",
                  "days+" + (daysCalls - d0) + " fetches+" + (fetches - f0));
            /* 2) a picked row keeps its selection and its phosphor cells */
            w.evtRender(pairOld + "\n" + pairNew, "");
            pRows3()[0].dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
            const keptSel = selN(), keptChg = chgN();
            /* the board has nothing new to say, so this tick must not redraw at
               all; the repaint path is covered by the liveCheck block below */
            body = pairOld + "\n" + pairNew;
            await w.evtTick();
            check(keptSel === 1 && selN() === 1 && keptChg > 0 && chgN() === keptChg,
                  "a picked row keeps its selection and its phosphor cells when the tick brings nothing new",
                  selN() + " sel / " + chgN() + " chg");
            /* 3) the plain tail view still refreshes, but an identical answer is
                  not repainted (that redraw is what looked like a page refresh) */
            body = pairOld;
            await w.evtfetch(0); /* the tail on screen, drawn from a clean status line */
            const table0 = d.querySelector("#evtlist table");
            const f1 = fetches;
            await w.evtTick();
            check(fetches === f1 + 1, "in the plain tail view the tick still asks the board",
                  "fetches+" + (fetches - f1));
            check(d.querySelector("#evtlist table") === table0,
                  "and an answer identical to what is on screen is not redrawn",
                  table0 === d.querySelector("#evtlist table") ? "same node" : "rebuilt");
            /* 4) but a genuinely new row must still land - and, since v1.129,
                  it lands as ONE inserted row, not as a rebuilt table */
            body = pairOld + "\n" + pairNew;
            await w.evtTick();
            check(pRows3().length === 2 && pRows3()[0].textContent.indexOf("23.96") >= 0,
                  "a new row still reaches the tail view on the next tick",
                  String(pRows3().length));
            check(d.querySelector("#evtlist table") === table0 && pRows3()[0].dataset.k === pairNew,
                  "and it arrives by inserting that one row into the table that was already there",
                  pRows3()[0].dataset.k === pairNew ? "row inserted" : "unexpected key");
            w.evtHttp = savedHttp;
            w.evtDays = savedDays;
        });
        /*
 * [EN] v1.127 audit (the fix itself put under the same question the owner
 *      asked): stopping the tick while a row is picked would cure the jump by
 *      freezing the card forever, and nothing on screen can un-pick a row. The
 *      reader's state therefore has to SURVIVE a repaint, not block it: a new
 *      row must still arrive, the picked row must keep its selection and its
 *      phosphor cells, and a second click must release it.
 * [FA] ممیزی نسخهٔ ۱٫۱۲۷ (همان پرسشی که کاربر پرسید، این بار روی خودِ درمان):
 *      متوقف‌کردن تیک وقتی ردیفی انتخاب شده، «پریدن» را با «قفل‌شدن همیشگی»
 *      عوض می‌کند و هیچ‌جای صفحه هم ردیف را آزاد نمی‌کند. پس حالت کاربر باید
 *      از بازکشیدن جان سالم ببرد، نه آن را ممنوع کند: ردیف تازه باید برسد،
 *      ردیف انتخاب‌شده انتخاب و خانه‌های فسفری‌اش را نگه دارد، و کلیک دوم
 *      آزادش کند.
 */
        const liveCheck = tickCheck.then(async () => {
            const pRows4 = () => [...d.querySelectorAll("#evtlist .evtrow")];
            const selN4 = () => d.querySelectorAll("#evtlist tr.sel").length;
            const chgN4 = () => d.querySelectorAll("#evtlist td.chg").length;
            const savedHttp = w.evtHttp;
            const savedDays = w.evtDays;
            let fetches = 0;
            let body = "";
            w.evtDays = async () => {};
            w.evtHttp = async () => {
                fetches++;
                return {
                    ok: true,
                    headers: { get: (h) => (h === "X-Log-Fs" ? "1" : (h === "X-Log-Lost" ? "0" : null)) },
                    text: async () => body
                };
            };
            const o = "1792756200,12,0,5,1,2,1,3,0,0,23800,23760,23840,20,0,23000,12100,1200,1100,450,300";
            const n1 = "1792756800,12,1024,0,1,2,1,3,0,0,24000,23960,24040,20,0,23000,12100,1200,1100,450,300";
            const n2 = "1792757400,14,1024,0,1,2,1,3,0,0,24300,24260,24340,20,0,23000,12100,1200,1100,450,300";
            body = o + "\n" + n1;
            await w.evtfetch(0);
            pRows4()[0].dispatchEvent(new w.MouseEvent("click", { bubbles: true }));
            const pickedSel = selN4(), pickedChg = chgN4();
            const pickedKey = pRows4()[0].dataset.k || "";
            /* 1) a new row arrives while a row is picked */
            body = o + "\n" + n1 + "\n" + n2;
            await w.evtTick();
            check(pRows4().length === 3 && pRows4()[0].textContent.indexOf("24.26") >= 0,
                  "a picked row does not stop the live tail: the new row still arrives",
                  pRows4().length + " rows / " + pRows4()[0].textContent.slice(0, 22));
            /* 2) and the picked row kept its selection and its phosphor cells */
            const still = pRows4().find(r => r.dataset.k === pickedKey);
            check(!!still && still.classList.contains("sel") && selN4() === pickedSel &&
                  chgN4() === pickedChg,
                  "the picked row survives the repaint with its selection and its phosphor cells",
                  (still ? still.dataset.k : "row gone") + " / " + selN4() + " sel / " + chgN4() + " chg");
            check(!!still && still.querySelectorAll("td.chg").length > 0 &&
                  [...still.querySelectorAll("td.chg")].every(td => td.innerHTML.indexOf('class="lt"') >= 0),
                  "and every highlighted cell still carries its own bidi span");
            /* 3) a second click on the same row releases it */
            if (still) { still.dispatchEvent(new w.MouseEvent("click", { bubbles: true })); }
            check(!!still && selN4() === 0 && chgN4() === 0,
                  "a second click on the picked row releases it",
                  selN4() + " sel / " + chgN4() + " chg");
            /* 4) and a released row does not come back on the next repaint */
            body = o + "\n" + n1 + "\n" + n2 + "\n" + "1792758000,14,1024,0,1,2,1,3,0,0,24300,24260,24340,20,0,23000,12100,1200,1100,450,300";
            await w.evtTick();
            check(selN4() === 0 && chgN4() === 0 && pRows4().length === 4,
                  "the release sticks: the next tick repaints without a selection",
                  selN4() + " sel / " + pRows4().length + " rows");
            w.evtHttp = savedHttp;
            w.evtDays = savedDays;
        });
        /*
 * [EN] v1.129 (owner: "change your method - do what the industry does"). The
 *      history card rebuilt the whole table on every tick and then hunted for
 *      the picked row by a name made of "epoch:uptime". Two rows CAN share a
 *      second - a day file opens with its own "day starts here" row
 *      (plink_log.h:1529) and the boot row of that session is written by tick()
 *      at the same instant (plink_log.h:2348) - so that name is not unique and
 *      the highlight could land on the neighbour. The industry answer is the
 *      one React and every keyed-diff library uses: identify a list item by the
 *      DATA it shows, reuse its element while it is still there, and touch only
 *      the rows that really changed.
 * [FA] نسخهٔ ۱٫۱۲۹ (دستور کاربر: «روشت را عوض کن، کار روز دنیا را بکن»). کارت
 *      تاریخچه هر بار کل جدول را از نو می‌ساخت و بعد ردیف انتخاب‌شده را با نامی
 *      از «epoch:uptime» می‌جست. دو ردیف می‌توانند یک ثانیه داشته باشند - ردیف
 *      «شروع روز» (plink_log.h:1529) و ردیف روشن‌شدن همان لحظه
 *      (plink_log.h:2348) - پس آن نام یکتا نیست و هایلایت می‌توانست روی همسایه
 *      بیفتد. پاسخ روز دنیا همان است که React و هر کتابخانهٔ keyed-diff
 *      می‌دهند: هر آیتم را با دادهٔ خودش بشناس، عنصرش را دوباره به کار ببر و
 *      فقط ردیف‌هایی را دست بزن که واقعاً عوض شده‌اند.
 */
        const structCheck = liveCheck.then(async () => {
            const pRows5 = () => [...d.querySelectorAll("#evtlist .evtrow")];
            const tagOf = (tr) => tr.querySelector("td.evtd").textContent.trim();
            const savedHttp = w.evtHttp;
            const savedDays = w.evtDays;
            let body = "";
            w.evtDays = async () => {};
            w.evtHttp = async () => ({
                ok: true,
                headers: { get: (h) => (h === "X-Log-Fs" ? "1" : (h === "X-Log-Lost" ? "0" : null)) },
                text: async () => body
            });
            const dayStart = "1792756800,12,0,7,1,1,3,0,0,0,24000,23960,24040,20,0";
            const bootRow = "1792756800,12,1024,0,1,1,3,0,0,0,24000,23960,24040,20,0";
            const later = "1792757400,13,1024,0,1,1,3,0,0,0,24000,23960,24040,20,0";
            const latest = "1792758000,14,1024,0,1,1,3,0,0,0,24000,23960,24040,20,0";
            body = dayStart + "\n" + bootRow;
            await w.evtfetch(0);
            const keys = pRows5().map(tr => tr.dataset.k);
            check(new Set(keys).size === keys.length && keys.length === 2,
                  "two rows written in the same second still carry different names",
                  keys.join(" | "));
            const start = pRows5().find(tr => tagOf(tr).indexOf("Day start") >= 0);
            w.evtPick(start);
            body = dayStart + "\n" + bootRow + "\n" + later;
            await w.evtTick();
            const keptStart = pRows5().find(tr => tr.dataset.k === start.dataset.k);
            check(keptStart === start && start.classList.contains("sel"),
                  "the picked row keeps the selection even though its neighbour shares its second",
                  (keptStart ? tagOf(keptStart) : "row gone") + " / same element: " + (keptStart === start));
            /* the industry assertion: a row still on screen is the SAME element,
               not a fresh copy of itself */
            const tb = d.querySelector("#evtlist tbody");
            body = dayStart + "\n" + bootRow + "\n" + later + "\n" + latest;
            await w.evtTick();
            check(d.querySelector("#evtlist tbody") === tb,
                  "the table body is patched in place, never rebuilt");
            check(pRows5().length === 4, "the new row is added and the older ones stay",
                  String(pRows5().length));
            check(pRows5().indexOf(start) >= 0,
                  "the row the reader picked is literally the same element after the update");
            w.evtHttp = savedHttp;
            w.evtDays = savedDays;
        });

            const pm = d.getElementById("pmod");
            const press = (key) => d.body.dispatchEvent(new w.KeyboardEvent("keydown", { key, bubbles: true }));
            const p1 = w.pmConfirm("تست کادر");
            setTimeout(async () => {
                check(pm.classList.contains("on") && d.getElementById("pmt").textContent === "تست کادر",
                      "pmConfirm opens the panel's own dialog with the text");
                press("Enter");
            }, 30);
            p1.then((v) => {
                check(v === true && !pm.classList.contains("on"), "Enter confirms and closes the dialog");
                const p2 = w.pmConfirm("تست لغو");
                setTimeout(() => press("Escape"), 30);
                return p2;
            }).then(async (v) => {
                check(v === false && !pm.classList.contains("on"), "Escape cancels the confirm dialog");
                await lostCheck;
                await tickCheck;
                await liveCheck;
                await structCheck;
                /* ---- v1.134: the history share is a menu in the log card ----
                   [EN] Owner order 2026-11-06: "put that percentage of memory as a
                        menu in the log menus so the user can change it too". A
                        menu that only paints a number would pass every text
                        check and still never reach the board, so this drives the
                        real control and watches what leaves the page. */
                {
                    const sel = d.getElementById("evtpct");
                    check(!!sel, "the log card carries the history-share menu the owner asked for");
                    const opts = sel ? [...sel.querySelectorAll("option")].map(o => +o.value) : [];
                    check(opts.length === 13 && opts[0] === 40 && opts[12] === 100,
                          "it offers 40 to 100 percent, the same range the board enforces",
                          opts.join(","));
                    const savedHttp2 = w.evtHttp;
                    const savedDays2 = w.evtDays;
                    const calls = [];
                    w.evtDays = async () => {};
                    w.evtHttp = async (u, m) => {
                        calls.push([u, m]);
                        return {
                            ok: true,
                            json: async () => ({ ok: 1, p: u.indexOf("p=70") >= 0 ? 70 : 85, min: 40, max: 95 }),
                            text: async () => ""
                        };
                    };
                    w.evtPctFill(100);
                    check(sel && [...sel.querySelectorAll("option")].some(o => o.value === "100" && o.textContent.indexOf("همه") >= 0),
                          "100 is offered as \"everything that is left\", not as a bare number");
                    w.evtPctFill(60);
                    check(sel && sel.value === "60",
                          "the menu shows the share the board reports, not a hard-coded one",
                          sel ? sel.value : "no menu");
                    if (sel) { sel.value = "70"; }
                    await w.evtPctSet();
                    check(calls.some(c => c[0] === "/evt/pct?p=70" && c[1] === "POST"),
                          "picking a share posts it to the board, it is not just painted",
                          JSON.stringify(calls));
                    check(sel && sel.value === "70",
                          "and the menu keeps the number the board answered with",
                          sel ? sel.value : "no menu");
                    w.evtHttp = savedHttp2;
                    w.evtDays = savedDays2;
                }
                console.log("=".repeat(68));
                if (failures === 0) {
                    console.log("ALL " + checks + " PANEL CLICK TESTS PASSED");
                } else {
                    console.log(failures + " of " + checks + " FAILED");
                }
                process.exit(failures === 0 ? 0 : 1);
            });
            /*
 * [EN] v1.100: the manual-mode precondition checklist - green
 * ticks when every condition holds, named reasons when not,
 * and the same list gates the mode.
 */
            {
                const all = w.mpre({ on: 1, fl: 2, t: [], p: [] });
                check(all.length === 6 && all.every(x => x.ok),
                      "a healthy board shows six green manual-mode conditions",
                      JSON.stringify(all));
                const noLink = w.mpre({ on: 0, fl: 2, t: [], p: [] });
                check(!noLink[0].ok && noLink[0].why.indexOf("ارتباط") >= 0,
                      "a dead link names its reason", noLink[0].why);
                const tTech = { on: 1, fl: 2, p: [] }; tTech.t = []; tTech.t[19] = 128;
                const rTech = w.mpre(tTech);
                check(!rTech[3].ok && !rTech[5].ok,
                      "the technical lockout lights its own tick and the no-fault tick",
                      JSON.stringify(rTech));
                const tFin = { on: 1, fl: 2, p: [] }; tFin.t = []; tFin.t[6] = 7;
                const rFin = w.mpre(tFin);
                check(!rFin[4].ok && rFin[4].why.indexOf("نهایی") >= 0,
                      "a final-fault channel names the third-strike reason", rFin[4].why);
                const tCut = { on: 1, fl: 2, t: [] }; tCut.p = []; tCut.p[11] = 0;
                const rCut = w.mpre(tCut);
                check(!rCut[2].ok && rCut[2].why.indexOf("قطع") >= 0,
                      "a panel-disabled channel names it", rCut[2].why);
                check(w.mchk({ on: 1, fl: 2, t: [], p: [] }).indexOf("همه سبز") >= 0,
                      "the checklist line says ready when all green");
                check(w.mchk(tTech).indexOf("وارد نمی") >= 0 &&
                      w.mchk(tTech).indexOf("خرابی فنی") >= 0,
                      "the checklist line lists the blocking reason");
                [...d.querySelectorAll('[id="mp1"],[id="mp2"]')].forEach(e => e.remove());
                const box1 = d.createElement("div"); box1.id = "mp1";
                const box2 = d.createElement("div"); box2.id = "mp2";
                d.body.appendChild(box1); d.body.appendChild(box2);
                w.mview({ on: 1, fl: 32, p: [] });
                check(box1.innerHTML.indexOf("mpre") < 0 &&
                      box1.innerHTML.length > 50 &&
                      box2.innerHTML.length > 50,
                      "in manual mode both channel cards show the checklist");
                w.mview({ on: 1, fl: 2, p: [] });
                check(box1.innerHTML === "" && box2.innerHTML === "",
                      "in auto mode the checklist is not shown");
            }
            {
                const pDis = (w.D && Array.isArray(w.D.p)) ? w.D.p.slice() : [];
                pDis[29] = 6000; pDis[33] = 21000; pDis[50] = 50; pDis[53] = 15;
                pDis[74] = 21000; pDis[75] = 29000; pDis[80] = 2; pDis[81] = 2; pDis[82] = 3;
                const tChg = new Array(32).fill(0);
                tChg[6] = 1; tChg[13] = 2; tChg[14] = 24100; tChg[15] = 26600;
                tChg[16] = 13200; tChg[17] = 13200; tChg[18] = 13400;
                w.draw({ on: 1, seq: 901, fl: 7, fl2: 0, t: tChg, p: pDis });
                const vp1 = d.getElementById("vp1"), vp2 = d.getElementById("vp2");
                check(vp1 && vp2 && vp1.textContent === "" && vp1.className === "bpct" &&
                      vp2.textContent === "" && vp2.className === "bpct",
                      "battery percentage stays hidden next to vb1/vb2 while charging on mains",
                      (vp1 ? vp1.className + ":" + vp1.textContent : "no-vp1"));
                const tDis = tChg.slice();
                tDis[6] = 6; tDis[13] = 6; tDis[14] = 0;
                tDis[18] = 12500; /* upper: (12500-10500)/4000 = 50% -> green */
                tDis[17] = 11500; /* lower: (11500-10500)/4000 = 25% -> yellow */
                tDis[16] = 11500; tDis[15] = 24000;
                w.draw({ on: 1, seq: 902, fl: 5, fl2: 0, t: tDis, p: pDis });
                check(vp1.textContent === "50%" && vp1.className === "bpct on" &&
                      vp2.textContent === "25%" && vp2.className === "bpct on y",
                      "on discharge each battery shows its own fullness % right beside its voltage",
                      vp1.textContent + "/" + vp1.className + " , " + vp2.textContent + "/" + vp2.className);
                /* 1% jitter must be held steady by the 2% discharge hysteresis (q80=2) */
                const tJit = tDis.slice();
                tJit[18] = 12540; /* raw 51% -> within 2% of 50%, stays 50% */
                tJit[17] = 10900; /* raw 10% -> >=2% move and <15% crit -> red 10% */
                w.draw({ on: 1, seq: 903, fl: 5, fl2: 0, t: tJit, p: pDis });
                check(vp1.textContent === "50%" && vp2.textContent === "10%" && vp2.className === "bpct on r",
                      "discharge % applies 2% hysteresis and critical red styling below q53",
                      vp1.textContent + " , " + vp2.textContent + "/" + vp2.className);
                /* Missing battery (<6V or BAT_LOST=8) must not claim 0% */
                /*
 * [EN] tLost copies tDis, so the lower battery is back at 11500 mV
 * = 25 % raw. The point of this check is that ONE missing
 * channel hides its % while the other one still shows ITS
 * OWN number - not that the number is frozen at the value
 * of the previous draw. The old expectation said 10 %, the
 * value of the jitter draw it no longer copies.
 */
                const tLost = tDis.slice();
                tLost[6] = 8; tLost[18] = 1200;
                w.draw({ on: 1, seq: 904, fl: 5, fl2: 0, t: tLost, p: pDis });
                check(vp1.textContent === "" && vp1.className === "bpct" &&
                      vp2.textContent === "25%" && vp2.className === "bpct on y",
                      "a disconnected battery channel hides % while the connected battery keeps showing its %",
                      vp1.textContent + "/" + vp1.className + " , " + vp2.textContent);
            }
            return;
        }
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
