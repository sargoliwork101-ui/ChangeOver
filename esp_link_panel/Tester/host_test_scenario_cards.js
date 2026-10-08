/**
 * [EN] Behavioural test for the seven scenario cards of the ESP panel.
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
 * [FA] تست رفتاری هفت کارت سناریوی پنل ESP.
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

/* The page deliberately uses Persian unit names for operators; older checks
   used the firmware shorthand "ms". Compare the value, not its localization. */
function msCell(text, value) {
    return String(text).replace(/میلی[‌ ]?ثانیه/g, 'ms').replace(/\s+/g, '') === String(value) + 'ms';
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
    /* v1.74: 72/73 are retired - the low-battery window is a Changeover
       constant now, so the panel must NOT offer a field for them. */
    const noInput = new Set([72, 73, 76, 117]);
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

    for (let card = 1; card <= 7; card += 1) {
        doc.querySelector('#usel button[data-u="' + card + '"]').click();
        check(doc.getElementById('ucard' + card).style.display === '', 'card ' + card + ' opens');
    }

    /* [EN] Each card must carry the "when does this come?" block and the
       numbered sections - that is the shape the user asked for.
       [FA] هر کارت باید جعبهٔ «کِی می‌آید؟» و بخش‌های شماره‌دار را داشته باشد. */
    for (let card = 1; card <= 7; card += 1) {
        const el = doc.getElementById('ucard' + card);
        check(el.querySelector('.c4ds') !== null, 'card ' + card + ' explains when it triggers');
        check(el.querySelectorAll('.sec').length >= 2, 'card ' + card + ' is split into numbered sections');
    }
    const factoryButtons = Array.from(doc.querySelectorAll('button')).filter(b =>
        b.textContent.indexOf('بازگردانی پیش‌فرض کارخانه') >= 0);
    check(factoryButtons.length === 9 && factoryButtons.every(b => b.classList.contains('fwb')),
          'every settings section gives its factory-reset key the same full-width panel layout');
    check(factoryButtons.every(b => b.classList.contains('sb2')),
          'all factory-reset keys use the panel secondary-button theme');
    /* The standalone simulator launch card was intentionally removed; the
       operating page must not advertise a dead route. */
    check(!doc.getElementById('simopen') && typeof win.opensim !== 'function',
        'the removed standalone simulator route is not advertised on the operating page');

    /* [EN] A browser min/max attribute is not a visible instruction. Every
       numeric scenario box must print the same range immediately below it.
       [FA] min/max اچِی‌تی‌ام‌ال به‌تنهایی راهنمای دیداری نیست؛ هر کادر عددی
       سناریو باید همان بازه را درست زیر خودش چاپ کند. */
    const numeric = [...doc.querySelectorAll('.bqr input[type="number"][id^="q"]')];
    const missingRanges = numeric.filter(el => {
        const r = el.closest('label').querySelector('.qrng');
        return !r || !r.textContent.includes(el.min) || !r.textContent.includes(el.max);
    });
    check(numeric.length > 0 && missingRanges.length === 0,
        'every numeric scenario box prints its min/max range below the box',
        missingRanges.map(el => el.id).join(','));
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
    check(textOf(doc, 's1b').includes('500') &&
          (textOf(doc, 's1b').includes('ms') || textOf(doc, 's1b').includes('میلی‌ثانیه')),
          'red blink at 50% duty is 500/500 ms', textOf(doc, 's1b'));
    typeInto(win, doc, 'q39', 20);
    check(textOf(doc, 's1b').includes('200') &&
          (textOf(doc, 's1b').includes('ms') || textOf(doc, 's1b').includes('میلی‌ثانیه')),
          'red blink follows the duty down to 200 ms', textOf(doc, 's1b'));

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

    const card = doc.getElementById('ucard3');

    /* [EN] v1.47: the discharge card OWNS the ladder - one writable copy in
       the whole page, read-only echoes elsewhere.
       [FA] کارت دشارژ صاحب نردبان است: یک نسخهٔ قابل‌نوشتن در کل صفحه. */
    check(card.querySelector('#q74') !== null && card.querySelector('#q75') !== null,
        'the discharge card owns the writable voltage limits');
    check(doc.querySelectorAll('#q74').length === 1 && doc.querySelectorAll('#q75').length === 1,
        'the ladder has exactly one writable field in the page');
    /* [EN] v1.49 (user order): the charge side owns its OWN ladder (119/120).
       Moving a discharge limit must not move the charge card.
       [FA] سمت شارژ نردبان خودش را دارد (۱۱۹/۱۲۰)؛ جابه‌جاکردن حد دشارژ نباید
       کارت شارژ را تکان بدهد. */
    check(doc.querySelectorAll('#ucard4 input#q119').length === 1 &&
          doc.querySelectorAll('#ucard4 input#q120').length === 1,
        'the charge card owns a separate, writable percent ladder');
    typeInto(win, doc, 'q119', 21000);
    typeInto(win, doc, 'q120', 29000);
    win.c4();
    const chargeMapBefore = textOf(doc, 'c4map');
    typeInto(win, doc, 'q74', 20000);
    win.c4();
    check(textOf(doc, 'c4map') === chargeMapBefore,
        'moving the discharge limit leaves the charge ladder untouched', textOf(doc, 'c4map'));
    typeInto(win, doc, 'q74', 21000);
    typeInto(win, doc, 'q119', 20000);
    win.c4();
    check(textOf(doc, 'c4map').includes('20000'),
        'and the charge card follows its own pair', textOf(doc, 'c4map'));
    typeInto(win, doc, 'q119', 21000);
    win.c4();

    /* v1.74: the low-battery card is gone, so only the charge card echoes. */
    check(doc.querySelectorAll('#ucard4 .qmv[data-q="74"]').length === 1,
        'the charge card echoes the discharge ladder read-only');
    win.qmfill();
    check(doc.querySelector('#ucard4 .qmv[data-q="74"]').textContent === '21000',
        'the echo carries the owner value', doc.querySelector('#ucard4 .qmv[data-q="74"]').textContent);
    typeInto(win, doc, 'q74', 20500);
    win.qmfill();
    check(doc.querySelector('#ucard4 .qmv[data-q="74"]').textContent === '20500',
        'editing the owner moves the echo');
    typeInto(win, doc, 'q74', 21000);
    win.qmfill();
    check(textOf(doc, 's3m').includes('21000') && textOf(doc, 's3m').includes('29000'),
        'the ladder line shows both limits', textOf(doc, 's3m'));

    /* [EN] One block per band; shared values echoed, never re-offered.
       [FA] هر باند یک بلوک؛ مقدار مشترک بازتاب می‌شود نه دوباره پیشنهاد. */
    /* [EN] v1.50 (user orders): the silent band is no longer a block of its
       own - its single number lives in band 1 and its sentence is one line
       under band 1. Band 2 owns its per-beep duration and gap now.
       [FA] باند بی‌صدا بلوک جدا ندارد؛ عددش در باند ۱ و جمله‌اش یک خط زیر آن
       است. باند ۲ مدت و گپ خودش را دارد. */
    const blocks = card.querySelectorAll('.bnd');
    check(blocks.length === 4, 'the silent band no longer takes a block of its own',
        'blocks=' + blocks.length);
    const owns = (k, id) => blocks[k].querySelector('#q' + id) !== null;
    const echoes = (k, id) => blocks[k].querySelector('.qmv[data-q="' + id + '"]') !== null;
    check(owns(0, 50), 'the silent threshold moved into band 1');
    check(textOf(doc, 's3q').includes('40') && textOf(doc, 's3q').includes('بوق'),
        'and its sentence is one line under band 1', textOf(doc, 's3q'));
    check(doc.getElementById('s3n0') === null && doc.getElementById('s3r0') === null,
        'the old silent-band lines are gone');
    check(owns(0, 51) && owns(0, 62) && owns(0, 59) && owns(0, 54) && owns(0, 65),
        'band 1 owns its percent, count, duration, interval and gap');
    /* v1.71: band 2 owns percent, count, per-beep duration AND its own repeat
       interval; the only shared number it echoes is the gap (65). */
    check(owns(1, 52) && owns(1, 63) && owns(1, 121) && owns(1, 122) && echoes(1, 65),
        'band 2 owns its shape and echoes only the shared gap');
    check(!echoes(1, 59) && !echoes(1, 54),
        'band 2 no longer borrows band 1 duration or band 1 interval');
    check(owns(2, 53) && owns(2, 64) && owns(2, 60) && owns(2, 55) && echoes(2, 65),
        'band 3 owns its duration and interval, echoes only the gap');
    check(owns(3, 56) && owns(3, 57) && owns(3, 58) && owns(3, 61) && echoes(3, 65),
        'the critical block owns its four values and echoes the gap');
    check(card.querySelectorAll('input#q65').length === 1,
        'the shared gap still has exactly one writable field');

    /* [EN] Shaping band 2 must not move band 1's line, and vice versa.
       [FA] شکل‌دادن باند ۲ نباید خط باند ۱ را تکان بدهد و برعکس. */
    typeInto(win, doc, 'q121', 1000);
    typeInto(win, doc, 'q122', 100);
    const band1Line = textOf(doc, 's3n1');
    typeInto(win, doc, 'q121', 3000);
    typeInto(win, doc, 'q122', 400);
    check(textOf(doc, 's3n1') === band1Line,
        'band 1 is untouched when band 2 is reshaped');
    check(textOf(doc, 's3n2').includes('3000') && textOf(doc, 's3n2').includes('400'),
        'band 2 reports its own duration and gap', textOf(doc, 's3n2'));
    /* v1.75: restore the FACTORY pair (1000 ms beep in a 60 s interval). The
       old restore left 1000 ms inside a 100 ms interval - a pattern the panel
       is right to repair, which then leaked id 121 into the next batch. */
    typeInto(win, doc, 'q121', 1000);
    typeInto(win, doc, 'q122', 60000);

    typeInto(win, doc, 'q65', 250);
    win.qmfill();
    /* v1.71: bands 2, 3 and the critical block all echo the shared gap. */
    const gapEchoes = [...card.querySelectorAll('.qmv[data-q="65"]')];
    check(gapEchoes.length === 3 && gapEchoes.every(e => e.textContent === '250'),
        'the common gap is echoed by bands 2, 3 and the critical block',
        gapEchoes.map(e => e.textContent).join(','));
    check(card.querySelectorAll('input#q65').length === 1,
        'and it is still writable in exactly one place');
    typeInto(win, doc, 'q65', 100);
    win.qmfill();

    /* [EN] Factory default under every field, from the reset tables.
       [FA] پیش‌فرض کارخانه زیر هر فیلد، از جدول دکمه‌های بازگردانی. */
    const noDefault = [];
    for (let k = 1; k <= 7; k += 1) {
        doc.querySelectorAll('#ucard' + k + ' .bqr label').forEach(l => {
            const inp = l.querySelector('input[type=number]');
            if (inp && !l.querySelector('.dflt')) {
                noDefault.push(inp.id || '(no id)');
            }
        });
    }
    check(noDefault.length === 0, 'every scenario field prints its factory default', noDefault.join(','));
    /* [EN] v1.50 (user order: "you wrote the default and the same number
       again"): the chip beside a field must stay silent while it agrees with
       the box, and speak only when the board holds something else.
       [FA] چیپ کنار فیلد تا وقتی با کادر هم‌نظر است ساکت می‌ماند. */
    win.D = { p: { 65: 100, 66: 1000 } };
    win.afill();
    check(doc.getElementById('a65').textContent === '',
        'no second copy of a number the box already shows',
        doc.getElementById('a65').textContent);
    /* v1.70 (user order): the "روی برد: ..." chip was deleted on purpose, so
       the span must stay empty even when the board disagrees. */
    win.D = { p: { 65: 250, 66: 1000 } };
    win.afill();
    check(doc.getElementById('a65').textContent === '',
        'and the deleted board-value chip never comes back',
        doc.getElementById('a65').textContent);
    win.D = null;

    const gapLabel = card.querySelector('#q65').closest('label');
    check(gapLabel.querySelector('.dflt').textContent.includes('100'),
        'the printed default is the real factory value (gap = 100)',
        gapLabel.querySelector('.dflt').textContent);
    check(card.querySelector('#q66').closest('label').querySelector('.t') !== null,
        'the caption is wrapped so the inputs line up');

    /* [EN] Band lines keep reporting the board numbers.
       [FA] خط هر باند همچنان اعداد برد را گزارش می‌کند. */
    check(textOf(doc, 's3r2').includes('10') && textOf(doc, 's3r2').includes('20'),
        'band 2 range is 10..20 percent', textOf(doc, 's3r2'));
    check(textOf(doc, 's3r2').includes('21800') && textOf(doc, 's3r2').includes('22600'),
        'band 2 range in mV comes off the ladder', textOf(doc, 's3r2'));
    const band2Timing = textOf(doc, 's3n2')
        .replace(/میلی[‌ ]?ثانیه/g, 'ms').replace(/\s+/g, '');
    check(/150ms.*850ms/.test(band2Timing),
        'green at the 15% mid-band is 150/850 ms', textOf(doc, 's3n2'));
    check(textOf(doc, 's3n2').includes('2100'),
        'band 2 beep window is 2 x 1000 + its own 100 ms gap = 2100 ms', textOf(doc, 's3n2'));
    check(textOf(doc, 's3n4').includes('خاموش') && /یک‌بار/.test(textOf(doc, 's3n4')),
        'the critical block says LEDs off and one-shot', textOf(doc, 's3n4'));

    typeInto(win, doc, 'q59', 90000);
    check(/⚠/.test(textOf(doc, 's3n1')), 'a 90 s beep in a 60 s interval is called out', textOf(doc, 's3n1'));
    typeInto(win, doc, 'q59', 1000);

    typeInto(win, doc, 'q53', 60);
    check(/⚠/.test(textOf(doc, 's3z')), 'a broken band order is called out', textOf(doc, 's3z'));
    typeInto(win, doc, 'q53', 1);
    check(!/⚠/.test(textOf(doc, 's3z')), 'and the warning clears when the order is restored', textOf(doc, 's3z'));

    /* [EN] "Percent stability" must be explained with the live numbers.
       [FA] «پایداری درصد» باید با اعداد زنده توضیح داده شود. */
    typeInto(win, doc, 'q80', 2);
    const hy = textOf(doc, 's3hy');
    check(hy.includes('درصد پایدار') && hy.includes('34') && hy.includes('36'),
        'the stability text explains the band around the stable percent', hy);
    /* [EN] v1.50 (user order): the 1% paragraph was dropped as noise; the
       0% one stays, and the text must not grow it back.
       [FA] بند ۱٪ به دستور کاربر حذف شد؛ بند ۰٪ می‌ماند. */
    check(hy.includes('بیرون آمدن از ۰٪') && !hy.includes('بیرون آمدن از ۱٪'),
        'the zero-exit paragraph stays and the one-exit paragraph is gone', hy);
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
    check(msCell(at(50)[3], 500) && msCell(at(50)[4], 500),
          'at 50% the yellow is 500/500 ms', JSON.stringify(at(50)));
    check(msCell(at(90)[3], 150),
          'at 90% the minimum-on floor takes over (100 -> 150 ms)', JSON.stringify(at(90)));
    check(at(100)[2] === '2٪', 'the remaining-to-full floor of 2% is applied at 100%', JSON.stringify(at(100)));
    check(msCell(at(0)[3], 1000) && msCell(at(0)[4], 0),
          'zero percent means the yellow is solid', JSON.stringify(at(0)));

    /* q69 is a valid independent clamp, but the firmware also guarantees a
       visible 2% remainder. At a 1000 ms period that physical floor is 20 ms,
       so q69=5 must be accepted and the panel must explain why the table stays
       at 20 ms rather than pretending the parameter is invalid.
       q69 یک کف مستقل معتبر است؛ کف ۲٪ فرم‌ور در دورهٔ ۱۰۰۰ میلی‌ثانیه
       برابر ۲۰ است، پس پنل باید تفاوت «پذیرفته‌شده» و «اثر واقعی» را بگوید. */
    typeInto(win, doc, 'q69', 5);
    const floorText = textOf(doc, 'c4formula');
    check(floorText.includes('20ms') && floorText.includes('5ms') && floorText.includes('معتبر'),
        'charging explains a valid 5 ms clamp whose effective floor remains 20 ms', floorText);
    check(doc.getElementById('q69').closest('label').querySelector('.qrng').textContent.includes('0') &&
          doc.getElementById('q69').closest('label').querySelector('.qrng').textContent.includes('10000'),
        'yellow minimum-on box visibly prints its firmware range 0..10000');

    typeInto(win, doc, 'q68', 2000);
    const rows2 = [...doc.getElementById('c4tb').rows].map(r => [...r.cells].map(c => c.textContent.trim()));
    check(msCell(rows2.find(r => r[0] === '50٪')[3], 1000),
          'doubling the period doubles the on time', JSON.stringify(rows2[3]));
    typeInto(win, doc, 'q68', 1000);
}

