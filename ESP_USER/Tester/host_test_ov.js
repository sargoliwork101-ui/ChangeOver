/*
 * host_test_ov.js - تست رفتاری آلارم اضافه‌ولتاژ پنل ساده در DOM واقعی
 *
 * آستانهٔ OV باید از سقف ورودی برد (q34) مشتق شود، نه از یک عدد ثابت ۲۸ ولت.
 * هیسترزیس (q71) جداگانه تنظیم می‌شود. این تست صفحهٔ پیش‌نمایش ساخته‌شده را
 * در jsdom بارگذاری می‌کند و تابع updInputState را با پارامترهای واقعی برد
 * صدا می‌زند.
 *
 * نیازمندی: node + jsdom (اختیاری؛ نبودن ابزار یعنی SKIP در run_simple_tests.sh)
 */
'use strict';
const fs = require('fs');
const path = require('path');
const { JSDOM, VirtualConsole } = require('jsdom');

const htmlPath = path.join(__dirname, '..', 'simple_preview.html');
const html = fs.readFileSync(htmlPath, 'utf8');
const vc = new VirtualConsole(); // canvas/CSS warnings را خفه کن
const dom = new JSDOM(html, {
    runScripts: 'dangerously',
    pretendToBeVisual: true,
    url: 'http://localhost/',
    virtualConsole: vc
});
const win = dom.window;

let failures = 0;
function check(cond, msg) {
    if (!cond) { failures++; console.error('FAIL: ' + msg); }
    else { console.log('PASS: ' + msg); }
}

setTimeout(() => {
    try {
        // 19 پارامتر: q34=27000 (ایندکس 15), q71=1500 (ایندکس 18)
        win.eval('updBoardParams([1000,10,1000,150,21000,29000,100,95,5,2,2,3,21000,29000,21000,27000,500,500,1500])');
        const b = win.eval('brd');
        check(b.imax === 27000, 'brd.imax از q34 خوانده شد (27000)');
        check(b.ovHyst === 1500, 'brd.ovHyst از q71 خوانده شد (1500)');

        // آستانه = 27000، پاک‌شدن = 25500
        win.eval('g_inputOV=false');
        let r = win.eval('updInputState(27500,true)');
        check(r.ov === true, '27500 > 27000 → آلارم روشن');
        r = win.eval('updInputState(26000,true)');
        check(r.ov === true, '26000 > 25500 → آلارم همچنان روشن (هیسترزیس)');
        r = win.eval('updInputState(25500,true)');
        check(r.ov === false, '25500 <= 25500 → آلارم پاک شد');

        // تغییر سقف به 29000 و هیسترزیس به 1000
        win.eval('updBoardParams([1000,10,1000,150,21000,29000,100,95,5,2,2,3,21000,29000,21000,29000,500,500,1000])');
        win.eval('g_inputOV=false');
        r = win.eval('updInputState(28500,true)');
        check(r.ov === false, '28500 < 29000 → آلارم خاموش (آستانهٔ جدید)');
        r = win.eval('updInputState(29500,true)');
        check(r.ov === true, '29500 > 29000 → آلارم روشن (آستانهٔ جدید)');
        r = win.eval('updInputState(28000,true)');
        check(r.ov === false, '28000 <= 28000 → آلارم پاک شد (29000−1000)');

        // متن هشدار باید سقف زندهٔ برد را نشان دهد، نه «حد ۲۸ ولت» ثابت
        const src = fs.readFileSync(path.join(__dirname, '..', 'simple_panel.h'), 'utf8');
        check(src.indexOf("حد '+(brd.imax/1000).toFixed(1)+' ولت") !== -1,
              'متن هشدار از brd.imax زنده ساخته می‌شود');
        check(src.indexOf('حد ۲۸ ولت') === -1 && src.indexOf('حد 28 ولت') === -1,
              'متن ثابت «حد ۲۸ ولت» حذف شده است');
    } catch (e) {
        failures++;
        console.error('FAIL: exception ' + e.message);
    }
    if (failures) { console.error(failures + ' FAILURES'); process.exit(1); }
    console.log('ALL ESP_USER OV TESTS PASSED');
    process.exit(0);
}, 500);
