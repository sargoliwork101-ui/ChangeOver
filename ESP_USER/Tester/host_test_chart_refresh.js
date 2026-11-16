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
 w.eval(script);
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
 p.dom.window.close();
 const fresh=panel();await tick();fresh.w.fetch=async()=>reply(sample());await fresh.w.pollChartsVolt();check(fresh.d.getElementById('voltCountInfo').textContent==='۳ نمونه','a fresh browser page restores counts from the ESP response');
 fresh.setSize(550);fresh.w.showTab(1);await tick();check(fresh.strokes()>0 && fresh.d.getElementById('cVoltLabels').textContent.length>0 && fresh.d.getElementById('voltAxisInfo').textContent.includes('V'),'opening charts after refresh restores axes, time labels and lines');
 fresh.dom.window.close();console.log(`ALL ${checks} CHART REFRESH CHECKS PASSED`);
})().catch(e=>{console.error(e);process.exit(1);});
