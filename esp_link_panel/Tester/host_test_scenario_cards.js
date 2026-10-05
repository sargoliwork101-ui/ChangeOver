/**
 * [EN] Behavioural test for the six scenario cards of the ESP panel.
 *
 *      The text checks elsewhere in this repository prove a field EXISTS.
 *      They cannot prove that the derived lines under those fields are the
 *      numbers the board will really use: a wrong divisor, a stale default
 *      or a line that silently stops following what the user typed all read
 *      as perfectly valid HTML. Since v1.43/v1.44 the scenario cards carry
 *      those derived numbers - the blink milliseconds, the beep window, the
 *      mV equivalent of every percent threshold - so they need a test that
 *      loads the real generated page in a DOM, types into the real inputs
 *      and asserts on what the card then says.
 *
 *      Every expected value in this file is derived by hand from the
 *      firmware, not copied from the panel: the yellow blink from
 *      func__Ui_ScenarioCharging_Tick, the beep window from the buzzer
 *      service, the percent map from UI_ALARM_PARAM_* ids 74/75.
 *
 *      Optional by design, exactly like host_test_panel_click.js: jsdom is
 *      not a dependency of this project and the firmware build must never
 *      need node, so a missing jsdom SKIPS (exit 0). Run it with:
 *
 *          npm install --no-save jsdom
 *          node esp_link_panel/Tester/host_test_scenario_cards.js
 *
 * [FA] تست رفتاری شش کارت سناریوی پنل ESP.
 *
 *      چک‌های متنی دیگرِ این مخزن فقط ثابت می‌کنند یک فیلد «هست». نمی‌توانند
 *      ثابت کنند خط‌های محاسبه‌شدهٔ زیر آن فیلدها همان عددی‌اند که برد واقعاً
 *      به‌کار می‌برد: یک مخرج اشتباه، یک پیش‌فرض کهنه یا خطی که بی‌صدا دیگر
 *      مقدار تایپ‌شده را دنبال نمی‌کند، همه HTML کاملاً سالمی هستند. از نسخهٔ
 *      ۱٫۴۳/۱٫۴۴ کارت‌های سناریو همین اعداد را نشان می‌دهند (میلی‌ثانیهٔ چشمک،
 *      پنجرهٔ بوق، معادل mV هر آستانهٔ درصدی)، پس تستی لازم است که صفحهٔ واقعی
 *      تولیدشده را در DOM بار کند، در ورودی‌های واقعی تایپ کند و روی چیزی که
 *      کارت می‌گوید assert بزند.
 *
 *      هر مقدار انتظارشده در این فایل دستی از فرمور درآمده، نه کپی از پنل.
 *
 *      مثل host_test_panel_click.js اختیاری است: نبودن jsdom یعنی SKIP با
 *      خروجی صفر، چون بیلد فرمور هرگز نباید به node نیاز داشته باشد.
 */

'use strict';

const fs = require('fs');
const path = require('path');

/* ==================== jsdom (optional) / jsdom (اختیاری) ==================== */

let JSDOM;
try {
    ({ JSDOM } = require('jsdom'));
} catch (e) {
    try {
        ({ JSDOM } = require(path.join(__dirname, '../../node_modules/jsdom')));
    } catch (e2) {
        console.log('SKIP: jsdom not installed.');
        console.log('      npm install --no-save jsdom');
        console.log('      node esp_link_panel/Tester/host_test_scenario_cards.js');
        process.exit(0);
    }
}

/* ==================== Test bookkeeping / شمارش تست ==================== */

let passed = 0;
let failed = 0;

function check(condition, name, detail) {
    if (condition) {
        passed += 1;
        console.log('  ok   ' + name);
    } else {
        failed += 1;
        console.log('  FAIL ' + name + (detail ? '\n         ' + detail : ''));
    }
}

/* ==================== Page load / بارکردن صفحه ==================== */

const PAGE = path.join(__dirname, '..', 'panel_preview.html');

function loadPanel() {
    const html = fs.readFileSync(PAGE, 'utf8');
    return new JSDOM(html, {
        runScripts: 'dangerously',
        pretendToBeVisual: true,
        url: 'http://localhost/'
    });
}

