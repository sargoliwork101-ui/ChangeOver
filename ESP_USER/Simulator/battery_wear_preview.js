'use strict';
// Preview-only review indicator. Never loaded or compiled by the ESP panel.
(function () {
 // Install independent demo controls; do not alter real counters or telemetry.
 function install() {
  const charts=document.getElementById('pg1');if(!charts||document.getElementById('wearDemo'))return;
  const style=document.createElement('style');style.textContent=`
   #wearDemo{margin-bottom:16px;line-height:1.9;padding:22px;--wear-spectrum:linear-gradient(90deg,#38d9b0 0%,#62cbb9 52%,#f3c76b 80%,#f28b82 100%)}
   #wearDemo h2{margin:0;font-size:17px;letter-spacing:-.3px}.wear-head{display:flex;align-items:center;justify-content:space-between;gap:10px;flex-wrap:wrap}
   .wear-heading{display:flex;align-items:center;gap:10px}.wear-heading>svg{width:23px;height:23px;color:var(--ac2)}
   .wear-badge{color:#ffd384;background:rgba(243,199,107,.09);border:1px solid rgba(243,199,107,.18);padding:3px 10px;border-radius:20px;font-size:10px;font-weight:500}
   .wear-note{font-size:11px;color:var(--mu2);margin:10px 0;line-height:1.9}.wear-rows{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px;margin:20px 0 14px}
   .wear-card{min-width:0;border:1px solid var(--ln2);border-radius:17px;padding:20px;background:linear-gradient(145deg,rgba(120,155,220,.045),transparent 65%),var(--in)}
   .wear-line-head{display:flex;align-items:center;gap:9px;font-size:12px}.wear-battery-icon{display:grid;place-items:center;width:33px;height:33px;border:1px solid var(--ln2);border-radius:10px;color:var(--mu2);background:var(--cd2)}
   .wear-battery-icon svg{width:20px;height:20px}.wear-serial{margin-right:auto;color:var(--mu);font-size:10px;direction:ltr;font-variant-numeric:tabular-nums}
   .wear-count{display:flex;align-items:baseline;gap:10px;flex-wrap:wrap;margin:12px 0 25px}.wear-number{font-size:39px;font-weight:800;line-height:1.3;letter-spacing:-1px;font-variant-numeric:tabular-nums}.wear-unit{font-size:10px;color:var(--mu2)}
   .wear-track{height:7px;border-radius:10px;background:var(--cd2);position:relative;margin:0 8px 10px;direction:ltr;border:1px solid var(--ln)}
   .wear-fill{height:100%;width:100%;background:var(--wear-spectrum);border-radius:10px;clip-path:inset(0 calc(100% - var(--wear-progress,0%)) 0 0 round 10px)}
   .wear-dot{position:absolute;left:var(--wear-progress,0%);top:50%;transform:translate(-50%,-50%);width:12px;height:12px;border-radius:50%;background:var(--wear-color,var(--mu));border:3px solid var(--cd);box-shadow:0 0 0 4px var(--wear-halo,transparent),0 0 16px var(--wear-halo,transparent);transition:left .25s ease,box-shadow .25s ease}
   .wear-tick{position:absolute;left:80%;height:15px;top:-5px;border-left:1px dashed var(--mu);opacity:.7}
   .wear-scale{display:flex;justify-content:space-between;gap:12px;color:var(--mu);font-size:10px;direction:ltr}.wear-scale span{direction:rtl}
   .wear-bottom{display:flex;align-items:center;justify-content:space-between;gap:10px;flex-wrap:wrap;border-top:1px solid var(--ln);margin-top:18px;padding-top:13px}
   .wear-state{font-size:10px;color:var(--wear-color,var(--mu));display:inline-flex;align-items:center;gap:6px;background:var(--wear-halo,transparent);padding:3px 9px;border-radius:20px}
   .wear-state::before{content:'';width:5px;height:5px;border-radius:50%;background:currentColor;flex:none}.wear-context{font-size:9px;color:var(--mu)}
   .wear-demo-controls{border-top:1px solid var(--ln);margin-top:16px;padding-top:4px}.wear-demo-controls>summary{display:flex;align-items:center;justify-content:space-between;gap:10px;cursor:pointer;list-style:none;font-size:11px;color:var(--mu2);padding:12px 0}
   .wear-demo-controls>summary::-webkit-details-marker{display:none}.wear-demo-controls>summary::after{content:'+';font-size:19px;line-height:1}.wear-demo-controls[open]>summary::after{content:'−'}.wear-controls-body{padding:0 0 4px}
   .wear-presets{display:flex;gap:7px;flex-wrap:wrap;margin:8px 0 18px}.wear-presets button{font:inherit;font-size:11px;background:var(--cd2);border:1px solid var(--ln2);color:var(--tx);border-radius:8px;padding:8px 11px;cursor:pointer;min-height:36px}
   .wear-presets button:hover{border-color:var(--ac)}.wear-presets button[aria-pressed=true]{border-color:var(--ac);background:var(--acbg)}
   .wear-inputs{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}.wear-inputs label{font-size:11px;display:grid;gap:7px}.wear-inputs input[type=range]{width:100%;min-width:0;accent-color:var(--ac)}
   .wear-inputs input[type=number]{width:100%;max-width:150px;box-sizing:border-box;background:var(--in);border:1px solid var(--ln2);color:var(--tx);padding:8px;border-radius:8px;font:inherit}
   .wear-options{display:flex;gap:16px;flex-wrap:wrap;font-size:11px;margin:16px 0}.wear-options label{display:inline-flex;align-items:center;gap:6px;white-space:nowrap}.wear-options input[type=checkbox]{width:16px;height:16px;flex:none;margin:0;accent-color:var(--ac)}
   #wearDemo [hidden]{display:none!important}#wearDemo :focus-visible{outline:2px solid var(--ac);outline-offset:3px;border-radius:5px}
   @media(prefers-reduced-motion:reduce){.wear-dot{transition:none}}
   @media(max-width:640px){#wearDemo{padding:16px}.wear-rows{grid-template-columns:1fr;gap:12px}.wear-card{padding:17px}.wear-inputs{grid-template-columns:1fr}.wear-number{font-size:35px}.wear-heading{gap:7px}#wearDemo h2{font-size:15px}}
  `;document.head.appendChild(style);
  const card=document.createElement('section');card.id='wearDemo';card.className='cd';card.setAttribute('aria-label','پیش‌نمایش آزمایشی کارکرد باتری');
  card.innerHTML=`
   <div class="wear-head"><div class="wear-heading"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" aria-hidden="true"><path d="M3 17l5-6 5 3 8-10M16 4h5v5M3 3v18h18"/></svg><h2>کارکرد باتری و زمان بررسی</h2></div><strong class="wear-badge">دادهٔ آزمایشی — فقط پیش‌نمایش</strong></div>
   <p class="wear-note">تعداد شارژ کامل نسبت به حد بررسیِ فرضی؛ نه درصد سلامت یا تعداد چرخهٔ کامل شارژ‌ودشارژ.</p>
   <div class="wear-rows" aria-live="polite" aria-atomic="true"></div>
   <p class="wear-note">این نشانگر فقط یادآور بررسی است، نه حکم تعویض یا تضمین سلامت. حد واقعی باید از مشخصات همان باتری تعیین شود.</p>
   <details class="wear-demo-controls"><summary>کنترل‌ها و توضیحات شبیه‌سازی</summary><div class="wear-controls-body">
    <div class="wear-presets"><button type="button" data-wear-preset="low" aria-pressed="false">کارکرد کمتر</button><button type="button" data-wear-preset="near" aria-pressed="false">نزدیک حد بررسی</button><button type="button" data-wear-preset="over" aria-pressed="false">عبور از حد بررسی</button><button type="button" data-wear-preset="unknown" aria-pressed="false">بدون اطلاعات</button></div>
    <div class="wear-inputs"><label for="wearUpper">شارژ کامل باتری بالا<input id="wearUpper" type="range" min="0" max="900" step="1" value="270"></label><label for="wearLower">شارژ کامل باتری پایین<input id="wearLower" type="range" min="0" max="900" step="1" value="330"></label><label for="wearLimit">حد بررسی فرضی (نه حد توصیه‌شده)<input id="wearLimit" type="number" min="1" max="100000" value="300"></label></div>
    <div class="wear-options"><label><input id="wearHasData" type="checkbox" checked>شمارنده موجود است</label><label><input id="wearHasLimit" type="checkbox" checked>حد بررسی مشخص است</label></div>
    <p class="wear-note">عدد ۳۰۰ و مرز زردِ ۸۰٪ فقط برای نمایش این طرح انتخاب شده‌اند و مشخصات فنی باتری شما نیستند. هیچ تغییری روی دستگاه یا مرورگر ذخیره نمی‌شود. سایر بخش‌های این صفحه نیز پیش‌نمایش آفلاین با داده‌های مصنوعی هستند.</p>
   </div></details>`;
  charts.prepend(card);
  const rows=card.querySelector('.wear-rows');
  for(const [index,[key,label]] of [['upper','باتری بالا'],['lower','باتری پایین']].entries()){
   const row=document.createElement('article');row.dataset.wear=key;row.className='wear-card';row.setAttribute('aria-label',label);
   row.innerHTML='<div class="wear-line-head"><span class="wear-battery-icon"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5" aria-hidden="true"><rect x="3" y="6" width="17" height="12" rx="3"/><path d="M22 10v4M7 10v4m4-4v4m4-4v4"/></svg></span><strong>'+label+'</strong><span class="wear-serial" aria-hidden="true">۰'+(index===0?'۱':'۲')+'</span></div><div class="wear-count"><strong class="wear-number"></strong> <span class="wear-unit"></span></div><div class="wear-track" role="img"><div class="wear-fill"></div><span class="wear-tick" aria-hidden="true"></span><span class="wear-dot" aria-hidden="true"></span></div><div class="wear-scale"><span>۰</span><span class="wear-limit"></span></div><div class="wear-bottom"><span class="wear-state"></span><span class="wear-context">بر اساس تعداد شارژ کامل</span></div>';
   rows.appendChild(row);
  }
  const upper=card.querySelector('#wearUpper'),lower=card.querySelector('#wearLower'),limit=card.querySelector('#wearLimit'),hasData=card.querySelector('#wearHasData'),hasLimit=card.querySelector('#wearHasLimit');
  const fa=value=>Number(value).toLocaleString('fa-IR');
  // Render only demo inputs; unknown data and invalid limits never produce a health verdict.
  function render() {
   const threshold=Number(limit.value),validLimit=hasLimit.checked&&Number.isInteger(threshold)&&threshold>0&&threshold<=100000;
   for(const [key,input] of [['upper',upper],['lower',lower]]){
    const row=rows.querySelector('[data-wear='+key+']'),count=Number(input.value);
    const known=hasData.checked&&validLimit,ratio=known?count/threshold:0;
    const message=!hasData.checked?'شمارنده موجود نیست':!validLimit?'حد بررسی مشخص نشده':ratio>=1?'نیاز به بررسی باتری':ratio>=.8?'نزدیک حد فرضی بررسی':'زیر حد فرضی بررسی';
    const color=!known?'var(--mu)':ratio>=1?'#f28b82':ratio>=.8?'#f3c76b':'#38d9b0';
    const halo=!known?'transparent':ratio>=1?'rgba(242,139,130,.12)':ratio>=.8?'rgba(243,199,107,.12)':'rgba(56,217,176,.12)';
    row.style.setProperty('--wear-progress',(Math.min(1,ratio)*100)+'%');row.style.setProperty('--wear-color',color);row.style.setProperty('--wear-halo',halo);
    row.querySelector('.wear-state').textContent=message;
    row.querySelector('.wear-number').textContent=hasData.checked?fa(count):'—';
    row.querySelector('.wear-unit').textContent=hasData.checked?'شارژ کامل — آزمایشی':'تعداد نامعلوم';
    row.querySelector('.wear-limit').textContent=validLimit?'حد فرضی: '+fa(threshold):'بدون حد بررسی';
    const track=row.querySelector('.wear-track');track.setAttribute('aria-label',message+'؛ '+row.querySelector('.wear-count').textContent+'؛ '+row.querySelector('.wear-limit').textContent);
    row.querySelector('.wear-dot').hidden=!known;row.querySelector('.wear-tick').hidden=!known;
   }
  }
  const presets=card.querySelectorAll('[data-wear-preset]');
  card.querySelectorAll('input').forEach(el=>el.addEventListener('input',()=>{presets.forEach(button=>button.setAttribute('aria-pressed','false'));render();}));
  presets.forEach(button=>button.addEventListener('click',()=>{
   const preset=button.dataset.wearPreset;hasData.checked=preset!=='unknown';hasLimit.checked=true;limit.value='300';
   const values={low:[60,90],near:[270,285],over:[330,420],unknown:[0,0]}[preset];upper.value=values[0];lower.value=values[1];render();
   presets.forEach(item=>item.setAttribute('aria-pressed',String(item===button)));
  }));
  render();showTab(1);
 }
 if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',install);else install();
})();
