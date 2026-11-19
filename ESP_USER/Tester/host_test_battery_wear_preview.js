'use strict';
// Test the generated simulator proposal through actual login, form, persistence and moved controls.
const fs=require('fs'),path=require('path'),os=require('os'),assert=require('assert/strict'),cp=require('child_process');
const {JSDOM,VirtualConsole}=require('jsdom');
const root=path.join(__dirname,'..'),tmp=fs.mkdtempSync(path.join(os.tmpdir(),'pack-preview-'));
const output=path.join(tmp,'preview.html');
cp.execFileSync('python3',[path.join(root,'Simulator','make_battery_wear_preview.py'),'--output',output]);
const html=fs.readFileSync(output,'utf8'),script=fs.readFileSync(path.join(root,'Simulator','battery_wear_preview.js'),'utf8');
const KEY='changeover_preview_pack_review_v1',REPLACEMENT='changeover_preview_pack_replacement_v1';let checks=0;
function check(ok,label){assert.ok(ok,label);checks++;console.log('PASS: '+label);}
const tick=async()=>{for(let i=0;i<8;i++)await new Promise(r=>setImmediate(r));};
async function session(map=new Map(),deny=false){
 const errors=[],vc=new VirtualConsole();vc.on('jsdomError',e=>errors.push(e.message));let outgoing=0;
 const dom=new JSDOM(html,{runScripts:'dangerously',pretendToBeVisual:true,url:'http://preview.test',virtualConsole:vc,beforeParse(w){
  w.setInterval=()=>0;w.HTMLCanvasElement.prototype.getContext=()=>null;
  w.fetch=async()=>{outgoing++;throw Error('must stay mocked');};
  Object.defineProperty(w,'localStorage',{configurable:true,value:{getItem:key=>map.has(key)?map.get(key):null,setItem:(key,value)=>{if(deny)throw Error('storage denied');map.set(key,String(value));}}});
 }});
 await tick();return {dom,w:dom.window,d:dom.window.document,errors,map,outgoing:()=>outgoing};
}
async function login(p,password='admin'){p.w.admOpen();p.d.getElementById('admPass').value=password;p.d.getElementById('admLoginBtn').click();await tick();}
function submit(p,value){p.d.getElementById('wearLimit').value=value;p.d.getElementById('previewLimitForm').dispatchEvent(new p.w.Event('submit',{bubbles:true,cancelable:true}));}
(async()=>{
 const p=await session(),{w,d}=p,card=d.getElementById('wearDemo');
 check(card&&card.previousElementSibling.contains(d.getElementById('stUptime'))&&card.nextElementSibling.querySelector('#cCharge'),'exactly one pack card sits between work statistics and calendar');
 check(d.querySelectorAll('#wearDemo').length===1&&card.querySelectorAll('.wear-number').length===1&&card.querySelector('.wear-number').textContent==='۳۳۰','pack uses maximum of both sample counters, not their sum or a full-cycle claim');
 check(!card.querySelector('input,button,details,.wear-limit')&&!/فرضی|حدسی|گمانی/.test(card.textContent),'public card has no settings, demo controls, limit values or speculative wording');
 check(d.querySelector('#packPreviewBanner strong').textContent==='پیش‌نمایش'&&!d.getElementById('packPreviewBanner').textContent.includes('مرورگر'),'preview retains a compact truthful label without developer prose');
 check(!/مرورگر|درصد سلامت|چرخهٔ کامل|۸۰٪|فرمول|شمارنده‌ها نمونه/.test(d.getElementById('packPreviewSettings').textContent)&&!d.getElementById('previewLimitHelp'),'programmer explanations are removed from manager settings');
 check(d.querySelector('#pg3>.simbox:first-child .simhint').textContent==='تنظیم داده‌ها','old developer introduction is replaced by a short control title');
 check(d.getElementById('chargeCalendar').textContent.includes('فقط سوابق موجود نمایش داده می‌شوند؛ شروع و مدت شارژ فقط در صورت وجود داده نمایش داده می‌شوند؛ این اطلاعات میتواند معیار سلامت باتری باشد.'),'approved calendar guidance is preserved verbatim');
 check(card.querySelector('.wear-state').textContent==='یادآور بررسی تنظیم نشده'&&card.querySelector('.wear-dot').hidden&&!p.map.has(KEY),'new preview starts with no invented service threshold or wear verdict');
 check(d.getElementById('packPreviewSettings').closest('#adminCard[hidden]')&&d.getElementById('previewLogSnapshot').hidden,'threshold and snapshot are hidden from visitors');
 submit(p,'300');check(!p.map.has(KEY),'programmatic submit while logged out cannot save even a simulator setting');
 await login(p,'wrong');check(d.getElementById('adminCard').hidden,'wrong password cannot open the proposed settings');
 await login(p);check(!d.getElementById('adminCard').hidden&&!d.getElementById('previewLogSnapshot').hidden,'real offline login unlocks settings and the log-only snapshot');
 check(d.querySelectorAll('#adminCard>.preview-admin-group').length===3&&d.querySelectorAll('#adminCard .admin-section').length===6,'admin actions are grouped into device, diagnosis and cleanup without dropping reset controls');
 check(!d.querySelector('[onclick*="admDl()"]')&&!d.getElementById('evtFormat')&&d.getElementById('admFormat'),'duplicate day-download panel and log format button are removed; one admin format remains');
 check(d.getElementById('previewLogSnapshot').closest('#evtCard')&&!d.getElementById('previewLogSnapshot').closest('#adminCard'),'snapshot is beside logs, not a separate admin section');
 check([...d.querySelectorAll('#adminCard .danger-zone')].every(el=>!el.open),'destructive panels stay collapsed by default');
 const ids=[...d.querySelectorAll('[id]')].map(el=>el.id);check(new Set(ids).size===ids.length,'regrouping introduces no duplicate element IDs');
 check([...d.querySelectorAll('#adminCard use')].every(el=>d.querySelector(el.getAttribute('href'))),'all admin line icons still resolve to local sprite symbols');
 const fetchOriginal=w.fetch,sent=[];w.fetch=(url,options)=>{if(options&&options.method==='POST')sent.push([url,options.body]);return fetchOriginal(url,options);};
 let warning='';w.confirm=text=>{warning=text;return false;};
 const resets=[...d.querySelectorAll('#adminCard button[onclick*="admConfirm("]')];
 for(const button of [...resets,d.querySelector('#adminCard button[onclick*="admClear()"]'),d.getElementById('admFormat'),d.getElementById('admFactoryReset')]){button.click();await tick();}
 check(!sent.length&&warning.includes('کالیبراسیون')&&warning.includes('رمز'),'cancel still blocks every regrouped destructive action and preserves the full factory warning');
 w.confirm=()=>true;for(const button of resets.filter(button=>button.getAttribute('onclick').includes('/latch/reset'))){button.click();await tick();}
 check(sent.length===3&&sent.every(([url])=>url==='/latch/reset')&&sent.map(([,body])=>new w.URLSearchParams(body).get('n')).join(',')==='5,6,8','regrouped reset buttons keep the exact permitted commands');
 sent.length=0;d.getElementById('previewLogSnapshot').click();await tick();check(sent.length===1&&sent[0][0]==='/evt/snap','relocated snapshot retains its original manual-log action');w.fetch=fetchOriginal;
 submit(p,'300');check(p.map.get(KEY)==='300'&&d.getElementById('previewSavedLimit').textContent==='۳۰۰ شارژ کامل'&&card.querySelector('.wear-state').textContent==='زمان بررسی پک رسیده است','admin save commits the service threshold and updates the read-only pack card');
 for(const [preset,count,message]of [['low','۹۰','در محدودهٔ کارکرد تنظیم‌شده'],['near','۲۷۰','نزدیک زمان بررسی'],['over','۴۲۰','زمان بررسی پک رسیده است'],['unknown','—','در انتظار دریافت اطلاعات']]){
  d.querySelector('[data-wear-preset='+preset+']').click();check(card.querySelector('.wear-number').textContent===count&&card.querySelector('.wear-state').textContent===message&&p.map.get(KEY)==='300','scenario '+preset+' changes sample counters without editing the saved threshold');
 }
 check(card.querySelector('.wear-dot').hidden,'no data produces a neutral indicator without a false zero or healthy state');
 d.querySelector('[data-wear-preset=low]').click();
 for(const [count,message]of [[239,'در محدودهٔ کارکرد تنظیم‌شده'],[240,'نزدیک زمان بررسی'],[299,'نزدیک زمان بررسی'],[300,'زمان بررسی پک رسیده است'],[900,'زمان بررسی پک رسیده است']]){
  d.getElementById('wearUpper').value=count;d.getElementById('wearUpper').dispatchEvent(new w.Event('input'));check(card.querySelector('.wear-state').textContent===message,'configured threshold boundary at '+count);
 }
 check(card.style.getPropertyValue('--wear-progress')==='100%','over-limit pointer stays at end of the spectrum');
 for(const value of ['',0,-1,1.5,100001]){submit(p,value);check(p.map.get(KEY)==='300'&&d.getElementById('previewSavedLimit').textContent==='۳۰۰ شارژ کامل','invalid threshold never replaces the saved limit: '+value);}
 d.getElementById('wearLimit').value='700';await w.admLogout();check(d.getElementById('wearLimit').value==='300'&&d.getElementById('previewLogSnapshot').hidden&&d.getElementById('adminCard').hidden,'logout discards unsaved edits and hides all manager-only controls');
 submit(p,'400');check(p.map.get(KEY)==='300','logged-out direct form submission still cannot change the saved limit');
 await login(p);d.getElementById('previewLimitClear').click();check(p.map.get(KEY)==='0'&&card.querySelector('.wear-dot').hidden,'only an explicit manager action disables the review reminder');
 submit(p,'300');await w.admLogout();check(d.getElementById('adminCard').hidden&&d.getElementById('previewLogSnapshot').hidden,'actual logout also locks the relocated snapshot');
 check(p.outgoing()===0&&!p.errors.length,'generated HTTP preview never contacts a device or emits script errors');w.close();
 const restored=await session(p.map);check(restored.d.getElementById('previewSavedLimit').textContent==='۳۰۰ شارژ کامل'&&restored.d.getElementById('adminCard').hidden,'saved threshold survives preview reload without granting administrator access');restored.w.close();
 const blocked=await session(new Map(),true);await login(blocked);submit(blocked,'300');check(blocked.d.getElementById('previewLimitStatus').textContent.includes('انجام نشد')&&blocked.d.querySelector('.wear-dot').hidden,'blocked browser storage reports failure and retains the active setting');blocked.w.close();
 const corrupt=await session(new Map([[KEY,'bad']]));check(corrupt.d.querySelector('.wear-dot').hidden,'corrupt stored setting never invents a service limit');corrupt.w.close();
 for(const file of ['simple_panel.h','simple_preview.html','index.html','simple_panel_clean.html','ESP_USER.ino']){const content=fs.readFileSync(path.join(root,file),'utf8');check(!content.includes('wearDemo')&&!content.includes('packPreviewSettings'),'simulator-only controls stay out of production '+file);}
 const replaced=await session(new Map([[KEY,'300']])),rw=replaced.w,rd=replaced.d,replace=rd.getElementById('previewReplaceBattery');
 let confirmations=0;rw.confirm=()=>{confirmations++;return false;};replace.click();await tick();
 check(confirmations===0&&!replaced.map.has(REPLACEMENT),'logged-out clicks cannot register a replacement or open confirmation');
 await login(replaced);
 const boardBefore=await (await rw.fetch('/diag')).json(),historyBefore=await (await rw.fetch('/evt')).text();
 replace.click();await tick();check(confirmations===1&&!replaced.map.has(REPLACEMENT)&&rd.querySelector('.wear-number').textContent==='۳۳۰','cancel preserves count, date and stored replacement state');
 rw.confirm=()=>{confirmations++;return true;};replace.click();replace.click();await tick();
 const record=JSON.parse(replaced.map.get(REPLACEMENT)),savedDate=rd.getElementById('previewReplacedAt').textContent;
 check(confirmations===2&&record.upper===270&&record.lower===330&&record.at>0,'confirmed replacement records both baselines and date once, including duplicate-click protection');
 check(rd.querySelector('.wear-number').textContent==='۰'&&rd.querySelector('.wear-unit').textContent==='شارژ پس از تعویض'&&savedDate!=='ثبت نشده','confirmed replacement starts the new pack at zero and shows its date');
 check(replaced.map.get(KEY)==='300','replacement preserves the configured review threshold');
 const boardAfter=await (await rw.fetch('/diag')).json(),historyAfter=await (await rw.fetch('/evt')).text();
 check(boardAfter.chargeCountCh1===boardBefore.chargeCountCh1&&boardAfter.chargeCountCh2===boardBefore.chargeCountCh2&&JSON.stringify(boardAfter.chargeHistory)===JSON.stringify(boardBefore.chargeHistory)&&historyBefore===historyAfter,'replacement never resets board counters or deletes existing charge/log history');
 rd.getElementById('wearUpper').value=271;rd.getElementById('wearUpper').dispatchEvent(new rw.Event('input'));
 check(rd.querySelector('.wear-number').textContent==='۱','a charge on either battery counts after replacement, even when it is not the larger absolute counter');
 rd.getElementById('wearUpper').value=275;rd.getElementById('wearLower').value=332;rd.getElementById('wearLower').dispatchEvent(new rw.Event('input'));
 check(rd.querySelector('.wear-number').textContent==='۵','pack work uses maximum of per-battery deltas, not summed charges or delta of absolute maxima');
 submit(replaced,'500');check(JSON.parse(replaced.map.get(REPLACEMENT)).at===record.at&&rd.getElementById('previewReplacedAt').textContent===savedDate&&rd.querySelector('.wear-number').textContent==='۵','changing the review threshold preserves replacement baseline, date and current work');
 rd.getElementById('wearUpper').value=269;rd.getElementById('wearUpper').dispatchEvent(new rw.Event('input'));
 check(rd.querySelector('.wear-number').textContent==='—'&&rd.querySelector('.wear-dot').hidden&&rd.querySelector('.wear-state').textContent==='اطلاعات کارکرد نیاز به بررسی دارد','counter rollback never becomes a negative value or a false fresh battery');
 const known=rd.getElementById('wearHasData');known.checked=false;known.dispatchEvent(new rw.Event('input'));const storedBefore=replaced.map.get(REPLACEMENT);replace.click();await tick();
 check(replaced.map.get(REPLACEMENT)===storedBefore&&rd.getElementById('previewLimitStatus').textContent.includes('پس از دریافت اطلاعات'),'replacement cannot be registered without current counter data');
 check(!/مرورگر|در برد ذخیره/.test(rd.getElementById('packPreviewSettings').textContent),'user messages contain no storage implementation detail or false board-save claim');
 const replacementMap=new Map(replaced.map);rw.close();
 const replacementReload=await session(replacementMap);
 check(replacementReload.d.querySelector('.wear-number').textContent==='۰'&&replacementReload.d.getElementById('previewReplacedAt').textContent===savedDate&&replacementReload.d.getElementById('adminCard').hidden,'replacement date and baseline survive reload without unlocking the manager');replacementReload.w.close();
 const replaceBlocked=await session(new Map([[KEY,'300'],[REPLACEMENT,storedBefore]]),true);await login(replaceBlocked);replaceBlocked.w.confirm=()=>true;
 replaceBlocked.d.getElementById('wearUpper').value=280;replaceBlocked.d.getElementById('wearUpper').dispatchEvent(new replaceBlocked.w.Event('input'));
 replaceBlocked.d.getElementById('previewReplaceBattery').click();await tick();
 check(replaceBlocked.map.get(REPLACEMENT)===storedBefore&&replaceBlocked.d.querySelector('.wear-number').textContent==='۱۰'&&replaceBlocked.d.getElementById('previewLimitStatus').textContent.includes('انجام نشد'),'failed replacement persistence retains the prior date/baseline and active count');replaceBlocked.w.close();
 for(const raw of ['bad','null',JSON.stringify({v:1,upper:-1,lower:0,at:Date.now()}),JSON.stringify({v:1,upper:270,lower:330,at:0})]){
  const bad=await session(new Map([[KEY,'300'],[REPLACEMENT,raw]]));check(bad.d.querySelector('.wear-number').textContent==='—'&&bad.d.querySelector('.wear-dot').hidden,'corrupt replacement record is not interpreted as an unused battery');bad.w.close();
 }
 check(!/fetch\(/.test(script)&&script.includes('@media(prefers-reduced-motion:reduce)'),'proposal adds no device calls and respects reduced motion');
 console.log(`ALL ${checks} BATTERY WEAR PREVIEW CHECKS PASSED`);
})().catch(e=>{console.error(e);process.exitCode=1;}).finally(()=>fs.rmSync(tmp,{recursive:true,force:true}));
