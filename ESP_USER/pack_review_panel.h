/**
 * @file pack_review_panel.h
 * @brief Shared ESP maintenance card and grouped administrator UI.
 */
#pragma once
static const char SIMPLE_PACK_REVIEW_JS[] PROGMEM = R"packjs(
'use strict';
// ESP-backed maintenance indicator and administrator settings.
(function () {
 // Build a local line icon using the panel sprite, without network assets.
 function icon(name) {return '<svg class="glyph" aria-hidden="true"><use href="#i-'+name+'"/></svg>';}
 // Find the existing action section without rebuilding its controls or handlers.
 function sectionFor(admin,selector) {const el=admin.querySelector(selector);return el?el.closest('details'):null;}
 // Install once while retaining existing native administrator handlers.
 function install() {
  const charts=document.getElementById('pg1'),admin=document.getElementById('adminCard');
  if(!charts||!admin||document.getElementById('packWork'))return;
  const stats=document.getElementById('stUptime').closest('.cd');
  const style=document.createElement('style');style.id='packReviewStyle';style.textContent=`
   #packWork{padding:22px;--wear-spectrum:linear-gradient(90deg,#38d9b0 0%,#62cbb9 52%,#f3c76b 80%,#f28b82 100%)}
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
   .preview-admin .pack-saved span{color:var(--mu2)}
   #packWork [hidden],#packReviewSettings [hidden]{display:none!important}#packReviewSettings :focus-visible{outline:2px solid var(--ac);outline-offset:3px}
   @media(prefers-reduced-motion:reduce){.wear-dot{transition:none}}
   @media(max-width:640px){.preview-admin-grid{grid-template-columns:1fr}#adminCard.preview-admin{padding:14px}#packWork{padding:17px}.wear-number{font-size:35px}.pack-context{max-width:100%}}
  `;document.head.appendChild(style);
  const card=document.createElement('section');card.id='packWork';card.className='cd';card.setAttribute('aria-label','کارکرد پک باتری');
  card.innerHTML='<div class="pack-top">'+icon('activity')+'<h2>کارکرد پک باتری</h2><span class="pack-tag">بررسی دوره‌ای</span></div><div class="pack-metric" aria-live="polite" aria-atomic="true"><strong class="wear-number"></strong><span class="wear-unit">شاخص کارکرد پک</span></div><div class="wear-track" role="img"><div class="wear-fill"></div><span class="wear-tick" aria-hidden="true"></span><span class="wear-dot" aria-hidden="true"></span></div><div class="wear-scale"><span>شروع کارکرد</span><span>زمان بررسی</span></div><div class="pack-bottom"><span class="wear-state" role="status"></span><span class="pack-context">یادآور بررسی بر اساس تعداد شارژ کامل</span></div>';
  stats.after(card);
  stats.querySelector('.ti').textContent='آمار کار';
  const diagnostics=document.getElementById('diagErrors');if(diagnostics)diagnostics.previousElementSibling.textContent='۱۰ خطای آخر';
  const network=sectionFor(admin,'#admSsid'),reset=sectionFor(admin,'#resetHelp'),technical=admin.querySelector('#technicalInfo'),cleanup=sectionFor(admin,'#admClearType'),factory=sectionFor(admin,'#admFactoryReset');
  const snap=admin.querySelector('[onclick*="/evt/snap"]'),downloads=snap?snap.closest('details'):null;
  if(snap){snap.id='packLogSnapshot';snap.className='btn ico';snap.dataset.admin='';snap.hidden=true;snap.querySelector('span').className='lbl';document.querySelector('#evtCard .icog').appendChild(snap);}
  if(downloads)downloads.remove();
  const duplicateFormat=document.getElementById('evtFormat');if(duplicateFormat)duplicateFormat.remove();
  const settings=document.createElement('details');settings.id='packReviewSettings';settings.className='setting-panel admin-section';settings.open=true;
  settings.innerHTML='<summary>'+icon('activity')+'<span>بررسی دوره‌ای پک</span></summary><div class="setting-body"><div class="pack-saved"><span>حد بررسی فعلی</span><b id="previewSavedLimit"></b></div><form id="previewLimitForm" novalidate><div class="settings-form"><label class="field" for="wearLimit"><span>حد بررسی (تعداد شارژ کامل)</span><input id="wearLimit" type="number" min="1" max="100000" step="1" inputmode="numeric" placeholder="تعداد را وارد کنید" aria-describedby="previewLimitStatus"></label><div class="form-actions"><button type="submit" class="btn action" id="previewLimitSave" title="ذخیره حد بررسی" aria-label="ذخیره حد بررسی">'+icon('save')+'<span>ذخیره حد بررسی</span></button><button type="button" class="btn action" id="previewLimitClear" title="غیرفعال‌کردن یادآور" aria-label="غیرفعال‌کردن یادآور">'+icon('x')+'<span>غیرفعال‌کردن یادآور</span></button></div></div></form><div class="pack-saved"><span>آخرین تعویض باتری</span><b id="previewReplacedAt">ثبت نشده</b></div><button type="button" class="btn action" id="previewReplaceBattery" title="ثبت تعویض باتری" aria-label="ثبت تعویض باتری">'+icon('reset')+'<span>باتری تعویض شد</span></button><p class="gmsg" id="previewLimitStatus" role="status"></p></div>';
  admin.classList.add('preview-admin');
  // Group existing DOM nodes so original confirmations, handlers and admin locking stay intact.
  for(const [name,glyph,items] of [['تنظیمات دستگاه','settings',[network,settings]],['بررسی و رفع خطا','activity',[technical,reset]],['پاک‌سازی و بازیابی','trash',[cleanup,factory]]]){
   const group=document.createElement('section');group.className='preview-admin-group';group.innerHTML='<h3>'+icon(glyph)+name+'</h3><div class="preview-admin-grid"></div>';
   for(const item of items){if(item){item.open=item===network||item===settings;group.lastElementChild.appendChild(item);}}
   admin.insertBefore(group,document.getElementById('admCardMsg'));
  }
  const input=document.getElementById('wearLimit'),saved=document.getElementById('previewSavedLimit'),status=document.getElementById('previewLimitStatus');
  const fa=value=>Number(value).toLocaleString('fa-IR');
  const dateFormatter=new Intl.DateTimeFormat('fa-IR-u-ca-persian',{year:'numeric',month:'long',day:'numeric',hour:'2-digit',minute:'2-digit'});
  let config=null,busy=false,epoch=0,polling=false;
  // Check the actual panel session before requesting or displaying private settings.
  function isAdmin(){return typeof admReady!=='undefined'&&admReady&&!admin.hidden;}
  // Render only the ESP-derived result; unavailable network data always returns to neutral.
  function render(data){
   const messages=['در انتظار دریافت اطلاعات','اطلاعات کارکرد نیاز به بررسی دارد','یادآور بررسی تنظیم نشده','در محدودهٔ کارکرد تنظیم‌شده','نزدیک زمان بررسی','زمان بررسی پک رسیده است'];
   const state=data&&Number.isInteger(data.state)&&data.state>=0&&data.state<=5?data.state:0;
   const available=!!(data&&data.valid===1&&Number.isInteger(data.count)&&data.count>=0);
   const known=available&&state>=3;
   const progress=known?Math.max(0,Math.min(1000,data.progress||0))/10:0;
   const color=!known?'var(--mu)':state===5?'#f28b82':state===4?'#f3c76b':'#38d9b0';
   const halo=!known?'transparent':state===5?'rgba(242,139,130,.12)':state===4?'rgba(243,199,107,.12)':'rgba(56,217,176,.12)';
   card.style.setProperty('--wear-progress',progress+'%');card.style.setProperty('--wear-color',color);card.style.setProperty('--wear-halo',halo);
   card.querySelector('.wear-number').textContent=available?fa(data.count):'—';card.querySelector('.wear-state').textContent=messages[state];
   card.querySelector('.wear-unit').textContent=data&&data.replaced?'شارژ پس از تعویض':'شاخص کارکرد پک';
   card.querySelector('.wear-track').setAttribute('aria-label','شاخص کارکرد پک: '+(available?fa(data.count):'در انتظار داده')+'؛ '+messages[state]);
   card.querySelector('.wear-dot').hidden=!known;card.querySelector('.wear-tick').hidden=!known;
  }
  // Refresh with a timeout and serialize polls so older replies cannot overwrite a confirmed save.
  async function poll(){
   if(polling)return;polling=true;
   const version=epoch,controller=new AbortController(),timer=setTimeout(()=>controller.abort(),8000);
   try{const response=await fetch('/pack',{cache:'no-store',signal:controller.signal});if(!response.ok)throw Error('read');const data=await response.json();if(version===epoch)render(data);}
   catch(e){if(version===epoch)render(null);}
   finally{clearTimeout(timer);polling=false;}
  }
  // Private metadata is kept only during the current authenticated session.
  function applyConfig(data){
   config=data;saved.textContent=data.invalid?'نیاز به بررسی':data.limit?fa(data.limit)+' شارژ کامل':'تنظیم نشده';
   input.value=data.limit?String(data.limit):'';
   document.getElementById('previewReplacedAt').textContent=data.invalid?'نیاز به بررسی':data.at?dateFormatter.format(new Date(data.at*1000)):'ثبت نشده';
  }
  // Load settings through the existing authenticated request helper.
  async function loadConfig(){
   const version=epoch;
   try{const response=await fetch('/pack/settings',{headers:admHeaders(),cache:'no-store'});const data=await response.json();if(version!==epoch||!isAdmin())return;if(response.status===403){admLock();return;}if(!response.ok||data.ok!==1)throw Error('read');applyConfig(data);}
   catch(e){if(version===epoch&&isAdmin())status.textContent='تنظیمات خوانده نشد؛ دوباره وارد مدیریت شوید.';}
  }
  // Submit one mutation at a time; only a verified ESP response can produce a saved message.
  async function mutate(path,fields,success){
   if(!isAdmin()||!config||busy)return;
   busy=true;epoch++;const version=epoch;
   settings.querySelectorAll('button').forEach(button=>button.disabled=true);
   try{
    const response=await admCall(path,Object.assign({revision:config.revision},fields));
    const data=response.body;if(version!==epoch||!isAdmin())return;
    if(response.status===403){admLock();return;}
    if(response.status!==200||data.ok!==1){status.textContent=data.e==='data'?'پس از دریافت اطلاعات تازه و تنظیم ساعت، تعویض باتری را ثبت کنید.':data.e==='revision'?'اطلاعات تغییر کرده است؛ دوباره بررسی کنید.':data.e==='record'?'اطلاعات تعویض نیاز به بررسی دارد.':'ذخیره انجام نشد؛ تنظیم قبلی حفظ شد.';await loadConfig();return;}
    applyConfig(data);status.textContent=success;
   }catch(e){if(version===epoch&&isAdmin()){status.textContent='پاسخ نرسید؛ نتیجهٔ ذخیره نامشخص است. دوباره وارد مدیریت شوید و بررسی کنید.';config=null;}}
   finally{busy=false;settings.querySelectorAll('button').forEach(button=>button.disabled=false);polling=false;poll();}
  }
  // Validate a deliberate threshold edit without choosing a default.
  function save(value){
   const raw=String(value).trim();
   if(!/^\d+$/.test(raw)||Number(raw)>100000){status.textContent='تعداد صحیح بین ۱ تا ۱۰۰٬۰۰۰ وارد کنید.';return;}
   return mutate('/pack/settings',{limit:Number(raw)},'در برد ذخیره شد.');
  }
  // Preserve the approved confirmation and register both baselines on the ESP, not in the browser.
  function registerReplacement(){
   if(!isAdmin()||!config||busy)return;
   if(!confirm('تعویض پک باتری ثبت شود؟ کارکرد پک از صفر آغاز می‌شود؛ سوابق قبلی و حد بررسی حفظ می‌شوند.'))return;
   return mutate('/pack/replace',{confirm:'REPLACE_PACK'},'تعویض باتری در برد ذخیره شد؛ کارکرد پک از صفر آغاز شد.');
  }
  document.getElementById('previewReplaceBattery').addEventListener('click',registerReplacement);
  document.getElementById('previewLimitForm').addEventListener('submit',event=>{event.preventDefault();if(input.value.trim()==='0'||!input.value.trim()){status.textContent='تعداد صحیح وارد کنید؛ برای غیرفعال‌کردن، دکمهٔ مربوط را بزنید.';return;}save(input.value);});
  document.getElementById('previewLimitClear').addEventListener('click',()=>save('0'));
  // Session changes invalidate in-flight private replies and discard unsaved edits.
  function syncAdmin(){epoch++;config=null;input.value='';saved.textContent='—';document.getElementById('previewReplacedAt').textContent='—';status.textContent='';if(isAdmin())loadConfig();}
  for(const name of ['admUnlock','admLock']){const original=window[name];window[name]=function(...args){const result=original.apply(this,args);syncAdmin();return result;};}
  render(null);syncAdmin();poll();setInterval(poll,5000);
 }
 if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',install);else install();
})();
)packjs";
