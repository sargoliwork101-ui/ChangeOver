/* [EN] Exercise real sample-row events, stable nodes, deletion and transaction guards. */
'use strict';
const fs=require('fs'),path=require('path'),assert=require('assert');
let JSDOM;try{({JSDOM}=require('jsdom'));}catch(e){console.log('SKIP: jsdom not installed; sample-row tests did not run.');process.exit(0);}
let checks=0;
const check=(v,msg)=>{assert.ok(v,msg);checks++;console.log('  ok   '+msg);};
const dom=new JSDOM(fs.readFileSync(path.join(__dirname,'../panel_preview.html'),'utf8'),{runScripts:'dangerously',pretendToBeVisual:true,url:'http://localhost/',beforeParse(w){w.setInterval=()=>0;w.scrollTo=()=>{};}});
const w=dom.window,d=w.document;
(async()=>{try{
 await new Promise(r=>setTimeout(r,80));
 w.eval('W={run:false,opening:false,K:[]};CALS=[];');w.D={on:1,p:Array(171).fill(0),t:Array(30).fill(0)};w.D.p[2]=w.D.p[3]=1000;
 for(let i=0;i<8;i++)w.CALS.push({sc:'BOTH',d:i,use:1,r1:80+i*20,r2:90+i*20,b1:i*30,b2:i*31,ts:100+i});
 w.calsmp();w.calrun();
 const rows=()=>[...d.querySelectorAll('#calsl [data-csample]')],original=rows(),table=d.querySelector('#calsl table'),sample=w.CALS[3],order=w.CALS.map(z=>z.ts).join(',');
 original[3].children[4].click();
 check(sample.use===0&&!original[3].querySelector('input').checked,'click anywhere on row toggles its checkbox off');
 check(rows().every((r,i)=>r===original[i])&&d.querySelector('#calsl table')===table,'selection keeps every row and the table DOM node');
 original[3].children[8].click();check(sample.use===1,'clicking warning cell toggles the same sample back on');
 original[3].querySelector('input').click();check(sample.use===0,'direct checkbox click toggles exactly once');
 const box=original[3].querySelector('input');box.focus();box.click();check(sample.use===1&&d.activeElement===box,'checkbox focus survives recalculation');
 check(w.CALS.map(z=>z.ts).join(',')===order,'selection never sorts or moves source samples');
 d.getElementById('calall').click();check(w.CALS.every(z=>z.use===0)&&rows().every((r,i)=>r===original[i]),'select-none patches all existing rows in place');
 d.getElementById('calall').click();check(w.CALS.every(z=>z.use===1),'select-all restores every sample');
 w.eval('LUT_PENDING={p:{},target:0}');d.getElementById('lutcmp').textContent='old payload';w.calcode();
 const bench=w.eval('W');bench.K=[{sample:3,savedSample:sample},{sample:6,savedSample:w.CALS[6]}];
 original[3].querySelector('button svg').dispatchEvent(new w.MouseEvent('click',{bubbles:true}));
 check(w.CALS.length===7&&!w.CALS.includes(sample),'delete icon removes just the chosen source sample');
 check(!bench.K[0].savedSample&&bench.K[1].sample===5,'capture repeat references cannot target a shifted sample');
 check(rows().every((r,i)=>r===original.filter((_,j)=>j!==3)[i]),'deletion retains all surviving row nodes and order');
 check(w.eval('LUT_PENDING===null')&&!d.getElementById('calcd').value&&!d.getElementById('lutcmp').textContent,'deletion invalidates stale code and prepared transmission');
 check(JSON.parse(w.localStorage.getItem('calsmp')).length===7&&d.getElementById('caln').textContent==='7','deletion persists source samples and updates sample count');
 const shifted=w.CALS[3];rows()[3].querySelector('input').click();check(shifted.use===0,'checkbox handler uses the updated index after deletion');
 rows()[3].querySelector('button').click();check(!w.CALS.includes(shifted)&&w.CALS.length===6,'delete handler uses the updated index and does not toggle a neighbor');
 for(const flag of ['CAL_BUSY','W.run','W.opening']){
  const before=JSON.stringify(w.CALS);w.eval(flag+'=true');rows()[0].children[2].click();rows()[0].querySelector('input').click();rows()[0].querySelector('button').click();d.getElementById('calall').click();
  check(JSON.stringify(w.CALS)===before&&rows()[0].querySelector('input').checked===(w.CALS[0].use!==0),flag+' blocks edits/deletion and restores checkbox state');w.eval(flag+'=false');
 }
 rows()[0].querySelector('button').click();check(w.calbuild(1).bad.includes('صفر'),'zero sample can be deleted but its missing-zero table is invalid');
 const pack=w.lpack(0,{ready:true,T:[{X:[],Y:[]},{X:[],Y:[]}]});check(!!pack.bad,'missing zero blocks transmission payload creation');
 while(w.CALS.length)rows()[0].querySelector('button').click();
 check(!rows().length&&w.CALS.length===0&&w.eval('CALR.length===0'),'last sample can be deleted and no stale calibration result remains');
 w.caldelete(-1);w.caldelete(NaN);check(w.CALS.length===0,'invalid delete identities do nothing');
 console.log('ALL '+checks+' SAMPLE ROW TESTS PASSED');
 }finally{dom.window.close();}})().catch(e=>{console.error(e);process.exitCode=1;});
