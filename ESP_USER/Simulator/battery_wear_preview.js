'use strict';
// Preview-only review indicator. Never loaded or compiled by the ESP panel.
(function () {
 // Install independent demo controls; do not alter real counters or telemetry.
 function install() {
  const charts=document.getElementById('pg1');if(!charts||document.getElementById('wearDemo'))return;
  const style=document.createElement('style');style.textContent=`
   #wearDemo{margin-bottom:16px;line-height:1.9}#wearDemo h2{margin:0;font-size:16px}
   .wear-head{display:flex;align-items:center;justify-content:space-between;gap:10px;flex-wrap:wrap}
   .wear-badge{color:#ffd384;background:#493921;padding:3px 10px;border-radius:20px;font-size:11px}
   .wear-note{font-size:11px;color:var(--mu2);margin:10px 0}.wear-rows{display:grid;gap:22px;margin:22px 0}
   .wear-line-head{display:flex;justify-content:space-between;align-items:center;gap:8px;flex-wrap:wrap;font-size:12px}
   .wear-state{font-size:11px;color:var(--wear-color,var(--mu))}.wear-track{height:8px;border-radius:8px;background:var(--in);position:relative;margin:14px 8px 8px;direction:ltr}
   .wear-fill{height:100%;width:var(--wear-progress,0%);background:var(--wear-color,var(--mu));border-radius:8px}
   .wear-dot{position:absolute;left:var(--wear-progress,0%);top:50%;transform:translate(-50%,-50%);width:12px;height:12px;border-radius:50%;background:var(--wear-color,var(--mu));border:3px solid var(--cd)}
   .wear-tick{position:absolute;left:80%;height:16px;top:-4px;border-left:1px dashed var(--mu)}
   .wear-scale{display:flex;justify-content:space-between;gap:12px;color:var(--mu);font-size:10px}.wear-demo-controls{border-top:1px solid var(--ln);padding-top:14px}
   .wear-presets{display:flex;gap:7px;flex-wrap:wrap;margin:10px 0}.wear-presets button{font:inherit;font-size:11px;background:var(--cd2);border:1px solid var(--ln2);color:var(--tx);border-radius:8px;padding:7px 10px;cursor:pointer}
   .wear-inputs{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}.wear-inputs label{font-size:12px;display:grid;gap:7px}.wear-inputs input[type=range]{width:100%;min-width:0;accent-color:var(--ac)}
   .wear-inputs input[type=number]{width:100%;max-width:150px;box-sizing:border-box;background:var(--in);border:1px solid var(--ln2);color:var(--tx);padding:8px;border-radius:8px;font:inherit}
   .wear-options{display:flex;gap:16px;flex-wrap:wrap;font-size:12px;margin:12px 0}.wear-options label{display:inline-flex;align-items:center;gap:6px;white-space:nowrap}.wear-options input[type=checkbox]{width:16px;height:16px;flex:none;margin:0;accent-color:var(--ac)}
   #wearDemo :focus-visible{outline:2px solid var(--ac);outline-offset:3px}
   @media(max-width:520px){.wear-inputs{grid-template-columns:1fr}.wear-line-head{align-items:flex-start}.wear-state{font-size:10px}}
  `;document.head.appendChild(style);
  const card=document.createElement('section');card.id='wearDemo';card.className='cd';card.setAttribute('aria-label','پیش‌نمایش آزمایشی کارکرد باتری');
  card.innerHTML=`
   <div class="wear-head"><h2>کارکرد باتری و زمان بررسی</h2><strong class="wear-badge">دادهٔ آزمایشی — فقط پیش‌نمایش</strong></div>
   <p class="wear-note">این نوار تعداد شارژ کامل را نسبت به یک حد بررسیِ فرضی نشان می‌دهد؛ درصد سلامت یا تعداد چرخهٔ کامل شارژ‌ودشارژ نیست.</p>
   <div class="wear-rows" aria-live="polite"></div>
   <p class="wear-note">عبور از حد یعنی «نیاز به بررسی باتری»، نه حکم قطعی تعویض. حد واقعی باید از مشخصات همان باتری تعیین شود. سبز بودن هم به معنی سالم بودن قطعی نیست.</p>
   <div class="wear-demo-controls"><b style="font-size:12px">کنترل شبیه‌سازی — هیچ تغییری روی دستگاه ذخیره نمی‌شود</b>
    <div class="wear-presets"><button type="button" data-wear-preset="low">کارکرد کمتر</button><button type="button" data-wear-preset="near">نزدیک حد بررسی</button><button type="button" data-wear-preset="over">عبور از حد بررسی</button><button type="button" data-wear-preset="unknown">بدون اطلاعات</button></div>
    <div class="wear-inputs"><label for="wearUpper">شارژ کامل باتری بالا<input id="wearUpper" type="range" min="0" max="900" step="1" value="270"></label><label for="wearLower">شارژ کامل باتری پایین<input id="wearLower" type="range" min="0" max="900" step="1" value="330"></label><label for="wearLimit">حد بررسی فرضی (نه حد توصیه‌شده)<input id="wearLimit" type="number" min="1" max="100000" value="300"></label></div>
    <div class="wear-options"><label><input id="wearHasData" type="checkbox" checked>شمارنده موجود است</label><label><input id="wearHasLimit" type="checkbox" checked>حد بررسی مشخص است</label></div>
    <p class="wear-note">عدد ۳۰۰ و مرز زردِ ۸۰٪ فقط برای نمایش این طرح انتخاب شده‌اند و مشخصات فنی باتری شما نیستند. سایر بخش‌های این صفحه نیز پیش‌نمایش آفلاین با داده‌های مصنوعی هستند.</p>
   </div>`;
  charts.prepend(card);
  const rows=card.querySelector('.wear-rows');
  for(const [key,label] of [['upper','باتری بالا'],['lower','باتری پایین']]){
   const row=document.createElement('div');row.dataset.wear=key;
   row.innerHTML='<div class="wear-line-head"><strong>'+label+'</strong><span class="wear-state"></span></div><div class="wear-track" role="img"><div class="wear-fill"></div><span class="wear-tick" aria-hidden="true"></span><span class="wear-dot" aria-hidden="true"></span></div><div class="wear-scale"><span class="wear-count"></span><span class="wear-limit"></span></div>';
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
    row.style.setProperty('--wear-progress',(Math.min(1,ratio)*100)+'%');row.style.setProperty('--wear-color',color);
    row.querySelector('.wear-state').textContent=message;
    row.querySelector('.wear-count').textContent=hasData.checked?fa(count)+' شارژ کامل — آزمایشی':'تعداد نامعلوم';
    row.querySelector('.wear-limit').textContent=validLimit?'حد فرضی: '+fa(threshold):'بدون حد بررسی';
    const track=row.querySelector('.wear-track');track.setAttribute('aria-label',message+'؛ '+row.querySelector('.wear-count').textContent+'؛ '+row.querySelector('.wear-limit').textContent);
    row.querySelector('.wear-dot').hidden=!known;row.querySelector('.wear-tick').hidden=!known;
   }
  }
  card.querySelectorAll('input').forEach(el=>el.addEventListener('input',render));
  card.querySelectorAll('[data-wear-preset]').forEach(button=>button.addEventListener('click',()=>{
   const preset=button.dataset.wearPreset;hasData.checked=preset!=='unknown';hasLimit.checked=true;limit.value='300';
   const values={low:[60,90],near:[270,285],over:[330,420],unknown:[0,0]}[preset];upper.value=values[0];lower.value=values[1];render();
  }));
  render();showTab(1);
 }
 if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',install);else install();
})();