/* ==================== Scenario 5 - imbalance ==================== */

function testImbalance(win, doc) {
    console.log('\nscenario 5 - imbalance / عدم‌توازن');

    check(doc.getElementById('q110').getAttribute('max') === '18000000' &&
          doc.getElementById('q111').getAttribute('max') === '18000000',
          'the imbalance wait editors allow the requested five hours');
    const c5 = doc.getElementById('ucard5');
    const c5secs = [...c5.querySelectorAll('.sec')].map(e => e.textContent.trim());
    check(c5secs.findIndex(x => x.indexOf('چشمک قرمز') >= 0) <
          c5secs.findIndex(x => x.indexOf('بوق در قفل') >= 0),
          'scenario 5 keeps the lamp section before the beep section');
    check(c5.querySelector('#q123') && c5.querySelector('#q124') &&
          c5.querySelector('#q115') && c5.querySelector('#q116') &&
          c5.querySelector('#q132') && c5.querySelector('#q133') &&
          c5.querySelector('#q136') &&
          c5.querySelector('#s5b') && c5.querySelector('#s5z'),
          'scenario 5 has separate, ordered lamp, clean-cycle and beep boxes');
    const c5beep = c5.querySelector('#q115').closest('.bqr');
    check(c5beep.querySelectorAll('label').length === 4 &&
          c5beep.querySelector('#q132').type === 'number' &&
          c5beep.querySelector('#q133').type === 'number',
          'scenario 5 exposes editable count and gap parameters');
    check(win.pdflt(132) === 1 && win.pdflt(133) === 0 && win.pdflt(136) === 3,
          'scenario 5 count/gap and clean-cycle factory defaults are correct');
    check(win.eval('XIDS').indexOf(132) >= 0 && win.eval('XIDS').indexOf(133) >= 0 &&
          win.eval('XIDS').indexOf(136) >= 0,
          'count, gap and clean-cycle threshold are included in JSON backup/import ids');
    check(win.getComputedStyle(doc.getElementById('s5z')).direction === 'rtl' &&
          win.getComputedStyle(doc.getElementById('s5b')).textAlign === 'right',
          'scenario 5 result messages are explicitly right-to-left');
    typeInto(win, doc, 'q112', 30000);
    typeInto(win, doc, 'q114', 10);
    typeInto(win, doc, 'q115', 3600000);
    typeInto(win, doc, 'q116', 200);
    typeInto(win, doc, 'q132', 1);
    win.simrun();
    check(doc.getElementById('q133').disabled === true,
          'the imbalance gap editor is disabled when its beep count is one');
    typeInto(win, doc, 'q132', 3);
    win.simrun();
    check(doc.getElementById('q133').disabled === false,
          'the imbalance gap editor is enabled when its beep count is greater than one');
    typeInto(win, doc, 'q133', 100);
    typeInto(win, doc, 'q136', 3);
    typeInto(win, doc, 'q118', 20);

    const lock = textOf(doc, 's5z');
    check(lock.includes('10'), 'the lock event count is shown', lock);
    check(lock.includes('3') && lock.includes('200') && lock.includes('گپ'),
          'the simulator follows the editable clean-cycle, count and total gap settings', lock);
    check(lock.includes('FLOAT') || lock.includes('رویداد کامل'),
          'the lock explanation requires a complete FLOAT-qualified event', lock);
    check(lock.includes('20'), 'the post-lock charge-cycle budget is shown', lock);

    typeInto(win, doc, 'q111', 0);
    check(/خاموش/.test(textOf(doc, 's5v')), 'a zero charge gate reads as off', textOf(doc, 's5v'));
    typeInto(win, doc, 'q111', 600000);
    check(/10 دقیقه/.test(textOf(doc, 's5v')), 'the charge gate is printed in minutes', textOf(doc, 's5v'));

    typeInto(win, doc, 'q115', 0);
    check(/بوق خاموش/.test(textOf(doc, 's5z')), 'a zero lock-beep period reads as off', textOf(doc, 's5z'));
}


/* ==================== v1.51 simulator / شبیه‌ساز ==================== */

/**
 * [EN] The LED/buzzer simulator must be driven ONLY by the boxes of its own
 *      card (user order 2026-10-05), so these checks never populate D: they
 *      type into the inputs, call the engine and assert on the lamps.
 * [FA] شبیه‌ساز فقط از کادرهای همان کارت تغذیه می‌شود؛ این چک‌ها هیچ داده‌ای
 *      از برد نمی‌گذارند و مستقیم روی چراغ‌ها assert می‌زنند.
 */
