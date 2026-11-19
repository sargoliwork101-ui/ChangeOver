/**
 * @file about_footer.h
 * @brief Muted company footer renderer shared by ESP and offline previews.
 */
#pragma once
static const char SIMPLE_ABOUT_FOOTER_JS[] PROGMEM = R"footer(
(function () {
 'use strict';
 const icons = {
  phone:'M5 3h4l2 5-3 2a14 14 0 0 0 6 6l2-3 5 2v4a2 2 0 0 1-2 2C10 21 3 14 3 5a2 2 0 0 1 2-2Z',
  website:'M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0ZM3 12h18M12 3c4 5 4 13 0 18-4-5-4-13 0-18Z',
  address:'M20 10c0 6-8 11-8 11S4 16 4 10a8 8 0 1 1 16 0ZM15 10a3 3 0 1 1-6 0 3 3 0 0 1 6 0Z',
  email:'M3 5h18v14H3ZM3 6l9 7 9-7',
  whatsapp:'M21 11.5a9 9 0 0 1-13 8L3 21l1.5-5A9 9 0 1 1 21 11.5ZM8 8c0 4 4 8 8 8',
  hours:'M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0ZM12 7v5l3 2',
  social:'M10 13a5 5 0 0 0 7 0l3-3a5 5 0 0 0-7-7l-2 2M14 11a5 5 0 0 0-7 0l-3 3a5 5 0 0 0 7 7l2-2',
  copy:'M8 8h13v13H8ZM16 8V3H3v13h5'
 };
 // Build only local line icons; never load third-party imagery or fonts.
 function icon(key) {
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  for (const [name,value] of Object.entries({viewBox:'0 0 24 24',fill:'none',stroke:'currentColor','stroke-width':'1.5','stroke-linecap':'round','stroke-linejoin':'round','aria-hidden':'true'})) svg.setAttribute(name,value);
  const path = document.createElementNS(svg.namespaceURI, 'path');path.setAttribute('d',icons[key]);svg.appendChild(path);
  return svg;
 }
 // Treat missing, non-string, and whitespace-only configuration values as absent.
 function text(data,key) {return typeof data[key] === 'string' ? data[key].trim() : '';}
 // Accept ordinary web links only; malformed or executable schemes stay plain text.
 function webLink(value) {
  if (/[\u0000-\u0020\u007f]/.test(value)) return '';
  try {
   const url = new URL(/^[a-z][a-z0-9+.-]*:/i.test(value) ? value : 'https://' + value);
   return ['http:','https:'].includes(url.protocol) && url.hostname && !url.username && !url.password ? url.href : '';
  } catch (e) {return '';}
 }
 // Normalize Persian/Arabic digits and common telephone punctuation, not arbitrary text.
 function phoneNumber(value) {
  const latin=value.replace(/[۰-۹٠-٩]/g,ch=>String(ch.charCodeAt(0)-(ch.charCodeAt(0)>=1776?1776:1632)));
  if (!/^[+\d\s()-]+$/.test(latin)) return '';
  const number=latin.replace(/[\s()-]/g,'');
  return /^\+?\d{5,15}$/.test(number) ? number : '';
 }
 // Try the secure clipboard first, then the user-gesture fallback used on ESP HTTP.
 async function copyPhone(value,status) {
  let copied=false;
  if (navigator.clipboard && navigator.clipboard.writeText) {
   try {await navigator.clipboard.writeText(value);copied=true;} catch(e) {}
  }
  if (!copied) {
   const focused=document.activeElement,selection=window.getSelection(),ranges=[];
   if(selection)for(let i=0;i<selection.rangeCount;i++)ranges.push(selection.getRangeAt(i).cloneRange());
   const area=document.createElement('textarea');area.value=value;area.readOnly=true;area.dir='ltr';
   area.style.cssText='position:fixed;left:-9999px;top:0;opacity:0;font-size:16px';
   area.setAttribute('aria-label','شماره تماس برای کپی');document.body.appendChild(area);
   try {area.select();copied=typeof document.execCommand==='function' && document.execCommand('copy');} catch(e) {}
   finally {
    area.remove();if(focused && focused.focus)focused.focus({preventScroll:true});
    if(selection){selection.removeAllRanges();ranges.forEach(range=>selection.addRange(range));}
   }
  }
  status.textContent=copied ? 'شماره کپی شد.' : 'کپی انجام نشد؛ شماره را نگه دارید و دستی کپی کنید.';
 }
 // Render values with textContent so configuration can never become active markup.
 function render(data) {
  const body=document.getElementById('aboutBody');if(!body)return;
  const fragment=document.createDocumentFragment();
  for (const [key,tag] of [['companyName','h2'],['description','p']]) {
   const value=text(data,key);if(!value)continue;
   const el=document.createElement(tag);el.dataset.about=key;el.textContent=value;el.className='company-'+key;fragment.appendChild(el);
  }
  const contacts=document.createElement('div');contacts.className='company-contacts';
  const status=document.createElement('p');status.id='aboutStatus';status.className='company-status';status.setAttribute('role','status');status.setAttribute('aria-live','polite');
  const labels={phone:'تماس با ما',website:'وب‌سایت',address:'دفتر نمایندگی',email:'ایمیل',whatsapp:'واتس‌اپ',hours:'ساعات پاسخ‌گویی',social:'شبکهٔ اجتماعی'};
  for(const [key,label] of Object.entries(labels)) {
   const value=text(data,key);if(!value)continue;
   let href='';const number=key==='phone'||key==='whatsapp'?phoneNumber(value):'';
   if(key==='phone'&&number)href='tel:'+number;
   if(key==='website'||key==='social')href=webLink(value);
   if(key==='email'&&/^[^\s@<>]+@[^\s@<>]+\.[^\s@<>]+$/.test(value)&&!/[\u0000-\u001f\u007f]/.test(value))href='mailto:'+value;
   if(key==='whatsapp'&&number){const digits=number.replace(/^\+|^00/,'');if(/^[1-9]\d{7,14}$/.test(digits))href='https://wa.me/'+digits;}
   const row=document.createElement('div');row.className='company-contact';row.dataset.about=key;
   row.appendChild(icon(key));
   const title=document.createElement('span');title.className='company-label';title.textContent=label+':';row.appendChild(title);
   const content=document.createElement(href?'a':'span');
   if(href){content.href=href;if(/^https?:/.test(href)){content.target='_blank';content.rel='noopener noreferrer';}}
   const valueNode=document.createElement('bdi');valueNode.dir=['address','hours'].includes(key)?'auto':'ltr';valueNode.textContent=value;content.appendChild(valueNode);row.appendChild(content);
   if(key==='phone') {
    const copy=document.createElement('button');copy.type='button';copy.className='company-copy';copy.setAttribute('aria-label','کپی شماره تماس');copy.title='کپی شماره تماس';
    copy.append(icon('copy'),document.createTextNode('کپی'));
    copy.addEventListener('click',()=>copyPhone(number||value,status));row.appendChild(copy);
   }
   contacts.appendChild(row);
  }
  if(contacts.childElementCount)fragment.appendChild(contacts);
  fragment.appendChild(status);body.replaceChildren(fragment);
 }
 let busy=false,loaded=false;
 // Fetch the compiled values once, with a bounded wait and an explicit retry on failure.
 async function load() {
  const body=document.getElementById('aboutBody');if(!body||busy||loaded)return;
  busy=true;const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),8000);
  try {
   const response=await fetch('/about',{cache:'no-store',signal:controller.signal});
   if(!response.ok)throw Error('about');
   const data=await response.json();if(!data||Array.isArray(data)||typeof data!=='object')throw Error('about');
   render(data);loaded=true;
  } catch(e) {
   const retry=document.createElement('button');retry.type='button';retry.className='company-retry';retry.textContent='دریافت اطلاعات تماس؛ تلاش دوباره';retry.addEventListener('click',load);body.replaceChildren(retry);
  } finally {clearTimeout(timer);busy=false;}
 }
 // Install a single shared footer, leaving the version/admin entry untouched.
 function install() {
  if(!document.getElementById('about')||document.getElementById('companyFooterStyle'))return;
  const style=document.createElement('style');style.id='companyFooterStyle';
  style.textContent=`
   .company-footer{margin-top:14px;padding:26px 8px 8px;border-top:1px solid var(--ln);color:var(--mu);text-align:center;font-size:12px;line-height:1.9}
   .company-footer .footer{justify-content:center;border:0;margin-top:16px;padding:4px;opacity:.65}
   .company-companyName{font-size:15px;font-weight:700;color:var(--mu2);margin:0 0 8px}
   .company-description{max-width:620px;margin:0 auto 17px;white-space:pre-line;overflow-wrap:anywhere}
   .company-contacts{display:flex;justify-content:center;align-items:center;gap:10px 24px;flex-wrap:wrap;max-width:850px;margin:auto}
   .company-contact{display:flex;justify-content:center;align-items:center;gap:7px;flex-wrap:wrap;min-width:0;max-width:100%;overflow-wrap:anywhere}
   .company-contact svg{width:16px;height:16px;flex:none;opacity:.75}
   .company-label{font-size:11px;opacity:.85}
   .company-contact a,.company-contact bdi{color:inherit;max-width:100%;overflow-wrap:anywhere;user-select:text}
   .company-contact a{text-decoration:none;text-underline-offset:4px;min-width:0}
   .company-contact a:hover{color:var(--tx);text-decoration:underline}
   .company-copy,.company-retry{font:inherit;color:inherit;background:none;border:0;cursor:pointer;display:inline-flex;align-items:center;justify-content:center;gap:4px;min-height:32px;padding:3px 7px;border-radius:6px}
   .company-copy{font-size:10px}.company-copy svg{width:13px;height:13px}
   .company-copy:hover,.company-retry:hover{background:var(--cd2);color:var(--tx)}
   .company-footer :focus-visible{outline:2px solid var(--ac);outline-offset:3px;border-radius:4px}
   .company-status{font-size:11px;margin:8px 0 0}.company-status:empty{display:none}
   @media(max-width:520px){.company-contacts{flex-direction:column;gap:10px}.company-footer{padding:22px 4px 6px}}
  `;
  document.head.appendChild(style);window.loadAbout=load;window.renderCompanyFooter=render;load();
 }
 if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',install);else install();
})();
)footer";