/* ==================== Helpers / کمک‌ها ==================== */

function textOf(doc, id) {
    const el = doc.getElementById(id);
    return el ? el.textContent.replace(/\s+/g, ' ').trim() : '';
}

function typeInto(win, doc, id, value) {
    const el = doc.getElementById(id);
    el.value = String(value);
    win.sall();
    win.c4();
}

/* ==================== Structure / ساختار ==================== */

function testStructure(win, doc) {
    console.log('\nstructure / ساختار');

    /* [EN] 27..118 are the alarm ids. 93..107 are edited in the chart and
       117 is a toggle button, so only the rest own a numeric input.
       [FA] شناسه‌های ۹۳..۱۰۷ در نمودار ویرایش می‌شوند و ۱۱۷ دکمه است. */
    const noInput = new Set([76, 117]);
    for (let id = 93; id <= 107; id += 1) {
        noInput.add(id);
    }
    let missing = [];
    let duplicated = [];
    for (let id = 27; id <= 118; id += 1) {
        if (noInput.has(id)) {
            continue;
        }
        const n = doc.querySelectorAll('#q' + id).length;
        if (n === 0) {
            missing.push(id);
        } else if (n > 1) {
            duplicated.push(id);
        }
    }
    check(missing.length === 0, 'every scenario id has an input', 'missing: ' + missing.join(','));
    check(duplicated.length === 0, 'no scenario id is duplicated', 'duplicated: ' + duplicated.join(','));

    for (let card = 1; card <= 6; card += 1) {
        doc.querySelector('#usel button[data-u="' + card + '"]').click();
        check(doc.getElementById('ucard' + card).style.display === '', 'card ' + card + ' opens');
    }

    /* [EN] Each card must carry the "when does this come?" block and the
       numbered sections - that is the shape the user asked for.
       [FA] هر کارت باید جعبهٔ «کِی می‌آید؟» و بخش‌های شماره‌دار را داشته باشد. */
    for (let card = 1; card <= 6; card += 1) {
        const el = doc.getElementById('ucard' + card);
        check(el.querySelector('.c4ds') !== null, 'card ' + card + ' explains when it triggers');
        check(el.querySelectorAll('.sec').length >= 2, 'card ' + card + ' is split into numbered sections');
    }
}

/* ==================== Scenario 1 - input overvoltage ==================== */

function testOvervoltage(win, doc) {
    console.log('\nscenario 1 - overvoltage / اضافه‌ولتاژ');

    typeInto(win, doc, 'q70', 28000);
    typeInto(win, doc, 'q71', 1000);
    const line = textOf(doc, 's1v');
    check(line.includes('28000') && line.includes('27000'),
        'enter is the threshold and exit is threshold minus hysteresis', line);

    /* [EN] Red blink: on = period * duty / 100. 1000 ms at 50% = 500/500.
       [FA] چشمک قرمز: روشن = دوره × duty ÷ ۱۰۰. */
    typeInto(win, doc, 'q38', 1000);
    typeInto(win, doc, 'q39', 50);
    check(/500 ms/.test(textOf(doc, 's1b')), 'red blink at 50% duty is 500/500 ms', textOf(doc, 's1b'));
    typeInto(win, doc, 'q39', 20);
    check(/200 ms/.test(textOf(doc, 's1b')), 'red blink follows the duty down to 200 ms', textOf(doc, 's1b'));

    /* [EN] Beep window = duration*count + gap*(count-1); must fit the period.
       [FA] پنجرهٔ بوق = مدت×تعداد + گپ×(تعداد−۱) و باید در دوره جا شود. */
    typeInto(win, doc, 'q40', 10000);
    typeInto(win, doc, 'q41', 1000);
    typeInto(win, doc, 'q42', 2);
    typeInto(win, doc, 'q43', 500);
    check(/2500/.test(textOf(doc, 's1z')), 'beep window is 2*1000 + 500 = 2500 ms', textOf(doc, 's1z'));

    typeInto(win, doc, 'q41', 6000);
    check(/⚠/.test(textOf(doc, 's1z')), 'a beep window larger than the period is called out', textOf(doc, 's1z'));
    typeInto(win, doc, 'q40', 0);
    check(/خاموش/.test(textOf(doc, 's1z')), 'period zero reads as beeper off', textOf(doc, 's1z'));
}

