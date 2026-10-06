/* plink_panel.h - the web panel HTML in PROGMEM (single R"HTML" raw string).
   Included by esp_link_panel.ino (single translation unit, order matters).
   No include guard on purpose: including twice would redefine everything. */
/* ==================== Web Panel (PROGMEM) ==================== */
static const char ESP_PANEL_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="fa" dir="rtl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>ChangeOver</title><link rel="stylesheet" href="/f.css?v=2"><style>
/* [EN] v1.25 theme (user order 2026-09-29 "the colouring is not good").
   The text colours were already fine - measured, they all passed AA. The
   real fault was that the SURFACES were indistinguishable: card against
   page was 1.07 and the borders 1.22, so every panel melted into one flat
   dark sheet with no depth. Rebuilt as a proper elevation ladder and
   checked against GitHub dark as a reference; this beats it on every
   surface step (card/page 1.14 vs 1.09, raised/card 1.22 vs 1.14,
   border/card 1.75 vs 1.42) while every text pair stays above AA 4.5.
   [FA] تم نسخهٔ ۱.۲۵ (دستور کاربر: «رنگ‌بندی خوب نیست»). رنگ متن‌ها از اول
   مشکلی نداشتند و همه AA را رد می‌کردند؛ ایراد واقعی این بود که سطح‌ها از هم
   تشخیص داده نمی‌شدند - کارت در برابر پس‌زمینه ۱.۰۷ و خط‌ها ۱.۲۲، پس همه‌چیز
   یک ورق تخت تیره می‌شد بدون عمق. به‌صورت نردبان ارتفاع بازسازی شد. */
:root{--bg:#0b0f18;--cd:#161d2b;--in:#080b12;--rs:#232d40;--ln:#38455e;--tx:#e9eef6;--mu:#96a1b8;--ac:#63a2ff;--ac2:#9ac8ff;--ok:#35d6a0;--wa:#f7c13c;--er:#ff6873;--ring:rgba(99,162,255,.38);--sh:0 10px 30px rgba(0,0,0,.45)}
*{box-sizing:border-box;margin:0}
::selection{background:rgba(99,162,255,.38)}
body{background:radial-gradient(1200px 300px at 50% -80px,rgba(99,162,255,.09),transparent),var(--bg);color:var(--tx);font:clamp(13px,.28vw + 12.1px,15.3px)/1.65 Vazirmatn,Tahoma,sans-serif;max-width:1480px;margin:auto;padding:0 14px 28px;scrollbar-color:#3a4767 transparent}
button,input,select,textarea{font:inherit;color:inherit}
button{cursor:pointer}
:focus-visible{outline:2px solid var(--ac);outline-offset:2px;border-radius:8px}
.n{direction:ltr;unicode-bidi:isolate;font-variant-numeric:tabular-nums}
header{position:sticky;top:0;z-index:60;display:flex;align-items:center;justify-content:space-between;gap:10px;padding:12px 2px;margin:0 -14px 12px;padding-left:14px;padding-right:14px;background:rgba(9,12,18,.82);backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px);border-bottom:1px solid var(--ln)}
h1{font-size:17px;font-weight:800;letter-spacing:-.2px;display:flex;align-items:center;gap:9px}
h1::before{content:"";width:11px;height:22px;border-radius:6px;background:linear-gradient(180deg,var(--ac2),var(--ac));box-shadow:0 0 12px rgba(91,157,255,.55)}
.lk{display:flex;align-items:center;gap:8px;font-size:12px;color:var(--mu);background:var(--cd);border:1px solid var(--ln);border-radius:999px;padding:5px 12px 5px 8px}
.lk i{width:9px;height:9px;border-radius:50%;background:var(--er);box-shadow:0 0 8px var(--er)}.lk.on i{background:var(--ok);box-shadow:0 0 8px var(--ok)}
nav{display:flex;gap:4px;background:var(--cd);border:1px solid var(--ln);border-radius:14px;padding:5px;position:sticky;top:var(--t-nav,55px);z-index:55;margin-bottom:14px;box-shadow:var(--sh)}
nav button{flex:1;border:0;background:none;border-radius:10px;padding:9px 8px;color:var(--mu);font-weight:600;transition:background .15s,color .15s}
nav button:hover{color:var(--tx);background:rgba(255,255,255,.04)}
nav button.a{background:linear-gradient(180deg,#24406e,#1b3358);color:#fff;box-shadow:inset 0 1px 0 rgba(255,255,255,.12),0 2px 8px rgba(0,0,0,.4)}
nav button.m.a{background:linear-gradient(180deg,#6e4a10,#543806);color:#ffe1a8}
section{margin-top:12px}
.cd{background:linear-gradient(180deg,rgba(255,255,255,.025),transparent 30%),var(--cd);border:1px solid var(--ln);border-radius:16px;padding:16px;margin-bottom:14px;box-shadow:var(--sh)}
.ti{font-size:12.5px;font-weight:700;color:var(--mu);margin-bottom:10px;text-transform:uppercase;letter-spacing:.3px}
.lb{color:var(--mu);font-size:.86em}
.bs{color:var(--mu);font-size:11px;font-variant-numeric:tabular-nums;letter-spacing:.5px;opacity:.75;margin-inline-start:auto;padding-inline-end:10px;direction:ltr}
/* [EN] v1.29 click-to-edit (user order): the value IS the control. A dotted
   underline is the only affordance - a number that looks like a button stops
   reading like a number, and this table's whole job is to be read.
   [FA] ویرایش با کلیک (دستور کاربر): خودِ عدد همان کنترل است. تنها نشانه،
   زیرخط نقطه‌چین است - عددی که شبیه دکمه باشد دیگر مثل عدد خوانده نمی‌شود و
   کار اصلی این جدول خوانده‌شدن است. */
.ev{color:var(--ac);border-bottom:1px dashed var(--ac);cursor:pointer;white-space:nowrap}
.ev:hover{color:var(--ac2);border-bottom-color:var(--ac2)}
.ev.ro{color:var(--tx);border:0;cursor:default}
.ev.cl{color:var(--wa);border-bottom-color:var(--wa)}
.evi{width:12ch;background:var(--in);color:var(--tx);border:1px solid var(--ac);border-radius:5px;
     padding:4px 7px;font:inherit;font-size:13px;text-align:left}
.evu{color:var(--mu);font-size:11px}
/* [EN] v1.30 (user order): the CHART is the editor and the table only reports.
   An SVG <text> cannot host an <input>, so the editor is a small floating
   panel anchored to whatever was clicked - one mechanism for the plot labels
   and for the chips under it, instead of two that can disagree.
   [FA] نمودار ویرایشگر است و جدول فقط گزارش می‌دهد. متن SVG نمی‌تواند input
   داشته باشد، پس ویرایشگر یک پنل کوچک Float است که به هر چیزی که کلیک شده
   لنگر می‌اندازد - یک ساز و کار برای برچسب‌های نمودار و تراشه‌های زیرش، نه دو
   تا که بتوانند با هم اختلاف پیدا کنند. */
.evs{cursor:pointer;text-decoration:underline dotted}
/* [EN] Chips under the chart. The times and gains were drawn ON the plane
   until the user said the picture had got too crowded; they are neither a
   voltage nor a current, so they never had an honest position on an I-V
   plane anyway. Same data-i, same editor - moved, not demoted.
   [FA] تراشه‌های زیر نمودار. زمان‌ها و گین‌ها تا پیش از این روی خود صفحه رسم
   می‌شدند تا کاربر گفت تصویر شلوغ شده؛ این‌ها نه ولتاژند نه جریان، پس اصلاً
   روی صفحهٔ ‎I-V‎ جای صادقانه‌ای نداشتند. همان ‎data-i‎، همان ویرایشگر. */
.evcg{display:flex;flex-wrap:wrap;align-items:center;gap:6px;margin-top:7px}
.qglg{display:flex;flex-wrap:wrap;gap:3px 13px;margin:3px 2px 7px;font-size:.86em}
.qglg .qgit{white-space:nowrap}
.qglg i{display:inline-block;width:11px;height:11px;border-radius:3px;margin-left:4px;vertical-align:-1px;border:1px solid rgba(140,160,190,.45)}
.evct{font-size:.82em;color:var(--mu);font-weight:700;margin-inline-end:2px}
.evc{display:inline-flex;align-items:baseline;gap:5px;background:var(--in);
 border:1px solid var(--rs);border-radius:999px;padding:3px 10px;font-size:11.5px;
 line-height:1.7;cursor:pointer}
.evc:hover{border-color:var(--ac)}
.evc .evcl{color:var(--mu)}
.evc .evs{color:var(--ac2);font-weight:700;text-decoration:underline dotted}
.evc .evs:hover{color:#cfe2ff}
.evs:hover{fill:var(--ac2)}
.evpop{position:absolute;z-index:70;background:var(--cd);border:1px solid var(--ac);
       border-radius:9px;padding:9px 11px;box-shadow:0 10px 30px #0009;direction:rtl}
.evpt{font-size:12px;font-weight:700;color:var(--tx);margin-bottom:6px;white-space:nowrap}
.evpr{display:flex;align-items:center;gap:6px;direction:ltr;justify-content:flex-end}
.evph{font-size:10.5px;color:var(--mu);margin-top:6px;white-space:nowrap}
.evv{color:var(--tx);font-weight:700;white-space:nowrap}
.fl{display:flex;flex-wrap:wrap;gap:6px;margin-top:12px;padding-top:12px;border-top:1px solid var(--ln)}
.tg{font-size:12px;padding:3px 10px;border-radius:999px;background:var(--rs);color:var(--mu);border:1px solid transparent;display:inline-flex;align-items:center;gap:6px}
.tg::before{content:"";width:7px;height:7px;border-radius:50%;background:currentColor;opacity:.9}
.tg.g{background:rgba(52,211,153,.12);color:var(--ok);border-color:rgba(52,211,153,.25)}
.tg.r{background:rgba(251,94,106,.12);color:var(--er);border-color:rgba(251,94,106,.3)}
.tg.y{background:rgba(251,191,36,.12);color:var(--wa);border-color:rgba(251,191,36,.28)}
.ch{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(430px,100%),1fr));gap:14px}
.hd{display:flex;justify-content:space-between;align-items:center;gap:8px;margin-bottom:10px;flex-wrap:wrap}
.hd b{font-size:15px;display:flex;align-items:center;gap:8px}
.hd b::before{content:"";width:4px;height:18px;border-radius:4px;background:linear-gradient(180deg,var(--ac2),var(--ac))}
.big{display:flex;justify-content:space-between;align-items:baseline;margin:6px 0}.big b{font-size:28px;font-weight:800;font-variant-numeric:tabular-nums}
.bg2{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(430px,100%),1fr));gap:0 14px}
.ch table td.n{text-align:left;white-space:nowrap}
.bar{height:8px;background:var(--in);border:1px solid var(--ln);border-radius:8px;overflow:hidden;position:relative;margin:5px 0 12px}
.bar i{position:absolute;inset:0 0 0 auto;width:0;background:linear-gradient(90deg,var(--ac),var(--ac2));transition:width .3s}
.bar u{position:absolute;top:0;bottom:0;width:2px;background:var(--wa);box-shadow:0 0 6px var(--wa)}
table{width:100%;border-collapse:collapse;font-size:.93em}td{padding:6px 2px;border-top:1px solid var(--ln)}td:last-child{text-align:left}
.bt{width:100%;border:0;border-radius:12px;padding:12px;margin-top:12px;font-weight:700;color:#fff;min-height:44px;transition:filter .15s,transform .05s}
.bt:active{transform:scale(.99)}
.cut{background:linear-gradient(180deg,#e5484d,#c62f35)}.run{background:linear-gradient(180deg,#2fbf8f,#1e9e73);color:#04120c}
.rw{display:grid;grid-template-columns:1fr auto;gap:2px 12px;align-items:center;padding:10px 0;border-top:1px solid var(--ln)}.rw:first-of-type{border-top:0}

.ap{font-size:12px;color:var(--ac2);margin-right:6px}
.ct{display:flex;align-items:center;gap:6px}
input[type=number],select{background:var(--in);border:1px solid var(--ln);border-radius:10px;padding:7px 9px;direction:ltr;min-height:36px;transition:border-color .15s,box-shadow .15s}
input[type=number]{width:min(108px,100%);max-width:100%;box-sizing:border-box}
input[type=number]:hover,select:hover{border-color:#2c3850}
input[type=number]:focus,select:focus{border-color:var(--ac);box-shadow:0 0 0 3px var(--ring);outline:none}
select{direction:rtl}
.sb{border:1px solid transparent;border-radius:10px;padding:7px 14px;background:linear-gradient(180deg,#3d7ef0,#2f68d8);color:#fff;font-weight:600;min-height:36px;transition:filter .15s,transform .05s;box-shadow:inset 0 1px 0 rgba(255,255,255,.18)}
.sb:hover{filter:brightness(1.1)}.sb:active{transform:scale(.98)}
.sw{border:1px solid var(--ln);border-radius:10px;padding:7px 0;width:68px;min-height:36px;background:var(--rs);color:var(--mu);font-weight:600;transition:background .15s}
.sw.on{background:linear-gradient(180deg,#2fbf8f,#1e9e73);color:#04120c;border-color:transparent}.sw.w.on{background:linear-gradient(180deg,#fbbf24,#dd9a12);color:#231600}
.sg{display:flex;background:var(--in);border:1px solid var(--ln);border-radius:10px;padding:3px}.sg button{border:0;background:none;min-width:36px;padding:5px 8px;border-radius:7px;color:var(--mu);font-weight:600}.sg button.on{background:var(--ac);color:#fff;box-shadow:0 2px 6px rgba(0,0,0,.4)}
.wn{background:rgba(251,94,106,.1);border:1px solid rgba(251,94,106,.35);color:#ffc2c7;border-radius:14px;padding:11px 13px;margin-top:12px;font-size:13px}
.wn b{color:var(--er)}.gb{display:none}.gb.v{display:block}
.mx{display:flex;justify-content:space-between;align-items:center;gap:12px}
.ms{display:grid;grid-template-columns:repeat(5,1fr);gap:6px;margin-top:10px;text-align:center}.ms div{background:var(--in);border:1px solid var(--ln);border-radius:10px;padding:7px 2px}.ms b{display:block;font-variant-numeric:tabular-nums}
.off{background:linear-gradient(180deg,#a02b33,#7c1f27);font-size:16px}
.fx{direction:ltr;text-align:right;unicode-bidi:isolate;font-size:11px;color:#7c86a0;font-variant-numeric:tabular-nums;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.fb{background:var(--in);border:1px solid var(--ln);border-radius:12px;padding:9px 11px;margin-bottom:6px}.fb .fx{font-size:12px;color:#a7b0c4;line-height:1.9;white-space:normal}.fb .lb{font-size:11px}
.as{display:flex;flex-wrap:wrap;align-items:center;gap:6px 10px;padding:10px 0;border-top:1px solid var(--ln)}.as .nm{flex:1 1 180px}.as .lv{font-weight:700;margin-left:4px;font-variant-numeric:tabular-nums}
.sb2{background:linear-gradient(180deg,#2a3a5c,#22304c);color:#d7e5ff;border-color:#31436a}.qr{background:linear-gradient(180deg,#2a3a5c,#22304c);color:#d7e5ff;border-color:#31436a}.qr.j{background:linear-gradient(180deg,#e5484d,#c62f35);color:#fff;border-color:transparent}.qr:disabled{opacity:.4;cursor:default}
.wt tr.wa td{background:rgba(251,191,36,.08)}.wt .wi{width:76px}
canvas{width:100%;height:160px;display:block;background:var(--in);border:1px solid var(--ln);border-radius:12px;margin-top:10px;direction:ltr}
.lg{display:flex;gap:14px;flex-wrap:wrap;font-size:12px;color:var(--mu);margin-top:8px}.lg i{display:inline-block;width:14px;height:4px;border-radius:2px;margin-left:5px;vertical-align:middle}
.ti2{display:flex;justify-content:space-between;align-items:center}
.ca{border-top:1px solid var(--ln);margin-top:4px;padding-top:12px}.ca .ti{margin-bottom:4px;color:var(--tx)}
.cg{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:10px}
.cb{border:1px solid #31436a;border-radius:12px;padding:10px 12px;background:linear-gradient(180deg,#2a3a5c,#22304c);color:#d7e5ff;display:flex;justify-content:space-between;align-items:center;min-height:44px;transition:filter .15s}
.cb:hover{filter:brightness(1.12)}.cb b{font-weight:500;color:var(--ac2)}
.cb.lo{background:var(--in);color:var(--mu);border-color:var(--ln)}.cb:disabled{opacity:.45;cursor:default}
.cm{margin-top:10px;min-height:1.6em}.cm.g{color:var(--ok)}.cm.r{color:var(--er)}.wr{color:var(--wa)}
body.dn #sh,body.dn #ch{opacity:.45;filter:grayscale(1)}
.bsb{position:sticky;top:var(--t-sub,113px);z-index:54;border-color:#6b5206}.bqr2{display:flex;gap:8px;margin-top:12px}
.bctl{display:flex;flex-wrap:wrap;gap:10px;align-items:center;margin-top:10px}
.tw{overflow:auto;max-height:420px;margin:6px 0 10px;border:1px solid var(--ln);border-radius:12px}.bt2{font-size:12px;direction:ltr;white-space:nowrap}.bt2 th{position:sticky;top:0;background:var(--rs);color:var(--mu);font-weight:600;text-align:left;padding:6px 8px}.bt2 td{padding:5px 8px;text-align:left}
.bt3{width:auto;font-size:13px}.bt3 th{color:var(--mu);font-weight:600;text-align:right;padding:5px 8px;white-space:nowrap}.bt3 td{padding:5px 8px;text-align:right}.bt3 input[type=number]{padding:6px 8px}
.bsum{font-size:12px;direction:ltr;text-align:left;line-height:1.9;margin-bottom:8px}.okc{color:var(--ok)}.erc{color:var(--er)}
.bxw textarea{width:100%;height:150px;background:var(--in);color:#a7b0c4;border:1px solid var(--ln);border-radius:12px;padding:9px;font:11px/1.5 monospace;direction:ltr;margin-top:6px}
.sx{font-size:12.5px;line-height:1.95;color:#aab3c5;margin:2px 0 6px;padding:0 2px}.sx b{color:var(--tx);font-weight:700}
.ds{font-size:13px;line-height:2;color:#c9d0df;background:var(--in);border:1px solid var(--ln);border-radius:12px;padding:11px 15px}.ds ul{padding-right:18px}.ds b{color:var(--tx)}
.qs{display:grid;grid-template-columns:1fr 1fr;gap:12px}.q{background:var(--in);border:1px solid var(--ln);border-radius:12px;padding:12px}.q input[type=number]{width:92px}.q .cut{background:linear-gradient(180deg,#e5484d,#c62f35)}.q .run{background:linear-gradient(180deg,#2fbf8f,#1e9e73);color:#04120c}
.eg3{display:flex;flex-wrap:wrap;gap:10px 16px;margin-top:6px}.eg3 label{display:flex;flex-direction:column;gap:4px;font-size:13px}
.pgx{display:none}.pgx.a{display:block}body:not(.br) .wstop{display:none}body.br .brun{opacity:.4;pointer-events:none}a.lnk{text-decoration:none;display:inline-block}
.bq{background:var(--in);border:1px solid rgba(251,191,36,.4);border-radius:12px;padding:11px;margin-top:10px}
/* [EN] v1.47 (user order "they jump up and down"): the fields were a wrap
   flex row, so a two-line caption pushed its own input below its neighbours
   and the row lost its baseline. A grid with stretched cells and a caption
   that grows into the free space puts every input on the same line and
   leaves room under it for the factory default.
   [FA] فیلدها ردیف flex بودند، پس عنوان دوخطی ورودی خودش را پایین می‌انداخت و
   ردیف از خط مبنا می‌افتاد. گرید با خانه‌های کشیده و عنوانی که فضای خالی را
   پر می‌کند، همهٔ ورودی‌ها را هم‌تراز می‌کند و زیرش جا برای پیش‌فرض می‌ماند. */
.bqr{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:10px 14px;align-items:stretch;margin-top:10px}
.bqr label{display:flex;flex-direction:column;gap:3px;font-size:13px;font-weight:600}
.bqr label .t{flex:1 0 auto}
.bqr label .lb,.bqr label .dflt{font-weight:400}
.bqr input[type=number]{width:100%}
.bqr>button,.bqr>.sb{align-self:end}
/* [EN] v1.76 (user order): one full-width factory key per scenario.
   [FA] کلید بازگردانی هر سناریو، تمام‌عرض. */
.fwb{grid-column:1/-1;width:100%}
.dflt{font-size:11px;color:var(--mu);opacity:.9;direction:rtl}
.dflt b{font-weight:700;color:#aab4c8;font-variant-numeric:tabular-nums}
/* [EN] A shared parameter is shown, not re-offered: one writable field owns
   it and every other place echoes it read-only with a pointer to the owner.
   Two editable copies of one register is how a panel starts lying.
   [FA] پارامتر مشترک نمایش داده می‌شود نه دوباره پیشنهاد: یک فیلد صاحب آن است
   و بقیه فقط بازتاب فقط-خواندنی با اشاره به صاحبش. دو فیلدِ قابل‌نوشتن روی یک
   رجیستر، همان جایی است که پنل شروع به دروغ‌گفتن می‌کند. */
.shv{display:flex;flex-direction:column;gap:3px;justify-content:flex-end;background:var(--cd);border:1px dashed var(--ln);border-radius:10px;padding:7px 10px;font-size:13px}
.shv .t{flex:1 0 auto;font-weight:600;color:var(--mu)}
.shv b{font-size:15px;font-variant-numeric:tabular-nums;direction:ltr;text-align:left}
.shv .ow{font-size:11px;color:var(--mu)}
.cc .ca{border-top:0;margin-top:0;padding-top:0}.stp2{background:linear-gradient(180deg,#e5484d,#c62f35);white-space:nowrap}
.vc{justify-content:center}.vc input[type=number]{width:min(108px,100%);max-width:100%;box-sizing:border-box}
.fl{margin-top:0;padding-top:0;border-top:0}#sh .hd{flex-wrap:wrap;gap:8px}
.sec{display:flex;justify-content:space-between;align-items:center;gap:8px;font-size:13px;font-weight:700;color:var(--tx);margin:16px 0 8px;padding-top:13px;border-top:1px solid var(--ln)}
.sec::after{content:"";flex:1;height:1px;background:linear-gradient(90deg,transparent,var(--ln));border-radius:1px}
.frr{display:grid;grid-template-columns:1fr 1fr;gap:0 28px}.frr .rw:first-of-type{border-top:1px solid var(--ln)}.fxw{margin-top:6px;font-size:12px}
.kc{font-size:12px;margin-top:6px}.cr{margin-top:10px;flex-wrap:wrap}.cr .cb{flex:1 1 120px}.cr input[type=number]{width:124px}
.off2{background:linear-gradient(180deg,#a02b33,#7c1f27);white-space:nowrap}
@media(max-width:1000px){.ch,.frr,.qs{grid-template-columns:1fr}}
@media(max-width:640px){.sbt button{font-size:12px;padding:8px 2px}.cb{font-size:12px;padding:9px 8px}.cb span{white-space:nowrap}.ch{grid-template-columns:1fr}.bg2{grid-template-columns:1fr}.ms{grid-template-columns:repeat(3,1fr)}header{margin:0 -8px 10px;padding-left:8px;padding-right:8px}body{padding:0 8px 24px}}
/* [EN] v1.57 (user order): the scenario bar sticks right under the top bar
   and stays visible while scrolling. [FA] نوار سناریوها زیر نوار بالایی
   می‌چسبد و هنگام اسکرول همیشه دیده می‌شود. */
.sbt{position:sticky;top:var(--t-sub,113px);z-index:54;flex-wrap:wrap;display:flex;gap:4px;background:var(--bg,#090c12);border:1px solid var(--ln);border-radius:12px;padding:4px;margin-bottom:12px}
#sbt{position:sticky;top:var(--t-sub,113px);z-index:54}
/* [FA] نوار کارت‌های سناریو یک طبقه پایین‌تر از نوار «شارژ و PID…» می‌چسبد،
   نه رویش: قبلاً هر دو روی ۱۱۳px بودند و این یکی آن یکی را می‌پوشاند. */
#usel{position:sticky;top:var(--t-sub2,160px);z-index:52}
.sbt button{flex:1;border:0;background:none;border-radius:8px;padding:8px 6px;color:var(--mu);font-weight:700;transition:background .15s,color .15s;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.sbt button:hover{color:var(--tx)}.sbt button.a{background:linear-gradient(180deg,#24406e,#1b3358);color:#fff;box-shadow:inset 0 1px 0 rgba(255,255,255,.12)}
.sgx{display:none}.sgx.a{display:block}
.ag{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px;margin:8px 0}
.ab{background:var(--in);border:1px solid var(--ln);border-radius:12px;padding:10px 12px;min-height:92px;transition:border-color .2s}
.ab small{color:var(--mu)}.ab b{font-size:19px;display:block;margin:2px 0;font-variant-numeric:tabular-nums}.ab .lb{display:block;min-height:20px}.ab .tg{margin-top:4px;display:inline-flex}
.wbx:empty{display:none}.wbx{margin:10px 0;padding:10px 12px;border-radius:12px;border:1px solid var(--ln);background:var(--rs);font-size:13px;line-height:1.9}
.wbx.bad{border-color:var(--er);background:rgba(255,104,115,.12);color:var(--er)}
.wbx.warn{border-color:var(--wa);background:rgba(247,193,60,.10);color:var(--wa)}
.ab.bad{border-color:rgba(251,94,106,.55);box-shadow:0 0 0 1px rgba(251,94,106,.2)}.ab.warn{border-color:rgba(251,191,36,.5)}.ab.good{border-color:rgba(52,211,153,.4)}
/* [EN] v1.79 (user: "what is this? there used to be a LED behind it"): the
   .bit class had markup (<i> dot + <small> label) but NO stylesheet rule at
   all, so the dots were invisible and the labels ran together as
   "ADCOC1OC2باتری...". ‎The LEDs are needed - they are the only per-bit view‎
   of the latched fault mask - so they are drawn properly instead of removed.
   A latched bit stays dim red at all times and brightens on the blink phase,
   so a fault is never invisible between blinks.
   [FA] کلاس .bit هیچ استایلی نداشت؛ پس نقطه‌ها دیده نمی‌شدند و برچسب‌ها به هم
   چسبیده بودند. حالا هر بیت یک LED واقعی با برچسب زیرش دارد: بیتِ قفل‌شده
   همیشه قرمزِ کم‌رنگ است و در فاز چشمک پررنگ می‌شود. */
.bit{display:inline-flex;flex-direction:column;align-items:center;gap:5px;min-width:54px}
.bit i{width:14px;height:14px;border-radius:50%;background:#28303f;border:1px solid var(--ln);box-shadow:inset 0 1px 2px #0009;transition:background .12s,box-shadow .12s}
.bit small{font-size:11px;line-height:1;color:var(--mu);white-space:nowrap}
.bit.set i{background:#8d2a2e;border-color:#b13b40}
.bit.set small{color:#ffbdbf}
.bit.set.on i{background:#ff5a5f;border-color:#ff5a5f;box-shadow:0 0 9px #ff5a5f,inset 0 1px 2px #0005}
/* [EN] v1.79: .prod (the live readout strip of scenarios 5 and 6) had no rule
   either and rendered as loose text. [FA] نوار مقدارهای زنده هم استایل نداشت. */
.prod{background:var(--in);border:1px solid var(--ln);border-radius:10px;padding:8px 12px;margin:2px 0 10px;font-size:13px;line-height:1.9}
.prod b{font-variant-numeric:tabular-nums}
.leds{display:flex;gap:14px;align-items:center;flex-wrap:wrap;background:var(--in);border:1px solid var(--ln);border-radius:14px;padding:10px 14px;margin:2px 0 12px}
.fx2{font-size:12px;color:#c9d0df;line-height:1.9;margin-top:4px}
@media(prefers-reduced-motion:reduce){*{transition:none!important}}
.ldon{display:inline-block;width:9px;height:9px;border-radius:50%;background:var(--ok);box-shadow:0 0 8px var(--ok);margin-right:8px}
.hnl input[type=number]{width:66px;min-height:30px;padding:4px 6px}
.movl{position:fixed;inset:0;z-index:300;background:rgba(3,5,9,.72);backdrop-filter:blur(3px);-webkit-backdrop-filter:blur(3px);display:flex;align-items:center;justify-content:center;padding:16px}
.mod{background:var(--cd);border:1px solid var(--ln);border-radius:18px;padding:22px;max-width:460px;width:100%;box-shadow:0 24px 60px rgba(0,0,0,.6)}
.mod h3{font-size:16px;margin-bottom:8px}
.mod .sb{width:100%;margin-top:10px}
input:disabled{opacity:.38;cursor:not-allowed}
/* [EN] v1.78: the two lock-behaviour switches are real checkboxes now, not
   buttons - a button looked like "press me to do it", while this is a stored
   yes/no setting. The row spans the card and carries its own answer line.
   [FA] دو کلید رفتارِ قفل حالا چک‌باکس واقعی‌اند، نه دکمه. */
.ckr{grid-column:1/-1;display:flex;align-items:flex-start;gap:10px;background:var(--in);border:1px solid var(--ln);border-radius:10px;padding:10px 12px}
.ckr input[type="checkbox"]{width:20px;height:20px;margin:2px 0 0;accent-color:#e5484d;flex:0 0 auto;cursor:pointer}
.ckr .ckt{display:block;font-weight:700}
.ckr .cks{display:block;color:var(--mu);font-size:12px;margin-top:3px;line-height:1.7}
.srvw{overflow-x:auto;border:1px solid var(--ln);border-radius:12px}

.srv{width:100%;min-width:680px;border-collapse:collapse;font-size:13px;table-layout:fixed}
.srv col.c1{width:118px}.srv col.c2{width:96px}.srv col.c4{width:128px}.srv col.c5{width:196px}
.srv th{color:var(--mu);font-weight:600;text-align:right;padding:7px 10px;border-bottom:1px solid var(--ln);white-space:nowrap}
.srv td{padding:6px 10px;border-top:1px solid var(--ln);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.srv tr:first-child td{border-top:0}
.srv .tg{min-width:104px;justify-content:center}
.srv input[type=number]{width:78px;min-height:30px;padding:3px 6px}
/* [EN] v1.43 charge-scenario card (user order 2026-10-04 "make it one usable
   page"): the card now carries the explanation, the voltage ladder and the
   blink arithmetic, so it needs a readable prose block, a calm "computed
   value" line and a table that is clearly a report, not an input grid.
   [FA] کارت سناریوی شارژ: توضیح، نردبان ولتاژ و حساب چشمک یک‌جا - پس یک بلوک
   متن خوانا، یک خط «مقدار محاسبه‌شده» آرام و جدولی که آشکارا گزارش است. */
.c4ds{margin:2px 0 4px}
.c4ds ul{margin:4px 0 6px}
.c4ds li{margin:2px 0}
.c4n{font-size:12.5px;line-height:2;color:var(--ac2);background:var(--in);border:1px solid var(--rs);border-radius:12px;padding:8px 12px;margin-top:8px}
.c4f{font-size:12.5px;line-height:2;color:#c9d0df;background:var(--in);border:1px solid var(--ln);border-radius:12px;padding:9px 12px;margin:2px 0 8px}
/* v1.51: شبیه‌ساز زندهٔ سناریو - سه LED و بازر، فقط از روی کادرهای همین صفحه */
.sim{border:1px solid #2c3550;border-radius:10px;padding:8px 10px;margin:0 0 10px;background:#141a28}
.simh{display:flex;align-items:center;gap:8px;flex-wrap:wrap;font-size:12px;margin-bottom:8px}
.simh .sb{margin-right:auto}
.simb{display:flex;align-items:center;gap:14px;flex-wrap:wrap}
.sl{width:22px;height:22px;border-radius:50%;flex:none;background:#242c3e;box-shadow:inset 0 2px 5px rgba(0,0,0,.6);transition:background .05s,box-shadow .05s}
.sl.r.on{background:#ff4545;box-shadow:0 0 16px #ff4545}
.sl.g.on{background:#2eff8f;box-shadow:0 0 16px #2eff8f}
.sl.y.on{background:#ffd23b;box-shadow:0 0 16px #ffd23b}
.szz{font-size:17px;opacity:.35;transition:opacity .05s}
.szz.on{opacity:1}
.simt{font-size:12px;color:#9fb0cc}
.simc{margin-top:8px;font-size:12px}
.simc input[type=range]{width:200px;vertical-align:middle;margin:0 8px}
/* [EN] v1.45 (user order): one block per discharge band, so every variable
   of a band sits together. A shared value appears in more than one block
   and is labelled - the blocks stay side by side instead of the reader
   hopping between a thresholds row and a timings row.
   [FA] هر باند دشارژ یک بلوک: همهٔ متغیرهای یک باند کنار هم. مقدار مشترک در
   چند بلوک می‌آید و برچسب می‌خورد، به‌جای پریدن بین ردیف آستانه و ردیف زمان. */
.bnd{background:var(--in);border:1px solid var(--ln);border-radius:14px;padding:10px 13px;margin:8px 0}
.bnd.bndc{border-color:rgba(251,94,106,.45)}
.bnh{display:flex;flex-wrap:wrap;align-items:baseline;gap:8px;margin-bottom:2px}
.bnh b{font-size:13.5px}
.bnd .bqr{margin-top:6px}
.bnd .c4n{margin-top:8px}
.c4tb{min-width:560px}
.c4tb td,.c4tb th{text-align:center}
.c4tb tr.hi td{background:rgba(99,162,255,.10)}
.c4tb td.n{text-align:center}
.srv .sb{min-height:30px;padding:3px 10px}
tr.rok{background:rgba(52,211,153,.05)}tr.rwr{background:rgba(251,191,36,.07)}tr.rbd{background:rgba(251,94,106,.08)}
/* v1.52: نوار سراسری «ارسال به برد» و نشانهٔ کادرهای ارسال‌نشده */
.pq{outline:2px solid #ffb020;outline-offset:1px}
#sbar{position:fixed;left:0;right:0;bottom:0;z-index:70;display:none;gap:10px;align-items:center;flex-wrap:wrap;
padding:8px 14px;background:#16203a;border-top:1px solid #35507f;box-shadow:0 -6px 18px rgba(0,0,0,.45);font-size:13px}
#sbar.on{display:flex}
#sbar b{color:#ffb020}
#sbst{color:#9fb0cc}
/* v1.70: کارت نتیجهٔ ارسال — #sbar با خالی‌شدن صف پنهان می‌شد و پیام موفقیت
   همان لحظه گم می‌شد؛ این کارت تا بسته نشود می‌ماند. */
#sres{position:fixed;inset:0;z-index:80;display:none;align-items:center;justify-content:center;background:rgba(5,8,14,.62)}
#sres.on{display:flex}
#sres .rb{max-width:560px;width:calc(100% - 28px);max-height:80vh;overflow:auto;background:#121a2c;border:1px solid #35507f;border-radius:16px;padding:16px}
#sres.ok .rb{border-color:#1f7a5a}#sres.warn .rb{border-color:#8a6a12}#sres.bad .rb{border-color:#8a2a2d}
#srst{display:block;font-size:15px;margin-bottom:8px}
#sres.ok #srst{color:var(--ok)}#sres.warn #srst{color:#ffb020}#sres.bad #srst{color:#e5484d}
#srsm{font-size:13px;color:#c7d3e6;line-height:1.9}#srsm ul{margin:6px 0;padding-inline-start:18px}#srsm code{direction:ltr;display:inline-block}
#srsa{display:flex;gap:8px;flex-wrap:wrap;margin-top:12px}
</style></head><body>
<div id="sbar"><span>📝 <b id="sbn">0</b> تغییر هنوز روی برد ننشسته — کادرهای نارنجی</span>
<button class="sb" onclick="sendall()">ارسال همهٔ تغییرات به برد</button>
<button class="sb sb2" onclick="pundo()">لغو و برگرداندن از برد</button>
<span id="sbst"></span></div>
<div id="sres" role="dialog" aria-modal="true"><div class="rb"><b id="srst"></b><div id="srsm"></div><div id="srsa"></div></div></div>
<header><h1>پنل ChangeOver</h1><span class="bs" id="bs">build 2543c2e</span><div class="lk" id="lk"><span id="lt">در حال اتصال…</span><i></i></div></header>
<nav><button class="a" data-t="0">پنل</button><button data-t="1">داده‌برداری بنچ</button><button data-t="2">تنظیمات</button></nav>
<div class="wn gb" id="mb"><div class="mx"><div><b>مود تست دستی فعال است</b> — شارژر خودکار و محافظت‌های باتری متوقف‌اند. <span id="ka"></span></div><button class="sb stp2" id="mx">خروج از مود دستی</button></div></div>
<main id="pg">
<div class="pgx a" id="p0">
<div id="lnkw" class="wbx"></div>
<div class="cd" id="sh"><div class="hd"><b>ولتاژها و وضعیت آلارم‌ها <span class="lb">· عدد مولتی‌متر (V) را کنار هر ولتاژ وارد کنید تا آفست آن کالیبره شود</span> <span class="ldon" id="aslive"></span></b><div class="fl" id="fl"></div></div><div id="ast"></div>
<div class="sec">فیلتر جریان <span class="lb">(مشترک هر دو کانال)</span></div>
<div class="sx">وضعیت زندهٔ دو مرحلهٔ فیلتر جریان (median و میانگین) که هر دو کانال از آن‌ها استفاده می‌کنند؛ ویرایش خود اعداد در زیرتب «فیلتر و کالیبراسیون» است.</div><div class="frr" id="fg"></div><div class="fx fxw" id="ff"></div></div>

<div class="ch" id="ch"></div>
<!-- [EN] The charge-stages chart was mounted here as well as in
     Settings. Removed on the user's order ("why did you put the chart
     in the panel too? delete it from there"), so this page now carries
     just the two charger cards (the report table that later sat inside
     this section was removed wholesale on the user's follow-up the same
     day - v1.36), and the chart has exactly one home: Settings > Charge
     and PID. The renderer is untouched - it was always one function
     filling every .qgm it finds, so dropping a mount removes a copy,
     never a capability.
     [FA] نمودار مراحل شارژ اینجا هم نصب بود، کنار نسخهٔ تب تنظیمات. به
     دستور کاربر از صفحهٔ شارژرها برداشته شد، پس این صفحه فقط دو کارت شارژر را دارد (جدولِ
     گزارشی که بعداً داخل همین بخش نشانده شد، به دستور پیگیریِ همان روز
     کاملاً حذف شد - v1.36) و نمودار دقیقاً یک خانه دارد: تنظیمات > شارژ و PID. رندرکننده دست‌نخورده است. -->

</div>
<div class="pgx" id="p1"></div>
<div class="pgx" id="p2">
<div class="sbt" id="sbt"><button class="a" data-s="0">شارژ و PID</button><button data-s="1">سناریوها</button><button data-s="2">نظارت و ایمنی</button><button data-s="3">کالیبراسیون و فیلتر جریان</button><button data-s="4">پشتیبان‌گیری</button></div>
<div class="sgx a" id="s0">
<div class="cd">
<div class="hd"><b>نمودار مراحل شارژ</b></div>
<div class="sx"><b>نمودار فقط ولتاژ و جریان را نشان می‌دهد</b> — هر عددی که روی محورها جای واقعی دارد، روی خود نمودار زیرخط‌دار و کلیک‌پذیر است. سیزده عددِ زیر نمودار (تراشه‌ها) نه ولتاژند نه جریان، پس آنجا آمده‌اند و با یک کلیک ویرایش می‌شوند؛ در سه دسته:
<br><b>۱ · رسیدن نرم به هدف</b> — <b>duty step</b> سقفِ سرعت حرکت duty به‌سمت هدفِ PID، <b>hyst</b> نادیده‌گرفتن جنبش‌های ریزِ خروجی PID تا PWM نلرزد، <b>V filter</b> نرم‌کردن عدد ولتاژ پیش از PID.
<br><b>۲ · ترمز اضطراری و حاشیهٔ امن</b> — <b>Backstop gain I</b> و <b>Backstop gain V</b> برشِ تناسبیِ سقف duty هنگام تجاوز از حد جریان یا ولتاژ (صفر = خاموش؛ این‌ها PID نیستند، بالای آن می‌ایستند) و <b>margin</b> فاصلهٔ ایمن از لبهٔ سقف جریان.
<br><b>۳ · تصمیم‌های زمانی</b> — <b>Absorb max</b> سقف زمان Absorb، <b>Absorb hold</b> حداقل ماندن در Absorb، <b>Taper sustain</b> مدتی که جریان باید زیر Taper بماند تا «شارژ کامل» اعلام شود، <b>settle</b> صبر بعد از وصل باتری، <b>JIT lockout</b> خاموشی اجباری بعد از هر تریپ، <b>manual deadman</b> انقضای duty دستی با سکوت پنل، <b>ramp down</b> فاصلهٔ پله‌های خاموشی نرم.
<br><b>قاعدهٔ تنظیم:</b> هر بار یک عدد را عوض کنید و یک چرخهٔ کامل را ببینید؛ برای آرام‌کردن حلقه اول duty step، بعد hyst و در آخر V filter.<br><b>ماندگاری:</b> هر مقداری که ثبت کنید روی فلش برد ذخیره می‌شود و با قطع برق می‌ماند؛ برد هر عدد را به بازهٔ مجازش گیره می‌زند و اگر نتیجه با آنچه تایپ کردید فرق داشت، برچسبش زرد می‌شود.</div>
<div class="qwm" style="margin:2px 0 0"></div>
<div class="qgm" style="direction:ltr;overflow-x:auto"></div>
<div class="qglg lb"></div>
<div class="lb qglm">در انتظار دادهٔ برد…</div>
<div class="qgcm"></div>
</div>

<div class="cd">
<div class="hd"><b>PID دوحلقه‌ای شارژ (CC/CV)</b><span class="lb">· شناسه‌های ۸۳..۹۲ · روی فلش برد ذخیره می‌شود (~۱٫۵ ثانیه پس از آخرین تغییر)</span></div>
<div class="sx"><span class="n">duty</span> را <span class="n">PID</span> می‌سازد، آن هم با <b>دو</b> حلقه، چون شارژ باید دو چیز را با هم محدود کند:<br><br><b>حلقهٔ جریان (<span class="n">CC</span>)</b> = وقتی باتری خالی است، جریان را ثابت نگه می‌دارد.<br><b>حلقهٔ ولتاژ (<span class="n">CV</span>)</b> = نزدیک پرشدن، ولتاژ را روی هدف می‌نشاند.<br><br><b>دو حلقه چطور یک duty را می‌رانند؟</b> خروجی یکی است: هر لحظه هر حلقه که عدد کمتری بخواهد، فرمان دست اوست و دیگری آن لحظه نادیده گرفته می‌شود. انتگرال‌گیر هم مشترک است، پس در لحظهٔ تحویل انتگرال روی <span class="n">۱۹۹۵۱۱</span> می‌ماند و <span class="n">duty</span> نمی‌پرد: صفر پرش.
<br><b>چرا یک حلقه کافی نبود؟</b> امتحان شد و جواب نداد: با یک ردیف ضریب، میلی‌ولت و میلی‌آمپر با هم مقایسه می‌شوند و <span class="n">duty</span> می‌لرزد (در ۱۰ ساعت <span class="n">۱۴۰۸</span> تا <span class="n">۸۴۶۶۰</span> بار تغییر جهت، در برابر ۴ بار الان). با حلقهٔ فقط‌ولتاژ هم جریان تا <span class="n">۷۰۷mA</span> بالا می‌رفت، چون ترمزِ جریان تنها <i>بعد از</i> عبور از حد وارد می‌شود.</div>
<div class="sec">حلقهٔ جریان — CC (Bulk)
 <span class="lb">· پیش‌فرض ۱۲ / ۱۶۰۰ / ۰ / ۱۰۰۰ / ۱۰۰۰ · کالیبرهٔ کارخانه</span></div>
<div class="sx">ردیف ضرایب حلقه‌ای که <b>جریان</b> را روی هدف نگه می‌دارد:<br><br><b>Kp</b> = واکنش فوری به خطا.<br><b>Ki</b> = جمع‌کنندهٔ خطا؛ همین است که در نهایت جریان را دقیق روی هدف می‌نشاند.<br><b>Kd</b> = واکنش به سرعت تغییر (پیش‌فرض صفر، چون نویز را بزرگ می‌کند).<br><b>نرخ صعود/نزول</b> = سقفِ سرعت حرکت نقطهٔ کار.</div>
<div class="bqr">
<label>Kp (‰ بر آمپر)<input type="number" id="q83" step="5" min="0" max="20000"><span class="lb" id="a83"></span></label>
<label>Ki<input type="number" id="q84" step="50" min="0" max="20000"><span class="lb" id="a84"></span></label>
<label>Kd<input type="number" id="q85" step="10" min="0" max="20000"><span class="lb" id="a85"></span></label>
<label>نرخ صعود (هزارمِ‌پرمیل در ثانیه)<input type="number" id="q86" step="10" min="10" max="20000"><span class="lb" id="a86"></span></label>
<label>نرخ نزول (هزارمِ‌پرمیل در ثانیه)<input type="number" id="q87" step="10" min="10" max="20000"><span class="lb" id="a87"></span></label>
</div>
<div class="sec">حلقهٔ ولتاژ — CV (Absorb)
 <span class="lb">· پیش‌فرض ۵۰ / ۱۸۰۰۰ / ۰ / ۱۰ / ۱۰۰۰ · نرخ صعود = کلید «رشد کندتر Absorb»</span></div>
<div class="sx">همان پنج عدد، این‌بار برای حلقه‌ای که <b>ولتاژ</b> را روی هدف Absorb نگه می‌دارد. هر لحظه هرکدام از دو حلقه duty کمتری بخواهد برنده است، پس هر دو حد همیشه رعایت می‌شوند.</div>
<div class="bqr">
<label>Kp (‰ بر ولت)<input type="number" id="q88" step="10" min="0" max="20000"><span class="lb" id="a88"></span></label>
<label>Ki<input type="number" id="q89" step="500" min="0" max="20000"><span class="lb" id="a89"></span></label>
<label>Kd<input type="number" id="q90" step="10" min="0" max="20000"><span class="lb" id="a90"></span></label>
<label>نرخ صعود (هزارمِ‌پرمیل در ثانیه)<input type="number" id="q91" step="5" min="10" max="20000"><span class="lb" id="a91"></span></label>
<label>نرخ نزول (هزارمِ‌پرمیل در ثانیه)<input type="number" id="q92" step="10" min="10" max="20000"><span class="lb" id="a92"></span></label>
</div>
<div id="pw" style="margin:6px 0 0"></div>
</div>
<div class="cd">
<!-- [EN] v1.38 (user order: \"why two factory-restore keys on the charge
     page? isn't one enough?\") - the PID-only button is gone; this single
     one restores the WHOLE charger scope (profile 20-26, PID 83-92 and
     limits 93-107 - the limits restore lost its own key when the table
     was deleted) and the caption says exactly that.
     [FA] v1.38 (دستور کاربر: «چرا دو تا کلید؟») - کلید جداگانهٔ PID رفت؛
     همین یکی کل محدودهٔ شارژر را برمی‌گرداند. -->
<div class="hd"><b>بازگردانی پیش‌فرض کارخانه</b><span class="lb">· مقادیر پروفایل شارژ (۲۰-۲۶)، ضرایب PID (۸۳-۹۲) و حدهای شارژر (۹۳-۱۰۷) را به کارخانه برمی‌گرداند؛ به آلارم‌ها، فیلترها و سناریوها دست نمی‌زند</span></div>
<div class="bqr"><button class="sb sb2" onclick="qdef()">بازگردانی پیش‌فرض کارخانه</button></div>
</div>
</div>
<!-- [EN] This closing tag was dropped in 0593f7c, when the profile card
     was removed and the sub-tabs renumbered. Without it s0 swallowed s1,
     s2 and s3, so hiding s0 hid them too and every sub-tab except
     "charge, filter and PID" looked dead. p2 was left unclosed and the
     tag that should have closed it was closing s0 instead.
     [FA] این تگ بسته در 0593f7c افتاد، وقتی کارت profile حذف و زیرتب‌ها
     دوباره شماره‌گذاری شدند. بدون آن s0 سه زیرتب دیگر را در خود می‌گرفت،
     پس مخفی‌کردن s0 آن‌ها را هم مخفی می‌کرد. -->
<div class="sgx" id="s1">

<div class="hd" style="margin-top:10px"><b>سناریوهای LED و بازر</b></div>
<div id="aw2" style="margin:2px 0 0"></div>
<div class="sbt" id="usel"><button class="a" data-u="1">۱ · اضافه‌ولتاژ</button><button data-u="2">۲ · قطع باتری</button><button data-u="3">۳ · دشارژ</button><button data-u="4">۴ · شارژ عادی</button><button data-u="5">۵ · عدم‌توازن</button><button data-u="6">۶ · باتری خراب</button></div>
<div class="cd" id="ucard1">
<div class="hd"><b>سناریو ۱ — اضافه‌ولتاژ ورودی (قرمز + بوق)</b></div>

<div class="ds c4ds">
<b>این سناریو کِی می‌آید؟</b> بالاترین اولویت برد است و روی هر سناریوی دیگری می‌نشیند.
<ul>
<li><b>ورود</b> — ولتاژ <b>ورودی</b> (نه باتری) از <b>آستانه</b> بالاتر برود.</li>
<li><b>خروج</b> — تا زیر «آستانه − hysteresis» برنگردد پاک نمی‌شود؛ همین فاصله جلوی لرزش روی مرز را می‌گیرد.</li>
<li><b>چهره</b> — قرمز چشمک‌زن + بوق دوره‌ای، و سبز ثابت روشن می‌ماند (ورودی هنوز هست).</li>
</ul>
</div>

<div class="sim" id="sim1"><div class="simh"><b>شبیه‌ساز این سناریو</b><span class="lb">از روی همین کادرها ساخته می‌شود — چیزی از برد خوانده نمی‌شود</span><label class="lb">سرعت <select id="sims1" onchange="simspd(this.value)"><option value="1">×۱</option><option value="10">×۱۰</option><option value="60">×۶۰</option><option value="600">×۶۰۰</option></select></label><button class="sb sb2" id="simb1" onclick="simtog(1)">توقف</button></div><div class="simb"><span class="sl r" id="sl1r"></span><span class="sl g" id="sl1g"></span><span class="sl y" id="sl1y"></span><span class="szz" id="sl1z">🔇</span><span class="simt" id="sl1t"></span></div><div class="simc"><label>ولتاژ ورودی (mV)<input type="range" id="simp1" min="20000" max="34000" step="100" value="26000" oninput="simlbl1()"><b id="simv1">26000mV</b></label> <span class="lb" id="sim1w"></span></div></div>
<div class="sec">۱) آستانهٔ ولتاژ ورودی <span class="lb">(mV)</span></div>
<div class="sx">این دو عدد تعیین می‌کنند «اضافه‌ولتاژ» از کجا شروع و کجا تمام می‌شود.<br><br><b>آستانه</b> = ولتاژی که بالاتر از آن سناریو روشن می‌شود.<br><b>hysteresis</b> = چقدر باید پایین‌تر از آستانه بیاید تا خاموش شود.<br><b>دلیل:</b> بدون آن، ولتاژِ دقیقاً روی مرز، سناریو را پشت‌سرهم روشن‌وخاموش می‌کند.</div>
<div class="bqr">
<label>آستانه اضافه‌ولتاژ ورودی (mV)<input type="number" id="q70" step="100" min="24000" max="32000"><span class="lb" id="a70"></span></label>
<label>hysteresis اضافه‌ولتاژ (mV)<input type="number" id="q71" step="100" min="0" max="2000"><span class="lb" id="a71"></span></label>
</div>
<div class="c4n" id="s1v"></div>

<div class="sec">۲) چشمک قرمز <span class="lb">(دوره / duty)</span></div>
<div class="sx">شکل چشمک چراغ قرمز در این سناریو.<br><br><b>دوره</b> = طول یک چرخهٔ کاملِ روشن+خاموش.<br><b>duty</b> = چند درصد از آن چرخه چراغ روشن باشد (۵۰٪ یعنی روشن و خاموش برابر).</div>
<div class="bqr">
<label>دوره چشمک قرمز (ms)<input type="number" id="q38" step="50" min="100" max="10000"><span class="lb" id="a38"></span></label>
<label>duty قرمز (٪)<input type="number" id="q39" step="5" min="0" max="100"><span class="lb" id="a39"></span></label>
</div>
<div class="c4n" id="s1b"></div>

<div class="sec">۳) بوق</div>
<div class="sx">صدا با چهار عدد ساخته می‌شود:<br><br><b>دوره</b> = هر چند وقت یک‌بار الگو تکرار شود (۰ = بی‌صدا).<br><b>مدت هر بوق</b> = طول خودِ یک بوق.<br><b>تعداد بوق</b> = چند بوق در هر تکرار پخش شود.<br><b>گپ</b> = سکوت بین آن بوق‌ها.<br><b>نکته:</b> درصد روشنیِ بوق جایی تایپ نمی‌شود و خودش از همین چهار عدد درمی‌آید.<br><b>قانون مشترک بوق‌ها:</b> گپ فقط <i>بین</i> دو بوق معنی دارد، پس با «تعداد بوق = ۱» کادر گپ خودکار غیرفعال می‌شود و با دو بوق به بالا دوباره فعال.</div>
<div class="bqr">
<label>دوره بوق (ms، صفر=خاموش)<input type="number" id="q40" step="500" min="0" max="600000"><span class="lb" id="a40"></span></label>
<label>مدت هر بوق (ms)<input type="number" id="q41" step="50" min="0" max="600000"><span class="lb" id="a41"></span></label>
<label>تعداد بوق<input type="number" id="q42" step="1" min="0" max="10"><span class="lb" id="a42"></span></label>
<label>گپ بین بوق‌ها (ms)<input type="number" id="q43" step="50" min="0" max="5000"><span class="lb" id="a43"></span></label>
</div>
<div class="c4n" id="s1z"></div>

<div class="bqr"><button class="sb sb2 fwb" onclick="ovdef()">بازگردانی پیش‌فرض کارخانهٔ سناریو ۱</button></div>
</div>
<div class="cd" id="ucard2" style="display:none">
<div class="hd"><b>سناریو ۲ — قطع باتری (قرمز + بوق)</b></div>
<div id="aw" style="margin:2px 0 0"></div>

<div class="ds c4ds">
<b>این سناریو کِی می‌آید؟</b> وقتی برد پرچم «قطع باتری» را قفل کند — یعنی مسیر باتری واقعاً باز شده باشد. دو تشخیص جدا کار می‌کنند:
<ul>
<li><b>قطع حین شارژ</b> — ولتاژ نیم‌باتری از <b>آستانهٔ قطع</b> بالاتر بپرد و این حالت به اندازهٔ <b>دبانس قطع</b> بماند (باتری نیست، پس ولتاژ بی‌بار بالا می‌رود).</li>
<li><b>غیبت و بازگشت</b> — افت زیر <b>آستانهٔ غیبت</b> به مدت دبانس غیبت یعنی باتری نیست؛ تنها راه پاک‌شدن، بالا رفتن از <b>آستانهٔ بازگشت</b> به مدت دبانس بازیابی است. برد همیشه بازگشت را دست‌کم ۵۰۰mV بالای غیبت نگه می‌دارد.</li>
<li><b>چهره</b> — قرمز چشمک‌زن + بوق، سبز ثابت.</li>
</ul>
</div>

<div class="sim" id="sim2"><div class="simh"><b>شبیه‌ساز این سناریو</b><span class="lb">از روی همین کادرها ساخته می‌شود — چیزی از برد خوانده نمی‌شود</span><label class="lb">سرعت <select id="sims2" onchange="simspd(this.value)"><option value="1">×۱</option><option value="10">×۱۰</option><option value="60">×۶۰</option><option value="600">×۶۰۰</option></select></label><button class="sb sb2" id="simb2" onclick="simtog(2)">توقف</button></div><div class="simb"><span class="sl r" id="sl2r"></span><span class="sl g" id="sl2g"></span><span class="sl y" id="sl2y"></span><span class="szz" id="sl2z">🔇</span><span class="simt" id="sl2t"></span></div></div>
<div class="sec">۱) تشخیص قطع حین شارژ <span class="lb">(mV / ms)</span></div>
<div class="sx">وقتی شارژر در حال کار است، باتریِ جداشده باعث می‌شود ولتاژ خروجی بپرد بالا.<br><br><b>آستانهٔ قطع</b> = ولتاژی که عبور از آن یعنی «باتری نیست».<br><b>دبانس قطع</b> = این حالت چقدر پیوسته بماند تا باور شود (ضدنویز).</div>
<div class="bqr">
<label>آستانهٔ قطع باتری (mV)<input type="number" id="q27" step="50" min="14000" max="15000"><span class="lb" id="a27"></span></label>
<label>دبانس قطع (ms)<input type="number" id="q28" step="10" min="50" max="1000"><span class="lb" id="a28"></span></label>
</div>
<div class="sec">۲) غیبت و بازگشت باتری <span class="lb">(mV / ms)</span></div>
<div class="sx">تشخیص دوم، از سمت ولتاژِ پایین:<br><br><b>آستانهٔ غیبت</b> = زیر این ولتاژ یعنی باتری سر جایش نیست<br><b>آستانهٔ بازگشت</b> = تنها راه پاک‌شدن پرچم، بالا رفتن از این عدد است (همیشه بالاتر از آستانهٔ غیبت تا پرچم نلرزد). دو <b>دبانس</b> هم می‌گویند هر کدام چقدر پیوسته بماند تا پذیرفته شود.</div>
<div class="bqr">
<label>آستانهٔ غیبت (mV)<input type="number" id="q29" step="100" min="3000" max="8000"><span class="lb" id="a29"></span></label>
<label>آستانهٔ بازگشت (mV)<input type="number" id="q30" step="100" min="4000" max="9000"><span class="lb" id="a30"></span></label>
<label>دبانس غیبت (ms)<input type="number" id="q31" step="50" min="100" max="5000"><span class="lb" id="a31"></span></label>
<label>دبانس بازیابی (ms)<input type="number" id="q32" step="50" min="100" max="5000"><span class="lb" id="a32"></span></label>
</div>
<div class="c4n" id="s2v"></div>

<div class="sec">۳) چشمک قرمز <span class="lb">(دوره / duty)</span></div>
<div class="sx">شکل چشمک قرمز همین سناریو.<br><br><b>دوره</b> = طول یک چرخهٔ روشن+خاموش<br><b>duty</b> = سهم روشنی از آن چرخه. عمداً از سناریو ۱ تندتر تنظیم شده تا دو خطا از روی چراغ قابل تشخیص باشند.</div>
<div class="bqr">
<label>دوره چشمک قرمز (ms)<input type="number" id="q44" step="50" min="100" max="10000"><span class="lb" id="a44"></span></label>
<label>duty قرمز (٪)<input type="number" id="q45" step="5" min="0" max="100"><span class="lb" id="a45"></span></label>
</div>
<div class="c4n" id="s2b"></div>

<div class="sec">۴) بوق</div>
<div class="sx">همان چهار عدد همیشگی، این‌بار برای این سناریو:<br><br><b>دوره</b> = فاصلهٔ تکرار الگو (۰ = بی‌صدا).<br><b>مدت هر بوق</b> = طول یک بوق.<br><b>تعداد بوق</b> = چند بوق در هر تکرار.<br><b>گپ</b> = سکوت بین آن بوق‌ها.<br><b>پیش‌فرض:</b> سه بوق کوتاه.<br><b>قانون مشترک بوق‌ها:</b> گپ فقط <i>بین</i> دو بوق معنی دارد، پس با «تعداد بوق = ۱» کادر گپ خودکار غیرفعال می‌شود و با دو بوق به بالا دوباره فعال.</div>
<div class="bqr">
<label>دوره بوق (ms، صفر=خاموش)<input type="number" id="q46" step="500" min="0" max="600000"><span class="lb" id="a46"></span></label>
<label>مدت هر بوق (ms)<input type="number" id="q47" step="50" min="0" max="600000"><span class="lb" id="a47"></span></label>
<label>تعداد بوق<input type="number" id="q48" step="1" min="0" max="10"><span class="lb" id="a48"></span></label>
<label>گپ بین بوق‌ها (ms)<input type="number" id="q49" step="50" min="0" max="5000"><span class="lb" id="a49"></span></label>
</div>
<div class="c4n" id="s2z"></div>

<div class="bqr"><button class="sb sb2 fwb" onclick="bdef()">بازگردانی پیش‌فرض کارخانهٔ سناریو ۲</button></div>

</div>
<div class="cd" id="ucard3" style="display:none">
<div class="hd"><b>سناریو ۳ — دشارژ، بی‌ورودی (سبز + باندهای بوق)</b></div>

<div class="ds c4ds">
<b>این سناریو کِی می‌آید؟</b> وقتی ورودی قطع است و بار روی باتری می‌رود (پایین‌ترین اولویت؛ هر آلارمی آن را کنار می‌زند).
<ul>
<li><b>سبز چشمک‌زن</b> — خاموشیِ سبز با مانده تا فول بزرگ می‌شود: باتری خالی‌تر ← سبز کم‌تر روشن. «حداقل خاموشی» نمی‌گذارد چشمک نزدیک ۱۰۰٪ دیده‌نشدنی شود.</li>
<li><b>چهار باند درصد</b> — هر باند با <b>سقف</b> خودش شروع می‌شود و تا سقف باند بالاتر ادامه دارد؛ برد همیشه ترتیب شروع ≥ دو-بوق ≥ سه-بوق ≥ بحرانی را نگه می‌دارد. بالای «شروع بوق» کاملاً بی‌صداست.</li>
<li><b>باند بحرانی</b> — LEDها خاموش و الگوی بوق فقط <b>یک‌بار</b> به‌اندازهٔ «طول یک‌باره» پخش می‌شود، بعد سکوت تا برگشت باتری. این یکی تکرار نمی‌شود.</li>
<li><b>پایداری</b> — درصد نمایش فقط وقتی حرکت می‌کند که از مقدار پایدار دست‌کم به‌اندازهٔ hysteresis دور شود؛ خروج از ۰٪ و ۱٪ آستانهٔ جدا دارند تا روی ته باتری بالا-پایین نپرد.</li>
</ul>
</div>

<div class="sim" id="sim3"><div class="simh"><b>شبیه‌ساز این سناریو</b><span class="lb">از روی همین کادرها ساخته می‌شود — چیزی از برد خوانده نمی‌شود</span><label class="lb">سرعت <select id="sims3" onchange="simspd(this.value)"><option value="1">×۱</option><option value="10">×۱۰</option><option value="60">×۶۰</option><option value="600">×۶۰۰</option></select></label><button class="sb sb2" id="simb3" onclick="simtog(3)">توقف</button></div><div class="simb"><span class="sl r" id="sl3r"></span><span class="sl g" id="sl3g"></span><span class="sl y" id="sl3y"></span><span class="szz" id="sl3z">🔇</span><span class="simt" id="sl3t"></span></div><div class="simc"><label>درصد باتری برای شبیه‌سازی<input type="range" id="simp3" min="0" max="100" value="50" oninput="simlbl(3)"><b id="simv3">50٪</b></label></div></div>
<div class="sec">۱) حد ولتاژ باتری — نردبانی که درصد این سناریو از آن می‌آید <span class="lb">(<span class="n">mV</span>؛ همان ۷۴/۷۵ کارت ۴ — هر جا عوض شود، هر دو جا عوض می‌شود)</span></div>
<div class="sx">درصد باتری مستقیم خوانده نمی‌شود؛ از یک نردبان ساده درمی‌آید:<br><br><b>حد پایین</b> = ولتاژی که ۰٪ حساب می‌شود.<br><b>حد بالا</b> = ولتاژی که ۱۰۰٪ حساب می‌شود.<br>بین این دو، درصد خطی تقسیم می‌شود.<br><b>توجه:</b> همهٔ باندهای بوق و چشمک سبز روی همین نردبان سوارند، پس تغییر آن همه را با هم جابه‌جا می‌کند.</div>
<div class="bqr">
<label>حد پایین — ۰٪ (mV)<input type="number" id="q74" step="100" min="15000" max="25000"><span class="lb" id="a74"></span></label>
<label>حد بالا — ۱۰۰٪ (mV)<input type="number" id="q75" step="100" min="25000" max="32000"><span class="lb" id="a75"></span></label>
</div>
<div class="c4n" id="s3m"></div>

<div class="sec">۲) چشمک سبز <span class="lb">(ms)</span></div>
<div class="sx">چراغ سبز در دشارژ، درصد باتری را نشان می‌دهد: هرچه باتری خالی‌تر، خاموشی بلندتر.<br><br><b>دوره</b> = طول یک چرخهٔ کامل<br><b>حداقل خاموشی</b> = کفِ زمان خاموشی تا در باتریِ پر هم یک چشمکِ دیدنی بماند.</div>
<div class="bqr">
<label>دوره چشمک سبز (ms)<input type="number" id="q66" step="50" min="100" max="10000"><span class="lb" id="a66"></span></label>
<label>حداقل خاموشی سبز (ms)<input type="number" id="q67" step="5" min="0" max="10000"><span class="lb" id="a67"></span></label>
</div>

<div class="sec">۳) باندها — همهٔ متغیرهای هر باند کنار هم
 <span class="lb">(از بالا به پایین؛ «مشترک» یعنی همان عدد در باند دیگری هم به‌کار می‌رود و با هم عوض می‌شوند)</span></div>
<div class="sx">چهار باند صدا، از پر به خالی. هر باند با چهار عدد خودش ساخته می‌شود:<br><br><b>سقف باند</b> = از این درصد پایین‌تر، این باند حاکم است.<br><b>تعداد بوق</b> = چند بوق در هر تکرار.<br><b>مدت هر بوق</b> = طول یک بوق.<br><b>فاصلهٔ تکرار</b> = هر چند وقت یک‌بار الگوی همین باند پخش شود.<br><b>گپ</b> = سکوت بین بوق‌ها؛ تنها عدد مشترک چهار باند است و در باند ۱ ویرایش می‌شود.<br><b>باند بحرانی</b> = هم‌شکل بقیه، فقط یک عدد اضافه دارد: <b>طول پخش یک‌باره</b>، چون یک‌بار پخش می‌شود و بعد تا برگشت باتری ساکت می‌ماند.<br><b>نکته:</b> هیچ باندی «درصد روشنی» دستی ندارد؛ برد خودش حساب می‌کند.</div>

<div class="bnd">
<div class="bnh"><b>باند ۱ — یک بوق</b><span class="lb" id="s3r1"></span></div>
<div class="bqr">
<label>شروع بوق — بالاتر از این درصد سکوت است (٪)<input type="number" id="q50" step="1" min="0" max="100"><span class="lb" id="a50"></span></label>
<label>سقف این باند (٪)<input type="number" id="q51" step="1" min="0" max="100"><span class="lb" id="a51"></span></label>
<label>تعداد بوق<input type="number" id="q62" step="1" min="0" max="10"><span class="lb" id="a62"></span></label>
<label>مدت هر بوق (ms)<input type="number" id="q59" step="50" min="0" max="600000"><span class="lb" id="a59"></span></label>
<label>فاصلهٔ تکرار (ms)<input type="number" id="q54" step="1000" min="0" max="600000"><span class="lb" id="a54"></span></label>
<label>گپ بین بوق‌ها (ms)<input type="number" id="q65" step="10" min="0" max="5000"><span class="lb" id="a65"></span></label>
</div>
<div class="c4n" id="s3q"></div>
<div class="c4n" id="s3n1"></div>
</div>

<div class="bnd">
<div class="bnh"><b>باند ۲ — دو بوق</b><span class="lb" id="s3r2"></span></div>
<div class="bqr">
<label>سقف این باند (٪)<input type="number" id="q52" step="1" min="0" max="100"><span class="lb" id="a52"></span></label>
<label>تعداد بوق<input type="number" id="q63" step="1" min="0" max="10"><span class="lb" id="a63"></span></label>
<label>مدت هر بوق (ms)<input type="number" id="q121" step="50" min="0" max="600000"><span class="lb" id="a121"></span></label>
<label>فاصلهٔ تکرار (ms)<input type="number" id="q122" step="1000" min="0" max="600000"><span class="lb" id="a122"></span></label>
<div class="shv"><span class="t">گپ بین بوق‌ها (ms)</span><b class="qmv" data-q="65"></b><span class="ow">مشترک · ویرایش در باند ۱</span></div>
</div>
<div class="c4n" id="s3n2"></div>
</div>

<div class="bnd">
<div class="bnh"><b>باند ۳ — سه بوق</b><span class="lb" id="s3r3"></span></div>
<div class="bqr">
<label>سقف این باند (٪)<input type="number" id="q53" step="1" min="0" max="100"><span class="lb" id="a53"></span></label>
<label>تعداد بوق<input type="number" id="q64" step="1" min="0" max="10"><span class="lb" id="a64"></span></label>
<label>مدت هر بوق (ms)<input type="number" id="q60" step="50" min="0" max="600000"><span class="lb" id="a60"></span></label>
<label>فاصلهٔ تکرار (ms)<input type="number" id="q55" step="1000" min="0" max="600000"><span class="lb" id="a55"></span></label>
<div class="shv"><span class="t">گپ بین بوق‌ها (ms)</span><b class="qmv" data-q="65"></b><span class="ow">مشترک · ویرایش در باند ۱</span></div>
</div>
<div class="c4n" id="s3n3"></div>
</div>

<div class="bnd bndc">
<div class="bnh"><b>باند بحرانی — فقط یک‌بار پخش می‌شود</b><span class="lb" id="s3r4"></span></div>
<div class="bqr">
<label>تعداد بوق<input type="number" id="q58" step="1" min="0" max="10"><span class="lb" id="a58"></span></label>
<label>مدت هر بوق (ms)<input type="number" id="q57" step="50" min="0" max="600000"><span class="lb" id="a57"></span></label>
<label>فاصلهٔ تکرار (ms)<input type="number" id="q56" step="500" min="0" max="600000"><span class="lb" id="a56"></span></label>
<label>طول پخش یک‌باره (ms)<input type="number" id="q61" step="500" min="0" max="120000"><span class="lb" id="a61"></span></label>
<div class="shv"><span class="t">گپ بین بوق‌ها (ms)</span><b class="qmv" data-q="65"></b><span class="ow">مشترک · ویرایش در باند ۱</span></div>
</div>
<div class="c4n" id="s3n4"></div>
</div>

<div class="c4n" id="s3z"></div>

<div class="sec">۴) پایداری درصد — ضدلرزشِ خودِ عدد <span class="lb">(٪)</span></div>
<div class="sx">این سه عدد روی خودِ «درصد» کار می‌کنند نه روی چراغ:<br><br><b>hysteresis</b> = تا درصد این اندازه تکان نخورد، عدد نمایش‌داده‌شده عوض نمی‌شود.<br><b>خروج از ۰٪</b> = برای بیرون‌آمدن از حالت صفر، درصد باید دست‌کم به این عدد برسد.<br><b>خروج از ۱٪</b> = همان قاعده برای حالت یک‌درصد.<br><b>دلیل:</b> بدون این‌ها نویزِ یک‌درصدی، باند بوق و چشمک را مدام عوض می‌کرد.</div>
<div class="c4f" id="s3hy"></div>
<div class="bqr">
<label>hysteresis پایداری دشارژ (٪)<input type="number" id="q80" step="1" min="0" max="50"><span class="lb" id="a80"></span></label>
<label>خروج از حالت ۰٪ (٪)<input type="number" id="q81" step="1" min="0" max="100"><span class="lb" id="a81"></span></label>
<label>خروج از حالت ۱٪ (٪)<input type="number" id="q82" step="1" min="0" max="100"><span class="lb" id="a82"></span></label>
</div>
<div class="c4f">سبز: مانده = ۱۰۰ − درصد → گام = دوره ÷ ۱۰۰ → خاموش = بیشینهٔ («حداقل خاموشی»، مانده × گام) → روشن = دوره − خاموش. بوق هر باند: پنجره = مدت × تعداد + گپ × (تعداد − ۱) و باید در «فاصلهٔ تکرار» همان باند جا شود، وگرنه برد آن باند را بی‌صدا می‌گذارد.</div>

<div class="bqr"><button class="sb sb2 fwb" onclick="dsdef()">بازگردانی پیش‌فرض کارخانهٔ سناریو ۳</button></div>
</div>
<div class="cd" id="ucard4" style="display:none">
<div class="hd"><b>سناریو ۴ — شارژ عادی (زرد + فول)</b></div>

<div class="ds c4ds">
<b>فول یعنی چه؟</b> برد دو راه مستقل برای «فول» دارد و هرکدام زودتر برسد، چهرهٔ سبزِ ثابت را می‌آورد:
<ul>
<li><b>فول شارژری</b> — هر کانالِ فعال کارش تمام شده و در FLOAT است (Taper زیر ۵۰mA یا سقف ۱ ساعت Absorb). این یکی عدد تنظیمی ندارد و از خود شارژر می‌آید.</li>
<li><b>فول ولتاژی</b> — درصد باتری (که از حد پایین/بالای همین کارت ساخته می‌شود) به <b>ورود فول</b> برسد.</li>
</ul>
<b>ورود فول (٪)</b> = درصدی که با <b>رسیدن به آن یا بالاتر</b>، حالت از «در حال شارژ» به «فول» می‌پرد: زرد خاموش، سبز ثابت.<br>
<b>خروج فول (٪)</b> = تا وقتی «فول» روشن است، فقط با <b>افتادن درصد زیر این عدد</b> دوباره به «در حال شارژ» برمی‌گردد. این فاصله hysteresis (فاصلهٔ ضدلرزش) است و نمی‌گذارد LED روی مرز بلرزد؛ برد همیشه خروج را دست‌کم یک واحد زیر ورود گیره می‌زند.
</div>

<div class="sim" id="sim4"><div class="simh"><b>شبیه‌ساز این سناریو</b><span class="lb">از روی همین کادرها ساخته می‌شود — چیزی از برد خوانده نمی‌شود</span><label class="lb">سرعت <select id="sims4" onchange="simspd(this.value)"><option value="1">×۱</option><option value="10">×۱۰</option><option value="60">×۶۰</option><option value="600">×۶۰۰</option></select></label><button class="sb sb2" id="simb4" onclick="simtog(4)">توقف</button></div><div class="simb"><span class="sl r" id="sl4r"></span><span class="sl g" id="sl4g"></span><span class="sl y" id="sl4y"></span><span class="szz" id="sl4z">🔇</span><span class="simt" id="sl4t"></span></div><div class="simc"><label>درصد باتری برای شبیه‌سازی<input type="range" id="simp4" min="0" max="100" value="50" oninput="simlbl(4)"><b id="simv4">50٪</b></label></div></div>
<div class="sec">۱) حد ولتاژ باتری — نردبان درصدِ شارژ <span class="lb">(mV؛ مستقل از نردبان دشارژ — v1.49)</span></div>
<div class="ds">از نسخهٔ ۱٫۴۹ (دستور کاربر) این دو عدد <b>فقط مال سمت شارژ</b>اند: درصدی که حین شارژ نشان داده می‌شود و نقطهٔ «فول» (۷۷/۷۸) از همین‌ها ساخته می‌شود. نردبان <b>دشارژ</b> جفت جداگانهٔ خودش را دارد (کارت ۳ · شناسه‌های ۷۴/۷۵) و دیگر با این دو تکان نمی‌خورد. پیش‌فرض هر دو جفت یکی است، پس تا وقتی خودتان عوض نکنید هیچ رفتاری تغییر نمی‌کند.</div>
<div class="bqr">
<label>حد پایین — ۰٪ (mV)<input type="number" id="q119" step="100" min="15000" max="25000"><span class="lb" id="a119"></span></label>
<label>حد بالا — ۱۰۰٪ (mV)<input type="number" id="q120" step="100" min="25000" max="32000"><span class="lb" id="a120"></span></label>
<div class="shv"><span class="t">نردبان دشارژ (۷۴/۷۵)</span><b class="qmv" data-q="74"></b><span class="ow">فقط برای مقایسه · ویرایش در کارت ۳ · دشارژ</span></div>
</div>
<div class="c4n" id="c4map"></div>

<div class="sec">۲) فول <span class="lb">(٪؛ ورود همیشه بالای خروج)</span></div>
<div class="sx"><b>ورود فول</b> = با رسیدن درصد به این عدد، چهرهٔ «فول» می‌آید (زرد خاموش، سبز ثابت).<br><b>خروج فول</b> = تا درصد زیر این عدد نیفتد، از حالت فول بیرون نمی‌آید.<br><b>دلیل:</b> فاصلهٔ این دو عدد همان ضدلرزشِ فول است.</div>
<div class="bqr">
<label>ورود فول (٪)<input type="number" id="q77" step="1" min="1" max="100"><span class="lb" id="a77"></span></label>
<label>خروج فول (٪)<input type="number" id="q78" step="1" min="0" max="100"><span class="lb" id="a78"></span></label>
</div>
<div class="c4n" id="c4full"></div>

<div class="sec">۳) چشمک زرد و پایداری <span class="lb">(ms / ٪)</span></div>
<div class="sx"><b>دوره چشمک زرد</b> = طول یک چرخهٔ کامل چشمک حین شارژ.<br><b>حداقل روشنی</b> = کفِ زمان روشنی تا نزدیک فول هم یک چشمک دیده شود.<br><b>hysteresis پایداری شارژ</b> = درصد سمت شارژ تا این اندازه تکان نخورد، نمایش عوض نمی‌شود.</div>
<div class="bqr">
<label>دوره چشمک زرد (ms)<input type="number" id="q68" step="50" min="100" max="10000"><span class="lb" id="a68"></span></label>
<label>حداقل روشنی زرد (ms)<input type="number" id="q69" step="5" min="0" max="10000"><span class="lb" id="a69"></span></label>
<label>hysteresis پایداری شارژ (٪)<input type="number" id="q79" step="1" min="0" max="50"><span class="lb" id="a79"></span></label>
</div>

<div class="sec">۴) اعداد چشمک — زرد با این اعداد می‌زند</div>
<div class="sx">این بخش عدد تنظیمی ندارد؛ فقط نشان می‌دهد برد با همان اعداد بالا در هر درصد چه روشن/خاموشی‌ای می‌سازد — برای مچ‌کردن حس چراغ با عددها پیش از ثبت.</div>
<div class="c4f">مانده تا فول = ۱۰۰ − درصد پایدار (کف ۲٪) → گام = دوره ÷ ۱۰۰ → روشن = مانده × گام (کف «حداقل روشنی»، سقف دوره) → خاموش = دوره − روشن. یعنی باتری هرچه پرتر، چشمکِ زرد کوتاه‌تر.</div>
<div class="srvw"><table class="srv c4tb"><thead><tr><th>درصد پایدار</th><th>ولتاژ تقریبی</th><th>مانده تا فول</th><th>زرد روشن</th><th>زرد خاموش</th><th>رفتار</th></tr></thead><tbody id="c4tb"></tbody></table></div>

<div class="bqr"><button class="sb sb2 fwb" onclick="chdef()">بازگردانی پیش‌فرض کارخانهٔ سناریو ۴</button></div>
</div>
<div class="cd" id="ucard5" style="display:none">
<div class="hd"><b>سناریو ۵ — عدم‌توازن دو نیم‌باتری (قفل دائمی)</b></div>

<div class="ds c4ds">
<b>این سناریو کِی می‌آید؟</b> وقتی اختلاف دو نیم‌باتری بارها بالا برود — یعنی یکی از دو نیم دارد خراب می‌شود. پنج گام پشت سر هم:
<ul>
<li><b>اندازه</b> — قدرمطلق اختلاف دو نیم.</li>
<li><b>گیت زمانی</b> — حین استراحت و شارژ، ولتاژ هنوز نشسته نیست؛ پس اندازه فقط بعد از «صبر» شمرده می‌شود. حین دشارژ گیتی نیست و حد دشارژ (معمولاً بالاتر) فوراً کار می‌کند.</li>
<li><b>رویداد</b> — ماندنِ بالای حد به اندازهٔ «پایداری رویداد» = یک رویداد. اپیزود تا وقتی اختلاف به اندازهٔ hysteresis پایین نیاید تمام‌شده حساب نمی‌شود، پس یک خرابی طولانی چند بار شمرده نمی‌شود.</li>
<li><b>قفل دائمی</b> — با رسیدن شمارندهٔ ماندگار به «سقف رویداد»: قرمز <b>چشمک‌زن</b> + بوق دوره‌ای. (چشمک به دستور کاربر در ۱۴۰۴/۲۰۲۶-۱۰-۰۵ جای قرمز ثابت را گرفت: چراغ ثابت شبیه «لامپی که یادشان رفته خاموش کنند» دیده می‌شد.) قفل روی فلش برد می‌ماند و با ریست پاک نمی‌شود؛ تنها راه خروج، تعویض باتری (۳ ثانیه بی‌باتری) است.</li>
<li><b>بعد از قفل</b> — شارژ همچنان مجاز است ولی سیکل‌هایش شمرده می‌شود؛ از «سیکل تا مسدودی» به بعد شارژ هم قطع می‌شود. مسدودی خروجی جدا قابل خاموش/روشن است.</li>
</ul>
</div>

<div class="prod">اختلاف الان: <b id="imbv"></b> <span class="lb">mV</span> · رویدادها: <b id="imbev"></b> <span id="imbmx" class="lb">از ۱۰</span> · وضعیت: <b id="imbst"></b></div>

<div class="sim" id="sim5"><div class="simh"><b>شبیه‌ساز این سناریو</b><span class="lb">از روی همین کادرها ساخته می‌شود — چیزی از برد خوانده نمی‌شود</span><label class="lb">سرعت <select id="sims5" onchange="simspd(this.value)"><option value="1">×۱</option><option value="10">×۱۰</option><option value="60">×۶۰</option><option value="600">×۶۰۰</option></select></label><button class="sb sb2" id="simb5" onclick="simtog(5)">توقف</button></div><div class="simb"><span class="sl r" id="sl5r"></span><span class="sl g" id="sl5g"></span><span class="sl y" id="sl5y"></span><span class="szz" id="sl5z">🔇</span><span class="simt" id="sl5t"></span></div><div class="simc"><label>حالت کاری <select id="sim5m" onchange="sim5mode()"><option value="c">در حال شارژ</option><option value="r" selected>استراحت (بعد از شارژ)</option><option value="d">دشارژ روی باتری</option></select></label> <span class="lb" id="sim5w"></span></div><div class="simc"><label>نیم‌باتری بالا (mV)<input type="range" id="simp5a" min="9000" max="15000" value="12000" oninput="simlbl5()"><b id="simv5a">12000mV</b></label></div><div class="simc"><label>نیم‌باتری پایین (mV)<input type="range" id="simp5b" min="9000" max="15000" value="12000" oninput="simlbl5()"><b id="simv5b">12000mV</b></label> <button class="sb sb2" onclick="simrst5()">شروع دوبارهٔ شمارش</button></div></div>
<div class="sec">۱) حد اختلاف <span class="lb">(mV)</span></div>
<div class="sx">معیارِ سنجش، قدرمطلق اختلاف ولتاژ دو نیم‌باتری است.<br><br><b>حد استراحت</b> = حدِ مجاز وقتی دستگاه در استراحت یا شارژ است (ولتاژ نشسته، پس حد سخت‌گیرانه‌تر).<br><b>حد دشارژ</b> = همان حد وقتی روی بار کار می‌کند.<br><b>دلیل:</b> چون افت روی بار طبیعی است، این عدد معمولاً بزرگ‌تر است. از نسخهٔ ۱٫۷۲ مقایسه فقط وقتی انجام می‌شود که هر دو نیم در یک حالت باشند.</div>
<div class="bqr">
<label>حد استراحت عدم‌توازن<input type="number" id="q108" step="50" min="0" max="2000"><span class="lb" id="a108"></span></label>
<label>حد دشارژ عدم‌توازن<input type="number" id="q109" step="50" min="0" max="2000"><span class="lb" id="a109"></span></label>
</div>

<div class="sec">۲) گیت زمانی و پایداری <span class="lb">(ms / mV)</span></div>
<div class="sx">این چهار عدد جلوی قضاوت زودهنگام را می‌گیرند:<br><br><b>صبر پس از پایان شارژ</b> = تا این مدت نگذرد، سنجش انجام نمی‌شود (۰ = این حالت اصلاً سنجیده نشود).<br><b>صبر پس از شروع شارژ</b> = همان قاعده برای ابتدای شارژ.<br><b>دلیل:</b> درست بعد از شارژ یا دشارژ، ولتاژ هنوز ننشسته و اختلافِ دیده‌شده واقعی نیست.<br><b>پایداری رویداد</b> = اختلاف باید این‌قدر پیوسته بالای حد بماند تا یک رویداد ثبت شود.<br><b>بازگشت (<span class="n">hysteresis</span>)</b> = رویداد تا وقتی اختلاف این‌قدر پایین نیاید بسته نمی‌شود، تا یک خرابی طولانی چندبار شمرده نشود.</div>
<div class="bqr">
<label>صبر پس از پایان شارژ (ms)<input type="number" id="q110" step="60000" min="0" max="3600000"><span class="lb" id="a110"></span></label>
<label>صبر پس از شروع شارژ (ms، ۰=خاموش)<input type="number" id="q111" step="60000" min="0" max="3600000"><span class="lb" id="a111"></span></label>
<label>پایداری رویداد (ms)<input type="number" id="q112" step="1000" min="1000" max="600000"><span class="lb" id="a112"></span></label>
<label>Hysteresis رویداد (mV)<input type="number" id="q113" step="50" min="0" max="1000"><span class="lb" id="a113"></span></label>
</div>
<div class="c4n" id="s5v"></div>

<div class="sec">۳) قضاوت و رفتار پس از قفل</div>
<div class="sx"><b>سقف رویداد تا قفل</b> = با رسیدن شمارندهٔ ماندگار به این عدد، باتری محکوم و قفل می‌شود.<br><b>سیکل‌های شارژ پس از قفل</b> = بعد از قفل این‌قدر سیکل شارژ هنوز مجاز است و بعد شارژ هم می‌ایستد.<br><b>مسدودی خروجی</b> = چک‌باکسِ پایین این بخش.<br><b>تیک‌دار یعنی:</b> باتریِ قفل‌شده علاوه بر شارژ، از مسیر خروجی هم برداشته می‌شود و بار دیگر از آن تغذیه نمی‌کند.</div>
<div class="bqr">
<label>سقف رویداد تا قفل<input type="number" id="q114" step="1" min="1" max="255"><span class="lb" id="a114"></span></label>
<label>سیکل‌های شارژ پس از قفل تا مسدودی<input type="number" id="q118" step="1" min="1" max="255"><span class="lb" id="a118"></span></label>
</div>
<div class="bqr"><label class="ckr"><input type="checkbox" id="ib117"><span><span class="ckt">پس از قفل، باتریِ محکوم از خروجی هم جدا شود (مسدودی خروجی)</span><span class="cks" id="a117"></span></span></label></div>

<div class="sec">۴) چراغ و بوقِ هشدار در قفل</div>
<div class="sx">چهرهٔ هشدار بعد از قفل فقط از همین چهار عدد ساخته می‌شود.<br><br><b>دورهٔ بوق</b> = هر چند وقت یک‌بار بوق تکرار شود (۰ = بی‌صدا) و <b>طول بوق</b> = طول همان بوق.<br><b>دورهٔ چشمک قرمز</b> = یک دور کامل روشن و خاموش (۰ = قرمز ثابت) و <b>سهم روشنی</b> = چند درصد از آن دوره چراغ روشن باشد (با دورهٔ ۰ بی‌اثر است). سناریو ۶ اعداد جدای خودش را دارد، پس این چهار عدد فقط به قفلِ عدم‌توازن مربوط‌اند.</div>
<div class="bqr">
<label>دورهٔ بوق در قفل (ms، ۰=خاموش)<input type="number" id="q115" step="60000" min="0" max="86400000"><span class="lb" id="a115"></span></label>
<label>طول بوق (ms)<input type="number" id="q116" step="10" min="20" max="2000"><span class="lb" id="a116"></span></label>
<label>دورهٔ چشمک قرمز در قفل (ms، ۰=ثابت)<input type="number" id="q123" step="100" min="0" max="10000"><span class="lb" id="a123"></span></label>
<label>سهم روشنی چشمک (٪)<input type="number" id="q124" step="5" min="5" max="95"><span class="lb" id="a124"></span></label>
</div>
<div class="c4n" id="s5z"></div>

<div class="bqr"><button class="sb sb2 fwb" onclick="ibdef()">بازگردانی پیش‌فرض کارخانهٔ سناریو ۵</button></div>
</div>
<div class="cd" id="ucard6" style="display:none">
<div class="hd"><b>سناریو ۶ — باتری خراب</b></div>

<div class="ds c4ds">
<b>این سناریو کِی می‌آید؟</b> وقتی باتری ساعت‌ها جریان می‌گیرد ولی هرگز به پایان شارژ نمی‌رسد — یعنی سلول مرده یا اتصالی داخلی. دستور کاربر: «باتری نباید دائم زیر شارژ بماند.»
<ul>
<li><b>چه چیزی شمرده می‌شود</b> — فقط زمانی که کانال واقعاً در حال پمپ‌کردن است (Bulk/Absorb). خاموشیِ شارژر زمان اضافه نمی‌کند.</li>
<li><b>چه چیزی ساعت را صفر می‌کند</b> — رسیدن به Float (شارژ کامل شد) یا یک وقفهٔ بلندتر از «مهلت وقفه». وقفهٔ کوتاه ساعت را نگه می‌دارد تا باتریِ لرزان با روشن/خاموش‌شدن مدام از حکم فرار نکند.</li>
<li><b>حکم</b> — با رسیدن به «مهلت شارژ» (پیش‌فرض ۲۴ ساعت): قرمز <b>ثابت</b> (عمداً ثابت، تا با چشمکِ قفل عدم‌توازن اشتباه نشود) + همان بوق قفل، و شارژ آن کانال قطعِ قطع.</li>
<li><b>تفاوتش با قطعِ باتری کم</b> — قطعِ باتری کم (داخل چنج‌اور، ثابت ۲۱۰۰۰/۲۱۲۰۰ میلی‌ولت) یک واکنشِ لحظه‌ای به ولتاژ است و با شارژ شدن خودبه‌خود برمی‌گردد؛ این یکی یک حکمِ ماندگار دربارهٔ سلامتِ خودِ باتری است و فقط با تعویض باتری پاک می‌شود.</li>
</ul>
</div>

<div class="prod">شارژ پیوستهٔ باتری ۱: <b id="dbel1"></b> · شارژ پیوستهٔ باتری ۲: <b id="dbel2"></b> · وضعیت: <b id="dbst"></b></div>
<div class="sx">هر باتری ساعت خودش را دارد و این دو عدد مستقل‌اند: عددِ هر باتری فقط وقتی بالا می‌رود که همان کانال در حال شارژ باشد، و به‌محضِ کامل‌شدنِ شارژِ همان باتری (رسیدن به Float) ساعتِ همان یکی صفر می‌شود — کانال دیگر دست‌نخورده می‌ماند. وقفهٔ کوتاه‌تر از «مهلت وقفه» ساعت را نگه می‌دارد و وقفهٔ بلندتر صفرش می‌کند.</div>

<div class="sim" id="sim6"><div class="simh"><b>شبیه‌ساز این سناریو</b><span class="lb">از روی همین کادرها ساخته می‌شود — چیزی از برد خوانده نمی‌شود</span><label class="lb">سرعت <select id="sims6" onchange="simspd(this.value)"><option value="1">×۱</option><option value="10">×۱۰</option><option value="60">×۶۰</option><option value="600">×۶۰۰</option></select></label><button class="sb sb2" id="simb6" onclick="simtog(6)">توقف</button></div><div class="simb"><span class="sl r" id="sl6r"></span><span class="sl g" id="sl6g"></span><span class="sl y" id="sl6y"></span><span class="szz" id="sl6z">🔇</span><span class="simt" id="sl6t"></span></div><div class="simc"><label>حالت کانال <select id="sim6m"><option value="c" selected>در حال شارژ (Bulk/Absorb)</option><option value="p">وقفهٔ شارژ</option><option value="f">رسید به Float (شارژ کامل)</option></select></label> <button class="sb sb2" onclick="simrst6()">شروع دوبارهٔ شمارش</button></div></div>

<div class="sec">۱) مهلت‌ها <span class="lb">(ms)</span></div>
<div class="sx"><b>مهلت شارژ پیوسته</b> = اگر یک کانال این‌قدر پیوسته شارژ کند و هرگز به پایان شارژ (Float) نرسد، حکم «باتری خراب» صادر می‌شود.<br><b>پیش‌فرض:</b> ۲۴ ساعت · <b>۰ یعنی:</b> این حکم خاموش.<br><b>مهلت وقفه</b> = وقفهٔ کوتاه‌تر از این مقدار، ساعت را فقط نگه می‌دارد؛ وقفهٔ بلندتر آن را صفر می‌کند.<br><b>دلیل:</b> بدون این عدد، باتریِ لرزان با روشن/خاموش‌شدن مدام برای همیشه از حکم فرار می‌کرد.</div>
<div class="bqr">
<label>مهلت شارژ پیوسته تا حکم خرابی (ms، ۰=خاموش)<input type="number" id="q125" step="3600000" min="0" max="172800000"><span class="lb" id="a125"></span></label>
<label>مهلت وقفه‌ای که ساعت را صفر می‌کند (ms)<input type="number" id="q126" step="60000" min="0" max="3600000"><span class="lb" id="a126"></span></label>
</div>
<div class="c4n" id="s6v"></div>

<div class="sec">۲) چراغ و بوقِ این سناریو</div>
<div class="sx">تا نسخهٔ ۱٫۷۹ این سناریو چهرهٔ خودش را نداشت: قرمزش در کد ثابت بود و بوقش را از قفلِ عدم‌توازن (۱۱۵/۱۱۶) قرض می‌گرفت. از <span class="n">v1.80</span> چهار عدد مستقل دارد:<br><br><b>دورهٔ بوق</b> = فاصلهٔ تکرار بوق (۰ = بی‌صدا).<br><b>طول هر بوق</b> = مدت خودِ بوق.<br><b>دورهٔ چشمک قرمز</b> = طول یک چرخهٔ روشن+خاموش (۰ = قرمز ثابت).<br><b>سهم روشنی</b> = چند درصد از آن چرخه چراغ روشن باشد.<br><b>پیش‌فرض:</b> همان رفتار قبلی — بوق هر ۱۰ دقیقه به طول <span class="n">۱۲۰ms</span> و قرمز ثابت؛ تا دست نزنید چیزی عوض نمی‌شود.<br><b>توجه:</b> همین قرمزِ ثابت است که این حکم را از چشمکِ قفلِ عدم‌توازن جدا می‌کند؛ اگر چشمک را روشن کنید، دو هشدار شبیه هم می‌شوند.</div>
<div class="bqr">
<label>دورهٔ بوق پس از حکم (ms، ۰=خاموش)<input type="number" id="q128" step="60000" min="0" max="86400000"><span class="lb" id="a128"></span></label>
<label>طول هر بوق (ms)<input type="number" id="q129" step="10" min="20" max="2000"><span class="lb" id="a129"></span></label>
<label>دورهٔ چشمک قرمز (ms، ۰=ثابت)<input type="number" id="q130" step="100" min="0" max="10000"><span class="lb" id="a130"></span></label>
<label>سهم روشنی چشمک (٪)<input type="number" id="q131" step="5" min="5" max="95"><span class="lb" id="a131"></span></label>
</div>
<div class="c4n" id="s6b"></div>

<div class="sec">۳) رفتار پس از حکم خرابی</div>
<div class="sx"><b>«جداسازی باتری از خروجی» یعنی چه؟</b> هر باتری دو مسیر دارد: مسیرِ <b>شارژ</b> (برق به باتری می‌رود) و مسیرِ <b>خروجی</b> (باتری به بار برق می‌دهد). وقتی حکم «باتری خراب» صادر شد، مسیر شارژِ آن کانال در هر حالت و برای همیشه قطع است — این قابل انتخاب نیست. تنها چیزی که این چک‌باکس تعیین می‌کند، تکلیفِ مسیر <b>خروجی</b> است:
<ul>
<li><b>تیک‌دار (پیش‌فرض خاموش است، خودتان روشن می‌کنید)</b> — باتریِ محکوم از خروجی هم برداشته می‌شود: نه شارژ می‌گیرد و نه به بار برق می‌دهد. انگار از مدار درآمده و فقط منتظر تعویض است. امن‌ترین حالت برای باتریِ مشکوک به اتصالیِ داخلی، ولی اگر همان یک باتری تنها منبع بار باشد، بار از دست می‌رود.</li>
<li><b>بدون تیک (پیش‌فرض)</b> — فقط شارژ قطع می‌شود؛ باتری سرِ جایش می‌ماند و تا وقتی ولتاژ دارد به بار برق می‌دهد و بعد خالی می‌شود. هشدار (قرمز ثابت + بوق) در هر دو حالت یکسان است.</li>
</ul>
این تیک روی فلش برد ذخیره می‌شود و دقیقاً همتای «مسدودی خروجی» در سناریو ۵ است.</div>
<div class="bqr"><label class="ckr"><input type="checkbox" id="db127"><span><span class="ckt">پس از حکم خرابی، باتری از خروجی هم جدا شود (نه شارژ، نه تغذیهٔ بار)</span><span class="cks" id="a127"></span></span></label></div>
<div class="ds c4ds"><b>چک‌لیست — لحظه‌به‌لحظه چه اتفاقی می‌افتد؟</b>
<ul>
<li><b>۱ · شمارش</b> — تا کانال در حال شارژ (Bulk یا Absorb) است، ساعتِ همان کانال جلو می‌رود. هر باتری ساعت خودش را دارد.</li>
<li><b>۲ · صفر شدن</b> — شارژ به Float رسید؟ یعنی باتری پر شد و سالم است: ساعتِ همان کانال صفر می‌شود. وقفهٔ بلندتر از «مهلت وقفه» هم ساعت را صفر می‌کند؛ وقفهٔ کوتاه فقط نگهش می‌دارد.</li>
<li><b>۳ · حکم</b> — ساعت به «مهلت شارژ پیوسته» (پیش‌فرض ۲۴ ساعت) رسید بدون اینکه هیچ‌وقت پر شود: باتری خراب اعلام می‌شود.</li>
<li><b>۴ · واکنش</b> — چراغ قرمز <b>ثابت</b> + بوق دوره‌ای، و شارژِ همان کانال برای همیشه قطع. کانال دیگر دست‌نخورده کار می‌کند.</li>
<li><b>۵ · خروجی</b> — فقط اگر چک‌باکسِ بالا تیک داشته باشد، باتری از خروجی هم جدا می‌شود؛ وگرنه همچنان بار را تغذیه می‌کند.</li>
<li><b>۶ · خروج از حکم</b> — با ریست، قطع برق یا تغییر تنظیمات پاک نمی‌شود (روی فلش برد، اسلات ۲۰۳). تنها راه: برداشتن باتری به مدت ۳ ثانیه و گذاشتن باتری نو.</li>
</ul>
</div>

<div class="bqr"><button class="sb sb2 fwb" onclick="dbdef()">بازگردانی پیش‌فرض کارخانهٔ سناریو ۶</button></div>
</div>
<input type="hidden" id="q76" value="">
</div>
<div class="sgx" id="s2">
<div class="cd">
<div class="hd"><b>پنجرهٔ ورودی سالم</b><span class="lb">· شناسه‌های ۳۳..۳۴ · روی فلش برد ذخیره می‌شود</span></div>
<div class="sx">تشخیص «ورودی حاضر» فقط داخل این پنجره انجام می‌شود:<br><br><b>کف</b> = پایین‌ترین ولتاژ ورودیِ قابل‌قبول (پیش‌فرض ۲۱۰۰۰mV).<br><b>سقف</b> = بالاترین ولتاژ ورودیِ قابل‌قبول (پیش‌فرض ۲۸۰۰۰mV).<br>این دو همیشه دست‌کم ۱۰۰۰mV از هم فاصله دارند.<br><b>بیرون پنجره:</b> شارژر کار نمی‌کند و منتظر ورودی می‌ماند.</div>
<div class="bqr">
<label>کف ورودی سالم (mV)<input type="number" id="q33" step="100" min="18000" max="24000"><span class="lb" id="a33"></span></label>
<label>سقف ورودی سالم (mV)<input type="number" id="q34" step="100" min="24000" max="30000"><span class="lb" id="a34"></span></label>
</div>

</div>
<div class="cd">
<div class="hd"><b>سقف‌های ایمنی شارژر</b><span class="lb">· شناسه‌های ۳۵..۳۷ · فقط پایین‌بردنی — هرگز بالای سقف کارخانه نمی‌روند · روی فلش برد ذخیره می‌شود</span></div>
<div class="sx"><b>Hard fault جریان</b> = بالای این مقدار کانال فوراً متوقف می‌شود (همیشه بالای جریان Bulk+۵۰ نگه داشته می‌شود تا تنظیم سالم تریپ نکند).<br><b>قطع OV</b> = بالای این ولتاژ، باتری نامعتبر و سوئیچینگ متوقف می‌شود.<br><b>کف اعتبار</b> = زیر این ولتاژ، عدد باتری اصلاً معتبر شمرده نمی‌شود.<br><b>قانون مشترک:</b> هر سه فقط پایین‌بردنی‌اند و هرگز از سقف کارخانه بالاتر نمی‌روند.</div>
<div class="bqr">
<label>Hard fault جریان (mA)<input type="number" id="q35" step="10" min="150" max="950"><span class="lb" id="a35"></span></label>
<label>قطع اضافه‌ولتاژ OV (mV)<input type="number" id="q36" step="50" min="14000" max="15000"><span class="lb" id="a36"></span></label>
<label>کف اعتبار باتری (mV)<input type="number" id="q37" step="100" min="0" max="8000"><span class="lb" id="a37"></span></label>
</div>

<div class="bqr"><button class="sb sb2" onclick="adef()">بازگردانی پیش‌فرض کارخانهٔ نظارت و ایمنی</button></div>
</div>
</div>
<div class="sgx" id="s3">
<!-- [EN] v1.34 (user order 2026-10-03): current calibration and the
     current filters got their own Settings sub-tab, so the charge
     sub-tab keeps only the chart + PID. The STAB handler toggles .sgx
     by DOM index. v1.34 made it the 5th .sgx (index 4); v1.42 (user
     order: backup sub-tab LAST) moves it to index 3, before s4=backup.
     [FA] v1.34 (دستور کاربر): کالیبراسیون جریان و فیلتر جریان به زیرتب
     مستقل تنظیمات آمدند تا زیرتب شارژ فقط نمودار + PID بماند. چون
     شنوندهٔ STAB با ترتیب DOM سوییچ می‌کند: v1.42 (دستور کاربر:
     پشتیبان‌گیری آخرین زیرتب) این بلوک را به اندیس ۳، قبل از s4، منتقل کرد. -->
<div class="cd">
<div class="hd"><b>فیلتر جریان</b><span class="lb">· مشترک هر دو کانال · Median + Average · مثل بقیه روی فلش برد ذخیره می‌شود</span></div>
<div class="sx">دو مرحلهٔ پشت‌سرهم روی عدد جریان:<br><br><b>پنجرهٔ median</b> = پالس‌های تکیِ پرت را حذف می‌کند (۱..۱۵؛ ۱ و ۲ = خاموش).<br><b>پنجرهٔ میانگین</b> = خروجی مرحلهٔ قبل را صاف می‌کند (۱..۳۰۰؛ ۱ = خاموش).<br>هر نمونه ۱ms است، پس مجموع این دو عدد یعنی چند میلی‌ثانیه تاریخچه.<br><b>هشدار:</b> در مود خودکارِ شارژر مجموع را بالای ~۵۰ نبرید؛ حلقهٔ تنظیم کند می‌شود.</div>
<div class="bqr">
<label>پنجرهٔ median<input type="number" id="q7" step="1" min="1" max="15"><span class="lb" id="a7"></span></label>
<label>پنجرهٔ میانگین (average)<input type="number" id="q8" step="1" min="1" max="300"><span class="lb" id="a8"></span></label>
</div>

</div>
<div class="cd">
<div class="hd"><b>کالیبراسیون جریان</b><span class="lb">· شناسه ۰..۳ و ۹..۱۰ · روی فلش برد ذخیره می‌شود</span></div>
<div class="sx"><b>آفست</b> = عدد ADC در جریان صفر که از هر نمونه کم می‌شود.<br><b>گین</b> = ضریب تبدیل شمارش به جریان.<br><b>ETA</b> = ضریب تبدیل جریان ورودی به جریان سمت باتری (صفر = بدون تبدیل).<br><b>هشدار کانال ۲:</b> این کانال جدول بنچ دارد، پس ETA آن صفر بماند؛ تغییر آفست یا گینِ کانال ۲ یعنی جدول بنچ باید دوباره ساخته شود.</div>
<div class="bqr">
<label>آفست کانال ۱ (count)<input type="number" id="q0" step="1" min="0" max="255"><span class="lb" id="a0"></span></label>
<label>آفست کانال ۲ (count)<input type="number" id="q1" step="1" min="0" max="255"><span class="lb" id="a1"></span></label>
<label>گین کانال ۱ (‰)<input type="number" id="q2" step="1" min="100" max="3000"><span class="lb" id="a2"></span></label>
<label>گین کانال ۲ (‰)<input type="number" id="q3" step="1" min="100" max="3000"><span class="lb" id="a3"></span></label>
<label>ضریب ETA کانال ۱ (‰، صفر=خاموش)<input type="number" id="q9" step="1" min="0" max="999"><span class="lb" id="a9"></span></label>
<label>ضریب ETA کانال ۲ (‰، صفر=خاموش)<input type="number" id="q10" step="1" min="0" max="999"><span class="lb" id="a10"></span></label>
</div>
</div>
</div>
<div class="sgx" id="s4">
<div class="cd">
<div class="hd"><b>پشتیبان‌گیری همهٔ تنظیمات</b></div>
<div class="bqr">
<button class="sb sb2" onclick="xexp()">⬇ خروجی (دانلود JSON)</button>
<label class="sb" style="cursor:pointer">⬆ ورودی (انتخاب فایل)<input type="file" id="xim" accept=".json,application/json" style="display:none"></label>
<span class="lb" id="xst"></span>
</div>
<div class="sx">خروجی، همهٔ مقادیر «اعمال‌شدهٔ» برد را در یک فایل JSON می‌ریزد و ورودی همان فایل را یکی‌یکی روی برد اعمال می‌کند (برد هر عدد را گیره می‌زند و نتیجه کنار همان فیلد دیده می‌شود). گذراها (۱۵..۱۹ و میوت ۷۶) جزو پشتیبان نیستند؛ فهرست از خود شناسه‌ها ساخته می‌شود، پس هر پارامتر تازه خودبه‌خود پشتیبان گرفته می‌شود.</div>

</div>
</div>

</div>
</main>

<script>
const $=i=>document.getElementById(i);
const ST=['خاموش','Bulk','Absorb','Float','راه‌اندازی','JIT wait','Input wait','Final fault','باتری قطع','دستی'];
const SC=['','g','g','g','y','r','y','r','r','y'];
/* ثابت‌های بخش 5.3 سند */
const K_UV=3300/4095*11/10*1000/101,K_MA=K_UV/10,K24=3300/4095*76000/6800,K24B=3300/4095*69200/6800,K12=3300/4095*41000/6800;
/* شناسه: [عنوان, واحد, کمینه, بیشینه, نوع(n عدد، b کلید), توضیح] */
/* [EN] v1.39 (user order: show duty as REAL percent everywhere). These two
   rows now READ and ACCEPT percent (0..50); only the wire value stays
   permille (x10 at num(), /10 when the applied value is printed). Every
   other duty display - live value, ceiling note, manual input, blink
   duties - already spoke percent, so the panel is now uniform.
   [FA] v1.39 (دستور کاربر: همه‌جا درصد واقعی duty) - این دو ردیف حالا درصد
   می‌خوانند و درصد قبول می‌کنند (۰..۵۰)؛ فقط مقدار سیمی پرمیل می‌ماند.
   همهٔ نمایش‌های دیگر duty از قبل درصد بودند. */
const PUN=id=>(id===13||id===14)?'٪':P[id][1];
const P={
13:['سقف duty','٪',0,500,'n','سقف duty همین کانال؛ عدد را به درصد واقعی بنویسید (حداکثر ۵۰٪) — روی سیم به‌صورت پرمیل ذخیره می‌شود. هر duty بالاتر — خودکار، فیکس یا دستی — محدود به همین سقف است.'],
14:['سقف duty','٪',0,500,'n','سقف duty همین کانال؛ عدد را به درصد واقعی بنویسید (حداکثر ۵۰٪) — روی سیم به‌صورت پرمیل ذخیره می‌شود. هر duty بالاتر — خودکار، فیکس یا دستی — محدود به همین سقف است.']};
/* ولتاژها: [عنوان, اندیس t, شناسهٔ آفست, ضریب مقسم] */
const V=[['ورودی',14,4,K24],['پک ۲۴V',15,5,K24B],['نود ۱۲V',16,6,K12],['باتری بالا',18],['باتری پایین',17]];
var D=null;/* var (نه let) تا در تست هاست هم قابل‌نوشتن باشد */
const v2=mv=>(mv/1000).toFixed(2),pc=pm=>(pm/10).toFixed(1)+'%';
function send(id,v){const a=$('a'+id);if(a)a.textContent='…';fetch('/s?id='+id+'&v='+v,{method:'POST'}).then(r=>{if(!r.ok)throw 0;}).catch(()=>{if(a)a.textContent='خطا';});}

/* ==================== صف ارسال سراسری / Global send queue ==================== */
/* [EN] v1.52 (user order 2026-10-05): a typed box no longer posts itself the
   moment it loses focus. Edits are staged locally, the page marks them, and
   ONE button ships the whole batch; after the POSTs the panel waits for the
   next telemetry frame and compares what the board reports back with what was
   sent, so the user gets a real handshake answer - "accepted and stored" or
   "the board clamped these".
   [FA] هر کادر دیگر به‌تنهایی ارسال نمی‌شود: تغییرات محلی صف می‌شوند و یک
   دکمه همه را با هم می‌فرستد؛ بعد از ارسال، پنل منتظر فریم بعدی برد می‌ماند و
   مقدار برگشتی را با مقدار فرستاده‌شده مقایسه می‌کند تا بگوید «نشست» یا
   «برد گیره زد». */
var PEND={};
function pbar(){const n=Object.keys(PEND).length,b=$('sbar');if(!b)return;
 b.className=n?'on':'';$('sbn').textContent=n;
 document.body.style.paddingBottom=n?'52px':'';}
function qput(id,v){PEND[id]=v;const e=$('q'+id);if(e)e.classList.add('pq');
 const a=$('a'+id);if(a)a.textContent='در صف';pbar();}
function pclr(id){delete PEND[id];const e=$('q'+id);if(e)e.classList.remove('pq');pbar();}
function pundo(){for(const id of Object.keys(PEND)){const e=$('q'+id);
  if(e){e.classList.remove('pq');e.value=(D&&D.p&&D.p[id]!=null)?D.p[id]:'';}}
 PEND={};pbar();stxt('sbst','تغییرات محلی پاک شد؛ کادرها دوباره مقدار برد را نشان می‌دهند.');
 if(typeof afresh==='function')afresh();if(typeof sall==='function')sall();}

/* ==================== قوانین بین‌فیلدی / Cross-field rules ==================== */
/* [EN] v1.56 (user order): the MCU no longer checks how these numbers fit
   TOGETHER - it only guards each field's own min/max. The panel owns the
   joint rules now and applies them to the staged batch BEFORE sending, then
   reports every value it had to move. Each rule is the one the firmware used
   to run, in the same direction (the first value is authoritative).
   [FA] میکرو دیگر جور بودن این اعداد با هم را چک نمی‌کند؛ فقط کمینه/بیشینهٔ
   هر فیلد را نگه می‌دارد. قوانین مشترک حالا مال پنل است: پیش از ارسال روی
   دستهٔ صف‌شده اعمال می‌شود و هر عددی که مجبور شده جابه‌جا کند را می‌گوید.
   جهت هر قانون همان چیزی است که قبلاً در فرم‌ور بود (عدد اول مرجع است). */
const MINGAP=100;
function fitdur(per,cnt,gap){/* بیشترین مدت هر بوق که در پنجره جا می‌شود */
 if(per<=0||cnt<=0)return 600000;
 const gaps=cnt>1?gap*(cnt-1):0;
 if(gaps>=per)return 0;
 return Math.floor((per-gaps)/cnt);}
/* [EN] v: id->value map of the WHOLE picture (boxes + board). Returns the
   list of fixes as [id, from, to]. [FA] فهرست اصلاح‌ها را برمی‌گرداند. */
function fixrules(v){
 const fx=[],set=(id,nv)=>{if(v[id]!==nv){fx.push([id,v[id],nv]);v[id]=nv;}};
 /* ۱) گپ: با بیش از یک بوق، دست‌کم ۱۰۰ms */
 if(v[42]>1&&v[40]!==0&&v[43]<MINGAP)set(43,MINGAP);
 if(v[48]>1&&v[46]!==0&&v[49]<MINGAP)set(49,MINGAP);
 if((v[62]>1||v[63]>1||v[64]>1||v[58]>1)&&v[65]<MINGAP)set(65,MINGAP);
 /* ۲) مدت هر بوق باید در پنجرهٔ خودش جا شود */
 if(v[40]!==0)set(41,Math.min(v[41],fitdur(v[40],v[42],v[43])));
 if(v[46]!==0)set(47,Math.min(v[47],fitdur(v[46],v[48],v[49])));
 if(v[54]!==0)set(59,Math.min(v[59],fitdur(v[54],v[62],v[65])));
 if(v[122]!==0)set(121,Math.min(v[121],fitdur(v[122],v[63],v[65])));
 if(v[55]!==0)set(60,Math.min(v[60],fitdur(v[55],v[64],v[65])));
 if(v[56]!==0)set(57,Math.min(v[57],fitdur(v[56],v[58],v[65])));
 /* ۳) ترتیب باندها: ۵۰ ≥ ۵۱ ≥ ۵۲ ≥ ۵۳ و عدد اول مرجع */
 if(v[51]>v[50])set(51,v[50]);
 if(v[52]>v[51])set(52,v[51]);
 if(v[53]>v[52])set(53,v[52]);
 /* ۵) کمینهٔ خاموشی/روشنی بیشتر از دوره نشود */
 if(v[67]>v[66])set(67,v[66]);
 if(v[69]>v[68])set(69,v[68]);
 /* ۷) نردبان درصد دست‌کم ۱۰۰mV پهنا داشته باشد (حد پایین مرجع) */
 if(v[75]<v[74]+100)set(75,v[74]+100);
 if(v[120]<v[119]+100)set(120,v[119]+100);
 /* ۸) خروج فول باید زیر ورود فول بماند */
 if(v[78]>=v[77])set(78,v[77]-1);
 return fx;}
/* [EN] Snapshot of every id the rules touch: the staged value if there is
   one, else the box, else what the board reported.
   [FA] عکس لحظه‌ای هر شناسه: مقدار صف‌شده، وگرنه کادر، وگرنه مقدار برد. */
const RIDS=[40,41,42,43,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,62,63,64,65,66,67,68,69,74,75,77,78,119,120,121,122];
/* [EN] v1.75 audit finding: an id nobody has touched - no pending edit, no
   board frame, an empty box - used to read as ZERO here. The joint rules then
   "repaired" that zero and sendall() shipped parameters the user never typed
   (one of them as -1). The fallback is the factory default now, and sendall()
   only applies a repair to an id it actually knows something about.
   [FA] شناسه‌ای که هیچ خبری از آن نداریم قبلاً صفر خوانده می‌شد و قانون‌های
   مشترک همان صفر را «درست» می‌کردند و ارسال می‌شد؛ حالا پیش‌فرض کارخانه
   خوانده می‌شود و فقط شناسه‌های معلوم اصلاح و ارسال می‌شوند. */
function rknown(id){
 if(id in PEND)return true;
 if(D&&D.p&&D.p[id]!=null)return true;
 const e=$('q'+id);
 return !!(e&&e.value!=='');}
function rsnap(){const v={};RIDS.forEach(id=>{
 const d=pdflt(id);
 v[id]=(id in PEND)?PEND[id]:c4v(id,(D&&D.p&&D.p[id]!=null)?D.p[id]:(d!=null?d:0));});return v;}
/* v1.70: نمایش نتیجهٔ ارسال؛ ‎retry=1‎ یعنی چیزی نرسیده و هنوز در PEND است. */
const esc=t=>String(t).replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
function sdlgx(){const w=$('sres');if(w)w.className='';}
function sdlg(kind,head,body,retry){const w=$('sres');if(!w)return;
 w.className='on '+kind;$('srst').textContent=head;$('srsm').innerHTML=body;
 $('srsa').innerHTML=(retry?'<button class="sb brun" onclick="sdlgx();sendall()">دوباره بفرست</button>':'')
  +'<button class="sb sb2" onclick="sdlgx()">باشه، بستن</button>';}
const sdlgl=a=>'<ul>'+a.map(x=>'<li><code>'+esc(x)+'</code></li>').join('')+'</ul>';
async function sendall(){
 if(!Object.keys(PEND).length)return;
 /* v1.56: قوانین مشترک اینجا اعمال می‌شوند، نه روی برد */
 const v=rsnap(),fixed=fixrules(v).filter(f=>rknown(f[0])),fixtxt=[];
 fixed.forEach(f=>{const id=f[0];qput(id,v[id]);const e=$('q'+id);if(e)e.value=v[id];
  fixtxt.push(id+': '+f[1]+'→'+f[2]);});
 const ids=Object.keys(PEND);
 const sent={};let ok=0;
 stxt('sbst','… در حال ارسال');
 for(const id of ids){const v=PEND[id];
  try{const r=await fetch('/s?id='+id+'&v='+v,{method:'POST'});if(r.ok){ok++;sent[id]=v;}}catch(e){}
  await sl(60);}
 if(ok===0){stxt('sbst','⛔ هیچ‌کدام ارسال نشد — ارتباط با برد برقرار نیست.');
  sdlg('bad','⛔ هیچ‌کدام ارسال نشد',
   'ارتباط با ESP/برد برقرار نیست، پس هیچ عددی روی برد ننشست.<br>'
   +'هر '+ids.length+' تغییر دست‌نخورده در صف مانده و کادرها نارنجی‌اند — '
   +'اتصال را چک کنید و دوباره بفرستید.',1);return;}
 /* دست‌دادن: منتظر فریم بعدی برد می‌مانیم و مقدار برگشتی را می‌سنجیم */
 stxt('sbst','… ارسال شد، منتظر تأیید برد');
 for(let k=0;k<25;k++){await sl(200);if(D&&D.p)break;}
 await sl(600);
 const bad=[];
 for(const id of Object.keys(sent)){
  const back=(D&&D.p)?D.p[id]:null;
  /* v1.70: بی‌پاسخ = مدرکی نداریم؛ در صف می‌ماند تا دوباره فرستاده شود. */
  if(back==null){bad.push(id+': بی‌پاسخ');continue;}
  if(+back!==+sent[id])bad.push(id+': '+sent[id]+'→'+back);
  pclr(id);}
 pbar();
 if(typeof afresh==='function')afresh();if(typeof sall==='function')sall();
 if(ok<ids.length){const left=Object.keys(PEND);
  stxt('sbst','⚠ '+ok+' از '+ids.length+' ارسال شد؛ بقیه در صف ماندند — دوباره بزنید.');
  sdlg('warn','⚠ ارسال ناقص — '+ok+' از '+ids.length+' نشست',
   'بقیه نرسیدند و هنوز در صف‌اند (کادرهای نارنجی). چیزی از دست نرفته؛ '
   +'فقط باید دوباره فرستاده شوند.'
   +(left.length?'<br>در صف مانده: '+sdlgl(left):'')
   +(bad.length?'<br>برد این‌ها را گیره زد: '+sdlgl(bad):''),1);return;}
 const pre=fixtxt.length?(' · پنل پیش از ارسال '+fixtxt.length+' عدد را جور کرد: '+fixtxt.join(' · ')):'';
 if(!bad.length){stxt('sbst','✅ '+ok+' تنظیم ارسال شد؛ برد همه را عیناً پذیرفت و ذخیره کرد.'+pre);
  sdlg('ok','✅ همه نشست — '+ok+' تنظیم',
   'برد هر '+ok+' مقدار را عیناً پس‌فرستاد و در حافظهٔ ماندگار ذخیره کرد؛ '
   +'نیازی به ارسال دوباره نیست.'
   +(fixtxt.length?'<br>پنل پیش از ارسال '+fixtxt.length+' عدد را جور کرد: '+sdlgl(fixtxt):''),0);
  return;}
 stxt('sbst','✅ '+ok+' تنظیم نشست، اما برد '+bad.length+' مقدار را به بازهٔ مجاز خودش گیره زد: '+bad.join(' · ')+pre);
 const mute=bad.filter(x=>x.indexOf('بی‌پاسخ')>=0).length;
 sdlg(mute?'warn':'ok',(mute?'⚠ ':'✅ ')+ok+' تنظیم ارسال شد',
  (mute?'برای '+mute+' شناسه هیچ تأییدی از برد نیامد — آن‌ها در صف نگه داشته شدند تا دوباره بفرستید.<br>':'')
  +'بقیه نشست. برد این مقدارها را به بازهٔ مجاز خودش گیره زد — عددِ روی برد همان سمت راست فلش است:'
  +sdlgl(bad)
  +(fixtxt.length?'پنل پیش از ارسال '+fixtxt.length+' عدد را جور کرد: '+sdlgl(fixtxt):''),mute?1:0);}
function num(id){const e=$('i'+id),p=P[id],t=+e.value;if(e.value===''||isNaN(t))return;const w=(id===13||id===14)?Math.round(t*10):Math.round(t);send(id,Math.min(p[3],Math.max(p[2],w)));e.value='';e.blur();}
function ctl(id){const p=P[id];
 if(id===13||id===14)return `<input type="number" id="i${id}" min="0" max="50" step="any" placeholder="0…50٪" onkeydown="if(event.key=='Enter')num(${id})"><button class="sb" onclick="num(${id})">ثبت</button>`;
 return `<input type="number" id="i${id}" min="${p[2]}" max="${p[3]}" placeholder="${p[2]<0?'±'+p[3]:p[2]+'…'+p[3]}" onkeydown="if(event.key=='Enter')num(${id})"><button class="sb" onclick="num(${id})">ثبت</button>`;}
const row=(id,x)=>`<div class="rw" title="${P[id][5].replace(/<[^>]*>/g,' ').replace(/"/g,'&quot;')}"><div>${P[id][0]} <span class="lb">${PUN(id)}</span><span class="ap n" id="a${id}"></span></div><div class="ct">${x||''}${ctl(id)}</div></div>`;

/* ---------- ساخت صفحه: ولتاژها + فیلتر (مشترک) ---------- */
/* v1.16k: merged voltages+alarm table - fixed layout, each value once, pills inline */
const SR=[['ورودی',0],['پک ۲۴V',1],['نود ۱۲V',2],['باتری بالا',3],['باتری پایین',4],['جریان ۱ (بالا)',5],['جریان ۲ (پایین)',6]];
$('ast').innerHTML=`<div class="srvw"><table class="srv"><colgroup><col class="c1"><col class="c2"><col class="c3"><col class="c4"><col class="c5"></colgroup><tr><th>سیگنال</th><th>مقدار</th><th>فرمول / بازه</th><th>وضعیت</th><th>کالیبره با مولتی‌متر</th></tr>${SR.map(r=>{const k=r[1];
 const cal=k<3?`<input type="number" step="any" id="vm${k}" placeholder="مولتی‌متر V" onkeydown="if(event.key=='Enter')vcal(${k})"> <button class="sb sb2" onclick="vcal(${k})">اعمال</button> <span class="lb">±<span class="ap n" id="a${[4,5,6][k]}"></span></span>`:'';
 return `<tr id="sr${k}"><td>${r[0]}</td><td class="n" id="v${k}"></td><td><div class="fx" id="fv${k}"></div></td><td><span class="tg" id="sp${k}"></span></td><td>${cal}</td></tr>`;}).join('')}</table></div>`+'<div class="ab" id="asb5" style="margin-top:8px;min-height:0"><small>خطاهای قفل‌شده (fault) — LED جدا برای هر بیت</small><div class="leds" style="margin:0 0 6px" id="asfb"><span class="bit" id="asbb0"><i></i><small>ADC</small></span><span class="bit" id="asbb1"><i></i><small>OC1</small></span><span class="bit" id="asbb2"><i></i><small>OC2</small></span><span class="bit" id="asbb3"><i></i><small>باتری</small></span><span class="bit" id="asbb4"><i></i><small>JIT1</small></span><span class="bit" id="asbb5"><i></i><small>JIT2</small></span><span class="bit" id="asbb6"><i></i><small>قطع‌باتری</small></span></div><div class="fx2" id="asf"></div></div>';
let ASB=null;
ASB={sp:[0,1,2,3,4,5,6].map(k=>$('sp'+k)),sr:[0,1,2,3,4,5,6].map(k=>$('sr'+k)),flt:$('asf'),bits:[0,1,2,3,4,5,6].map(k=>$('asbb'+k)),box5:$('asb5'),mask:-1,live:$('aslive'),tick:false};
/* ‎v1.14b (user order 2026-09-26)‎: پنجرهٔ median/میانگین به تب «تنظیمات» رفت؛ اینجا فقط وضعیت زندهٔ فیلتر و نمونه‌های نمودار می‌مانند */
$('fg').innerHTML='<div class="lb" id="fspan" style="margin-top:6px"></div>';
/* ---------- دو ستون جدا: شارژر ۱ و شارژر ۲ ---------- */
$('ch').innerHTML=[1,2].map(n=>`<div class="cd"><div class="hd"><b>شارژر ${n} <span class="lb">· باتری ${n==1?'بالا':'پایین'}</span></b><span class="tg" id="st${n}"></span></div>
<div class="sx">کارت زندهٔ همین کانال: جریان تخمینی باتری، duty فعلی و وضعیت ماشین حالت. مود «دستی» حلقهٔ کنترل را کنار می‌گذارد و duty را به شما می‌دهد — ولی حدهای سخت (Hard fault جریان، قطع OV و سقف duty) همچنان فعال می‌مانند.</div>
<div class="bg2"><div class="big"><span class="lb">جریان باتری (iest)</span><b class="n" id="ie${n}"></b></div>
<div class="big"><span class="lb">duty <span id="dc${n}"></span></span><b class="n" id="du${n}"></b></div></div><div class="bar"><i id="db${n}"></i><u id="cl${n}"></u></div>
<div class="bctl"><span class="lb">مود</span><button class="sw" id="ma${n}">خودکار</button><button class="sw w" id="mm${n}">دستی</button><span class="lb">·</span><span class="lb">duty دستی ٪</span><input type="number" step="any" id="qm${n}" data-s style="width:76px"><button class="sb" onclick="qset(${n})">اعمال</button><button class="sb off2" onclick="qzero(${n})">صفر</button><button class="sw" id="fx${n}" title="duty ثابت همین کانال با حفاظتها؛ مود دستی سراسری اولویت دارد">فیکس</button></div>
${row(12+n)}
<div class="lb">بستن پنل: ۱۰ ثانیه بعد مود دستی خاموش و duty صفر می‌شود؛ بعد از تریپ JIT همان duty را دوباره اعمال کنید.</div>
<div class="sec">زنجیرهٔ اندازه‌گیری و محاسبه</div>
<div class="sx">این بخش تنظیمی ندارد: مسیر یک عدد را از شمارش خام ADC تا جریان/ولتاژ نهایی نشان می‌دهد تا معلوم باشد هر ضریب و فیلتر کجای زنجیره اثر می‌گذارد.</div>
<table>${[['ADC خام','count',0],['ولتاژ شنت','µV',1],['جریان بدون فیلتر','mA',2],['جریان فیلترشده','mA',3],['تخمین باتری (iest)','mA',4]].map(r=>`<tr><td>${r[0]}<div class="fx" id="f${n}${r[2]}"></div></td><td class="n"><b id="c${n}${r[2]}"></b></td><td class="lb">${r[1]}</td></tr>`).join('')}</table>
<div class="lb kc">ثابت‌ها: ADC دوازده‌بیتی، ۳۳۰۰mV، R41/R42 = 1k/10k، LM358 × 101، شنت 10 mOhm</div>
<canvas id="cv${n}"></canvas><div class="lg"><span><i style="background:#78849f"></i>بدون فیلتر · نوسان <b class="n" id="pu${n}"></b> mA</span><span><i style="background:#63a2ff"></i>فیلترشده · نوسان <b class="n" id="pf${n}"></b> mA</span><span class="hnl">نقاط <input type="number" id="hN${n}" data-s min="10" max="600" value="100"> از <b class="n" id="hC${n}">--</b></span></div>
<button class="bt" id="tg${n}"></button></div>`).join('');
[1,2].forEach(n=>$('tg'+n).onclick=()=>{const c=D&&D.p[10+n];if(c!==0&&!confirm('PWM شارژر '+n+' فوراً قطع شود؟'))return;send(10+n,c===0?1:0);});
/* ---------- تاریخچهٔ نمودار هر کانال ---------- */
let LS=-1;const hn=c=>{const e=$('hN'+(c+1)),v=e?Math.round(+e.value):0;return !v?100:Math.min(600,Math.max(10,v));},H=[0,1].map(()=>({u:[],f:[]}));
function vcal(k){const R=V[k],m=Math.round(+$('vm'+k).value*1000),shown=D&&D.t[R[1]],off=D&&D.p[R[2]];if(!(m>0))return alert('عدد مولتی‌متر را به ولت وارد کنید (مثلاً 13.05).');if(off==null)return;
 const no=Math.min(5000,Math.max(-5000,off+m-shown));if(confirm(R[0]+': آفست '+off+' ← '+no+'mV\n(نمایش '+v2(shown)+'V، مولتی‌متر '+v2(m)+'V)')){send(R[2],no);$('vm'+k).value='';}}
/* نمودار زندهٔ فیلتر هر کانال */
function chart(){[0,1].forEach(ci=>{const c=$('cv'+(ci+1)),w=c.clientWidth,h=c.clientHeight,dp=devicePixelRatio||1;if(!w)return;
 if(c.width!=Math.round(w*dp)){c.width=Math.round(w*dp);c.height=Math.round(h*dp);}
 const x=c.getContext('2d');x.setTransform(dp,0,0,dp,0,0);x.clearRect(0,0,w,h);const s=H[ci],hc=$('hC'+(ci+1));if(hc)hc.textContent=s.u.length;if(s.u.length<2)return;
 let lo=Math.min(...s.u,...s.f),hi=Math.max(...s.u,...s.f);if(hi-lo<10){const m=(hi+lo)/2;lo=m-5;hi=m+5;}const pd=(hi-lo)*.12,a=lo-pd,z=hi+pd;
 const X=i=>w-8-(s.u.length-1-i)*(w-16)/(hn(ci)-1),Y=v=>h-8-(v-a)/(z-a)*(h-16);
 const ln=(A,col,lw)=>{x.beginPath();A.forEach((v,i)=>i?x.lineTo(X(i),Y(v)):x.moveTo(X(i),Y(v)));x.strokeStyle=col;x.lineWidth=lw;x.stroke();};
 x.fillStyle='#96a1b8';x.font='11px Vazirmatn,sans-serif';x.fillText(Math.round(hi)+'mA',8,16);x.fillText(Math.round(lo)+'mA',8,h-10);
 ln(s.u,'#6b7691',1);ln(s.f,'#63a2ff',2);const pp=A=>{const B=A.slice(-50);return Math.max(...B)-Math.min(...B);};$('pu'+(ci+1)).textContent=pp(s.u);$('pf'+(ci+1)).textContent=pp(s.f);});}
/* تعویض تب: پنل و داده‌برداری بنچ */
/* v1.69: offsetهای چسبان ثابت (۵۵/۱۱۳px) نوار کارت‌ها را روی نوار بخش‌ها
   می‌نشاند و قایمش می‌کرد. حالا ارتفاع واقعی اندازه گرفته و در متغیرهای CSS
   نوشته می‌شود تا هر نوار زیر نوار بالایی بچسبد. */
function stickfit(){
 const r=document.documentElement.style,
  h=document.querySelector('header'),n=document.querySelector('nav'),
  sb=$('sbt'),H=h?h.offsetHeight:0,N=n?n.offsetHeight:0,S=sb?sb.offsetHeight:0;
 r.setProperty('--t-nav',H+'px');
 r.setProperty('--t-sub',(H+N+6)+'px');
 r.setProperty('--t-sub2',(H+N+S+10)+'px');}
addEventListener('resize',stickfit);addEventListener('load',stickfit);
let TAB=0;document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{TAB=+b.dataset.t;document.querySelectorAll('nav button').forEach(x=>x.classList.toggle('a',x===b));document.querySelectorAll('.pgx').forEach((x,i)=>x.classList.toggle('a',i==TAB));stickfit();if(D)draw(D);});
/* v1.15b: زیرتب داخل تنظیمات — v1.42 (دستور کاربر: پشتیبان‌گیری آخرین زیرتب): ۰=شارژ و PID، ۱=سناریوها، ۲=نظارت و ایمنی، ۳=کالیبراسیون و فیلتر جریان، ۴=پشتیبان‌گیری */
let STAB=0;document.querySelectorAll('#sbt button').forEach(b=>b.onclick=()=>{STAB=+b.dataset.s;document.querySelectorAll('#sbt button').forEach(x=>x.classList.toggle('a',x===b));document.querySelectorAll('.sgx').forEach((x,i)=>x.classList.toggle('a',i==STAB));stickfit();if(D)draw(D);});
let UCARD=1;function usel(n){UCARD=n;for(let k=1;k<=6;k++){const c=$('ucard'+k);if(c)c.style.display=k===n?'':'none';}document.querySelectorAll('#usel button').forEach(b=>b.classList.toggle('a',+b.dataset.u===n));stickfit();}
document.querySelectorAll('#usel button').forEach(b=>b.onclick=()=>usel(+b.dataset.u));
$('mx').onclick=()=>send(19,0);
{const b6=$('ib117');if(b6)b6.onchange=()=>{send(117,b6.checked?1:0);};}
{const b7=$('db127');if(b7)b7.onchange=()=>{send(127,b7.checked?1:0);};}

/* ---------- به‌روزرسانی: فرمول‌های بخش 5.3 با مقادیر زنده ---------- */
const f1=x=>x.toFixed(1),V_=mv=>(mv/1000).toFixed(2)+'V',nz=v=>v==null?'?':v;
/* iest مثل STM32 (charger.c): زیر Vin 10V یا Vbat 5V برگشت به همانی */
const ie=(fl,vin,eta,vb)=>vin<10000||vb<5000?fl+' (همانی: ولتاژ زیر حد)':Math.floor(Math.floor(fl*eta/1000)*vin/vb);
const LUTX=[0,20,37,106,189,236,253,283,312,353,390,441,557,707],LUTY=[0,0,111,766,1616,2753,3347,4037,4686,5523,6231,7043,8867,10794];
const lutPow=c=>{for(let i=1;i<LUTX.length;i++){if(c<=LUTX[i]){const x0=LUTX[i-1],x1=LUTX[i];if(x1==x0)return LUTY[i];return LUTY[i-1]+Math.floor((c-x0)*(LUTY[i]-LUTY[i-1])/(x1-x0));}}const n=LUTX.length-1,d=LUTX[n]-LUTX[n-1];if(!d)return LUTY[n];return LUTY[n]+Math.floor((c-LUTX[n])*(LUTY[n]-LUTY[n-1])/d);};
const lutTap=(ch,vb)=>{const pw=lutPow(Math.round(ch)),v=Math.min(15000,Math.max(8000,vb));return ' => LUT:'+pw+'mW/'+v+'='+Math.floor(pw*1000/v)+'mA';};
/* v1.19: آینهٔ جدول توان کانال ۱ (SOLO1، ۱۷ لنگر) — قرینهٔ کانال ۲ */
const LUT1X=[0,5,11,31,54,81,114,148,189,231,277,330,382,444,504,567,640],LUT1Y=[0,0,135,445,795,1061,1670,2189,2778,3390,4007,4720,5474,6306,7159,8061,9089];
const lut1Pow=c=>{for(let i=1;i<LUT1X.length;i++){if(c<=LUT1X[i]){const x0=LUT1X[i-1],x1=LUT1X[i];if(x1==x0)return LUT1Y[i];return LUT1Y[i-1]+Math.floor((c-x0)*(LUT1Y[i]-LUT1Y[i-1])/(x1-x0));}}const n=LUT1X.length-1,d=LUT1X[n]-LUT1X[n-1];if(!d)return LUT1Y[n];return LUT1Y[n]+Math.floor((c-LUT1X[n])*(LUT1Y[n]-LUT1Y[n-1])/d);};
const lut1Tap=(ch,vb)=>{const pw=lut1Pow(Math.round(ch)),v=Math.min(15000,Math.max(8000,vb));return ' => LUT:'+pw+'mW/'+v+'='+Math.floor(pw*1000/v)+'mA';};
function formulas(t,p){
 [1,2].forEach(n=>{const b=n==1?0:7,raw=t[b],off=p[n-1],g=p[n+1],eta=p[8+n],vb=n==1?t[18]:t[17],vin=t[14],fl=t[b+3];
  $('f'+n+'0').textContent='12-bit ADC · Vref 3300mV';
  $('f'+n+'1').textContent=`${raw} × 3300/4095 × 11/10 × 1000/101 = ${raw} × 8.7767 ≈ ${Math.round(raw*K_UV)}`;
  $('f'+n+'2').textContent=off==null||g==null?'':`(${raw} − ${off}) × 0.8777 × ${g}/1000 ≈ ${f1(Math.max(raw-off,0)*K_MA*g/1000)}${n==2?lutTap(Math.max(raw-off,0)*K_MA*g/1000,vb):lut1Tap(Math.max(raw-off,0)*K_MA*g/1000,vb)}`;
  $('f'+n+'3').textContent=`convert( average[W=${nz(p[8])}]( median[N=${nz(p[7])}]( raw ) ) ) = ${fl}`;
  $('f'+n+'4').textContent=eta==null?'':eta==0?`eta = 0 → Iest = I = ${fl}`:`${fl} × ${V_(vin)} × ${eta}‰ / ${V_(vb)} ≈ ${ie(fl,vin,eta,vb)}`;});
 V.forEach((v,i)=>{const e=$('fv'+i);if(i<3){const o=p[v[2]]==null?0:p[v[2]],c=Math.round((t[v[1]]-o+(i==2?150+Math.floor(t[9]*470/1000):0))/v[3]);e.textContent=`${c} × ${v[3].toFixed(3)} ${o<0?'−':'+'} ${Math.abs(o)}${i==2?' − (150 + '+t[9]+'×470/1000)':''}`;}
  else e.textContent=i==3?'V24 − V12':'= V12';});
 $('ff').textContent=`I_filtered = convert( average[W=${nz(p[8])}]( median[N=${nz(p[7])}]( raw counts ) ) )`;}
function hist(d){const t=d.t;if(d.on==1&&d.seq!==LS){LS=d.seq;[0,1].forEach(c=>{const b=c*7,s=H[c];s.u.push(t[b+2]);s.f.push(t[b+3]);if(s.u.length>hn(c)){s.u.shift();s.f.shift();}});}}
function qfill(){if(!D||!D.p)return;for(const id of [7,8]){const e=$('q'+id),a=$('a'+id);if(!e)continue;if(document.activeElement!==e&&e.value==='')e.value=D.p[id]==null?'':D.p[id];if(a&&!(D.q&(1<<id)))a.textContent=D.p[id]==null?'':D.p[id];}}
function qdef(){[[20,14400],[21,14300],[22,14600],[23,13500],[24,12800],[25,650],[26,50]].forEach(x=>{const e=$('q'+x[0]);if(e)e.value=x[1];qput(x[0],x[1]);});pdef();ldef();qgraph();}
/* ===== v1.14: نمودار مراحل شارژ — مقدار هر خط از فیلد تایپ‌نشده/متفاوت با مقدار اعمال‌شده می‌آید (پیش‌نمایش خط‌چین) ===== */
const QDEF=[14400,14300,14600,13500,12800,650,50];
/* [EN] v1.33: the q20..q26 input boxes went when the duplicate profile form
   did, so there is no longer a "typed but not yet applied" state for these
   seven - the chart writes straight to the board. qv() is now just "the
   applied value, or the factory default before the first frame arrives",
   and the .p preview flag it used to return had no readers left.
   [FA] کادرهای ‎q20..q26‎ همراه فرم تکراری profile رفتند، پس دیگر حالت «تایپ
   شده ولی هنوز اعمال نشده» برای این هفت تا وجود ندارد - نمودار مستقیم روی
   برد می‌نویسد. qv() حالا فقط «مقدار اعمال‌شده، یا پیش‌فرض کارخانه پیش از
   رسیدن اولین فریم» است و پرچم پیش‌نمایشی که برمی‌گرداند هیچ خواننده‌ای نداشت. */
function qv(id){const d=D&&D.p&&D.p[id]!=null?D.p[id]:QDEF[id-20];return{v:d,d};}
/* v1.14d: نگهبان ترکیب profile — آینهٔ قوانین Charger_ClampProfile روی برد.
   هر قانون: [فیلد اصلی، فیلد مرجع] + پیام فارسی. خروجی خالی = ترکیب سالم. */
function qchk(){const w=[],a=qv(20).v,e=qv(21).v,o=qv(22).v,f=qv(23).v,r=qv(24).v,im=qv(25).v,tp=qv(26).v;
 const bad=(v,lo,hi)=>!(v>=lo&&v<=hi);
 if(bad(a,11000,14600))w.push({ids:[20],msg:'Absorb باید ۱۱۰۰۰..۱۴۶۰۰ باشد'});
 else{
  if(bad(e,a-500,a-50))w.push({ids:[21,20],msg:'ورود Absorb باید Absorb−۵۰۰ تا Absorb−۵۰ باشد ('+(a-500)+'..'+(a-50)+')'});
  if(bad(o,a+100,Math.min(a+400,14750)))w.push({ids:[22,20],msg:'سقف تجاوز باید Absorb+۱۰۰ تا Absorb+۴۰۰ (سقف ۱۴۷۵۰) باشد'});
  if(bad(f,9000,a-300))w.push({ids:[23,20],msg:'Float باید ۹۰۰۰..Absorb−۳۰۰ باشد (≤ '+(a-300)+')'});
  else if(bad(r,8000,f-300))w.push({ids:[24,23],msg:'Reentry باید ۸۰۰۰..Float−۳۰۰ باشد (≤ '+(f-300)+')'});
 }
 if(bad(im,100,900))w.push({ids:[25],msg:'جریان Bulk باید ۱۰۰..۹۰۰ باشد'});
 if(bad(tp,10,Math.min(300,im)))w.push({ids:[26,25],msg:'Taper باید ۱۰..Bulk max باشد (≤ '+Math.min(300,im)+')'});
 return w;}
/* ===== v1.25 (دستور کاربر ۲۰۲۶-۰۹-۲۹): در «!» هر بخش، خودِ پارامترها هم
   توضیح داده شوند — هرکدام چیست و چه کار می‌کند.
   یک جدول و یک پاس، نه ۸ بلوک HTML دستی: بلوک‌های دستی همان چیزی‌اند که در این
   پروژه بارها کهنه شده‌اند، پس متن از یک جا می‌آید و دکمه‌ها فقط شناسه‌هایشان را
   با ‎data-p‎ اعلام می‌کنند.
   v1.25: explain each PARAMETER inside its section's "!" bubble. One table and
   one pass rather than eight hand-written HTML blobs - hand-written copies are
   exactly what has gone stale here before. */
const PX={
 9:['بازده کانال ۱','بازده مبدل بر حسب پرمیل برای تخمین جریان ورودی از توان باتری؛ صفر یعنی بدون تصحیح.'],
 10:['بازده کانال ۲','همان محاسبه برای کانال دوم؛ فقط روی عدد تخمینی جریان ورودی اثر دارد، نه روی شارژ.'],
 11:['فعال‌بودن شارژر ۱','اجازهٔ کار کانال ۱ از سمت پنل؛ صفر یعنی این کانال اصلاً سوئیچ نمی‌کند.'],
 12:['فعال‌بودن شارژر ۲','همان اجازه برای کانال ۲؛ برای تست تک‌کاناله یکی را خاموش می‌کنند.'],
 13:['سقف duty ۱','بیشترین duty مجاز کانال ۱ (‰). هر duty بالاتر — خودکار یا دستی — به همین سقف گیره می‌شود.'],
 14:['سقف duty ۲','همان سقف برای کانال ۲؛ برای محدودکردن توان یک کانال بدون دست‌زدن به profile.'],
 15:['duty ثابت ۱ روشن','کانال ۱ به‌جای حلقهٔ کنترل، duty ثابت بگیرد؛ فقط برای تست و کالیبراسیون.'],
 16:['مقدار duty ثابت ۱','همان duty ثابتی که کانال ۱ در حالت بالا می‌گیرد (‰).'],
 17:['duty ثابت ۲ روشن','همان قفل دستی برای کانال ۲.'],
 18:['مقدار duty ثابت ۲','همان duty ثابتی که کانال ۲ در حالت قفل دستی می‌گیرد (‰)؛ همچنان به سقف duty همان کانال گیره می‌شود.'],
 19:['مود تست دستی','کل شارژر را از حالت خودکار بیرون می‌آورد و کنترل duty را به شما می‌دهد؛ حدهای سخت همچنان فعال‌اند.'],
 38:['دورهٔ LED اضافه‌ولتاژ','طول یک چرخهٔ چشمک LED هنگام هشدار اضافه‌ولتاژ (ms).'],
 39:['روشنی LED اضافه‌ولتاژ','چند درصد از هر چرخه LED روشن باشد؛ ۵۰ یعنی نصف روشن نصف خاموش.'],
 40:['دورهٔ بوق اضافه‌ولتاژ','هر چند وقت یک‌بار دستهٔ بوق اضافه‌ولتاژ تکرار شود (ms)؛ صفر یعنی بی‌صدا.'],
 41:['طول هر بوق اضافه‌ولتاژ','هر تک‌بوق چقدر طول بکشد (ms).'],
 42:['تعداد بوق اضافه‌ولتاژ','در هر دسته چند بوق زده شود.'],
 43:['فاصلهٔ بوق‌های اضافه‌ولتاژ','سکوت بین دو بوق یک دسته (ms)؛ اگر تعداد بیش از یکی است باید معنادار باشد وگرنه به هم می‌چسبند.'],
 44:['دورهٔ LED قطع باتری','طول یک چرخهٔ چشمک قرمز وقتی باتری جدا تشخیص داده شده (ms).'],
 45:['روشنی LED قطع باتری','چند درصد از هر چرخه LED روشن باشد؛ ۵۰ یعنی نصف روشن نصف خاموش.'],
 46:['دورهٔ بوق قطع باتری','هر چند وقت یک‌بار دستهٔ بوق قطع باتری تکرار شود (ms)؛ صفر یعنی بی‌صدا.'],
 47:['طول هر بوق قطع باتری','هر تک‌بوق چقدر طول بکشد (ms).'],
 48:['تعداد بوق قطع باتری','در هر دسته چند بوق زده شود؛ پیش‌فرض سه بوق کوتاه.'],
 49:['فاصلهٔ بوق‌های قطع باتری','سکوت بین دو بوق یک دسته (ms)؛ وقتی تعداد بیش از یکی است باید معنادار باشد.'],
 50:['درصد شروع هشدار مصرف','از این درصد شارژ به پایین، بوق دوره‌ای حالت مصرف شروع می‌شود.'],
 51:['درصد بوق دوتایی','از این درصد به پایین، هشدار به دو بوق تغییر می‌کند — یعنی وضعیت جدی‌تر شد.'],
 52:['درصد بوق سه‌تایی','از این درصد به پایین، سه بوق؛ مرحلهٔ هشدار بعدی.'],
 53:['درصد بحرانی','از این درصد به پایین، حالت بحرانی با الگوی مخصوص خودش.'],
 54:['فاصلهٔ هشدار عادی','هر چند وقت یک‌بار هشدار مصرف در بازهٔ عادی تکرار شود (ms).'],
 55:['فاصلهٔ هشدار سه‌تایی','هر چند وقت یک‌بار هشدار سه‌تایی تکرار شود (ms).'],
 56:['فاصلهٔ تکرار بحرانی','هر چند وقت یک‌بار الگوی بحرانی تکرار شود (ms) — همان نقش ۵۴ و ۵۵ برای باندهای دیگر.'],
 57:['مدت هر بوق بحرانی','طول هر بوق باند بحرانی (ms) — از نسخهٔ ۱٫۷۱ مثل باندهای دیگر «مدت» است، نه درصد duty.'],
 58:['تعداد بوق بحرانی','چند بوق در هر دستهٔ بحرانی.'],
 59:['طول بوق عادی','طول هر بوق در بازهٔ عادی (ms).'],
 60:['طول بوق سه‌تایی','طول هر بوق در بازهٔ سه‌تایی (ms).'],
 61:['طول هشدار بحرانی','مدت یک‌بارهٔ هشدار بحرانی (ms) که قفل می‌شود.'],
 62:['تعداد بوق عادی','چند بوق در هر دستهٔ عادی.'],
 63:['تعداد بوق دوتایی','چند بوق در هر دستهٔ دوتایی.'],
 64:['تعداد بوق سه‌تایی','چند بوق در هر دستهٔ سه‌تایی.'],
 65:['فاصلهٔ بوق‌های مصرف','سکوت بین بوق‌های یک دسته در حالت مصرف (ms).'],
 66:['دورهٔ LED سبز','طول یک چرخهٔ چشمک سبز (ms)؛ سبز یعنی وضعیت خوب.'],
 67:['کمینهٔ خاموشی سبز','حداقل زمان خاموشی در هر چرخهٔ سبز تا چشمک دیده شود (ms).'],
 68:['دورهٔ LED زرد','طول یک چرخهٔ چشمک زرد (ms)؛ زرد یعنی در حال شارژ.'],
 69:['کمینهٔ روشنی زرد','حداقل زمان روشنی در هر چرخهٔ زرد تا چشمک پایان شارژ واقعاً دیده شود (ms).'],
 76:['بی‌صداکردن بوق','فقط برای همین نشست پنل بوق را خاموش می‌کند؛ روی فلش ذخیره نمی‌شود و با ریست برمی‌گردد.'],
 77:['درصد ورود به پر','از این درصد به بالا، نمایش «پر» می‌شود.'],
 78:['درصد خروج از پر','زیر این درصد، از حالت «پر» بیرون می‌آید؛ عمداً کمتر از ورود است تا نمایش چشمک نزند.'],
 79:['hysteresis شارژ','چقدر تغییر لازم است تا درصد نمایش شارژ عوض شود؛ جلوی بالا-پایین شدن دائمی عدد را می‌گیرد.'],
 80:['hysteresis مصرف','همان پایدارسازی برای درصد در حالت مصرف.'],
 81:['خروج از صفر درصد','تا این درصد بالا نرود، نمایش از صفر بیرون نمی‌آید.'],
 82:['خروج از یک درصد','تا این درصد بالا نرود، نمایش از یک بیرون نمی‌آید.'],
 0:['آفست جریان ۱','شمارش ADC که در جریان صفر خوانده می‌شود و از هر نمونه کم می‌گردد؛ اگر در حالت بی‌بار عدد جریان صفر نیست، این را تنظیم کنید.'],
 1:['آفست جریان ۲','شمارش ADC که کانال دوم در جریان صفر می‌خواند و از هر نمونه کم می‌شود؛ اگر بی‌بار عدد جریان ۲ صفر نیست، این را تنظیم کنید.'],
 2:['ضریب جریان ۱','مقیاس محور جدول توان کانال ۱ (‰). با جدول کالیبراسیون جفت است — تغییرش خوانش جریان را بی‌صدا غلط می‌کند.'],
 3:['ضریب جریان ۲','مقیاس محور جدول توان کانال ۲ (‰). با جدول کالیبراسیون جفت است — تغییرش خوانش جریان ۲ را بی‌صدا غلط می‌کند.'],
 4:['آفست ولتاژ ورودی','عدد ثابتی که به ولتاژ ورودی اضافه/کم می‌شود (mV). فقط خطای جمعی را می‌گیرد، نه خطای ضربی.'],
 5:['آفست ولتاژ پک','عدد ثابتی که به ولتاژ پک ۲۴ ولت اضافه/کم می‌شود (mV). فقط خطای جمعی را می‌گیرد؛ خطای ضربی را باید از مرجع ADC درست کرد.'],
 6:['آفست ولتاژ ۱۲V','عدد ثابتی که به ولتاژ نقطهٔ میانی اضافه/کم می‌شود (mV). ولتاژ باتری بالا از تفریق همین عدد به دست می‌آید، پس روی هر دو نیمه اثر دارد.'],
 7:['پنجرهٔ median','چند نمونه را مرتب کرده و وسطی را برمی‌دارد؛ پرش‌های تک‌نمونه‌ای را می‌کشد بدون اینکه پله‌های واقعی را کند کند.'],
 8:['پنجرهٔ میانگین','چند نمونه میانگین گرفته شود؛ بزرگ‌تر یعنی آرام‌تر ولی کندتر.'],
 20:['حداکثر ولتاژ باتری (Absorb)','ولتاژی که شارژر در فاز Absorb روی آن نگه می‌دارد. هدف اصلی حلقهٔ ولتاژ.'],
 21:['آستانهٔ ورود به Absorb','از این ولتاژ به بالا از Bulk وارد Absorb می‌شود.'],
 22:['سقف تجاوز Absorb','اگر ولتاژ از این رد شد، duty سریع پایین کشیده می‌شود.'],
 23:['ولتاژ Float','ولتاژ نگه‌داری بعد از پر شدن؛ کمتر از Absorb تا باتری نجوشد.'],
 24:['ولتاژ بازگشت به Bulk','اگر باتری تا این حد افت کرد، دوباره از Bulk شروع می‌شود.'],
 25:['جریان Bulk','سقف جریان فاز Bulk؛ هدف حلقهٔ جریان. حد سخت ۹۵۰ میلی‌آمپر جداست.'],
 26:['جریان Taper','زیر این جریان در Absorb، شارژ تمام‌شده حساب می‌شود.'],
 27:['ولتاژ قطع باتری','بالاتر از این و بدون جریان یعنی باتری جدا شده.'],
 28:['دیبانس قطع','چند میلی‌ثانیه شرط بالا باید برقرار بماند تا آلارم بزند؛ کوتاه یعنی آلارم فیک.'],
 29:['ولتاژ نبود باتری','زیر این یعنی اصلاً باتری وصل نیست.'],
 30:['ولتاژ بازگشت باتری','بالای این یعنی باتری دوباره وصل شده.'],
 31:['دیبانس نبود باتری','مدت لازم برای قطعی‌شدن تشخیص «نیست».'],
 32:['زمان ریکاوری','چقدر بعد از رفع مشکل صبر کند تا دوباره عادی شود.'],
 33:['کمینهٔ ورودی سالم','زیر این ولتاژ، ورودی معتبر نیست و شارژ شروع نمی‌شود.'],
 34:['بیشینهٔ ورودی سالم','بالای این، ورودی بیش از حد است و شارژ متوقف می‌شود.'],
 35:['Hard fault جریان','از این جریان رد شود، کانال فوراً خاموش می‌شود. آخرین خط دفاع.'],
 36:['قطع اضافه‌ولتاژ OV','از این ولتاژ رد شود، duty صفر می‌شود. پیش‌فرض کمی زیر سقف مطلق تا زودتر تصمیم بگیرد.'],
 37:['کف اعتبار باتری','زیر این ولتاژ، عدد باتری معتبر شمرده نمی‌شود.'],
 70:['آستانهٔ اضافه‌ولتاژ UI','از این بالاتر، رابط هشدار اضافه‌ولتاژ می‌دهد.'],
 71:['hysteresis اضافه‌ولتاژ','چقدر باید پایین بیاید تا هشدار برداشته شود؛ جلوی چشمک‌زدن را می‌گیرد.'],
 72:['بازنشسته (۷۲)','این شناسه از v1.74 بازنشسته است: آستانهٔ باتری کم به‌صورت ثابت داخل ماژول چنج‌اور رفت و دیگر از پنل تنظیم نمی‌شود.'],
 73:['بازنشسته (۷۳)','این شناسه از v1.74 بازنشسته است: سطح پاک‌شدن باتری کم به‌صورت ثابت داخل ماژول چنج‌اور رفت و دیگر از پنل تنظیم نمی‌شود.'],
 74:['ولتاژ ۰٪ — سمت دشارژ','نردبان درصدِ دشارژ: کف آن. باندهای بوق و چشمک سبز از این ساخته می‌شوند (سمت شارژ جفت جدا دارد: ۱۱۹/۱۲۰).'],
 75:['ولتاژ ۱۰۰٪ — سمت دشارژ','سقف نردبان درصدِ دشارژ: ولتاژی که صد درصد حساب می‌شود. باندهای بوق و چشمک سبز بین این و حد پایین تقسیم می‌شوند (سمت شارژ جفت جدا دارد: ۱۱۹/۱۲۰).'],
 119:['ولتاژ ۰٪ — سمت شارژ','نردبان درصدِ شارژ، مستقل از نردبان دشارژ (۷۴/۷۵). درصدی که حین شارژ نشان داده می‌شود و نقطهٔ فول از این حساب می‌شوند.'],
 120:['ولتاژ ۱۰۰٪ — سمت شارژ','سقف نردبان درصدِ شارژ. جابه‌جاکردن آن، باندهای دشارژ را تکان نمی‌دهد.'],
 121:['مدت هر بوق — باند ۲','طول تک‌تک بوق‌های باند دو-بوق. از v1.50 مستقل از باند ۱ است، پس شکل‌دادن بوق دوتایی، بوق تکی را عوض نمی‌کند.'],
 123:['دورهٔ چشمک قرمز — قفل عدم‌توازن','یک دور کامل خاموش/روشن چراغ قرمز در حالت قفل. پیش‌فرض ۱۰۰۰ms؛ صفر یعنی قرمزِ ثابتِ قدیمی. (به دستور کاربر، قفل باید چشمک بزند نه اینکه ثابت بماند.)'],
 124:['سهم روشنی چشمک — قفل عدم‌توازن','چند درصد از هر دوره چراغ روشن باشد. پیش‌فرض ۵۰٪ یعنی روشن و خاموش برابر؛ ۲۰٪ یعنی تک‌تک کوتاه روی زمینهٔ خاموش.'],
 125:['مهلت شارژ پیوسته تا حکم خرابی','اگر کانال این‌قدر پیوسته شارژ کند و هرگز به پایان شارژ (Float) نرسد، باتری خراب اعلام می‌شود: قرمز ثابت و قطع شارژ تا تعویض باتری. پیش‌فرض ۸۶٬۴۰۰٬۰۰۰ms = ۲۴ ساعت. صفر یعنی این حکم خاموش است.'],
 126:['مهلت وقفه‌ای که ساعت را صفر می‌کند','وقفهٔ کوتاه‌تر از این مقدار ساعتِ شارژ را فقط نگه می‌دارد، وقفهٔ بلندتر آن را صفر می‌کند. پیش‌فرض ۶۰۰٬۰۰۰ms = ۱۰ دقیقه؛ بدون این عدد، باتریِ لرزان با روشن/خاموش‌شدن مدام برای همیشه از حکم فرار می‌کرد.'],
 128:['دورهٔ بوق — حکم باتری خراب','هر چند وقت یک‌بار بوقِ هشدارِ «باتری خراب» تکرار شود (ms). پیش‌فرض ۶۰۰٬۰۰۰ms = هر ۱۰ دقیقه؛ صفر یعنی این سناریو بی‌صدا باشد. تا v1.79 این عدد از قفل عدم‌توازن (۱۱۵) قرض گرفته می‌شد و قابل تنظیم نبود.'],
 129:['طول هر بوق — حکم باتری خراب','طول تک‌بوقی که در هر دوره پخش می‌شود (ms). پیش‌فرض ۱۲۰ms.'],
 130:['دورهٔ چشمک قرمز — حکم باتری خراب','یک دور کامل خاموش/روشن چراغ قرمز پس از حکم (ms). پیش‌فرض ۰ یعنی قرمزِ ثابت — همان چیزی که این سناریو را از چشمکِ قفل عدم‌توازن جدا می‌کند؛ اگر عوضش کنید این تمایز از بین می‌رود.'],
 131:['سهم روشنی چشمک — حکم باتری خراب','چند درصد از هر دورهٔ چشمک، چراغ روشن باشد. وقتی دورهٔ چشمک صفر (ثابت) است، این عدد بی‌اثر است.'],
 127:['جداسازی باتری از خروجی در قفل سناریو ۶','مثل تیک ۱۱۷ در عدم‌توازن: در قفلِ «باتری خراب» باتری از خروجی هم جدا شود یا فقط شارژش قطع بماند. پیش‌فرض: خاموش.'],
 122:['فاصلهٔ تکرار — باند ۲','هر چند وقت یک‌بار الگوی دو-بوقِ همین باند تکرار شود (ms). از نسخهٔ ۱٫۷۱ باند ۲ هم مثل بقیه فاصلهٔ خودش را دارد و فقط گپ (۶۵) مشترک است.'],
 83:['Kp جریان','واکنش فوری حلقهٔ جریان به خطا. زیاد یعنی تند و لرزان.'],
 84:['Ki جریان','خطای انباشته را جمع می‌کند؛ همان چیزی که در نهایت جریان را دقیقاً روی هدف می‌نشاند.'],
 85:['Kd جریان','واکنش به سرعت تغییر. صفر است چون اندازه‌گیری نویز دارد و D نویز را تقویت می‌کند.'],
 86:['نرخ صعود جریان','سقف سرعت بالارفتن duty. واحد: هزارمِ‌پرمیل در ثانیه - ۱۰۰۰ یعنی ۱ پرمیل‌برثانیه (رفتن از صفر تا سقف، ۱۰۰۰ ثانیه طول می‌کشد).'],
 87:['نرخ نزول جریان','سقف سرعت پایین‌آمدن duty؛ بالاتر یعنی ترمز تندتر. واحد همان هزارمِ‌پرمیل در ثانیه است (۱۰۰۰ = ۱‰/s)؛ معمولاً باید از نرخ صعود تندتر باشد.'],
 88:['Kp ولتاژ','همان Kp ولی برای حلقهٔ ولتاژ. عمداً کوچک است تا نویز ولتاژ پک تقویت نشود.'],
 89:['Ki ولتاژ','انباشت خطای ولتاژ؛ بزرگ‌ترین عامل کم‌کردن اورشوت.'],
 90:['Kd ولتاژ','صفر، به همان دلیل حلقهٔ جریان.'],
 91:['نرخ صعود ولتاژ','سقف سرعت بالارفتن duty در حلقهٔ ولتاژ؛ عمداً کوچک است تا هنگام تحویل CC به CV پرش نکند. واحد: هزارمِ‌پرمیل در ثانیه (۱۰۰۰ = ۱‰/s).'],
 92:['نرخ نزول ولتاژ','سقف سرعت کاهش duty وقتی ولتاژ از هدف رد می‌شود. واحد: هزارمِ‌پرمیل در ثانیه (۱۰۰۰ = ۱‰/s).'],
 93:['سقف زمان Absorb','بیشترین مدت مجاز Absorb (ms). صفر = بدون سقف. جزئیات کامل در تولتیپ تراشهٔ زیر نمودار.'],
 94:['جریان شروع شمارش سقف زمانی','سقف زمانی Absorb تا وقتی جریان بالاست اصلاً شروع نمی‌شود؛ از نقطه‌ای که جریان زیر این حد بیاید «شمارش آغاز می‌شود» (در نام‌گذاری فرمور: Arm یعنی آمادهٔ شمردن). واحد: mA.'],
 95:['کمینهٔ مدت Absorb','Absorb دست‌کم این مدت ادامه می‌یابد (ms). صفر = بدون نگه‌داشت اجباری.'],
 96:['پایداری Taper','جریان باید این مدت پیوسته زیر «جریان پایانی» بماند تا شارژ تمام اعلام شود (ms).'],
 97:['گام تغییر duty','بیشترین تغییر duty در هر تیک ۵۰ میلی‌ثانیه‌ای PID، به پرمیل (‰).'],
 98:['hysteresis خروجی PID','تقاضای جدید باید دست‌کم این‌قدر (میلی‌پرمیل) با duty کنونی فاصله داشته باشد تا خروجی عوض شود.'],
 99:['پنجرهٔ فیلتر ولتاژ','میانگین نمایی روی N نمونه؛ ثابت زمانی N × ۵۰ میلی‌ثانیه. ۱ = فیلتر خاموش.'],
 100:['ولتاژ آستانهٔ پشتیبان','از این ولتاژ به بالا، پشتیبانِ سقف duty فعال می‌شود (mV).'],
 101:['گین پشتیبان جریان','شدت برشِ سقف duty به ازای تجاوز جریان از Bulk max. صفر = خاموش.'],
 102:['گین پشتیبان ولتاژ','شدت برشِ سقف duty به ازای تجاوز ولتاژ از آستانهٔ پشتیبان. صفر = خاموش.'],
 103:['حاشیهٔ هدف جریان','هدف حلقهٔ جریان این‌قدر (mA) زیر Bulk max گذاشته می‌شود.'],
 104:['زمان نشست اتصال','پس از اتصال معتبر، این مدت (ms) صبر و بعد شارژ شروع می‌شود.'],
 105:['قفل پس از تریپ','پس از هر قطع اضطراری، کانال این مدت (ms) خاموش می‌ماند.'],
 106:['واچ‌داگ حالت دستی','اگر پنل این مدت (ms) ساکت بماند، دستور دستی خودکار رها می‌شود.'],
 107:['فاصلهٔ رِمپ پایین','فاصلهٔ زمانی (ms) بین پله‌های کاهش نرم duty.'],
108:['حد استراحت عدم‌توازن','حداکثر اختلاف مجاز دو نیم (mV) در استراحت/شارژ؛ پیش‌فرض ۳۰۰.'],
109:['حد دشارژ عدم‌توازن','حداکثر اختلاف مجاز دو نیم (mV) در دشارژ؛ پیش‌فرض ۵۰۰.'],
110:['صبر پس از پایان شارژ','سنجش استراحت پس از این مدت از پایان شارژ (ms)؛ ۰=خاموش؛ پیش‌فرض ۱۰ دقیقه.'],
111:['صبر پس از شروع شارژ','سنجش حین شارژ پس از این مدت از شروع (ms)؛ ۰=خاموش؛ پیش‌فرض ۱۰ دقیقه.'],
112:['پایداری رویداد','بالای‌حد پیوسته به این مدت (ms) یک رویداد است؛ پیش‌فرض ۳۰ ثانیه.'],
113:['Hysteresis رویداد','اپیزود در حد منهای این مقدار (mV) بسته می‌شود؛ پیش‌فرض ۱۰۰.'],
114:['سقف رویداد تا قفل','پس از این‌قدر رویداد ماندگار، قفل؛ پیش‌فرض ۱۰.'],
115:['دورهٔ بوق در قفل','فاصلهٔ دو بوق در قفل (ms)؛ ۰=بی‌صدا؛ پیش‌فرض یک ساعت.'],
116:['طول بوق قفل','مدت هر بوق در قفل (ms)؛ پیش‌فرض ۲۰۰.'],
117:['تیک مسدودی خروجی','۱ = مسدودی خروجی در قفل، ۰ = ریسک با شما؛ پیش‌فرض ۱.'],
118:['سیکل‌های شارژ پس از قفل','سیکل‌های شارژ مجاز پس از قفل تا قطع شارژ؛ پیش‌فرض ۲۰.']};

// [EN] v1.36 (user order: the chip tooltips must be EXPLICIT - plain
//   language, real units, and what bigger vs smaller actually does. The
//   gains especially: they are feed-forward ceiling cuts, not PID gains,
//   and 0 disables them. v1.73 removed the "!" help bubbles, so these
//   texts now feed the chip title tooltips under the chart and the event
//   popup only - keep them short enough for a tooltip.
//   [FA] v1.36 (دستور کاربر: تولتیپ تراشه‌ها باید شفاف شود - زبان ساده،
//   واحد واقعی، و اینکه بیشتر/کمتر دقیقاً چه می‌کند؛ مخصوصاً گین‌ها:
//   آن‌ها برش تناسبی «سقف duty» هستند، نه ضرایب PID، و صفر یعنی خاموش.
//   از v1.73 حباب‌های «!» حذف شده‌اند، پس این متن‌ها فقط در تولتیپ
//   تراشه‌های زیر نمودار و پنجرهٔ رویداد دیده می‌شوند؛ کوتاه بمانند.)
//   هشدار: این سطرها کامنت JS با نشانهٔ «//» هستند؛ ناظر ثابتی، نشانه‌های
//   مارک‌آپ را فقط در این شکلِ کامنت می‌پذیرد (قاعدهٔ «در کامنت HTML ننویس»).
// [EN] v1.40 (user order: "I understand NONE of these with your definitions -
//   explain SIMPLY, with a NUMERIC EXAMPLE"). Every tooltip now follows the
//   same fixed pattern: one plain sentence, one worked example computed with
//   the actual factory defaults, then what bigger/smaller does. PID decides
//   the destination; these parameters only gate HOW SAFELY/SMOOTHLY we get
//   there - each tooltip says so plainly.
//   [FA] v1.40 (دستور کاربر: «با این تعاریف هیچ‌کدام را نمی‌فهمم؛ ساده با
//   مثال عددی توضیح بده»). الگوی ثابت برای همه: یک جملهٔ ساده + یک مثال
//   عددی با همان اعداد کارخانه + اثر بیشتر/کمتر. PID مقصد را تعیین می‌کند؛
//   این پارامترها فقط نگهبانِ «چطوری امن و نرم برسیم»‌اند.
const PXT={
 93:'سقف زمان ‎Absorb‎. اگر شارژ خودش تمام نشد، بعد از این مدت «تمام» اعلام می‌شود. مثال: سقف ۶۰ دقیقه؛ جریان روی ‎۳۰mA‎ گیر کرده و پایین نمی‌آید، پس سر دقیقهٔ ۶۰ بسته می‌شود. شمارش از پارامتر ۹۴ شروع می‌شود. ۰ یعنی بدون سقف.',
 94:'شمارشِ سقفِ پارامتر ۹۳ تا وقتی جریان بالای این حد است شروع نمی‌شود. مثال: حد ‎۱۰۰mA‎؛ با جریان ‎۱۲۰mA‎ هنوز شروع نمی‌شود؛ با ‎۹۵mA‎ شروع می‌شود.',
 95:'کف زمان ‎Absorb‎: دست‌کم این مدت ادامه می‌دهد. مثال: کف ۱۰ دقیقه؛ اگر دقیقهٔ ۳ جریان به ‎۲۰mA‎ بیفتد، باز تا دقیقهٔ ۱۰ ادامه دارد. ۰ یعنی بدون کف.',
 96:'برای اعلام «شارژ کامل»، جریان باید این مدت پیوسته زیر «جریان پایانی» بماند. مثال: ۶۰ ثانیه؛ یک پرش لحظه‌ای به ‎۶۰mA‎ شمارش را صفر می‌کند. کمتر = زودتر، بیشتر = مطمئن‌تر.',
 97:'سرعت حرکت ‎duty‎ به‌سوی هدفِ ‎PID‎. مثال: ۸ پرمیل در هر تیک ۵۰ میلی‌ثانیه؛ برای ۱۰۰ پرمیل تغییر حدود ۱۳ تیک (≈۰٫۶ ثانیه) لازم است. بیشتر = تندتر ولی پرشی، کمتر = نرم‌تر.',
 98:'تکان‌های خیلی ریزِ ‎PID‎ نادیده گرفته می‌شود تا ‎PWM‎ نلرزد. واحد: میلی‌پرمیل. مثال: ۷۰۰ یعنی تغییر کمتر از ۰٫۷ پرمیل اعمال نمی‌شود؛ باقی‌مانده گم نمی‌شود و جمع می‌شود.',
 99:'نرم‌کردن عدد ولتاژ باتری پیش از ‎PID‎. مثال: ۳۲ یعنی میانگین ۳۲ نمونهٔ آخر، حدود ۱٫۶ ثانیه؛ یک افت واقعی ‎۵۰mV‎ چند ثانیه طول می‌کشد تا دیده شود. ۱ یعنی فیلتر خاموش.',
 100:'از این ولتاژ بالاتر، ترمز اضطراری ولتاژ وارد می‌شود. مثال: ‎۱۴۸۰۰mV‎؛ زیر آن خبری نیست، بالای آن سقف ‎duty‎ بریده می‌شود و شدتش با پارامتر ۱۰۲ تنظیم می‌شود.',
 101:'شدت ترمز اضطراری جریان. مثال: ۱۰۰ یعنی هر ‎۱mA‎ عبور از سقف ‎Bulk‎، سقف ‎duty‎ را ۰٫۱ پرمیل پایین می‌کشد؛ پس ‎۱۰۰mA‎ تجاوز یعنی ۱۰ پرمیل پایین. ۰ یعنی خاموش.',
 102:'شدت ترمز اضطراری ولتاژ. مثال: ۵۰۰ یعنی هر ‎۱mV‎ بالاتر از آستانهٔ ۱۰۰، سقف ‎duty‎ را ۰٫۵ پرمیل پایین می‌آورد؛ ‎۱۰۰mV‎ تجاوز یعنی ۵۰ پرمیل پایین. ۰ یعنی خاموش.',
 103:'فاصلهٔ ایمن تا سقف جریان. هدف حلقه = سقف ‎Bulk‎ منهای این عدد. مثال: سقف ‎۶۵۰mA‎ و فاصلهٔ ۱۰ یعنی قفل روی ‎۶۴۰mA‎، تا موج کوچک ترمز ۱۰۱ را بی‌دلیل روشن نکند.',
 104:'بعد از وصل باتری این‌قدر صبر می‌کند تا ولتاژ بنشیند، بعد ‎Bulk‎ شروع می‌شود. مثال: ۱۵ ثانیه. واحد: میلی‌ثانیه. کمتر = شروع سریع‌تر ولی پرریسک‌تر.',
 105:'بعد از هر تریپ، این مدت خاموش می‌ماند و بعد وصل می‌شود. مثال: ۳ ثانیه؛ جلوی وصل و قطع پشت‌سرهم را می‌گیرد. ۰ یعنی بدون قفل. واحد: میلی‌ثانیه.',
 106:'نگهبان مود دستی: اگر پنل این مدت ساکت بماند، فرمان دستی باطل می‌شود. مثال: ۳ ثانیه؛ با قطع مرورگر خروجی خودبه‌خود صفر می‌شود. واحد: میلی‌ثانیه.',
 107:'خاموشی نرم: ‎duty‎ هر این‌قدر میلی‌ثانیه یک پله پایین می‌آید تا جریان یک‌باره صفر نشود. مثال: ‎۵۰۰ms‎ آرام، ‎۱۰۰ms‎ پنج برابر تندتر.',
 108:'بیشترین اختلاف مجاز دو نیمِ باتری، در استراحت و حین شارژ. مثال: حد ‎۳۰۰mV‎؛ ‎۱۳٫۱۰‎ و ‎۱۲٫۷۵‎ ولت یعنی ‎۳۵۰mV‎ و خارج از حد. واحد: ‎mV‎.',
 109:'همان حد ولی روی بار (دشارژ) که معمولاً بزرگ‌تر است. مثال: حد ‎۵۰۰mV‎؛ ‎۳۵۰‎ سالم است، ‎۶۰۰‎ اگر ۳۰ ثانیه بماند رویداد می‌سازد. واحد: ‎mV‎.',
 110:'اندازه‌گیری استراحت فقط تا این مدت بعد از پایان شارژ انجام می‌شود. مثال: ۱۰ دقیقه. ۰ یعنی خاموش. واحد: میلی‌ثانیه.',
 111:'اندازه‌گیری حین شارژ فقط تا این مدت بعد از شروع انجام می‌شود، با حد پارامتر ۱۰۸. مثال: ۱۰ دقیقه. ۰ یعنی خاموش. واحد: میلی‌ثانیه.',
 112:'اختلاف باید این مدت پیوسته بالا بماند تا جدی گرفته شود. مثال: ۳۰ ثانیه. واحد: میلی‌ثانیه.',
 113:'رویداد وقتی بسته می‌شود که اختلاف به «حد منهای این عدد» برسد. مثال: حد ۳۰۰ و این عدد ۱۰۰ یعنی بسته‌شدن در ‎۲۰۰mV‎. واحد: ‎mV‎.',
 114:'با این تعداد رویداد ماندگار، باتری قفل می‌شود. قطع برق هم پاکش نمی‌کند. مثال: ۱۰.',
 115:'در حالت قفل، هر این‌قدر یک بوق کوتاه زده می‌شود. مثال: یک ساعت. ۰ یعنی بی‌صدا. واحد: میلی‌ثانیه.',
 116:'طول هر بوق در حالت قفل. مثال: ‎۲۰۰ms‎. واحد: میلی‌ثانیه.',
 117:'اگر تیک بخورد، در حالت قفل باتری از خروجی جدا می‌شود. پیش‌فرض: روشن. برداشتن تیک یعنی پذیرفتن ریسک.',
 118:'بعد از قفل، فقط این تعداد سیکل شارژ مجاز است و بعد شارژ هم می‌ایستد. مثال: ۲۰.'};

/* [EN] v1.36: the fuller v1.36 text (PXT) wins, else the classic PX row,
   so help bubbles and chips always agree.
   [FA] v1.36: متن کامل v1.36 اگر هست همان، وگرنه ردیف کلاسیک PX - تا حباب
   کمک و تراشه همیشه یک حرف بزنند. */
const pxt=id=>(typeof PXT!=='undefined'&&PXT[id])?[EVN[id]||((PX[id]||['',''])[0]),PXT[id]]:PX[id];
/* [EN] v1.31: the chart is mounted in two places - above the operating table
   on the chargers page (where you read the numbers, so where you reach for
   them) and next to the profile fields in settings (where the dashed
   "typed but not applied" preview is worth seeing). ONE renderer fills every
   mount, so the two can never say different things; adding or moving a mount
   is markup only. The containers are classes, not ids, for exactly that
   reason - $('qg') would have silently filled only the first.
   [FA] نمودار در دو جا نصب شده - بالای جدول عملکرد در صفحهٔ شارژرها (جایی که
   اعداد را می‌خوانید، پس همان‌جا سراغشان می‌روید) و کنار فیلدهای profile در
   تنظیمات (جایی که پیش‌نمایش خط‌چینِ «تایپ‌شده ولی هنوز اعمال‌نشده» ارزش
   دیدن دارد). یک رندرکننده همهٔ محل‌ها را پر می‌کند، پس آن دو هرگز نمی‌توانند
   دو چیز متفاوت بگویند؛ افزودن یا جابه‌جاکردن یک محل فقط مارک‌آپ است. */
function qgraph(){const MG=document.querySelectorAll('.qgm');
 if(!MG.length||EVOPEN!=null)return;const g=MG[0];
 const q={a:qv(20),e:qv(21),o:qv(22),f:qv(23),r:qv(24)},im=qv(25),tp=qv(26);
 /* v3 (دستور کاربر ۲۰۲۶-۰۹-۲۹): نمودار دوبعدی جریان-ولتاژ.
    محور افقی = جریان (mA)، محور عمودی = ولتاژ (mV).
    قبلاً فقط یک نردبان عمودی ولتاژ بود و جریان اصلاً روی نمودار نبود؛ حالا
    مسیر واقعی شارژ ‎CC/CV‎ دیده می‌شود: Bulk یک خط عمودی روی سقف جریان است
    (جریان ثابت، ولتاژ بالا می‌رود) و Absorb یک خط افقی روی ولتاژ هدف
    (ولتاژ ثابت، جریان پایین می‌آید). نقطهٔ زندهٔ هر باتری روی مختصات واقعی
    خودش (جریان، ولتاژ) می‌نشیند، پس یک نگاه می‌گوید کجای مسیر است.
    v3: a real current-voltage chart. X = current, Y = voltage. The CC/CV
    path is now visible as what it is - a vertical leg at the current limit
    and a horizontal leg at the absorb voltage - and each battery's live dot
    sits at its true (I, V), so one look says where it is on that path.
    ناحیه‌ها از مقادیر اعمال‌شده (.d) می‌آیند؛ تایپِ هنوز-اعمال‌نشده فقط خط‌چین */
 const lo=Math.max(7600,Math.min(q.r.d,12000)-500),hi=15060,W=760,H=420,X0=48,X1=742;
 const LBL_GAP=11;
 /* [EN] v1.36 (user order: chart texts are FAR too big on some browsers):
    the SVG stretches with the page width, and SVG scales font-size along
    with everything else - on a wide browser window an 8px label renders
    at ~15px. Therefore the on-screen font size is DYNAMIC: measure the
    real mount width and shrink every size by W/width so it renders close
    to the designed pixel size on every browser. Shrink-only (never grow),
    floor 6.5 so an ultra-wide window cannot make it unreadable.
    [FA] v1.36 (دستور کاربر: نوشته‌های نمودار روی بعضی مرورگرها خیلی درشت
    است): چون SVG خودش را با پهنای صفحه می‌کشد، فونت هم با آن بزرگ می‌شود؛
    پس اندازهٔ فونت داینامیک شد: با پهنای واقعی محل نصب سنجیده و هر
    اندازه در ‎W/width‎ ضرب می‌شود تا روی صفحه نزدیک همان پیکسلِ طراحی بماند.
    فقط کوچک‌کننده (هرگز بزرگ نمی‌کند) و کف ۶٫۵ تا روی صفحهٔ خیلی عریض
    ناخوانا نشود. */
 const RWM=(MG.length&&MG[0].clientWidth)?MG[0].clientWidth:0;
 const FKF=RWM>W?W/RWM:1;
 const F=fs=>Math.max(6.5,+(fs*FKF).toFixed(2));
 const IMAX=Math.max(200,Math.round(Math.max((im.d||650)*1.25,(evval(35)||0)*1.05)/50)*50);
 const Y=mv=>Math.round(H-24-(H-46)*(mv-lo)/(hi-lo));
 const X=ma=>Math.round(X0+(X1-X0)*Math.min(Math.max(ma,0),IMAX)/IMAX);
 const V=mv=>(mv/1000).toFixed(2);
 /* برچسب‌ها جدا جمع و با کمینهٔ فاصله رندر می‌شوند تا در ناحیه‌های باریک در هم نروند */
 const LL=[],PL=[]; /* [EN] v1.41 audit: zone-name container ZL died with the v1.39 legend move / [FA] ممیزی v1.41: ظرف ZL با رفتن نام‌ها به راهنما مرده بود */
 /* [EN] v1.38 (user follow-up): the Persian left label carries ONLY the
    name - number+unit live on the right English label; it hangs BELOW its
    line (never above); and each line gets exactly ONE Persian label. Zone
    names float at the CENTER of their band so a band name can never look
    like a second label for the boundary line touching it. Names come from
    PX (the same source the chips and help bubbles read), so a rename can
    never drift in two places. Left labels are display mirrors: editing
    stays with the English labels on the right.
    [FA] v1.38 (دستور پیگیری): برچسب فارسیِ چپ فقط نام است - عدد و واحد
    همان برچسب انگلیسی سمت راست؛ زیر خط خودش می‌آویزد (هرگز بالای آن نیست)
    و هر خط دقیقاً یک برچسب فارسی دارد. نام ناحیه‌ها هم وسط ناحیهٔ خودشان
    شناور است تا اسم ناحیه، دومین برچسب خط مرزیِ مجاور به نظر نرسد. نام‌ها
    از PX می‌آیند (همان منبع تراشه‌ها و حباب‌های کمک) تا تکرار دوبرابری
    نشود. برچسب‌های چپ فقط نمایشی‌اند؛ ویرایش همان انگلیسی سمت راست است. */
 const pfn=id=>(PX[id]?PX[id][0]:(EVN[id]||''));
 /* [EN] v1.39 (user order: the band names crowded the plot - take them out
    and put them OUTSIDE the chart, like a map legend: which colour means
    what). ONE definition list drives both the painted bands and the legend,
    so a colour and its meaning can never drift apart.
    [FA] v1.39 (دستور کاربر: نام ناحیه‌ها نمودار را شلوغ کرده - ببر بیرون
    نمودار مثل راهنما). یک فهرست واحد هم باندها را رنگ می‌کند هم راهنما را
    می‌سازد تا رنگ و معنا هیچ‌وقت از هم کپی نشوند. */
 const ZONES=[[()=>hi,()=>15000,'rgba(251,94,106,.16)','بالای ۱۵ ولت — ناحیهٔ خطر'],
  [()=>15000,()=>q.o.d,'rgba(251,94,106,.09)','ناحیهٔ Over — کاهش سریع duty'],
  [()=>q.o.d,()=>q.e.d,'rgba(251,191,36,.08)','ناحیهٔ Absorb'],
  [()=>q.e.d,()=>q.f.d,'HATCH','ناحیهٔ Bulk — شارژ با جریان ثابت'],
  [()=>q.f.d,()=>q.r.d,'rgba(52,211,153,.09)','ناحیهٔ Float'],
  [()=>q.r.d,()=>lo,'rgba(99,162,255,.11)','زیر Reentry — شارژ دوباره از Bulk']];
 const zone=(mv1,mv2,fill)=>{const y1=Y(Math.max(mv1,mv2)),y2=Y(Math.min(mv1,mv2));
  return`<rect x="${X0}" y="${y1}" width="${X1-X0}" height="${Math.max(3,y2-y1)}" fill="${fill}"/>`;};
 const aln=(mv,c)=>`<line x1="${X0}" y1="${Y(mv)}" x2="${X1}" y2="${Y(mv)}" stroke="${c}" stroke-width="1.8"/>`;
 const pvln=(o,c)=>o.p?`<line x1="${X0}" y1="${Y(o.v)}" x2="${X1}" y2="${Y(o.v)}" stroke="${c}" stroke-width="1.8" stroke-dasharray="6 4"/>`:'';
 const lbl=(o,c,txt,id)=>{LL.push({y:Y(o.p?o.v:o.d)-4,txt,c,pv:o.p,i:id,val:V(o.d)+'V'});
  PL.push({y:Y(o.p?o.v:o.d)+10,txt:pfn(id),c,pv:o.p});};
 const put=(A,x,anchor,fs)=>{A.sort((p,q2)=>p.y-q2.y);let last=4;
  return A.map(o=>{const yc=Math.min(Math.max(o.y,14),H-10),y=Math.max(last+LBL_GAP,yc),sh=y-yc>3;last=y;
   return (sh?`<line x1="${x}" y1="${yc+3}" x2="${x}" y2="${y-3}" stroke="${o.c}" stroke-width="1" opacity=".6"/>`:'')+
   `<text ${o.i!=null?evat(o.i):''} x="${x}" y="${y}" text-anchor="${anchor}" font-size="${F(fs)}" font-weight="700" fill="${o.i!=null&&evcl(o.i)?'#f7c13c':o.c}">${o.txt}${o.val?' '+o.val:''}${o.pv?' · پیش‌نمایش':''}</text>`;}).join('');};
 let s=`<svg viewBox="0 0 ${W} ${H}" style="width:100%;min-width:640px;font-family:inherit">`;
 s+=`<defs><pattern id="bkh" width="9" height="9" patternTransform="rotate(45)" patternUnits="userSpaceOnUse"><rect width="9" height="9" fill="rgba(99,162,255,.08)"/><line x1="0" y1="0" x2="0" y2="9" stroke="rgba(99,162,255,.22)" stroke-width="1"/></pattern></defs>`;
 s+=`<rect x="${X0}" y="12" width="${X1-X0}" height="${H-34}" fill="#080b12" stroke="#38455e" rx="6"/>`;
 /* شبکهٔ ولتاژ (افقی) */
 for(let mv=Math.ceil(lo/500)*500;mv<=hi;mv+=500){const y=Y(mv);
  s+=`<line x1="${X0}" y1="${y}" x2="${X1}" y2="${y}" stroke="#26304a" stroke-width="1"/>`+
     `<text x="${X0-4}" y="${y+3}" text-anchor="end" font-size="${F(8)}" fill="#96a1b8">${(mv/1000).toFixed(1)}</text>`;}
 /* شبکهٔ جریان (عمودی) — محور افقی تازه */
 const istep=IMAX>600?200:100;
 for(let ma=0;ma<=IMAX;ma+=istep){const x=X(ma);
  s+=`<line x1="${x}" y1="12" x2="${x}" y2="${H-22}" stroke="#26304a" stroke-width="1"/>`+
     `<text x="${x}" y="${H-12}" text-anchor="middle" font-size="${F(8)}" fill="#96a1b8">${ma}</text>`;}
 ZONES.forEach(z=>{s+=zone(z[0](),z[1](),z[2]==='HATCH'?'url(#bkh)':z[2]);});
 s+=`<line x1="${X0}" y1="${Y(15000)}" x2="${X1}" y2="${Y(15000)}" stroke="#ff6873" stroke-width="1.2" stroke-dasharray="3 4"/>`;
 LL.push({y:Y(15000)-4,txt:'Hard cutoff 15V',c:'#ff6873'});
 PL.push({y:Y(15000)+10,txt:'سقف سخت',c:'#ff6873'});
 /* [EN] Two safety ceilings that were only ever numbers in a card: on the
       voltage axis they belong, and now they are set from it. Down-only on
       the board, so the window in EVB can never raise them.
    [FA] دو سقف ایمنی که همیشه فقط عددی در یک کادر بودند: جایشان روی محور
       ولتاژ است و حالا از همان‌جا تنظیم می‌شوند. روی برد فقط پایین‌آوردنی‌اند. */
 Object.keys(EVV).forEach(k=>{const id=+k;if(id<35)return;const mv=evval(id);
  if(mv==null||mv<=lo||mv>=hi)return;
  s+=`<line x1="${X0}" y1="${Y(mv)}" x2="${X1}" y2="${Y(mv)}" stroke="${EVV[id][0]}" stroke-width="1.3" stroke-dasharray="5 3"/>`;
  LL.push({y:Y(mv)-4,txt:EVV[id][1],c:EVV[id][0],i:id,val:V(mv)+'V'});
  PL.push({y:Y(mv)+10,txt:pfn(id),c:EVV[id][0]});});
 s+=aln(q.o.d,'#fb923c')+pvln(q.o,'#fb923c');lbl(q.o,'#fb923c','Over',22);
 s+=aln(q.a.d,'#f7c13c')+pvln(q.a,'#f7c13c');lbl(q.a,'#f7c13c','Absorb',20);
 s+=aln(q.e.d,'#e8a33d')+pvln(q.e,'#e8a33d');lbl(q.e,'#e8a33d','Absorb enter',21);
 s+=aln(q.f.d,'#35d6a0')+pvln(q.f,'#35d6a0');lbl(q.f,'#35d6a0','Float',23);
 s+=aln(q.r.d,'#63a2ff')+pvln(q.r,'#63a2ff');lbl(q.r,'#63a2ff','Reentry',24);
 /* [EN] The current axis carries four settable thresholds now. Every one is
       clickable, including the hard fault - which is why IMAX above had to
       stop being derived from the bulk limit alone: a current chart that
       clips a current limit at its right edge is a quiet lie.
    [FA] محور جریان حالا چهار آستانهٔ تنظیم‌شدنی دارد و همه کلیک‌پذیرند، از
       جمله Hard fault - و دقیقاً به همین دلیل IMAX بالا دیگر فقط از Bulk max
       مشتق نمی‌شود: نمودار جریانی که یک حد جریان را لب راستش ببُرد، یک دروغ
       بی‌صداست. */
 Object.keys(EVI).forEach(k=>{const id=+k;
  const ma=(id===25)?im.d:(id===26)?tp.d:evval(id);
  if(ma==null)return;const x=X(ma),cfg=EVI[id];
  /* [EN] v1.37 (user order: the current-limit numbers read badly in one
     run) - each threshold label is TWO lines: the name on top and the value
     underneath it, so 'Bulk max' and '650mA' never fight for the same line.
     [FA] v1.37 (دستور کاربر): برچسب حد جریان دوخطی شد: نام در خط بالا و
     عدد در خط زیرین، تا خواناتر شود. */
  const lx=x+(cfg[2]<0?-3:3),an=cfg[2]<0?'end':'start';
  s+=`<line x1="${x}" y1="12" x2="${x}" y2="${H-22}" stroke="${cfg[0]}" stroke-width="${id===25?1.6:1.4}"${id===25?'':' stroke-dasharray="4 3"'}/>`+
     `<text ${evat(id)} x="${lx}" y="${H-26}" text-anchor="${an}" font-size="${F(8.5)}" font-weight="700" fill="${evcl(id)?'#f7c13c':cfg[0]}"><tspan x="${lx}" dy="-9">${cfg[1]}</tspan><tspan x="${lx}" dy="9">${ma}mA</tspan></text>`;});
 /* [EN] The edge of fitted LUT data. Only drawn once the axis actually
    reaches it, so the normal single-battery view is unchanged; it appears
    the moment the user opens the current band towards a parallel pack.
    Read-only on purpose - it is where the bench evidence stops.
    [FA] لبهٔ دادهٔ برازش‌شده. فقط وقتی رسم می‌شود که محور واقعاً به آن برسد،
    پس نمای عادی تک‌باتری دست‌نخورده می‌ماند و همین که کاربر باند جریان را
    به‌سمت پک موازی باز کند ظاهر می‌شود. */
 if(IMAX>EVCAL){const xc=X(EVCAL);
  s+=`<line x1="${xc}" y1="12" x2="${xc}" y2="${H-22}" stroke="#7f8ba3" stroke-width="1" stroke-dasharray="2 4" stroke-opacity=".8"/>`+
     `<text x="${xc+3}" y="20" font-size="${F(8)}" fill="#7f8ba3">تا اینجا کالیبره شده · بالاتر برون‌یابی است</text>`;}
 /* مسیر واقعی شارژ ‎CC/CV‎: پای عمودی روی سقف جریان، بعد پای افقی روی ولتاژ Absorb */
 s+=`<polyline points="${X(im.d)},${Y(lo)} ${X(im.d)},${Y(q.e.d)} ${X(im.d)},${Y(q.a.d)} ${X(tp.d)},${Y(q.a.d)} ${X(0)+6},${Y(q.f.d)}" fill="none" stroke="#e8eaf2" stroke-width="2" stroke-opacity=".55" stroke-linejoin="round" stroke-dasharray="7 4"/>`;
 /* موقعیت زندهٔ هر باتری: حالا روی مختصات واقعی (جریان، ولتاژ).
    دستور کاربر ۲۰۲۶-۱۰-۰۳: عددهای شارژ زنده باید «روی همهٔ خط‌ها و
    نوشته‌ها» بیایند تا گم نشوند و خوانا بمانند. پس خطوط راهنما و نقطه
    همین‌جا و زیر برچسب‌ها رسم می‌شوند، ولی متن عددها جمع می‌شود و در
    انتها - بعد از ‎put(PL/LL)‎ و همهٔ محورها - رسم می‌گردد، با هالهٔ
    تیره (‎paint-order)‎ تا روی هر منحنی/برچسبی خوانا بماند. */
 let lg='';const BT=[];
 if(D&&D.t){const tt=D.t;
  const BST={0:['Off','#96a1b8'],1:['Bulk','#63a2ff'],2:['Absorb','#f7c13c'],3:['Float','#35d6a0'],4:['Bring-up','#9ac8ff'],5:['JIT wait','#96a1b8'],6:['Input wait','#96a1b8'],7:['Final fault','#ff6873'],8:['Battery lost','#ff6873'],9:['Manual','#c084fc']};
  const bats=[['Battery low (Vlow)',tt[17],tt[13],tt[10],'#c084fc',0.60],['Battery high (Vhigh)',tt[18],tt[6],tt[3],'#f7c13c',0.82]];
  bats.forEach(b=>{
   if(b[1]>lo&&b[1]<hi){const y=Y(b[1]),x=X(b[3]);
    s+=`<line x1="${X0}" y1="${y}" x2="${x}" y2="${y}" stroke="${b[4]}" stroke-width="1.2" stroke-dasharray="2 3"/>`;
    s+=`<line x1="${x}" y1="${y}" x2="${x}" y2="${H-22}" stroke="${b[4]}" stroke-width="1.2" stroke-dasharray="2 3"/>`;
    s+=`<circle cx="${x}" cy="${y}" r="4.5" fill="${b[4]}" stroke="#080b12" stroke-width="1.8"/>`;
    BT.push({x:x,y:Math.min(y+15,H-28),c:b[4],t:b[0].split(' (')[0]+' '+V(b[1])+'V · '+b[3]+'mA'});}});
  lg=bats.map(b=>{const st=BST[b[2]]||('#'+b[2]);
   return `<span class="tg" style="background:${st[1]}22;color:${st[1]};border:1px solid ${st[1]}66">● ${b[0]}: <b>${V(b[1])}V</b> · ${b[3]}mA · ${st[0]}</span>`;}).join(' ')+
   `<span class="lb"> · Bulk ≤ ${im.v}mA · Taper < ${tp.v}mA · پس از هر تغییر ~۱٫۵ ثانیه بعد روی فلش برد ذخیره می‌شود</span>`;
 }else lg='در انتظار دادهٔ برد…';
 /* [EN] v1.32 (user order): the loop and timing numbers go ON the plot, each
    one next to the line it actually acts on - not listed underneath it. A
    tspan carries data-i exactly like an axis label does, so the same click
    handler and the same floating editor serve them with no second mechanism.
    They are deliberately quiet: small and muted until you hover, because
    these are the numbers you touch once, and the chart still has to be
    readable at a glance for the ones you watch.
    [FA] اعداد حلقه و زمان‌بندی روی خود نمودار می‌آیند، هر کدام کنار همان خطی
    که واقعاً رویش اثر می‌گذارد - نه فهرست‌شده زیرش. هر tspan دقیقاً مثل برچسب
    محور صفت ‎data-i‎ دارد، پس همان شنوندهٔ کلیک و همان ویرایشگر Float بدون هیچ
    ساز و کار دومی سرویسشان می‌دهد. عمداً کم‌صدا هستند: ریز و محو تا وقتی ماوس
    رویشان برود، چون این‌ها عددهایی‌اند که یک‌بار تنظیم می‌شوند و نمودار باید
    برای آن‌هایی که مدام می‌پاییدشان در یک نگاه خوانا بماند. */
 /* [EN] The timers and gains used to be drawn here, as <tspan> runs floating
    over the plane. They are gone from the picture and live in chips under it
    now (user order 2026-10-03: "write those times underneath so the charts do
    not get so crowded - do the same for the gains"). Nothing became
    read-only: the chips carry the same data-i and the same editor.
    [FA] زمان‌ها و گین‌ها قبلاً همین‌جا روی صفحه رسم می‌شدند. حالا از تصویر
    بیرون آمده‌اند و زیر نمودار به‌صورت تراشه نشسته‌اند (دستور کاربر: «اون
    زمان‌ها رو همون زیرش بنویس که انقدر شلوغ نشه نمودارها؛ گین هم همین کار رو
    براش بکن»). هیچ‌چیز فقط‌خواندنی نشد: تراشه‌ها همان ‎data-i‎ و همان ویرایشگر
    را دارند. */
 s+=put(PL,X0+6,'start','8.5')+put(LL,X1-4,'end','9');
 /* عددهای شارژ زنده - دقیقاً روی همهٔ خط‌ها و نوشته‌ها (آخرین لایهٔ رسم)،
    با جداکنندهٔ عمودی ساده تا در هم نروند و هالهٔ تیره برای خوانایی. */
 BT.sort((p,q2)=>p.y-q2.y);{let last=-99;BT.forEach(o=>{o.y=Math.min(H-28,Math.max(last+10,o.y));last=o.y;});}
 BT.forEach(o=>{s+=`<rect x="${o.x-72}" y="${o.y-8}" width="144" height="11" fill="rgba(8,11,18,.72)" rx="3"/>`+
  `<text x="${o.x}" y="${o.y}" text-anchor="middle" font-size="${F(8.5)}" font-weight="700" fill="${o.c}" paint-order="stroke" stroke="#080b12" stroke-width="2.5" stroke-linejoin="round">${o.t}</text>`;});
 s+=`<text x="${X0}" y="8" font-size="${F(8)}" fill="#96a1b8">ولتاژ باتری / Battery voltage (V)</text>`;
 s+=`<text x="${X1}" y="${H-2}" text-anchor="end" font-size="${F(8)}" fill="#96a1b8">جریان شارژ / Charge current (mA)</text></svg>`;
 MG.forEach(m=>{m.innerHTML=s;});
 document.querySelectorAll('.qglg').forEach(m=>{m.innerHTML='<span class="lb">راهنمای رنگ‌ها:</span>'+
  ZONES.map(z=>`<span class="qgit"><i style="background:${z[2]==='HATCH'?'repeating-linear-gradient(45deg,rgba(154,200,255,.22) 0 2px,rgba(154,200,255,.06) 2px 4px)':z[2]}"></i>${z[3]}</span>`).join('');});
 document.querySelectorAll('.qglm').forEach(m=>{m.innerHTML=lg;});

 /* [EN] Chips under the chart, grouped the way the user asked for them:
    the timers in one row, the gains and steps in the next. Each chip shows
    the English name the firmware uses, the live applied value, and carries
    the Persian name plus its full description as a hover title, so the page
    explains itself without the help panel having to be open.
    [FA] تراشه‌های زیر نمودار، گروه‌بندی‌شده همان‌طور که خواسته شد: زمان‌ها در یک
    ردیف، گین‌ها و گام‌ها در ردیف بعد. هر تراشه نام انگلیسیِ خود فرم‌ور، مقدار
    اعمال‌شدهٔ زنده، و نام و شرح فارسی را به‌صورت hover نشان می‌دهد. */
 const qq=t=>String(t==null?'':t).replace(/&/g,'&amp;').replace(/"/g,'&quot;').replace(/</g,'&lt;');
 const chip=id=>{const pr=pxt(id);return '<span class="evc" title="'+qq(pr?pr[0]+' — '+pr[1]:EVN[id])+'">'+
  '<span class="evcl">'+EVN[id]+'</span><span '+evat(id)+
  (evcl(id)?' style="color:#f7c13c"':'')+'>'+evtxt(id)+'</span></span>';};
 const grp=(t,ids)=>'<div class="evcg"><span class="evct">'+t+'</span>'+ids.map(chip).join('')+'</div>';
 document.querySelectorAll('.qgcm').forEach(m=>{
  /* [EN] v1.42 (user order): show the 13 chips in the THREE groups the
     user himself named - 1) reaching the target softly, 2) emergency
     brakes that stand ABOVE the PID on purpose, 3) the state machine's
     timing decisions. EVCT and EVCG stay two flat id lists because the
     consistency audit and the charger tester derive from exactly those
     two names; only the DISPLAY regroups them.
     [FA] v1.42 (دستور کاربر): نمایش تراشه‌ها در سه دستهٔ خودِ کاربر.
     دو فهرست ‎EVCT/EVCG‎ برای ممیزی دست‌نخورده می‌ماند؛ فقط نمایش گروهی شده. */
  m.innerHTML=grp('۱ · رسیدن نرم به هدف — با چه شتاب و سلایقی به هدف PID برسیم',EVCG.slice(0,3))+
   grp('۲ · ترمز اضطراری و حاشیهٔ امن — نگهبان‌هایی بالای PID، عمداً از او جدا',EVCG.slice(3))+
   grp('۳ · تصمیم‌های زمانی — چه زمانی چه اتفاقی بیفتد',EVCT);});

 /* [FA] v1.69 (دستور کاربر): بنر «ترکیب نامعتبر — برد این‌ها را گیره می‌زند»
    حذف شد. از v1.56 برد اصلاً ترکیب‌ها را بررسی نمی‌کند؛ خود پنل پیش از ارسال
    اعداد را جور می‌کند و هر تغییری را در خط وضعیت گزارش می‌دهد، پس آن بنر هم
    نادرست بود و هم فقط شلوغی. فقط حاشیهٔ قرمز فیلد مقصر می‌ماند. */
 const w=qchk();
 document.querySelectorAll('.qwm').forEach(we=>{we.innerHTML='';we.style.cssText='margin:2px 0 0';});
}
/* [‎EN] bind the profile inputs: on change, POST /s (fire-and-forget‎; the ack span next to the field shows the APPLIED value reported by the STM32). v1.14d: a typed value that breaks the profile rules asks for confirmation first, because the board will clamp it. / اتصال ورودی‌های profile: با تغییر، ‎POST /s‎؛ نشانگر کنار فیلد مقدار «اعمال‌شده» را از STM32 نشان می‌دهد. v1.14d: مقدار ناسازگار قبل از ارسال تأیید می‌خواهد چون برد گیره‌اش می‌زند. */
for(const id of [7,8]){const e=$('q'+id);if(!e)continue;e.onchange=()=>{const v=parseInt(e.value,10);if(isNaN(v))return;
 if(id>=20){const m=qchk().filter(x=>x.ids.includes(id));
  if(m.length&&!confirm('⚠ '+m.map(x=>x.msg).join('\n')+'\n\nبرد مقدار را گیره می‌زند تا مجموعه سازنده بماند. باز هم ارسال شود؟')){e.value='';qgraph();return;}}
 qput(id,v);};if(id>=20)e.oninput=qgraph;}
function cfill(){if(!D||!D.p)return;for(const id of [0,1,2,3,9,10]){const e=$('q'+id);if(!e)continue;if(document.activeElement!==e&&e.value==='')e.value=D.p[id]==null?'':D.p[id];}}
for(const id of [0,1,2,3,9,10]){const e=$('q'+id);if(!e)continue;e.onchange=()=>{const v=parseInt(e.value,10);if(isNaN(v))return;qput(id,v);};}
/* ===== v1.15: تب آلارم‌ها — آینهٔ قوانین Fault_ClampAlarms/Charger_ClampAlarms روی برد ===== */
/* v1.24: upper bound MUST track the top real id. It was left at 97 when the
   third PID row was deleted, so ap() built u93..u97 = undefined and the
   settings backup carried five phantom ids. Same class of bug as v1.22's
   AIDS/ADEF mismatch - a host test now pins it to the PDEF length. */
/* v1.28: ids 93..107 (charger limits + backstop gains) join the generic
   fill/validate/send machinery, so they need no bespoke handlers. */
/* v1.49: ۱۱۹/۱۲۰ (نردبان سمت شارژ) هم مثل بقیه خوانده و نوشته می‌شود */
const AIDS=[];for(let _i=27;_i<=131;_i++)AIDS.push(_i);
const LDEF=[3600000,100,600000,60000,8,700,32,14800,100,500,10,15000,3000,3000,500];
const IDEF=[300,500,600000,600000,30000,100,10,3600000,200,1,20];

/* [EN] v1.49: factory defaults of the charge-side percent map (ids 119/120),
   the same numbers as the discharge pair so the split changes nothing until
   somebody moves one side on purpose.
   [FA] پیش‌فرض کارخانهٔ نردبان سمت شارژ (۱۱۹/۱۲۰) - همان اعداد جفت دشارژ. */
/* ۱۱۹..۱۲۴: نگاشت درصد شارژ، شکل بوق باند ۲ و (v1.68) چشمک قرمز قفل عدم‌توازن */
/* v1.71: ۱۲۲ = فاصلهٔ تکرار باند ۲ (همان ۶۰۰۰۰ که قبلاً از باند ۱ قرض می‌گرفت) */
/* v1.72: ۱۲۵..۱۲۷ هم به همین آرایه اضافه شد (۲۴ ساعت، ۱۰ دقیقه، قطع خروجی خاموش) */
/* v1.80: ۱۲۸..۱۳۱ چراغ و بوقِ خودِ سناریو ۶ (پیش‌فرض = همان رفتار قبلی: بوق هر ۱۰ دقیقه، قرمز ثابت) */
const CDEF=[21000,29000,1000,60000,1000,50,86400000,600000,0,600000,120,0,50];
/* v1.71: شناسهٔ ۵۷ دیگر درصد نیست؛ مدت هر بوق بحرانی بر حسب ms است (۱۰۰۰۰ = همان صدای قبلی) */
const ADEF=[14800,150,6000,7000,1000,1000,21000,28000,950,14850,2000,1000,50,10000,1000,1,0,1000,50,3000,233,3,100,40,20,10,1,60000,20000,10000,10000,1,1000,2000,10000,1,2,3,100,1000,10,1000,150,28000,1000,21000,21200,21000,29000,0,100,95,5,2,2,3];
function av(id){const e=$('q'+id),d=D&&D.p&&D.p[id]!=null?D.p[id]:(id>=119?CDEF[id-119]:id>=108?IDEF[id-108]:id>=93?LDEF[id-93]:id>=83?PDEF[id-83]:ADEF[id-27]);
 if(e&&e.value!==''){const v=parseInt(e.value,10);if(!isNaN(v))return{v,d};}
 return{v:d,d};}
function ap(){
 const g=(id,fb)=>D&&D.p&&D.p[id]!=null?D.p[id]:fb;
 /* [EN] profile refs come from the APPLIED board values (QDEF fallback); alarm refs from typed-or-applied (av). */
 const o={over:g(22,14600),imax:g(25,650),d:av(27).v,dd:av(28).v,ab:av(29).v,bk:av(30).v,ad:av(31).v,rc:av(32).v,mn:av(33).v,mx:av(34).v,hd:av(35).v,ov:av(36).v,fl:av(37).v};
 AIDS.forEach(id=>{o['u'+id]=av(id).v;});
 return o;}
function achk(){const a=ap(),w=[],bad=(v,lo,hi)=>!(v>=lo&&v<=hi);
 const dlo=Math.max(14000,a.over+50),dhi=Math.min(15000,a.ov-100);
 if(dlo>dhi)w.push({ids:[27,36],msg:'بازهٔ قطع خالی است — قطع OV را بالا ببرید یا سقف تجاوز را پایین بیاورید'});
 else if(bad(a.d,dlo,dhi))w.push({ids:[27],msg:'قطع باتری باید '+dlo+'..'+dhi+' باشد (بالای تجاوز+۵۰، زیر OV−۱۰۰)'});
 if(bad(a.dd,50,1000))w.push({ids:[28],msg:'دبانس قطع باید ۵۰..۱۰۰۰ باشد'});
 if(bad(a.ab,3000,8000))w.push({ids:[29],msg:'غیبت باید ۳۰۰۰..۸۰۰۰ باشد'});
 else if(!(a.ab<=a.bk-500))w.push({ids:[29,30],msg:'غیبت باید زیر بازگشت−۵۰۰ باشد (≤ '+(a.bk-500)+')'});
 if(bad(a.bk,4000,9000))w.push({ids:[30],msg:'برگشت باید ۴۰۰۰..۹۰۰۰ باشد'});
 else if(!(a.bk>=a.ab+500))w.push({ids:[30,29],msg:'برگشت باید بالای غیبت+۵۰۰ باشد (≥ '+(a.ab+500)+')'});
 if(bad(a.ad,100,5000))w.push({ids:[31],msg:'دبانس غیبت باید ۱۰۰..۵۰۰۰ باشد'});
 if(bad(a.rc,100,5000))w.push({ids:[32],msg:'دبانس بازیابی باید ۱۰۰..۵۰۰۰ باشد'});
 if(bad(a.mn,18000,24000))w.push({ids:[33],msg:'کف ورودی باید ۱۸۰۰۰..۲۴۰۰۰ باشد'});
 else if(!(a.mn<=a.mx-1000))w.push({ids:[33,34],msg:'کف ورودی باید زیر سقف−۱۰۰۰ باشد (≤ '+(a.mx-1000)+')'});
 if(bad(a.mx,24000,30000))w.push({ids:[34],msg:'سقف ورودی باید ۲۴۰۰۰..۳۰۰۰۰ باشد'});
 else if(!(a.mx>=a.mn+1000))w.push({ids:[34,33],msg:'سقف ورودی باید بالای کف+۱۰۰۰ باشد (≥ '+(a.mn+1000)+')'});
 if(bad(a.hd,a.imax+50,950))w.push({ids:[35,25],msg:'Hard fault باید '+(a.imax+50)+'..۹۵۰ باشد (بالای Bulk+۵۰، هرگز بالای ۹۵۰)'});
 const olo=Math.max(14000,a.over+150);
 if(bad(a.ov,olo,15000))w.push({ids:[36,22],msg:'قطع OV باید '+olo+'..۱۵۰۰۰ باشد (بالای تجاوز+۱۵۰، هرگز بالای ۱۵۰۰۰)'});
 if(bad(a.fl,0,8000))w.push({ids:[37],msg:'کف اعتبار باید ۰..۸۰۰۰ باشد'});
 /* v1.16: نگهبان اعداد UI (آینهٔ Ui_ClampAlarms) */
 const perOk=v=>v===0||(v>=1000&&v<=600000);
 const fit=(dur,per,cnt,gap)=>per===0||dur*cnt+(cnt>1?gap*(cnt-1):0)<=per;
 const perW=(id,v,nm)=>{if(!(v===0||(v>=1000&&v<=600000)))w.push({ids:[id],msg:nm+' باید صفر (خاموش) یا ۱۰۰۰..۶۰۰۰۰۰ باشد'});};
 [[38,'دوره چشمک قرمز OV',100,10000],[39,'duty قرمز OV',0,100],[42,'تعداد بوق OV',0,10],[43,'گپ بوق OV',0,5000],
  [44,'دوره چشمک قرمز قطع باتری',100,10000],[45,'duty قرمز قطع باتری',0,100],[48,'تعداد بوق قطع باتری',0,10],[49,'گپ بوق قطع باتری',0,5000]].forEach(x=>{if(bad(a['u'+x[0]],x[2],x[3]))w.push({ids:[x[0]],msg:x[1]+' باید '+x[2]+'..'+x[3]+' باشد'});});
 perW(40,a.u40,'دوره بوق OV');perW(46,a.u46,'دوره بوق قطع باتری');
 if(bad(a.u41,0,600000))w.push({ids:[41],msg:'مدت هر بوق OV باید ۰..۶۰۰۰۰۰ باشد'});
 else if(!fit(a.u41,a.u40,a.u42,a.u43))w.push({ids:[41,40,42,43],msg:'بوق OV در دوره جا نمی‌شود (مدت×تعداد+گپ‌ها ≤ دوره) — برد بی‌صدا می‌ماند'});
 if(a.u42>1&&a.u40!==0&&a.u43<100)w.push({ids:[43],msg:'گپ بوق OV با چند بوق باید دست‌کم ۱۰۰ باشد'});
 if(bad(a.u47,0,600000))w.push({ids:[47],msg:'مدت هر بوق قطع باتری باید ۰..۶۰۰۰۰۰ باشد'});
 else if(!fit(a.u47,a.u46,a.u48,a.u49))w.push({ids:[47,46,48,49],msg:'بوق قطع باتری در دوره جا نمی‌شود — برد بی‌صدا می‌ماند'});
 if(a.u48>1&&a.u46!==0&&a.u49<100)w.push({ids:[49],msg:'گپ بوق قطع باتری با چند بوق باید دست‌کم ۱۰۰ باشد'});
 [[50,'شروع بوق'],[51,'باند دو-بوق'],[52,'باند سه-بوق'],[53,'باند بحرانی']].forEach(x=>{if(bad(a['u'+x[0]],0,100))w.push({ids:[x[0]],msg:x[1]+' باید ۰..۱۰۰ باشد'});});
 if(!(a.u51<=a.u50))w.push({ids:[51,50],msg:'باند دو-بوق باید زیر شروع بوق باشد (≤ '+a.u50+')'});
 if(!(a.u52<=a.u51))w.push({ids:[52,51],msg:'باند سه-بوق باید زیر باند دو-بوق باشد (≤ '+a.u51+')'});
 if(!(a.u53<=a.u52))w.push({ids:[53,52],msg:'باند بحرانی باید زیر باند سه-بوق باشد (≤ '+a.u52+')'});
 perW(54,a.u54,'فاصلهٔ تکرار باند ۱');perW(122,a.u122,'فاصلهٔ تکرار باند ۲');perW(55,a.u55,'فاصلهٔ تکرار باند ۳');perW(56,a.u56,'فاصلهٔ تکرار باند بحرانی');
 if(bad(a.u57,0,600000))w.push({ids:[57],msg:'مدت هر بوق بحرانی باید ۰..۶۰۰۰۰۰ باشد'});
 [[58,'تعداد بوق بحرانی'],[62,'تعداد بوق باند ۱'],[63,'تعداد بوق باند ۲'],[64,'تعداد بوق باند ۳']].forEach(x=>{if(bad(a['u'+x[0]],0,10))w.push({ids:[x[0]],msg:x[1]+' باید ۰..۱۰ باشد'});});
 if(bad(a.u65,0,5000))w.push({ids:[65],msg:'گپ بوق دشارژ باید ۰..۵۰۰۰ باشد'});
 else if((a.u58>1||a.u62>1||a.u63>1||a.u64>1)&&a.u65<100)w.push({ids:[65],msg:'گپ بوق دشارژ با چند بوق باید دست‌کم ۱۰۰ باشد'});
 if(bad(a.u59,0,600000))w.push({ids:[59],msg:'مدت هر بوق باند ۱ باید ۰..۶۰۰۰۰۰ باشد'});
 else if(!fit(a.u59,a.u54,a.u62,a.u65))w.push({ids:[59,54,62,65],msg:'بوق باند ۱ در فاصلهٔ خودش جا نمی‌شود — برد بی‌صدا می‌ماند'});
 if(bad(a.u121,0,600000))w.push({ids:[121],msg:'مدت هر بوق باند ۲ باید ۰..۶۰۰۰۰۰ باشد'});
 else if(!fit(a.u121,a.u122,a.u63,a.u65))w.push({ids:[121,122,63,65],msg:'بوق باند ۲ در فاصلهٔ خودش جا نمی‌شود — برد بی‌صدا می‌ماند'});
 if(bad(a.u60,0,600000))w.push({ids:[60],msg:'مدت هر بوق باند ۳ باید ۰..۶۰۰۰۰۰ باشد'});
 else if(!fit(a.u60,a.u55,a.u64,a.u65))w.push({ids:[60,55,64,65],msg:'بوق باند ۳ در فاصلهٔ خودش جا نمی‌شود — برد بی‌صدا می‌ماند'});
 if(bad(a.u61,0,120000))w.push({ids:[61],msg:'طول بوق بحرانی یک‌باره باید ۰..۱۲۰۰۰۰ باشد'});
 if(!fit(a.u57,a.u56,a.u58,a.u65))w.push({ids:[57,56,58,65],msg:'بوق باند بحرانی در فاصلهٔ خودش جا نمی‌شود — برد بی‌صدا می‌ماند'});
 if(bad(a.u66,100,10000))w.push({ids:[66],msg:'دوره چشمک سبز باید ۱۰۰..۱۰۰۰۰ باشد'});
 if(bad(a.u67,0,10000))w.push({ids:[67],msg:'حداقل خاموشی سبز باید ۰..۱۰۰۰۰ باشد'});
 else if(!(a.u67<=a.u66))w.push({ids:[67,66],msg:'حداقل خاموشی سبز باید زیر دوره باشد (≤ '+a.u66+')'});
 if(bad(a.u68,100,10000))w.push({ids:[68],msg:'دوره چشمک زرد باید ۱۰۰..۱۰۰۰۰ باشد'});
 if(bad(a.u69,0,10000))w.push({ids:[69],msg:'حداقل روشنی زرد باید ۰..۱۰۰۰۰ باشد'});
 else if(!(a.u69<=a.u68))w.push({ids:[69,68],msg:'حداقل روشنی زرد باید زیر دوره باشد (≤ '+a.u68+')'});
 if(bad(a.u70,24000,32000))w.push({ids:[70],msg:'آستانه اضافه‌ولتاژ باید ۲۴۰۰۰..۳۲۰۰۰ باشد'});
 if(bad(a.u71,0,2000))w.push({ids:[71],msg:'hysteresis اضافه‌ولتاژ باید ۰..۲۰۰۰ باشد'});
 /* v1.74: شناسه‌های ۷۲/۷۳ بازنشسته شدند (پنجرهٔ باتری کم داخل چنج‌اور ثابت شد) و قانونی ندارند */
 if(bad(a.u74,15000,25000))w.push({ids:[74],msg:'کف نگاشت درصد باید ۱۵۰۰۰..۲۵۰۰۰ باشد'});
 else if(!(a.u74<=a.u75-100))w.push({ids:[74,75],msg:'کف نگاشت باید دست‌کم ۱۰۰ زیر سقف باشد (≤ '+(a.u75-100)+')'});
 if(bad(a.u75,25000,32000))w.push({ids:[75],msg:'سقف نگاشت درصد باید ۲۵۰۰۰..۳۲۰۰۰ باشد'});
 else if(!(a.u75>=a.u74+100))w.push({ids:[75,74],msg:'سقف نگاشت باید دست‌کم ۱۰۰ بالای کف باشد (≥ '+(a.u74+100)+')'});
 if(bad(a.u76,0,1))w.push({ids:[76],msg:'میوت باید ۰ یا ۱ باشد'});
 if(bad(a.u77,1,100))w.push({ids:[77],msg:'ورود فول باید ۱..۱۰۰ باشد'});
 if(bad(a.u78,0,100))w.push({ids:[78],msg:'خروج فول باید ۰..۱۰۰ باشد'});
 else if(!(a.u78<=a.u77-1))w.push({ids:[78,77],msg:'خروج فول باید زیر ورود باشد (≤ '+(a.u77-1)+')'});
 if(bad(a.u79,0,50))w.push({ids:[79],msg:'hysteresis شارژ باید ۰..۵۰ باشد'});
 if(bad(a.u80,0,50))w.push({ids:[80],msg:'hysteresis دشارژ باید ۰..۵۰ باشد'});
 if(bad(a.u81,0,100))w.push({ids:[81],msg:'خروج از ۰٪ باید ۰..۱۰۰ باشد'});
 if(bad(a.u82,0,100))w.push({ids:[82],msg:'خروج از ۱٪ باید ۰..۱۰۰ باشد'});
 return w;}
function afresh(){const w=achk();
 /* v1.77: گپ‌ها بلافاصله پس از هر تغییر/پرشدن مقدار، نه فقط در فریم شبیه‌ساز */
 if(typeof gapen==='function')gapen();
 /* [FA] v1.69 (دستور کاربر): همان بنر در تب تنظیمات هم حذف شد؛ دلیلش بالاتر
    در qchk آمده. فقط فیلد مقصر قرمز می‌شود. */
 [$('aw'),$('aw2')].forEach(el=>{if(el){el.innerHTML='';el.style.cssText='margin:2px 0 0';}});
 for(const id of AIDS){const ne=$('q'+id);if(ne)ne.style.borderColor=w.some(x=>x.ids.includes(id))?'#e5484d':'';}
 try{c4();sall();}catch(e){}
 const ms=$('xmuteS');if(ms)ms.textContent=(D&&D.p&&D.p[76]===1)?'🔇 میوت روشن — موقتی، با ریست برد پاک می‌شود؛ LEDها همچنان چشمک می‌زنند':'🔊 بوق روشن';}
function apend(id){if(!D)return 0;return id<32?(D.q&(1<<id)):id<64?(D.q2&(1<<(id-32))):id<96?((D.q3||0)&(1<<(id-64))):id<128?((D.q4||0)&(1<<(id-96))):((D.q5||0)&(1<<(id-128)));}
/* v1.70 (دستور کاربر): چیپ «روی برد: …» حذف شد — از فریم تله‌متری پر می‌شد و
   بعد از تغییر هنوز عدد قدیمی را نشان می‌داد. خود کادر از برد پر می‌شود،
   پیش‌فرض کارخانه کنار فیلد هست و کارت نتیجه می‌گوید برد چه را پذیرفت.
   span های ‎a<id>‎ می‌مانند («در صف»/«…»/«خطا» در آن‌ها نوشته می‌شود). */
function afill(){if(!D||!D.p)return;
 for(const id of AIDS){const e=$('q'+id),a=$('a'+id);if(!e)continue;
  if(document.activeElement!==e&&e.value==='')e.value=D.p[id]==null?'':D.p[id];
  if(a&&!apend(id)&&!(id in PEND))a.textContent='';}}
function adef(){ADEF.slice(6,11).forEach((v,k)=>{const id=33+k;$('q'+id).value=v;qput(id,v);});afresh();}
/* v1.22: پیش‌فرض کارخانهٔ PID سه‌مرحله‌ای — همان اعداد CHG_PID_* در charger.h */
const PDEF=[12,1600,0,1000,1000,50,18000,0,10,1000];
/* v1.22: نگهبان ترکیب PID — آینهٔ func__Charger_ClampPid روی برد به اضافهٔ دو
   هشدار تیون که شبیه‌سازی نشان داد. خروجی خالی = ترکیب سالم. */
function pv(id){const e=$('q'+id),d=D&&D.p&&D.p[id]!=null?D.p[id]:PDEF[id-83];
 if(e&&e.value!==''){const v=parseInt(e.value,10);if(!isNaN(v))return v;}
 return d;}
function pchk(){const w=[];
 for(let id=83;id<=92;id++){const v=pv(id),rate=((id-83)%5)>=3;
  if(rate&&(v<10||v>20000))w.push({ids:[id],msg:'نرخ شیب باید بین ۱۰ تا ۲۰۰۰۰ <span class=\"n\">m‰/s</span> باشد'});
  if(!rate&&(v<0||v>20000))w.push({ids:[id],msg:'ضریب باید ۰ تا ۲۰۰۰۰ باشد'});}
 if(pv(91)>=pv(86))w.push({ids:[91,86],msg:'نرخ صعود حلقهٔ ولتاژ باید کمتر از حلقهٔ جریان بماند؛ وگرنه <span class=\"n\">duty</span> در <span class=\"n\">Absorb</span> تندتر از <span class=\"n\">Bulk</span> بالا می‌رود'});
 if(pv(88)>300)w.push({ids:[88],msg:'<span class=\"n\">Kp</span> حلقهٔ ولتاژ بالای ۳۰۰ ناپایدار می‌شود'});
 else if(pv(88)>100)w.push({ids:[88],msg:'<span class=\"n\">Kp</span> حلقهٔ ولتاژ بالای ۱۰۰ نویز را بزرگ می‌کند، چون ولتاژ پک فیلتر ندارد. در آزمایش با نویز <span class=\"n\">±۷mV</span>: <span class=\"n\">Kp=۱۵۰</span> حدود ۷۴۴ بار تغییر جهت <span class=\"n\">duty</span> در ۱۰ ساعت، ولی <span class=\"n\">Kp=۵۰</span> فقط ۶۲ بار — با همان دقت و همان زمان شارژ. یعنی سایش بیشتر، بدون سود'});
 if(pv(83)>150)w.push({ids:[83],msg:'<span class=\"n\">Kp</span> حلقهٔ جریان بالای ۱۵۰ لرزش می‌آورد: هر یک پرمیل <span class=\"n\">duty</span> حدود <span class=\"n\">۷mA</span> جریان را جابه‌جا می‌کند، پس حلقه بین دو پرمیل مجاور بالا-پایین می‌پرد'});
 const box=$('pw');if(box)box.innerHTML=!w.length?'<span class="lb">✅ ترکیب PID سالم است · پشتیبان‌های ۶۵۰ میلی‌آمپر و ۱۴٫۸ ولت همیشه فعال‌اند</span>':w.map(x=>'<div class="wn">⚠ '+x.msg+'</div>').join('');
 return w;}
function pdef(){PDEF.forEach((v,k)=>{const id=83+k,e=$('q'+id);if(e)e.value=v;qput(id,v);});pchk();}
/* [EN] The limits have no q-inputs any more - they are edited in place in
   the operating table - so this just writes and lets the table re-render
   from the applied values. Clearing EVWANT stops stale clamp warnings.
   [FA] حدها دیگر ورودی q ندارند و درجا در جدول عملکرد ویرایش می‌شوند، پس
   اینجا فقط می‌نویسد و جدول از مقادیر اعمال‌شده بازرسم می‌شود. پاک‌کردن
   EVWANT جلوی هشدار گیرهٔ کهنه را می‌گیرد. */
function ldef(){LDEF.forEach((v,k)=>{const id=93+k;delete EVWANT[id];qput(id,v);});afresh();}
/* [EN] v1.76 (user order: "give every section its own factory key and drop
   the all-in-one one"): ONE helper, six id lists. Each key restores only the
   ids of its own scenario, reading the very same tables pdflt() prints under
   the fields - one source, so a key can never disagree with the number shown
   beneath the box. The old sdef() that reset 38..82 in one go is gone.
   [FA] از v1.76 هر سناریو کلید کارخانهٔ خودش را دارد و فقط شناسه‌های همان
   سناریو را برمی‌گرداند؛ کلیدِ همه‌باهم حذف شد. همه از همان جدولی می‌خوانند
   که زیر هر کادر چاپ می‌شود. */
function odef(ids){ids.forEach(id=>{const v=pdflt(id);if(v==null)return;
 const e=$('q'+id);if(e)e.value=v;qput(id,v);});afresh();}
const UDEF={
 1:[38,39,40,41,42,43,70,71],
 2:[27,28,29,30,31,32,44,45,46,47,48,49],
 3:[50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,74,75,80,81,82,121,122],
 4:[68,69,77,78,79],
 5:[108,109,110,111,112,113,114,115,116,117,118,123,124],
 6:[125,126,127,128,129,130,131]};
function ovdef(){odef(UDEF[1]);}
function bdef(){odef(UDEF[2]);}
function dsdef(){odef(UDEF[3]);}
function chdef(){odef(UDEF[4]);}
function ibdef(){odef(UDEF[5]);}
/* [EN] v1.72 scenario 6 factory defaults (ids 125..127 live in CDEF).
   [FA] پیش‌فرض کارخانهٔ سناریو ۶. */
function dbdef(){odef(UDEF[6]);}
/* v1.15b: کارت وضعیت گروه‌بندی‌شده — اسکلت یک‌بار ساخته می‌شود و هر poll فقط متن/رنگ به‌روز می‌شود (بدون پر/خالی شدن و چشمک) */
const FEXP=[
 ['خطای ADC','نمونه‌برداری ADC نامعتبر است و اندازه‌گیری‌ها قابل‌اعتماد نیست؛ برد محافظه‌کار می‌شود. سیم‌کشی آنالوگ و تغذیه را بررسی کنید.'],
 ['اضافه‌جریان کانال ۱','جریان کانال ۱ از حد گذشت و کانال متوقف شد؛ باتری/بار کانال ۱ را بررسی و برد را ریست کنید.'],
 ['اضافه‌جریان کانال ۲','جریان کانال ۲ از حد گذشت و کانال متوقف شد؛ باتری/بار کانال ۲ را بررسی و برد را ریست کنید.'],
 ['باتری ضعیف','ولتاژ باتری خیلی پایین است؛ باتری را بررسی/شارژ کنید.'],
 ['خطای جیتر کانال ۱','ناپایداری داخلی نمونه‌برداری کانال ۱؛ اگر ماندگار شد برد را ریست کنید.'],
 ['خطای جیتر کانال ۲','ناپایداری داخلی نمونه‌برداری کانال ۲؛ اگر ماندگار شد برد را ریست کنید.'],
 ['قطع باتری','سیم باتری قطع است یا باتری نیست: یا ولتاژ حین پمپ بالای آستانهٔ قطع (۲۷) رفته یا باتری زیر آستانهٔ غیبت (۲۹) با ورودی سالم دیده شده. سیم‌کشی باتری را بررسی کنید؛ با بازگشت هر دو نیمه بالای آستانهٔ برگشت (۳۰) و پایداری (۳۲)، لچ خودکار پاک می‌شود.']];
/* [EN] v1.28/v1.29 (user order): the operating table lives on the chargers
   page, and every number in it IS its own input - click the value, type,
   Enter. There is no separate box of fields any more: a table that shows a
   number next to a form that sets the same number is two places to be wrong,
   and this code base has lost days to exactly that shape of bug.
   Every cell is read from the board's APPLIED parameters. Writing today's
   numbers into the markup is how the help text ended up claiming the
   backstops were not adjustable long after they were.
   [FA] جدول عملکرد روی صفحهٔ شارژرهاست و هر عددش خودش ورودی خودش است - روی
   مقدار کلیک کنید، تایپ کنید، Enter. دیگر کادر جدای فیلدها وجود ندارد: جدولی
   که عددی را نشان دهد کنار فرمی که همان عدد را تنظیم کند، یعنی دو جا برای
   غلط‌بودن، و همین شکلِ باگ قبلاً روزها از این مخزن گرفته است.
   هر خانه از پارامترهای «اعمال‌شدهٔ» برد خوانده می‌شود. */

/* [EN] id -> [min, max, step, unit]. Hand-written here because the browser
   cannot see the firmware's tables - so audit_consistency.py pins every row
   of this against ParamMin/ParamMax and fails the build-check on drift. A
   hand-copied table is allowed in this repo only when something mechanical
   is watching it.
   [FA] شناسه -> [کمینه، بیشینه، گام، واحد]. دستی است چون مرورگر جدول‌های
   فرم‌ور را نمی‌بیند - پس audit_consistency.py تک‌تک ردیف‌هایش را به
   ‎ParamMin/ParamMax‎ میخ می‌کند و سر اختلاف، بررسی را رد می‌کند. جدول دستی در
   این مخزن فقط وقتی مجاز است که چیزی ماشینی مراقبش باشد. */
const EVB={
 20:[11000,14600,50,'mv'], 21:[10500,14550,50,'mv'], 22:[11100,14750,50,'mv'],
 23:[9000,14300,50,'mv'],  24:[8000,14000,50,'mv'],  25:[100,2950,10,'ma'],
 26:[10,1475,5,'ma'],      35:[150,3000,10,'ma'],    36:[14000,15000,50,'mv'],
 93:[0,21600000,60000,'ms'], 94:[10,1500,10,'ma'],   95:[0,7200000,60000,'ms'],
 96:[1000,600000,5000,'ms'], 97:[1,100,1,'pm'],      98:[0,999,50,'mpm'],
 99:[1,64,1,'n'],            100:[13000,14800,50,'mv'], 101:[0,2000,10,'n'],
 102:[0,2000,10,'n'],        103:[0,100,5,'ma'],     104:[0,120000,1000,'ms'],
 105:[0,60000,500,'ms'],     106:[500,60000,500,'ms'], 107:[50,5000,50,'ms']};

const EVU={mv:'mV',ma:'mA',ms:'ms',pm:'‰',n:''};

/* [EN] Where the bench LUTs stop being fitted data and start extending their
   last slope (calibration.h: chain 640mA on ch1 = 631mA of battery current
   at 14.4V). The current ceilings were opened on 2026-10-03 so paralleled
   packs can be charged, and above this line the current reading is
   extrapolated rather than measured. Drawing it is the honest way to open a
   range: the user's hand is free AND the user can see where the evidence
   ends. Not settable - it is a property of the fit, not a preference.
   [FA] جایی که جدول‌های بنچ از «دادهٔ برازش‌شده» به «ادامهٔ شیب آخر» تبدیل
   می‌شوند (۶۳۱ میلی‌آمپر جریان باتری در ۱۴٫۴ ولت). سقف جریان‌ها در
   ۲۰۲۶-۱۰-۰۳ باز شد تا پک موازی هم شارژ شود، و بالای این خط عدد جریان
   برون‌یابی است نه اندازه‌گیری. رسم‌کردنش راه صادقانهٔ بازکردن یک بازه است:
   هم دست کاربر باز است هم می‌بیند شواهد کجا تمام می‌شود. تنظیم‌شدنی نیست -
   مشخصهٔ برازش است نه سلیقه. */
const EVCAL=631;

/* [EN] Which ids are drawn WHERE. These two lists drive the rendering, so
   they cannot drift from what is actually on screen, and the audit reads
   them to prove no ordered parameter lost its editor.
   EVV = horizontal voltage lines, EVI = vertical current lines (the chart's
   two axes). Everything else settable is annotated inline next to the line
   it acts on; the audit derives that set as EVB minus these two rather than
   keeping a third list that could fall out of step.
   [FA] اینکه هر شناسه کجا رسم می‌شود. همین دو فهرست رندر را می‌رانند، پس
   نمی‌توانند از آنچه واقعاً روی صفحه است جدا بیفتند، و ممیزی از رویشان ثابت
   می‌کند هیچ پارامتر دستور داده‌شده‌ای ویرایشگرش را از دست نداده.
   EVV = خطوط افقی ولتاژ، EVI = خطوط عمودی جریان (دو محور نمودار). هر چیز
   تنظیم‌شدنی دیگری به‌صورت یادداشت، کنار همان خطی که رویش اثر می‌گذارد، روی خود
   نمودار می‌آید؛ ممیزی آن مجموعه را «EVB منهای این دو» مشتق می‌کند تا فهرست
   سومی که بتواند از قافله عقب بماند وجود نداشته باشد. */
/* [EN] ONE table of the English names for the numbers that sit under the
   chart. The chips, the chart and the help text all read it, so a rename
   cannot leave two different words for one setting on the same page.
   [FA] یک جدول واحد برای نام انگلیسی عددهایی که زیر نمودار می‌نشینند. تراشه‌ها،
   نمودار و متن راهنما همگی از همین می‌خوانند، پس تغییر نام نمی‌تواند دو واژهٔ
   متفاوت برای یک تنظیم روی یک صفحه باقی بگذارد. */
const EVN={93:'Absorb max',95:'Absorb hold',96:'Taper sustain',97:'duty step',
 98:'hyst',99:'V filter',101:'Backstop gain I',102:'Backstop gain V',
 103:'margin',104:'settle',105:'JIT lockout',106:'manual deadman',
 107:'ramp down'};
const EVCT=[93,95,96,104,105,106,107];      /* زمان‌ها / timers */
const EVCG=[97,98,99,101,102,103];          /* گین‌ها و گام‌ها / gains & steps */
const EVV={22:['#fb923c','Over'],20:['#f7c13c','Absorb'],
           21:['#e8a33d','Absorb enter'],23:['#35d6a0','Float'],
           24:['#63a2ff','Reentry'],36:['#ff6873','OV cutoff'],
           100:['#fc8086','Backstop']};
const EVI={25:['#63a2ff','Bulk max',-1],26:['#35d6a0','Taper',1],
           94:['#f7c13c','Count start',1],35:['#ff6873','Hard fault',-1]};

let EVOPEN=null;          /* id being edited, or null */
const EVWANT={};          /* what the user asked for, to spot a clamp */

function ms2(ms){if(ms==null)return '';if(ms===0)return 'بدون سقف';
 if(ms>=3600000)return (ms/3600000).toFixed(ms%3600000?1:0)+' ساعت';
 if(ms>=60000)return (ms/60000).toFixed(ms%60000?1:0)+' دقیقه';
 return (ms/1000).toFixed(ms%1000?1:0)+' ثانیه';}

function evfmt(v,u){if(v==null)return '';
 if(u==='mv')return (v/1000).toFixed(2)+'V';
 if(u==='ma')return v+'mA';
 if(u==='ms')return ms2(v);
 if(u==='pm')return v+' ‰';
 if(u==='mpm')return v+' m‰'; /* [FA] میلی‌پرمیل - واحد hysteresis خروجی (id 98) */
 return ''+v;}

function evval(id){return (D&&D.p&&D.p[id]!=null)?D.p[id]:null;}
function evtxt(id){const b=EVB[id];return b?evfmt(evval(id),b[3]):'';}
function evcl(id){const v=evval(id);return (EVWANT[id]!=null&&v!=null&&EVWANT[id]!==v);}

/* [EN] Read-only rendering. The operating table uses ONLY this now (user
   order): it reports, it does not edit.
   [FA] رندر فقط‌خواندنی. جدول عملکرد حالا فقط از همین استفاده می‌کند. */
function evr(id){return '<b class="evv">'+evtxt(id)+'</b>';}
/* [EN] A derived number with no parameter behind it (the duty ceiling is
   max(id 13, id 14)); it is reported, never offered as a control.
   [FA] عددی مشتق که پارامتری پشتش نیست؛ گزارش می‌شود، هرگز به‌عنوان کنترل
   پیشنهاد نمی‌شود. */
function evr_plain(txt){return '<b class="evv">'+txt+'</b>';}

/* [EN] An editable chip (HTML) and the attributes that make an SVG <text>
   editable. Same data-i contract, so one click handler serves both.
   [FA] تراشهٔ ویرایش‌پذیر (HTML) و صفت‌هایی که متن SVG را ویرایش‌پذیر می‌کنند. */
function evat(id){return 'class="evs" data-i="'+id+'"';}

function evclose(){const e=$('evpop');if(e)e.remove();EVOPEN=null;}

/* [EN] Floating editor anchored to the clicked label. It lives on <body>, not
   inside the SVG, so a chart redraw cannot delete the field mid-typing - but
   the redraw is suppressed anyway while it is open, because watching the
   numbers move under an open editor is its own kind of wrong.
   [FA] ویرایشگر Float که به برچسب کلیک‌شده لنگر می‌اندازد. روی body است نه
   داخل SVG، پس بازرسم نمودار نمی‌تواند وسط تایپ فیلد را پاک کند - با این حال
   تا باز است بازرسم متوقف می‌شود، چون تکان‌خوردن اعداد زیر ویرایشگرِ باز خودش
   یک جور غلط است. */
function evedit(el){
 const id=+el.dataset.i,b=EVB[id];if(!b)return;
 evclose();
 const v=evval(id);
 EVOPEN=id;
 const pop=document.createElement('div');pop.id='evpop';pop.className='evpop';
 pop.innerHTML='<div class="evpt">'+((typeof PX!=='undefined'&&PX[id])?PX[id][0]:('شناسهٔ '+id))+'</div>'+
  '<div class="evpr"><input type="number" class="evi" min="'+b[0]+'" max="'+b[1]+'" step="'+b[2]+'">'+
  '<span class="evu">'+(EVU[b[3]]||'')+'</span></div>'+
  '<div class="evph">مجاز: '+b[0]+' تا '+b[1]+' · Enter ثبت · Esc لغو</div>';
 document.body.appendChild(pop);
 const r=el.getBoundingClientRect(),pw=pop.offsetWidth||200;
 const sx=window.pageXOffset||0,sy=window.pageYOffset||0;
 pop.style.left=Math.max(8,Math.min((window.innerWidth||900)-pw-8,r.left+sx))+'px';
 pop.style.top=(r.bottom+sy+6)+'px';
 const inp=pop.querySelector('input');inp.value=(v==null?'':v);inp.focus();inp.select();
 let done=false;
 const close=save=>{if(done)return;done=true;
  if(save){const n=parseInt(inp.value,10);
   if(!isNaN(n)&&n!==v){EVWANT[id]=n;
    /* [EN] Some of these ids also have a classic input in the settings tab.
       Leaving it stale makes qv() see "typed value != applied value" and the
       chart then labels the line "preview", dashed - announcing a pending
       change that does not exist. Two representations of one number, so both
       move together or neither is trustworthy.
       [FA] بعضی از این شناسه‌ها در تب تنظیمات ورودی کلاسیک هم دارند. کهنه
       گذاشتنش باعث می‌شود qv() ببیند «مقدار تایپ‌شده با اعمال‌شده فرق دارد» و
       نمودار آن خط را خط‌چین و «پیش‌نمایش» برچسب بزند - اعلام تغییری معلق که
       اصلاً وجود ندارد. دو نمایش از یک عدد: یا با هم حرکت می‌کنند یا هیچ‌کدام
       قابل اعتماد نیست. */
    const qe=$('q'+id);if(qe)qe.value=n;
    qput(id,n);}}
  evclose();qgraph();};
 inp.onkeydown=e=>{if(e.key==='Enter'){e.preventDefault();close(true);}
                   else if(e.key==='Escape'){e.preventDefault();close(false);}};
 inp.onblur=()=>close(true);}

/* [EN] One delegated listener for every editable value, wherever it is drawn.
   [FA] یک شنوندهٔ واگذارشده برای هر مقدار ویرایش‌پذیر، هر جا که رسم شده باشد. */
document.addEventListener('click',e=>{
 const t=e.target&&e.target.closest?e.target.closest('[data-i]'):null;
 if(!t||!t.dataset||t.dataset.i==null)return;
 if(t.classList&&t.classList.contains('ro'))return;
 if(!EVB[+t.dataset.i])return;
 evedit(t);});

/* [EN] No EVOPEN guard here, deliberately. The chart freezes while you type
   because you are aiming at a label on it; the table is a read-only report
   and a report that stops reporting is just stale. Mutation testing is what
   settled this: removing the guard broke nothing, which meant the guard was
   defending nothing - so it went, rather than gaining a test to justify it.
   [FA] اینجا عمداً گاردِ EVOPEN نیست. نمودار موقع تایپ فریز می‌شود چون شما
   به برچسبی روی آن نشانه رفته‌اید؛ ولی جدول یک گزارش فقط‌خواندنی است و
   گزارشی که گزارش ندهد فقط کهنه است. موتیشن‌تست این را حل کرد: برداشتن گارد
   هیچ‌چیزی را نشکست، یعنی گارد از هیچ‌چیز دفاع نمی‌کرد - پس خودش رفت، نه
   اینکه تستی برای توجیهش اضافه شود. */



/* [EN] One delegated listener: the chart is re-rendered on every data
   refresh, so per-element handlers would leak and die with each redraw.
   [FA] یک شنوندهٔ واگذارشده: نمودار با هر رفرش داده بازرسم می‌شود و
   شنوندهٔ تک‌عنصری با هر بازرسم می‌مرد. */


function astat(){const s=$('ast');if(!s||!ASB||!D||!D.t||!D.p)return;
 const t=D.t,p=D.p;
 const g=(id,fb)=>p[id]!=null?p[id]:fb;
 const vin=t[14],vl=t[17],vh=t[18],i1=t[3],i2=t[10];
 const mn=g(33,21000),mx=g(34,28000),dc=g(27,14800),ab=g(29,6000),hd=g(35,950),ov=g(36,15000),fl=g(37,2000);
 $('v0').textContent=v2(vin);$('v1').textContent=v2(t[15]);$('v2').textContent=v2(t[16]);
 $('v3').textContent=v2(vh);$('v4').textContent=v2(vl);
 $('v5').textContent=i1+'mA';$('v6').textContent=i2+'mA';
 $('fv5').textContent='Hard fault '+hd+'mA';$('fv6').textContent='Hard fault '+hd+'mA';
 const set=(k,txt,cls)=>{ASB.sp[k].textContent=txt;ASB.sp[k].className='tg '+cls;ASB.sr[k].className=cls==='g'?'rok':cls==='y'?'rwr':'rbd';};
 const vinOk=vin>=mn&&vin<=mx;
 set(0,vinOk?'داخل بازه':'خارج بازه',vinOk?'g':'r');
 set(1,'','g');set(2,'','g');
 [[vh,3],[vl,4]].forEach(B=>{const v=B[0],over=v>=dc,lost=v<ab,inv=v<fl||v>=ov,bad=over||lost||inv;
  set(B[1],bad?(over?'بالای قطع':lost?'غایب':'نامعتبر'):'سالم',bad?'r':'g');});
 [[i1,5],[i2,6]].forEach(C=>{const v=C[0];
  set(C[1],v>=hd?'تریپ':v>=hd-100?'نزدیک تریپ':'سالم',v>=hd?'r':v>=hd-100?'y':'g');});
 if(t[19]!==ASB.mask){ASB.mask=t[19];
  if(!t[19]){ASB.flt.textContent='✅ بدون خطای قفل‌شده';ASB.box5.className='ab good';}
  else{let h='';for(let bit=0;bit<7;bit++)if(t[19]&(1<<bit))h+=`<div>⚠ <b>${FEXP[bit][0]}</b> — ${FEXP[bit][1]}</div>`;
   if(t[19]&~127)h+=`<div>⚠ بیت ناشناخته: <span class="n">fault 0x${t[19].toString(16)}</span></div>`;
   ASB.flt.innerHTML=h;ASB.box5.className='ab bad';}}
 ASB.tick=!ASB.tick;if(ASB.live)ASB.live.style.opacity=ASB.tick?1:.3;}
/* ===== v1.16: آینهٔ LED و بازر برد — همان اولویت Ui_Tick با مقادیر اعمال‌شده؛ چشمک با همان دوره/duty برد (فاز محلی، هم‌سرعت) ===== */
/* [EN] v1.53 (user order): the board-driven LED/buzzer mirror is gone - the
   per-card simulators replaced it, and dropping it also drops the 50ms
   telemetry-replay maths. What is left is the fault-bit mirror plus the
   periodic refresh of the derived card numbers.
   [FA] آینهٔ LED و بازرِ برد حذف شد (شبیه‌ساز کارت‌ها جایش را گرفت)؛ فقط
   آینهٔ بیت‌های خطا و تازه‌سازی دوره‌ای اعداد کارت‌ها مانده است. */
function uview(){
 const now=performance.now();
 if(ASB&&ASB.bits&&D&&D.t){const m=D.t[19]||0,ph=(now%500)<250;
  /* v1.79: بیتِ قفل‌شده همیشه دیده می‌شود (set) و فقط روشنی‌اش چشمک می‌زند (on) */
  ASB.bits.forEach((e,bit)=>{const on=(m&(1<<bit))!==0;if(e)e.className='bit'+(on?' set':'')+((on&&ph)?' on':'');});}
 try{c4();sall();}catch(e){}
}
/* ==================== سناریو ۴ — اعداد زندهٔ کارت شارژ ==================== */
/* [EN] Mirror of func__Ui_ScenarioCharging_Tick arithmetic, nothing more: the
   card must SHOW the numbers the board will use, so the maths here is the
   same order of steps (remaining -> step -> on -> clamp -> off) and reads
   the typed-or-applied values through the same helpers as the guards.
   [FA] آینهٔ همان حساب تیک شارژ برد - نه چیز بیشتر: کارت باید همان اعدادی را
   نشان دهد که برد به‌کار می‌برد، پس گام‌ها به همان ترتیب‌اند و مقدارها از همان
   «تایپ‌شده یا اعمال‌شده» خوانده می‌شوند. */
const C4={sig:'',pct:[0,10,25,50,75,90,98,100]};
function c4v(id,fb){const e=$('q'+id);if(e&&e.value!==''){const n=parseInt(e.value,10);if(!isNaN(n))return n;}
 return (D&&D.p&&D.p[id]!=null)?D.p[id]:fb;}
function c4mv(p,lo,hi){const span=hi-lo;if(span<=0)return lo;const off=Math.ceil(span*p/100);return lo+off;}
function c4blink(pct,per,minOn){
 let remaining=pct>=100?0:100-pct;
 if(remaining<2)remaining=2;                       /* کف ۲٪ مثل برد */
 const stepMs=Math.floor(per/100);
 let onMs=remaining*stepMs;
 if(onMs<minOn)onMs=minOn;
 if(onMs>per)onMs=per;
 return {remaining,onMs,offMs:per-onMs};}
function c4(){
 const tb=$('c4tb');if(!tb)return;
 /* v1.49: کارت شارژ از نردبان سمت شارژ (۱۱۹/۱۲۰) می‌خواند، نه از نردبان دشارژ */
 const lo=c4v(119,21000),hi=c4v(120,29000),per=c4v(68,1000),minOn=c4v(69,150),en=c4v(77,100),ex=c4v(78,95),hy=c4v(79,5);
 const sig=[lo,hi,per,minOn,en,ex,hy].join(',');
 if(sig!==C4.sig){
  C4.sig=sig;
  const m=$('c4map');
  if(m)m.innerHTML=hi>lo
   ?('۰٪ = <span class="n">'+lo+'</span>mV · ۱۰۰٪ = <span class="n">'+hi+'</span>mV · هر ۱٪ ≈ <span class="n">'+Math.round((hi-lo)/100)+'</span>mV · ۵۰٪ ≈ <span class="n">'+c4mv(50,lo,hi)+'</span>mV')
   :'⚠ حد بالا باید دست‌کم ۱۰۰mV بالاتر از حد پایین باشد.';
  const f=$('c4full');
  if(f)f.innerHTML='ورود فول <span class="n">'+en+'</span>٪ ≈ <span class="n">'+c4mv(en,lo,hi)+'</span>mV (سبز ثابت) · خروج فول <span class="n">'+ex+'</span>٪ ≈ <span class="n">'+c4mv(ex,lo,hi)+'</span>mV (برگشت به شارژ) · پهنای hysteresis <span class="n">'+(en-ex)+'</span>٪ ≈ <span class="n">'+(c4mv(en,lo,hi)-c4mv(ex,lo,hi))+'</span>mV';
  let rows='';
  C4.pct.forEach(p=>{
   const b=c4blink(p,per,minOn);
   const note=p===0?'زرد ثابت روشن (درصد صفر)':(p>=en?'فول — زرد خاموش، سبز ثابت':(p>=98?'کمینهٔ چشمک (کف ۲٪ و حداقل روشنی)':'چشمک معمولی'));
   rows+='<tr><td>'+p+'٪</td><td class="n">'+c4mv(p,lo,hi)+'mV</td><td>'+b.remaining+'٪</td><td class="n">'+b.onMs+'ms</td><td class="n">'+b.offMs+'ms</td><td class="lb">'+note+'</td></tr>';});
  tb.innerHTML=rows;}
}


/* ==================== شبیه‌ساز زندهٔ سناریو / Live scenario simulator ==================== */
/* [EN] v1.51: each card drives its own LED trio and buzzer from the values
   typed in THAT card, replayed on the panel clock with the board's own
   phase/duty/window maths. No telemetry is read: the picture shows what the
   board WILL do with these numbers, not what it is doing now.
   [FA] سه LED و بازر هر کارت فقط از روی اعداد همان کارت ساخته می‌شوند و با
   ساعت خود پنل پخش می‌شوند؛ چیزی از برد خوانده نمی‌شود. پس آنچه می‌بینید
   «کاری است که برد با این اعداد خواهد کرد»، نه وضعیت فعلی برد. */
/* [EN] v1.57 (user order: "the simulator does not really work"): each card
   now owns a VIRTUAL clock instead of reading the wall clock. It only
   advances while that card is running, and it advances by the chosen speed,
   so a 30 s stability timer or a one-hour beep period can actually be seen.
   Stop really stops, and restart really restarts.
   [FA] هر کارت ساعت مجازی خودش را دارد: فقط وقتی کارت در حال اجراست جلو
   می‌رود و با ضریب سرعت انتخابی جلو می‌رود، پس تایمر ۳۰ ثانیه‌ای یا بوق
   ساعتی هم دیده می‌شود. «توقف» واقعاً متوقف می‌کند. */
var SIMON=[0,1,1,1,1,1,1],SIMT=[0,0,0,0,0,0,0],SIMC={b:-1,t:0},SIMSPD=1,SIMLAST=0;
function simspd(v){SIMSPD=parseInt(v,10)||1;
 [1,2,3,4,5,6].forEach(n=>{const e=$('sims'+n);if(e)e.value=String(SIMSPD);});}
function simtog(n){SIMON[n]=SIMON[n]?0:1;const b=$('simb'+n);
 if(b)b.textContent=SIMON[n]?'توقف':'ادامه';}
/* [EN] Advance the virtual clocks once per frame. [FA] جلو بردن ساعت‌ها. */
function simclk(){const r=performance.now();
 const dt=SIMLAST?Math.min(500,r-SIMLAST):0;SIMLAST=r;
 for(let n=1;n<=6;n++)if(SIMON[n])SIMT[n]+=dt*SIMSPD;}
function simlbl(n){const r=$('simp'+n),v=$('simv'+n);if(r&&v)v.textContent=r.value+'٪';}
function simval(n,d){const r=$('simp'+n);return r?parseInt(r.value,10):d;}
/* [EN] One LED trio. [FA] یک سه‌تایی LED. */
function simset(n,r,g,y,z,t){
 const L=[['r',r],['g',g],['y',y]];
 L.forEach(c=>{const e=$('sl'+n+c[0]);if(e)e.className='sl '+c[0]+(c[1]?' on':'');});
 const b=$('sl'+n+'z');if(b){b.className='szz'+(z?' on':'');b.textContent=z?'🔊':'🔇';}
 stxt('sl'+n+'t',t);}
/* [EN] Inside one beep of this period? [FA] داخل یکی از بوق‌های این دوره؟ */
function simbz(ph,per,dur,cnt,gap){
 if(per<=0||cnt<=0||dur<=0)return false;
 const p=ph%per;
 for(let k=0;k<cnt;k++){const st=k*(dur+gap);if(p>=st&&p<(st+dur))return true;}
 return false;}
function simblink(ph,per,duty){const on=Math.floor(per*duty/100);return (ph%per)<on;}

/* [EN] v1.55 (user order): a gap only exists BETWEEN beeps, so with one beep
   per round the gap box is switched off and cannot be typed into. Gap 65 is
   shared by band 1, band 3 and the critical band, so it stays alive while any
   of those still asks for more than one beep.
   [FA] گپ فقط «بین» بوق‌هاست؛ با یک بوق در هر دور، کادر گپ خاموش می‌شود و
   نمی‌شود رویش چیزی نوشت. گپ ۶۵ مشترک است، پس تا وقتی یکی از باندهایش بیش از
   یک بوق بخواهد روشن می‌ماند. */
/* [EN] v1.77 (user question: "with a single beep, what does the gap between
   beeps even mean?" and "you did it for some sections, do it everywhere"):
   the table had TWO faults. Band 2's repeat interval (122) was listed as if
   it were a gap, so asking for one beep wrongly killed the interval - an
   interval is the time BETWEEN ROUNDS and means something even with a single
   beep. And the shared gap 65 did not count band 2 (63) among its users,
   although band 2 beeps with that same gap. Every gap field in the page is
   in this table now, and nothing that is not a gap is.
   [FA] جدول دو ایراد داشت: «فاصلهٔ تکرارِ» باند ۲ (۱۲۲) اشتباهاً گپ حساب شده
   بود و با یک بوق خاموش می‌شد - حال آنکه فاصلهٔ تکرار بین دورهاست و با یک
   بوق هم معنا دارد؛ و گپ مشترک ۶۵ باند ۲ (۶۳) را جزو مصرف‌کننده‌هایش
   نمی‌شمرد. حالا همهٔ گپ‌های صفحه اینجا هستند و فقط گپ‌ها. */
const GAPOF=[[43,[42]],[49,[48]],[65,[62,63,64,58]]];
function gapen(){GAPOF.forEach(g=>{const e=$('q'+g[0]);if(!e)return;
 const need=g[1].some(id=>c4v(id,1)>1);
 const why='با یک بوق در هر دور، فاصلهٔ بین بوق‌ها معنا ندارد';
 e.disabled=!need;e.title=need?'':why;
 const l=e.closest('label');if(l)l.style.opacity=need?'':'0.45';
 /* [EN] The read-only echoes of the same gap go grey with it, so a dead gap
    looks dead in every card that shows it, not just where it is typed.
    [FA] بازتاب‌های فقط-خواندنی همان گپ هم با آن خاکستری می‌شوند. */
 document.querySelectorAll('.qmv[data-q="'+g[0]+'"]').forEach(m=>{
  const h=m.closest('.shv')||m;h.style.opacity=need?'':'0.45';h.title=need?'':why;});});}
function simrun(){
 simclk();
 const now=SIMT[3];
 const ph=n=>SIMT[n];
 /* ۱) اضافه‌ولتاژ ورودی: سبز ثابت، قرمز با دوره/duty، بوق دوره‌ای */
 /* [EN] v1.57 (user question "when exactly does it beep? where is the
    threshold?"): the card now has the input-voltage slider, so the alarm
    only starts ABOVE threshold 70 and only clears below 70 - hysteresis 71,
    exactly like the board. [FA] اسلایدر ولتاژ ورودی اضافه شد: آژیر فقط
    بالای آستانه (۷۰) شروع می‌شود و فقط زیر «آستانه − hysteresis (۷۱)»
    پاک می‌شود. */
 {const per=c4v(38,1000),p=ph(1),mv=simval(1,26000),
   thr=c4v(70,29000),hys=c4v(71,500),clr=thr-hys;
  if(mv>thr)S1.al=true;else if(mv<clr)S1.al=false;
  stxt('sim1w','بوق و قرمز از بالاتر از '+thr+'mV شروع می‌شود و تا زیر '+clr+
   'mV (آستانه '+thr+' − hysteresis '+hys+') پاک نمی‌شود.');
  if(S1.al){
   simset(1,simblink(p,per,c4v(39,50)),true,false,
    simbz(p,c4v(40,10000),c4v(41,1000),c4v(42,1),c4v(43,0)),
    'ورودی '+mv+'mV بالای آستانه '+thr+'mV · سبز ثابت + قرمز چشمک‌زن + بوق · تا زیر '+clr+'mV پاک نمی‌شود.');
  }else{
   simset(1,false,true,false,false,
    'ورودی '+mv+'mV '+(mv>clr?'در بازهٔ hysteresis است (زیر آستانه اما بالای '+clr+'mV)':'زیر آستانه')+' · فقط سبز ثابت، بدون بوق.');}
  simlbl1();}
 /* ۲) قطع باتری: همان چهره با اعداد خودش */
 {const per=c4v(44,1000),p=ph(2);
  simset(2,simblink(p,per,c4v(45,50)),true,false,
   simbz(p,c4v(46,3000),c4v(47,233),c4v(48,3),c4v(49,100)),
   'سبز ثابت + قرمز چشمک‌زن؛ بوق طبق بخش ۴.');}
 /* ۳) دشارژ: درصدِ اسلایدر → باند، چشمک سبز و بوق همان باند */
 {const pct=simval(3,50),p=ph(3),per=c4v(66,1000),minOff=c4v(67,10);
  const b1=c4v(50,40),b2=c4v(51,20),b3=c4v(52,10),bc=c4v(53,1);
  let off=(100-pct)*Math.floor(per/100);if(off<minOff)off=minOff;if(off>per)off=per;
  const gOn=(p%per)<(per-off);
  let z=false,txt='';
  if(pct<bc){
   /* بحرانی: LEDها خاموش و الگو فقط یک‌بار به طول «پخش یک‌باره» */
   if(SIMC.b!==4){SIMC.b=4;SIMC.t=now;}
   const one=c4v(61,10000),el=now-SIMC.t;
   z=el<one&&simbz(el,c4v(56,10000),c4v(57,10000),c4v(58,1),c4v(65,100));
   simset(3,false,false,false,z,'باند بحرانی: هر سه LED خاموش · الگو فقط یک‌بار ('+sms(one)+') پخش می‌شود، بعد سکوت.');
   return simrest();}
  if(pct>=b1){SIMC.b=0;txt='بالای '+b1+'٪: فقط سبز چشمک می‌زند، بدون بوق.';}
  else if(pct>=b2){SIMC.b=1;z=simbz(p,c4v(54,60000),c4v(59,1000),c4v(62,1),c4v(65,100));txt='باند ۱ — یک بوق.';}
  else if(pct>=b3){SIMC.b=2;z=simbz(p,c4v(122,60000),c4v(121,1000),c4v(63,2),c4v(65,100));txt='باند ۲ — دو بوق (مدت و فاصلهٔ تکرار خودش).';}
  else{SIMC.b=3;z=simbz(p,c4v(55,20000),c4v(60,2000),c4v(64,3),c4v(65,100));txt='باند ۳ — سه بوق.';}
  simset(3,false,gOn,false,z,txt+' سبز: '+(per-off)+'ms روشن / '+off+'ms خاموش.');}
 simrest();}
/* [‎EN] Cards 4..6 kept apart from the critical early-return. [FA]‎ کارت‌های ۴ تا ۶. */
function simrest(){
 const now=SIMT[5];
 const ph=n=>SIMT[n];
 /* ۴) شارژ: سبز ثابت، زرد با «مانده تا فول»، فول = زرد خاموش */
 {const pct=simval(4,50),p=ph(4),per=c4v(68,1000),minOn=c4v(69,150),en=c4v(77,100);
  if(pct>=en)simset(4,false,true,false,false,'فول: سبز ثابت، زرد خاموش (درصد ≥ ورود فول '+en+'٪).');
  else if(pct===0)simset(4,false,true,true,false,'درصد صفر: زرد ثابت روشن.');
  else{let rem=100-pct;if(rem<2)rem=2;
   let on=rem*Math.floor(per/100);if(on<minOn)on=minOn;if(on>per)on=per;
   simset(4,false,true,(p%per)<on,false,'در حال شارژ: زرد '+on+'ms روشن / '+(per-on)+'ms خاموش · بوقی ندارد.');}}
 /* ۶) عدم‌توازن: حالت کاری (شارژ/استراحت/دشارژ) پنجره و حد را تعیین می‌کند */
 {const a=sv5('a'),bb=sv5('b'),d=Math.abs(a-bb),md=$('sim5m')?$('sim5m').value:'r',
   lim=(md==='d')?c4v(109,500):c4v(108,300),
   st=c4v(112,30000),ev=c4v(114,10),drop=c4v(113,100),
   wc=c4v(111,600000),wr=c4v(110,600000),
   per=c4v(115,3600000),len=c4v(116,200);
  const el=now-S5.t0;           /* زمان سپری‌شده از شروع همین حالت */
  /* پنجره: دشارژ فوری · شارژ بعد از ۱۱۱ (صفر=هرگز) · استراحت بعد از ۱۱۰ (صفر=خاموش) */
  let open=false,why='';
  if(md==='d'){open=true;why='دشارژ روی باتری: پنجره از همان لحظه باز است · حد دشارژ '+lim+'mV';}
  else if(md==='c'){if(wc===0){why='در حال شارژ: گیت صفر است، یعنی حین شارژ هرگز اندازه گرفته نمی‌شود';}
   else if(el>=wc){open=true;why='در حال شارژ: '+sms(wc)+' از شروع شارژ گذشته، پنجره باز است · حد استراحت '+lim+'mV';}
   else{why='در حال شارژ: تا باز شدن پنجره '+sms(wc-el)+' مانده (ولتاژ هنوز ننشسته)';}}
  else{if(wr===0){why='استراحت: گیت صفر است، یعنی حین استراحت اندازه گرفته نمی‌شود';}
   else if(el>=wr){open=true;why='استراحت: '+sms(wr)+' از پایان شارژ گذشته، پنجره باز است · حد استراحت '+lim+'mV';}
   else{why='استراحت: تا باز شدن پنجره '+sms(wr-el)+' مانده (ولتاژ هنوز ننشسته)';}}
  stxt('sim5w',why);
  if(!S5.lock&&open){
   if(d>lim){if(!S5.ht)S5.ht=now;
    if((now-S5.ht)>=st){S5.n++;S5.ht=0;S5.ep=true;if(S5.n>=ev){S5.lock=true;S5.lt=now;}}
   }else{S5.ht=0;if(S5.ep&&d<=(lim-drop))S5.ep=false;}
  }else if(!open){S5.ht=0;}
  if(S5.lock){
   /* چشمک قرمز با دوره و duty ۱۲۳/۱۲۴ — دقیقاً همان چیزی که برد می‌سازد (دورهٔ صفر = ثابت) */
   const bper=c4v(123,1000),bdt=c4v(124,50),ron=(bper===0)?true:simblink(now,bper,bdt);
   simset(5,ron,false,false,simbz(now-S5.lt,per,len,1,0),
    'قفل شد (رویداد '+ev+' اُم) · '+(bper===0?'قرمز ثابت':'چشمک قرمز '+Math.round(bper*bdt/100)+'/'+Math.round(bper*(100-bdt)/100)+'ms')+' · '+(per===0?'بوق خاموش':'یک بوق '+len+'ms هر '+sms(per))+
    ' · قفل با متوازن شدن باتری هم باز نمی‌شود.');
  }else{
   simset(5,false,true,false,false,
    'اختلاف '+d+'mV '+(d>lim?'بالاتر':'پایین‌تر')+' از حد '+lim+'mV · '+
    (open?(S5.ht?('در حال شمارش پایداری، تا رویداد بعدی '+sms(Math.max(0,st-(now-S5.ht)))):'شمارش پایداری شروع نشده'):'پنجره بسته است، پس شمارشی نیست')+
    ' · رویدادها: '+S5.n+' از '+ev+' · افت لازم برای بستن اپیزود: '+drop+'mV');}
 }
 /* ۷) باتری خراب: ساعتِ شارژِ پیوسته، وقفه، و حکم ۲۴ ساعته */
 {const n6=SIMT[6],md=$('sim6m')?$('sim6m').value:'c',
   lim=c4v(125,86400000),gap=c4v(126,600000),blk=c4v(127,0),
   per=c4v(115,3600000),len=c4v(116,200);
  const dt=S6.l?Math.max(0,n6-S6.l):0;S6.l=n6;
  if(!S6.lock){
   if(md==='f'){S6.acc=0;S6.pa=0;}
   else if(md==='c'){S6.acc+=dt;S6.pa=0;if(lim>0&&S6.acc>=lim){S6.lock=true;S6.lt=n6;}}
   else{S6.pa+=dt;if(S6.pa>=gap){S6.acc=0;}}
  }
  if(S6.lock){
   simset(6,true,false,false,simbz(n6-S6.lt,per,len,1,0),
    'حکم صادر شد: باتری پس از '+sms(lim)+' شارژ پیوسته شارژ نشد · قرمز ثابت · '+(per===0?'بوق خاموش':'یک بوق '+len+'ms هر '+sms(per))+' · شارژ این کانال قطع'+(blk?' و باتری از خروجی هم جدا شد':' (خروجی دست‌نخورده)')+' · فقط با تعویض باتری پاک می‌شود.');
  }else{
   simset(6,false,true,md==='c',false,
    (md==='f'?'رسید به Float: شارژ کامل شد و ساعت صفر شد.':md==='c'?('در حال شارژ · '+sms(Math.round(S6.acc))+' از '+(lim>0?sms(lim):'مهلت خاموش')+(lim>0?(' · تا حکم '+sms(Math.max(0,lim-Math.round(S6.acc)))+' مانده'):'')):('وقفهٔ شارژ · ساعت روی '+sms(Math.round(S6.acc))+' نگه داشته شده · اگر وقفه از '+sms(gap)+' بگذرد صفر می‌شود (مانده '+sms(Math.max(0,gap-Math.round(S6.pa)))+')')));}
 }
 gapen();
}
var S6={acc:0,pa:0,lock:false,lt:0,l:0};
function simrst6(){S6={acc:0,pa:0,lock:false,lt:0,l:SIMT[6]};}
/* [EN] The two imbalance knobs plus the counter they feed.
   [FA] دو ولوم عدم‌توازن و شمارنده‌ای که تغذیه می‌کنند. */
var S1={al:false};
function simlbl1(){const r=$('simp1'),v=$('simv1');if(r&&v)v.textContent=r.value+'mV';}
var S5={n:0,ht:0,lock:false,lt:0,ep:false,t0:0};
function sim5mode(){S5.t0=SIMT[5];S5.ht=0;S5.ep=false;}
function sv5(k){const r=$('simp5'+k);return r?parseInt(r.value,10):12000;}
function simlbl5(){['a','b'].forEach(k=>{const v=$('simv5'+k);if(v)v.textContent=sv5(k)+'mV';});}
function simrst5(){S5={n:0,ht:0,lock:false,lt:0,ep:false,t0:SIMT[5]};}
setInterval(simrun,60);

/* ==================== سناریوهای ۱،۲،۳،۵،۶ — اعداد زنده ==================== */
/* [EN] Same contract as c4(): the cards show the numbers the BOARD will use,
   computed from the typed-or-applied values with the board's own order of
   steps. No logic is invented here - every line mirrors a clamp or a tick
   that already exists in the firmware.
   [FA] همان قرارداد c4(): کارت‌ها همان اعدادی را نشان می‌دهند که برد به‌کار
   می‌برد، از مقدار «تایپ‌شده یا اعمال‌شده» و با همان ترتیب گام‌های برد. هیچ
   منطقی اینجا ساخته نمی‌شود؛ هر خط آینهٔ یک گیره یا یک تیک موجود است. */
function sms(ms){
 if(ms===0)return 'خاموش';
 if(ms<1000)return ms+'ms';
 if(ms<60000)return (Math.round(ms/100)/10)+' ثانیه';
 return (Math.round(ms/6000)/10)+' دقیقه';}
function sduty(per,duty){const onMs=Math.floor(per*duty/100);return {onMs,offMs:per-onMs};}
function swin(per,dur,cnt,gap){
 const winMs=dur*cnt+(cnt>1?gap*(cnt-1):0);
 const fits=per>0&&dur>0&&cnt>0&&winMs<=per;
 const sil=per>0?Math.max(0,per-winMs):0;
 return {winMs,fits,sil};}
function stxt(el,html){const e=$(el);if(e)e.innerHTML=html;}
/* ==================== مقدار مشترک و پیش‌فرض کارخانه ==================== */
/* [EN] A shared parameter has exactly ONE writable field. Everywhere else it
   is echoed read-only from the SAME source the writable field uses, with a
   pointer to the owner. Two editable copies of one register cannot be kept
   honest: the loser of the last edit keeps showing a number the board does
   not hold.
   [FA] هر پارامتر مشترک دقیقاً یک فیلد قابل‌نوشتن دارد؛ جاهای دیگر فقط بازتاب
   فقط-خواندنی از همان منبع‌اند با اشاره به صاحبش. دو نسخهٔ قابل‌ویرایش از یک
   رجیستر صادق نمی‌مانند. */
function qmfill(){
 document.querySelectorAll('.qmv').forEach(e=>{
  const id=+e.dataset.q,pr=$('q'+id);
  const applied=(D&&D.p&&D.p[id]!=null)?D.p[id]:null;
  const typed=(pr&&pr.value!=='')?parseInt(pr.value,10):NaN;
  const shown=!isNaN(typed)?typed:(applied!=null?applied:pdflt(id));
  e.textContent=shown==null?'':shown;
  /* عدد تایپ‌شده و هنوز اعمال‌نشده با رنگ هشدار دیده شود */
  e.style.color=(!isNaN(typed)&&applied!=null&&typed!==applied)?'var(--wa)':'';});}
/* [EN] Factory default of a parameter id, from the same tables the reset
   buttons use - one source, so a default can never drift from its button.
   [FA] پیش‌فرض کارخانهٔ هر شناسه، از همان جدولی که دکمه‌های بازگردانی
   می‌خوانند - یک منبع، پس پیش‌فرض از دکمه‌اش جدا نمی‌افتد. */
function pdflt(id){
 id=+id;
 /* v1.75 audit: CDEF grew to 119..127 with scenario 6 (ids 125..127) but this
    window still said 124, so the dead-battery fields printed no factory default. */
 if(id>=119&&id<=131)return CDEF[id-119];
 if(id>=108&&id<=118)return IDEF[id-108];
 if(id>=93&&id<=107)return LDEF[id-93];
 if(id>=83&&id<=92)return PDEF[id-83];
 if(id>=27&&id<=82)return ADEF[id-27];
 return null;}
/* [EN] Put the factory default under every numeric field and wrap the caption
   so the inputs line up (the caption takes the slack, not the input).
   [FA] پیش‌فرض کارخانه زیر هر فیلد عددی، و بسته‌بندی عنوان تا ورودی‌ها هم‌تراز
   بمانند (فضای خالی را عنوان می‌گیرد، نه ورودی). */
function qdeco(){
 document.querySelectorAll('.bqr label').forEach(l=>{
  const inp=l.querySelector('input[type=number]');
  if(!inp||l.querySelector('.t'))return;
  const cap=document.createElement('span');
  cap.className='t';
  while(l.firstChild&&l.firstChild!==inp){cap.appendChild(l.firstChild);}
  l.insertBefore(cap,inp);
  const id=(inp.id&&inp.id.charAt(0)==='q')?inp.id.slice(1):null;
  const d=id?pdflt(id):null;
  if(d==null)return;
  const hint=document.createElement('span');
  hint.className='dflt';
  hint.innerHTML='پیش‌فرض کارخانه: <b>'+d+'</b>';
  l.appendChild(hint);});}
qdeco();
function sall(){
 if(!$('s1v'))return;
 qmfill();
 const now=performance.now(),live=!!(D&&D.t&&D.on==1);
 const lo=c4v(74,21000),hi=c4v(75,29000);
 /* v1.46: درصد با کف‌گیری (مثل برد) و ولتاژ با سقف‌گیری (اولین mV که برد همان درصد را گزارش می‌کند) */
 const pmv=p=>hi>lo?lo+Math.ceil((hi-lo)*p/100):lo;
 const mvp=mv=>hi>lo?Math.max(0,Math.min(100,Math.floor((mv-lo)*100/(hi-lo)))):0;
 /* --- سناریو ۱: اضافه‌ولتاژ ورودی --- */
 {const t=c4v(70,28000),h=c4v(71,1000),per=c4v(38,1000),d=sduty(per,c4v(39,50)),
   b=swin(c4v(40,10000),c4v(41,1000),c4v(42,1),c4v(43,0));
  stxt('s1v','ورود: ورودی بالاتر از <span class="n">'+t+'</span>mV · خروج: افت تا <span class="n">'+(t-h)+'</span>mV یا پایین‌تر · پهنای ضدلرزش <span class="n">'+h+'</span>mV');
  stxt('s1b','قرمز: <span class="n">'+d.onMs+'</span>ms روشن / <span class="n">'+d.offMs+'</span>ms خاموش — هر دوره <span class="n">'+per+'</span>ms');
  stxt('s1z',b.fits?('الگوی بوق: <span class="n">'+c4v(42,1)+'</span> بوق × <span class="n">'+c4v(41,1000)+'</span>ms (+ گپ) = پنجرهٔ <span class="n">'+b.winMs+'</span>ms، تکرار هر '+sms(c4v(40,10000))+' · سکوت بین دو الگو '+sms(b.sil)):(c4v(40,10000)===0?'بوق خاموش است (دوره = ۰).':'⚠ پنجرهٔ بوق <span class="n">'+b.winMs+'</span>ms از دوره <span class="n">'+c4v(40,10000)+'</span>ms بزرگ‌تر است — برد بی‌صدا می‌ماند.'));
  }
 /* --- سناریو ۲: قطع باتری --- */
 {const per=c4v(44,1000),d=sduty(per,c4v(45,50)),
   b=swin(c4v(46,3000),c4v(47,233),c4v(48,3),c4v(49,100));
  stxt('s2v','قطع حین شارژ: بالای <span class="n">'+c4v(27,14800)+'</span>mV به مدت <span class="n">'+c4v(28,150)+'</span>ms · غیبت: زیر <span class="n">'+c4v(29,6000)+'</span>mV به مدت '+sms(c4v(31,1000))+' · بازگشت: بالای <span class="n">'+c4v(30,7000)+'</span>mV به مدت '+sms(c4v(32,1000))+' · فاصلهٔ غیبت تا بازگشت <span class="n">'+(c4v(30,7000)-c4v(29,6000))+'</span>mV');
  stxt('s2b','قرمز: <span class="n">'+d.onMs+'</span>ms روشن / <span class="n">'+d.offMs+'</span>ms خاموش — هر دوره <span class="n">'+per+'</span>ms');
  stxt('s2z',b.fits?('الگوی بوق: <span class="n">'+c4v(48,3)+'</span> بوق × <span class="n">'+c4v(47,233)+'</span>ms + <span class="n">'+(c4v(48,3)>1?c4v(49,100)*(c4v(48,3)-1):0)+'</span>ms مجموع گپ‌ها = <span class="n">'+b.winMs+'</span>ms، تکرار هر '+sms(c4v(46,3000))):(c4v(46,3000)===0?'بوق خاموش است (دوره = ۰).':'⚠ پنجرهٔ بوق <span class="n">'+b.winMs+'</span>ms در دوره <span class="n">'+c4v(46,3000)+'</span>ms جا نمی‌شود — برد بی‌صدا می‌ماند.'));
  }
 /* --- سناریو ۳: دشارژ --- */
 {const per=c4v(66,1000),minOff=c4v(67,10),step=Math.floor(per/100),
   gb=p=>{let off=(100-p)*step;if(off<minOff)off=minOff;if(off>per)off=per;return {onMs:per-off,offMs:off};},
   b1=c4v(50,40),b2=c4v(51,20),b3=c4v(52,10),bc=c4v(53,1),gap=c4v(65,100),
   /* هر باند: سقف، کف، تعداد، مدت، فاصله */
   /* v1.71: هر باند تعداد، مدت و فاصلهٔ تکرار خودش را دارد؛ فقط گپ مشترک است */
   bands=[[b1,100,0,0,0,gap],[b2,b1,c4v(62,1),c4v(59,1000),c4v(54,60000),gap],
          [b3,b2,c4v(63,2),c4v(121,1000),c4v(122,60000),gap],
          [bc,b3,c4v(64,3),c4v(60,2000),c4v(55,20000),gap]];
  stxt('s3m',hi>lo?('۰٪ = <span class="n">'+lo+'</span>mV · ۱۰۰٪ = <span class="n">'+hi+'</span>mV · هر ۱٪ ≈ <span class="n">'+Math.round((hi-lo)/100)+'</span>mV · همین دو عدد درصد باندهای زیر را می‌سازند')
   :'⚠ حد بالا باید دست‌کم ۱۰۰mV بالاتر از حد پایین باشد.');
  bands.forEach((b,k)=>{
   const g=gb(Math.max(0,Math.min(100,Math.round((b[0]+b[1])/2))));
   stxt('s3r'+k,'از <span class="n">'+b[0]+'</span>٪ تا <span class="n">'+b[1]+'</span>٪ · <span class="n">'+pmv(b[0])+'</span>..<span class="n">'+pmv(b[1])+'</span>mV');
   let txt='سبز در میانهٔ باند: <span class="n">'+g.onMs+'</span>ms روشن / <span class="n">'+g.offMs+'</span>ms خاموش · ';
   /* v1.50 (دستور کاربر): باند بی‌صدا بلوک جدا ندارد؛ حرفش یک خط زیر باند ۱ است */
   if(k===0){stxt('s3q','بالاتر از <span class="n">'+b1+'</span>٪ هیچ بوقی نیست، فقط سبز چشمک می‌زند (سبز در میانهٔ آن بازه: <span class="n">'+g.onMs+'</span>ms روشن / <span class="n">'+g.offMs+'</span>ms خاموش).');return;}
   else{const bg=b[5],w=swin(b[4],b[3],b[2],bg);
    txt+='بوق: <span class="n">'+b[2]+'</span> × <span class="n">'+b[3]+'</span>ms + <span class="n">'+(b[2]>1?bg*(b[2]-1):0)+'</span>ms گپ = پنجرهٔ <span class="n">'+w.winMs+'</span>ms، تکرار هر '+sms(b[4])+' · ';
    txt+=w.fits?('<span class="okc">در فاصله جا می‌شود</span> · سکوت بین دو الگو '+sms(w.sil)):(b[4]===0||b[2]===0?'<span class="lb">خاموش (فاصله یا تعداد صفر است)</span>':'<span class="erc">⚠ در فاصله جا نمی‌شود — برد این باند را بی‌صدا می‌گذارد</span>');}
   stxt('s3n'+k,txt);});
  {const cnt=c4v(58,1),one=c4v(61,10000),cw=swin(c4v(56,10000),c4v(57,10000),cnt,gap),cfit=cw.fits;
   stxt('s3r4','از <span class="n">0</span>٪ تا <span class="n">'+bc+'</span>٪ · <span class="n">'+pmv(0)+'</span>..<span class="n">'+pmv(bc)+'</span>mV');
   stxt('s3n4','LEDها خاموش · بوق: <span class="n">'+cnt+'</span> × <span class="n">'+c4v(57,10000)+'</span>ms + <span class="n">'+(cnt>1?gap*(cnt-1):0)+'</span>ms گپ = پنجرهٔ <span class="n">'+cw.winMs+'</span>ms، تکرار هر '+sms(c4v(56,10000))+' · کل پخش فقط یک‌بار به طول '+sms(one)+'، بعد سکوت تا بالا رفتن باتری از <span class="n">'+bc+'</span>٪ · '+(cfit?'<span class="okc">در فاصله جا می‌شود</span>':(c4v(56,10000)===0||cnt===0?'<span class="lb">خاموش (فاصله یا تعداد صفر است)</span>':'<span class="erc">⚠ در فاصله جا نمی‌شود — برد این باند را بی‌صدا می‌گذارد</span>')));}
  const ordOk=(bc<=b3)&&(b3<=b2)&&(b2<=b1);
  stxt('s3z',ordOk?'ترتیب باندها درست است: <span class="n">'+bc+'</span> ≤ <span class="n">'+b3+'</span> ≤ <span class="n">'+b2+'</span> ≤ <span class="n">'+b1+'</span> ≤ 100٪.':'⚠ ترتیب باندها به هم خورده (باید بحرانی ≤ سه-بوق ≤ دو-بوق ≤ شروع بوق باشد) — برد آن‌ها را گیره می‌زند.');
 }
 /* --- سناریو ۳: معنی پایداری درصد --- */
 {const hyst=c4v(80,2),z=c4v(81,2),ex=35;
  stxt('s3hy','<b>ساده بگوییم:</b> عدد درصد از روی ولتاژ حساب می‌شود و ولتاژ باتری مدام کمی بالا-پایین می‌پرد (موتور روشن شود، بار وصل شود...). پس درصدِ لحظه‌ای هم می‌لرزد. برد برای چشمک سبز و بوق‌ها از عدد لرزان استفاده نمی‌کند؛ یک عدد آرام نگه می‌دارد که فقط وقتی عدد لرزان واقعاً دور شد، تکان می‌خورد. اسم آن عدد آرام «درصد پایدار» است. '
   +'<b>۱) حدِ تکان خوردن (<span class="n">'+hyst+'</span>)</b> — فرض کنید عدد آرام <span class="n">'+ex+'</span>٪ است. تا وقتی عدد لرزان بین <span class="n">'+(ex-hyst+1)+'</span> و <span class="n">'+(ex+hyst-1)+'</span> بالا-پایین می‌رود، نمایش و بوق‌ها همان <span class="n">'+ex+'</span> می‌مانند؛ به <span class="n">'+(ex+hyst)+'</span> (یا <span class="n">'+(ex-hyst)+'</span>) که برسد، عدد آرام می‌پرد روی همان. هرچه این عدد بزرگ‌تر، آرام‌تر و دیرتر؛ صفر یعنی بدون آرام‌سازی. '
   +'<b>۲) بیرون آمدن از ۰٪ (<span class="n">'+z+'</span>)</b> — وقتی روی ۰٪ ایستاده‌ایم، با یک جرقهٔ ولتاژ از ۰ بیرون نمی‌آید؛ باید عدد لرزان دست‌کم <span class="n">'+z+'</span> شود، و آن‌وقت فقط یک پله به ۱٪ می‌رود. '
   +'<b>چرا مهم است؟</b> دقیقاً سرِ مرزِ باندها، بدون این اعداد بوق هر چند ثانیه شروع و قطع می‌شد. با این‌ها یک‌بار تصمیم گرفته می‌شود و پای آن می‌ماند.');}
 /* --- سناریو ۵: عدم‌توازن --- */
 {const ev=c4v(114,10),st=c4v(112,30000),wr=c4v(110,600000),wc=c4v(111,600000);
  stxt('s5v','شمارش فقط پس از '+sms(wr)+' از پایان شارژ و '+(wc===0?'<b>بدون گیت</b> حین شارژ':sms(wc)+' از شروع شارژ')+' · حین دشارژ بدون گیت با حد <span class="n">'+c4v(109,500)+'</span>mV · هر رویداد = ماندن بالای حد به مدت '+sms(st)+' · اپیزود با افت <span class="n">'+c4v(113,100)+'</span>mV زیر حد بسته می‌شود');
  stxt('s5z','قفل در رویداد شمارهٔ <span class="n">'+ev+'</span> · کمترین زمان ممکن تا قفل ≈ '+sms(ev*st)+' (اگر اختلاف پشت‌سرهم بالای حد بماند) · در قفل: '+(c4v(123,1000)===0?'<span class=\"n\">قرمز ثابت</span>':'چشمک قرمز <span class=\"n\">'+Math.round(c4v(123,1000)*c4v(124,50)/100)+'</span>ms روشن / <span class=\"n\">'+Math.round(c4v(123,1000)*(100-c4v(124,50))/100)+'</span>ms خاموش')+' + '+(c4v(115,3600000)===0?'بوق خاموش':'بوق <span class="n">'+c4v(116,200)+'</span>ms هر '+sms(c4v(115,3600000)))+' · پس از قفل تا <span class="n">'+c4v(118,20)+'</span> سیکل شارژ مجاز است، بعد شارژ هم قطع می‌شود · خروج فقط با تعویض باتری (۳ ثانیه)');}
}
function xmute(){const v=(D&&D.p&&D.p[76]===1)?0:1;const f=$('q76');if(f)f.value=v;send(76,v);}
/* اتصال ورودی‌های آلارم (۲۷..۸۲): مثل profile + نگهبان + ‎q2/q3‎ برای شناسه‌های ۳۲..۸۲ */
for(const id of AIDS){const e=$('q'+id);if(!e)continue;e.onchange=()=>{const v=parseInt(e.value,10);if(isNaN(v))return;
 const m=achk().filter(x=>x.ids.includes(id));
 if(m.length&&!confirm('⚠ '+m.map(x=>x.msg).join('\n')+'\n\nبرد مقدار را گیره می‌زند تا مجموعه سازنده بماند. باز هم ارسال شود؟')){e.value='';afresh();return;}
 qput(id,v);};e.oninput=(id>=83?pchk:afresh);}
/* ===== v1.15b: پشتیبان‌گیری JSON تنظیمات (فیلتر + profile + آلارم‌ها) =====
   [EN] v1.57 (user order "add whatever the backup still needs"): the file now
   carries an identity - panel build stamp, parameter count, the id list it was
   taken from and a timestamp - so restoring it onto a board that was flashed
   with a DIFFERENT firmware can be noticed instead of silently writing a
   number into an id that now means something else. On the way back in the
   values go through the SAME fixrules() the send key uses (v1.56), and
   through each parameter's own min/max, so an old or hand-edited file can
   never push an impossible combination onto the board.
   [FA] فایل پشتیبان حالا شناسنامه دارد: مهر بیلد پنل، تعداد پارامترها، فهرست
   شناسه‌ها و تاریخ. پس اگر روی بردی با فرم‌ور دیگر بازخوانی شود، به‌جای
   نوشتن بی‌صدای عدد در شناسه‌ای که معنایش عوض شده، هشدار می‌گیرید. موقع
   بازخوانی هم مقادیر از همان قوانین fixrules و از بازهٔ مجاز هر پارامتر
   رد می‌شوند. */
const XIDS=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,20,21,22,23,24,25,26];AIDS.forEach(id=>{if(id!==76)XIDS.push(id);});
const xbuild=()=>{const e=$('bs');return e?e.textContent.replace('build ','').trim():'?';};
function xexp(){const x=$('xst');if(!D||!D.p){if(x)x.textContent='هنوز داده‌ای از برد نرسیده';return;}
 const o={app:'ChangeOver-settings',v:2,build:xbuild(),pn:PN,ids:XIDS.length,
  saved:new Date().toISOString(),params:{}};
 XIDS.forEach(id=>{o.params[id]=D.p[id];});
 const u=URL.createObjectURL(new Blob([JSON.stringify(o,null,1)],{type:'application/json'}));
 const a=document.createElement('a');a.href=u;
 a.download='changeover-settings-'+o.build+'-'+o.saved.slice(0,10)+'.json';a.click();
 setTimeout(()=>URL.revokeObjectURL(u),2000);
 if(x)x.textContent='⬇ خروجی گرفته شد ('+XIDS.filter(id=>D.p[id]!=null).length+' مقدار اعمال‌شده · بیلد '+o.build+')';}
/* [EN] Clamp one imported number to that parameter's own range.
   [FA] محدودکردن یک عدد واردشده به بازهٔ خود همان پارامتر. */
function xclamp(id,n){const e=$('q'+id);
 if(e){const lo=+e.min,hi=+e.max;
  if(e.min!==''&&Number.isFinite(lo))n=Math.max(lo,n);
  if(e.max!==''&&Number.isFinite(hi))n=Math.min(hi,n);
  return n;}
 const pr=(typeof P!=='undefined')?P[id]:null;
 if(!pr||!Number.isFinite(+pr[2])||!Number.isFinite(+pr[3]))return n;
 return Math.min(+pr[3],Math.max(+pr[2],n));}
async function ximp(f){const x=$('xst');let o;try{o=JSON.parse(await f.text());}catch(e){if(x)x.textContent='⚠ فایل JSON معتبر نیست';return;}
 if(!o||o.app!=='ChangeOver-settings'){if(x)x.textContent='⚠ این فایل پشتیبان پنل ChangeOver نیست';return;}
 /* v1.64: وسط داده‌برداری چیزی روی برد نوشته نشود */
 if(typeof W!=='undefined'&&W&&W.run){if(x)x.textContent='⛔ داده‌برداری بنچ در جریان است؛ اول آن را تمام کنید.';return;}
 const ps=o.params?o.params:{};
 /* --- شناسنامه: اگر فایل مال بیلد دیگری است، صریح بپرس --- */
 const warn=[];
 if(o.build&&o.build!==xbuild())warn.push('فایل از بیلد '+o.build+' گرفته شده و پنل فعلی بیلد '+xbuild()+' است');
 if(o.pn&&o.pn!==PN)warn.push('تعداد پارامترها فرق دارد ('+o.pn+' در فایل، '+PN+' در این نسخه)');
 if(!o.v||o.v<2)warn.push('فایل قدیمی است و شناسنامهٔ نسخه ندارد');
 const outs=Object.keys(ps).filter(k=>+k>=PN);
 if(outs.length)warn.push(outs.length+' شناسه در این نسخه وجود ندارد و نادیده گرفته می‌شود');
 let jobs=XIDS.filter(id=>Number.isFinite(+ps[id])).map(id=>[id,xclamp(id,Math.round(+ps[id]))]);
 if(!jobs.length){if(x)x.textContent='⚠ هیچ مقدار معتبری در فایل نیست';return;}
 const outr=jobs.filter(j=>Math.round(+ps[j[0]])!==j[1]);
 /* --- همان قوانین مشترک کلید ارسال (v1.56) روی مقادیر فایل --- */
 const v=rsnap();jobs.forEach(j=>{if(j[0] in v)v[j[0]]=j[1];});
 const fixed=fixrules(v);
 jobs=jobs.map(j=>[j[0],(j[0] in v)?v[j[0]]:j[1]]);
 const msg=jobs.length+' مقدار از فایل روی برد نوشته شود؟'+
  (warn.length?'\n\n⚠ '+warn.join('\n⚠ '):'')+
  (outr.length?'\n\n'+outr.length+' عدد به بازهٔ مجاز خودش محدود شد.':'')+
  (fixed.length?'\n\n'+fixed.length+' عدد برای سازگاری با بقیه جور شد: '+fixed.map(z=>z[0]+': '+z[1]+'→'+z[2]).join(' · '):'');
 if(!confirm(msg))  {if(x)x.textContent='بازخوانی لغو شد';return;}
 let ok=0;for(const j of jobs){try{const r=await fetch('/s?id='+j[0]+'&v='+j[1],{method:'POST'});if(r.ok)ok++;}catch(e){}if(x)x.textContent='… '+ok+'/'+jobs.length;await sl(60);}
 if(x)x.textContent=(ok===jobs.length?'✅ ':'⚠ ')+ok+'/'+jobs.length+' روی برد نوشته شد'+
  (fixed.length?' · '+fixed.length+' عدد پیش از نوشتن جور شد':'')+
  (warn.length?' · هشدار: '+warn.join(' · '):'');
 const xi=$('xim');if(xi)xi.value='';}
$('xim').onchange=e=>{if(e.target.files[0])ximp(e.target.files[0]);};
/* v1.27: سلامت لینک. تا پیش از این، اگر STM32 و ESP ناهماهنگ فلش می‌شدند پنل
   فقط خالی می‌ماند و هیچ توضیحی نبود — همان حالتی که عیناً شبیه کابل قطع است.
   حالا بایت نسخه در فریم هست و گیرنده ناهم‌نسخگی را می‌شمارد، پس می‌شود صریح گفت
   چه اتفاقی افتاده. خطاهای CRC هم نمایش داده می‌شوند تا هارنس نویزی دیده شود.
   v1.27: link health. A version-mismatched flash used to show an empty panel
   with no explanation - identical in appearance to an unplugged cable. */
function lnkhealth(d){const e=$('lnkw');if(!e)return;
 const vm=d.vm|0,ce=d.ce|0;
 if(vm>0){e.className='wbx bad';e.innerHTML='⛔ <b>نسخهٔ فرم‌ور و پنل یکی نیست</b> — '+vm+
  ' فریم با نسخهٔ ناشناخته رد شد. برد و ESP باید <b>با هم</b> دوباره فلش شوند (Clean + Rebuild کامل).';return;}
 if(ce>0){e.className='wbx warn';e.innerHTML='⚠ <b>'+ce+'</b> فریم به‌خاطر خطای CRC رد شد — اتصال سیم و زمین لینک را بررسی کنید. داده‌ها معتبرند؛ فقط بخشی از قاب‌ها رسیده‌اند.';return;}
 e.className='wbx';e.innerHTML='';}
function draw(d){D=d;const t=d.t,p=d.p,on=d.on==1,man=(d.fl&32)!=0;lnkhealth(d);qfill();cfill();afill();
/* [EN] The chart is mounted twice again (user order 2026-10-03: "why did you
   take the chart away entirely? go back to the previous version") - on the
   chargers page above the operating table, and in the settings tab where it
   had always been. ONE renderer fills both: qgraph() writes to every .qgm it
   finds, so the two copies cannot drift apart. The redraw must be gated on
   BOTH pages; a mount without a matching gate is a container that is
   present, empty and permanently silent while the markup looks correct.
   PID moved into sub-tab 0, so its cross-field warning runs there; the
   backup page is sub-tab 3 and needs neither.
   [FA] نمودار دوباره در دو جا نصب است (دستور کاربر: «چرا نمودار را کلاً
   بردی؟ برگرد به نسخهٔ قبلی») - روی صفحهٔ شارژرها بالای جدول عملکرد، و در تب
   تنظیمات همان‌جایی که همیشه بود. یک رندرکننده هر دو را پر می‌کند، پس دو
   نسخه نمی‌توانند از هم جدا بیفتند. بازرسم باید به هر دو صفحه مشروط باشد؛
   محل نصبی بدون گارد متناظر، ظرفی است که هست، خالی است و برای همیشه ساکت
   می‌ماند در حالی که مارک‌آپ درست به نظر می‌رسد. */
if(TAB==2&&STAB==0)qgraph();
if(TAB==2){if(STAB==0)pchk();else if(STAB!=3)afresh();}astat();
 document.body.classList.toggle('dn',!on);$('lk').classList.toggle('on',on);
 $('lt').innerHTML=on?`آنلاین · <span class="n">seq ${d.seq}</span>`:(d.n?'لینک قطع است':'در انتظار STM32…');
 hist(d);
 const F=[['snapshot',d.fl&1],['ورودی ۲۴V',d.fl&2],['اندازه‌گیری معتبر',d.fl&4]];
 $('fl').innerHTML=F.map(f=>`<span class="tg ${f[1]?'g':'r'}">${f[0]}</span>`).join('')+(t[19]&64?'<span class="tg r">خطا: باتری قطع</span>':'')+
  (t[19]&~64?`<span class="tg r n">fault 0x${t[19].toString(16)}</span>`:'')+(man?'<span class="tg y">مود دستی</span>':'');
 [1,2].forEach(n=>{const b=n==1?0:7,s=t[b+6],en=p[10+n],ce=p[12+n],fx=p[13+2*n];
  const st=$('st'+n);st.textContent=(ST[s]||'#'+s)+(fx===1&&!man?' · فیکس':'');st.className='tg '+(SC[s]||'');
  $('ie'+n).innerHTML=t[b+4]+' <span class="lb">mA</span>';$('du'+n).textContent=pc(t[b+5]);$('dc'+n).textContent=ce==null?'':'· سقف '+pc(ce);
  $('db'+n).style.width=Math.min(100,t[b+5]/10)+'%';$('cl'+n).style.left=(100-Math.min(100,(ce==null?1000:ce)/10))+'%';
  [0,1,2,3,4].forEach(k=>$('c'+n+k).textContent=t[b+k]);
  const g=$('tg'+n);g.textContent=en===0?'وصل مجدد شارژر '+n:'قطع شارژر '+n;g.className='bt '+(en===0?'run':'cut');
});
 /* v1.43 scenario-5 live face: q/ep-|Δ MV| over half-pack, events, latched cycles + fl2 bits */
 if($('imbv')){const ibmx=p[114]==null?10:p[114],f2=d.fl2||0;
  $('imbv').textContent=t[25]==null?'':t[25];
  $('imbev').textContent=t[26]==null?'':t[26];
  $('imbmx').textContent='از '+ibmx;
  const f2s=(f2&2)?'قفل دائمی - باتری را تعویض کنید':(f2&1)?'اپیزود جاری':'سالم';
  $('imbst').textContent=f2s;$('imbst').className='tg '+((f2&2)?'r':(f2&1)?'y':'g');
  const b117=p[117]==null?1:p[117],bt=document.getElementById('ib117');
  if(bt&&document.activeElement!==bt)bt.checked=!!b117;
  if($('a117'))$('a117').textContent=b117?'الان روشن است: باتریِ قفل‌شده نه شارژ می‌شود و نه به بار برق می‌دهد.':'الان خاموش است: فقط شارژ قطع می‌شود و باتریِ قفل‌شده همچنان بار را تغذیه می‌کند (ریسکش با شماست).';}
 /* ‎v1.72 scenario-6 live face: t[28]‎=ماسک قفل باتری خراب، t[29]=بیشترین شارژ پیوسته (ثانیه) */
 if($('dbst')){const dm=t[28]==null?0:t[28],lim=p[125]==null?86400000:p[125];
  /* v1.76: t[29] = شارژ پیوستهٔ کانال ۱ و t[30] = کانال ۲ (ثانیه) — هر باتری ساعت خودش */
  const elc=k=>{const v=t[k];return v==null?'':(sms(v*1000)+(lim>0?(' از '+sms(lim)):' (مهلت خاموش است)'));};
  $('dbel1').textContent=elc(29);
  $('dbel2').textContent=elc(30);
  const ds=dm?('قفل دائمی — باتری '+((dm&3)===3?'هر دو کانال':(dm&1)?'کانال ۱':'کانال ۲')+' خراب است، تعویضش کنید'):'سالم';
  $('dbst').textContent=ds;$('dbst').className='tg '+(dm?'r':'g');
  const b127=p[127]==null?0:p[127],bt7=document.getElementById('db127');
  if(bt7&&document.activeElement!==bt7)bt7.checked=!!b127;
  if($('a127'))$('a127').textContent=b127?'الان روشن است: باتریِ محکوم از خروجی هم برداشته می‌شود — نه شارژ می‌گیرد و نه به بار برق می‌دهد.':'الان خاموش است: فقط شارژِ آن کانال قطع است و باتری تا وقتی ولتاژ دارد به بار برق می‌دهد.';}

 for(let id=0;id<20;id++){const a=$('a'+id);if(a&&!(d.q&(1<<id)))a.textContent=p[id]==null?'':(id===13||id===14)?pc(p[id]):p[id];}
 const fe=$('fspan');if(fe){const mn=p[7]==null?null:(p[7]>=3?p[7]:0),av=p[8]==null?null:(p[8]>=2?p[8]:0);
  fe.innerHTML=(mn==null||av==null)?'':'فیلتر فعال: median '+(p[7]>=3?p[7]+'×1ms':'خاموش (۱..۲)')+' + میانگین '+(p[8]>=2?p[8]+'×1ms':'خاموش (۱)')+' ≈ <b>'+((mn||0)+(av||0))+'ms</b> تاریخچه در کادانس ۱kHz — پنل هر ۱۰۰ms فریم TLM می‌گیرد؛ برای صاف‌شدنِ قابل‌مشاهده مجموع را بالای ~۲۰۰ms ببرید (در مود خودکار ≤۵۰).';}
 formulas(t,p);chart();mview(d);
 $('mb').classList.toggle('v',man);$('ka').innerHTML=man?(d.ka<1500?`پایش لینک فعال · <span class="n">keepalive ${d.ka} ms</span>`:'<b>keepalive متوقف است</b>'):'';}
async function poll(){const c=new AbortController(),k=setTimeout(()=>c.abort(),2000);try{const r=await fetch('/t',{cache:'no-store',signal:c.signal});const d=await r.json();clearTimeout(k);if(document.hidden){D=d;hist(d);}else draw(d);}catch(e){clearTimeout(k);document.body.classList.add('dn');$('lk').classList.remove('on');$('lt').textContent='ESP در دسترس نیست';}
 setTimeout(poll,300);}
/* ---------- ابزار بنچ (بخش 5.5 و 5.6 نسخه ۲؛ همه دستی، هیچ ضریبی خودکار ارسال نمی‌شود) ----------
 * Bench tools (spec 5.5 / 5.6 v2): manual only; no coefficient is ever sent automatically. */
const gv=id=>{const e=$(id);if(!e||e.value==='')return null;const v=+e.value;return isNaN(v)?null:v;};
const fa=n=>'۱۲'[n-1],r0=Math.round;
/* ذخیرهٔ خودکار ورودی‌ها در همین مرورگر (با بستن صفحه پاک نمی‌شوند) */
let BDS={};try{BDS=JSON.parse(localStorage.getItem('bd')||'{}');}catch(e){}
function bsave(){document.querySelectorAll('[data-s]').forEach(e=>{if(e.value==='')delete BDS[e.id];else BDS[e.id]=e.value;});try{localStorage.setItem('bd',JSON.stringify(BDS));}catch(e){}}
function bload(r){r.querySelectorAll('[data-s]').forEach(e=>{if(BDS[e.id]!=null)e.value=BDS[e.id];});}
/* ----- داده‌برداری بنچ (بخش 5.6 نسخه ۲): همهٔ مرحله‌ها در یک جدول؛ جلو رفتن فقط با دکمهٔ کاربر -----
 * هر مرحله: duty → صبر → پنجرهٔ /m (قبلش GET_PARAMS) → توقف روی ردیف فعال برای عدد مولتی‌متر → با دکمهٔ ثبت و مرحلهٔ بعد یک ردیف ۸۹ستونی در ESP.
 * Capture v2: set duty → settle → /m window (preceded by GET_PARAMS) → STOP on the active table row for the DMM → one 134-column row only on submit. */
/* [EN] v1.60 (user order: "SOLO means nothing - name them properly"): the
   wire/CSV tags are now BAT1 / BAT2 / BOTH and every place the user reads
   them shows the plain Persian name instead.
   [FA] نام سناریوها به «فقط باتری ۱ / فقط باتری ۲ / هر دو باتری» تغییر کرد؛
   برچسب داخل فایل CSV هم ‎BAT1/BAT2/BOTH‎ است. */
const WSC={BAT1:[1],BAT2:[2],BOTH:[1,2]};
const WSN={BAT1:'فقط باتری ۱',BAT2:'فقط باتری ۲',BOTH:'هر دو باتری'};let W={run:false,abort:false,act:null};
const sl=ms=>new Promise(r=>setTimeout(r,ms));
async function req(u,m,body){const o={method:m||'GET',cache:'no-store'};if(body!=null){o.body=body;o.headers={'Content-Type':'text/plain'};}const r=await fetch(u,o);let j={};try{j=await r.json();}catch(e){}j._s=r.status;return j;}
function wst(m,c){const e=$('wS0');e.innerHTML=m;e.className='cm '+(c||'lb');}
/* ایمنی حین ثبت: لینک، Final fault، JIT، قطع ۱۵V، خاموش شدن ناخواستهٔ مود دستی */
function wchk(){if(W.abort)throw 'پایان توسط کاربر';const d=D;if(!d||d.on!=1)throw 'لینک STM32 قطع شد';
 (W.act||[]).forEach(n=>{const s=d.t[(n-1)*7+6];if(s==7)throw 'Final fault کانال '+fa(n);if(s==5)throw 'تریپ JIT کانال '+fa(n);if(d.t[n==1?18:17]>=15000)throw 'قطع ۱۵V کانال '+fa(n);});
 if(W.man&&!(d.fl&32))throw 'مود دستی قطع شد (deadman یا محافظ پنل)';}
/* نوشتن پارامتر و صبر تا گزارش همان مقدار از STM32 */
async function setv(id,v){for(let k=0;k<3;k++){const j=await req('/s?id='+id+'&v='+v,'POST');if(j._s!=200)throw 'پاسخ ESP: '+j._s;const e=Date.now()+2500;while(Date.now()<e){await sl(150);if(D&&D.p[id]===v)return;}}throw 'برد مقدار شناسهٔ '+id+' = '+v+' را گزارش نکرد';}
async function wrestore(o){wst('بازگردانی تنظیمات قبل از ثبت…');W.man=false;W.act=null;const L=[[16,0],[18,0],[19,o[19]],[11,o[11]],[12,o[12]]];if(o[19]===0)L.push([16,o[16]],[18,o[18]]);for(const [i,v] of L){try{await setv(i,v);}catch(e){}}}
async function wlog(txt){const j=await req('/benchlog/add','POST',txt);if(j._s==507)throw 'فایل پر است (حدود ۱۰۰KB). فایل را دانلود و پاک کنید.';if(j._s!=200)throw 'نوشتن در فایل انجام نشد (پاسخ '+j._s+')';wfs(j.size);return j;}
function wfs(sz){$('wF').innerHTML=sz==null?'':`<span class="n">${(sz/1024).toFixed(1)} / 100 KB</span>`;}
async function winfo(){try{const j=await req('/benchlog?i=1');if(j._s!=200)return null;wfs(j.size);if(!j.fs)$('wF').innerHTML='<b class="erc">فایل‌سیستم ESP در دسترس نیست</b>';return j;}catch(e){return null;}}
async function wclear(){if(W.run)return;if(!confirm('فایل ثبت بنچ کامل پاک شود؟ (اول آن را دانلود کنید)'))return;const j=await req('/benchlog/clear','POST');if(j._s==200){try{localStorage.removeItem('wrun');}catch(e){}wst('فایل پاک شد.','cm g');}else wst('پاک کردن انجام نشد.','cm r');winfo();}
const asc=s=>String(s||'').replace(/[^ -~]/g,'').replace(/,/g,';').trim().slice(0,80);
function wsweep(){const a=+($('wA').value),b=+($('wB').value);
 if(!(a>=0&&a<=50&&b>=0&&b<=50&&a<=b))throw 'بازه sweep نامعتبر است (از/تا 0..50 و از<=تا)';
 const L=[];for(let d=a;d<=b;d++)L.push(d);return L;}
function wdl(){const a=document.createElement('a');a.href='/benchlog';a.download='benchlog.csv';document.body.appendChild(a);a.click();a.remove();}
function wexist(sz){return new Promise(res=>{const e=$('wEx'),kb=(sz/1024).toFixed(1);
 $('wExB').innerHTML='<h3>فایل بنچ قبلی پیدا شد</h3><div class="lb">فایل داخل ESP از قبل <b class="n">'+kb+' KB</b> داده دارد. چه شود؟</div><button class="sb" id="wExD">دانلود قبلی و ادامه همان فایل</button><button class="sb sb2" id="wExC">پاک کردن و شروع تازه</button><button class="sb stp2" id="wExX">انصراف</button>';
 e.style.display='flex';
 const done=v=>{e.style.display='none';res(v);};
 $('wExD').onclick=()=>done('append');$('wExC').onclick=()=>done('clear');$('wExX').onclick=()=>done('cancel');});}
function wlist(){const a=$('wL').value.split(/[,، ]+/).filter(x=>x!=='').map(Number);if(!a.length||a.some(x=>!(x>=0&&x<=50)))throw 'فهرست duty نامعتبر است (درصد بین ۰ و ۵۰، با کاما جدا؛ مثلاً 5,10,15,20)';return a;}
/* پنجرهٔ /m: هر ۲۰ فیلد t[] با مجموع/کمینه/بیشینه/آخرین فریم، OR خطاها، seq و flags آخر.
 * باز شدن فرم مولتی‌متر پنجره را صفر می‌کند و همان لحظهٔ زدن «ثبت» خوانده می‌شود — آمار مال لحظهٔ عدد دادن شماست، نه قبلش (دستور کاربر ۲۰۲۶-۰۹-۲۵).
 * Opening the DMM form resets the window; the submit press reads it: the stats belong to the moment you press, not before. */
async function wopen(){const j=await req('/m','POST');if(j._s!=200)throw 'پنجرهٔ آمار ESP پاسخ نداد';W.winMs=Date.now();}
async function wlatch(){const j=await req('/m');if(!j.n||!j.s||j.s.length<25)throw 'در این بازه TLM نرسید';j.a=i=>j.s[i]/j.n;return j;}
/* خانه‌های زندهٔ ردیف فعال از آخرین /t — فقط نمایش؛ ردیف فایل از /m لحظهٔ ثبت ساخته می‌شود */
/* [EN] v1.65 (user order: "during the test do not show me the extra stuff -
   only the duty, the voltages and currents the board reads, and the numbers
   I have to type; keep everything else behind the scenes"). The table now
   shows exactly those columns. The CSV behind it is unchanged: raw counts,
   filtered and estimated currents, min/max of every field, flags and the
   whole settings block still go to the file.
   [FA] جدول فقط duty، ولتاژها و جریان‌هایی که برد می‌خواند و عددهایی که شما
   وارد می‌کنید را نشان می‌دهد. فایل CSV پشت صحنه بدون تغییر همه‌چیز را
   ثبت می‌کند: شمارش خام، جریان فیلترشده و تخمینی، کمینه/بیشینه و تنظیمات. */
function wlive(act){
 if(!D||D.on!=1)return['-','-','-','-','-'];
 const cur=n=>act.includes(n)?D.t[n===1?3:10]:'قطع';
 return [v2(D.t[14]),v2(D.t[18]),v2(D.t[17]),cur(1),cur(2)];}
function wmeas(m,act){
 const cur=n=>act.includes(n)?r0(m.a(n===1?3:10)):'قطع';
 return [v2(r0(m.a(14))),v2(r0(m.a(18))),v2(r0(m.a(17))),cur(1),cur(2)];}

/* ردیف CSV (۱۴۹ ستون، ترتیب دقیق بخش 5.6، مولتی‌متر نسخه ۴).
   v1.25 رفع باگ: شمارندهٔ پارامترها روی ۹۹ (تعداد v1.22) جا مانده بود در حالی که
   تعداد واقعی ۹۳ است، پس هر ردیف ۶ ستون اضافه می‌نوشت و همهٔ ستون‌های بعد از بلوک
   پارامتر زیر عنوان اشتباه می‌افتادند. حالا از PN که از تعداد واقعی می‌آید استفاده
   می‌شود تا دوباره کهنه نشود. v1.25: +۵ ستون شمارش خام برای کالیبراسیون.
   CSV row, exact 5.6 column order (DMM v4). v1.25 BUGFIX: the parameter loop was
   stuck at 99 (the v1.22 count) while the real count is 93, so every row wrote 6
   extra columns and everything after the parameter block landed under the wrong
   heading. It now derives the bound so it cannot go stale again. */
const PN=132;
/* v1.26 (دستور کاربر ۲۰۲۶-۰۹-۲۹): ۹۳ ستون از ۱۴۹ ستونِ هر ردیف، «تنظیمات» بودند
   که در طول یک سوییپ اصلاً عوض نمی‌شوند — یعنی ۶۲٪ هر ردیف تکرار بی‌فایده. حالا
   تنظیمات یک‌بار به‌صورت خط «# settings:» نوشته می‌شود و ردیف‌ها فقط ۵۶ ستون
   متغیر دارند. با سقف ~۱۰۰ کیلوبایتی فایل، این یعنی ~۲٫۷ برابر نقطهٔ سوییپ بیشتر.
   نکتهٔ ایمنی: اگر وسط کار تنظیمی عوض شود نباید گم شود، پس امضای تنظیمات هر ردیف
   سنجیده می‌شود و در صورت تغییر، خط «# settings:» تازه پیش از آن ردیف نوشته
   می‌شود — پس فایل هنوز کامل است و هر ردیف می‌داند با چه تنظیماتی گرفته شده.
   v1.26: 93 of the 149 columns were SETTINGS that never change during a sweep -
   62% of every row repeated for nothing. They are now written once as a
   "# settings:" line and the rows carry only the 56 varying columns, which is
   ~2.7x more sweep points inside the ~100 KB file cap. If a setting DOES change
   mid-run it must not be lost, so the signature is checked per row and a fresh
   settings line is emitted before the row that differs. */
function wset(){const P=[];for(let k=0;k<PN;k++)P.push(D.p[k]==null?'-':D.p[k]);return P.join(',');}
let WSIG=null;
async function wsync(){const g=wset();if(g===WSIG)return;WSIG=g;
 await wlog('# settings: '+g+'\n');}
function wrow(sc,i,pm,se,sa,m,v,iso){const q=x=>x==null?'-':x;
 const C=b=>[m.a(b).toFixed(1),m.lo[b],m.hi[b],r0(m.a(b+1)),r0(m.a(b+2)),m.lo[b+2],m.hi[b+2],r0(m.a(b+3)),m.lo[b+3],m.hi[b+3],r0(m.a(b+4)),m.lo[b+4],m.hi[b+4],m.la[b+5],m.la[b+6]];
 return [sc,i+1,pm,se,sa,iso,...C(0),...C(7),m.seq,m.fl,...[14,15,16,17,18].map(k=>r0(m.a(k))),m.or,...[20,21,22,23,24].map(k=>m.la[k]),q(v.ii),q(v.vi),q(v.b1),q(v.v1),q(v.b2),q(v.v2),v.note||'-'].join(',')+'\n';}
/* جدول واحد: هر مرحلهٔ هر سناریو یک ردیف؛ ردیف فعال ورودی‌ها و دکمه‌ها را دارد */
const WH=['سناریو','#','duty ٪',
 'ورودی V (برد)','باتری ۱V (برد)','باتری ۲V (برد)','جریان ۱mA (برد)','جریان ۲mA (برد)',
 'جریان ۱mA (شما)','جریان ۲mA (شما)','جریان ورودی mA (شما)','ورودی V (شما)','باتری ۱V (شما)','باتری ۲V (شما)',
 'وضعیت'];
const WSTC=WH.length-1;   /* ستون وضعیت / status column */
function wbuild(SC,L){W.K=[];SC.forEach(sc=>L.forEach((d,i)=>W.K.push({sc,i,d})));
 $('wT').innerHTML=`<div class="tw"><table class="bt2 wt"><tr>${WH.map(h=>`<th>${h}</th>`).join('')}</tr>${W.K.map((k,x)=>`<tr id="wr${x}"><td>${WSN[k.sc]||k.sc}</td><td>${k.i+1}</td><td>${k.d}</td>${'<td>·</td>'.repeat(WH.length-4)}<td class="lb">در صف</td></tr>`).join('')}</table></div>`;}
function wcell(x,A,st,cl){const r=$('wr'+x);if(!r)return;const c=r.children;A.forEach((v,i)=>{if(v!==undefined)c[3+i].innerHTML=v;});if(st!=null){c[WSTC].textContent=st;c[WSTC].className=cl||'lb';}}
/* اسکرول خودکار فقط داخل کادر جدول (v1.17b: خود صفحه تکان نمی‌خورد) / ‎auto-scroll inside the table box only (the page never jumps)‎ */
function wsee(x){const tw=$('wT').firstChild,r=$('wr'+x);if(!tw||!r||!tw.getBoundingClientRect)return;const rt=r.getBoundingClientRect(),tt=tw.getBoundingClientRect();if(rt.top<tt.top-2)tw.scrollTop-=(tt.top-rt.top);else if(rt.bottom>tt.bottom+2)tw.scrollTop+=(rt.bottom-tt.bottom);}
/* فرم ورود عدد (v1.17b، مولتی‌متر نسخه ۴): بیرون جدول، بالای آن — ورود داده دیگر اسکرول افقی نمی‌خواهد و جدول فقط برای مرور می‌ماند
 * Entry form (v1.17b, DMM v4): outside/above the table - data entry needs no horizontal scroll; the table stays for review */
function wform(x,act){const r=$('wr'+x);r.classList.add('wa');const k=W.K[x];
 wcell(x,wlive(act),'منتظر عدد شما','wr');
 const N=id=>`<input type="number" step="any" id="${id}">`;
 const L=(id,t,pre)=>`<label class="lb">${t} <input type="number" step="any" id="${id}"${pre!=null?' value="'+pre+'"':''}></label>`;
 const F=n=>act.includes(n)?`<label class="lb">جریان باتری ${n} mA ${N('wB'+n)}</label>`:'';
 let WVI=window.WVI||'';/* [EN] input voltage is quasi-static: carry the last submitted DMM reading into the next step (user order 2026-09-25: no need to retype it every step) / ولتاژ ورودی تقریباً ثابت است: آخرین عدد ثبت‌شده در مرحلهٔ بعد پیش‌پر می‌شود */
 const box=$('wF0');
 box.innerHTML=`<div class="bq"><div class="hd"><b>${WSN[k.sc]||k.sc} · مرحلهٔ ${k.i+1} · duty ${k.d}٪ — عددهای مولتی‌متر</b> <span class="lb">· Tab: خانهٔ بعدی · Enter: ثبت و رفتن به duty بعدی · Esc: بستن جدول و رفتن به ساخت جدول میکرو</span></div><div class="bctl">${F(1)}${F(2)}<label class="lb">جریان ورودی کل mA ${N('wIi')}</label>${L('wVi','ولتاژ ورودی V',WVI)}${L('wV1','ولتاژ باتری ۱V')}${L('wV2','ولتاژ باتری ۲V')}<label class="lb">یادداشت <input type="text" id="wN" class="dl" style="width:150px"></label><button class="sb" id="wGo">ثبت و مرحلهٔ بعد</button><button class="sb sb2" id="wRe">تکرار همین مرحله</button><button class="sb stp2" id="wEn">پایان</button></div><div class="lb">اجباری: جریان ورودی کل + جریان هر باتری روشن (<b>منفی هم مجاز</b> — تخلیهٔ باتری با شارژر خاموش، مثل بار زنر). جریان باتری باید نزدیک عدد پنل باشد؛ ورودی کل به ولتاژ/جریان باتری وابسته است (فرمول توان: ~۲٫۵ برابر در جریان کم تا ~۰٫۹ برابر در بالای بازه — مصرف ثابت برد در جریان کم برجسته می‌شود). ولتاژها (V) و یادداشت اختیاری.</div></div>`;
 const f=$('wB'+act[0]);if(f)f.focus();
 const lv=setInterval(()=>wcell(x,wlive(act)),400);
 return new Promise(res=>{$('wGo').onclick=()=>{const v={},ok=id=>gv(id);/* v1.9 (user order 2026-09-25): negative currents are VALID - with the charger off the battery itself discharges into other loads (e.g. the zener), the DMM then reads minus */
   v.ii=ok('wIi');if(v.ii==null)return alert('جریان ورودی کل اجباری است.');
   for(const n of act){v['b'+n]=ok('wB'+n);if(v['b'+n]==null)return alert('جریان باتری '+n+' اجباری است (کانال '+n+' روشن است).');}
   [['vi','wVi'],['v1','wV1'],['v2','wV2']].forEach(k=>{const y=gv(k[1]);v[k[0]]=y==null?null:r0(y*1000);});WVI=window.WVI=(v.vi==null)?'':(v.vi/1000);v.note=asc($('wN').value);res({a:'next',v,iso:new Date().toISOString()});};
  $('wRe').onclick=()=>res({a:'repeat'});$('wEn').onclick=()=>res({a:'end'});
   /* [EN] v1.65 (user order): Tab walks the boxes of the row being filled (the
     browser already does that, the boxes are simply in reading order), Enter
     submits and moves to the next duty step, Escape closes the table and
     goes on to the "build the firmware table" step. The same sentence is
     printed above the boxes so nobody has to be told twice.
     [FA] Tab بین خانه‌های همین ردیف، Enter ثبت و رفتن به duty بعدی، Esc
     بستن جدول و رفتن به مرحلهٔ ساخت جدول میکرو. همین راهنما بالای کادرها
     هم نوشته شده است. */
  box.onkeydown=ev=>{
   if(ev.key=='Enter'&&ev.target.tagName=='INPUT'){ev.preventDefault();$('wGo').click();return;}
   if(ev.key=='Escape'){ev.preventDefault();$('wEn').click();}};
  W.esc=ev=>{if(ev.key=='Escape'){ev.preventDefault();$('wEn').click();}};
  document.addEventListener('keydown',W.esc);
  W.ft=setInterval(()=>{try{wchk();}catch(er){clearInterval(W.ft);res({a:'err',e:er});}},200);}).finally(()=>{clearInterval(W.ft);clearInterval(lv);if(W.esc){document.removeEventListener('keydown',W.esc);W.esc=null;}box.innerHTML='';box.onkeydown=null;r.classList.remove('wa');});}
async function wStart(){if(W.run)return;if(!D||D.on!=1)return alert('لینک STM32 برقرار نیست.');let L;
 try{try{localStorage.setItem('wsw',$('wSw').checked?'1':'0');}catch(e){}L=$('wSw').checked?wsweep():wlist();}catch(e){return alert(e);}
 const SC=Object.keys(WSC).filter(k=>$('wc'+k).checked);if(!SC.length)return alert('حداقل یک سناریو را انتخاب کنید.');
 const o={};[11,12,16,18,19].forEach(i=>o[i]=D.p[i]);if(Object.values(o).some(v=>v==null))return alert('پارامترها هنوز از STM32 خوانده نشده‌اند.');
 if(D.p[15]===1||D.p[17]===1)return alert('مود duty فیکس (۱۵/۱۷) روشن است؛ اول خاموشش کنید.');
 const fi=await winfo();if(!fi||!fi.fs)return alert('فایل‌سیستم ESP در دسترس نیست؛ در Arduino IDE چیدمان فلش دارای FS را انتخاب و دوباره فلش کنید.');
 let wSkipC=false;if(fi.size>0){const wC=await wexist(fi.size);if(wC=='cancel')return;wSkipC=true;
 if(wC=='clear'){const wJ=await req('/benchlog/clear','POST');if(wJ._s!=200)return alert('پاک کردن فایل انجام نشد.');try{localStorage.removeItem('wrun');}catch(e){}await winfo();}else{wdl();}}
 if(!wSkipC&&!confirm('داده‌برداری شروع شود؟ '+SC.join('، ')+'\nپنل duty هر مرحله را می‌گذارد و منتظر عدد مولتی‌متر شما می‌ماند. آخر هر سناریو تنظیمات قبلی برمی‌گردد.'))return;
 W={run:true,abort:false,act:null,man:false};document.body.classList.add('br');wbuild(SC,L);$('wDone').classList.remove('v');let err=null,x=0;
 try{for(const sc of SC){const act=WSC[sc];
   let run=1;try{run=(+localStorage.getItem('wrun')||0)+1;localStorage.setItem('wrun',run);}catch(e){}
   wst(WSN[sc]+': آماده‌سازی (duty صفر، قطع/وصل کانال‌ها، مود دستی)…');W.act=null;W.man=false;
   await setv(16,0);await setv(18,0);for(const n of [1,2])await setv(10+n,act.includes(n)?1:0);
   await setv(19,1);{const e=Date.now()+3000;while(!(D.fl&32)){if(Date.now()>e)throw 'مود دستی روشن نشد';await sl(100);}}W.man=true;W.act=act;
   await wlog(`# run ${run} browser_ts=${new Date().toISOString()} scenario=${sc} duty_list=${L.join(';')}\n`);
   WSIG=null;await wsync();
   try{for(let i=0;i<L.length;){const pm=r0(L[i]*10),lb=sc+' · مرحلهٔ '+(i+1)+' از '+L.length+' · duty '+L[i]+'% ('+Math.round((i+1)/L.length*100)+'%)';
     wcell(x,[],'در حال اندازه‌گیری','wr');wsee(x);
     for(const n of act){const c=D.p[12+n],v=Math.min(pm,c==null?500:c,500);await setv(14+2*n,v);}
     await wopen();
     wst(lb+': عددهای مولتی‌متر را در ردیف رنگی جدول بنویسید','cm wr');const f=await wform(x,act);
     if(f.a=='err')throw f.e;if(f.a=='end'){W.abort=true;throw 'پایان توسط کاربر';}if(f.a=='repeat'){wcell(x,Array(WH.length-3).fill('·'),'تکرار');continue;}
     const m=await wlatch();
     await wsync();  /* اگر تنظیمی عوض شده، پیش از این ردیف ثبتش کن */
     await wlog(wrow(sc,i,pm,0,r0(Date.now()-W.winMs),m,f.v,f.iso));
     calpush(m,f.v,act,sc,L[i]);
     const mv=x=>x==null?'-':v2(x);
     const A=wmeas(m,act).concat([f.v.b1??'-',f.v.b2??'-',f.v.ii??'-',
      mv(f.v.vi),mv(f.v.v1),mv(f.v.v2)]);
     wcell(x,A,'ثبت شد','okc');
     i++;x++;}}
   finally{await wrestore(o);}}}
 catch(e){err=e;}
 W.K.forEach((k,y)=>{const c=$('wr'+y);if(c&&c.children[WSTC].textContent!='ثبت شد')wcell(y,[],'ثبت نشد');});
 W.run=false;W.act=null;W.man=false;document.body.classList.remove('br');
 if(err&&err!=='پایان توسط کاربر')wst('متوقف شد: '+err+' · تنظیمات قبلی برگشت. ردیف‌های ثبت‌شده در فایل مانده‌اند.','cm r');else wst(err?'با دکمهٔ پایان تمام شد؛ تنظیمات قبلی برگشت.':'همهٔ مرحله‌ها ثبت شد؛ تنظیمات قبلی برگشت.','cm g');
 $('wDone').classList.add('v');winfo();
 /* [EN] v1.65: the table is finished, so put the next step in front of the
    user instead of leaving him to find it. [FA] پس از پایان جدول، مرحلهٔ
    بعد (ساخت جدول میکرو) جلوی چشم کاربر می‌آید. */
 caln();calsmp();calchk();
 /* [EN] No auto-scroll on purpose: a jumping page during bench work was a
    reported bug (v1.17b), so the next step is announced in place.
    [FA] عمداً صفحه را جابه‌جا نمی‌کنیم؛ پرش صفحه قبلاً باگ گزارش‌شده بود. */
 stxt('calst','جدول بسته شد. مرحلهٔ بعد پایین همین صفحه است: «محاسبه از نمونه‌ها» و بعد «ساخت کد برای میکرو».');}

/* ==================== Bench Calibration / کالیبراسیون از جدول بنچ ====================
   [EN] v1.57 (user order: "take the bench capture straight onto the board with
   the user's confirmation, so I do not have to hand the file to an AI every
   time"). Every recorded wizard step now also lands in a small sample list,
   and this block turns that list into the SIX calibration numbers the board
   actually stores:

     id 0 / id 1  current offset ch1 / ch2 (ADC counts at zero current)
     id 2 / id 3  current gain   ch1 / ch2 (per-mille)
     id 4 / 5 / 6 input, 24V pack and 12V node offsets (mV)

   The board's own formula is  I_mA = (raw - offset) * 0.8777 * gain/1000, so a
   straight least-squares line  dmm = slope*raw + intercept  gives both numbers
   at once: gain = slope/0.8777*1000 and offset = -intercept/slope. The voltage
   offsets are simply the average of (multimeter - board), added to whatever
   offset the board is using now.

   Nothing is written silently: the panel shows the current value, the proposed
   value, how many points it used and how good the fit is, refuses obviously
   bad data, and before writing it downloads a full settings backup so one
   click can undo the whole thing.
   [FA] هر مرحلهٔ ثبت‌شدهٔ ویزارد یک نمونه هم اینجا ذخیره می‌کند. این بخش از
   روی همان نمونه‌ها با «کمترین مربعات» شش عدد کالیبراسیون را حساب می‌کند،
   مقدار فعلی و پیشنهادی و کیفیت برازش را نشان می‌دهد، دادهٔ بد را رد می‌کند و
   پیش از نوشتن روی برد، یک پشتیبان کامل دانلود می‌کند. */
var CALS=[];try{CALS=JSON.parse(localStorage.getItem('calsmp')||'[]')||[];}catch(e){CALS=[];}
function calsave(){try{localStorage.setItem('calsmp',JSON.stringify(CALS.slice(-400)));}catch(e){}}
function calpush(m,v,act,sc,duty){
 CALS.push({sc:sc,d:duty,use:1,r1:m.a(0),r2:m.a(7),vin:m.a(14),v24:m.a(15),v12:m.a(16),vlo:m.a(17),vhi:m.a(18),
  b1:act.includes(1)?v.b1:null,b2:act.includes(2)?v.b2:null,
  dvi:v.vi,dv1:v.v1,dv2:v.v2,ts:Date.now()});
 calsave();caln();calsmp();calchk();}
function calclr(){if(!confirm('همهٔ نمونه‌های جمع‌شده پاک شوند؟'))return;CALS=[];calsave();caln();calsmp();
 stxt('caltb','');stxt('calst','نمونه‌ها پاک شدند.');CALP=[];}
function caln(){stxt('caln',String(CALS.length));}
/* [‎EN] least squares y = a*x + b, plus R‎². [FA] برازش خطی + کیفیت. */
function calfit(xs,ys){const n=xs.length;let sx=0,sy=0,sxx=0,sxy=0;
 for(let i=0;i<n;i++){sx+=xs[i];sy+=ys[i];sxx+=xs[i]*xs[i];sxy+=xs[i]*ys[i];}
 const den=n*sxx-sx*sx;if(n<2||Math.abs(den)<1e-9)return null;
 const a=(n*sxy-sx*sy)/den,b=(sy-a*sx)/n,my=sy/n;
 let ss=0,sr=0,mx=0;for(let i=0;i<n;i++){const e=ys[i]-(a*xs[i]+b);sr+=e*e;ss+=(ys[i]-my)*(ys[i]-my);
  if(Math.abs(e)>mx)mx=Math.abs(e);}
 return {a,b,r2:ss>0?(1-sr/ss):1,n,mx,span:Math.max(...xs)-Math.min(...xs)};}
var CALP=[],CALR=[];
function calrun(){
 const cur=id=>(D&&D.p&&D.p[id]!=null)?D.p[id]:null;
 if(!D||!D.p){stxt('calst','⚠ هنوز داده‌ای از برد نرسیده — لینک برقرار نیست.');return;}
 if(calsel(null).length<3){stxt('calst','⚠ فقط '+calsel(null).length+' نمونهٔ تیک‌خورده هست؛ برای کالیبره حداقل ۳ مرحله لازم است.');return;}
 CALP=[];const rows=[];
 /* --- جریان دو کانال: شیب و آفست با هم از یک خط --- */
 [[1,'r1','b1',0,2],[2,'r2','b2',1,3]].forEach(ch=>{
  const S=calsel(ch[0]).filter(z=>Number.isFinite(z[ch[2]])&&Number.isFinite(z[ch[1]]));
  const f=calfit(S.map(z=>z[ch[1]]),S.map(z=>z[ch[2]]));
  const ttl='کانال '+fa(ch[0]);
  if(!f||f.a<=0){rows.push([ttl+' (جریان)','','','⛔ دادهٔ کافی/سالم برای این کانال نیست',0]);return;}
  let g=Math.round(f.a/K_MA*1000),o=Math.round(-f.b/f.a);
  const gc=Math.min(3000,Math.max(100,g)),oc=Math.min(255,Math.max(0,o));
  const w=[];
  if(f.n<4)w.push('فقط '+f.n+' نقطه');
  if(f.span<20)w.push('بازهٔ duty خیلی باریک است');
  if(f.r2<0.98)w.push('نقاط پراکنده‌اند (R²='+f.r2.toFixed(3)+')');
  if(f.mx>150)w.push('خطای باقیمانده تا '+Math.round(f.mx)+'mA');
  if(g!==gc||o!==oc)w.push('عدد خام خارج از بازهٔ مجاز بود و محدود شد');
  const bad=w.length>0&&(f.n<4||f.r2<0.9);
  rows.push([ttl+' — گین (‰)',ch[4],cur(ch[4]),gc,w.length?'⚠ '+w.join(' · '):'✅ برازش خوب ('+f.n+' نقطه، R²='+f.r2.toFixed(4)+')',bad?0:1]);
  rows.push([ttl+' — آفست (count)',ch[3],cur(ch[3]),oc,'از همان خط به‌دست آمد (جریان صفر در raw='+oc+')',bad?0:1]);});
 /* --- سه آفست ولتاژ: میانگین اختلاف مولتی‌متر با برد --- */
 const voff=(ttl,id,get)=>{const d=[];calsel(null).forEach(z=>{const x=get(z);if(x!=null&&Number.isFinite(x))d.push(x);});
  if(d.length<2){rows.push([ttl,id,cur(id),cur(id),'⛔ عدد مولتی‌متر برای این ولتاژ ثبت نشده — می‌توانید دستی بنویسید',0]);return;}
  const mean=d.reduce((a,b)=>a+b,0)/d.length;
  let sd=0;d.forEach(x=>sd+=(x-mean)*(x-mean));sd=Math.sqrt(sd/d.length);
  const nv=Math.round(xclamp(id,(cur(id)||0)+mean));const w=[];
  if(d.length<3)w.push('فقط '+d.length+' نقطه');
  if(sd>120)w.push('پراکندگی اندازه‌ها بالاست (±'+Math.round(sd)+'mV)');
  if(Math.abs(mean)>2000)w.push('اختلاف '+Math.round(mean)+'mV غیرعادی بزرگ است — سیم مولتی‌متر را چک کنید');
  const bad=Math.abs(mean)>2000;
  rows.push([ttl,id,cur(id),nv,(w.length?'⚠ '+w.join(' · '):'✅ اختلاف میانگین '+Math.round(mean)+'mV')+' ('+d.length+' نقطه)',bad?0:1]);};
 voff('آفست ولتاژ ورودی (mV)',4,z=>(z.dvi!=null)?(z.dvi-z.vin):null);
 voff('آفست ولتاژ پک ۲۴V (mV)',5,z=>(z.dv1!=null&&z.dv2!=null)?((z.dv1+z.dv2)-z.v24):null);
 voff('آفست نود ۱۲V (mV)',6,z=>(z.dv2!=null)?(z.dv2-z.vlo):null);
 /* [EN] v1.58 (user order): nothing here is take-it-or-leave-it. Every
    proposed number is an input box the user can correct, the board's
    CURRENT number sits next to it, and a tick box decides whether that
    line is written at all. A line the fit refused is simply unticked -
    the user can still fix the number by hand and tick it.
    [FA] هیچ عددی اجباری نیست: هر پیشنهاد یک کادر قابل ویرایش است، عدد
    فعلی برد کنارش نوشته شده و تیک هر ردیف تعیین می‌کند نوشته بشود یا نه.
    ردیفی که برازشش مردود شده فقط تیکش برداشته می‌شود. */
 calchk();
 CALR=rows;
 stxt('caltb','<div class="tw"><table class="bt2"><tr><th>اعمال</th><th>عدد</th><th>الان روی برد</th><th>مقدار جدید (قابل ویرایش)</th><th>تفاوت</th><th>کیفیت</th></tr>'+
  rows.map((r,i)=>'<tr><td><input type="checkbox" id="calk'+i+'"'+(r[5]?' checked':'')+'></td>'+
   '<td>'+r[0]+'</td><td><b>'+(r[2]==null?'':r[2])+'</b></td>'+
   '<td><input type="number" style="width:110px" id="calv'+i+'" value="'+r[3]+'" oninput="caldiff('+i+')"></td>'+
   '<td id="cald'+i+'" class="lb"></td><td>'+r[4]+'</td></tr>').join('')+'</table></div>');
 rows.forEach((r,i)=>caldiff(i));
 const n=rows.filter(r=>r[5]).length;
 stxt('calst',(n?'✅ ':'⚠ ')+n+' عدد از '+rows.length+' تیک‌خورده است (از '+calsel(null).length+' نمونهٔ انتخاب‌شده). '+
  'هر عدد را می‌توانید دستی اصلاح کنید یا تیکش را بردارید؛ پیش از نوشتن یک پشتیبان کامل دانلود می‌شود.');}
/* [EN] Show how far the (possibly hand-edited) number is from the board's
   current one. [FA] فاصلهٔ عدد جدید با عدد فعلی برد. */
function caldiff(i){const r=CALR[i];if(!r)return;const e=$('calv'+i),d=$('cald'+i);if(!e||!d)return;
 const nv=+e.value,cu=r[2];
 if(!Number.isFinite(nv)||cu==null){d.textContent='—';return;}
 const cl=xclamp(r[1],nv);
 d.innerHTML=(nv>cu?'+':'')+(nv-cu)+(cl!==nv?' <b class="erc">خارج از بازهٔ مجاز — به '+cl+' محدود می‌شود</b>':'');}
/* [EN] v1.59 (user order: "these calibration numbers must be saveable and
   restorable so I do not have to repeat the whole test every time"). Two
   things are saved now: the FINISHED numbers already travel in the settings
   backup (ids 0..14 are part of XIDS), and the RAW bench samples get their
   own file here. With the samples back you can refit, change a number by
   hand and re-apply onto another board without touching the hardware again.
   [FA] دو چیز ذخیره می‌شود: عددهای نهایی کالیبراسیون از قبل داخل همان فایل
   پشتیبان تنظیمات هستند (شناسه‌های ۰ تا ۱۴)، و نمونه‌های خام بنچ هم فایل
   مخصوص خودشان را گرفتند. با برگرداندن نمونه‌ها می‌شود دوباره محاسبه کرد،
   عددی را دستی عوض کرد و روی برد دیگری نشاند، بدون تکرار کل تست. */
/* [EN] v1.60 (user order: "can it tell whether a row belongs to charger 1 or
   2, and can the user pick which rows are used?"). Every sample now carries
   the scenario it came from - only battery 1, only battery 2, or both - and
   its duty step, and this list lets the user untick any row before fitting.
   An unticked row stays in the file; it is simply not used in the maths.
   [FA] هر نمونه می‌داند از کدام سناریو آمده (فقط باتری ۱ / فقط باتری ۲ /
   هر دو) و duty آن چند بوده. با تیک هر ردیف می‌توانید آن را از محاسبه
   بیرون بگذارید؛ ردیف بیرون‌گذاشته‌شده پاک نمی‌شود، فقط در حساب نمی‌آید. */
function calsmp(){const b=$('calsl');if(!b)return;
 if(!CALS.length){b.innerHTML='<span class="lb">هنوز نمونه‌ای ثبت نشده است.</span>';return;}
 b.innerHTML='<div class="tw"><table class="bt2"><tr><th>استفاده</th><th>#</th><th>سناریو</th><th>duty ٪</th>'+
  '<th>raw باتری ۱</th><th>جریان مولتی‌متر ۱mA</th><th>raw باتری ۲</th><th>جریان مولتی‌متر ۲mA</th><th>ولتاژ ورودی mV</th></tr>'+
  CALS.map((z,i)=>'<tr><td><input type="checkbox" id="calu'+i+'"'+(z.use===0?'':' checked')+' onchange="caluse('+i+',this.checked)"></td>'+
   '<td>'+(i+1)+'</td><td>'+(WSN[z.sc]||z.sc||'')+'</td><td>'+(z.d==null?'':z.d)+'</td>'+
   '<td>'+(z.r1==null?'':Math.round(z.r1))+'</td><td>'+(z.b1==null?'':Math.round(z.b1))+'</td>'+
   '<td>'+(z.r2==null?'':Math.round(z.r2))+'</td><td>'+(z.b2==null?'':Math.round(z.b2))+'</td>'+
   '<td>'+(z.dvi==null?'':z.dvi)+'</td></tr>').join('')+'</table></div>';}
function caluse(i,on){if(CALS[i]){CALS[i].use=on?1:0;calsave();}}
function calpick(on){CALS.forEach(z=>{z.use=on?1:0;});calsave();calsmp();}
/* [EN] Only ticked rows, and only rows whose scenario really drove that
   channel. [FA] فقط ردیف‌های تیک‌خورده، و فقط ردیف‌هایی که همان کانال در
   آن سناریو واقعاً کار می‌کرده. */
function calsel(n){return CALS.filter(z=>z.use!==0&&(n==null||z.sc==null||WSC[z.sc]==null||WSC[z.sc].indexOf(n)>=0));}
/* ==================== Voltage slope / شیب ولتاژ ====================
   [EN] v1.62 (user question: "how do we know the battery voltage is read
   right? shouldn't that have a table too?"). It must NOT have a table: a
   resistive divider is a straight line, so two numbers describe it fully -
   a slope (the divider ratio) and an offset. The board today lets the panel
   tune the OFFSET only (ids 4/5/6); the slope lives in the divider resistor
   constants in bsp_measurement.c. So the honest thing is to MEASURE the
   slope from the same bench samples and say it out loud: if it is 1.000 the
   offset is enough, if it is not, an offset can never fix it (the error
   grows with voltage) and the divider constant itself has to be corrected
   in the firmware - which the code generator now prints.
   [FA] ولتاژ جدول نمی‌خواهد چون مقسم مقاومتی یک خط صاف است: فقط «شیب»
   (نسبت مقسم) و «آفست». برد فقط آفست را قابل تنظیم کرده؛ شیب داخل
   مقدار مقاومت‌هاست. پس شیب را از همین نمونه‌ها اندازه می‌گیریم و صریح
   می‌گوییم: اگر ۱٫۰۰۰ بود آفست کافی است، اگر نبود آفست هرگز درستش
   نمی‌کند (خطا با ولتاژ بزرگ می‌شود) و باید مقاومت مقسم در فرم‌ور اصلاح
   شود - که تولیدکنندهٔ کد همان را چاپ می‌کند. */
/* [EN] v1.62b self-audit finding: the input rail (R46) and the pack rail
   (R47) are two DIFFERENT resistors that today both alias the same
   constant. Printing one #define for both would have made the generator
   emit two conflicting lines for the same macro - so each rail gets its
   own per-rail macro, and if the two measured slopes disagree the output
   says the alias has to be split first.
   [FA] یافتهٔ ممیزی خودم: ریل ورودی (R46) و ریل پک (R47) دو مقاومت جدا
   هستند ولی امروز هر دو به یک ثابت اشاره می‌کنند. پس هر ریل ماکروی خودش
   را می‌گیرد و اگر دو شیب با هم نخوانند، خروجی می‌گوید اول باید این
   اشتراک شکسته شود. */
const VDIV=[['ولتاژ ورودی',4,'BSP_MEASUREMENT_DIV24_TOP_OHMS',68000,6800,z=>z.dvi,z=>z.vin],
            ['ولتاژ پک ۲۴V',5,'BSP_MEASUREMENT_DIV24BAT_TOP_OHMS',68000,6800,
             z=>(z.dv1!=null&&z.dv2!=null)?(z.dv1+z.dv2):null,z=>z.v24],
            ['نود ۱۲V',6,'BSP_MEASUREMENT_DIV12_TOP_OHMS',34398,6800,z=>z.dv2,z=>z.vlo]];
function calvfit(k){const xs=[],ys=[];
 calsel(null).forEach(z=>{const y=k[5](z),x=k[6](z);
  if(y!=null&&Number.isFinite(y)&&x!=null&&Number.isFinite(x)){xs.push(x);ys.push(y);}});
 if(xs.length<3||(Math.max(...xs)-Math.min(...xs))<1000)return null;   /* شیب بدون بازهٔ ولتاژ معنا ندارد */
 return calfit(xs,ys);}

/* ==================== Rules shown to the user / شرط‌ها روی خود صفحه ====================
   [EN] v1.61 (user order: "put your conditions in the panel and TELL the user
   when he broke one"). Until now the quality rules lived inside calrun() and
   only produced a short warning next to a number. They are listed here, each
   with the plain sentence the user sees, and the list is rendered as a
   check-list: green when the data satisfies it, red with the reason when it
   does not. Nothing is hidden in code comments any more.
   [FA] شرط‌ها تا حالا داخل کد بودند و فقط یک هشدار کوتاه می‌دادند. حالا
   فهرست‌شان روی خود صفحه است: سبز یعنی داده‌ات این شرط را دارد، قرمز یعنی
   ندارد و دقیقاً می‌گوید چرا و چه‌کار کنی. */
function calchk(){
 const box=$('calck');if(!box)return;
 const R=[],ok=(t)=>R.push([1,t]),no=(t,f)=>R.push([0,t,f]);
 const S=calsel(null);
 /* ۱) لینک */
 if(D&&D.p)ok('ارتباط با برد برقرار است و عددهای فعلی خوانده شدند.');
 else no('ارتباط با برد برقرار نیست.','تا لینک وصل نشود نه مقدار فعلی دیده می‌شود نه چیزی نوشتنی است.');
 /* ۲) تعداد نمونه به تفکیک باتری */
 [1,2].forEach(n=>{const k=n===1?'b1':'b2',r=n===1?'r1':'r2';
  const c=calsel(n).filter(z=>Number.isFinite(z[k])&&Number.isFinite(z[r]));
  if(c.length>=4)ok('باتری '+fa(n)+': '+c.length+' نمونهٔ کامل دارد (حداقل ۴ لازم است).');
  else no('باتری '+fa(n)+': فقط '+c.length+' نمونهٔ کامل دارد.',
   'حداقل ۴ مرحله با «فقط باتری '+fa(n)+'» یا «هر دو باتری» بگیرید و جریان مولتی‌متر همان باتری را وارد کنید.');
  /* ۳) پخش‌بودن نقاط */
  if(c.length>1){const xs=c.map(z=>z[r]),sp=Math.max(...xs)-Math.min(...xs);
   if(sp>=20)ok('باتری '+fa(n)+': نقاط به اندازهٔ کافی پخش‌اند (بازهٔ raw برابر '+Math.round(sp)+').');
   else no('باتری '+fa(n)+': همهٔ نقاط تقریباً روی یک duty هستند (بازهٔ raw فقط '+Math.round(sp)+').',
    'از duty کم تا زیاد بروید (مثلاً ۲ تا ۲۰ درصد)؛ با نقاط چسبیده شیب قابل محاسبه نیست.');
   /* ۴) تکراری نبودن duty */
   const ds=c.map(z=>z.d).filter(x=>x!=null),u=new Set(ds);
   if(ds.length&&u.size<ds.length)no('باتری '+fa(n)+': '+(ds.length-u.size)+' مرحله با duty تکراری ثبت شده.',
    'تکراری‌ها را یا تیک بردارید یا نگه دارید؛ تکرار وزن آن نقطه را بی‌دلیل بالا می‌برد.');
   else ok('باتری '+fa(n)+': duty مرحله‌ها تکراری نیست.');}
 });
 /* ۵) عددهای ولتاژ */
 const nv=S.filter(z=>z.dvi!=null).length,nb=S.filter(z=>z.dv1!=null&&z.dv2!=null).length;
 if(nv>=3)ok('ولتاژ ورودی در '+nv+' مرحله با مولتی‌متر ثبت شده.');
 else no('ولتاژ ورودی فقط در '+nv+' مرحله ثبت شده.','بدون حداقل ۳ عدد، آفست ولتاژ ورودی محاسبه نمی‌شود.');
 if(nb>=3)ok('ولتاژ هر دو نیم‌باتری در '+nb+' مرحله ثبت شده.');
 else no('ولتاژ نیم‌باتری‌ها فقط در '+nb+' مرحله ثبت شده.','آفست پک ۲۴V و نود ۱۲V به عدد هر دو نیم‌باتری نیاز دارد.');
 /* ۶) قانونی‌بودن جدولی که ساخته می‌شود */
 [1,2].forEach(n=>{const t=calbuild(n,(D&&D.p&&D.p[n-1]!=null)?D.p[n-1]:0,(D&&D.p&&D.p[n+1]!=null)?D.p[n+1]:1000);
  if(t.bad)no('جدول باتری '+fa(n)+': '+t.bad+'.','جدول ساخته نمی‌شود تا چیز نادرستی وارد کد میکرو نشود.');
  else ok('جدول باتری '+fa(n)+': '+t.X.length+' نقطه، دو محور هم‌طول، جریان صعودی و توان بدون نزول.'+
   (t.note.length?' ('+t.note.join(' · ')+')':''));});
 /* ۷) شیب ولتاژ: آیا آفست تنهایی کافی است؟ */
 VDIV.forEach(k=>{const f=calvfit(k);
  if(!f){no(k[0]+': شیب قابل اندازه‌گیری نیست.',
   'برای سنجش شیب لازم است همین ولتاژ در چند مرحله با اختلاف حداقل ۱ ولت ثبت شود (مثلاً باتری خالی و پر).');return;}
  const err=Math.abs(f.a-1)*100;
  /* [EN] v1.63 (user decision): the dividers are 1% parts, so a scale error
     inside ~2% is just part tolerance and is deliberately ignored.
     [FA] مقاومت‌ها ۱٪ هستند، پس خطای ضریبی تا حدود ۲٪ تلرانس قطعه است و
     عمداً نادیده گرفته می‌شود. */
  if(err<2)ok(k[0]+': شیب '+f.a.toFixed(4)+' است ('+err.toFixed(1)+'٪) — در حد تلرانس ۱٪ مقاومت‌ها، کاری لازم نیست.');
  else no(k[0]+': شیب '+f.a.toFixed(4)+' است، یعنی '+err.toFixed(1)+'٪ خطای ضریبی.',
   'این بیشتر از تلرانس ۱٪ مقاومت‌هاست، پس احتمالاً قطعهٔ اشتباه یا اتصال بد است؛ آفست درستش نمی‌کند. در «ساخت کد برای میکرو» عدد اصلاح‌شدهٔ مقسم چاپ می‌شود.');});
 /* ۸) جریان منفی/صفر در همهٔ نقاط */
 if(S.some(z=>Number.isFinite(z.b1)&&z.b1>0)||S.some(z=>Number.isFinite(z.b2)&&z.b2>0))
  ok('حداقل در بعضی مرحله‌ها جریان واقعی شارژ ثبت شده.');
 else no('هیچ مرحله‌ای جریان شارژ مثبت ندارد.','با duty بالاتر یا باتری خالی‌تر تست کنید؛ از روی جریان صفر چیزی درنمی‌آید.');
 box.innerHTML='<table class="bt2"><tr><th>شرط</th><th>نتیجه</th></tr>'+
  R.map(r=>'<tr><td>'+(r[0]?'✅ ':'⛔ ')+r[1]+'</td><td class="lb">'+(r[0]?'':r[2])+'</td></tr>').join('')+'</table>';
 return R.filter(r=>!r[0]).length;}

/* ==================== Firmware snippet / خروجی برای کد میکرو ====================
   [EN] v1.61 (user order: "give me something I can paste into the firmware so
   a new board does not need this table pushed into it"). The board's own
   bench table is chain-current -> battery POWER, and measurement.c divides
   that power by the LIVE battery voltage to get current. So the generator
   rebuilds exactly that: for each accepted sample it computes the chain
   current the firmware itself would compute, and the battery power the
   multimeter actually proved (I_dmm x V_bat), sorts them, drops duplicates
   and prints the two C arrays plus the matching default offsets and gains.
   Paste it into Firmware/Modules/Measurement/calibration.h and every board
   flashed with that build already knows the table.
   [FA] جدول داخل برد «جریان زنجیره ← توان باتری» است و فرم‌ور توان را بر
   ولتاژ زندهٔ باتری تقسیم می‌کند تا جریان دربیاید. این خروجی دقیقاً همان را
   می‌سازد و دو آرایهٔ C به‌علاوهٔ آفست و گین پیش‌فرض را چاپ می‌کند تا داخل
   calibration.h کپی کنید. */
/* [EN] v1.64 (user order: "the panel must guarantee a battery's two axes
   come out the same length - the MCU must not spend time on it"). This
   builder is now the single place that proves a table is legal before it
   ever reaches the firmware, and it returns the proof instead of printing
   it in three different places:
     - both axes identical length (they are built as one list of pairs)
     - at least two points (the firmware extends the last segment's slope)
     - chain axis strictly increasing (the interpolation divides by the gap)
     - power axis NEVER decreasing. This one matters most: the firmware
       computes (yHigh - yLow) in UNSIGNED arithmetic, so one noisy point
       that dips would wrap around to a gigantic number instead of a small
       negative one. A dipping point is dropped here and reported.
   [FA] این سازنده تنها جایی است که قانونی‌بودن جدول را پیش از رسیدن به
   فرم‌ور ثابت می‌کند: هم‌طولی دو محور، حداقل دو نقطه، صعودی‌بودن محور
   جریان، و هرگز نزول‌نکردن محور توان - چون فرم‌ور تفریق را بدون علامت
   انجام می‌دهد و یک نقطهٔ نویزیِ نزولی به عددی غول‌پیکر تبدیل می‌شد. */
function calbuild(n,off,gn){
 const r=n===1?'r1':'r2',b=n===1?'b1':'b2',v=n===1?'dv1':'dv2',note=[];
 const pts=[];
 calsel(n).forEach(z=>{if(!Number.isFinite(z[r])||!Number.isFinite(z[b]))return;
  const vb=(z[v]!=null)?z[v]:(n===1?z.vhi:z.vlo);if(!vb)return;
  const chain=Math.max(0,Math.round((z[r]-off)*K_MA*gn/1000));
  pts.push([chain,Math.max(0,Math.round(z[b]*vb/1000))]);});
 pts.sort((a,c)=>a[0]-c[0]);
 const X=[],Y=[];let dup=0,dip=0;
 pts.forEach(q=>{
  if(X.length&&q[0]===X[X.length-1]){dup++;return;}      /* جریان تکراری */
  if(Y.length&&q[1]<Y[Y.length-1]){dip++;return;}        /* توان نزولی */
  X.push(q[0]);Y.push(q[1]);});
 if(X.length&&X[0]!==0){X.unshift(0);Y.unshift(0);}
 if(dup)note.push(dup+' نقطه با جریان تکراری کنار گذاشته شد');
 if(dip)note.push(dip+' نقطهٔ نویزی که توانش پایین‌تر از نقطهٔ قبل بود کنار گذاشته شد');
 let bad='';
 if(X.length!==Y.length)bad='دو محور هم‌طول نشدند';                 /* نباید رخ دهد */
 else if(X.length<2)bad='کمتر از ۲ نقطه ('+X.length+') — حداقل ۲ لازم است';
 else{for(let i=1;i<X.length;i++){if(X[i]<=X[i-1])bad='محور جریان صعودی نیست';
   if(Y[i]<Y[i-1])bad='محور توان نزول دارد';}}
 return {X:X,Y:Y,note:note,bad:bad};}
function calcode(){
 const get=id=>{const r=CALR.filter(x=>x[1]===id)[0];if(!r)return (D&&D.p&&D.p[id]!=null)?D.p[id]:0;
  const e=$('calv'+CALR.indexOf(r));return e?Math.round(+e.value):r[3];};
 const off=[get(0),get(1)],gn=[get(2),get(3)],msg=[];
 let out='/* [EN] Generated by the ChangeOver panel on '+new Date().toISOString()+
  '\n *      from '+calsel(null).length+' accepted bench samples. The panel has\n'+
  ' *      already checked: equal axis lengths, >= 2 points, increasing chain\n'+
  ' *      axis, non-decreasing power axis.\n'+
  ' *      Paste into Firmware/Modules/Measurement/calibration.h.\n'+
  ' * [FA] ساختهٔ پنل ChangeOver؛ هم‌طولی، حداقل دو نقطه، صعودی‌بودن محور\n'+
  ' *      جریان و نزول‌نکردن محور توان همین‌جا بررسی شده است. */\n\n';
 [1,2].forEach(n=>{const t=calbuild(n,off[n-1],gn[n-1]);
  t.note.forEach(x=>msg.push('باتری '+fa(n)+': '+x));
  if(t.bad){msg.push('⛔ باتری '+fa(n)+': '+t.bad);
   out+='/* table '+n+' - battery '+n+' NOT GENERATED: '+t.bad+' */\n\n';return;}
  if(t.X.length>24)msg.push('باتری '+fa(n)+': جدول '+t.X.length+' نقطه‌ای شد (هر نقطه ۸ بایت فلش)');
  out+='/* table '+n+' - battery '+n+', '+t.X.length+' points, axes equal length (any count is valid) */\n'+
   'static const uint32_t CAL_Current'+n+'LutChainMa[] =\n    { '+t.X.map(q=>q+'u').join(', ')+' };\n'+
   'static const uint32_t CAL_Current'+n+'LutBatteryMw[] =\n    { '+t.Y.map(q=>q+'u').join(', ')+' };\n\n';});
 {const f0=calvfit(VDIV[0]),f1=calvfit(VDIV[1]);
  if(f0&&f1&&Math.abs(f0.a-f1.a)>0.005)
   out+='/* WARNING: the input rail and the pack rail measure DIFFERENT slopes ('+
    f0.a.toFixed(4)+' vs '+f1.a.toFixed(4)+'). They currently share one constant in\n'+
    ' * bsp_measurement.c, so give each its own value before pasting the two lines below. */\n';}
 VDIV.forEach(k=>{const f=calvfit(k);if(!f)return;
  const nt=Math.round(f.a*k[3]+(f.a-1)*k[4]);
  out+='/* '+k[0]+': measured slope '+f.a.toFixed(4)+' ('+((f.a-1)*100).toFixed(2)+'% scale error)\n'+
   ' *   '+(Math.abs(f.a-1)<0.02?'within the 1% resistor tolerance - keep the constant as it is.':
    'the offset cannot fix a scale error - in bsp_measurement.c set\n *   #define '+k[2]+'  '+nt+'u   (was '+k[3]+'u)')+' */\n';});
 out+='/* defaults that belong WITH the tables above (esp_link.h / plink_params.h):\n'+
  ' *   current offset ch1 = '+off[0]+' counts, ch2 = '+off[1]+' counts\n'+
  ' *   current gain   ch1 = '+gn[0]+' permille, ch2 = '+gn[1]+' permille\n'+
  ' *   voltage offsets: input = '+get(4)+'mV, 24V pack = '+get(5)+'mV, 12V node = '+get(6)+'mV\n'+
  ' * A table fitted with one gain/offset pair is only valid with that pair. */\n';
 const t=$('calcd');if(t){t.value=out;t.style.display='block';}
 stxt('calst',(msg.length?('⚠ '+msg.join(' · ')+' — '):'')+
  'کد آماده است؛ متن زیر را کپی یا دانلود کنید. هم‌طولی و صعودی‌بودن جدول‌ها همین‌جا بررسی شد.');}
function calcdl(){const t=$('calcd');if(!t||!t.value)return;
 const u=URL.createObjectURL(new Blob([t.value],{type:'text/plain'}));
 const a=document.createElement('a');a.href=u;a.download='calibration_generated.h';a.click();
 setTimeout(()=>URL.revokeObjectURL(u),2000);}
function calcopy(){const t=$('calcd');if(!t||!t.value)return;t.select();
 try{document.execCommand('copy');stxt('calst','کد در کلیپ‌بورد کپی شد.');}catch(e){}}
function calexp(){if(!CALS.length){stxt('calst','نمونه‌ای برای ذخیره نیست.');return;}
 const o={app:'ChangeOver-bench-samples',v:1,build:xbuild(),n:CALS.length,
  saved:new Date().toISOString(),samples:CALS};
 const u=URL.createObjectURL(new Blob([JSON.stringify(o)],{type:'application/json'}));
 const a=document.createElement('a');a.href=u;
 a.download='changeover-bench-'+o.saved.slice(0,10)+'.json';a.click();
 setTimeout(()=>URL.revokeObjectURL(u),2000);
 stxt('calst','⬇ '+CALS.length+' نمونه در فایل ذخیره شد.');}
async function calimp(f){let o;try{o=JSON.parse(await f.text());}catch(e){stxt('calst','⚠ فایل JSON معتبر نیست');return;}
 if(!o||o.app!=='ChangeOver-bench-samples'||!Array.isArray(o.samples)){stxt('calst','⚠ این فایل نمونه‌های بنچ نیست');return;}
 const add=o.samples.filter(z=>z&&Number.isFinite(z.r1));
 if(!add.length){stxt('calst','⚠ نمونهٔ معتبری در فایل نیست');return;}
 const keep=CALS.length&&confirm('نمونه‌های فعلی ('+CALS.length+' تا) هم نگه داشته شوند؟\nلغو = فقط نمونه‌های فایل بماند.');
 CALS=keep?CALS.concat(add):add;calsave();caln();calsmp();
 stxt('calst','⬆ '+add.length+' نمونه بازخوانی شد (مجموع '+CALS.length+'). حالا «محاسبه» را بزنید.');
 const e=$('calf');if(e)e.value='';}
async function calapply(){
 /* [EN] v1.64 line-by-line audit finding: the bench wizard owns the board
    while it runs (manual mode, duty, channel enables). Writing calibration
    in the middle of it would change the numbers under a measurement that is
    already in progress and silently poison the row being recorded.
    [FA] تا وقتی ویزارد بنچ در حال اجراست برد در اختیار اوست؛ نوشتن وسط کار
    همان ردیفی را که دارد ثبت می‌شود خراب می‌کند. */
 if(W&&W.run){stxt('calst','⛔ داده‌برداری بنچ در جریان است؛ اول آن را تمام کنید.');return;}
 CALP=[];
 CALR.forEach((r,i)=>{const k=$('calk'+i),e=$('calv'+i);if(!k||!e||!k.checked)return;
  const nv=+e.value;if(!Number.isFinite(nv))return;CALP.push([r[1],xclamp(r[1],Math.round(nv)),r[0]]);});
 if(!CALP.length){stxt('calst','هیچ ردیفی تیک نخورده است — اول «محاسبه» را بزنید و ردیف‌های موردنظر را تیک بزنید.');return;}
 const txt=CALP.map(c=>c[2]+': '+((D&&D.p&&D.p[c[0]]!=null)?D.p[c[0]]:'?')+' → '+c[1]).join('\n');
 if(!confirm(CALP.length+' عدد کالیبراسیون روی برد نوشته شود؟\n\n'+txt+
  '\n\nیک فایل پشتیبان از تنظیمات فعلی دانلود می‌شود تا در صورت نیاز برگردانید.'))return;
 xexp();
 let ok=0;for(const c of CALP){try{const r=await fetch('/s?id='+c[0]+'&v='+c[1],{method:'POST'});if(r.ok)ok++;}catch(e){}
  stxt('calst','… '+ok+'/'+CALP.length);await sl(80);}
 await sl(800);
 const bad=CALP.filter(c=>D&&D.p&&D.p[c[0]]!=null&&+D.p[c[0]]!==+c[1]);
 stxt('calst',(ok===CALP.length?'✅ ':'⚠ ')+ok+'/'+CALP.length+' روی برد نوشته شد'+
  (bad.length?' · برد '+bad.length+' عدد را به بازهٔ خودش گیره زد':' · برد همه را عیناً پذیرفت')+
  ' · پشتیبان قبلی در فایل دانلودشده است.');}

/* ---------- v1.66: ارسال مستقیم جدول به حافظهٔ خود میکرو (دستور کاربر ۲۰۲۶-۱۰-۰۵) ----------
   [FA] «حتماً باید جدول را در کد میکرو بچسبانیم و بیلد کنیم؟» نه. برد از این
   نسخه یک بلوک فلش «جدا از بقیهٔ متغیرها» دارد (دو صفحهٔ ۱ کیلوبایتی،
   پینگ‌پنگ، CRC مخصوص خودش) که فقط جدول بنچ در آن می‌نشیند. مسیر قدیمی هم
   دست‌نخورده ماند: «ساخت کد برای میکرو» همچنان فایل calibration.h را می‌دهد.
   سه مرحله: چیدن (‎BEGIN+CHUNK)‎ ← کامیت با CRC32 ← دست‌دادن؛ و فقط بعد از
   دست‌دادنِ موفق، دکمهٔ ریست برد فعال می‌شود تا همهٔ ماژول‌ها با جدول نو
   شروع کنند.
   [EN] The push is content-addressed: the browser CRC32s the exact bytes it
   sends, the board CRC32s what it received, and the commit is refused unless
   they match. */
const LUTMAX=24;
const LUTST={0:'موفق',1:'برد مرحلهٔ شروع را ندیده بود',2:'تعداد نقاط برای برد نامعتبر بود',
 3:'یکی از تکه‌های جدول به برد نرسید',4:'محور جریان روی برد صعودی نبود',
 5:'محور توان روی برد افت داشت',6:'CRC برد با CRC پنل نخواند (داده در راه خراب شد)',
 7:'نوشتن روی فلش برد شکست خورد'};
function lcrc(b){let c=0xFFFFFFFF;for(let i=0;i<b.length;i++){c^=b[i];
 for(let k=0;k<8;k++)c=(c>>>1)^(0xEDB88320&-(c&1));}return (c^0xFFFFFFFF)>>>0;}
function lpack(){
 const get=id=>{const r=CALR.filter(x=>x[1]===id)[0];if(!r)return (D&&D.p&&D.p[id]!=null)?D.p[id]:0;
  const e=$('calv'+CALR.indexOf(r));return e?Math.round(+e.value):r[3];};
 const off=[get(0),get(1)],gn=[get(2),get(3)],msg=[],T=[];
 [1,2].forEach(n=>{const t=calbuild(n,off[n-1],gn[n-1]);
  t.note.forEach(x=>msg.push('باتری '+fa(n)+': '+x));
  if(t.bad){msg.push('⛔ باتری '+fa(n)+': '+t.bad+' — این کانال فرستاده نمی‌شود');T.push({X:[],Y:[]});return;}
  if(t.X.length>LUTMAX){msg.push('⛔ باتری '+fa(n)+': '+t.X.length+' نقطه از سقف '+fa(LUTMAX)+
   ' نقطهٔ حافظهٔ برد بیشتر است؛ این کانال فرستاده نمی‌شود (ساخت کد و بیلد همچنان کار می‌کند)');
   T.push({X:[],Y:[]});return;}
  T.push(t);});
 if(!T[0].X.length&&!T[1].X.length)return {bad:'هیچ کانالی جدول قابل‌ارسال ندارد',msg:msg};
 const by=[],cs=[T[0].X.length,T[1].X.length];
 T.forEach(t=>{by.push(t.X.length&255);
  for(let i=0;i<t.X.length;i++){[t.X[i],t.Y[i]].forEach(v=>{
   by.push(v&255,(v>>>8)&255,(v>>>16)&255,(v>>>24)&255);});}});
 T.forEach(t=>{for(let i=0;i<t.X.length;i++){cs.push(t.X[i],t.Y[i]);}});
 const crc=lcrc(by);cs.push(crc);
 return {T:T,crc:crc,body:cs.join(','),msg:msg};}
async function lsend(){
 if(W&&W.run){stxt('calst','⛔ داده‌برداری بنچ در جریان است؛ اول آن را تمام کنید.');return;}
 if(!D||D.on!=1){stxt('calst','⛔ لینک STM32 برقرار نیست؛ جدول فرستاده نمی‌شود.');return;}
 const p=lpack();
 if(p.bad){stxt('calst','⛔ '+p.bad+(p.msg.length?' · '+p.msg.join(' · '):''));return;}
 if(!confirm('جدول مستقیماً در حافظهٔ خود میکرو نوشته شود؟\n\n'+
  'باتری ۱: '+p.T[0].X.length+' نقطه · باتری ۲: '+p.T[1].X.length+' نقطه\n'+
  'محل ذخیره: بلوک فلش مخصوص جدول، جدا از بقیهٔ تنظیمات.\n'+
  'اگر داده درست نرسد، برد کامیت را رد می‌کند و جدول قبلی سر جایش می‌ماند.'))return;
 stxt('calst','… جدول در حال ارسال به برد');
 let r;try{r=await req('/lut','POST',p.body);}catch(e){stxt('calst','⚠ ارسال به ESP نرسید');return;}
 if(!r||r.ok!==1){stxt('calst','⚠ ESP جدول را نپذیرفت ('+((r&&r.e)||'?')+')');return;}
 /* ارسال گام‌به‌گام است: ESP هر فریم را فقط بعد از تأیید فریم قبلی می‌فرستد
    (تا حلقهٔ گیرندهٔ برد سرریز نکند)، پس تا ۱۰ ثانیه منتظر می‌مانیم و مرحله را
    به کاربر نشان می‌دهیم. */
 const LSTG=['','شروع','نقاط باتری ۱','نقاط باتری ۲','ثبت در فلش'];
 let a=null;
 for(let i=0;i<40;i++){await sl(250);
  try{a=await req('/lut','GET');}catch(e){a=null;continue;}
  if(a&&a.txe){break;}
  if(a&&a.tx){stxt('calst','… ارسال جدول: '+(LSTG[a.tx]||a.tx));continue;}
  if(a&&a.st===3)break;}
 if(a&&a.txe===1){stxt('calst','⚠ برد به مرحلهٔ ارسال پاسخ نداد (سیم یا نویز لینک)؛ '+
  'جدول قبلی بدون تغییر ماند — دوباره بزنید.');return;}
 if(a&&a.txe===2){stxt('calst','⛔ برد یکی از مرحله‌های ارسال را رد کرد: '+
  (LUTST[a.s]||('کد '+a.s))+' · جدول قبلی بدون تغییر ماند.');return;}
 if(!a||a.st!==3){stxt('calst','⚠ برد پاسخ کامیت را نداد؛ جدول قبلی بدون تغییر ماند.');return;}
 if(a.s!==0){stxt('calst','⛔ برد جدول را رد کرد: '+(LUTST[a.s]||('کد '+a.s))+
  ' · جدول قبلی بدون تغییر ماند.');return;}
 if(a.crc>>>0!==p.crc>>>0){stxt('calst','⛔ دست‌دادن نخواند (CRC برد '+a.crc+' ≠ CRC پنل '+p.crc+
  ') · جدول قبلی بدون تغییر ماند.');return;}
 stxt('calst','✅ جدول در فلش برد نوشته و دست‌دادن تأیید شد (باتری ۱: '+a.n1+' نقطه، باتری ۲: '+a.n2+
  ' نقطه، CRC '+a.crc+'). برای اینکه همهٔ ماژول‌ها با جدول جدید شروع کنند، «ریست برد» را بزنید.');
 if(confirm('جدول با موفقیت ذخیره شد.\n\nبرد همین حالا ریست شود تا همهٔ تنظیمات با جدول جدید بالا بیایند؟\n'+
  '(شارژ چند ثانیه قطع می‌شود؛ پارامترهای ذخیره‌شده دست‌نخورده برمی‌گردند.)'))await lrst();}
async function lrst(){
 let r;try{r=await req('/lut/reset','POST');}catch(e){stxt('calst','⚠ درخواست ریست به ESP نرسید');return;}
 if(!r||r.ok!==1){stxt('calst','⛔ ریست رد شد: اول باید یک ارسال موفق با دست‌دادن تأییدشده انجام شود.');return;}
 stxt('calst','… فرمان ریست فرستاده شد؛ برد چند ثانیهٔ دیگر با جدول جدید بالا می‌آید.');}

/* ---------- ساخت تب‌ها ---------- */
/* تب ۱: داده‌برداری بنچ */
$('p1').innerHTML=`<div class="cd"><div class="ds">هر مرحله: پنل duty را می‌گذارد، جدول همان ردیف را نشان می‌دهد و عدد مولتی‌متر را در فرم بالای جدول بنویس و <b>ثبت</b> کن — آمار همان لحظهٔ ثبت قفل می‌شود. <b>SOLO1</b>: کانال ۱ · <b>SOLO2</b>: کانال ۲ · <b>BOTH</b>: هر دو. آمپرمتر: یکی در تغذیهٔ کل برد + سری با سیم شارژ هر باتری روشن. هیچ ضریبی خودکار اعمال نمی‌شود.</div>
<div class="bctl"><label class="lb">duty % <input type="text" id="wL" data-s class="dl" value="2,4,6,8,10,12,14,16,18,20" style="width:160px"></label>
${Object.keys(WSC).map(k=>`<label class="lb"><input type="checkbox" id="wc${k}" checked> ${WSN[k]}</label>`).join('')}</div>
<div class="bctl"><label class="lb"><input type="checkbox" id="wSw" checked> sweep خودکار با گام ۱٪</label><label class="lb">از <input type="number" id="wA" data-s value="1" min="0" max="50"></label><label class="lb">تا <input type="number" id="wB" data-s value="50" min="0" max="50"></label><span class="lb">خاموش = فهرست دستی بالا</span></div>
<div class="movl" id="wEx" style="display:none"><div class="mod" id="wExB"></div></div>
<div class="bctl"><button class="sb brun" onclick="wStart()">شروع</button><button class="sb stp2 wstop" onclick="W.abort=true">پایان</button><span class="lb">فایل: <b id="wF"></b></span><a class="sb sb2 lnk" href="/benchlog" download="benchlog.csv">دانلود فایل</a><button class="sb sb2 brun" onclick="wclear()">پاک کردن فایل</button></div>
<div class="cm lb" id="wS0"></div><div id="wF0"></div><div id="wT"></div>
<div class="wn gb" id="wDone" style="background:rgba(52,211,153,.10);color:#a7f3d0"><b style="color:var(--ok)">فایل آماده است.</b> <a class="sb lnk" href="/benchlog" download="benchlog.csv">دانلود benchlog.csv</a> <button class="sb sb2" onclick="wclear()">پاک کردن فایل</button></div></div><div class="cd"><div class="hd"><b>کالیبراسیون خودکار از همین جدول</b><span class="lb">· نمونه‌های ثبت‌شده: <b id="caln">0</b> · عددها فقط با تأیید شما روی برد نوشته می‌شوند</span></div>
<div class="ds">هر مرحله‌ای که در ویزارد «ثبت» می‌کنید یک نمونه هم اینجا می‌ماند. «محاسبه» از روی همین نمونه‌ها گین و آفست جریان هر دو کانال و سه آفست ولتاژ را درمی‌آورد، مقدار فعلی برد را کنار پیشنهاد می‌گذارد و کیفیت هر برازش را می‌گوید. برای نتیجهٔ خوب حداقل ۴ مرحله با duty پخش‌شده (مثلاً ۲ تا ۲۰٪) بگیرید.</div>
<div class="bqr2"><button class="sb sb2" onclick="calrun()">محاسبه از نمونه‌ها</button><button class="sb brun" onclick="calapply()">اعمال روی برد (با تأیید)</button><button class="sb sb2" onclick="calexp()">⬇ ذخیرهٔ نمونه‌ها</button><label class="sb" style="cursor:pointer">⬆ بازخوانی نمونه‌ها<input type="file" id="calf" accept=".json,application/json" style="display:none" onchange="if(this.files[0])calimp(this.files[0])"></label><button class="sb stp2" onclick="calclr()">پاک کردن نمونه‌ها</button></div>
<div class="bqr2"><button class="sb" onclick="calpick(1)">انتخاب همه</button><button class="sb" onclick="calpick(0)">هیچ‌کدام</button></div><div id="calck" style="margin:6px 0"></div><div id="calsl" style="margin:6px 0"></div><div class="ds">دو راه برای رساندن جدول به میکرو هست و هر دو فعال‌اند: <b>۱) ارسال مستقیم</b> — جدول همین حالا در یک بلوک فلشِ مخصوص خودش روی برد نوشته می‌شود (جدا از بقیهٔ تنظیمات)، برد CRC آن را پس می‌فرستد و فقط در صورت تطابق پذیرفته می‌شود؛ بعد می‌توانید برد را ریست کنید تا همه چیز با جدول نو شروع کند. سقف این راه <b>۲۴ نقطه برای هر باتری</b> است. <b>۲) ساخت کد</b> — همان روش قبلی: فایل calibration.h ساخته می‌شود تا در پروژه بچسبانید و بیلد کنید (بدون محدودیت نقطه). اگر رکورد فلش خالی یا خراب باشد، برد خودبه‌خود به جدول کامپایل‌شده برمی‌گردد.</div><div class="bqr2"><button class="sb sb2" onclick="calcode()">ساخت کد برای میکرو</button><button class="sb" onclick="calcopy()">کپی کد</button><button class="sb" onclick="calcdl()">دانلود calibration_generated.h</button></div><div class="bqr2"><button class="sb brun" onclick="lsend()">⇪ ارسال مستقیم جدول به برد</button><button class="sb sb2" onclick="lrst()">↻ ریست برد (بعد از ارسال موفق)</button></div><textarea id="calcd" style="display:none;width:100%;height:220px;direction:ltr;font-family:monospace;font-size:12px" readonly></textarea><div class="cm lb" id="calst"></div><div id="caltb"></div></div>
`;
caln();calsmp();calchk();bload(document.body);try{$('wSw').checked=localStorage.getItem('wsw')!=='0';}catch(e){};$('wSw').onchange=()=>{const s=$('wSw').checked,L=$('wL'),A=$('wA'),B=$('wB');if(L)L.disabled=s;if(A)A.disabled=!s;if(B)B.disabled=!s;};$('wSw').onchange();document.body.addEventListener('input',bsave);document.body.addEventListener('change',bsave);winfo();
/* ---------- کنترل دستی duty دائمی (دستور کاربر ۲۰۲۶-۰۹-۲۵): کنترلها داخل کارت هر شارژر (از v1.16p)؛
 * ---------- قرارداد ایمنی بخش 5.2 اسپک بدون تغییر: deadman ۱۰ ثانیه، سقف کانال (‎p13/p14)‎،
 * ---------- JIT با شروع‌مجددِ شمارش (Arm در فرمور) با ارسال دوبارهٔ همان duty. هیچ ضریبی اینجا ارسال نمی‌شود. ---------- */
const manOn=()=>!!(D&&((D.fl&32)||D.p[19]===1));
async function qset(n){if(W.run)return alert('داده‌برداری ویزارد در جریان است؛ اول آن را تمام کنید.');
 const v=gv('qm'+n);if(v==null)return alert('عدد duty (٪) را وارد کنید.');if(!D||D.on!=1)return alert('لینک STM32 برقرار نیست.');
 const lim=(D.p[12+n]==null?500:D.p[12+n]),pm=Math.max(0,Math.min(lim,r0(v*10)));
 if(pm<r0(v*10))alert('duty به سقف کانال ('+(lim/10)+'٪) محدود شد.');
 const man=manOn(),fx=D&&D.p[13+2*n]===1;let go=man||fx;
 if(!go)go=confirm('مود دستی خاموش است؛ روشن شود و duty اعمال گردد؟\n(لغو = فقط عدد duty ذخیره می‌شود)');
 try{if(go&&!man&&!fx)await setv(19,1);await setv(14+2*n,pm);$('qm'+n).value='';}catch(e){alert(e);}}
async function qzero(n){if(W.run)return alert('داده‌برداری ویزارد در جریان است؛ اول آن را تمام کنید.');
 if(!D||D.on!=1)return alert('لینک STM32 برقرار نیست.');
 const man=manOn(),fx=D&&D.p[13+2*n]===1;if(!man&&!fx&&!confirm('مود دستی خاموش است؛ روشن شود و duty صفر گردد؟'))return;
 try{if(!man&&!fx)await setv(19,1);await setv(14+2*n,0);}catch(e){alert(e);}}
async function fset(n){if(W.run)return alert('داده‌برداری ویزارد در جریان است؛ اول آن را تمام کنید.');if(!D||D.on!=1)return alert('لینک STM32 برقرار نیست.');const on=D&&D.p[13+2*n]===1;try{await setv(13+2*n,on?0:1);}catch(e){alert(e);}}
async function mset(v){if(W.run)return alert('داده‌برداری ویزارد در جریان است؛ اول آن را تمام کنید.');
 if(!D||D.on!=1)return alert('لینک STM32 برقرار نیست.');const man=manOn();if((man?1:0)===v)return;
 if(v&&!man&&!confirm('شارژر خودکار و محافظت‌های باتری متوقف می‌شوند و duty را خودتان تعیین می‌کنید. ادامه؟'))return;
 try{await setv(19,v);}catch(e){alert(e);}}
[1,2].forEach(n=>{const a=$('ma'+n),m=$('mm'+n);if(a)a.onclick=()=>mset(0);if(m)m.onclick=()=>mset(1);});
[1,2].forEach(n=>{const f=$('fx'+n);if(f)f.onclick=()=>fset(n);});
function mview(d){const man=(d.fl&32)!=0,sup=d.p[19]!=null&&d.on==1;
 [1,2].forEach(n=>{const a=$('ma'+n),m=$('mm'+n);if(!a||!m)return;
  a.disabled=m.disabled=!sup;a.classList.toggle('on',sup&&!man);m.classList.toggle('on',sup&&man);const f=$('fx'+n);if(f){f.disabled=!sup||d.p[13+2*n]==null;f.classList.toggle('on',d.p[13+2*n]===1);}});}
poll();
setInterval(uview,250); /* v1.16: آینهٔ LED با ۵۰ms — چشمک هم‌سرعت برد */
</script></body></html>)HTML";
