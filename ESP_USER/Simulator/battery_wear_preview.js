'use strict';
// Simulator-only pack indicator and settings proposal; never compiled into ESP firmware.
(function () {
 const STORAGE_KEY='changeover_preview_pack_review_v1';
 // Build a local line icon using the panel sprite, without network assets.
 function icon(name) {return '<svg class="glyph" aria-hidden="true"><use href="#i-'+name+'"/></svg>';}
 // Find the existing action section without rebuilding its controls or handlers.
 function sectionFor(admin,selector) {const el=admin.querySelector(selector);return el?el.closest('details'):null;}
 // Install only on the complete offline preview; leave shipped files and device routes untouched.
 function install() {
  const charts=document.getElementById('pg1'),admin=document.getElementById('adminCard');
  if(!charts||!admin||document.getElementById('wearDemo'))return;
  const stats=document.getElementById('stUptime').closest('.cd');
  const style=document.createElement('style');style.id='packPreviewStyle';style.textContent=`
   #wearDemo{padding:22px;--wear-spectrum:linear-gradient(90deg,#38d9b0 0%,#62cbb9 52%,#f3c76b 80%,#f28b82 100%)}
   .pack-top{display:flex;align-items:center;gap:10px}.pack-top>.glyph{color:var(--ac2);width:22px;height:22px}.pack-top h2{font-size:15px;margin:0}.pack-top .pack-tag{margin-right:auto;font-size:10px;color:var(--mu2);border:1px solid var(--ln2);border-radius:20px;padding:2px 9px}
   .pack-metric{display:flex;align-items:baseline;gap:10px;flex-wrap:wrap;margin:15px 0 25px}.wear-number{font-size:39px;font-weight:800;line-height:1.3;font-variant-numeric:tabular-nums}.wear-unit{font-size:11px;color:var(--mu2)}
   .wear-track{height:7px;border-radius:10px;background:var(--in);position:relative;margin:0 8px 10px;direction:ltr;border:1px solid var(--ln)}
   .wear-fill{height:100%;width:100%;background:var(--wear-spectrum);border-radius:10px;clip-path:inset(0 calc(100% - var(--wear-progress,0%)) 0 0 round 10px)}
   .wear-dot{position:absolute;left:var(--wear-progress,0%);top:50%;transform:translate(-50%,-50%);width:12px;height:12px;border-radius:50%;background:var(--wear-color,var(--mu));border:3px solid var(--cd);box-shadow:0 0 0 4px var(--wear-halo,transparent),0 0 16px var(--wear-halo,transparent);transition:left .25s ease}
   .wear-tick{position:absolute;left:80%;height:15px;top:-5px;border-left:1px dashed var(--mu);opacity:.7}
   .wear-scale{display:flex;justify-content:space-between;gap:10px;color:var(--mu);font-size:10px;direction:ltr}.wear-scale span{direction:rtl}
   .pack-bottom{display:flex;align-items:center;justify-content:space-between;gap:10px;flex-wrap:wrap;border-top:1px solid var(--ln);margin-top:18px;padding-top:13px}.pack-context{font-size:10px;color:var(--mu2)}
   .wear-state{font-size:11px;color:var(--wear-color,var(--mu));display:inline-flex;align-items:center;gap:6px;background:var(--wear-halo,transparent);padding:4px 10px;border-radius:20px}
   .wear-state::before{content:'';width:5px;height:5px;border-radius:50%;background:currentColor;flex:none}
   .preview-banner{display:flex;align-items:center;justify-content:space-between;gap:8px;flex-wrap:wrap;font-size:11px;color:var(--mu2);padding:10px 14px;border:1px dashed var(--ln2);border-radius:11px;background:var(--in)}
   .preview-banner strong{font-size:11px;color:var(--ac2)}.preview-banner button{font:inherit;color:var(--tx);border:1px solid var(--ln2);border-radius:7px;padding:6px 10px;background:var(--cd2);cursor:pointer;min-height:36px}
   #adminCard.preview-admin{padding:20px}.preview-admin .admin-head{padding-bottom:16px;border-bottom:1px solid var(--ln);margin-bottom:20px}.preview-admin .admin-head h2{display:flex;align-items:center;gap:8px;font-size:15px}
   .preview-admin-group{margin-top:20px;min-width:0}.preview-admin-group>h3{display:flex;align-items:center;gap:8px;font-size:12px;color:var(--mu2);font-weight:600;margin:0 0 10px}
   .preview-admin-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px;align-items:start;min-width:0}.preview-admin .admin-section{margin:0;border-radius:13px;background:var(--in);min-width:0;overflow:hidden}
   .preview-admin .admin-section>summary{padding:14px;gap:8px;font-size:12px;min-height:50px;box-sizing:border-box}.preview-admin .admin-section>summary>.glyph{width:18px;height:18px;color:var(--ac2)}
   .preview-admin .admin-section.danger-zone>summary>.glyph{color:var(--er)}.preview-admin .setting-body{padding:0 14px 14px}.preview-admin .settings-grid{grid-template-columns:1fr}
   .preview-admin .settings-form{grid-template-columns:1fr;gap:12px}.preview-admin .setting-block{background:var(--cd);padding:12px}.preview-admin .setting-block h3{margin-bottom:8px}
   .preview-admin .action-row,.preview-admin .form-actions{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,125px),1fr));gap:8px}
   .preview-admin .btn.action{width:100%;white-space:normal;min-height:40px;padding:9px;box-sizing:border-box;font-size:11px}.preview-admin .admin-head>.btn{width:auto;min-width:75px}
   .preview-admin .glyph{width:18px;height:18px;flex:none}.preview-admin .setting-note{font-size:10.5px;line-height:1.9}.preview-admin .setting-body>.ti{font-size:11px;margin:0 0 12px}
   .preview-admin .gmsg{margin-top:10px;overflow-wrap:anywhere}.preview-admin .field input{box-sizing:border-box}.preview-admin .pack-saved{display:flex;align-items:center;justify-content:space-between;gap:10px;font-size:11px;padding:10px 0;margin-bottom:8px;border-bottom:1px solid var(--ln)}
   .preview-admin .pack-saved span{color:var(--mu2)}.preview-admin .preview-admin-demo{margin-top:12px}.preview-admin .preview-admin-demo>summary{font-size:11px;color:var(--mu2);padding:10px 0;cursor:pointer}
   .preview-demo-inputs{display:grid;gap:14px}.preview-demo-inputs label{font-size:11px;display:grid;gap:6px}.preview-demo-inputs input[type=range]{width:100%;min-width:0;accent-color:var(--ac)}
   .preview-demo-options{display:flex;align-items:center;gap:7px;font-size:11px;margin:12px 0}.preview-demo-options input{width:16px;height:16px;flex:none;margin:0}
   .preview-demo-presets{display:flex;gap:6px;flex-wrap:wrap;margin:10px 0}.preview-demo-presets button{font:inherit;font-size:10px;border:1px solid var(--ln2);border-radius:7px;padding:7px 9px;background:var(--cd2);color:var(--tx);cursor:pointer;min-height:36px}
   #wearDemo [hidden],#packPreviewSettings [hidden]{display:none!important}#packPreviewSettings :focus-visible,.preview-banner :focus-visible{outline:2px solid var(--ac);outline-offset:3px}
   @media(prefers-reduced-motion:reduce){.wear-dot{transition:none}}
   @media(max-width:640px){.preview-admin-grid{grid-template-columns:1fr}#adminCard.preview-admin{padding:14px}#wearDemo{padding:17px}.wear-number{font-size:35px}.pack-context{max-width:100%}}
  `;document.head.appendChild(style);
  const banner=document.createElement('aside');banner.id='packPreviewBanner';banner.className='preview-banner';
  banner.innerHTML='<strong>پیش‌نمایش — داده‌های آزمایشی</strong><span>تغییرها فقط در همین مرورگر ذخیره می‌شوند؛ دستگاه تغییر نمی‌کند.</span><button type="button" id="packPreviewLogin">ورود مدیر</button>';
  document.querySelector('main').prepend(banner);
  banner.querySelector('button').addEventListener('click',()=>{if(isAdmin()){showTab(2);}else{admOpen();}});
  const card=document.createElement('section');card.id='wearDemo';card.className='cd';card.setAttribute('aria-label','کارکرد پک باتری');
  card.innerHTML='<div class="pack-top">'+icon('activity')+'<h2>کارکرد پک باتری</h2><span class="pack-tag">بررسی دوره‌ای</span></div><div class="pack-metric" aria-live="polite" aria-atomic="true"><strong class="wear-number"></strong><span class="wear-unit">شاخص کارکرد پک</span></div><div class="wear-track" role="img"><div class="wear-fill"></div><span class="wear-tick" aria-hidden="true"></span><span class="wear-dot" aria-hidden="true"></span></div><div class="wear-scale"><span>شروع کارکرد</span><span>زمان بررسی</span></div><div class="pack-bottom"><span class="wear-state" role="status"></span><span class="pack-context">یادآور بررسی بر اساس تعداد شارژ کامل</span></div>';
  stats.after(card);
  const network=sectionFor(admin,'#admSsid'),reset=sectionFor(admin,'#resetHelp'),technical=admin.querySelector('#technicalInfo'),cleanup=sectionFor(admin,'#admClearType'),factory=sectionFor(admin,'#admFactoryReset');
  const snap=admin.querySelector('[onclick*="/evt/snap"]'),downloads=snap?snap.closest('details'):null;
  if(snap){snap.id='previewLogSnapshot';snap.className='btn ico';snap.dataset.admin='';snap.hidden=true;snap.querySelector('span').className='lbl';document.querySelector('#evtCard .icog').appendChild(snap);}
  if(downloads)downloads.remove();
  const duplicateFormat=document.getElementById('evtFormat');if(duplicateFormat)duplicateFormat.remove();
  const settings=document.createElement('details');settings.id='packPreviewSettings';settings.className='setting-panel admin-section';settings.open=true;
  settings.innerHTML='<summary>'+icon('activity')+'<span>بررسی دوره‌ای پک</span></summary><div class="setting-body"><div class="pack-saved"><span>حد بررسی فعلی</span><b id="previewSavedLimit"></b></div><form id="previewLimitForm" novalidate><div class="settings-form"><label class="field" for="wearLimit"><span>حد بررسی (تعداد شارژ کامل)</span><input id="wearLimit" type="number" min="1" max="100000" step="1" inputmode="numeric" placeholder="تعداد را وارد کنید" aria-describedby="previewLimitHelp previewLimitStatus"></label><div class="form-actions"><button type="submit" class="btn action" id="previewLimitSave" title="ذخیره حد بررسی" aria-label="ذخیره حد بررسی">'+icon('save')+'<span>ذخیره حد بررسی</span></button><button type="button" class="btn action" id="previewLimitClear" title="غیرفعال‌کردن یادآور" aria-label="غیرفعال‌کردن یادآور">'+icon('x')+'<span>غیرفعال‌کردن یادآور</span></button></div></div></form><p class="setting-note" id="previewLimitHelp">مدیر، حد بررسی را مطابق برنامهٔ نگهداری باتری تعیین می‌کند. شاخص پک برابر بیشترین تعداد شارژ کامل دو باتری است؛ این عدد، درصد سلامت یا تعداد چرخهٔ کامل پک نیست. پیام نزدیک‌شدن از ۸۰٪ حد تنظیم‌شده فعال می‌شود.</p><p class="gmsg" id="previewLimitStatus" role="status"></p><details class="preview-admin-demo"><summary>داده‌های نمایشی این پیش‌نمایش</summary><div class="preview-demo-presets"><button type="button" data-wear-preset="low">کارکرد کمتر</button><button type="button" data-wear-preset="near">نزدیک بررسی</button><button type="button" data-wear-preset="over">زمان بررسی</button><button type="button" data-wear-preset="unknown">قطع داده</button></div><div class="preview-demo-inputs"><label for="wearUpper">شمارندهٔ نمایشی باتری بالا<input id="wearUpper" type="range" min="0" max="900" value="270"></label><label for="wearLower">شمارندهٔ نمایشی باتری پایین<input id="wearLower" type="range" min="0" max="900" value="330"></label></div><label class="preview-demo-options"><input id="wearHasData" type="checkbox" checked>دریافت داده فعال است</label><p class="setting-note">این شمارنده‌ها نمونه‌اند و فقط کادر پک را در همین پیش‌نمایش تغییر می‌دهند. دکمه‌های نمونه، حد ذخیره‌شده را تغییر نمی‌دهند.</p></details></div>';
  admin.classList.add('preview-admin');
  // Group existing DOM nodes so original confirmations, handlers and admin locking stay intact.
  for(const [name,glyph,items] of [['تنظیمات دستگاه','settings',[network,settings]],['بررسی و رفع خطا','activity',[technical,reset]],['پاک‌سازی و بازیابی','trash',[cleanup,factory]]]){
   const group=document.createElement('section');group.className='preview-admin-group';group.innerHTML='<h3>'+icon(glyph)+name+'</h3><div class="preview-admin-grid"></div>';
   for(const item of items){if(item){item.open=item===network||item===settings;group.lastElementChild.appendChild(item);}}
   admin.insertBefore(group,document.getElementById('admCardMsg'));
  }
  const input=document.getElementById('wearLimit'),saved=document.getElementById('previewSavedLimit'),status=document.getElementById('previewLimitStatus');
  const upper=document.getElementById('wearUpper'),lower=document.getElementById('wearLower'),hasData=document.getElementById('wearHasData');
  let threshold=0,storageFailed=false;
  // A missing setting disables the reminder; never choose a battery service limit automatically.
  try {const raw=localStorage.getItem(STORAGE_KEY);if(raw!==null&&/^\d+$/.test(raw)&&Number(raw)<=100000)threshold=Number(raw);}catch(e){storageFailed=true;}
  const fa=value=>Number(value).toLocaleString('fa-IR');
  // Match the preview's actual admin state; hiding the form alone is not a device security mechanism.
  function isAdmin(){return typeof admReady!=='undefined'&&admReady&&!admin.hidden;}
  // Keep the public card read-only and leave unknown input neutral, not healthy or worn.
  function render(){
   const count=Math.max(Number(upper.value),Number(lower.value)),known=hasData.checked&&threshold>0;
   const ratio=known?count/threshold:0;
   const message=!hasData.checked?'در انتظار دریافت اطلاعات':!threshold?'یادآور بررسی تنظیم نشده':ratio>=1?'زمان بررسی پک رسیده است':ratio>=.8?'نزدیک زمان بررسی':'در محدودهٔ کارکرد تنظیم‌شده';
   const color=!known?'var(--mu)':ratio>=1?'#f28b82':ratio>=.8?'#f3c76b':'#38d9b0';
   const halo=!known?'transparent':ratio>=1?'rgba(242,139,130,.12)':ratio>=.8?'rgba(243,199,107,.12)':'rgba(56,217,176,.12)';
   card.style.setProperty('--wear-progress',(Math.min(1,ratio)*100)+'%');card.style.setProperty('--wear-color',color);card.style.setProperty('--wear-halo',halo);
   card.querySelector('.wear-number').textContent=hasData.checked?fa(count):'—';card.querySelector('.wear-state').textContent=message;
   card.querySelector('.wear-track').setAttribute('aria-label','شاخص کارکرد پک: '+(hasData.checked?fa(count):'در انتظار داده')+'؛ '+message);
   card.querySelector('.wear-dot').hidden=!known;card.querySelector('.wear-tick').hidden=!known;
   saved.textContent=threshold?fa(threshold)+' شارژ کامل':'تنظیم نشده';
  }
  // Save only this simulator's namespaced setting; failed writes never alter the active threshold.
  function save(value){
   if(!isAdmin()){status.textContent='برای تغییر تنظیمات وارد مدیریت شوید.';return;}
   const raw=String(value).trim();
   if(!/^\d+$/.test(raw)||Number(raw)>100000||(Number(raw)<1&&raw!=='0')){status.textContent='تعداد صحیح بین ۱ تا ۱۰۰٬۰۰۰ وارد کنید.';return;}
   const next=Number(raw);
   try{localStorage.setItem(STORAGE_KEY,String(next));if(localStorage.getItem(STORAGE_KEY)!==String(next))throw Error('verify');}
   catch(e){status.textContent='ذخیره در مرورگر انجام نشد؛ تنظیم قبلی حفظ شد.';return;}
   threshold=next;input.value=threshold?String(threshold):'';status.textContent=threshold?'حد بررسی در این مرورگر ذخیره شد.':'یادآور بررسی غیرفعال شد.';render();
  }
  document.getElementById('previewLimitForm').addEventListener('submit',event=>{event.preventDefault();if(input.value.trim()==='0'){status.textContent='برای غیرفعال‌کردن، دکمهٔ مربوط را بزنید.';return;}save(input.value);});
  document.getElementById('previewLimitClear').addEventListener('click',()=>save('0'));
  settings.querySelectorAll('.preview-admin-demo input').forEach(el=>el.addEventListener('input',()=>{if(isAdmin())render();}));
  settings.querySelectorAll('[data-wear-preset]').forEach(button=>button.addEventListener('click',()=>{
   if(!isAdmin())return;const preset=button.dataset.wearPreset,values={low:[60,90],near:[240,270],over:[330,420],unknown:[0,0]}[preset];
   upper.value=values[0];lower.value=values[1];hasData.checked=preset!=='unknown';render();
  }));
  // Reset unsaved input and disclosure state on logout; retain only the saved preview threshold.
  function syncAdmin(){input.value=threshold?String(threshold):'';status.textContent=storageFailed?'ذخیره‌سازی مرورگر در دسترس نیست؛ تنظیم حد تا رفع آن ذخیره نمی‌شود.':'';if(!isAdmin())settings.querySelector('.preview-admin-demo').open=false;banner.querySelector('button').textContent=isAdmin()?'تنظیمات مدیر':'ورود مدیر';}
  for(const name of ['admUnlock','admLock']){const original=window[name];window[name]=function(...args){const result=original.apply(this,args);syncAdmin();return result;};}
  render();syncAdmin();showTab(1);
 }
 if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',install);else install();
})();
