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

    check(doc.querySelectorAll('#ucard4 .qmv[data-q="74"]').length === 1 &&
          doc.querySelectorAll('#ucard5 .qmv[data-q="75"]').length === 1,
        'the charge and low-battery cards echo it read-only');
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
    check(owns(1, 52) && owns(1, 63) && owns(1, 121) && owns(1, 122) && echoes(1, 54),
        'band 2 owns its duration and gap, and echoes only the shared interval');
    check(!echoes(1, 59) && !echoes(1, 65),
        'band 2 no longer borrows band 1 duration or the common gap');
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
    typeInto(win, doc, 'q121', 1000);
    typeInto(win, doc, 'q122', 100);

    typeInto(win, doc, 'q65', 250);
    win.qmfill();
    const gapEchoes = [...card.querySelectorAll('.qmv[data-q="65"]')];
    check(gapEchoes.length === 2 && gapEchoes.every(e => e.textContent === '250'),
        'the common gap is echoed by band 3 and the critical block only',
        gapEchoes.map(e => e.textContent).join(','));
    check(!textOf(doc, 's3n2').includes('250'),
        'and it no longer reaches band 2');
    typeInto(win, doc, 'q65', 100);
    win.qmfill();

    /* [EN] Factory default under every field, from the reset tables.
       [FA] پیش‌فرض کارخانه زیر هر فیلد، از جدول دکمه‌های بازگردانی. */
    const noDefault = [];
    for (let k = 1; k <= 6; k += 1) {
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
    win.D = { p: { 65: 250, 66: 1000 } };
    win.afill();
    check(doc.getElementById('a65').textContent.includes('250')
        && doc.getElementById('a65').textContent.includes('روی برد'),
        'but a board value that disagrees is still reported',
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
    check(/150 ms.*850 ms/.test(textOf(doc, 's3n2')),
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
    /* [EN] The board divides integers: 21200 on a 21000..29000 ladder is 2%,
       not the 3% a rounding panel would print.
       [FA] برد تقسیم صحیح می‌کند: ۲۱۲۰۰ روی نردبان ۲۱..۲۹ ولت می‌شود ۲٪. */
    typeInto(win, doc, 'q72', 21200);
    typeInto(win, doc, 'q73', 21400);
    check(textOf(doc, 's5v').includes('2٪') && !textOf(doc, 's5v').includes('3٪'),
        'percent is floored exactly like the board, not rounded', textOf(doc, 's5v'));
    typeInto(win, doc, 'q72', 22600);
    typeInto(win, doc, 'q73', 23400);
    check(line.includes('800'), 'the anti-chatter width is computed', line);

    typeInto(win, doc, 'q73', 22000);
    check(/⚠/.test(textOf(doc, 's5v')), 'a clear level below the alarm level is called out', textOf(doc, 's5v'));
    typeInto(win, doc, 'q73', 21200);
    typeInto(win, doc, 'q72', 21000);

    /* [EN] The ladder itself lives on card 4 now - card 5 must say so.
       [FA] خود نردبان حالا در کارت ۴ است و کارت ۵ باید همین را بگوید. */
    const card5 = doc.getElementById('ucard5');
    check(doc.querySelectorAll('#ucard5 input#q74').length === 0, 'the ladder is not duplicated on card 5');
    check(/کارت/.test(card5.textContent) && card5.querySelector('button[onclick="usel(3)"]') !== null,
        'card 5 points at the card that owns the ladder (card 3)');
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

    for (let n = 1; n <= 6; n += 1) {
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

    /* Card 5: flag only, with the hysteresis window honoured. */
    typeInto(win, doc, 'q72', 21000);
    typeInto(win, doc, 'q73', 21200);
    doc.getElementById('simp5').value = 20000;
    win.simrun();
    check(on('sl5r'), 'below the alarm level card 5 raises the flag');
    doc.getElementById('simp5').value = 22000;
    win.simrun();
    check(!on('sl5r'), 'above the clear level the flag drops');
    doc.getElementById('simp5').value = 21100;
    win.simrun();
    check(doc.getElementById('sl5t').textContent.indexOf('ضدلرزش') >= 0,
          'inside the window the card says the state is held');

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
    win.simrst6();
    doc.getElementById('simp6a').value = 12000;
    doc.getElementById('simp6b').value = 12000;
    doc.getElementById('sim6m').value = 'r';
    typeInto(win, doc, 'q108', 300);
    typeInto(win, doc, 'q109', 500);
    typeInto(win, doc, 'q110', 600000);
    typeInto(win, doc, 'q111', 600000);
    typeInto(win, doc, 'q112', 30000);
    typeInto(win, doc, 'q114', 3);
    win.sim6mode();
    win.simrun();
    check(doc.getElementById('sim6w').textContent.indexOf('تا باز شدن پنجره') >= 0,
          'at rest the window is still shut right after a charge');
    check(doc.getElementById('sl6t').textContent.indexOf('پنجره بسته') >= 0,
          'and nothing is counted while it is shut');

    doc.getElementById('sim6m').value = 'd';
    win.sim6mode();
    win.simrun();
    check(doc.getElementById('sim6w').textContent.indexOf('دشارژ') >= 0 &&
          doc.getElementById('sim6w').textContent.indexOf('500 mV') >= 0,
          'on battery the window is open at once and uses the discharge limit');

    doc.getElementById('sim6m').value = 'c';
    win.sim6mode();
    typeInto(win, doc, 'q111', 0);
    win.simrun();
    check(doc.getElementById('sim6w').textContent.indexOf('هرگز') >= 0,
          'a zero charge gate means no measurement while charging');
    typeInto(win, doc, 'q111', 600000);

    /* virtual clock: only runs while the card runs, and honours the speed */
    const t0 = win.SIMT[6];
    win.SIMON[6] = 0;
    win.simrun();
    check(win.SIMT[6] === t0, 'a stopped card freezes its own clock');
    win.SIMON[6] = 1;

    /* three events on the discharge window latch the lock */
    doc.getElementById('sim6m').value = 'd';
    win.sim6mode();
    doc.getElementById('simp6b').value = 11000;    /* 1000 mV apart */
    for (let i = 0; i < 6; i += 1) { win.SIMT[6] += 30000; win.simrun(); }
    check(win.S6.lock === true && on('sl6r'),
          'three stable over-limit events latch the lock: solid red');
    win.simrst6();
    doc.getElementById('simp6b').value = 12000;
    win.simrun();
    check(!on('sl6r') && win.S6.n === 0, 'the restart button clears the simulated lock');

    /* v1.55: a gap box is dead while its band asks for a single beep. */
    typeInto(win, doc, 'q42', 1);
    win.simrun();
    check(doc.getElementById('q43').disabled === true,
          'one beep per round switches that gap box off');
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
          'one press posts every staged edit once');
    check(Object.keys(win.PEND).length === 0, 'an accepted batch leaves the queue empty');
    const st = doc.getElementById('sbst').textContent;
    check(st.indexOf('گیره') >= 0 && st.indexOf('39: 60→50') >= 0,
          'the report names the value the board clamped', st);
    win.qput(38, 2000);
    win.D.p[38] = 2000;
    await win.sendall();
    check(doc.getElementById('sbst').textContent.indexOf('عیناً پذیرفت') >= 0,
          'a clean batch is reported as accepted and stored');
}



/* ==================== v1.57 backup identity + bench calibration ==================== */

/**
 * [EN] The backup file must carry an identity (build, parameter count, date)
 *      and the bench calibration must turn a known straight line of samples
 *      back into the exact gain/offset that produced it.
 * [FA] فایل پشتیبان باید شناسنامه داشته باشد و کالیبراسیون بنچ باید از روی
 *      نمونه‌های یک خط معلوم، همان گین و آفست سازندهٔ آن خط را دربیاورد.
 */
function testBackupAndCal(win, doc) {
    console.log('\nv1.57 backup identity + bench calibration / شناسنامهٔ پشتیبان و کالیبراسیون');

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
    const XIDS = win.eval('XIDS'), PN = win.eval('PN'), K = win.eval('K_MA');
    XIDS.forEach(id => { win.D.p[id] = 1; });
    win.eval('xexp')();
    doc.createElement = oldCreate;
    win.Blob = OldBlob;
    check(blobs.length === 1, 'the export button produces exactly one file');
    const o = JSON.parse(blobs[0]);
    check(o.app === 'ChangeOver-settings' && o.v === 2, 'the file says what it is and which layout it uses');
    check(typeof o.build === 'string' && o.build.length > 0, 'the file records the panel build it came from');
    check(o.pn === PN, 'the file records how many parameters that build had');
    check(typeof o.saved === 'string' && o.saved.indexOf('T') > 0, 'the file records when it was taken');
    check(Object.keys(o.params).length === XIDS.length, 'every backed-up id is in the file');
    check(o.params['76'] === undefined, 'the live-only id 76 stays out of the backup');
    [15, 16, 17, 18, 19].forEach(id => {
        check(o.params[String(id)] === undefined, 'the momentary id ' + id + ' stays out of the backup');
    });

    /* --- an out-of-range number from a hand-edited file is pulled back --- */
    check(win.eval('xclamp')(2, 99999) === 3000, 'an impossible gain from a file is clamped to its maximum');
    check(win.eval('xclamp')(0, -5) === 0, 'a negative offset from a file is clamped to zero');

    /* --- calibration: feed a perfect line and demand the numbers back --- */
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
    win.CALR.forEach((r, i) => { prop[r[1]] = +doc.getElementById('calv' + i).value; });
    check(prop[2] === gain, 'the fit recovers the current gain of channel 1 exactly');
    check(prop[3] === gain, 'the fit recovers the current gain of channel 2 exactly');
    check(prop[0] === off, 'the fit recovers the zero-current offset of channel 1');
    check(prop[4] === 300, 'the input-voltage offset is the mean multimeter difference');
    check(prop[5] === 300, 'the 24 V pack offset uses the sum of the two halves');
    check(prop[6] === 200, 'the 12 V node offset uses the lower half');
    check(doc.getElementById('caltb').innerHTML.indexOf('1200') >= 0,
          'the preview table shows the proposed number before anything is written');
    check(doc.getElementById('calst').textContent.indexOf('تیک‌خورده') >= 0,
          'nothing is written until the user presses apply');
    /* --- v1.58: every proposal is editable and tickable --- */
    check(doc.getElementById('calv0') && doc.getElementById('calv0').tagName === 'INPUT',
          'each proposed number is an input box the user can correct');
    check(doc.getElementById('calk0') && doc.getElementById('calk0').type === 'checkbox',
          'each line has its own apply tick');
    check(doc.querySelectorAll('#caltb th')[2].textContent.indexOf('الان روی برد') >= 0,
          'the board value sits in its own column next to the new one');
    doc.getElementById('calv0').value = 1500;
    win.eval('caldiff')(0);
    check(doc.getElementById('cald0').textContent.indexOf('+500') >= 0,
          'editing a number updates the difference against the board value');
    doc.getElementById('calv0').value = 99999;
    win.eval('caldiff')(0);
    check(doc.getElementById('cald0').innerHTML.indexOf('خارج از بازهٔ مجاز') >= 0,
          'an impossible hand-typed number is flagged before it is sent');


    /* --- v1.60: a sample knows its scenario and can be excluded by hand --- */
    win.eval('calsmp')();
    check(doc.getElementById('calsl').innerHTML.indexOf('هر دو باتری') >= 0,
          'the sample list names the scenario in plain words, not SOLO');
    check(doc.getElementById('calsl').innerHTML.indexOf('SOLO') < 0,
          'the word SOLO is gone from what the user reads');
    win.CALS.push({ sc: 'BAT1', d: 30, use: 1, r1: 900, r2: 900, vin: 24000, v24: 25000,
                    v12: 12500, vlo: 12500, vhi: 12500, b1: 10, b2: 10,
                    dvi: 24300, dv1: 12600, dv2: 12700, ts: 1 });   /* an obvious outlier */
    win.eval('calrun')();
    const spoiled = +doc.getElementById('calv0').value;
    win.eval('caluse')(win.CALS.length - 1, false);
    win.eval('calrun')();
    check(spoiled !== gain && +doc.getElementById('calv0').value === gain,
          'unticking a bad row takes it straight out of the maths');
    check(win.CALS[win.CALS.length - 1].use === 0,
          'an unticked row is kept in the file, only excluded from the fit');
    /* a battery-2-only row must not touch the channel-1 fit */
    win.CALS[win.CALS.length - 1] = { sc: 'BAT2', d: 30, use: 1, r1: 900, r2: 900,
        vin: 24000, v24: 25000, v12: 12500, vlo: 12500, vhi: 12500, b1: 10, b2: null,
        dvi: 24300, dv1: 12600, dv2: 12700, ts: 1 };
    win.eval('calrun')();
    check(+doc.getElementById('calv0').value === gain,
          'a battery-2-only row is ignored when fitting battery 1');
    win.CALS.pop();
    win.eval('calrun')();

    /* --- v1.61: the conditions are on the page, and a firmware snippet --- */
    win.eval('calchk')();
    check(doc.getElementById('calck').innerHTML.indexOf('✅') >= 0,
          'the rule check-list is rendered with a result for each rule');
    const keepAll = win.CALS;
    win.CALS = [keepAll[0]];
    win.eval('calchk')();
    check(doc.getElementById('calck').innerHTML.indexOf('⛔') >= 0 &&
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
    check(doc.getElementById('calcd').value.indexOf('NOT ENOUGH POINTS') >= 0 ||
          doc.getElementById('calst').textContent.indexOf('حداقل ۲ نقطه') >= 0,
          'a table with fewer than two points is refused, not emitted');
    win.CALS = keepP;
    win.eval('calrun')();
    win.eval('calcode')();
    check(doc.getElementById('calcd').value.indexOf('any count is valid') >= 0,
          'the generated header states that the point count is free');

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
    win.eval('calimp')({ text: async () => JSON.stringify(sf) }).then(() => {});

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
 * [EN] The MCU now guards only each field's own min/max, so every JOINT rule
 *      must hold here or a nonsense pattern would reach the board silently.
 * [FA] میکرو فقط بازهٔ تک‌فیلدی را نگه می‌دارد، پس قوانین مشترک باید اینجا
 *      درست باشند وگرنه الگوی بی‌معنا بی‌صدا روی برد می‌نشیند.
 */
function testFixRules(win, doc) {
    console.log('\nv1.56 cross-field rules in the panel / قوانین مشترک در پنل');

    const base = {};
    [40,41,42,43,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,62,63,64,65,66,67,68,69,
     72,73,74,75,77,78,119,120,121,122].forEach(id => { base[id] = 0; });
    const run = (over) => {
        const v = Object.assign({}, base, over);
        const fx = win.fixrules(v);
        return { v: v, fx: fx };
    };

    let r = run({ 40: 10000, 42: 3, 43: 0 });
    check(r.v[43] === 100, 'more than one beep forces the 100 ms gap floor');
    r = run({ 40: 10000, 42: 1, 43: 0 });
    check(r.v[43] === 0, 'a single beep leaves the gap alone');
    r = run({ 62: 1, 64: 1, 58: 1, 65: 0 });
    check(r.v[65] === 0, 'the shared gap is untouched while every band wants one beep');
    r = run({ 64: 2, 65: 0 });
    check(r.v[65] === 100, 'one band with two beeps raises the shared gap');

    r = run({ 40: 1000, 42: 2, 43: 100, 41: 999999 });
    check(r.v[41] === 450, 'a beep duration is cut to what the window can hold',
          String(r.v[41]));
    r = run({ 40: 0, 41: 999999 });
    check(r.v[41] === 999999, 'a silent period leaves the duration alone');

    r = run({ 50: 40, 51: 90, 52: 95, 53: 99 });
    check(r.v[51] === 40 && r.v[52] === 40 && r.v[53] === 40,
          'the bands are pulled back into order, band 1 authoritative');

    r = run({ 56: 1000, 57: 10, 58: 3, 65: 100 });
    check(r.v[58] === 1, 'the critical count shrinks until its own window fits');
    r = run({ 56: 10000, 57: 100, 58: 3, 65: 100 });
    check(r.v[58] === 3, 'a fitting critical pattern is untouched');

    r = run({ 66: 1000, 67: 5000 });
    check(r.v[67] === 1000, 'the minimum off time cannot exceed the period');
    r = run({ 68: 1000, 69: 5000 });
    check(r.v[69] === 1000, 'the minimum on time cannot exceed the period');

    r = run({ 72: 21000, 73: 20000 });
    check(r.v[73] === 21000, 'the clear level is pulled up to the alarm level');
    r = run({ 74: 21000, 75: 20000 });
    check(r.v[75] === 21100, 'the percent ladder keeps at least 100 mV of span');
    r = run({ 119: 21000, 120: 21000 });
    check(r.v[120] === 21100, 'the charge ladder gets the same 100 mV rule');
    r = run({ 77: 90, 78: 95 });
    check(r.v[78] === 89, 'the full exit is pulled below the full entry');

    r = run({});
    check(r.fx.length === 0 || r.fx.every(f => f[1] !== f[2]),
          'a fix is only reported when the value really moved');
}

/* ==================== Runner / اجراکننده ==================== */

const dom = loadPanel();
const win = dom.window;

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
        testLowBattery(win, doc);
        testImbalance(win, doc);
        testSimulator(win, doc);
        await testSendQueue(win, doc);
        testFixRules(win, doc);
        testBackupAndCal(win, doc);
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