/* ==================== Scenario 2 - battery disconnect ==================== */

function testDisconnect(win, doc) {
    console.log('\nscenario 2 - disconnect / قطع باتری');

    typeInto(win, doc, 'q29', 6000);
    typeInto(win, doc, 'q30', 7000);
    const line = textOf(doc, 's2v');
    check(line.includes('6000') && line.includes('7000'), 'absence and return thresholds are both shown', line);
    check(line.includes('1000'), 'the distance between them is computed', line);

    typeInto(win, doc, 'q46', 3000);
    typeInto(win, doc, 'q47', 233);
    typeInto(win, doc, 'q48', 3);
    typeInto(win, doc, 'q49', 100);
    check(/899/.test(textOf(doc, 's2z')), '3 x 233 ms + 2 x 100 ms gap = 899 ms', textOf(doc, 's2z'));
}

/* ==================== Scenario 3 - discharge bands ==================== */

function testDischarge(win, doc) {
    console.log('\nscenario 3 - discharge / دشارژ');

    typeInto(win, doc, 'q74', 21000);
    typeInto(win, doc, 'q75', 29000);
    typeInto(win, doc, 'q50', 40);
    typeInto(win, doc, 'q51', 20);
    typeInto(win, doc, 'q52', 10);
    typeInto(win, doc, 'q53', 1);
    typeInto(win, doc, 'q66', 1000);
    typeInto(win, doc, 'q67', 10);
    typeInto(win, doc, 'q62', 1);
    typeInto(win, doc, 'q63', 2);
    typeInto(win, doc, 'q64', 3);
    typeInto(win, doc, 'q59', 1000);
    typeInto(win, doc, 'q60', 2000);
    typeInto(win, doc, 'q54', 60000);
    typeInto(win, doc, 'q55', 20000);
    typeInto(win, doc, 'q65', 100);

    /* [EN] v1.45 (user order): the card must carry the ladder itself and
       every variable of a band must sit in that band's own block.
       [FA] نردبان باید داخل همین کارت باشد و متغیرهای هر باند در بلوک خودش. */
    const card = doc.getElementById('ucard3');
    check(card.querySelectorAll('input.qm[data-q="74"]').length === 1 &&
          card.querySelectorAll('input.qm[data-q="75"]').length === 1,
        'the discharge card carries the low/high voltage limits');
    check(textOf(doc, 's3m').includes('21000') && textOf(doc, 's3m').includes('29000'),
        'the ladder line shows both limits', textOf(doc, 's3m'));

    const blocks = card.querySelectorAll('.bnd');
    check(blocks.length === 5, 'there is one block per band (silent, 1, 2, 3, critical)', 'blocks=' + blocks.length);
    const inBlock = (k, id) => blocks[k].querySelector('#q' + id) !== null ||
                               blocks[k].querySelector('input.qm[data-q="' + id + '"]') !== null;
    check(inBlock(1, 51) && inBlock(1, 62) && inBlock(1, 59) && inBlock(1, 54) && inBlock(1, 65),
        'band 1 block holds its percent, count, duration, interval and gap');
    check(inBlock(2, 52) && inBlock(2, 63) && inBlock(2, 59) && inBlock(2, 54) && inBlock(2, 65),
        'band 2 block holds all five of its variables too');
    check(inBlock(3, 53) && inBlock(3, 64) && inBlock(3, 60) && inBlock(3, 55) && inBlock(3, 65),
        'band 3 block holds its own duration and interval');
    check(inBlock(4, 56) && inBlock(4, 57) && inBlock(4, 58) && inBlock(4, 61) && inBlock(4, 65),
        'the critical block holds period, duty, count, one-shot length and gap');

    /* [EN] A shared value edited in one block must move in the other.
       [FA] مقدار مشترک که در یک بلوک عوض شود باید در بلوک دیگر هم عوض شود. */
    const mir = [...card.querySelectorAll('input.qm[data-q="65"]')];
    check(mir.length >= 3, 'the shared gap appears in every band that uses it', 'copies=' + mir.length);
    mir[1].value = '250';
    mir[1].oninput();
    check(doc.getElementById('q65').value === '250', 'editing a mirror writes the primary input');
    win.sall();
    check(mir[0].value === '250' && mir[2].value === '250', 'the other copies of a shared value follow');
    typeInto(win, doc, 'q65', 100);
    win.qmfill();
    check(mir[1].value === '100', 'a primary edit pushes back into the mirrors');

    /* [EN] Band lines: range in percent and mV, green at the mid band and
       the beep window against the interval.
       [FA] خط هر باند: بازه بر حسب درصد و mV، سبز میانه و پنجرهٔ بوق. */
    check(textOf(doc, 's3r2').includes('10') && textOf(doc, 's3r2').includes('20'),
        'band 2 range is 10..20 percent', textOf(doc, 's3r2'));
    check(textOf(doc, 's3r2').includes('21800') && textOf(doc, 's3r2').includes('22600'),
        'band 2 range in mV comes off the 74/75 ladder', textOf(doc, 's3r2'));
    check(/150 ms.*850 ms/.test(textOf(doc, 's3n2')),
        'green at the 15% mid-band is 150/850 ms', textOf(doc, 's3n2'));
    check(textOf(doc, 's3n2').includes('2100'),
        'band 2 beep window is 2 x 1000 + 100 = 2100 ms', textOf(doc, 's3n2'));
    check(textOf(doc, 's3n0').includes('بدون بوق'), 'the silent band says it has no beep', textOf(doc, 's3n0'));
    check(textOf(doc, 's3n4').includes('خاموش') && /یک‌بار/.test(textOf(doc, 's3n4')),
        'the critical block says LEDs off and one-shot', textOf(doc, 's3n4'));

    typeInto(win, doc, 'q59', 90000);
    check(/⚠/.test(textOf(doc, 's3n1')), 'a 90 s beep in a 60 s interval is called out', textOf(doc, 's3n1'));
    typeInto(win, doc, 'q59', 1000);

    typeInto(win, doc, 'q53', 60);
    check(/⚠/.test(textOf(doc, 's3z')), 'a broken band order is called out', textOf(doc, 's3z'));
    typeInto(win, doc, 'q53', 1);
    check(!/⚠/.test(textOf(doc, 's3z')), 'and the warning clears when the order is restored', textOf(doc, 's3z'));
}

