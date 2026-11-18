'use strict';
// Optional real Chromium layout check; requires playwright-core and CHROMIUM_PATH.
const {chromium}=require('playwright-core');
const {pathToFileURL}=require('url');
const path=require('path');
const assert=require('assert/strict');
(async()=>{
 const browser=await chromium.launch({executablePath:process.env.CHROMIUM_PATH,args:['--no-sandbox','--disable-dev-shm-usage'],headless:true});
 try{
  const page=await browser.newPage(),errors=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.route('**/*',r=>/^https?:/.test(r.request().url())?r.abort():r.continue());
  await page.goto(pathToFileURL(path.join(__dirname,'..','simple_preview.html')).href);
  await page.click('#tab2');
  for(const width of [320,360,390,768,1366]){
   await page.setViewportSize({width,height:900});await page.waitForTimeout(400);
   for(const selector of ['#connectionSettings','#evtCard','#technicalInfo'])await page.locator(selector).evaluate(e=>e.open=true);
   await page.evaluate(()=>evtFs(0));
   let state=await page.evaluate(()=>({scroll:document.documentElement.scrollWidth,admin:getComputedStyle(document.getElementById('adminCard')).display}));
   assert(state.scroll<=width,'public overflow at '+width);assert.equal(state.admin,'none');
   // Layout only: authentication and exact mutation routes have separate native-click tests.
   await page.evaluate(()=>{admTokenSet('a'.repeat(64));admUnlock('admin');document.querySelectorAll('#adminCard details').forEach(e=>e.open=true);});
   await page.evaluate(()=>evtFs(0));
   state=await page.evaluate(()=>({scroll:document.documentElement.scrollWidth,buttons:[...document.querySelectorAll('#adminCard button')].map(b=>({h:b.getBoundingClientRect().height,w:b.getBoundingClientRect().width}))}));
   assert(state.scroll<=width,'admin overflow at '+width);
   assert(state.buttons.every(b=>b.h>=38&&b.w>=38),'undersized action at '+width);
   if(process.env.SCREENSHOT_DIR)await page.screenshot({path:path.join(process.env.SCREENSHOT_DIR,'settings-'+width+'.png'),fullPage:true});
   console.log('PASS: public/admin settings at '+width+'px; no horizontal overflow; usable targets');
   await page.evaluate(()=>admLock());
  }
  assert.deepEqual(errors,[]);console.log('PASS: no browser script errors');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
