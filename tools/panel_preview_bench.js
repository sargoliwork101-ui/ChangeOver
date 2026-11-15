/* [EN] Preview-only fixtures and controls. Never embedded in the ESP panel. */
(function(){
 'use strict';
 const el=document.createElement('div');el.className='cd';el.id='previewBench';
 el.innerHTML='<div class="hd"><b>آزمایش کالیبراسیون — فقط شبیه‌ساز</b></div>'+
 '<div class="ds">این عددها ساختگی‌اند، نه اندازه‌گیری برد. با دکمه‌های زیر سناریو را عوض کنید؛ «محاسبه» جدول هر باتری و دکمهٔ حذف نقطه را نشان می‌دهد. حذف از یک باتری به دیگری دست نمی‌زند. تغییر تیک نمونه‌ها نتیجه را خودکار به‌روز می‌کند. برای نگه‌داشتن حذف‌ها، «ذخیرهٔ نمونه‌ها» را بزنید؛ نام فایل تأیید می‌خواهد و بازخوانی آن همان جدول را می‌سازد.</div>'+
 '<div class="bqr2"><button class="sb pbg" id="demoClean">نمونهٔ سالم + یک تکراری</button><button class="sb pby" id="demoBad">یک اندازه‌گیری مشکوک</button><button class="sb pbb" id="demoDense">۲۸ نقطه؛ بیشتر از ظرفیت برد</button><button class="sb" id="demoCapture">آزمایش ثبت و تکرار مرحله‌ها</button><button class="sb" id="demoMeter">پرکردن عدد مولتی‌متر نمایشی</button></div><div id="demoExplain" class="ds"></div>';
 document.getElementById('p1').prepend(el);
 const explain=text=>document.getElementById('demoExplain').textContent=text;
 async function seed(kind,automatic){
  if(W.run||CAL_BUSY)return explain('اول داده‌برداری یا ارسال در حال اجرا را تمام کنید.');
  if(!D||!D.p)return explain('منتظر دریافت دادهٔ شبیه‌ساز بمانید.');
  if(!automatic&&CALS.length&&!await pmConfirm('نمونه‌های فعلی این مرورگر با داده‌های ساختگی جایگزین شوند؟ اگر لازم‌اند ابتدا ذخیرهٔ نمونه‌ها را بزنید.'))return;
  const count=kind==='dense'?28:7;
  CALS=Array.from({length:count},(_,i)=>({sc:'BOTH',d:i===0?0:i*50/(count-1),use:1,r1:80+i*20,r2:90+i*20,b1:i===0?-20:i*26-20,b2:i===0?-15:i*27-15,ts:Date.now()+i}));
  if(kind!=='dense')CALS.push({...CALS[3],b1:CALS[3].b1+1,b2:CALS[3].b2+1});
  if(kind==='bad')CALS[4].b1=700;
  ['BAT1','BAT2','BOTH'].forEach(sc=>document.getElementById('wc'+sc).checked=true);
  calinvalidate();calsave();caln();calsmp();calrun();
  explain(kind==='bad'?'نقطهٔ شمارش ۱۶۰ باتری ۱ عمداً ۷۰۰ میلی‌آمپر وارد شده؛ نقطهٔ بعد فقط ۱۱۰ است. این افت هشدار می‌دهد؛ هیچ ردیفی خودکار حذف نمی‌شود. همین نقطه را حذف کنید تا جدول دوباره بررسی شود.':kind==='dense'?'۲۸ نقطه داریم؛ سقف ارسال هر باتری ۲۴ است. این چهار نقطه لزوماً غلط نیستند؛ تا رسیدن به ۲۴ یا کمتر، چند نقطهٔ میانی را دستی حذف کنید و نقطهٔ صفر و انتهای بازه را نگه دارید.':'۸ نمونه به ۷ نقطه تبدیل می‌شوند: دو نمونه شمارش خام یکسان دارند و در یک نقطه با جریان بزرگ‌تر ادغام می‌شوند. حذف از جدول، نمونهٔ اصلی را پاک نمی‌کند؛ «بازگردانی» آن را برمی‌گرداند.');
 }
 document.getElementById('demoClean').onclick=()=>seed('clean');
 document.getElementById('demoBad').onclick=()=>seed('bad');
 document.getElementById('demoDense').onclick=()=>seed('dense');
 document.getElementById('demoCapture').onclick=async()=>{
  if(W.run||CAL_BUSY)return;
  if(!await pmConfirm('ثبت آزمایشی با ۳ مرحله برای هر دو باتری آغاز شود؟ نمونه‌ها و فایل قبلی خودکار پاک نمی‌شوند.'))return;
  document.getElementById('wSw').checked=false;document.getElementById('wL').value='0,10,20';wmode();
  ['BAT1','BAT2','BOTH'].forEach(sc=>document.getElementById('wc'+sc).checked=sc==='BOTH');
  explain('در هر مرحله «پرکردن عدد مولتی‌متر نمایشی» و سپس «ثبت» را بزنید. با ↻ به مرحلهٔ قبل برگردید. حتی پس از تأیید و پایان هم ↻ ردیف‌ها فعال است؛ با تأیید جداگانه دوباره اندازه بگیرید.');
  wStart();
 };
 document.getElementById('demoMeter').onclick=()=>{
  if(!W.run||!document.getElementById('wGo')||W.saving)return explain('این دکمه فقط هنگام بازبودن فرم اندازه‌گیری کار می‌کند.');
  W.act.forEach(n=>{const e=document.getElementById('wB'+n),current=Math.round((D.t[(n-1)*7]-80)*1.3-20);if(e)e.value=current;});
 };
 // [EN] Never overwrite a saved sample set automatically or write calibration to the sim board.
 let tries=0;const ready=setInterval(()=>{
  if(++tries>100){clearInterval(ready);return;}
  if(!D||!D.p)return;clearInterval(ready);
  if(!CALS.length)seed('clean',true);else{calrun();explain('نمونه‌های ذخیره‌شدهٔ شما حفظ شدند. برای دیدن مثال‌ها یکی از دکمه‌های بالا را بزنید.');}
 },100);
})();
