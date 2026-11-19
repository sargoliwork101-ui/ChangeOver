/**
 * @file charge_calendar.h
 * @brief Shared charge-calendar browser asset compiled into ESP program memory.
 */
#pragma once
static const char SIMPLE_CHARGE_CALENDAR_JS[] PROGMEM = R"calendar(
/* Charge calendar shared by ESP firmware and previews. */
(function () {
 'use strict';
 // Install after the page has declared its renderers.
 function installCalendar() {
  if (document.getElementById('chargeCalendar')) return;
  const canvas = document.getElementById('cCharge');
  if (!canvas || typeof window.drawChargeHistory !== 'function') return;
  const card = canvas.closest('.chart-card');
  card.classList.add('cal-preview-card');
  const style = document.createElement('style');
  style.textContent = `
   .cal-preview-card [hidden]{display:none!important}
   #chargeCalendar{margin-top:18px;color:var(--tx);direction:rtl}
   .cal-toolbar{display:flex;align-items:center;justify-content:space-between;gap:10px;flex-wrap:wrap;margin-bottom:18px}
   .cal-navigation{display:flex;align-items:center;gap:9px;flex-wrap:wrap}
   .cal-navigation button{font:inherit;color:var(--tx);background:var(--cd2);border:1px solid var(--ln2);border-radius:9px;padding:9px 12px;cursor:pointer}
   .cal-navigation button:disabled{opacity:.35;cursor:default}
   #calMonth{font-size:16px;min-width:110px;text-align:center}
   .cal-legend{display:flex;gap:16px;font-size:12px;flex-wrap:wrap;color:var(--mu2)}
   .cal-blue{color:#78a9ff}.cal-green{color:#38d9b0}
   .cal-week,.cal-grid{display:grid;grid-template-columns:repeat(7,minmax(0,1fr));gap:7px}
   .cal-week{margin:16px 0 8px;text-align:center;color:var(--mu);font-size:11px}
   .cal-day{min-width:0;min-height:92px;padding:10px 7px;border:1px solid var(--ln);border-radius:12px;background:var(--cd2);color:var(--tx);font-family:inherit;text-align:right;cursor:pointer;display:flex;flex-direction:column;gap:10px}
   .cal-day:disabled{opacity:.3;cursor:default}
   .cal-day:hover{border-color:var(--ac)}
   .cal-day[aria-pressed="true"]{border-color:var(--ac);box-shadow:inset 0 0 0 1px var(--ac);background:var(--acbg)}
   .cal-day.cal-today .cal-number{color:var(--ac2);text-decoration:underline;text-underline-offset:5px}
   .cal-number{font-size:15px;font-weight:800;line-height:1.5}
   .cal-counts{display:flex;gap:5px;flex-wrap:wrap;margin-top:auto;min-height:21px;font-size:12px}
   .cal-counts span{border-radius:5px;padding:2px 5px;background:rgba(100,145,220,.09)}
   .cal-empty-mark{color:var(--mu);font-size:12px;margin-top:auto;line-height:1.8}
   #calSummary{margin:12px 0;color:var(--mu);font-size:11px;line-height:1.9}
   .cal-details{margin-top:18px;border:1px solid var(--ln2);border-radius:14px;padding:17px;background:var(--cd2)}
   #calSelected{font-size:14px;margin:0 0 12px}
   #calEvents{display:grid;gap:9px;max-height:280px;overflow-y:auto;scrollbar-width:thin}
   .cal-event{display:flex;justify-content:space-between;gap:12px;padding-top:9px;border-top:1px solid var(--ln);font-size:13px}
   .cal-event time{color:var(--tx2);font-variant-numeric:tabular-nums}
   .cal-event-meta{display:flex;flex-direction:column;gap:4px;text-align:left;max-width:62%}
   .cal-duration{font-size:12px;line-height:1.8;color:var(--tx2)}
   .cal-no-record{font-size:12px;line-height:2;color:var(--mu)}
   .cal-preview-note{font-size:11px;color:var(--mu);line-height:2;margin:9px 0}
   @media(max-width:520px){.cal-grid,.cal-week{gap:4px}.cal-day{padding:6px 3px;min-height:84px;border-radius:8px}.cal-counts{gap:2px;flex-direction:column;align-items:flex-start;font-size:10px}.cal-counts span{padding:1px 3px}.cal-empty-mark{font-size:10px}.cal-navigation{gap:5px}.cal-navigation button{font-size:11px;padding:9px}#calMonth{font-size:14px;min-width:90px}.cal-details{padding:12px}}
  `;
  document.head.appendChild(style);
  card.querySelector('.ti').textContent = 'تقویم شارژ';
  canvas.closest('.canvas-wrap').hidden = true;
  document.getElementById('cChargeLabels').hidden = true;
  document.getElementById('chargeFull').hidden = true;
  card.querySelector('.action-row').hidden = true;
  document.getElementById('chargeCap').parentElement.hidden = true;
  document.getElementById('chargeHistoryList').hidden = true;
  const intro = card.querySelector('.ti').nextElementSibling;
  intro.textContent = 'یک روز را انتخاب کن تا وضعیت، ساعت شروع، پایان و مدت شارژ هر باتری را ببینی.';
  const root = document.createElement('section');
  root.id = 'chargeCalendar';
  root.setAttribute('aria-label', 'تقویم تاریخچهٔ شارژ');
  root.innerHTML = `
   <p class="cal-preview-note">۳۰ روز اخیر؛ بر اساس سوابق ثبت‌شدهٔ پنل.</p>
   <div class="cal-toolbar"><div class="cal-navigation"><button type="button" id="calPrev" aria-label="ماه قبل">ماه قبل</button><strong id="calMonth" aria-live="polite"></strong><button type="button" id="calNext" aria-label="ماه بعد">ماه بعد</button><button type="button" id="calToday">این ماه</button></div></div>
   <div class="cal-legend"><span class="cal-blue">● باتری بالا</span><span class="cal-green">● باتری پایین</span><span>عدد رنگی = همهٔ نوبت‌های شارژ ثبت‌شده</span></div>
   <div class="cal-week" aria-hidden="true"><span>شنبه</span><span>یکشنبه</span><span>دوشنبه</span><span>سه‌شنبه</span><span>چهارشنبه</span><span>پنجشنبه</span><span>جمعه</span></div>
   <div id="calGrid" class="cal-grid" role="group" aria-label="روزهای ماه"></div>
   <p id="calSummary"></p>
   <div class="cal-details"><h3 id="calSelected"></h3><div id="calEvents" aria-live="polite"></div></div>
   <p class="cal-preview-note">فقط سوابق موجود نمایش داده می‌شوند؛ شروع و مدت شارژ فقط در صورت وجود داده نمایش داده می‌شوند؛ این اطلاعات میتواند معیار سلامت باتری باشد.</p>
  `;
  card.appendChild(root);
  const partsFormatter = new Intl.DateTimeFormat('en-US-u-ca-persian', {year:'numeric',month:'numeric',day:'numeric'});
  const titleFormatter = new Intl.DateTimeFormat('fa-IR-u-ca-persian', {weekday:'long',year:'numeric',month:'long',day:'numeric'});
  let records = [], selected = '', month = monthStart(new Date()), signature = '';
  // Read Jalali dates from the browser calendar, in the same local timezone.
  function jalali(date) {
   const parts = Object.fromEntries(partsFormatter.formatToParts(date).map(part => [part.type, part.value]));
   return [Number(parts.year),Number(parts.month),Number(parts.day)];
  }
  // Use local noon and calendar arithmetic so DST cannot shift a day.
  function dayDate(date, offset) {
   return new Date(date.getFullYear(), date.getMonth(), date.getDate() + offset, 12);
  }
  // Locate the first local date of the displayed Persian month.
  function monthStart(date) {
   return dayDate(date, 1 - jalali(date)[2]);
  }
  // Generate an unambiguous local key without UTC date conversions.
  function dayKey(date) {
   return date.getFullYear() + '-' + p2(date.getMonth() + 1) + '-' + p2(date.getDate());
  }
  // Collect the complete Jalali month, including leap-year Esfand.
  function monthDays() {
   const identity = jalali(month), days = [];
   for (let offset = 0; offset < 32; offset++) {
    const date = dayDate(month, offset), j = jalali(date);
    if (j[0] !== identity[0] || j[1] !== identity[1]) break;
    days.push(date);
   }
   return days;
  }
  // Format only supplied positive durations; zero in the ESP history means unknown.
  function durationText(seconds) {
   if (seconds === null) return 'مدت شارژ ثبت نشده';
   const hours = Math.floor(seconds / 3600);
   const remaining = seconds % 3600;
   const minutes = Math.floor(remaining / 60);
   const tail = remaining % 60;
   const parts = [];
   if (hours) parts.push(faDigits(hours) + ' ساعت');
   if (minutes) parts.push(faDigits(minutes) + ' دقیقه');
   if (tail) parts.push(faDigits(tail) + ' ثانیه');
   return 'مدت شارژ: ' + parts.join(' و ');
  }
  // Show only a supplied start timestamp; elapsed duration does not prove a start time.
  function startTime(event) {
   const time = document.createElement('time');time.className = 'cal-start';
   if (event.start === null) {
    time.textContent = 'شروع شارژ ثبت نشده';
    return time;
   }
   const date = new Date(event.start * 1000);
   time.dateTime = date.toISOString();
   const clock = faDigits(p2(date.getHours()) + ':' + p2(date.getMinutes()));
   const sameDay = dayKey(date) === dayKey(new Date(event.ts * 1000));
   time.textContent = 'شروع: ' + (sameDay ? '' : titleFormatter.format(date) + ' · ') + 'ساعت ' + clock;
   return time;
  }
  // Render recorded counts only; absence never implies no charging occurred.
  function renderCalendar() {
   const now=new Date(),earliest=dayDate(now,-29),latest=dayDate(now,0);
   if(month<monthStart(earliest))month=monthStart(earliest);
   if(month>monthStart(latest))month=monthStart(latest);
   const days = monthDays(), today = dayKey(now), groups = new Map();
   const allowed=date=>date>=earliest&&date<=latest;
   const visible=records.filter(event=>{const date=new Date(event.ts*1000);return allowed(dayDate(date,0));});
   for (const event of visible) {
    const key = dayKey(new Date(event.ts * 1000));
    if (!groups.has(key)) groups.set(key, []);
    groups.get(key).push(event);
   }
   if (!days.some(date => allowed(date) && dayKey(date) === selected)) {
    const withRecords = days.filter(date => groups.has(dayKey(date)));
    selected = dayKey(withRecords.at(-1) || days.find(date => dayKey(date) === today) || days.find(allowed));
   }
   const nextSignature = JSON.stringify([records, month.getTime(), selected, today]);
   if (nextSignature === signature) return;
   signature = nextSignature;
   const identity = jalali(month);
   document.getElementById('calMonth').textContent = J_MONTHS[identity[1] - 1] + ' ' + faDigits(identity[0]);
   document.getElementById('calNext').disabled = month >= monthStart(latest);
   document.getElementById('calPrev').disabled = month <= monthStart(earliest);
   const grid = document.getElementById('calGrid'), fragment = document.createDocumentFragment();
   const blanks = (month.getDay() + 1) % 7;
   for (let i = 0; i < blanks; i++) {
    const blank = document.createElement('span');blank.setAttribute('aria-hidden', 'true');fragment.appendChild(blank);
   }
   let monthCount = 0;
   for (const date of days) {
    const key = dayKey(date), events = groups.get(key) || [], counts = [0, 0];
    events.forEach(event => counts[event.ch - 1]++);
    monthCount += events.length;
    const button = document.createElement('button');
    button.disabled=!allowed(date);
    button.type = 'button';button.className = 'cal-day' + (key === today ? ' cal-today' : '');button.dataset.day = key;
    button.setAttribute('aria-pressed', String(key === selected));
    const label = titleFormatter.format(date);
    const summary = !allowed(date) ? 'خارج از بازهٔ ۳۰ روز اخیر' : events.length ? 'باتری بالا ' + faDigits(counts[0]) + '، باتری پایین ' + faDigits(counts[1]) : 'بدون سابقهٔ ثبت‌شده';
    button.setAttribute('aria-label', label + '؛ ' + summary);
    button.innerHTML = '<span class="cal-number">' + faDigits(jalali(date)[2]) + '</span>' +
     (events.length ? '<span class="cal-counts">' + (counts[0] ? '<span class="cal-blue">● ' + faDigits(counts[0]) + '</span>' : '') + (counts[1] ? '<span class="cal-green">● ' + faDigits(counts[1]) + '</span>' : '') + '</span>' : '<span class="cal-empty-mark">بدون سابقه</span>');
    button.addEventListener('click', function () {
     if(!allowed(date))return;
     selected = key;renderCalendar();
     grid.querySelector('[data-day="' + key + '"]').focus({preventScroll:true});
    });
    fragment.appendChild(button);
   }
   grid.replaceChildren(fragment);
   document.getElementById('calSummary').textContent = faDigits(monthCount) + ' نوبت شارژ ثبت‌شده در این ماه؛ از ' + faDigits(visible.length) + ' سابقهٔ موجود در ۳۰ روز اخیر.';
   const chosenDate = days.find(date => dayKey(date) === selected);
   document.getElementById('calSelected').textContent = titleFormatter.format(chosenDate);
   const selectedEvents = (groups.get(selected) || []).slice().sort((a, b) => a.ts - b.ts);
   const details = document.getElementById('calEvents');details.replaceChildren();
   if (!selectedEvents.length) {
    const note = document.createElement('p');note.className = 'cal-no-record';
    note.textContent = 'برای این روز سابقه‌ای ثبت نشده؛ نمی‌توان گفت شارژی انجام نشده است.';
    details.appendChild(note);
   }
   selectedEvents.forEach(event => {
    const row = document.createElement('div');row.className = 'cal-event';
    const battery = document.createElement('strong');battery.className = event.ch === 1 ? 'cal-blue' : 'cal-green';
    battery.textContent = (event.ch === 1 ? 'باتری بالا' : 'باتری پایین') + ' · ' + (event.status === 'partial' ? 'شارژ نیمه‌تمام' : event.status === 'charging' ? 'در حال شارژ' : event.status === 'complete' ? 'شارژ کامل' : 'پایان نامشخص');
    row.dataset.status=event.status;
    const time = document.createElement('time');time.dateTime = new Date(event.ts * 1000).toISOString();
    const date = new Date(event.ts * 1000);time.textContent = (event.status === 'charging' ? 'آخرین ثبت: ساعت ' : event.status === 'partial' ? 'پایان: ساعت ' : event.status === 'complete' ? 'تکمیل: ساعت ' : 'آخرین ثبت: ساعت ') + faDigits(p2(date.getHours()) + ':' + p2(date.getMinutes()));
    const duration = document.createElement('span');duration.className = 'cal-duration';duration.textContent = durationText(event.dur);
    if(event.status === 'charging' && event.dur !== null)duration.textContent=duration.textContent.replace('مدت شارژ:', 'مدت تا آخرین ثبت:');
    const meta = document.createElement('div');meta.className = 'cal-event-meta';meta.append(startTime(event), time, duration);
    row.append(battery, meta);details.appendChild(row);
   });
  }
  // Change the month without touching polling, history storage or the voltage chart.
  function moveMonth(direction) {
   const days = monthDays();
   month = monthStart(dayDate(direction < 0 ? days[0] : days.at(-1), direction));
   selected = '';renderCalendar();
  }
  document.getElementById('calPrev').addEventListener('click', () => moveMonth(-1));
  document.getElementById('calNext').addEventListener('click', () => moveMonth(1));
  document.getElementById('calToday').addEventListener('click', () => {month = monthStart(new Date());selected = '';renderCalendar();});
  // Keep the existing diagnostic and erase pathways as the single data source.
  window.drawChargeHistory = function (events) {
   window._lastChargeHistory = events;
   records = (Array.isArray(events) ? events : []).filter(event => event && Number.isFinite(event.ts) && event.ts >= 1000000000 && (event.ch === 1 || event.ch === 2)).map(event => ({ts:event.ts,ch:event.ch,status:event.status === undefined ? 'complete' : ['complete','partial','charging'].includes(event.status) ? event.status : 'unknown',start:Number.isInteger(event.start) && event.start >= 1000000000 && event.start <= event.ts ? event.start : null,dur:Number.isInteger(event.dur) && event.dur > 0 && event.dur <= 4294967295 ? event.dur : null}));
   renderCalendar();
  };
  window.drawChargeHistory(window._lastChargeHistory || []);
 }
 if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', installCalendar);
 else installCalendar();
})();
)calendar";
