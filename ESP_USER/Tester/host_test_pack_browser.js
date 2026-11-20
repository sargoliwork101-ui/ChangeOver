'use strict';
// Own an isolated native ESP server; exercise production HTML/assets over real HTTP without preview shims.
const {chromium}=require('playwright-core');
const fs=require('fs'),os=require('os'),path=require('path'),net=require('net'),cp=require('child_process'),assert=require('assert/strict');
const binary=process.env.SIMPLE_SIM_BINARY,chromiumPath=process.env.CHROMIUM_PATH;
assert(binary&&fs.existsSync(binary),'SIMPLE_SIM_BINARY must name the compiled Simulator/main.cpp executable');
assert(chromiumPath&&fs.existsSync(chromiumPath),'CHROMIUM_PATH must name an installed compatible Chromium');
const root=fs.mkdtempSync(path.join(os.tmpdir(),'pack-browser-'));
let server,browser,serverLog='',checks=0;
function check(ok,why){assert.ok(ok,why);checks++;console.log('PASS: '+why);}
async function unusedPort(){const s=net.createServer();await new Promise(resolve=>s.listen(0,'127.0.0.1',resolve));const port=s.address().port;await new Promise(resolve=>s.close(resolve));return port;}
async function start(port,linkDown=false){
 serverLog='';server=cp.spawn(path.resolve(binary),[],{env:{...process.env,SIMPLE_SIM_FS:root,SIMPLE_SIM_PORT:String(port),SIMPLE_SIM_LINK_DOWN:linkDown?'1':'0',SIMPLE_SIM_PREVIEW:path.join(root,'no-preview-shim.html')},stdio:['ignore','pipe','pipe']});
 await new Promise((resolve,reject)=>{
  const timer=setTimeout(()=>reject(Error('native server startup timeout')),15000);
  function data(chunk){serverLog=(serverLog+chunk).slice(-24000);if(serverLog.includes('0.0.0.0:'+port)){clearTimeout(timer);resolve();}}
  server.stdout.on('data',data);server.stderr.on('data',data);server.once('error',e=>{clearTimeout(timer);reject(e);});server.once('exit',code=>{clearTimeout(timer);reject(Error('native server exited '+code));});
 });
}
async function stop(){
 const child=server;server=null;if(!child||child.exitCode!==null||child.signalCode)return;
 await new Promise(resolve=>{const timer=setTimeout(()=>child.kill('SIGKILL'),3000);child.once('exit',()=>{clearTimeout(timer);resolve();});child.kill('SIGTERM');});
}
async function login(page){await page.evaluate(()=>admOpen());await page.locator('#admPass').fill('admin');await page.locator('#admLoginBtn').click();await page.waitForFunction(()=>!document.getElementById('adminCard').hidden&&document.getElementById('previewSavedLimit').textContent!=='—');}
(async()=>{
 try{
  const port=await unusedPort(),base='http://127.0.0.1:'+port;await start(port);
  browser=await chromium.launch({executablePath:chromiumPath,headless:true,args:['--no-sandbox','--disable-dev-shm-usage']});
  const page=await browser.newPage({viewport:{width:1280,height:1000}}),errors=[],failed=[],mutations=[];let navigating=false;
  function observe(target){
   target.on('pageerror',e=>errors.push(e.message));target.on('requestfailed',r=>{
    const error=r.failure()?.errorText;
    // Navigation and chart-range changes intentionally abort outstanding read requests, never saves.
    if(error==='net::ERR_ABORTED'&&r.method()==='GET'&&(navigating||new URL(r.url()).pathname==='/charts'))return;
    failed.push({url:r.url(),error});
   });
  }
  observe(page);page.on('request',r=>{if(r.method()==='POST'&&r.url().includes('/pack/'))mutations.push(r.url());});
  await page.request.post(base+'/sim',{form:{s1:'3',s2:'3'}});
  await page.goto(base);await page.evaluate(()=>showTab(1));await page.waitForSelector('#packWork');
  await page.waitForFunction(()=>document.querySelector('.wear-number').textContent!=='—');
  check(await page.locator('#packWork').isVisible()&&!await page.locator('#previewReplaceBattery').isVisible(),'public visitor sees one read-only pack card');
  check(await page.locator('#pg3,#packPreviewBanner,#wearUpper,#wearLower,.preview-admin-demo').count()===0,'production page has no simulator shim or mock controls');
  for(const route of ['/pack/settings','/pack/replace'])check((await page.request.post(base+route,{form:{limit:'300',revision:'0',confirm:'REPLACE_PACK'}})).status()===403,'unauthenticated real HTTP mutation denied: '+route);
  await login(page);await page.locator('#wearLimit').fill('300');await page.locator('#previewLimitSave').click();
  await page.waitForFunction(()=>document.getElementById('previewLimitStatus').textContent==='در برد ذخیره شد.');
  check(fs.readFileSync(path.join(root,'pack_review.txt'),'utf8').startsWith('PR1,300,'),'saved message corresponds to a real ESP-path file, not browser persistence');
  await page.waitForFunction(async()=>{const data=await(await fetch('/diag')).json();return data.chargeHistory.every(event=>event.status!=='charging');});
  const before=await page.evaluate(async()=>{const d=await(await fetch('/diag')).json();return {up:d.chargeCountCh1,down:d.chargeCountCh2,history:d.chargeHistory};});
  const count=mutations.length;page.once('dialog',d=>d.dismiss());await page.locator('#previewReplaceBattery').click();
  check(mutations.length===count,'cancelled confirmation sends no replacement POST');
  page.once('dialog',d=>d.accept());await page.locator('#previewReplaceBattery').click();
  await page.waitForFunction(()=>document.getElementById('previewLimitStatus').textContent.includes('تعویض باتری در برد ذخیره شد'));
  await page.waitForFunction(()=>document.querySelector('.wear-number').textContent==='۰');
  check(await page.locator('.wear-number').textContent()==='۰','confirmed replacement starts at zero over real HTTP');
  const date=await page.locator('#previewReplacedAt').textContent(),record=fs.readFileSync(path.join(root,'pack_review.txt'),'utf8');
  check(date!=='ثبت نشده'&&date!=='—','persisted replacement date is displayed');
  const after=await page.evaluate(async()=>{const d=await(await fetch('/diag')).json();return {up:d.chargeCountCh1,down:d.chargeCountCh2,history:d.chargeHistory};});
  check(JSON.stringify(after)===JSON.stringify(before),'replacement leaves both lifetime counters and closed charge history unchanged');
  for(const width of [1280,768,640,390,320]){
   await page.setViewportSize({width,height:1100});await page.evaluate(()=>showTab(2));
   await page.waitForFunction(()=>document.documentElement.scrollWidth<=innerWidth);
   check(await page.locator('#previewReplaceBattery').isVisible()&&await page.locator('#adminCard .admin-section').count()===6,'all grouped admin actions fit at '+width+'px');
   check(await page.locator('#previewReplaceBattery').evaluate(e=>e.getBoundingClientRect().height>=38),'replacement touch target usable at '+width+'px');
   if(process.env.SCREENSHOT_DIR&&[1280,390].includes(width)){fs.mkdirSync(process.env.SCREENSHOT_DIR,{recursive:true});await page.screenshot({path:path.join(process.env.SCREENSHOT_DIR,'pack-admin-'+width+'.png'),fullPage:true,animations:'disabled'});}
   await page.evaluate(()=>showTab(1));check(await page.locator('#packWork').isVisible(),'public card fits at '+width+'px');
  }
  navigating=true;await page.reload();navigating=false;await page.evaluate(()=>showTab(1));await page.waitForFunction(()=>document.querySelector('.wear-number').textContent==='۰');
  check(!await page.locator('#previewReplaceBattery').isVisible(),'reload does not unlock private controls');
  await login(page);check(await page.locator('#previewReplacedAt').textContent()===date,'replacement date survives reload');await page.evaluate(()=>admLogout());
  navigating=true;await page.close();await stop();await start(port,true);
  const reboot=await browser.newPage();observe(reboot);await reboot.goto(base);navigating=false;await reboot.evaluate(()=>showTab(1));await reboot.waitForSelector('#packWork');
  check(fs.readFileSync(path.join(root,'pack_review.txt'),'utf8')===record,'real native process restart retains exact committed record');
  check(await reboot.locator('.wear-number').textContent()==='—','restart with UART disconnected never treats restored counters as fresh');
  await login(reboot);check(await reboot.locator('#previewReplacedAt').textContent()===date&&await reboot.locator('#previewSavedLimit').textContent()==='۳۰۰ شارژ کامل','private date and limit survive process restart independently of browser storage');
  await reboot.evaluate(()=>admLogout());
  check(errors.length===0&&failed.length===0,'live production browser has no script or network failures: '+JSON.stringify({errors,failed}));
  console.log('ALL '+checks+' LIVE PACK BROWSER CHECKS PASSED');
 }catch(e){console.error(serverLog);throw e;}
 finally{if(browser)await browser.close();await stop();fs.rmSync(root,{recursive:true,force:true});}
})().catch(e=>{console.error(e);process.exitCode=1;});
