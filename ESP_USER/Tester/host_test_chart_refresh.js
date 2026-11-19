'use strict';
// Reproduce hidden-canvas, refresh, resize, request races and history invalidation.
const fs=require('fs'),path=require('path'),assert=require('assert/strict');
const {JSDOM}=require('jsdom');
const source=fs.readFileSync(path.join(__dirname,'..','simple_panel.h'),'utf8');
const html=source.split('R"rawliteral(')[1].split(')rawliteral"')[0];
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
const tick=()=>new Promise(r=>setImmediate(r));
const reply=(body,status=200)=>({ok:status===200,status,json:async()=>body,text:async()=>JSON.stringify(body)});
const sample=(range='1d')=>({range,vin:[24000,24500,25000],vpack:[25200,25300,25400],time:[1790000000,1790000060,1790000120]});
let checks=0;
function check(value,message){assert.ok(value,message);checks++;console.log('PASS: '+message);}
function panel(){
 const dom=new JSDOM(html,{runScripts:'outside-only',pretendToBeVisual:true,url:'http://panel.test'}),w=dom.window,d=w.document;
 w.addEventListener=()=>{};w.setInterval=()=>0;w.fetch=async()=>{throw Error('offline');};
 let width=0,height=180,strokes=0,context=true;
 const ctx=new Proxy({}, {get:(o,k)=>k in o?o[k]:k==='stroke'?()=>strokes++:()=>{},set:(o,k,v)=>(o[k]=v,true)});
 for(const id of ['cVolt','cCharge']){
  const c=d.getElementById(id);Object.defineProperty(c,'clientWidth',{get:()=>width});Object.defineProperty(c,'clientHeight',{get:()=>height});c.getContext=()=>context?ctx:null;
 }
 w.eval(script+'\nwindow.chartViewsForTest=chartViews;window.voltYForTest=()=>chartVoltY;');
 return {w,d,dom,setSize:(v,h=180)=>{width=v;height=h;},setContext:v=>context=v,strokes:()=>strokes};
}
(async()=>{
 const p=panel(),{w,d}=p;await tick();
 w.renderVoltFromServer(sample());
 check(!d.getElementById('cVolt')._drawn && p.strokes()===0,'hidden charts are cached, never marked rendered');
 check(d.getElementById('voltCountInfo').textContent==='۳ نمونه' && d.getElementById('voltMinMax').textContent.includes('24.0'),'sample count and numeric summary update even while canvas is hidden');
 p.setSize(600);d.getElementById('pg1').classList.add('a');
 p.setContext(false);w.redrawCharts();check(!d.getElementById('cVolt')._drawn,'unavailable context cannot poison the render cache');
 p.setContext(true);w.redrawCharts();check(p.strokes()>0 && d.getElementById('cVolt')._drawn,'identical data renders once the canvas becomes available');
 let strokes=p.strokes();w.renderVoltFromServer(sample());check(p.strokes()===strokes,'unchanged voltage data does not repaint');
 p.setSize(400);w.renderVoltFromServer(sample());check(p.strokes()>strokes && d.getElementById('cVolt').width===400,'size change repaints identical data at the real CSS width');
 Object.defineProperty(w,'devicePixelRatio',{value:2,configurable:true});w.redrawCharts();check(d.getElementById('cVolt').width===800,'high-DPI backing size matches CSS pixels times pixel ratio');
 d.getElementById('pg1').classList.remove('a');p.setSize(0);w.redrawCharts();w.redrawCharts();check(d.getElementById('cVolt').width===800,'hidden redraws never multiply the canvas backing dimensions');
 p.setSize(400);d.getElementById('pg1').classList.add('a');w.redrawCharts();check(d.getElementById('cVolt').width===800,'revealing a high-DPI chart preserves the correct backing size');
 strokes=p.strokes();const changed=sample();changed.vin[1]=24600;w.renderVoltFromServer(changed);check(p.strokes()>strokes,'interior sample changes repaint even if last value, min and max are unchanged');
 let requests=0;w.fetch=async()=>{requests++;return reply(sample());};await w.redrawCharts();check(requests===0,'font and resize redraws use cached data with zero network requests');
 const events=[{ts:Math.floor(Date.now()/1000),ch:1}];w.drawChargeHistory(events,'1d');strokes=p.strokes();w.drawChargeHistory(events,'1d');check(p.strokes()===strokes,'unchanged charge history does not repaint on every diagnostic poll');
 w.drawChargeHistory([],'1d');check(!d.getElementById('cChargeLabels').textContent,'empty charge history clears old time labels');
 w.renderVoltFromServer({range:'1d',vin:[],vpack:[],time:[]});check(d.getElementById('voltCountInfo').textContent==='۰ نمونه' && !d.getElementById('voltMinMax').textContent,'empty voltage history clears old min/max and displays a real zero count');
 let pending=[];w.fetch=(url,options)=>new Promise(resolve=>pending.push({url,options,resolve}));
 const first=w.pollChartsVolt(),same=w.pollChartsVolt();check(first===same && pending.length===1,'simultaneous same-range polls share one in-flight request');
 w.setChartRange('volt','7d');const next=w.pollChartsVolt();check(pending.length===2 && pending[0].options.signal.aborted,'changing range cancels the obsolete request and starts the newest one');
 pending[1].resolve(reply(sample('7d')));await next;const current=d.getElementById('voltRangeInfo').textContent;
 pending[0].resolve(reply(sample('1d')));await first;check(d.getElementById('voltRangeInfo').textContent===current && current.includes('۷ روزه'),'late old-range responses cannot overwrite the selected range');
 w.fetch=async()=>reply({},503);await w.pollChartsVolt();check(d.getElementById('voltMinMax').textContent.includes('24.0') && d.getElementById('voltRangeInfo').textContent.includes('دادهٔ قبلی حفظ شد'),'request failures preserve existing values and report retry status');
 w.fetch=async()=>reply({range:'7d',vin:[],vpack:[1],time:[]});await w.pollChartsVolt();check(d.getElementById('voltCountInfo').textContent==='۳ نمونه','malformed responses do not replace good chart data');
 let oldDiag;w.fetch=()=>new Promise(resolve=>oldDiag=resolve);const diag=w.pollDiag();w.resetHistoryView();oldDiag(reply({valid:true,errors:[{ts:1790000000,fault:128}],chargeHistory:events}));await diag;
 check(d.getElementById('diagErrors').textContent.includes('خطایی ثبت نشده') && !d.getElementById('chargeHistoryList').querySelector('.row'),'diagnostic replies started before format cannot resurrect cleared history');
 let oldChart;w.fetch=()=>new Promise(resolve=>oldChart=resolve);const chart=w.pollChartsVolt();w.resetHistoryView();oldChart(reply(sample('7d')));await chart;
 check(d.getElementById('voltCountInfo').textContent==='۰ نمونه' && !d.getElementById('voltMinMax').textContent,'chart replies started before format cannot resurrect cleared history');
 d.getElementById('pg1').classList.remove('a');let hiddenRequests=0;w.fetch=async()=>{hiddenRequests++;return reply(sample());};await w.pollCharts();check(hiddenRequests===0,'periodic chart polling is paused outside the chart tab');
 // Exercise native input events against both real chart viewports.
 w.fetch=async()=>reply(sample());w.setChartRange('volt','1d');await w.pollChartsVolt();
 const dense={range:'1d',vin:[],vpack:[],time:[]};
 const now=Math.floor(Date.now()/1000);
 for(let i=0;i<101;i++){dense.vin.push(24000+i);dense.vpack.push(25000+i);dense.time.push(now-6000+i*60);}
 w.renderVoltFromServer(dense);w.drawChargeHistory(dense.time.map((ts,i)=>({ts,ch:i%2+1})),'1d');
 d.getElementById('pg1').classList.add('a');p.setSize(600);
 function pointer(c,type,id,x,y=40){const e=new w.Event(type,{bubbles:true,cancelable:true});Object.assign(e,{pointerId:id,clientX:x,clientY:y,pointerType:'touch',button:0});c.dispatchEvent(e);}
 for(const [kind,id] of [['volt','cVolt'],['charge','cCharge']]){
  const c=d.getElementById(id);c.getBoundingClientRect=()=>({left:0,top:0,width:600,height:180});
  const wheel=new w.WheelEvent('wheel',{deltaY:-600,clientX:300,cancelable:true});c.dispatchEvent(wheel);
  check(!wheel.defaultPrevented && w.eval('chartViewsForTest.'+kind+'.end-chartViewsForTest.'+kind+'.start')===1,'ordinary wheel scroll remains untouched: '+kind);
  const zoom=new w.WheelEvent('wheel',{deltaY:-600,ctrlKey:true,clientX:300,cancelable:true});c.dispatchEvent(zoom);
  if(kind==='volt')check(w.eval('voltYForTest().span')<.4,'Ctrl-wheel narrows the voltage axis as well as time');
  const span=w.eval('chartViewsForTest.'+kind+'.end-chartViewsForTest.'+kind+'.start');
  check(zoom.defaultPrevented && span<0.4 && span>0,'Ctrl-wheel zooms the existing data: '+kind);
  const start=w.eval('chartViewsForTest.'+kind+'.start');
  pointer(c,'pointerdown',1,300);pointer(c,'pointermove',1,420);pointer(c,'pointerup',1,420);
  check(w.eval('chartViewsForTest.'+kind+'.start')<start,'horizontal drag pans the zoomed view: '+kind);
  pointer(c,'pointerdown',1,200);pointer(c,'pointerdown',2,400);pointer(c,'pointermove',2,500);
  check(w.eval('chartViewsForTest.'+kind+'.end-chartViewsForTest.'+kind+'.start')<span,'two-finger pinch zooms: '+kind);
  pointer(c,'pointercancel',1,200);pointer(c,'pointerup',2,500);
  const state=w.eval('JSON.stringify(chartViewsForTest.'+kind+')');
  w.redrawCharts();w.renderVoltFromServer(dense);w.drawChargeHistory(w._lastChargeHistory,'1d');
  check(w.eval('JSON.stringify(chartViewsForTest.'+kind+')')===state,'resize and polling preserve zoom: '+kind);
  for(let i=0;i<10;i++)c.dispatchEvent(new w.WheelEvent('wheel',{deltaY:10000,ctrlKey:true,clientX:300,cancelable:true}));
  check(w.eval('chartViewsForTest.'+kind+'.start===0 && chartViewsForTest.'+kind+'.end===1'),'zoom out clamps at the complete range: '+kind);
  w.chartViewSet(kind,0.2,0.3);w.eval('(function(){'+d.getElementById(kind+'Full').getAttribute('onclick')+'})')();
  check(w.eval('chartViewsForTest.'+kind+'.start===0 && chartViewsForTest.'+kind+'.end===1'),'full-view button restores all data: '+kind);
  w.chartViewSet(kind,0.2,0.3);w.setChartRange(kind,'7d');await tick();
  check(w.eval('chartViewsForTest.'+kind+'.start===0 && chartViewsForTest.'+kind+'.end===1'),'range switch resets only its viewport: '+kind);
 }
 w.setChartRange('volt','1d');await w.pollChartsVolt();w.renderVoltFromServer(dense);w.chartViewSet('volt',0.4,0.2);
 const vc=d.getElementById('cVolt');check(vc._chartData.length<101 && vc._chartTime.length===vc._chartData.length && vc._rawVolt.vin.length===101,'voltage zoom slices display data without changing original samples');
 const fullScale={...vc._voltScale};
 w.chartViewSet('volt',.4,.2,.45,.1);
 check(vc._voltScale.max-vc._voltScale.min<(fullScale.max-fullScale.min)*.11,'vertical zoom actually narrows rendered voltage limits');
 check(new Set(vc._voltScale.ticks.map(w.fmtVolt)).size===vc._voltScale.ticks.length,'fine voltage ticks have distinct readable labels');
 const beforeY=w.eval('voltYForTest().start');pointer(vc,'pointerdown',7,300,80);pointer(vc,'pointermove',7,300,110);pointer(vc,'pointerup',7,300,110);
 check(w.eval('voltYForTest().start')>beforeY,'vertical dragging moves the voltage viewport');
 const scaleState=JSON.stringify(vc._voltScale);w.redrawCharts();w.renderVoltFromServer(dense);
 check(JSON.stringify(vc._voltScale)===scaleState,'vertical scale survives polling and redraw');
 w.chartFull('volt');check(w.eval('voltYForTest().start===0&&voltYForTest().span===1'),'full-view resets the vertical axis');
 const fullButton=d.getElementById('voltFull');
 check(fullButton.nextElementSibling.textContent.includes('Vin')&&fullButton.nextElementSibling.nextElementSibling.textContent.includes('Vpack'),'voltage legends sit beside the full-view icon');
 check(d.getElementById('segVolt').closest('.volt-ranges').parentElement===fullButton.closest('.volt-tools').parentElement,'range buttons and legends share the toolbar directly above the plot');
 check([...d.querySelectorAll('#segVolt button')].every(b=>!b.querySelector('br')&&/^.[ ](?:روز|ماه)$/.test(b.textContent)),'range number and unit remain adjacent, without a second row');
 check(d.getElementById('voltMinMax').tagName==='SPAN'&&!source.includes('#voltMinMax{')&&!d.getElementById('voltAxisInfo'),'voltage min/max returns to plain compact text without a bold card or axis description');
 check(fullButton.closest('.volt-tools')&&fullButton.closest('.chart-card')===vc.closest('.chart-card')&&fullButton.querySelector('svg path')&&!fullButton.textContent.trim()&&fullButton.getAttribute('aria-label')==='نمایش کامل','full view is an accessible line icon inside the chart wrapper');
 w.chartViewSet('volt',.4,.2,.2,.05);w.setChartRange('volt','1d');
 check(w.eval('voltYForTest().start===0&&voltYForTest().span===1'),'range changes reset vertical zoom');
 w.renderVoltFromServer(dense);w.chartViewSet('volt',.4,.2);
 vc.dispatchEvent(new w.MouseEvent('mousemove',{clientX:55,clientY:60}));
 check(d.getElementById('chartTooltip').innerHTML.includes((vc._chartData[0]/1000).toFixed(2)),'tooltip uses the visible zoomed samples');
 w.resetHistoryView();w.chartViewSet('volt',0.4,0.01);w.chartViewSet('charge',0.4,0.01);
 check(!vc._chartData.length && !d.getElementById('cChargeLabels').textContent,'zoom is safe on empty histories');
 w.renderVoltFromServer({range:'1d',vin:[24000],vpack:[25000],time:[now]});w.drawChargeHistory([{ts:now,ch:1}],'1d');
 check(vc._chartData.length===1,'single-sample histories are safe at maximum zoom');
 p.dom.window.close();
 const fresh=panel();await tick();fresh.w.fetch=async()=>reply(sample());await fresh.w.pollChartsVolt();check(fresh.d.getElementById('voltCountInfo').textContent==='۳ نمونه','a fresh browser page restores counts from the ESP response');
 fresh.setSize(550);fresh.w.showTab(1);await tick();check(fresh.strokes()>0 && fresh.d.getElementById('cVoltLabels').textContent.length>0 && !fresh.d.getElementById('voltAxisInfo'),'opening charts after refresh restores axes, time labels and lines');
 fresh.dom.window.close();console.log(`ALL ${checks} CHART REFRESH CHECKS PASSED`);
})().catch(e=>{console.error(e);process.exit(1);});
