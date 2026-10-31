/*
 * host_test_csv.js - آزمون خروجی تاریخچهٔ شارژ پنل کاربر
 * ستون مدت حذف شده است؛ زمان، کانال و شمار ردیف‌ها باید حفظ شوند.
 * نیازمندی: node + jsdom + پیش‌نمایش ساخته‌شده
 */
'use strict';
const fs = require('fs');
const path = require('path');
const { JSDOM, VirtualConsole } = require('jsdom');
const html = fs.readFileSync(path.join(__dirname, '..', 'simple_preview.html'), 'utf8');
const virtualConsole = new VirtualConsole();
const dom = new JSDOM(html, {
    runScripts: 'dangerously',
    pretendToBeVisual: true,
    url: 'http://localhost/',
    virtualConsole
});
const win = dom.window;
let failures = 0;
function check(condition, message) {
    if (condition) { console.log('PASS: ' + message); }
    else { failures++; console.error('FAIL: ' + message); }
}
setTimeout(() => {
    try {
        for (let i = 1; i < 10000; i++) { win.clearInterval(i); win.clearTimeout(i); }
        /* ردیف قدیمی هم ممکن است مدت داشته باشد؛ خروجی تازه نباید آن را بیاورد. */
        win._lastChargeHistory = [
            { ts: 1700000300, ch: 2, dur: 0 },
            { ts: 1700000100, ch: 1, dur: 7200 },
            { ts: 1700000200, ch: 2 }
        ];
        const data = win.eval('chargeCsvData()');
        check(data.count === 3, 'هر سه رویداد تاریخچه در خروجی می‌مانند');
        check(data.rows[0].length === 4, 'خروجی چهار ستون دارد: زمان، تاریخ، ساعت، کانال');
        check(data.rows[0][3] === 'کانال', 'ستون داده، فقط کانال است');
        check(data.rows[0].indexOf('مدت (ثانیه)') === -1, 'ستون مدت حذف شده است');
        check(data.rows.every(row => row.length === 4), 'هیچ ردیفی مقدار مدت ندارد');
        check(data.rows[1][0] === 1700000100 && data.rows[1][3] === 1,
              'قدیمی‌ترین رویداد و کانال آن درست است');
        check(data.rows[3][0] === 1700000300 && data.rows[3][3] === 2,
              'جدیدترین رویداد و کانال آن درست است');
        const text = win.eval('csvText(chargeCsvData().rows)');
        check(text.indexOf('7200') === -1, 'مدت ذخیره‌شدهٔ قدیمی وارد فایل تازه نمی‌شود');
        win._lastChargeHistory = [];
        check(win.eval('chargeCsvData().count') === 0, 'تاریخچهٔ خالی، خروجی خالی می‌دهد');
        const pct = win.eval("chartDataPct={pct:[57],time:[1700000100],range:'1d'}; pctCsvData()");
        check(pct.rows[0][3] === 'درصد باتری' && pct.rows[1][3] === 57,
              'خروجی درصد دست‌نخورده است');
        const volt = win.eval("chartDataVolt={vin:[24000],vpack:[25500],time:[1700000100],range:'1d'}; voltCsvData()");
        check(volt.rows[0].length === 5 && volt.rows[1][3] === 24000 && volt.rows[1][4] === 25500,
              'خروجی ولتاژ دست‌نخورده است');
    } catch (error) {
        failures++;
        console.error('FAIL: exception ' + error.message);
    }
    if (failures) { console.error(failures + ' FAILURES'); process.exit(1); }
    console.log('ALL ESP_USER CSV TESTS PASSED');
    process.exit(0);
}, 500);