/* ==================== Scenario 4 - charging / full ==================== */

function testCharging(win, doc) {
    console.log('\nscenario 4 - charging / شارژ');

    typeInto(win, doc, 'q74', 21000);
    typeInto(win, doc, 'q75', 29000);
    typeInto(win, doc, 'q68', 1000);
    typeInto(win, doc, 'q69', 150);
    typeInto(win, doc, 'q77', 100);
    typeInto(win, doc, 'q78', 95);

    check(textOf(doc, 'c4map').includes('80'), 'one percent of a 21..29 V ladder is 80 mV', textOf(doc, 'c4map'));
    const full = textOf(doc, 'c4full');
    check(full.includes('29000') && full.includes('28600'),
        'enter/exit full are translated to mV', full);

    const rows = [...doc.getElementById('c4tb').rows].map(r => [...r.cells].map(c => c.textContent.trim()));
    const at = p => rows.find(r => r[0] === p + '٪');
    check(at(50)[3] === '500 ms' && at(50)[4] === '500 ms', 'at 50% the yellow is 500/500 ms', JSON.stringify(at(50)));
    check(at(90)[3] === '150 ms', 'at 90% the minimum-on floor takes over (100 -> 150 ms)', JSON.stringify(at(90)));
    check(at(100)[2] === '2٪', 'the remaining-to-full floor of 2% is applied at 100%', JSON.stringify(at(100)));
    check(at(0)[3] === '1000 ms' && at(0)[4] === '0 ms', 'zero percent means the yellow is solid', JSON.stringify(at(0)));

    typeInto(win, doc, 'q68', 2000);
    const rows2 = [...doc.getElementById('c4tb').rows].map(r => [...r.cells].map(c => c.textContent.trim()));
    check(rows2.find(r => r[0] === '50٪')[3] === '1000 ms', 'doubling the period doubles the on time', JSON.stringify(rows2[3]));
    typeInto(win, doc, 'q68', 1000);
}

