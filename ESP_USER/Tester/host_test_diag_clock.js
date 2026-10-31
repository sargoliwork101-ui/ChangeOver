/*
 * host_test_diag_clock.js - تست ساعت زندهٔ کارت آمار پنل ساده
 *
 * زمان کل شارژ فقط وقتی باید جلو برود که شارژ فعال و تله‌متری تازه باشد.
 * کارکرد برد جداست و نباید با وضعیت شارژ متوقف شود.
 *
 * نیازمندی: node + jsdom + پیش‌نمایش ساخته‌شده
 */
'use strict';
const fs = require('fs');
const path = require('path');
const { JSDOM, VirtualConsole } = require('jsdom');

const htmlPath = path.join(__dirname, '..', 'simple_preview.html');
const html = fs.readFileSync(htmlPath, 'utf8');
const vc = new VirtualConsole();
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
        /* تایمرهای خود صفحه کنار گذاشته می‌شوند تا زمان ساختگی تنها منبع باشد */
        for (let i = 1; i < 10000; i++) { win.clearInterval(i); win.clearTimeout(i); }

        let now = 100000;
        win.Date.now = function () { return now; };
        function advance(ms) { now += ms; }
        function tick() { win.eval('tickDiagClocks()'); }
        function chargeText() { return win.document.getElementById('stChargeTime').textContent; }
        function uptimeText() { return win.document.getElementById('stUptime').textContent; }
        function diag(charge, uptime) {
            const j = { valid: 1, totalChargeSec: charge, uptimeSec: uptime,
                        chargeCount: 0, resetCount: 0, errors: [], power: [] };
            win.eval('renderDiag(' + JSON.stringify(j) + ')');
        }
        function tlm(charging, on) {
            if (on === undefined) on = true;
            win.eval('lastData={on:' + (!!on) + ',age:0}; g_isCharging=' + (!!charging) + '; g_tlmReceiveAt=Date.now();');
        }

        diag(600, 1000);
        tlm(false);
        tick();
        check(chargeText() === '10 دقیقه 0 ثانیه', 'در بی‌کاری، زمان شارژ از عدد برد شروع می‌شود');
        check(uptimeText() === '16 دقیقه 40 ثانیه', 'کارکرد برد از عدد برد شروع می‌شود');

        advance(1000); tlm(false); tick();
        check(chargeText() === '10 دقیقه 0 ثانیه', 'پس از ۱ ثانیه بی‌کاری، زمان شارژ جلو نمی‌رود');
        check(uptimeText() === '16 دقیقه 41 ثانیه', 'پس از ۱ ثانیه بی‌کاری، کارکرد برد جلو می‌رود');

        advance(1000); tlm(false); tick();
        check(chargeText() === '10 دقیقه 0 ثانیه', 'پس از ۲ ثانیه بی‌کاری، زمان شارژ هنوز ثابت است');

        advance(1000); tlm(true); tick();
        check(chargeText() === '10 دقیقه 1 ثانیه', 'با شارژ فعال و داده تازه، زمان شارژ جلو می‌رود');

        advance(1000); tlm(true); tick();
        check(chargeText() === '10 دقیقه 2 ثانیه', 'ادامه شارژ فعال، زمان شارژ را جلو می‌برد');

        diag(605, 1005);
        tlm(true);
        tick();
        check(chargeText() === '10 دقیقه 5 ثانیه', 'دیاگ تازه عدد ذخیره‌شده برد را جایگزین می‌کند');

        advance(3000); /* تله‌متری تازه نمی‌شود */
        tick();
        check(chargeText() === '10 دقیقه 5 ثانیه', 'با داده کهنه، زمان شارژ جلو نمی‌رود');

        advance(1000); tlm(true); tick();
        check(chargeText() === '10 دقیقه 6 ثانیه', 'پس از تازه‌شدن داده، شمارش از همان‌جا ادامه می‌یابد');

        advance(1000); tlm(true, false); tick();
        check(chargeText() === '10 دقیقه 6 ثانیه', 'با لینک قطع، زمان شارژ جلو نمی‌رود');

        win.eval('renderDiag({valid:0})');
        check(chargeText() === '--', 'بدون دیاگ معتبر، زمان شارژ خط تیره می‌شود');
        check(uptimeText() === '--', 'بدون دیاگ معتبر، کارکرد برد خط تیره می‌شود');
    } catch (e) {
        failures++;
        console.error('FAIL: exception ' + e.message);
    }
    if (failures) { console.error(failures + ' FAILURES'); process.exit(1); }
    console.log('ALL ESP_USER DIAG CLOCK TESTS PASSED');
    process.exit(0);
}, 500);
