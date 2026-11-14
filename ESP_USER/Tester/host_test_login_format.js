'use strict';
// Native clicks against the generated offline panel, including its fetch adapter.
const fs=require('fs'),path=require('path'),assert=require('assert/strict');
const {JSDOM,VirtualConsole}=require('jsdom');
const html=fs.readFileSync(path.join(__dirname,'..','simple_preview.html'),'utf8');
const errors=[];
const vc=new VirtualConsole();vc.on('jsdomError',e=>errors.push(e.message));
const dom=new JSDOM(html,{runScripts:'dangerously',url:'file:///panel.html',virtualConsole:vc,
    beforeParse(w){w.HTMLCanvasElement.prototype.getContext=()=>null;w.setInterval=()=>0;}});
const w=dom.window,d=w.document;
const flush=async()=>{for(let i=0;i<6;i++)await new Promise(r=>setImmediate(r));};
let checks=0;
function check(ok,msg){assert.ok(ok,msg);checks++;console.log('PASS: '+msg);}
async function login(password){
    w.admOpen();d.getElementById('admPass').value=password;
    d.getElementById('admLoginBtn').click();await flush();
}
(async()=>{
    await flush();
    const format=d.getElementById('admFormat');
    check(format && format.closest('#adminCard').hidden,'format lives only in hidden admin card');
    check((await w.fetch('/evt/format',{method:'POST'})).status===403,'offline format requires authentication');
    await login('wrong');
    check(!d.getElementById('admGate').hidden && d.getElementById('admMsg').textContent.includes('نادرست'),'wrong password still refused');
    await login('admin');
    check(d.getElementById('admGate').hidden && !d.getElementById('adminCard').hidden,'admin password accepted through actual click and form encoding');
    const originalFetch=w.fetch;let formatCalls=0;
    w.fetch=(u,o)=>{if(u==='/evt/format')formatCalls++;return originalFetch(u,o);};
    w.confirm=()=>true;await w.evtFs(0);
    const removeWifi=[...d.querySelectorAll('#evtFs [data-file]')].find(b=>b.dataset.file==='/wifi.json');
    check(!!removeWifi,'offline admin sees an exact-path delete control for settings');
    removeWifi.click();await flush();
    const afterDelete=await (await w.fetch('/evt/fs')).json();
    check(!afterDelete.files.some(f=>f.name==='/wifi.json') && !d.getElementById('adminCard').hidden,'native offline click deletes selected settings file without logging admin out');
    await originalFetch('/evt/format',{method:'POST'});
    await w.fetch('/evt/snap',{method:'POST'});
    check((await (await w.fetch('/evt/days')).text()).trim().length>0,'history exists before format');
    w.confirm=()=>false;format.click();await flush();
    check(formatCalls===0,'cancelled confirmation never sends format');
    w.confirm=()=>true;format.click();format.click();await flush();
    check(formatCalls===1 && !format.disabled,'confirmed format sends once and unlocks');
    check((await (await w.fetch('/evt/days')).text()).trim()==='','format actually clears offline history');
    check(d.getElementById('admCardMsg').textContent.includes('فرمت شد'),'success shown only for plain ok response');
    // A failed or denied format must not be called a success.
    w.fetch=async()=>({ok:false,status:500,text:async()=>'fail'});
    format.click();await flush();
    check(!format.disabled && d.getElementById('admCardMsg').textContent.includes('انجام نشد'),'format failure is visible and retryable');
    w.fetch=async()=>({ok:false,status:403,text:async()=>'admin'});
    format.click();await flush();
    check(d.getElementById('adminCard').hidden && !format.disabled,'expired session locks admin without losing button state');
    w.fetch=originalFetch;
    await w.admLogout();await login('admin');
    d.getElementById('admOld').value='admin';d.getElementById('admNew').value='new &+ رمز';d.getElementById('admNew2').value='new &+ رمز';
    const change=[...d.querySelectorAll('#adminCard button')].find(b=>b.textContent==='تغییر');
    change.click();await flush();
    check(d.getElementById('admCardMsg').textContent.includes('عوض شد'),'form adapter also handles password change');
    await w.admLogout();await login('admin');
    check(!d.getElementById('admGate').hidden,'old password refused after change and logout');
    await login('new &+ رمز');
    check(d.getElementById('admGate').hidden,'changed password survives logout; encoded characters preserved');
    check(errors.length===0,'no script or stylesheet errors during native clicks');
    console.log(`ALL ${checks} LOGIN / FORMAT CHECKS PASSED`);
})().then(()=>w.close()).catch(e=>{console.error(e);w.close();process.exitCode=1;});