/* ==================== Scenario 5 - low battery alarm ==================== */

function testLowBattery(win, doc) {
    console.log('\nscenario 5 - low battery / باتری کم');

    typeInto(win, doc, 'q74', 21000);
    typeInto(win, doc, 'q75', 29000);
    typeInto(win, doc, 'q72', 22600);
    typeInto(win, doc, 'q73', 23400);
    const line = textOf(doc, 's5v');
    check(line.includes('22600') && line.includes('23400'), 'both alarm levels are shown', line);
    check(line.includes('20٪') && line.includes('30٪'), 'both are translated to percent on the 74/75 ladder', line);
    check(line.includes('800'), 'the anti-chatter width is computed', line);

    typeInto(win, doc, 'q73', 22000);
    check(/⚠/.test(textOf(doc, 's5v')), 'a clear level below the alarm level is called out', textOf(doc, 's5v'));
    typeInto(win, doc, 'q73', 21200);
    typeInto(win, doc, 'q72', 21000);

    /* [EN] The ladder itself lives on card 4 now - card 5 must say so.
       [FA] خود نردبان حالا در کارت ۴ است و کارت ۵ باید همین را بگوید. */
    const card5 = doc.getElementById('ucard5');
    check(doc.querySelectorAll('#ucard5 #q74').length === 0, 'the ladder is not duplicated on card 5');
    check(/کارت/.test(card5.textContent) && card5.querySelector('button[onclick="usel(4)"]') !== null,
        'card 5 points at the card that owns the ladder');
}

/* ==================== Scenario 6 - imbalance ==================== */

function testImbalance(win, doc) {
    console.log('\nscenario 6 - imbalance / عدم‌توازن');

    typeInto(win, doc, 'q112', 30000);
    typeInto(win, doc, 'q114', 10);
    typeInto(win, doc, 'q115', 3600000);
    typeInto(win, doc, 'q116', 200);
    typeInto(win, doc, 'q118', 20);

    const lock = textOf(doc, 's6z');
    check(lock.includes('10'), 'the lock event count is shown', lock);
    check(/5 دقیقه/.test(lock), '10 events x 30 s is a 5 minute floor to the lock', lock);
    check(lock.includes('20'), 'the post-lock charge-cycle budget is shown', lock);

    typeInto(win, doc, 'q111', 0);
    check(/بدون گیت/.test(textOf(doc, 's6v')), 'a zero charge gate reads as "no gate"', textOf(doc, 's6v'));
    typeInto(win, doc, 'q111', 600000);
    check(/10 دقیقه/.test(textOf(doc, 's6v')), 'the charge gate is printed in minutes', textOf(doc, 's6v'));

    typeInto(win, doc, 'q115', 0);
    check(/بوق خاموش/.test(textOf(doc, 's6z')), 'a zero lock-beep period reads as off', textOf(doc, 's6z'));
}

/* ==================== Runner / اجراکننده ==================== */

const dom = loadPanel();
const win = dom.window;

setTimeout(() => {
    const doc = win.document;
    console.log('ESP panel scenario-card tests - the real generated page in a DOM');
    console.log('='.repeat(70));
    try {
        testStructure(win, doc);
        testOvervoltage(win, doc);
        testDisconnect(win, doc);
        testDischarge(win, doc);
        testCharging(win, doc);
        testLowBattery(win, doc);
        testImbalance(win, doc);
    } catch (err) {
        failed += 1;
        console.log('  FAIL threw: ' + err.message);
    }
    console.log('='.repeat(70));
    if (failed === 0) {
        console.log('ALL ' + passed + ' SCENARIO CARD TESTS PASSED');
    } else {
        console.log(failed + ' OF ' + (passed + failed) + ' SCENARIO CARD TESTS FAILED');
    }
    win.close();
    process.exit(failed === 0 ? 0 : 1);
}, 1500);