function testSimulator(win, doc) {
    console.log('\nv1.51 simulator / شبیه‌ساز زنده');

    const on = (id) => (doc.getElementById(id).className || '').indexOf(' on') >= 0;

    for (let n = 1; n <= 7; n += 1) {
        check(doc.getElementById('sim' + n) !== null &&
              doc.getElementById('sl' + n + 'r') !== null &&
              doc.getElementById('sl' + n + 'g') !== null &&
              doc.getElementById('sl' + n + 'y') !== null &&
              doc.getElementById('sl' + n + 'z') !== null,
              'card ' + n + ' has a simulator with three LEDs and a buzzer');
    }

    check(doc.body.innerHTML.indexOf('c4dot') < 0,
          'the old board-driven dots are gone');

    /* The engine must not need any board data at all. / بدون داده برد کار کند. */
    const keptD = win.D;
    win.D = null;
    let threw = false;
    try { win.simrun(); } catch (e) { threw = true; }
    win.D = keptD;
    check(!threw, 'the simulator runs with no board data at all');

    /* Pure helpers mirror the firmware window maths. / کمک‌تابع‌ها. */
    check(win.simbz(0, 1000, 200, 2, 100) === true &&
          win.simbz(250, 1000, 200, 2, 100) === false &&
          win.simbz(350, 1000, 200, 2, 100) === true &&
          win.simbz(600, 1000, 200, 2, 100) === false,
          'simbz replays count x duration + gap inside the period');
    check(win.simbz(0, 0, 200, 2, 0) === false &&
          win.simbz(0, 1000, 200, 0, 0) === false,
          'a zero period or zero count is silent');
    check(win.simblink(0, 1000, 40) === true && win.simblink(500, 1000, 40) === false,
          'simblink follows period and duty');

    /* Card 1: green solid, red follows the duty the user typed. */
    typeInto(win, doc, 'q38', 1000);
    typeInto(win, doc, 'q39', 50);
    win.SIMT[1] = win.performance.now();
    win.simrun();
    check(on('sl1g') && !on('sl1y'), 'card 1 keeps green solid and yellow off');

    /* Card 3: the slider alone decides the band. */
    doc.getElementById('simp3').value = 90;
    win.simrun();
    check(!on('sl3r') && !on('sl3y'), 'card 3 shows no red or yellow above band 1');
    check(doc.getElementById('sl3t').textContent.indexOf('بدون بوق') >= 0,
          'a full battery is announced as silent');
    doc.getElementById('simp3').value = 0;
    win.simrun();
    check(!on('sl3r') && !on('sl3g') && !on('sl3y'),
          'the critical band turns all three LEDs off');

    /* Card 4: full means yellow off, zero percent means yellow solid. */
    doc.getElementById('simp4').value = 100;
    win.simrun();
    check(on('sl4g') && !on('sl4y'), 'a full charge is green solid with the yellow off');
    doc.getElementById('simp4').value = 0;
    win.simrun();
    check(on('sl4y'), 'zero percent holds the yellow solid on');

    /* v1.74: the low-battery card is gone; its window lives in Changeover,
       so no simulator may claim an anti-chatter band any more. */
    check(doc.getElementById('simp5') === null
          || doc.getElementById('simp5').type === 'range',
          'no leftover low-battery voltage slider on card 5');

    /* Card 1 (v1.57): the over-voltage alarm follows threshold 70 / hyst 71. */
    typeInto(win, doc, 'q70', 29000);
    typeInto(win, doc, 'q71', 500);
    doc.getElementById('simp1').value = 26000;
    win.simrun();
    check(!on('sl1r') && !on('sl1z'), 'below the threshold card 1 stays quiet');
    check(doc.getElementById('sim1w').textContent.indexOf('29000') >= 0 &&
          doc.getElementById('sim1w').textContent.indexOf('28500') >= 0,
          'the card spells out the beep-on and beep-off voltages');
    doc.getElementById('simp1').value = 29500;
    win.simrun();
    check(doc.getElementById('sl1t').textContent.indexOf('بالای آستانه') >= 0,
          'above the threshold the alarm starts');
    doc.getElementById('simp1').value = 28700;   /* inside hysteresis */
    win.simrun();
    check(doc.getElementById('sl1t').textContent.indexOf('بالای آستانه') >= 0,
          'inside the hysteresis band the alarm is held');
    doc.getElementById('simp1').value = 28000;
    win.simrun();
    check(doc.getElementById('sl1t').textContent.indexOf('زیر آستانه') >= 0,
          'below threshold minus hysteresis the alarm clears');

    /* Card 6 (v1.57): mode decides the window, the virtual clock decides time. */
    win.simspd(1);
    win.simrst5();
    doc.getElementById('simp5a').value = 12000;
    doc.getElementById('simp5b').value = 12000;
    doc.getElementById('sim5m').value = 'r';
    typeInto(win, doc, 'q108', 300);
    typeInto(win, doc, 'q109', 500);
    typeInto(win, doc, 'q110', 600000);
    typeInto(win, doc, 'q111', 600000);
    typeInto(win, doc, 'q112', 30000);
    typeInto(win, doc, 'q114', 3);
    win.sim5mode();
    win.simrun();
    check(doc.getElementById('sim5w').textContent.indexOf('تا باز شدن پنجره') >= 0,
          'at rest the window is still shut right after a charge');
    check(doc.getElementById('sl5t').textContent.indexOf('پنجره بسته') >= 0,
          'and nothing is counted while it is shut');

    doc.getElementById('sim5m').value = 'd';
    win.sim5mode();
    win.simrun();
    check(doc.getElementById('sim5w').textContent.indexOf('دشارژ') >= 0 &&
          doc.getElementById('sim5w').textContent.indexOf('500') >= 0 &&
          (doc.getElementById('sim5w').textContent.indexOf('mV') >= 0 ||
           doc.getElementById('sim5w').textContent.indexOf('میلی‌ولت') >= 0),
          'on battery the window is open at once and uses the discharge limit');

    doc.getElementById('sim5m').value = 'c';
    win.sim5mode();
    typeInto(win, doc, 'q111', 0);
    win.simrun();
    check(doc.getElementById('sim5w').textContent.indexOf('هرگز') >= 0,
          'a zero charge gate means no measurement while charging');
    typeInto(win, doc, 'q111', 600000);

    /* virtual clock: only runs while the card runs, and honours the speed */
    const t0 = win.SIMT[5];
    win.SIMON[5] = 0;
    win.simrun();
    check(win.SIMT[5] === t0, 'a stopped card freezes its own clock');
    win.SIMON[5] = 1;

    /* Three stable candidates latch only when each candidate reaches a real
       FLOAT/Full boundary. A merely stable discharge episode must remain a
       pending candidate; this mirrors the firmware's cycle qualification. */
    doc.getElementById('simp5b').value = 11000;    /* 1000 mV apart */
    const full5 = doc.getElementById('sim5full');
    for (let i = 0; i < 3; i += 1) {
        doc.getElementById('sim5m').value = 'd';
        win.sim5mode();
        win.SIMT[5] += 30000; win.simrun();
        win.SIMT[5] += 30000; win.simrun();
        doc.getElementById('sim5m').value = 'r';
        full5.checked = true;
        win.sim5mode();
        win.simrun();
        full5.checked = false;
        if (i < 2) {
            doc.getElementById('sim5m').value = 'c';
            win.sim5mode();
        }
    }
    check(win.S5.lock === true && on('sl5r'),
          'three FLOAT-qualified over-limit events latch the lock: solid red');
    win.simrst5();
    doc.getElementById('simp5b').value = 12000;
    win.simrun();
    check(!on('sl5r') && win.S5.n === 0, 'the restart button clears the simulated lock');

    /* v1.55: a gap box is dead while its band asks for a single beep.
       v1.77 (user question + order): EVERY gap behaves the same way, the
       shared gap counts band 2 too, and a repeat interval is never treated
       as a gap. */
    typeInto(win, doc, 'q42', 1);
    win.simrun();
    check(doc.getElementById('q43').disabled === true,
          'one beep per round switches that gap box off');
    typeInto(win, doc, 'q48', 1);
    win.simrun();
    check(doc.getElementById('q49').disabled === true,
          'the cut-battery gap switches off on a single beep as well');
    ['q62', 'q63', 'q64', 'q58'].forEach(id => typeInto(win, doc, id, 1));
    win.simrun();
    check(doc.getElementById('q65').disabled === true,
          'the shared gap dies when all four bands want a single beep');
    check(doc.getElementById('q122').disabled === false,
          'a repeat interval is NOT a gap and stays editable with one beep');
    typeInto(win, doc, 'q63', 2);
    win.simrun();
    check(doc.getElementById('q65').disabled === false,
          'band 2 alone keeps the shared gap alive');
    typeInto(win, doc, 'q63', 1);

    /* v1.78 (user order): the two output-block switches are checkboxes that
       mirror the stored parameter and write it back on change. */
    [['ib117', 117], ['db127', 127]].forEach(([cid, pid]) => {
      const el = doc.getElementById(cid);
      check(el && el.type === 'checkbox', cid + ' is a checkbox, not a button');
      const sent = [];
      const old = win.send;
      win.send = (id, v) => sent.push([id, v]);
      el.checked = true; el.onchange();
      check(sent.length === 1 && sent[0][0] === pid && sent[0][1] === 1,
            'ticking ' + cid + ' writes 1 to param ' + pid);
      el.checked = false; el.onchange();
      check(sent.length === 2 && sent[1][1] === 0,
            'unticking ' + cid + ' writes 0 to param ' + pid);
      win.send = old;
    });
    check(doc.getElementById('a117') && doc.getElementById('a127'),
          'each output-block checkbox keeps its own plain-language state line');

    /* v1.82: scenario 6 owns its lamp/beep fields plus an independent
       count/gap pair; all are editable and belong to its factory key. */
    [[128, 600000], [129, 120], [130, 0], [131, 50], [134, 1], [135, 0]].forEach(([id, def]) => {
      const el = doc.getElementById('q' + id);
      check(el && el.tagName === 'INPUT' && el.type === 'number',
            'scenario 6 owns a box for parameter ' + id);
      check(win.pdflt(id) === def,
            'parameter ' + id + ' prints its factory default ' + def);
      check(win.eval('UDEF[6]').indexOf(id) >= 0,
            'parameter ' + id + " is reset by scenario 6's own factory key");
    });
    check(win.eval('UDEF[5]').indexOf(128) < 0 && win.eval('UDEF[5]').indexOf(115) >= 0 &&
          win.eval('UDEF[6]').indexOf(134) >= 0 && win.eval('UDEF[6]').indexOf(135) >= 0,
          'the imbalance key keeps 115/116 and scenario 6 owns its 134/135 pair');
    typeInto(win, doc, 'q134', 1);
    win.simrun();
    check(doc.getElementById('q135').disabled === true,
          'the dead-battery gap editor is disabled when its beep count is one');
    typeInto(win, doc, 'q134', 3);
    win.simrun();
    check(doc.getElementById('q135').disabled === false,
          'the dead-battery gap editor is enabled when its beep count is greater than one');
    typeInto(win, doc, 'q134', 1);
    typeInto(win, doc, 'q135', 0);

    /* [EN] Scenario 6 must present the red lamp and its independent buzzer as
       two sections, not as one combined control block.
       [FA] سناریوی ۶ باید چراغ قرمز و بوق مستقلش را در دو بخش جدا نشان دهد،
       نه در یک بلوک ترکیبی. */
    const c6 = doc.getElementById('ucard6');
    const c6secs = [...c6.querySelectorAll('.sec')].map(e => e.textContent.trim());
    const lampSection = c6secs.findIndex(x => x.indexOf('چراغ قرمز مستقل') >= 0);
    const beepSection = c6secs.findIndex(x => x.indexOf('بوق مستقل') >= 0);
    check(lampSection >= 0 && beepSection > lampSection,
          'scenario 6 orders an independent lamp section before an independent beep section',
          c6secs.join(' | '));
    check(!c6secs.some(x => x.indexOf('چراغ و بوق') >= 0) &&
          c6.querySelector('#s6lamp') && c6.querySelector('#s6z'),
          'scenario 6 has separate lamp/beep result lines and no combined heading');
    check(c6.querySelector('#q130').closest('.bqr') !== c6.querySelector('#q128').closest('.bqr') &&
          c6.querySelector('#q130').closest('.bqr').textContent.indexOf('بوق') < 0 &&
          c6.querySelector('#q128').closest('.bqr').textContent.indexOf('چراغ') < 0,
          'scenario 6 lamp inputs and buzzer inputs are physically separated');

    /* Scenario 7: the technical-fault card is a real, independently driven
       state machine. It must not silently borrow scenario 5/6 beep settings,
       and one phase must drive all three LEDs. */
    const c7 = doc.getElementById('ucard7');
    const c7text = c7.textContent;
    check(c7text.indexOf('ترانزیستور شارژر') >= 0 &&
          c7text.indexOf('رلهٔ شارژر باز') >= 0 &&
          c7text.indexOf('PWM واقعی صفر') >= 0 && c7text.indexOf('JIT') >= 0 &&
          c7text.indexOf('بیشتر از ۲۰٪') >= 0 && c7text.indexOf('جریان شارژ دقیقاً صفر') >= 0,
          'scenario 7 documents both transistor-fault signatures and their gates');
    check([137, 138, 139, 140, 141, 142].every(id =>
          win.eval('UDEF[7]').indexOf(id) >= 0),
          'scenario 7 owns its six independent alarm parameters');
    check(c7.querySelector('#q137') && c7.querySelector('#q142') &&
          c7.querySelector('#s7v'),
          'scenario 7 exposes its own beep and synchronized-LED controls');

    typeInto(win, doc, 'q137', 3000);
    typeInto(win, doc, 'q138', 200);
    typeInto(win, doc, 'q139', 3);
    typeInto(win, doc, 'q140', 100);
    typeInto(win, doc, 'q141', 1000);
    typeInto(win, doc, 'q142', 50);
    const s7v = textOf(doc, 's7v');
    check(s7v.indexOf('3000') >= 0 && s7v.indexOf('200') >= 0 &&
          s7v.indexOf('3') >= 0 && s7v.indexOf('100') >= 0 &&
          s7v.indexOf('1000') >= 0 && s7v.indexOf('50') >= 0,
          'scenario 7 summary follows all six typed settings', s7v);

    const sameLedState = () => ['r', 'g', 'y'].map(c => on('sl7' + c));
    win.SIMON[7] = 0;
    win.SIMT[7] = 0;
    doc.getElementById('sim7m').value = 's';
    win.simrun();
    const ledOn = sameLedState();
    check(ledOn[0] === ledOn[1] && ledOn[1] === ledOn[2] && ledOn[0] === true && on('sl7z'),
          'scenario 7 JIT signature turns all three LEDs on together with its beep');
    win.SIMT[7] = 600;
    win.simrun();
    const ledOff = sameLedState();
    check(ledOff[0] === ledOff[1] && ledOff[1] === ledOff[2] && ledOff[0] === false,
          'scenario 7 turns all three LEDs off together');
    doc.getElementById('sim7m').value = 'o';
    win.SIMT[7] = 0;
    win.simrun();
    check(sameLedState().every(Boolean) &&
          doc.getElementById('sl7t').textContent.indexOf('PWM بیشتر از ۲۰٪') >= 0,
          'scenario 7 open-fault signature keeps the shared LED phase and text');
    doc.getElementById('sim7m').value = 'n';
    win.simrun();
    check(sameLedState().every(v => !v) && !on('sl7z'),
          'scenario 7 no-fault mode clears all LEDs and the buzzer');

    /* v1.79 (user: "there used to be a LED behind it"): the latched-fault
       bits are real LEDs again - styled, and visible between blinks. */
    if (typeof win.uview === 'function' && win.ASB && win.ASB.bits && win.ASB.bits[0]) {
      win.D = win.D || {};
      win.D.t = win.D.t || [];
      win.D.t[19] = 0b0000101;
      win.uview();
      const cls = i => win.ASB.bits[i].className;
      check(/\bset\b/.test(cls(0)) && /\bset\b/.test(cls(2)),
            'a latched fault bit is marked set no matter the blink phase');
      check(!/\bset\b/.test(cls(1)), 'a clear fault bit stays dark');
      win.D.t[19] = 0;
      win.uview();
      check(!/\bset\b/.test(cls(0)), 'clearing the mask turns the LED off again');
    }
    check(/\.bit\s*\{/.test(doc.documentElement.innerHTML) || /\.bit\{/.test(doc.documentElement.innerHTML),
          'the fault-bit LEDs have a stylesheet rule');
    typeInto(win, doc, 'q42', 3);
    win.simrun();
    check(doc.getElementById('q43').disabled === false,
          'and three beeps bring it back');
    typeInto(win, doc, 'q62', 1);
    typeInto(win, doc, 'q64', 1);
    typeInto(win, doc, 'q58', 1);
    win.simrun();
    check(doc.getElementById('q65').disabled === true,
          'the shared gap dies only when every band that uses it wants one beep');
    typeInto(win, doc, 'q64', 3);
    win.simrun();
    check(doc.getElementById('q65').disabled === false,
          'one band asking for three beeps keeps the shared gap alive');

    /* Stop button freezes the phase. / دکمهٔ توقف فاز را نگه می‌دارد. */
    win.simtog(1);
    check(win.SIMON[1] === 0 && doc.getElementById('simb1').textContent === 'ادامه',
          'the stop button pauses that card only');
    check(win.SIMON[2] === 1, 'the other cards keep running');
    win.simtog(1);
}


/* ==================== v1.52 send queue / صف ارسال ==================== */

/**
 * [EN] Nothing may reach the board until the global button is pressed, and
 *      the report after the handshake must tell the truth about clamping.
 * [FA] تا وقتی دکمهٔ سراسری زده نشود هیچ چیز به برد نمی‌رود؛ و گزارش بعد از
 *      دست‌دادن باید دربارهٔ گیره‌خوردن راست بگوید.
 */
async function testSendQueue(win, doc) {
    console.log('\nv1.52 send queue / صف ارسال سراسری');

    const all = [];
    win.fetch = (u, o) => {
        all.push(String(u));
        /* telemetry keeps answering with the board image the test controls */
        return Promise.resolve({ ok: true, json: () => Promise.resolve(win.D) });
    };
    const posts = { get length() { return all.filter(u => u.indexOf('/s?') >= 0).length; },
                    at(i) { return all.filter(u => u.indexOf('/s?') >= 0)[i]; } };
    win.PEND = {};
    win.pbar();

    typeInto(win, doc, 'q38', 2000);
    doc.getElementById('q38').onchange();
    check(posts.length === 0, 'typing alone posts nothing to the board');
    check(win.PEND['38'] === 2000, 'the edit is staged in the queue instead');
    check(doc.getElementById('q38').className.indexOf('pq') >= 0,
          'the staged box is marked');
    check(doc.getElementById('sbar').className === 'on' &&
          doc.getElementById('sbn').textContent === '1',
          'the global bar appears and counts the pending edit');

    /* An out-of-range pending value is rejected locally: no /s is sent and
       the report names the exact id, name, value and board-side consequence.
       / مقدار خارج از بازه پیش از هر POST محلی رد می‌شود. */
    win.qput(111, 18000001);
    await win.sendall();
    check(posts.length === 0 && win.PEND['111'] === 18000001,
          'an out-of-range pending value blocks the whole batch before POST');
    const rangeReport = doc.getElementById('srsm').textContent;
    check(rangeReport.indexOf('شناسهٔ 111') >= 0 &&
          rangeReport.indexOf('صبر پس از شروع شارژ') >= 0 &&
          rangeReport.indexOf('18000001') >= 0 &&
          rangeReport.indexOf('18000000') >= 0 &&
          rangeReport.indexOf('نمی‌پذیرد') >= 0 &&
          rangeReport.indexOf('clamp') >= 0,
          'the blocked report explains id, name, value, range and board clamp/reject',
          rangeReport);
    win.pclr(111);

    /* Undo puts the board value back and empties the queue. */
    win.D = { p: { 38: 1000 }, t: new Array(25).fill(0), q: 0, q2: 0, q3: 0, q4: 0, fl: 0, on: 1 };
    win.pundo();
    check(Object.keys(win.PEND).length === 0 && posts.length === 0,
          'undo clears the queue without touching the board');
    check(doc.getElementById('q38').value === '1000',
          'undo restores the value the board reported');

    /* The button ships the batch and then judges the echo. */
    win.qput(38, 2000);
    win.qput(39, 60);
    const done = win.sendall();
    win.D.p[38] = 2000;
    win.D.p[39] = 50;          /* the board clamps this one */
    await done;
    check(posts.length === 2 &&
          posts.at(0).indexOf('/s?id=38&v=2000') >= 0 &&
          posts.at(1).indexOf('/s?id=39&v=60') >= 0,
          'one press posts every staged edit once',
          all.filter(u => u.indexOf('/s?') >= 0).join(' '));
    check(Object.keys(win.PEND).length === 0, 'an accepted batch leaves the queue empty');
    const st = doc.getElementById('sbst').textContent;
    check(st.indexOf('گیره') >= 0 && st.indexOf('39: 60→50') >= 0,
          'the report names the value the board clamped', st);
    check(st.indexOf('38: 2000→2000') >= 0 && st.indexOf('39: 60→50') >= 0,
          'the report lists every value in a multi-edit batch', st);
    win.qput(38, 2000);
    win.D.p[38] = 2000;
    await win.sendall();
    check(doc.getElementById('sbst').textContent.indexOf('عیناً پذیرفت') >= 0,
          'a clean batch is reported as accepted and stored');
}



/* ==================== v1.81 parameter schema + bench calibration ==================== */

/**
 * [EN] The backup file must carry the parameter schema (id, name, unit and
 *      limits), not an opaque build stamp. Restore must report a changed
 *      parameter identity precisely, and bench calibration must turn a known
 *      straight line of samples back into the exact gain/offset that produced it.
 * [FA] فایل پشتیبان باید شمای پارامتر (شناسه، نام، واحد و محدوده) را نگه دارد،
 *      نه مهر مبهم بیلد را. بازگردانی باید تغییر دقیق هویت پارامتر را گزارش
 *      کند و کالیبراسیون بنچ نیز همان گین و آفست خط معلوم را برگرداند.
 */
async function testBackupAndCal(win, doc) {
    console.log('\nv1.81 parameter schema + bench calibration / شمای پارامتر و کالیبراسیون');

    /* --- the export payload carries the identity fields --- */
    const blobs = [];
    const OldBlob = win.Blob;
    win.Blob = function (parts, opts) { blobs.push(String(parts[0])); return new OldBlob(parts, opts); };
    win.URL.createObjectURL = () => 'blob:x';
    win.URL.revokeObjectURL = () => {};
    const a = doc.createElement('a');
    const oldCreate = doc.createElement.bind(doc);
    doc.createElement = (t) => { const e = oldCreate(t); if (t === 'a') { e.click = () => {}; } return e; };
    win.D = { p: {}, t: [] };
    win.PEND = {};
    win.CALS = [];
    const XIDS = win.eval('XIDS'), K = win.eval('K_MA');
    XIDS.forEach(id => { win.D.p[id] = 1; });
    win.eval('xexp')();
    doc.createElement = oldCreate;
    win.Blob = OldBlob;
    check(blobs.length === 1, 'the export button produces exactly one file');
    const o = JSON.parse(blobs[0]);
    check(o.app === 'ChangeOver-settings' && o.v === 4, 'the file says what it is and uses the full backup format');
    check(o.build === undefined, 'the backup does not use the panel build as its compatibility identity');
    check(typeof o.saved === 'string' && o.saved.indexOf('T') > 0, 'the file records when it was taken');
    check(Array.isArray(o.schema) && o.schema.length === XIDS.length, 'the file records one schema entry for every backed-up id');
    check(o.schema.every(s => s.id != null && typeof s.name === 'string' && 'unit' in s && 'min' in s && 'max' in s),
          'each schema entry carries the id, readable name, unit and limits');
    check(Object.keys(o.params).length === XIDS.length, 'every backed-up id is in the file');
    check(o.params['0'] === 1 && o.params['6'] === 1,
          'the applied calibration ids 0..6 are included in the full backup');
    check(o.pending && Object.keys(o.pending).length === 0 && Array.isArray(o.benchSamples) && o.lut === null,
          'the full backup has explicit pending, raw-sample and LUT slots even when they are empty');
    check(o.params['76'] === undefined, 'the live-only id 76 stays out of the backup');
    check(o.params['72'] === undefined && o.params['73'] === undefined,
          'retired ids 72 and 73 stay out of the backup');
    const changedSchema = o.schema.map(s => Object.assign({}, s));
    changedSchema.find(s => s.id === 25).name = 'نام قدیمی جریان';
    changedSchema.find(s => s.id === 26).max = 123;
    const schemaDiff = win.eval('xdiff')(changedSchema).join('\n');
    check(schemaDiff.indexOf('شناسهٔ 25') >= 0 && schemaDiff.indexOf('نام از') >= 0,
          'restore identifies a changed parameter name by id');
    check(schemaDiff.indexOf('شناسهٔ 26') >= 0 && schemaDiff.indexOf('max') >= 0,
          'restore identifies a changed parameter limit by id');
    [15, 16, 17, 18, 19].forEach(id => {
        check(o.params[String(id)] === undefined, 'the momentary id ' + id + ' stays out of the backup');
    });

    /* --- the complete backup keeps pending edits, raw bench rows and the
       last real LUT readback in the same schema. Export must not substitute a
       generated proposal for the board's readback. --- */
    const fullBlobs = [];
    const OldBlobFull = win.Blob;
    win.Blob = function (parts, opts) { fullBlobs.push(String(parts[0])); return new OldBlobFull(parts, opts); };
    win.D = { on: 1, p: { 0: 12, 1: 13, 2: 1000, 3: 1000, 4: 0, 5: 0, 6: 0 }, t: [] };
    win.PEND = { 0: 19, 127: 1 };
    win.CALS = [{ sc: 'BOTH', d: 5, use: 1, r1: 125, r2: 125, vin: 24000, v24: 25000,
                  v12: 12500, vlo: 12500, vhi: 12500, b1: 100, b2: 100, ts: 2 }];
    win.eval("LUT_LAST_AFTER = { ready: 1, source: 'readback', T: [{ X: [100, 200], Y: [110, 220] }, { X: [100], Y: [120] }] }");
    await win.eval('xexp')();
    win.Blob = OldBlobFull;
    const full = JSON.parse(fullBlobs[0]);
    check(full.v === 4 && full.params['0'] === 12 && full.params['6'] === 0,
          'a complete backup keeps the applied calibration values separately from pending values');
    check(full.pending['0'] === 19 && full.pending['127'] === 1,
          'a complete backup keeps queued values that have not reached the board');
    check(full.benchSamples.length === 1 && full.benchSamples[0].r1 === 125,
          'a complete backup keeps the raw bench calibration row');
    check(full.lut && full.lut.T[0].Y[1] === 220,
          'a complete backup keeps the last LUT read from the board');

    /* --- an out-of-range number from a hand-edited file is pulled back --- */
    check(win.eval('xclamp')(2, 99999) === 3000, 'an impossible gain from a file is clamped to its maximum');
    check(win.eval('xclamp')(0, -5) === 0, 'a negative offset from a file is clamped to zero');

    /* --- applied values must replace stale visible calibration/filter boxes ---
       [FA] مقدار اعمال‌شده باید کادرهای قدیمی کالیبراسیون/فیلتر را جایگزین کند. */
    win.D = { p: { 0: 17, 1: 19, 2: 1200, 3: 1300, 7: 5, 8: 20, 27: 14800 }, t: [] };
    win.PEND = {};
    /* The previous scenario may leave its last input focused. The production
       rule intentionally preserves a focused field, so blur it before testing
       an incoming telemetry refresh. */
    if (doc.activeElement && typeof doc.activeElement.blur === 'function') {
        doc.activeElement.blur();
    }
    doc.getElementById('q0').value = '7';
    doc.getElementById('q7').value = '3';
    doc.getElementById('q27').value = '14000';
    win.eval('cfill')();
    win.eval('qfill')();
    win.eval('afill')();
    check(doc.getElementById('q0').value === '17' &&
          doc.getElementById('q2').value === '1200',
          'refresh replaces stale current offset/gain boxes with applied board values');
    check(doc.getElementById('q7').value === '5',
          'refresh replaces a stale median-filter value with the applied board value');
    check(doc.getElementById('q27').value === '14800',
          'refresh replaces a stale alarm value with the applied board value');
    check(Math.abs(win.eval('K24') - (3300 / 4095 * 68000 / 6800)) < 1e-9 &&
          Math.abs(win.eval('K24B') - (3300 / 4095 * 68000 / 6800)) < 1e-9 &&
          Math.abs(win.eval('K12') - (3300 / 4095 * 34398 / 6800)) < 1e-9,
          'the panel uses the live BSP divider constants, not historical voltage factors');
    win.PEND[0] = 99;
    doc.getElementById('q0').value = '99';
    win.eval('cfill')();
    check(doc.getElementById('q0').value === '99',
          'an explicitly queued local edit is not overwritten by telemetry');
    win.PEND = {};

    /* --- calibration: fit once, then show a named batch summary --------- */
    const gain = 1200, off = 7;
    win.CALS = [];
    for (let duty = 2; duty <= 20; duty += 2) {
        const raw = off + duty * 25;
        const mA = (raw - off) * K * gain / 1000;
        win.CALS.push({ sc: 'BOTH', d: duty, use: 1, r1: raw, r2: raw, vin: 24000, v24: 25000,
                        v12: 12500, vlo: 12500, vhi: 12500,
                        b1: mA, b2: mA, dvi: 24300, dv1: 12600, dv2: 12700, ts: 1 });
    }
    win.D = { p: { 0: 0, 1: 0, 2: 1000, 3: 1000, 4: 0, 5: 0, 6: 0 }, t: [] };
    win.eval('calrun')();
    const prop = {};
    win.CALR.forEach(r => { prop[Number(r[1])] = Number(r[3]); });
    check(prop[2] === gain, 'the fit recovers the current gain of channel 1 exactly');
    check(prop[3] === gain, 'the fit recovers the current gain of channel 2 exactly');
    check(prop[0] === off, 'the fit recovers the zero-current offset of channel 1');
    check(prop[4] === 300, 'the input-voltage offset is the mean multimeter difference');
    check(prop[5] === 300, 'the 24 V pack offset uses the sum of the two halves');
    check(prop[6] === 200, 'the 12 V node offset uses the lower half');
    check(doc.querySelectorAll('#calst .calsummaryrow').length === 7 &&
          doc.getElementById('calst').textContent.indexOf('گین') >= 0 &&
          doc.getElementById('calst').textContent.indexOf('ولتاژ ورودی') >= 0,
          'the result is a named status summary containing every calibration parameter');
    const panelCss = Array.from(doc.querySelectorAll('style')).map(s => s.textContent).join('\n');
    check(doc.getElementById('calst').classList.contains('cal-summary') &&
          panelCss.indexOf('.calstatus.cal-summary') >= 0 &&
          panelCss.indexOf('border-color:transparent!important') >= 0,
          'a calibration summary removes the outer error frame while retaining the inner orange summary frame');
    check(!doc.getElementById('caltb') && !doc.querySelector('#calst input'),
          'the old calibration result table and its per-row editors are gone');
    check(doc.getElementById('calst').textContent.indexOf('در انتظار تأیید کاربر') >= 0,
          'nothing is written until the user confirms the complete batch');

    /* A sample-selection edit invalidates the previous fit immediately; the
       old numbers must not remain usable until the user runs the fit again.
       [FA] تغییر انتخاب نمونه باید همان لحظه برازش قبلی را باطل کند. */
    win.eval('caluse')(0, false);
    check(win.CALR.length === 0 &&
          doc.getElementById('calst').textContent.indexOf('نتیجهٔ قبلی دیگر معتبر نیست') >= 0,
          'changing sample selection invalidates the old calibration result');
    win.eval('caluse')(0, true);
    win.eval('calrun')();

    /* --- v1.58: one confirmation, no per-parameter ticks --------------- */
    check(!doc.querySelector('[id^="calv"]') && !doc.querySelector('[id^="calk"]') &&
          !doc.querySelector('[id^="cald"]'),
          'the calibration result no longer exposes per-parameter inputs, ticks or diffs');
    check(doc.getElementById('calmodal') && doc.getElementById('calmodalBody') &&
          doc.getElementById('calmodalBody').className.indexOf('calmodalbody') >= 0,
          'the one-shot confirmation dialog has a dedicated scrollable body');

    /* --- v1.60: a sample knows its scenario and can be excluded by hand --- */
    win.eval('calsmp')();
    check(doc.getElementById('calsl').innerHTML.indexOf('هر دو باتری') >= 0,
          'the sample list names the scenario in plain words, not SOLO');
    check(doc.getElementById('calsl').innerHTML.indexOf('SOLO') < 0,
          'the word SOLO is gone from what the user reads');
    check(doc.getElementById('calall') && doc.getElementById('calall').type === 'checkbox' &&
          doc.querySelectorAll('#calsl button').length === 0,
          'the sample list has one master checkbox above the rows and no select-all buttons');
    win.eval('calpick')(0);
    check(win.CALS.every(z => z.use === 0) && doc.getElementById('calall').checked === false,
          'unchecking the master sample checkbox excludes every row');
    check(win.CALR.length === 0,
          'unchecking all samples also invalidates the previous fit');
    win.eval('calpick')(1);
    check(win.CALS.every(z => z.use !== 0) && doc.getElementById('calall').checked === true,
          'checking the master sample checkbox includes every row again');
    win.CALS.push({ sc: 'BAT1', d: 30, use: 1, r1: 900, r2: 900, vin: 24000, v24: 25000,
                    v12: 12500, vlo: 12500, vhi: 12500, b1: 10, b2: 10,
                    dvi: 24300, dv1: 12600, dv2: 12700, ts: 1 });   /* an obvious outlier */
    win.eval('calrun')();
    const spoiled = Number(win.CALR.find(r => Number(r[1]) === 2)[3]);
    win.eval('caluse')(win.CALS.length - 1, false);
    win.eval('calrun')();
    check(spoiled !== gain && Number(win.CALR.find(r => Number(r[1]) === 2)[3]) === gain,
          'unticking a bad row takes it straight out of the maths');
    check(win.CALS[win.CALS.length - 1].use === 0,
          'an unticked row is kept in the file, only excluded from the fit');
    /* a battery-2-only row must not touch the channel-1 fit */
    win.CALS[win.CALS.length - 1] = { sc: 'BAT2', d: 30, use: 1, r1: 900, r2: 900,
        vin: 24000, v24: 25000, v12: 12500, vlo: 12500, vhi: 12500, b1: 10, b2: null,
        dvi: 24300, dv1: 12600, dv2: 12700, ts: 1 };
    win.eval('calrun')();
    check(Number(win.CALR.find(r => Number(r[1]) === 2)[3]) === gain,
          'a battery-2-only row is ignored when fitting battery 1');
    win.CALS.pop();
    win.eval('calrun')();

    /* Selecting only BAT2 narrows every green/red rule and the fit itself.
       The other battery is not allowed to turn the selected result red. */
    doc.getElementById('wcBAT1').checked = false;
    doc.getElementById('wcBAT2').checked = true;
    doc.getElementById('wcBOTH').checked = false;
    win.eval('calchk')();
    check(doc.getElementById('calck').textContent.indexOf('دامنهٔ بررسی: فقط باتری ۲') >= 0 &&
          doc.getElementById('calck').textContent.indexOf('باتری ۱:') < 0 &&
          doc.getElementById('calck').textContent.indexOf('باتری ۲:') >= 0,
          'a BAT2-only selection checks and reports only battery 2');
    win.eval('calrun')();
    check(win.CALR.some(r => Number(r[1]) === 1) && win.CALR.some(r => Number(r[1]) === 3) &&
          !win.CALR.some(r => Number(r[1]) === 0) && !win.CALR.some(r => Number(r[1]) === 2),
          'a BAT2-only fit does not produce battery-1 calibration rows');
    const savedScopeSamples = win.CALS;
    const bat2ScopeSamples = savedScopeSamples.slice(0, 4).map(z => Object.assign({}, z, { sc: 'BAT2' }));
    const noisyBat1Samples = savedScopeSamples.slice(4, 8).map(z => Object.assign({}, z, {
        sc: 'BAT1', dvi: 50000, dv1: 40000, dv2: 40000
    }));
    win.CALS = bat2ScopeSamples.concat(noisyBat1Samples);
    win.eval('calrun')();
    const scopedVoltage = Number(win.CALR.find(r => Number(r[1]) === 4)[3]);
    check(scopedVoltage === 300,
          'battery-1 voltage samples do not alter a BAT2-only calibration result');
    win.CALS = savedScopeSamples;
    ['BAT1', 'BAT2', 'BOTH'].forEach(k => { doc.getElementById('wc' + k).checked = true; });
    win.eval('calrun')();

    /* --- v1.61: the conditions are on the page, and a firmware snippet --- */
    win.eval('calchk')();
    const ruleRows = doc.querySelectorAll('#calck .ckr');
    check(ruleRows.length >= 10 &&
          [...ruleRows].every(row => row.querySelector('i') && row.textContent.trim().length > 1),
          'the rule check-list is rendered with a result for each rule');
    const keepAll = win.CALS;
    win.CALS = [keepAll[0]];
    win.eval('calchk')();
    check(doc.querySelectorAll('#calck .ckr.no').length >= 2 &&
          doc.getElementById('calck').innerHTML.indexOf('حداقل ۴') >= 0,
          'too few points is shown as a failed rule with what to do about it');
    win.CALS = keepAll;
    win.eval('calchk')();
    win.eval('calrun')();
    win.eval('calcode')();
    const code = doc.getElementById('calcd').value;
    check(code.indexOf('CAL_Current1LutChainMa[] =') >= 0 &&
          code.indexOf('CAL_Current1LutBatteryMw[] =') >= 0,
          'the generator emits the two C arrays the firmware already uses');
    check(code.indexOf('CAL_Current2LutChainMa[] =') >= 0,
          'both channels get a table');
    check(/\{ 0u,/.test(code), 'every generated table starts at the origin');
    check(code.indexOf('current gain   ch1 = 1200 permille') >= 0,
          'the snippet repeats the gain/offset the table was fitted with');
    check(doc.getElementById('calcd').style.display === 'block',
          'the snippet is shown on the page, ready to copy');

    /* --- v1.62: the voltage SLOPE is measured and reported --- */
    /* two voltage levels 2 V apart, board reading 2% low: offset cannot fix it */
    const vs = [];
    for (let i = 0; i < 6; i += 1) {
        const board = 12000 + i * 400;
        vs.push({ sc: 'BOTH', d: 2 + i, use: 1, r1: 100 + i * 50, r2: 100 + i * 50,
                  vin: board * 2, v24: board * 2, v12: board, vlo: board, vhi: board,
                  b1: 50 + i * 20, b2: 50 + i * 20,
                  dvi: Math.round(board * 2 * 1.02), dv1: Math.round(board * 1.02),
                  dv2: Math.round(board * 1.02), ts: 1 });
    }
    const keepS = win.CALS;
    win.CALS = vs;
    win.eval('calchk')();
    check(doc.getElementById('calck').innerHTML.indexOf('خطای ضریبی') >= 0,
          'a 2% scale error on the voltage is reported as a slope problem, not an offset');
    win.eval('calrun')();
    win.eval('calcode')();
    const vcode = doc.getElementById('calcd').value;
    check(vcode.indexOf('BSP_MEASUREMENT_DIV24_TOP_OHMS') >= 0 &&
          vcode.indexOf('scale error') >= 0,
          'the snippet prints the corrected divider constant for the firmware');
    check(vcode.indexOf('69496u') >= 0,
          'the corrected constant is slope x top + (slope - 1) x bottom, checked by hand');
    check(vcode.indexOf('BSP_MEASUREMENT_DIV24BAT_TOP_OHMS') >= 0,
          'the input rail and the pack rail get their own macro, not one shared line');
    win.CALS = keepS;
    win.eval('calchk')();
    win.eval('calrun')();

    /* --- v1.63: any point count is fine, but fewer than two is refused --- */
    const keepP = win.CALS;
    /* one sample sitting exactly at the zero-current point: a single anchor */
    win.CALS = [{ sc: 'BOTH', d: 2, use: 1, r1: off, r2: off, vin: 24000, v24: 25000,
                  v12: 12500, vlo: 12500, vhi: 12500, b1: 0, b2: 0,
                  dvi: 24300, dv1: 12600, dv2: 12700, ts: 1 }];
    win.eval('calrun')();
    win.eval('calcode')();
    check(doc.getElementById('calcd').value.indexOf('NOT GENERATED') >= 0 &&
          doc.getElementById('calst').textContent.indexOf('کمتر از ۲ نقطه') >= 0,
          'a table with fewer than two points is refused, not emitted');
    win.CALS = keepP;
    win.eval('calrun')();
    win.eval('calcode')();
    check(doc.getElementById('calcd').value.indexOf('any count is valid') >= 0,
          'the generated header states that the point count is free');

    /* --- v1.64: the panel proves the table is legal before emitting it --- */
    const keepN = win.CALS;
    const noisy = keepN.map(z => Object.assign({}, z));
    noisy[3].b1 = 1;            /* one point whose power dips below the previous */
    win.CALS = noisy;
    const built = win.eval('calbuild')(1, 7, 1200);
    check(built.X.length === built.Y.length,
          'the two axes of one battery always come out the same length');
    check(built.Y.every((v, i) => i === 0 || v >= built.Y[i - 1]),
          'a dipping power point is dropped, never emitted');
    check(built.note.join(' ').indexOf('نویزی') >= 0,
          'and the user is told that a noisy point was dropped');
    check(built.X.every((v, i) => i === 0 || v > built.X[i - 1]),
          'the chain axis comes out strictly increasing');
    win.eval('calcode')();
    check(doc.getElementById('calst').textContent.indexOf('نویزی') >= 0,
          'the warning survives to the status line instead of being overwritten');
    win.CALS = keepN;
    win.eval('calrun')();

    /* --- v1.64: nothing may be written while the bench wizard owns the board --- */
    const Wv = win.eval('W'); Wv.run = true;
    win.eval('calapply')();
    check(doc.getElementById('calst').textContent.indexOf('داده‌برداری بنچ در جریان') >= 0,
          'applying calibration is refused while the bench wizard is running');
    await win.eval('ximp')({ text: async () => JSON.stringify({ app: 'ChangeOver-settings', v: 2, params: { 0: 1 } }) });
    Wv.run = false;

    /* The successful path applies every valid calibration parameter in one
       confirmation, then reconciles each local owner with board readback.
       [FA] مسیر موفق باید همهٔ پارامترهای معتبر را با یک تأیید اعمال کند و
       بعد هر مالک محلی را با readback برد هماهنگ کند. */
    const applyId = 2;
    const applyRow = win.CALR.findIndex(r => Number(r[1]) === applyId);
    const oldSetv = win.eval('setv');
    const oldXexp = win.eval('xexp');
    win.eval('xexp = () => {}');
    win.eval('setv = async (id, value) => { D.p[id] = value; }');
    win.PEND = { 0: 999 };
    await win.eval('calapply')();
    await win.eval('calmodalApply')();
    check(doc.getElementById('calst').textContent.indexOf('در صف تغییرات پنل') >= 0 &&
          doc.getElementById('calst').textContent.indexOf('شناسهٔ 0') >= 0,
          'calibration refuses to overwrite a same-id pending edit');
    win.PEND = {};
    const expected = {};
    win.CALR.forEach(r => {
        if (Number(r[1]) === applyId) r[3] = 1234;
        if (Number.isFinite(Number(r[1])) && r[5] === 1 && Number.isFinite(Number(r[3]))) {
            expected[Number(r[1])] = Number(r[3]);
        }
    });
    doc.getElementById('q' + applyId).value = '1000';
    if (doc.activeElement && typeof doc.activeElement.blur === 'function') {
        doc.activeElement.blur();
    }
    await win.eval('calapply')();
    check(doc.getElementById('calmodal').className.indexOf('on') >= 0 &&
          doc.getElementById('calmodalBody').textContent.indexOf('گین') >= 0 &&
          doc.getElementById('calmodalBody').textContent.indexOf('شناسه') < 0,
          'one confirmation modal lists named calibration values without internal ids');
    await win.eval('calmodalApply')();
    check(Object.keys(expected).every(id => win.D.p[id] === expected[id]),
          'one confirmed operation writes every valid calibration parameter');
    check(win.D.p[applyId] === 1234 && win.CALR[applyRow][2] === 1234,
          'calibration apply keeps the board readback in its local row snapshot');
    check(doc.getElementById('q' + applyId).value === '1234' &&
          doc.getElementById('calmodal').className.indexOf('on') < 0,
          'calibration apply refreshes the owning filter control and closes the modal');
    check(doc.getElementById('calst').textContent.indexOf('✅') >= 0 &&
          doc.getElementById('calst').textContent.indexOf('readback') >= 0,
          'calibration apply reports successful board readback for the batch');

    /* The voltage helper must use the same awaited setter/readback path, and
       must refuse to calculate from a missing live voltage. */
    const vcalCalls = [];
    win.setv = async (id, value) => { vcalCalls.push([id, value]); win.D.p[id] = value; };
    win.confirm = () => true;
    win.D = { p: { 4: 10 }, t: [] };
    doc.getElementById('vm0').value = '13.05';
    await win.eval('vcal')(0);
    check(vcalCalls.length === 0 &&
          doc.getElementById('calst').textContent.indexOf('telemetry معتبر') >= 0,
          'voltage calibration refuses a missing live board reading');
    win.D.t[14] = 12500;
    await win.eval('vcal')(0);
    check(vcalCalls.length === 1 && vcalCalls[0][0] === 4 && vcalCalls[0][1] === 560 &&
          doc.getElementById('calst').textContent.indexOf('readback همان شناسه') >= 0,
          'voltage calibration awaits the same-id setter/readback path');
    win.setv = oldSetv;
    win.xexp = oldXexp;
    win.PEND = {};

    /* v1.81: importing a settings file must stage locally, never call /s. */
    const importUrls = [];
    win.fetch = (u) => { importUrls.push(String(u)); return Promise.resolve({ ok: true }); };
    win.confirm = () => true;
    win.D = { p: { 25: 650, 26: 50, 127: 0 }, t: [] };
    win.PEND = {};
    const importSchema = win.eval('xschema()');
    await win.eval('ximp')({ text: async () => JSON.stringify({
        app: 'ChangeOver-settings', v: 3, schema: importSchema,
        params: { 25: 700, 26: 60, 127: 1 }
    }) });
    check(importUrls.filter(u => u.indexOf('/s?') >= 0).length === 0,
          'restoring a backup does not write to the board immediately');
    check(win.PEND[25] === 700 && win.PEND[26] === 60 && win.PEND[127] === 1 &&
          doc.getElementById('db127').checked === true &&
          doc.getElementById('db127').className.indexOf('pq') >= 0 &&
          doc.getElementById('sbar').className === 'on',
          'restored changes are staged in the yellow global queue, including checkboxes');
    check(doc.getElementById('xst').textContent.indexOf('روی پنل آماده شد') >= 0,
          'restore tells the user that values are staged, not written');
    win.PEND = {};
    win.pbar();

    /* A full backup restores all three local layers together. The imported
       LUT is only retained for review; no endpoint is called and no LUT
       payload is staged as an automatic board write. */
    win.D = { p: { 0: 12, 1: 13, 2: 1000, 3: 1000, 4: 0, 5: 0, 6: 0 }, t: [] };
    win.PEND = {};
    win.CALS = [];
    win.LUT_BACKUP = null;
    await win.eval('ximp')({ text: async () => JSON.stringify(full) });
    check(importUrls.filter(u => u.indexOf('/s?') >= 0).length === 0 &&
          win.PEND[0] === 19 && win.PEND[127] === 1,
          'full restore stages pending values without writing them to the board');
    check(win.CALS.length === 1 && win.CALS[0].r1 === 125,
          'full restore brings raw calibration samples back into the sample list');
    check(win.LUT_BACKUP && win.LUT_BACKUP.T[0].Y[1] === 220 &&
          doc.getElementById('xst').textContent.indexOf('روی برد اعمال نشد') >= 0,
          'full restore reports the imported LUT and keeps it review-only');
    win.PEND = {};
    win.pbar();

    /* --- v1.65: the live table shows only what the user needs --- */
    const heads = Array.from(doc.querySelectorAll('#wT th')).map(h => h.textContent);
    if (heads.length) {
        check(heads.join('|').indexOf('raw') < 0 && heads.join('|').indexOf('iest') < 0,
              'raw counts and estimated current are no longer shown during the test');
    }
    check(win.eval('WH').join('|').indexOf('raw') < 0 &&
          win.eval('WH').join('|').indexOf('iest') < 0,
          'the column set itself drops the extra fields');
    check(win.eval('WH').filter(h => h.indexOf('(شما)') >= 0).length === 6,
          'the six numbers the user types each have their own column');
    check(win.eval('WH').filter(h => h.indexOf('(برد)') >= 0).length === 5,
          'the board readings the user needs are shown: input, both batteries, both currents');
    check(win.eval('WSTC') === win.eval('WH').length - 1,
          'the status column index follows the header list instead of a hard-coded 12');

    /* --- v1.66 bench input mode: SWEEP and manual duty are exclusive ----- */
    const sweepToggle = doc.getElementById('wSw');
    const manualDuty = doc.getElementById('wManual');
    const sweepFields = doc.getElementById('wSweep');
    const sweepStep = doc.getElementById('wStep');
    sweepToggle.checked = true;
    sweepToggle.onchange();
    check(manualDuty.className.indexOf('wmode-hidden') >= 0 &&
          sweepFields.className.indexOf('wmode-hidden') < 0 &&
          doc.getElementById('wA').disabled === false &&
          doc.getElementById('wB').disabled === false &&
          sweepStep && sweepStep.disabled === false,
          'SWEEP shows only its start, end and step controls');
    sweepToggle.checked = false;
    sweepToggle.onchange();
    check(manualDuty.className.indexOf('wmode-hidden') < 0 &&
          sweepFields.className.indexOf('wmode-hidden') >= 0 &&
          doc.getElementById('wL').disabled === false &&
          doc.getElementById('wA').disabled === true &&
          doc.getElementById('wB').disabled === true &&
          sweepStep.disabled === true,
          'turning SWEEP off shows only the manual duty list');
    sweepToggle.checked = true;
    doc.getElementById('wA').value = '2';
    doc.getElementById('wB').value = '6';
    sweepStep.value = '2';
    sweepToggle.onchange();
    check(JSON.stringify(win.eval('wsweep')()) === JSON.stringify([2, 4, 6]),
          'the SWEEP step field controls the generated duty sequence');

    /* --- v1.59: the raw bench samples can be saved and restored --- */
    const sbl = [];
    const OldBlob2 = win.Blob;
    win.Blob = function (parts, opts) { sbl.push(String(parts[0])); return new OldBlob2(parts, opts); };
    const oldCreate2 = doc.createElement.bind(doc);
    doc.createElement = (t) => { const e = oldCreate2(t); if (t === 'a') { e.click = () => {}; } return e; };
    win.eval('calexp')();
    doc.createElement = oldCreate2;
    win.Blob = OldBlob2;
    const sf = JSON.parse(sbl[sbl.length - 1]);
    check(sf.app === 'ChangeOver-bench-samples' && sf.samples.length === win.CALS.length,
          'the bench samples can be written to their own file');
    check(sf.n === win.CALS.length && typeof sf.saved === 'string',
          'the sample file says how many points it holds and when it was taken');
    const kept = win.CALS.length;
    win.CALS = [];
    await win.eval('calimp')({ text: async () => JSON.stringify(sf) });

    /* --- noisy / too few samples must be refused, not applied --- */
    win.CALS = [{ r1: 100, r2: 100, vin: 24000, v24: 25000, v12: 12500, vlo: 12500, vhi: 12500,
                  b1: 50, b2: 50, dvi: 24000, dv1: 12500, dv2: 12500, ts: 1 }];
    win.eval('calrun')();
    check(doc.getElementById('calst').textContent.indexOf('حداقل ۳') >= 0,
          'one sample is refused with a plain reason');
    win.CALS = [];
}

/* ==================== v1.56 panel-side rules / قوانین سمت پنل ==================== */

/**
 * [EN] Cross-field relationships are safety advice, not a second firmware
 *      setter. The panel must preserve every typed value until POST and let
 *      the owning MCU setter clamp/reject it; a later readback is the source
 *      of truth. This test deliberately keeps the old adversarial combinations
 *      and proves that fixrules() neither mutates them nor invents a repair.
 * [FA] رابطه‌های بین‌فیلدی توصیهٔ ایمنی‌اند، نه setter دوم فرم‌افزار. پنل باید
 *      مقدار تایپ‌شده را تا POST نگه دارد و clamp/reject را به setter برد بسپارد؛
 *      مقدار readback بعدی مرجع است. ترکیب‌های قدیمی عمداً آزمایش می‌شوند تا
 *      معلوم شود fixrules نه مقدار را تغییر می‌دهد و نه اصلاح ساختگی می‌سازد.
 */
function testFixRules(win, doc) {
    console.log('\nv1.56 advisory cross-field rules / قوانین توصیه‌ای بین‌فیلدی');

    const base = {};
    [40,41,42,43,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,62,63,64,65,66,67,68,69,
     74,75,77,78,119,120,121,122].forEach(id => { base[id] = 0; });
    const cases = [
        { 40: 10000, 42: 3, 43: 0 },
        { 40: 10000, 42: 1, 43: 0 },
        { 62: 1, 64: 1, 58: 1, 65: 0 },
        { 64: 2, 65: 0 },
        { 40: 1000, 42: 2, 43: 100, 41: 999999 },
        { 40: 0, 41: 999999 },
        { 50: 40, 51: 90, 52: 95, 53: 99 },
        { 56: 1000, 57: 10, 58: 3, 65: 100 },
        { 56: 2000, 57: 1000, 58: 5, 65: 100 },
        { 56: 200, 57: 1000, 58: 5, 65: 100 },
        { 66: 1000, 67: 5000 },
        { 68: 1000, 69: 5000 },
        { 74: 21000, 75: 20000 },
        { 119: 21000, 120: 21000 },
        { 77: 90, 78: 95 }
    ];
    cases.forEach((over, i) => {
        const v = Object.assign({}, base, over);
        const before = JSON.stringify(v);
        const fx = win.fixrules(v);
        check(JSON.stringify(v) === before && fx.length === 0,
              'cross-field case ' + (i + 1) + ' remains typed and has no panel repair');
    });

    /* q27 is independent of absorbOver and OV cutoff. Both firmware hard-edge
       values must pass the panel preflight, including the requested 14.8 V. */
    win.PEND = {};
    win.qput(27, 14800);
    check(win.pvalidate(['27']).length === 0,
          'q27 accepts 14800 mV on its independent firmware range');
    win.pclr(27);
    win.qput(27, 15000);
    check(win.pvalidate(['27']).length === 0,
          'q27 accepts the firmware upper edge at 15000 mV');
    win.pclr(27);
    doc.getElementById('q27').value = '14800';
    doc.getElementById('q36').value = '15000';
    const advisories = win.achk();
    check(!advisories.some(w => w.ids.indexOf(27) >= 0),
          'q27 has no cross-field veto when absorbOver or OV cutoff differs');
    win.PEND = {};
}

/* ==================== Runner / اجراکننده ==================== */

const dom = loadPanel();
const win = dom.window;

/* ==================== v1.66 direct LUT push / ارسال مستقیم جدول ==================== */

/**
 * [EN] The push is a handshake, not a hope: the panel CRC32s exactly what it
 *      sends, refuses to call a mismatched or rejected push a success, and
 *      never offers the reboot unless the board echoed the same CRC. The
 *      build-time route must still be there - the user asked to keep it.
 * [FA] ارسال یک «دست‌دادن» است نه امید: پنل دقیقاً همان بایت‌هایی را که
 *      می‌فرستد CRC می‌گیرد، ارسال ردشده یا ناهمخوان را موفق اعلام نمی‌کند و
 *      تا وقتی برد همان CRC را پس نداده دکمهٔ ریست را پیشنهاد نمی‌دهد. راه
 *      بیلد هم باید سر جایش بماند - خواستهٔ کاربر.
 */
async function testLutPush(win, doc) {
    console.log('\nv1.66 direct LUT push / ارسال مستقیم جدول به برد');

    const K = win.eval('K_MA');
    const gain = 1200, off = 7;
    win.CALS = [];
    for (let duty = 2; duty <= 20; duty += 2) {
        const raw = off + duty * 25;
        const mA = (raw - off) * K * gain / 1000;
        win.CALS.push({ sc: 'BOTH', d: duty, use: 1, r1: raw, r2: raw, vin: 24000, v24: 25000,
                        v12: 12500, vlo: 12500, vhi: 12500,
                        b1: mA, b2: mA, dvi: 24300, dv1: 12600, dv2: 12700, ts: 1 });
    }
    win.D = { p: { 0: off, 1: off, 2: gain, 3: gain, 4: 0, 5: 0, 6: 0 }, t: [], on: 1 };
    win.eval('calrun')();

    const packed = win.eval('lpack')();
    check(!packed.bad, 'a clean sample set packs into a sendable table');
    const nums = packed.body.split(',').map(Number);
    check(nums[0] === packed.T[0].X.length && nums[1] === packed.T[1].X.length,
          'the body starts with the two point counts');
    check(nums.length === 2 + 2 * (nums[0] + nums[1]) + 1,
          'the body carries every point pair exactly once, then the CRC');
    check(nums[nums.length - 1] === packed.crc,
          'the CRC the board will check is the last field of the body');
    check(packed.T[0].X.length <= 24 && packed.T[1].X.length <= 24,
          'the panel never offers to send more points than the board can store');

    /* The CRC must be the standard reflected CRC32 of [n][8 B per point]. */
    const bytes = [];
    packed.T.forEach(t => {
        bytes.push(t.X.length & 255);
        for (let i = 0; i < t.X.length; i++) {
            [t.X[i], t.Y[i]].forEach(v => bytes.push(v & 255, (v >>> 8) & 255, (v >>> 16) & 255, (v >>> 24) & 255));
        }
    });
    check(win.eval('lcrc')(bytes) === packed.crc,
          'the CRC covers the point bytes in the order the board rebuilds them');

    /* --- a matching handshake is a success and offers the reboot --- */
    const calls = [];
    const postedBodies = [];
    let calFail = false;
    let readBefore = packed.T.map(t => ({ X: t.X.map(v => v + 10), Y: t.Y.map(v => v + 100) }));
    let activeRows = packed.T;
    let readPhase = 0;
    let readAfterTransaction = false;
    let readMode = 'good';
    const readJson = () => {
        if (readMode === 'unavailable' && readPhase) return { ready: 0, pending: 0, error: 1 };
        const rows = readPhase ? activeRows : readBefore;
        return { ready: 1, pending: 0, error: 0,
                 n1: rows[0].X.length, n2: rows[1].X.length,
                 r1: rows[0].X.map((v, i) => [v, rows[0].Y[i]]),
                 r2: rows[1].X.map((v, i) => [v, rows[1].Y[i]]) };
    };
    const stub = (ack) => (u, o) => {
        calls.push((o && o.method ? o.method : 'GET') + ' ' + u);
        if (String(u) === '/lut/read' && o && o.method === 'POST') {
            readPhase = readAfterTransaction ? 1 : 0;
            readAfterTransaction = false;
            return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve({ ok: 1, pending: 1 }) });
        }
        if (String(u).indexOf('/s?id=') === 0 && o && o.method === 'POST') {
            if (calFail) return Promise.resolve({ ok: false, status: 503, json: () => Promise.resolve({}) });
            const q = new URL('http://panel' + String(u)).searchParams;
            const id = Number(q.get('id')), value = Number(q.get('v'));
            win.D.p[id] = value;
            return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve({ ok: 1 }) });
        }
        if (String(u) === '/lut' && o && o.method === 'POST') {
            postedBodies.push(o.body);
            readAfterTransaction = true;
            return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve({ ok: 1 }) });
        }
        if (String(u) === '/lut') {
            const answer = Object.assign({}, ack, { read: readJson() });
            return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve(answer) });
        }
        if (String(u).indexOf('/lut/reset') === 0) {
            readAfterTransaction = true;
            return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve({ ok: 1 }) });
        }
        return Promise.resolve({ ok: true, status: 200, json: () => Promise.resolve(win.D) });
    };
    const oldConfirm = win.confirm;
    const applyPending = async (name) => {
        check(!!doc.getElementById('lutApply') &&
              doc.querySelectorAll('#lutcmp input.lutcalcheck').length > 0,
              name + ': the change audit exposes an apply action and calibration checkboxes');
        await win.eval('lapplypending')();
    };
    const ackOk = { st: 3, s: 0, n1: packed.T[0].X.length, n2: packed.T[1].X.length,
                    crc: packed.crc, sent: packed.crc, age: 10, n: 3 };

    win.PEND = { 0: off + 1 };
    calls.length = 0;
    await win.eval('lsend')(1);
    check(doc.getElementById('calst').textContent.indexOf('در صف تغییرات پنل') >= 0 &&
          doc.getElementById('calst').textContent.indexOf('شناسهٔ 0') >= 0,
          'a LUT update refuses to overwrite a pending calibration id');
    check(calls.length === 0, 'a pending calibration conflict does not even start LUT readback');
    win.PEND = {};
    win.pbar();

    /* The operator accepts the push but declines the reboot for now. */
    win.confirm = (m) => String(m).indexOf('ریست شود') < 0;
    win.fetch = stub(ackOk);
    await win.eval('lsend')();
    const calChecks = Array.from(doc.querySelectorAll('#lutcmp input.lutcalcheck'));
    check(calChecks.length === 7 && calChecks.every(x => x.checked),
          'every in-scope calibration value is checked by default');
    check(doc.querySelector('#lutcmp .lutchange') !== null &&
          doc.getElementById('lutcmp').innerHTML.indexOf('rgba(247,148,30') < 0,
          'the change audit uses the orange change-card class instead of an inline red frame');
    const keepVoltage = doc.querySelector('#lutcmp input[data-cal-id="4"]');
    keepVoltage.click();
    check(!doc.querySelector('#lutcmp input[data-cal-id="4"]').checked &&
          doc.getElementById('lutcmp').textContent.indexOf('حفظ می‌شود') >= 0,
          'unticking one calibration value marks it to be preserved');
    await applyPending('the first LUT transaction');
    check(!calls.some(c => c.indexOf('/s?id=4&') >= 0),
          'an unchecked calibration value is absent from the setter payload');
    check(doc.getElementById('calst').textContent.indexOf('✅') >= 0,
          'a matching CRC and point-by-point readback are reported as a real success');
    check(doc.getElementById('calst').classList.contains('cal-ok'),
          'a successful transaction uses the application success status theme');
    check(doc.getElementById('lutcmp').textContent.indexOf('فعلی روی برد') >= 0 &&
          doc.getElementById('lutcmp').textContent.indexOf('پیشنهادی برای ارسال') >= 0 &&
          doc.getElementById('lutcmp').textContent.indexOf('پس از commit') >= 0,
          'the LUT audit keeps current, proposed and post-commit columns visible');
    check(doc.getElementById('lutcmp').textContent.indexOf('قبل از ارسال') >= 0 &&
          doc.getElementById('lutcmp').textContent.indexOf('پیشنهاد پنل') >= 0 &&
          doc.getElementById('lutcmp').textContent.indexOf('بعد از اعمال') >= 0 &&
          doc.getElementById('lutcmp').textContent.indexOf('ID 0') >= 0 &&
          doc.getElementById('lutcmp').textContent.indexOf('readback همان شناسه') >= 0,
          'the calibration audit names each id and maps board-before, panel-proposal and board-after values');
    check(doc.getElementById('lutcmp').textContent.indexOf('باتری ۲') >= 0 &&
          doc.getElementById('lutcmp').textContent.indexOf('تغییر کرد و تأیید شد') >= 0,
          'battery 2 is compared point by point and a changed row is marked');
    check(calls.filter(c => c.indexOf('/lut/reset') >= 0).length === 0,
          'declining the reboot leaves the board running on the table it just stored');

    /* Same push, reboot accepted. */
    calls.length = 0;
    win.confirm = () => true;
    win.fetch = stub(ackOk);
    await win.eval('lsend')();
    await applyPending('the rebooted LUT transaction');
    check(calls.filter(c => c.indexOf('POST /lut/reset') >= 0).length === 1,
          'the confirmed reboot is sent so every module starts on the new table');
    check(calls.filter(c => c === 'GET /t').length >= 1,
          'after reset the panel gets a fresh calibration telemetry readback before declaring persistence');

    /* --- the read-only board view is useful on its own ------------------- */
    calls.length = 0;
    readPhase = 0;
    readMode = 'good';
    win.fetch = stub(ackOk);
    await win.eval('lreadnow')();
    check(doc.getElementById('lutcmp').textContent.indexOf('جدول فعال واقعی روی برد') >= 0 &&
          doc.getElementById('lutcmp').textContent.indexOf('chainMa') >= 0 &&
          doc.getElementById('lutcmp').textContent.indexOf('powerMw') >= 0,
          'a standalone board readback shows clear chainMa/powerMw tables');
    check(doc.getElementById('lutcmp').textContent.indexOf('پیشنهادی برای ارسال') < 0,
          'the standalone board view does not mix empty audit columns into the actual values');
    check(doc.getElementById('lutcmp').textContent.indexOf('مقدار واقعی روی برد') >= 0 &&
          doc.getElementById('lutcmp').textContent.indexOf('telemetry معتبر STM32') >= 0 &&
          doc.getElementById('lutcmp').textContent.indexOf('ID 6') >= 0,
          'the standalone board view explains the source of calibration readback and names every id');

    /* --- a CRC that does not match is NOT a success and reboots nothing --- */
    calls.length = 0;
    win.fetch = stub({ st: 3, s: 0, n1: 4, n2: 3, crc: (packed.crc ^ 1) >>> 0, sent: packed.crc, age: 10, n: 3 });
    await win.eval('lsend')();
    await applyPending('the CRC-mismatch LUT transaction');
    check(doc.getElementById('calst').textContent.indexOf('دست‌دادن نخواند') >= 0,
          'a CRC mismatch is called out instead of being hidden');
    check(doc.getElementById('calst').textContent.indexOf('جدول قبلی بدون تغییر ماند') >= 0,
          'and the user is told the previous table is untouched');
    check(calls.filter(c => c.indexOf('/lut/reset') >= 0).length === 0,
          'nothing reboots the board after a failed push');

    /* --- a board-side refusal is reported with its reason --- */
    win.fetch = stub({ st: 3, s: 5, n1: 0, n2: 0, crc: 0, sent: packed.crc, age: 10, n: 3 });
    await win.eval('lsend')();
    await applyPending('the board-refused LUT transaction');
    check(doc.getElementById('calst').textContent.indexOf('محور توان') >= 0,
          'the board status code is translated into a plain reason');

    /* A successful ACK without a value readback is not promoted to success. */
    readMode = 'unavailable';
    /* A rejected previous transaction did not perform its post-commit read,
       so do not let the stub leak that one-shot after flag into this new
       transaction's required before-read. */
    readAfterTransaction = false;
    win.fetch = stub(ackOk);
    win.confirm = () => true;
    await win.eval('lsend')();
    await applyPending('the post-commit-readback-failure LUT transaction');
    check(doc.getElementById('calst').textContent.indexOf('بازخوانی عددبه‌عدد بعد از commit ناموفق') >= 0,
          'a missing active-table readback is shown as a failure, not hidden by CRC');
    check(doc.getElementById('lutcmp').textContent.indexOf('بازخوانی ناموفق') >= 0,
          'the table marks unavailable post-commit values visibly');
    readMode = 'good';

    /* --- zero points are a valid clear, not an unchanged result ---------
       [EN] A non-zero active override followed by a zero-point readback is
            a real deletion of that override. A malformed/empty source table
            is also intentionally encoded as zero points for that channel;
            this test makes the destructive meaning visible instead of
            claiming that the channel was not sent.
       [FA] بازخوانی صفرنقطه‌ای پس از override غیرصفر، حذف واقعی override
            است و نباید «بدون تغییر» نمایش داده شود. جدول نامعتبر/خالی نیز
            عمداً برای آن کانال صفرنقطه‌ای رمز می‌شود؛ تست معنای حذف را
            آشکار می‌کند تا پیام دروغین «فرستاده نشد» برنگردد. */
    {
        const clear = win.eval('lpost')([100, 200], null, null, true, true, '');
        check(clear.c === 'changed' && clear.t.indexOf('تغییر کرد و تأیید شد') >= 0,
              'before non-zero, proposed zero and after zero are marked changed/confirmed');
        const extra = win.eval('lpost')([100, 200], null, [100, 200], true, true, '');
        check(extra.c === 'bad' && extra.t.indexOf('اضافه') >= 0,
              'a zero-point proposal with a non-zero post-readback is a mismatch');

        const savedSamples = win.CALS;
        win.CALS = savedSamples.map(z => Object.assign({}, z, { sc: 'BAT1' }));
        const partial = win.eval('lpack')();
        const partialNumbers = partial.body.split(',').map(Number);
        check(!partial.bad && partial.T[1].X.length === 0 && partialNumbers[1] === 0,
              'an unusable battery-2 source is encoded as a valid zero-point channel');
        check(partial.msg.join(' ').indexOf('override قبلی را حذف می‌کند') >= 0,
              'the invalid-channel warning says zero points remove the old override');
        win.CALS = savedSamples;

        const proposed = { bad: false, T: [{ X: [], Y: [] }, { X: [], Y: [] }], body: 'zero' };
        const before = { ready: true, T: [{ X: [100], Y: [200] }, { X: [300], Y: [400] }] };
        const after = { ready: true, T: [{ X: [], Y: [] }, { X: [], Y: [] }] };
        win.eval('lrender')(proposed, before, after);
        check(doc.getElementById('lutcmp').textContent.indexOf('تغییر کرد و تأیید شد') >= 0,
              'the DOM audit shows the non-zero to zero-point transition as confirmed');
    }

    /* --- one battery update carries the other battery unchanged ---------
       [EN] This is the regression the original all-table sender missed:
            changing battery 1 must send its proposed gain/offset and LUT,
            while battery 2 is copied from the fresh board readback.
       [FA] این رگرسیون مسیر قبلی است: تغییر باتری ۱ باید گین/آفست و LUT
            پیشنهادی خودش را بفرستد، اما باتری ۲ از readback تازه بدون تغییر
            حمل شود. */
    {
        const savedSamples = win.CALS;
        win.CALS = savedSamples.map(z => Object.assign({}, z, { sc: 'BAT1' }));
        win.eval('calrun')();
        const idxOff1 = win.CALR.findIndex(r => Number(r[1]) === 0);
        const idxGain1 = win.CALR.findIndex(r => Number(r[1]) === 2);
        win.CALR[idxOff1][3] = Number(win.D.p[0]) + 1;
        win.CALR[idxGain1][3] = Number(win.D.p[2]) + 1;
        readBefore = [
            { X: packed.T[0].X.map(v => v + 20), Y: packed.T[0].Y.map(v => v + 200) },
            { X: [900, 1000], Y: [1800, 2200] }
        ];
        readPhase = 0;
        readMode = 'good';
        const onePreview = win.eval('lpack')(1, { ready: true, T: readBefore });
        activeRows = onePreview.T;
        const ackOne = { st: 3, s: 0, n1: onePreview.T[0].X.length,
                         n2: onePreview.T[1].X.length, crc: onePreview.crc,
                         sent: onePreview.crc, age: 10, n: 3 };
        calls.length = 0;
        postedBodies.length = 0;
        win.confirm = (m) => String(m).indexOf('ریست شود') < 0;
        win.fetch = stub(ackOne);
        await win.eval('lsend')(1);
        check(doc.getElementById('lutcmp').textContent.indexOf('فقط باتری ۱') >= 0 &&
              doc.getElementById('lutcmp').textContent.indexOf('باتری ۲') < 0,
              'a single-battery audit renders only the selected battery');
        await applyPending('the single-battery LUT transaction');
        check(doc.getElementById('calst').textContent.indexOf('✅') >= 0,
              'a single-battery update completes with calibration and LUT readback');
        check(calls.some(c => c.indexOf('/s?id=0&') >= 0) && calls.some(c => c.indexOf('/s?id=2&') >= 0) &&
              [4, 5, 6].every(id => calls.some(c => c.indexOf('/s?id=' + id + '&') >= 0)) &&
              !calls.some(c => c.indexOf('/s?id=1&') >= 0) && !calls.some(c => c.indexOf('/s?id=3&') >= 0),
              'battery 1 sends its own offset/gain plus every valid global voltage offset');
        const oneBody = postedBodies[postedBodies.length - 1].split(',').map(Number);
        check(oneBody[0] === onePreview.T[0].X.length &&
              oneBody[1] === readBefore[1].X.length,
              'battery 2 stays in the transaction with exactly its fresh readback point count');
        check(doc.getElementById('lutcmp').textContent.indexOf('باتری ۲') < 0,
              'the unselected battery does not create a status or warning in the selected-battery audit');

        /* A calibration endpoint failure is a hard stop: no LUT POST may
           follow a missing/mismatched gain or offset readback. */
        win.CALR[idxOff1][3] = Number(win.D.p[0]) + 1;
        calFail = true;
        readAfterTransaction = false;
        calls.length = 0;
        postedBodies.length = 0;
        win.confirm = () => true;
        await win.eval('lsend')(1);
        await applyPending('the failed-calibration LUT transaction');
        check(doc.getElementById('calst').textContent.indexOf('پارامترهای کالیبراسیون') >= 0 &&
              doc.getElementById('calst').textContent.indexOf('جدول LUT ارسال نشد') >= 0,
              'a calibration readback failure is reported before LUT commit');
        check(!calls.some(c => c === 'POST /lut'),
              'a failed calibration readback sends no LUT frame');
        calFail = false;
        win.CALS = savedSamples;
        activeRows = packed.T;
        readBefore = packed.T.map(t => ({ X: t.X.slice(), Y: t.Y.slice() }));
        readPhase = 0;
    }

    /* --- the wizard still owns the board while it runs --- */
    const Wv = win.eval('W'); Wv.run = true;
    await win.eval('lsend')();
    check(doc.getElementById('calst').textContent.indexOf('داده‌برداری بنچ در جریان') >= 0,
          'pushing a table is refused while the bench wizard owns the board');
    Wv.run = false;
    win.confirm = oldConfirm;

    /* --- option (c) is still there: generate the header and rebuild --- */
    win.eval('calcode')();
    check(doc.getElementById('calcd').value.indexOf('CAL_Current1LutChainMa') >= 0,
          'the build-time route still produces calibration.h - the user asked to keep it');
    check(doc.getElementById('lbtnS1') && doc.getElementById('lbtnS2') &&
          doc.getElementById('lbtnRead') && doc.querySelector('button[onclick="calcode()"]'),
          'the calibration card offers readback plus separate battery update buttons');
    check(doc.querySelector('button[onclick="calapply()"]') &&
          doc.querySelector('button[onclick="calapply()"]').textContent.indexOf('فقط پارامترهای کالیبراسیون') >= 0 &&
          doc.querySelector('button[onclick="calapply()"]') !== doc.getElementById('lbtnSA'),
          'the standalone calibration action is named explicitly and is not a duplicate LUT apply button');
    check(doc.getElementById('calst').className.indexOf('calstatus') >= 0 &&
          doc.getElementById('calst').compareDocumentPosition(doc.getElementById('lutcmp')) & 4,
          'calibration and LUT messages live in a themed RTL status box before the readback card');
}


setTimeout(async () => {
    const doc = win.document;
    console.log('ESP panel scenario-card tests - the real generated page in a DOM');
    console.log('='.repeat(70));
    try {
        testStructure(win, doc);
        testOvervoltage(win, doc);
        testDisconnect(win, doc);
        testDischarge(win, doc);
        testCharging(win, doc);
            testImbalance(win, doc);
        testSimulator(win, doc);
        await testSendQueue(win, doc);
        testFixRules(win, doc);
        await testBackupAndCal(win, doc);
        await testLutPush(win, doc);
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
