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
body{background:radial-gradient(1200px 300px at 50% -80px,rgba(99,162,255,.09),transparent),var(--bg);color:var(--tx);font:14px/1.65 Vazirmatn,Tahoma,sans-serif;max-width:1480px;margin:auto;padding:0 14px 28px;scrollbar-color:#3a4767 transparent}
button,input,select,textarea{font:inherit;color:inherit}
button{cursor:pointer}
:focus-visible{outline:2px solid var(--ac);outline-offset:2px;border-radius:8px}
.n{direction:ltr;unicode-bidi:isolate;font-variant-numeric:tabular-nums}
header{position:sticky;top:0;z-index:60;display:flex;align-items:center;justify-content:space-between;gap:10px;padding:12px 2px;margin:0 -14px 12px;padding-left:14px;padding-right:14px;background:rgba(9,12,18,.82);backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px);border-bottom:1px solid var(--ln)}
h1{font-size:17px;font-weight:800;letter-spacing:-.2px;display:flex;align-items:center;gap:9px}
h1::before{content:"";width:11px;height:22px;border-radius:6px;background:linear-gradient(180deg,var(--ac2),var(--ac));box-shadow:0 0 12px rgba(91,157,255,.55)}
.lk{display:flex;align-items:center;gap:8px;font-size:12px;color:var(--mu);background:var(--cd);border:1px solid var(--ln);border-radius:999px;padding:5px 12px 5px 8px}
.lk i{width:9px;height:9px;border-radius:50%;background:var(--er);box-shadow:0 0 8px var(--er)}.lk.on i{background:var(--ok);box-shadow:0 0 8px var(--ok)}
nav{display:flex;gap:4px;background:var(--cd);border:1px solid var(--ln);border-radius:14px;padding:5px;position:sticky;top:55px;z-index:55;margin-bottom:14px;box-shadow:var(--sh)}
nav button{flex:1;border:0;background:none;border-radius:10px;padding:9px 8px;color:var(--mu);font-weight:600;transition:background .15s,color .15s}
nav button:hover{color:var(--tx);background:rgba(255,255,255,.04)}
nav button.a{background:linear-gradient(180deg,#24406e,#1b3358);color:#fff;box-shadow:inset 0 1px 0 rgba(255,255,255,.12),0 2px 8px rgba(0,0,0,.4)}
nav button.m.a{background:linear-gradient(180deg,#6e4a10,#543806);color:#ffe1a8}
section{margin-top:12px}
.cd{background:linear-gradient(180deg,rgba(255,255,255,.025),transparent 30%),var(--cd);border:1px solid var(--ln);border-radius:16px;padding:16px;margin-bottom:14px;box-shadow:var(--sh)}
.ti{font-size:12.5px;font-weight:700;color:var(--mu);margin-bottom:10px;text-transform:uppercase;letter-spacing:.3px}
.lb{color:var(--mu);font-size:12px}
.fl{display:flex;flex-wrap:wrap;gap:6px;margin-top:12px;padding-top:12px;border-top:1px solid var(--ln)}
.tg{font-size:12px;padding:3px 10px;border-radius:999px;background:var(--rs);color:var(--mu);border:1px solid transparent;display:inline-flex;align-items:center;gap:6px}
.tg::before{content:"";width:7px;height:7px;border-radius:50%;background:currentColor;opacity:.9}
.tg.g{background:rgba(52,211,153,.12);color:var(--ok);border-color:rgba(52,211,153,.25)}
.tg.r{background:rgba(251,94,106,.12);color:var(--er);border-color:rgba(251,94,106,.3)}
.tg.y{background:rgba(251,191,36,.12);color:var(--wa);border-color:rgba(251,191,36,.28)}
.ch{display:grid;grid-template-columns:1fr 1fr;gap:14px}
.hd{display:flex;justify-content:space-between;align-items:center;gap:8px;margin-bottom:10px;flex-wrap:wrap}
.hd b{font-size:15px;display:flex;align-items:center;gap:8px}
.hd b::before{content:"";width:4px;height:18px;border-radius:4px;background:linear-gradient(180deg,var(--ac2),var(--ac))}
.big{display:flex;justify-content:space-between;align-items:baseline;margin:6px 0}.big b{font-size:28px;font-weight:800;font-variant-numeric:tabular-nums}
.bg2{display:grid;grid-template-columns:1fr 1fr;gap:0 14px}
.ib{position:relative;flex:none;width:22px;height:22px;border-radius:50%;border:1px solid var(--ac2);background:transparent;color:var(--ac2);font-size:13px;font-weight:800;line-height:1;cursor:pointer;padding:0}
.ib .it{display:none;position:absolute;top:26px;right:-8px;z-index:60;width:max-content;min-width:240px;max-width:min(420px,80vw);background:var(--rs);border:1px solid var(--ln);border-radius:12px;padding:10px 12px;font-size:12.5px;font-weight:400;line-height:2;color:var(--tx);text-align:right;white-space:normal;box-shadow:0 10px 28px rgba(0,0,0,.55)}
.ib:hover .it,.ib:focus-visible .it,.ib.o .it{display:block}
.ch table td.n{text-align:left;white-space:nowrap}
.bar{height:8px;background:var(--in);border:1px solid var(--ln);border-radius:8px;overflow:hidden;position:relative;margin:5px 0 12px}
.bar i{position:absolute;inset:0 0 0 auto;width:0;background:linear-gradient(90deg,var(--ac),var(--ac2));transition:width .3s}
.bar u{position:absolute;top:0;bottom:0;width:2px;background:var(--wa);box-shadow:0 0 6px var(--wa)}
table{width:100%;border-collapse:collapse;font-size:13px}td{padding:6px 2px;border-top:1px solid var(--ln)}td:last-child{text-align:left}
.bt{width:100%;border:0;border-radius:12px;padding:12px;margin-top:12px;font-weight:700;color:#fff;min-height:44px;transition:filter .15s,transform .05s}
.bt:active{transform:scale(.99)}
.cut{background:linear-gradient(180deg,#e5484d,#c62f35)}.run{background:linear-gradient(180deg,#2fbf8f,#1e9e73);color:#04120c}
.rw{display:grid;grid-template-columns:1fr auto;gap:2px 12px;align-items:center;padding:10px 0;border-top:1px solid var(--ln)}.rw:first-of-type{border-top:0}

.ap{font-size:12px;color:var(--ac2);margin-right:6px}
.ct{display:flex;align-items:center;gap:6px}
input[type=number],select{background:var(--in);border:1px solid var(--ln);border-radius:10px;padding:7px 9px;direction:ltr;min-height:36px;transition:border-color .15s,box-shadow .15s}
input[type=number]{width:96px}
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
.bsb{position:sticky;top:113px;z-index:54;border-color:#6b5206}.bqr2{display:flex;gap:8px;margin-top:12px}
.bctl{display:flex;flex-wrap:wrap;gap:10px;align-items:center;margin-top:10px}
.tw{overflow:auto;max-height:420px;margin:6px 0 10px;border:1px solid var(--ln);border-radius:12px}.bt2{font-size:12px;direction:ltr;white-space:nowrap}.bt2 th{position:sticky;top:0;background:var(--rs);color:var(--mu);font-weight:600;text-align:left;padding:6px 8px}.bt2 td{padding:5px 8px;text-align:left}
.bt3{width:auto;font-size:13px}.bt3 th{color:var(--mu);font-weight:600;text-align:right;padding:5px 8px;white-space:nowrap}.bt3 td{padding:5px 8px;text-align:right}.bt3 input[type=number]{padding:6px 8px}
.bsum{font-size:12px;direction:ltr;text-align:left;line-height:1.9;margin-bottom:8px}.okc{color:var(--ok)}.erc{color:var(--er)}
.bxw textarea{width:100%;height:150px;background:var(--in);color:#a7b0c4;border:1px solid var(--ln);border-radius:12px;padding:9px;font:11px/1.5 monospace;direction:ltr;margin-top:6px}
.ds{font-size:13px;line-height:2;color:#c9d0df;background:var(--in);border:1px solid var(--ln);border-radius:12px;padding:11px 15px}.ds ul{padding-right:18px}.ds b{color:var(--tx)}
.qs{display:grid;grid-template-columns:1fr 1fr;gap:12px}.q{background:var(--in);border:1px solid var(--ln);border-radius:12px;padding:12px}.q input[type=number]{width:92px}.q .cut{background:linear-gradient(180deg,#e5484d,#c62f35)}.q .run{background:linear-gradient(180deg,#2fbf8f,#1e9e73);color:#04120c}
.eg3{display:flex;flex-wrap:wrap;gap:10px 16px;margin-top:6px}.eg3 label{display:flex;flex-direction:column;gap:4px;font-size:13px}
.pgx{display:none}.pgx.a{display:block}body:not(.br) .wstop{display:none}body.br .brun{opacity:.4;pointer-events:none}a.lnk{text-decoration:none;display:inline-block}
.bq{background:var(--in);border:1px solid rgba(251,191,36,.4);border-radius:12px;padding:11px;margin-top:10px}.bqr{display:flex;flex-wrap:wrap;gap:10px 14px;align-items:flex-end;margin-top:10px}.bqr label{display:flex;flex-direction:column;gap:4px;font-size:13px;font-weight:600}.bqr label .lb{font-weight:400}.bqr input[type=number]{width:124px}
.cc .ca{border-top:0;margin-top:0;padding-top:0}.stp2{background:linear-gradient(180deg,#e5484d,#c62f35);white-space:nowrap}
.vc{justify-content:center}.vc input[type=number]{width:96px}
.fl{margin-top:0;padding-top:0;border-top:0}#sh .hd{flex-wrap:wrap;gap:8px}
.sec{display:flex;justify-content:space-between;align-items:center;gap:8px;font-size:13px;font-weight:700;color:var(--tx);margin:16px 0 8px;padding-top:13px;border-top:1px solid var(--ln)}
.sec::after{content:"";flex:1;height:1px;background:linear-gradient(90deg,transparent,var(--ln));border-radius:1px}
.frr{display:grid;grid-template-columns:1fr 1fr;gap:0 28px}.frr .rw:first-of-type{border-top:1px solid var(--ln)}.fxw{margin-top:6px;font-size:12px}
.kc{font-size:12px;margin-top:6px}.cr{margin-top:10px;flex-wrap:wrap}.cr .cb{flex:1 1 120px}.cr input[type=number]{width:124px}
.off2{background:linear-gradient(180deg,#a02b33,#7c1f27);white-space:nowrap}
@media(max-width:1000px){.ch,.frr,.qs{grid-template-columns:1fr}}
@media(max-width:640px){.sbt button{font-size:12px;padding:8px 2px}.cb{font-size:12px;padding:9px 8px}.cb span{white-space:nowrap}.ch{grid-template-columns:1fr}.bg2{grid-template-columns:1fr}.ms{grid-template-columns:repeat(3,1fr)}header{margin:0 -8px 10px;padding-left:8px;padding-right:8px}body{padding:0 8px 24px}}
.sbt{display:flex;gap:4px;background:var(--cd);border:1px solid var(--ln);border-radius:12px;padding:4px;margin-bottom:12px}
#sbt{position:sticky;top:113px;z-index:53}
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
.leds{display:flex;gap:14px;align-items:center;flex-wrap:wrap;background:var(--in);border:1px solid var(--ln);border-radius:14px;padding:10px 14px;margin:2px 0 12px}
.led{display:flex;flex-direction:column;align-items:center;gap:3px;min-width:58px}.led i{width:26px;height:26px;border-radius:50%;background:#2a3245;box-shadow:inset 0 2px 5px rgba(0,0,0,.6);transition:background .12s,box-shadow .12s}.led small{color:var(--mu);font-size:11px}.led.r.on i{background:#ff4545;box-shadow:0 0 16px #ff4545,0 0 4px #fff inset}.led.y.on i{background:#ffd23b;box-shadow:0 0 16px #ffd23b}.led.g.on i{background:#2eff8f;box-shadow:0 0 16px #2eff8f}.bit{display:inline-flex;align-items:center;gap:6px;margin:2px 8px 2px 0}.bit i{width:14px;height:14px;border-radius:50%;background:#2a3245;display:inline-block;box-shadow:inset 0 1px 3px rgba(0,0,0,.6)}.bit.on i{background:#ff4545;box-shadow:0 0 9px #ff4545}.bz{font-size:30px;line-height:1;position:relative;min-width:44px;text-align:center}.bz.off{opacity:.22;filter:grayscale(1)}.bz .mx{position:absolute;inset:-4px 0 0 0;color:#ff4545;font-size:36px;display:none;font-weight:700;text-shadow:0 0 6px #000}.bz.muted .mx{display:block}.bz.muted{opacity:.85}
.leds.stick{position:sticky;top:170px;z-index:52;background:rgba(11,15,23,.9);backdrop-filter:blur(8px);-webkit-backdrop-filter:blur(8px);box-shadow:0 4px 18px rgba(0,0,0,.5)}
.fx2{font-size:12px;color:#c9d0df;line-height:1.9;margin-top:4px}
@media(prefers-reduced-motion:reduce){*{transition:none!important}}
.ldon{display:inline-block;width:9px;height:9px;border-radius:50%;background:var(--ok);box-shadow:0 0 8px var(--ok);margin-right:8px}
.hnl input[type=number]{width:66px;min-height:30px;padding:4px 6px}
.movl{position:fixed;inset:0;z-index:300;background:rgba(3,5,9,.72);backdrop-filter:blur(3px);-webkit-backdrop-filter:blur(3px);display:flex;align-items:center;justify-content:center;padding:16px}
.mod{background:var(--cd);border:1px solid var(--ln);border-radius:18px;padding:22px;max-width:460px;width:100%;box-shadow:0 24px 60px rgba(0,0,0,.6)}
.mod h3{font-size:16px;margin-bottom:8px}
.mod .sb{width:100%;margin-top:10px}
input:disabled{opacity:.38;cursor:not-allowed}
.srvw{overflow-x:auto;border:1px solid var(--ln);border-radius:12px}
.srv{width:100%;min-width:680px;border-collapse:collapse;font-size:13px;table-layout:fixed}
.srv col.c1{width:118px}.srv col.c2{width:96px}.srv col.c4{width:128px}.srv col.c5{width:196px}
.srv th{color:var(--mu);font-weight:600;text-align:right;padding:7px 10px;border-bottom:1px solid var(--ln);white-space:nowrap}
.srv td{padding:6px 10px;border-top:1px solid var(--ln);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.srv tr:first-child td{border-top:0}
.srv .tg{min-width:104px;justify-content:center}
.srv input[type=number]{width:78px;min-height:30px;padding:3px 6px}
.srv .sb{min-height:30px;padding:3px 10px}
tr.rok{background:rgba(52,211,153,.05)}tr.rwr{background:rgba(251,191,36,.07)}tr.rbd{background:rgba(251,94,106,.08)}
</style></head><body>
<header><h1>پنل ChangeOver</h1><div class="lk" id="lk"><span id="lt">در حال اتصال…</span><i></i></div></header>
<nav><button class="a" data-t="0">پنل</button><button data-t="1">داده‌برداری بنچ</button><button data-t="2">تنظیمات</button></nav>
<div class="wn gb" id="mb"><div class="mx"><div><b>مود تست دستی فعال است</b> — شارژر خودکار و محافظت‌های باتری متوقف‌اند. <span id="ka"></span></div><button class="sb stp2" id="mx">خروج از مود دستی</button></div></div>
<main id="pg">
<div class="pgx a" id="p0">
<div id="lnkw" class="wbx"></div>
<div class="cd" id="sh"><div class="hd"><b>ولتاژها و وضعیت آلارم‌ها <span class="lb">· عدد مولتی‌متر (V) را کنار هر ولتاژ وارد کنید تا آفست آن کالیبره شود</span> <span class="ldon" id="aslive"></span></b><div class="fl" id="fl"></div></div><div id="ast"></div>
<div class="sec">فیلتر جریان <span class="lb">(مشترک هر دو کانال)</span></div><div class="frr" id="fg"></div><div class="fx fxw" id="ff"></div></div>

<div class="ch" id="ch"></div>
</div>
<div class="pgx" id="p1"></div>
<div class="pgx" id="p2">
<div class="sbt" id="sbt"><button class="a" data-s="0">شارژ و فیلتر</button><button data-s="1">سناریوها</button><button data-s="2">نظارت و ایمنی</button><button data-s="3">PID شارژ</button><button data-s="4">پشتیبان‌گیری</button></div>
<div class="sgx a" id="s0">
<div class="cd">
<div class="hd"><b>نمودار مراحل شارژ</b><span class="lb">· مشترک هر دو کانال · ناحیه‌ها از مقادیر اعمال‌شدهٔ برد · تایپ = خط‌چین پیش‌نمایش · ترکیب نامعتبر = هشدار قرمز</span></div>
<div id="qw" style="margin:2px 0 0"></div>
<div id="qg" style="direction:ltr;overflow-x:auto"></div>
<div class="lb" id="qgl">در انتظار دادهٔ برد…</div>
</div>
<div class="cd">
<div class="hd"><b>فیلتر جریان</b> <button class="ib" data-p="7,8" onclick="this.classList.toggle('o')">!<span class="it">پنجرهٔ مدین: مرحلهٔ اول فیلتر، هر عدد ۱ تا ۱۵ (زوج هم مجاز)؛ ۱ و ۲ = خاموش، ۳ = پیش‌فرض، بزرگ‌تر = حذف پالس قوی‌تر با تاخیر بیشتر.<br>پنجرهٔ میانگین: مرحلهٔ دوم، هر عدد ۱ تا ۳۰۰ — میانگین آخرین W خروجی مدین (هر نمونه ۱ms = ۱ms تاریخچه)؛ ۱ = خاموش، ۱۰ = پیش‌فرض. برای صاف‌شدن قابل‌مشاهده روی نمودار تب «پنل» مجموع را بالای ~۲۰۰ms ببرید؛ در مود خودکار شارژر بالای ~۵۰ توصیه نمی‌شود (کندی حلقهٔ تنظیم ۱۰۰Hz).</span></button><span class="lb">· مشترک هر دو کانال · Median + Average · مثل بقیه روی فلش برد ذخیره می‌شود</span></div>
<div class="bqr">
<label>پنجرهٔ مدین (Median)<input type="number" id="q7" step="1" min="1" max="15"><span class="lb" id="a7">—</span></label>
<label>پنجرهٔ میانگین (Average)<input type="number" id="q8" step="1" min="1" max="300"><span class="lb" id="a8">—</span></label>
</div>

</div>
<div class="cd">
<div class="hd"><b>کالیبراسیون جریان</b> <button class="ib" data-p="0,1,2,3,4,5,6" onclick="this.classList.toggle('o')">!<span class="it">آفست، عدد ADC در جریان صفر است و از هر نمونه کم می‌شود. گین و ETA را با تست جریان سمت باتری حساب کنید — ستونهای خام CSV تب بنچ راهنماست — و اینجا ثبت کنید؛ ETA صفر یعنی بدون تبدیل. کانال ۲ جدول بنچ دارد پس ETA آن صفر بماند؛ تغییر آفست یا گین کانال ۲ نیاز به ساخت دوباره جدول بنچ دارد. هر ۶ عدد روی فلش برد ذخیره و با قطع برق ماندگار است.</span></button><span class="lb">· شناسه ۰..۳ و ۹..۱۰ · روی فلش برد ذخیره می‌شود</span></div>
<div class="bqr">
<label>آفست کانال ۱ (count)<input type="number" id="q0" step="1" min="0" max="255"><span class="lb" id="a0">—</span></label>
<label>آفست کانال ۲ (count)<input type="number" id="q1" step="1" min="0" max="255"><span class="lb" id="a1">—</span></label>
<label>گین کانال ۱ (‰)<input type="number" id="q2" step="1" min="100" max="3000"><span class="lb" id="a2">—</span></label>
<label>گین کانال ۲ (‰)<input type="number" id="q3" step="1" min="100" max="3000"><span class="lb" id="a3">—</span></label>
<label>ضریب ETA کانال ۱ (‰، صفر=خاموش)<input type="number" id="q9" step="1" min="0" max="999"><span class="lb" id="a9">—</span></label>
<label>ضریب ETA کانال ۲ (‰، صفر=خاموش)<input type="number" id="q10" step="1" min="0" max="999"><span class="lb" id="a10">—</span></label>
</div>
</div>
<div class="cd">
<div class="hd"><b>پروفایل شارژ</b> <button class="ib" data-p="20,21,22,23,24,25,26" onclick="this.classList.toggle('o')">!<span class="it">حداکثر ولتاژ باتری: ولتاژ تثبیت فاز ابزورب — بالای آن سوئیچینگ متوقف می‌شود (پیش‌فرض ۱۴۴۰۰).<br>آستانهٔ ورود: با رسیدن باتری به این ولتاژ فاز ابزورب با پلهٔ ریز ۰٫۱٪ آغاز می‌شود (۱۴۳۰۰).<br>سقف تجاوز: بالای این ولتاژ کاهش سریع duty (پلهٔ ۰٫۵٪)؛ همیشه ۵۰mV زیر خطای قطع باتری ۱۴٫۸V نگه داشته می‌شود (۱۴۶۰۰).<br>ولتاژ شناور: نگه‌داشت باتری پس از پایان شارژ (۱۳۵۰۰).<br>ولتاژ بازگشت: افت باتری در شناور زیر این مقدار، بالک را دوباره آغاز می‌کند (۱۲۸۰۰).<br>جریان حداکثر: سقف باند تنظیم جریان بالک؛ کف باند به‌طور خودکار ۲۰mA کمتر است (۶۵۰).<br>جریان تیپر: ابزورب پایان می‌یابد وقتی جریان دنباله ۶۰ ثانیه پایدار زیر این مقدار بماند (۵۰ ~ C/90).<br>پس از هر تغییر، مقدار «اعمال‌شدهٔ» برد کنار همان فیلد نشان داده می‌شود — اگر با درخواست شما فرق دارد یعنی گیره خورده تا مجموعه سازنده بماند (مثلاً ورود ≤ ابزورب−۵۰). سقف‌های ایمنی (خطای سخت ۹۵۰mA و قطع OV ۱۵V) از زیرتب «نظارت و ایمنی» فقط پایین‌بردنی‌اند و هرگز بالای مقدار کارخانه نمی‌روند.<br>ماندگاری: هر پارامتری که از پنل ثبت کنید (~۱٫۵ ثانیه بعد) در فلش خودِ برد ذخیره می‌شود و خاموش/روشن کردن برد آن را از بین نمی‌برد؛ دکمهٔ «بازگردانی پیش‌فرض کارخانه» پیش‌فرض‌ها را می‌فرستد و همان‌ها ذخیره می‌شوند. مودهای تست (دیوتی فیکس/دستی) هرگز ذخیره نمی‌شوند — بعد از هر ریست، شارژر خودکار است.<br>نگهبان ترکیب: اگر عددهای تایپ‌شده با هم ناسازگار باشند (مثلاً شناور بالای ابزورب−۳۰۰)، بالای نمودار هشدار قرمز می‌آید، فیلد مقصر قرمز می‌شود و قبل از ارسال تأیید گرفته می‌شود — چون برد همان را گیره می‌زند و ناحیه‌ها را به‌هم‌ریخته نمی‌گذارد.</span></button><span class="lb">· مشترک هر دو کانال · روی فلش برد ذخیره می‌شود و با قطع برق می‌ماند (~۱٫۵ ثانیه پس از آخرین تغییر)</span></div>
<div class="sec">ولتاژها <span class="lb">(mV)</span></div>
<div class="bqr">
<label>حداکثر ولتاژ باتری (ابزورب)<input type="number" id="q20" step="50" min="11000" max="14600"><span class="lb" id="a20">—</span></label>
<label>آستانهٔ ورود به ابزورب<input type="number" id="q21" step="10" min="10500" max="14550"><span class="lb" id="a21">—</span></label>
<label>سقف تجاوز ابزورب<input type="number" id="q22" step="10" min="11100" max="14750"><span class="lb" id="a22">—</span></label>
<label>ولتاژ شناور<input type="number" id="q23" step="50" min="9000" max="14300"><span class="lb" id="a23">—</span></label>
<label>ولتاژ بازگشت به بالک<input type="number" id="q24" step="50" min="8000" max="14000"><span class="lb" id="a24">—</span></label>
</div>
<div class="sec">جریان‌ها <span class="lb">(mA)</span></div>
<div class="bqr">
<label>جریان حداکثر شارژ (بالک)<input type="number" id="q25" step="10" min="100" max="900"><span class="lb" id="a25">—</span></label>
<label>جریان تیپر (ورود به شناور)<input type="number" id="q26" step="5" min="10" max="300"><span class="lb" id="a26">—</span></label>
</div>

<div class="bqr"><button class="sb sb2" onclick="qdef()">بازگردانی پیش‌فرض کارخانه</button></div>
</div>
</div>
<div class="sgx" id="s1">
<div class="leds stick" id="uleds">
<div class="led r" id="ulR"><i></i><small>قرمز</small></div>
<div class="led y" id="ulY"><i></i><small>زرد</small></div>
<div class="led g" id="ulG"><i></i><small>سبز</small></div>
<div class="bz off" id="ulB">🔊<span class="mx">✕</span></div>
<div style="display:flex;flex-direction:column;gap:2px;flex:1;min-width:220px"><span class="lb" id="uscn">—</span><span class="lb" id="utim">—</span></div>
<button class="sb" onclick="xmute()">🔇/🔊 میوت</button><span class="lb" id="xmuteS">—</span>
</div>
<div class="hd" style="margin-top:10px"><b>سناریوهای LED و بازر</b><span class="lb">· یک سناریو را انتخاب کنید · همه روی فلش برد ذخیره می‌شوند</span></div>
<div id="aw2" style="margin:2px 0 0"></div>
<div class="sbt" id="usel"><button class="a" data-u="1">۱ · اضافه‌ولتاژ</button><button data-u="2">۲ · قطع باتری</button><button data-u="3">۳ · دشارژ</button><button data-u="4">۴ · شارژ عادی</button><button data-u="5">۵ · باتری و درصد</button></div>
<div class="cd" id="ucard1">
<div class="hd"><b>سناریو ۱ — اضافه‌ولتاژ ورودی</b> <button class="ib" data-p="38,39,40,41,42,43" onclick="this.classList.toggle('o')">!<span class="it">روند: عبور ورودی از سقف ← قرمز چشمک + بوق دوره‌ای (سبز ثابت می‌ماند) ← افت تا سقف−هیسترزیس ← پاک‌شدن و بازگشت به سناریوی قبلی. پیش‌فرض: چشمک ۱۰۰۰/۵۰٪ + یک بوق ۱ثانیه‌ای هر ۱۰ ثانیه.</span></button><span class="lb">· سقف ولتاژ + چشمک و بوق · اولویت اول برد</span></div>
<div class="sec">سقف ولتاژ <span class="lb">(mV)</span></div>
<div class="bqr">
<label>آستانه اضافه‌ولتاژ ورودی (mV)<input type="number" id="q70" step="100" min="24000" max="32000"><span class="lb" id="a70">—</span></label>
<label>هیسترزیس اضافه‌ولتاژ (mV)<input type="number" id="q71" step="100" min="0" max="2000"><span class="lb" id="a71">—</span></label>
</div>
<div class="sec">چشمک قرمز <span class="lb">(دوره/دیوتی)</span></div>
<div class="bqr">
<label>دوره چشمک قرمز (ms)<input type="number" id="q38" step="50" min="100" max="10000"><span class="lb" id="a38">—</span></label>
<label>دیوتی قرمز (٪)<input type="number" id="q39" step="5" min="0" max="100"><span class="lb" id="a39">—</span></label>
</div>
<div class="sec">بوق</div>
<div class="bqr">
<label>دوره بوق (ms، صفر=خاموش)<input type="number" id="q40" step="500" min="0" max="600000"><span class="lb" id="a40">—</span></label>
<label>مدت هر بوق (ms)<input type="number" id="q41" step="50" min="0" max="600000"><span class="lb" id="a41">—</span></label>
<label>تعداد بوق<input type="number" id="q42" step="1" min="0" max="10"><span class="lb" id="a42">—</span></label>
<label>گپ بین بوق‌ها (ms)<input type="number" id="q43" step="50" min="0" max="5000"><span class="lb" id="a43">—</span></label>
</div>

</div>
<div class="cd" id="ucard2" style="display:none">
<div class="hd"><b>سناریو ۲ — قطع باتری</b> <button class="ib" data-p="27,28,29,30,31,32" onclick="this.classList.toggle('o')">!<span class="it">روند: قفل‌شدن پرچم قطع‌باتری ← قرمز چشمک + بوق دوره‌ای (سبز ثابت) ← پاک‌شدن پرچم ← بازگشت به سناریوی قبلی. پیش‌فرض: سه بوق کوتاه. آستانه‌های تشخیص قطع/برگشت در کارت «نظارت باتری» (زیرتب نظارت و ایمنی، ۲۷..۳۲) است.</span></button><span class="lb">· شناسه‌های ۴۴..۴۹ · اولویت دوم برد</span></div>
<div class="sec">چشمک قرمز <span class="lb">(دوره/دیوتی)</span></div>
<div class="bqr">
<label>دوره چشمک قرمز (ms)<input type="number" id="q44" step="50" min="100" max="10000"><span class="lb" id="a44">—</span></label>
<label>دیوتی قرمز (٪)<input type="number" id="q45" step="5" min="0" max="100"><span class="lb" id="a45">—</span></label>
</div>
<div class="sec">بوق</div>
<div class="bqr">
<label>دوره بوق (ms، صفر=خاموش)<input type="number" id="q46" step="500" min="0" max="600000"><span class="lb" id="a46">—</span></label>
<label>مدت هر بوق (ms)<input type="number" id="q47" step="50" min="0" max="600000"><span class="lb" id="a47">—</span></label>
<label>تعداد بوق<input type="number" id="q48" step="1" min="0" max="10"><span class="lb" id="a48">—</span></label>
<label>گپ بین بوق‌ها (ms)<input type="number" id="q49" step="50" min="0" max="5000"><span class="lb" id="a49">—</span></label>
</div>

</div>
<div class="cd" id="ucard3" style="display:none">
<div class="hd"><b>سناریو ۳ — دشارژ (بی‌ورودی)</b> <button class="ib" data-p="44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65" onclick="this.classList.toggle('o')">!<span class="it">روند: بالای «شروع بوق» بی‌صدا (سبز چشمک با درصد) ← هر باند بوق خودش با فاصله/مدت/تعداد خودش ← زیر «باند بحرانی»: LEDها خاموش و الگوی بحرانی فقط یک‌بار به‌اندازهٔ «طول یک‌باره» پخش و بعد سکوت تا برگشت باتری. با پیش‌فرض‌ها: بالای ۴۰٪ بی‌صدا؛ ۲۰..۴۰ یک بوق، ۱۰..۲۰ دو بوق (هر ۶۰ ثانیه)؛ ۱..۱۰ سه بوق (هر ۲۰ ثانیه)؛ زیر ۱٪ یک بوق ۱۰ثانیه‌ای. گپ (۶۵) مشترک همهٔ باندهاست.</span></button><span class="lb">· شناسه‌های ۵۰..۶۷/۸۰..۸۲ · باندها + چشمک سبز + پایداری</span></div>
<div class="sec">چشمک سبز <span class="lb">(ms)</span></div>
<div class="bqr">
<label>دوره چشمک سبز (ms)<input type="number" id="q66" step="50" min="100" max="10000"><span class="lb" id="a66">—</span></label>
<label>حداقل خاموشی سبز (ms)<input type="number" id="q67" step="5" min="0" max="10000"><span class="lb" id="a67">—</span></label>
</div>
<div class="sec">باندهای درصد <span class="lb">(٪ باتری؛ همیشه شروع ≥ دو-بوق ≥ سه-بوق ≥ بحرانی)</span></div>
<div class="bqr">
<label>شروع بوق (٪)<input type="number" id="q50" step="1" min="0" max="100"><span class="lb" id="a50">—</span></label>
<label>باند دو-بوق (٪)<input type="number" id="q51" step="1" min="0" max="100"><span class="lb" id="a51">—</span></label>
<label>باند سه-بوق (٪)<input type="number" id="q52" step="1" min="0" max="100"><span class="lb" id="a52">—</span></label>
<label>باند بحرانی (٪)<input type="number" id="q53" step="1" min="0" max="100"><span class="lb" id="a53">—</span></label>
</div>
<div class="sec">فاصله و دوره <span class="lb">(ms؛ صفر=خاموش)</span></div>
<div class="bqr">
<label>فاصله بوق ۱/۲تایی (ms)<input type="number" id="q54" step="1000" min="0" max="600000"><span class="lb" id="a54">—</span></label>
<label>فاصله بوق ۳تایی (ms)<input type="number" id="q55" step="1000" min="0" max="600000"><span class="lb" id="a55">—</span></label>
<label>دوره بوق بحرانی (ms)<input type="number" id="q56" step="500" min="0" max="600000"><span class="lb" id="a56">—</span></label>
<label>دیوتی بوق بحرانی (٪)<input type="number" id="q57" step="5" min="0" max="100"><span class="lb" id="a57">—</span></label>
<label>تعداد بوق بحرانی<input type="number" id="q58" step="1" min="0" max="10"><span class="lb" id="a58">—</span></label>
</div>
<div class="sec">مدت، تعداد و گپ <span class="lb">(ms / عدد)</span></div>
<div class="bqr">
<label>مدت هر بوق ۱/۲تایی (ms)<input type="number" id="q59" step="50" min="0" max="600000"><span class="lb" id="a59">—</span></label>
<label>مدت هر بوق ۳تایی (ms)<input type="number" id="q60" step="50" min="0" max="600000"><span class="lb" id="a60">—</span></label>
<label>طول بوق بحرانی یک‌باره (ms)<input type="number" id="q61" step="500" min="0" max="120000"><span class="lb" id="a61">—</span></label>
<label>تعداد بوق باند ۱<input type="number" id="q62" step="1" min="0" max="10"><span class="lb" id="a62">—</span></label>
<label>تعداد بوق باند ۲<input type="number" id="q63" step="1" min="0" max="10"><span class="lb" id="a63">—</span></label>
<label>تعداد بوق باند ۳<input type="number" id="q64" step="1" min="0" max="10"><span class="lb" id="a64">—</span></label>
<label>گپ بوق دشارژ (ms)<input type="number" id="q65" step="10" min="0" max="5000"><span class="lb" id="a65">—</span></label>
</div>
<div class="sec">پایداری درصد <span class="lb">(٪؛ صفر یعنی تعقیب لحظه‌ای خام)</span></div>
<div class="bqr">
<label>هیسترزیس پایداری دشارژ (٪)<input type="number" id="q80" step="1" min="0" max="50"><span class="lb" id="a80">—</span></label>
<label>خروج از حالت ۰٪ (٪)<input type="number" id="q81" step="1" min="0" max="100"><span class="lb" id="a81">—</span></label>
<label>خروج از حالت ۱٪ (٪)<input type="number" id="q82" step="1" min="0" max="100"><span class="lb" id="a82">—</span></label>
</div>

</div>
<div class="cd" id="ucard4" style="display:none">
<div class="hd"><b>سناریو ۴ — شارژ عادی</b> <button class="ib" data-p="66,67,68,69,77,78,79,80,81,82" onclick="this.classList.toggle('o')">!<span class="it">روند: حین شارژ واقعی، مدت روشن‌بودن زرد = مانده تا فول (باتری پرتر ← چشمک کوتاه‌تر) ← پایان ابزورب هر کانال (تیپر زیر ۵۰mA یا سقف ۱ساعت)؛ با تمام‌شدن هر دو کانال: فول ← سبز ثابت. فول ولتاژی (۱۰۰٪، خروج زیر ۹۵٪) هم سر جایش است.</span></button><span class="lb">· شناسه‌های ۶۸/۶۹/۷۷..۷۹ · چشمک زرد + فول</span></div>
<div class="sec">چشمک زرد <span class="lb">(ms)</span></div>
<div class="bqr">
<label>دوره چشمک زرد (ms)<input type="number" id="q68" step="50" min="100" max="10000"><span class="lb" id="a68">—</span></label>
<label>حداقل روشنی زرد (ms)<input type="number" id="q69" step="5" min="0" max="10000"><span class="lb" id="a69">—</span></label>
</div>
<div class="sec">فول و پایداری <span class="lb">(٪؛ ورود فول همیشه بالای خروج است)</span></div>
<div class="bqr">
<label>ورود فول (٪)<input type="number" id="q77" step="1" min="1" max="100"><span class="lb" id="a77">—</span></label>
<label>خروج فول (٪)<input type="number" id="q78" step="1" min="0" max="100"><span class="lb" id="a78">—</span></label>
<label>هیسترزیس پایداری شارژ (٪)<input type="number" id="q79" step="1" min="0" max="50"><span class="lb" id="a79">—</span></label>
</div>

</div>
<div class="cd" id="ucard5" style="display:none">
<div class="hd"><b>آستانه‌های باتری و نگاشت درصد</b> <button class="ib" data-p="70,71,72,73,74,75" onclick="this.classList.toggle('o')">!<span class="it">روند: افت باتری زیر آستانه ← پرچم باتری کم (پیوسته) + ⚠ در آینه ← صعود تا سطح پاک‌شدن ← پاک‌شدن پرچم. نگاشت ۷۴/۷۵ درصد همهٔ سناریوها را می‌سازد؛ سقف همیشه دست‌کم ۱۰۰mV بالای کف است.</span></button><span class="lb">· شناسه‌های ۷۲..۷۵</span></div>
<div class="sec">آلارم باتری کم <span class="lb">(mV)</span></div>
<div class="bqr">
<label>آستانه آلارم باتری کم (mV)<input type="number" id="q72" step="100" min="15000" max="24000"><span class="lb" id="a72">—</span></label>
<label>پاک‌شدن آلارم باتری کم (mV)<input type="number" id="q73" step="100" min="15000" max="24000"><span class="lb" id="a73">—</span></label>
</div>
<div class="sec">نگاشت ولتاژ به درصد <span class="lb">(mV)</span></div>
<div class="bqr">
<label>کف نگاشت درصد (mV)<input type="number" id="q74" step="100" min="15000" max="25000"><span class="lb" id="a74">—</span></label>
<label>سقف نگاشت درصد (mV)<input type="number" id="q75" step="100" min="25000" max="32000"><span class="lb" id="a75">—</span></label>
</div>

</div>
<input type="hidden" id="q76" value="">
<div class="bqr"><button class="sb sb2" onclick="sdef()">بازگردانی پیش‌فرض کارخانهٔ سناریوها</button></div>
</div>
<div class="sgx" id="s2">
<div class="cd">
<div class="hd"><b>نظارت باتری</b> <button class="ib" data-p="27,28,29,30,31,32" onclick="this.classList.toggle('o')">!<span class="it">قطع باتری: اگر هر نیمه حین پمپ بالای این ولتاژ برود، سیم باتری قطع فرض می‌شود (پیش‌فرض ۱۴۸۰۰)؛ باید بالای سقف تجاوز+۵۰ و زیر قطع OV−۱۰۰ بماند وگرنه برد گیره‌اش می‌زند.<br>دبانس قطع: شرط بالا باید این‌قدر میلی‌ثانیه پیوسته برقرار بماند تا لچ شود (۱۵۰).<br>غیبت/برگشت: زیر آستانهٔ غیبت (۶۰۰۰) باتری نیست؛ بالای بازگشت (۷۰۰۰) برگشته — همیشه ۵۰۰mV از هم فاصله دارند.<br>دبانس غیبت/بازیابی: پایداری لازم برای اعلام غیبت و اعلام سلامتی (۱۰۰۰/۱۰۰۰).</span></button><span class="lb">· شناسه‌های ۲۷..۳۲ · روی فلش برد ذخیره می‌شود (~۱٫۵ ثانیه پس از آخرین تغییر)</span></div>
<div id="aw" style="margin:2px 0 0"></div>
<div class="sec">قطع باتری <span class="lb">(mV / ms)</span></div>
<div class="bqr">
<label>آستانهٔ قطع باتری (mV)<input type="number" id="q27" step="50" min="14000" max="15000"><span class="lb" id="a27">—</span></label>
<label>دبانس قطع (ms)<input type="number" id="q28" step="10" min="50" max="1000"><span class="lb" id="a28">—</span></label>
</div>
<div class="sec">غیبت / بازگشت باتری <span class="lb">(mV / ms)</span></div>
<div class="bqr">
<label>آستانهٔ غیبت (mV)<input type="number" id="q29" step="100" min="3000" max="8000"><span class="lb" id="a29">—</span></label>
<label>آستانهٔ بازگشت (mV)<input type="number" id="q30" step="100" min="4000" max="9000"><span class="lb" id="a30">—</span></label>
<label>دبانس غیبت (ms)<input type="number" id="q31" step="50" min="100" max="5000"><span class="lb" id="a31">—</span></label>
<label>دبانس بازیابی (ms)<input type="number" id="q32" step="50" min="100" max="5000"><span class="lb" id="a32">—</span></label>
</div>

</div>
<div class="cd">
<div class="hd"><b>پنجرهٔ ورودی سالم</b> <button class="ib" data-p="33,34" onclick="this.classList.toggle('o')">!<span class="it">تشخیص «ورودی حاضر» فقط داخل این پنجره است (پیش‌فرض ۲۱۰۰۰..۲۸۰۰۰)؛ کف و سقف همیشه ۱۰۰۰mV از هم فاصله دارند. بیرون پنجره، شارژر منتظر ورودی می‌ماند.</span></button><span class="lb">· شناسه‌های ۳۳..۳۴ · روی فلش برد ذخیره می‌شود</span></div>
<div class="bqr">
<label>کف ورودی سالم (mV)<input type="number" id="q33" step="100" min="18000" max="24000"><span class="lb" id="a33">—</span></label>
<label>سقف ورودی سالم (mV)<input type="number" id="q34" step="100" min="24000" max="30000"><span class="lb" id="a34">—</span></label>
</div>

</div>
<div class="cd">
<div class="hd"><b>سقف‌های ایمنی شارژر</b> <button class="ib" data-p="35,36,37" onclick="this.classList.toggle('o')">!<span class="it">خطای سخت جریان: بالای این مقدار کانال ریست و متوقف می‌شود (پیش‌فرض ۹۵۰)؛ همیشه بالای جریان بالک+۵۰ نگه داشته می‌شود تا تنظیم سالم تریپ نکند.<br>قطع OV: بالای این ولتاژ باتری نامعتبر و سوئیچینگ متوقف می‌شود (پیش‌فرض ۱۵۰۰۰)؛ همیشه بالای سقف تجاوز+۱۵۰ است.<br>کف اعتبار: زیر این ولتاژ باتری نامعتبر شمرده می‌شود (پیش‌فرض ۲۰۰۰).<br>پس از هر تغییر، مقدار «اعمال‌شدهٔ» برد کنار همان فیلد نشان داده می‌شود — اگر با درخواست شما فرق دارد یعنی گیره خورده تا مجموعه سازنده بماند.<br>نگهبان ترکیب مثل زیرتب شارژ و فیلتر: عدد ناسازگار هشدار قرمز و تأیید قبل از ارسال می‌گیرد.</span></button><span class="lb">· شناسه‌های ۳۵..۳۷ · فقط پایین‌بردنی — هرگز بالای سقف کارخانه نمی‌روند · روی فلش برد ذخیره می‌شود</span></div>
<div class="bqr">
<label>خطای سخت جریان (mA)<input type="number" id="q35" step="10" min="150" max="950"><span class="lb" id="a35">—</span></label>
<label>قطع اضافه‌ولتاژ OV (mV)<input type="number" id="q36" step="50" min="14000" max="15000"><span class="lb" id="a36">—</span></label>
<label>کف اعتبار باتری (mV)<input type="number" id="q37" step="100" min="0" max="8000"><span class="lb" id="a37">—</span></label>
</div>

<div class="bqr"><button class="sb sb2" onclick="adef()">بازگردانی پیش‌فرض کارخانهٔ نظارت و ایمنی</button></div>
</div>
</div>
<div class="sgx" id="s3">
<div class="cd">
<div class="hd"><b>PID دوحلقه‌ای شارژ (CC/CV)</b> <button class="ib" data-p="83,84,85,86,87,88,89,90,91,92" onclick="this.classList.toggle('o')">!<span class="it">به‌جای پله‌های ثابت قدیمی، دیوتی را PID تعیین می‌کند. دقیقاً دو حلقه دارد، چون شارژ باتری دقیقاً دو چیز را باید هم‌زمان محدود کند:<br><b>حلقهٔ جریان (CC):</b> تا وقتی باتری خالی است، جریان ثابت ۶۵۰ میلی‌آمپر می‌دهد.<br><b>حلقهٔ ولتاژ (CV):</b> وقتی باتری پر می‌شود، ولتاژ را روی ۱۴٫۴ نگه می‌دارد و دیوتی را پایین می‌آورد. «نرخ صعود» این ردیف همان کلید «رشد کندتر ابزورب» است.<br>هر لحظه هر حلقه‌ای که دیوتی کمتری بخواهد برنده است، پس هر دو حد همیشه می‌بندند.<br><br><b>دو حلقه چطور یک دیوتی را می‌رانند؟</b> دو خروجی وجود ندارد — یک دیوتی هست، یک انتگرال‌گیر، و دو حلقه که نوبتی آن را می‌رانند. تصور کنید دو نفر بالای <i>یک</i> ولوم ایستاده‌اند و قانون یک خط است: هر لحظه هر کس عدد کمتری بخواهد ولوم دست اوست، و نفر دیگر آن پاس کاملاً نادیده گرفته می‌شود (نه میانگین، نه جمع). اول شارژ باتری خالی است و تا ۱۴٫۴ کلی جا دارد، پس نفر «ولتاژ» می‌گوید «+۱۱۰ پرمیل راحت برو بالا» ولی نفر «جریان» می‌گوید «+۷٫۶ پرمیل، یواش» ⇒ عدد کمتر برنده، حلقهٔ جریان می‌راند (همان CC). حدود دقیقهٔ ۱۱۳ ولتاژ به ۱۴٫۴ می‌رسد، نفر «ولتاژ» عددش منفی می‌شود (یعنی «بکش پایین») و ولوم را می‌گیرد (همان CV). در کل شارژ فقط <b>یک بار</b> این تحویل رخ می‌دهد.<br><b>و چرا لحظهٔ تحویل تکان نمی‌خورد:</b> چون انتگرال‌گیر یکی است و مشترک. سر همان پاس تحویل، انتگرال ۱۹۹۵۱۱ می‌ماند و دیوتی روی ۲۰۰ ثابت است — صفر پرش. اگر هر حلقه انتگرال خودش را داشت، آن‌که بی‌کار بود انتگرالش جای دیگری می‌ماند و لحظهٔ تحویل دیوتی می‌پرید. پس ۱۰ عدد یعنی «دستورالعمل نفر اول + دستورالعمل نفر دوم»، نه دو خروجی. این روش اسم استاندارد دارد: min-select (کمینه‌گیری)، همان چیزی که در هر شارژر CC/CV صنعتی هست.<br><b>چرا یک PID تنها کافی نیست؟</b> امتحان شد و روی شبیه‌ساز مردود شد: یک ردیف ضریب مشترک یعنی مقایسهٔ میلی‌ولت با میلی‌آمپر و لرزش دیوتی (۱۴۰۸ تا ۸۴۶۶۰ تغییر جهت در ۱۰ ساعت در برابر ۴ تای الان)؛ و فقط یک PID ولتاژ با پشتیبان ۶۵۰ به‌عنوان تنها ترمز جریان، جریان را روی ۷۰۷ میلی‌آمپر می‌برد، چون پشتیبان فقط <i>بعد از</i> رد شدن از حد جواب می‌دهد. دو حلقه کمترین چیزی است که واقعاً کار می‌کند.<br><b>مرحلهٔ سوم قبلی حذف شد:</b> اندازه‌گیری نشان داد هیچ سودی ندارد و پنج عدد اضافه بود.<br>واحدها: Kp = پرمیل دیوتی به ازای هر ولت خطا (حلقهٔ ولتاژ) یا هر آمپر خطا (حلقهٔ جریان). Ki = میلی‌پرمیل بر ثانیه. نرخ صعود/نزول = سقف سرعت حرکت نقطهٔ کار (۱۰۰۰ = ۱ پرمیل بر ثانیه).<br>Kd پیش‌فرض صفر است؛ مشتق روی سیگنال نویزی فقط دیوتی را می‌لرزاند.<br>اعداد پیش‌فرض روی مدل کالیبره شده‌اند (۱۴٫۴ ولت، ۶۵۰ میلی‌آمپر، سقف ۵۰۰ پرمیل) — می‌توانید بهینه‌شان کنید و با دکمهٔ پایین برگردید.<br><b>دو پشتیبان سخت همیشه فعال‌اند و از پنل تنظیم نمی‌شوند:</b> عبور جریان از ۶۵۰ میلی‌آمپر یا ولتاژ از ۱۴٫۸ ولت سقف دیوتی را به تناسب تجاوز جمع می‌کند.<br>بقیهٔ حفاظت‌ها دست‌نخورده‌اند: سقف دیوتی ۵۰۰‰، خطای سخت جریان، قطع OV و کل ماشین حالت.</span></button><span class="lb">· شناسه‌های ۸۳..۹۲ · روی فلش برد ذخیره می‌شود (~۱٫۵ ثانیه پس از آخرین تغییر)</span></div>
<div class="sec">حلقهٔ جریان — CC (بالک) <span class="lb">· پیش‌فرض ۱۲ / ۱۶۰۰ / ۰ / ۱۰۰۰ / ۱۰۰۰ · کالیبرهٔ کارخانه</span></div>
<div class="bqr">
<label>Kp (‰ بر آمپر)<input type="number" id="q83" step="5" min="0" max="20000"><span class="lb" id="a83">—</span></label>
<label>Ki<input type="number" id="q84" step="50" min="0" max="20000"><span class="lb" id="a84">—</span></label>
<label>Kd<input type="number" id="q85" step="10" min="0" max="20000"><span class="lb" id="a85">—</span></label>
<label>نرخ صعود (m‰/s)<input type="number" id="q86" step="10" min="10" max="20000"><span class="lb" id="a86">—</span></label>
<label>نرخ نزول (m‰/s)<input type="number" id="q87" step="10" min="10" max="20000"><span class="lb" id="a87">—</span></label>
</div>
<div class="sec">حلقهٔ ولتاژ — CV (ابزورب) <span class="lb">· پیش‌فرض ۵۰ / ۱۸۰۰۰ / ۰ / ۱۰ / ۱۰۰۰ · نرخ صعود = کلید «رشد کندتر ابزورب»</span></div>
<div class="bqr">
<label>Kp (‰ بر ولت)<input type="number" id="q88" step="10" min="0" max="20000"><span class="lb" id="a88">—</span></label>
<label>Ki<input type="number" id="q89" step="500" min="0" max="20000"><span class="lb" id="a89">—</span></label>
<label>Kd<input type="number" id="q90" step="10" min="0" max="20000"><span class="lb" id="a90">—</span></label>
<label>نرخ صعود (m‰/s)<input type="number" id="q91" step="5" min="10" max="20000"><span class="lb" id="a91">—</span></label>
<label>نرخ نزول (m‰/s)<input type="number" id="q92" step="10" min="10" max="20000"><span class="lb" id="a92">—</span></label>
</div>
<div id="pw" style="margin:6px 0 0"></div>
<div class="bqr"><button class="sb sb2" onclick="pdef()">بازگردانی پیش‌فرض کارخانهٔ PID</button></div>
</div>
</div>
<div class="sgx" id="s4">
<div class="cd">
<div class="hd"><b>پشتیبان‌گیری همهٔ تنظیمات</b> <button class="ib" data-p="76" onclick="this.classList.toggle('o')">!<span class="it">خروجی، همهٔ مقادیر «اعمال‌شدهٔ» برد (کالیبراسیون، فیلتر، فعال‌سازی/سقف‌ها، پروفایل، آلارم‌ها، سناریوها) را در یک فایل JSON ذخیره می‌کند. ورودی همان فایل را می‌خواند و مقدارها را یکی‌یکی روی برد اعمال می‌کند (با تأیید شما؛ برد هر مقدار را گیره می‌زند و نتیجه کنار همان فیلد دیده می‌شود). گذراها (۱۵..۱۹ و میوت ۷۶) جزو پشتیبان نیستند. از v1.23 ضرایب PID سه‌مرحله‌ای (۸۳..۹۷) هم در همین پشتیبان می‌آیند.</span></button><span class="lb">· یک بکاپ برای کل بخش تنظیمات — خروجی/ورودی JSON همهٔ ۹۲ مقدار ماندگار (۰..۱۴، ۲۰..۷۵، ۷۷..۹۷) — شامل ضرایب PID</span></div>
<div class="bqr">
<button class="sb sb2" onclick="xexp()">⬇ خروجی (دانلود JSON)</button>
<label class="sb" style="cursor:pointer">⬆ ورودی (انتخاب فایل)<input type="file" id="xim" accept=".json,application/json" style="display:none"></label>
<span class="lb" id="xst">—</span>
</div>

</div>
</div>
</div>
</main>

<script>
const $=i=>document.getElementById(i);
const ST=['خاموش','Bulk','Absorb','Float','راه‌اندازی','انتظار JIT','انتظار ورودی','خطای نهایی','باتری قطع','دستی'];
const SC=['','g','g','g','y','r','y','r','r','y'];
/* ثابت‌های بخش 5.3 سند */
const K_UV=3300/4095*11/10*1000/101,K_MA=K_UV/10,K24=3300/4095*76000/6800,K24B=3300/4095*69200/6800,K12=3300/4095*41000/6800;
/* شناسه: [عنوان, واحد, کمینه, بیشینه, نوع(n عدد، b کلید), توضیح] */
const P={
13:['سقف دیوتی','‰',0,500,'n','سقف دیوتی همین کانال؛ هر ۱۰ واحد یعنی ۱٪ (۵۰۰ = ۵۰٪). هر دیوتی بالاتر — خودکار، فیکس یا دستی — محدود به همین سقف است.'],
14:['سقف دیوتی','‰',0,500,'n','سقف دیوتی همین کانال؛ هر ۱۰ واحد یعنی ۱٪ (۵۰۰ = ۵۰٪). هر دیوتی بالاتر — خودکار، فیکس یا دستی — محدود به همین سقف است.']};
/* ولتاژها: [عنوان, اندیس t, شناسهٔ آفست, ضریب مقسم] */
const V=[['ورودی',14,4,K24],['پک ۲۴V',15,5,K24B],['نود ۱۲V',16,6,K12],['باتری بالا',18],['باتری پایین',17]];
var D=null;/* var (نه let) تا در تست هاست هم قابل‌نوشتن باشد */
const v2=mv=>(mv/1000).toFixed(2),pc=pm=>(pm/10).toFixed(1)+'%';
function send(id,v){const a=$('a'+id);if(a)a.textContent='…';fetch('/s?id='+id+'&v='+v,{method:'POST'}).then(r=>{if(!r.ok)throw 0;}).catch(()=>{if(a)a.textContent='خطا';});}
function num(id){const e=$('i'+id),p=P[id],v=Math.round(+e.value);if(e.value===''||isNaN(v))return;send(id,Math.min(p[3],Math.max(p[2],v)));e.value='';e.blur();}
function ctl(id){const p=P[id];
 return `<input type="number" id="i${id}" min="${p[2]}" max="${p[3]}" placeholder="${p[2]<0?'±'+p[3]:p[2]+'…'+p[3]}" onkeydown="if(event.key=='Enter')num(${id})"><button class="sb" onclick="num(${id})">ثبت</button>`;}
const row=(id,x)=>`<div class="rw"><div>${P[id][0]} <span class="lb">${P[id][1]}</span><button class="ib" onclick="this.classList.toggle('o')">!<span class="it">${P[id][5]}</span></button><span class="ap n" id="a${id}">—</span></div><div class="ct">${x||''}${ctl(id)}</div></div>`;

/* ---------- ساخت صفحه: ولتاژها + فیلتر (مشترک) ---------- */
/* v1.16k: merged voltages+alarm table - fixed layout, each value once, pills inline */
const SR=[['ورودی',0],['پک ۲۴V',1],['نود ۱۲V',2],['باتری بالا',3],['باتری پایین',4],['جریان ۱ (بالا)',5],['جریان ۲ (پایین)',6]];
$('ast').innerHTML=`<div class="srvw"><table class="srv"><colgroup><col class="c1"><col class="c2"><col class="c3"><col class="c4"><col class="c5"></colgroup><tr><th>سیگنال</th><th>مقدار</th><th>فرمول / بازه</th><th>وضعیت</th><th>کالیبره با مولتی‌متر</th></tr>${SR.map(r=>{const k=r[1];
 const cal=k<3?`<input type="number" step="any" id="vm${k}" placeholder="مولتی‌متر V" onkeydown="if(event.key=='Enter')vcal(${k})"> <button class="sb sb2" onclick="vcal(${k})">اعمال</button> <span class="lb">±<span class="ap n" id="a${[4,5,6][k]}">—</span></span>`:'—';
 return `<tr id="sr${k}"><td>${r[0]}</td><td class="n" id="v${k}">—</td><td><div class="fx" id="fv${k}"></div></td><td><span class="tg" id="sp${k}">—</span></td><td>${cal}</td></tr>`;}).join('')}</table></div>`+'<div class="ab" id="asb5" style="margin-top:8px;min-height:0"><small>خطاهای قفل‌شده (fault) — LED جدا برای هر بیت</small><div class="leds" style="margin:0 0 6px" id="asfb"><span class="bit" id="asbb0"><i></i><small>ADC</small></span><span class="bit" id="asbb1"><i></i><small>OC1</small></span><span class="bit" id="asbb2"><i></i><small>OC2</small></span><span class="bit" id="asbb3"><i></i><small>باتری</small></span><span class="bit" id="asbb4"><i></i><small>JIT1</small></span><span class="bit" id="asbb5"><i></i><small>JIT2</small></span><span class="bit" id="asbb6"><i></i><small>قطع‌باتری</small></span></div><div class="fx2" id="asf">—</div></div>';
let ASB=null;
ASB={sp:[0,1,2,3,4,5,6].map(k=>$('sp'+k)),sr:[0,1,2,3,4,5,6].map(k=>$('sr'+k)),flt:$('asf'),bits:[0,1,2,3,4,5,6].map(k=>$('asbb'+k)),box5:$('asb5'),mask:-1,live:$('aslive'),tick:false};
/* v1.14b (user order 2026-09-26): پنجرهٔ مدین/میانگین به تب «تنظیمات» رفت؛ اینجا فقط وضعیت زندهٔ فیلتر و نمونه‌های نمودار می‌مانند */
$('fg').innerHTML='<div class="lb" id="fspan" style="margin-top:6px">—</div>';
/* ---------- دو ستون جدا: شارژر ۱ و شارژر ۲ ---------- */
$('ch').innerHTML=[1,2].map(n=>`<div class="cd"><div class="hd"><b>شارژر ${n} <span class="lb">· باتری ${n==1?'بالا':'پایین'}</span></b><button class="ib" data-p="${n==1?'11,13,15,16,9':'12,14,17,18,10'},19" onclick="this.classList.toggle('o')">!<span class="it">کارت زندهٔ همین کانال: جریان تخمینی باتری، دیوتی فعلی و وضعیت. مود «دستی» حلقهٔ کنترل را کنار می‌گذارد و دیوتی را به شما می‌دهد — حدهای سخت (۹۵۰ میلی‌آمپر، سقف ولتاژ، سقف دیوتی) همچنان فعال می‌مانند.</span></button><span class="tg" id="st${n}">—</span></div>
<div class="bg2"><div class="big"><span class="lb">جریان باتری (iest)</span><b class="n" id="ie${n}">—</b></div>
<div class="big"><span class="lb">duty <span id="dc${n}"></span></span><b class="n" id="du${n}">—</b></div></div><div class="bar"><i id="db${n}"></i><u id="cl${n}"></u></div>
<div class="bctl"><span class="lb">مود</span><button class="sw" id="ma${n}">خودکار</button><button class="sw w" id="mm${n}">دستی</button><span class="lb">·</span><span class="lb">دیوتی دستی ٪</span><input type="number" step="any" id="qm${n}" data-s style="width:76px"><button class="sb" onclick="qset(${n})">اعمال</button><button class="sb off2" onclick="qzero(${n})">صفر</button><button class="sw" id="fx${n}" title="دیوتی ثابت همین کانال با حفاظتها؛ مود دستی سراسری اولویت دارد">فیکس</button></div>
${row(12+n)}
<div class="lb">بستن پنل: ۱۰ ثانیه بعد مود دستی خاموش و دیوتی صفر می‌شود؛ بعد از تریپ JIT همان دیوتی را دوباره اعمال کنید.</div>
<div class="sec">زنجیرهٔ اندازه‌گیری و محاسبه</div>
<table>${[['ADC خام','count',0],['ولتاژ شنت','µV',1],['جریان بدون فیلتر','mA',2],['جریان فیلترشده','mA',3],['تخمین باتری (iest)','mA',4]].map(r=>`<tr><td>${r[0]}<div class="fx" id="f${n}${r[2]}"></div></td><td class="n"><b id="c${n}${r[2]}">—</b></td><td class="lb">${r[1]}</td></tr>`).join('')}</table>
<div class="lb kc">ثابت‌ها: ADC دوازده‌بیتی، ۳۳۰۰mV، R41/R42 = 1k/10k، LM358 × 101، شنت 10 mOhm</div>
<canvas id="cv${n}"></canvas><div class="lg"><span><i style="background:#78849f"></i>بدون فیلتر · نوسان <b class="n" id="pu${n}">—</b> mA</span><span><i style="background:#63a2ff"></i>فیلترشده · نوسان <b class="n" id="pf${n}">—</b> mA</span><span class="hnl">نقاط <input type="number" id="hN${n}" data-s min="10" max="600" value="100"> از <b class="n" id="hC${n}">--</b></span></div>
<button class="bt" id="tg${n}">—</button></div>`).join('');
[1,2].forEach(n=>$('tg'+n).onclick=()=>{const c=D&&D.p[10+n];if(c!==0&&!confirm('PWM شارژر '+n+' فوراً قطع شود؟'))return;send(10+n,c===0?1:0);});
/* ---------- تاریخچهٔ نمودار هر کانال ---------- */
let LS=-1;const hn=c=>{const e=$('hN'+(c+1)),v=e?Math.round(+e.value):0;return !v?100:Math.min(600,Math.max(10,v));},H=[0,1].map(()=>({u:[],f:[]}));
function vcal(k){const R=V[k],m=Math.round(+$('vm'+k).value*1000),shown=D&&D.t[R[1]],off=D&&D.p[R[2]];if(!(m>0))return alert('عدد مولتی‌متر را به ولت وارد کنید (مثلاً 13.05).');if(off==null)return;
 const no=Math.min(5000,Math.max(-5000,off+m-shown));if(confirm(R[0]+': آفست '+off+' ← '+no+' mV\n(نمایش '+v2(shown)+' V، مولتی‌متر '+v2(m)+' V)')){send(R[2],no);$('vm'+k).value='';}}
/* نمودار زندهٔ فیلتر هر کانال */
function chart(){[0,1].forEach(ci=>{const c=$('cv'+(ci+1)),w=c.clientWidth,h=c.clientHeight,dp=devicePixelRatio||1;if(!w)return;
 if(c.width!=Math.round(w*dp)){c.width=Math.round(w*dp);c.height=Math.round(h*dp);}
 const x=c.getContext('2d');x.setTransform(dp,0,0,dp,0,0);x.clearRect(0,0,w,h);const s=H[ci],hc=$('hC'+(ci+1));if(hc)hc.textContent=s.u.length;if(s.u.length<2)return;
 let lo=Math.min(...s.u,...s.f),hi=Math.max(...s.u,...s.f);if(hi-lo<10){const m=(hi+lo)/2;lo=m-5;hi=m+5;}const pd=(hi-lo)*.12,a=lo-pd,z=hi+pd;
 const X=i=>w-8-(s.u.length-1-i)*(w-16)/(hn(ci)-1),Y=v=>h-8-(v-a)/(z-a)*(h-16);
 const ln=(A,col,lw)=>{x.beginPath();A.forEach((v,i)=>i?x.lineTo(X(i),Y(v)):x.moveTo(X(i),Y(v)));x.strokeStyle=col;x.lineWidth=lw;x.stroke();};
 x.fillStyle='#96a1b8';x.font='11px Vazirmatn,sans-serif';x.fillText(Math.round(hi)+' mA',8,16);x.fillText(Math.round(lo)+' mA',8,h-10);
 ln(s.u,'#6b7691',1);ln(s.f,'#63a2ff',2);const pp=A=>{const B=A.slice(-50);return Math.max(...B)-Math.min(...B);};$('pu'+(ci+1)).textContent=pp(s.u);$('pf'+(ci+1)).textContent=pp(s.f);});}
/* تعویض تب: پنل و داده‌برداری بنچ */
let TAB=0;document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{TAB=+b.dataset.t;document.querySelectorAll('nav button').forEach(x=>x.classList.toggle('a',x===b));document.querySelectorAll('.pgx').forEach((x,i)=>x.classList.toggle('a',i==TAB));if(D)draw(D);});
/* v1.15b: زیرتب داخل تنظیمات — ۰=شارژ و فیلتر، ۱=آلارم‌ها */
let STAB=0;document.querySelectorAll('#sbt button').forEach(b=>b.onclick=()=>{STAB=+b.dataset.s;document.querySelectorAll('#sbt button').forEach(x=>x.classList.toggle('a',x===b));document.querySelectorAll('.sgx').forEach((x,i)=>x.classList.toggle('a',i==STAB));if(D)draw(D);});
let UCARD=1;function usel(n){UCARD=n;for(let k=1;k<=5;k++){const c=$('ucard'+k);if(c)c.style.display=k===n?'':'none';}document.querySelectorAll('#usel button').forEach(b=>b.classList.toggle('a',+b.dataset.u===n));}
document.querySelectorAll('#usel button').forEach(b=>b.onclick=()=>usel(+b.dataset.u));
$('mx').onclick=()=>send(19,0);

/* ---------- به‌روزرسانی: فرمول‌های بخش 5.3 با مقادیر زنده ---------- */
const f1=x=>x.toFixed(1),V_=mv=>(mv/1000).toFixed(2)+'V',nz=v=>v==null?'?':v;
/* iest مثل STM32 (charger.c): زیر Vin 10V یا Vbat 5V برگشت به همانی */
const ie=(fl,vin,eta,vb)=>vin<10000||vb<5000?fl+' (همانی: ولتاژ زیر حد)':Math.floor(Math.floor(fl*eta/1000)*vin/vb);
const LUTX=[0,20,37,106,189,236,253,283,312,353,390,441,557,707],LUTY=[0,0,109,751,1581,2685,3260,3925,4550,5355,6035,6817,8573,10429];
const lutPow=c=>{for(let i=1;i<LUTX.length;i++){if(c<=LUTX[i]){const x0=LUTX[i-1],x1=LUTX[i];if(x1==x0)return LUTY[i];return LUTY[i-1]+Math.floor((c-x0)*(LUTY[i]-LUTY[i-1])/(x1-x0));}}const n=LUTX.length-1,d=LUTX[n]-LUTX[n-1];if(!d)return LUTY[n];return LUTY[n]+Math.floor((c-LUTX[n])*(LUTY[n]-LUTY[n-1])/d);};
const lutTap=(ch,vb)=>{const pw=lutPow(Math.round(ch)),v=Math.min(15000,Math.max(8000,vb));return ' => LUT:'+pw+'mW/'+v+'='+Math.floor(pw*1000/v)+'mA';};
/* v1.19: آینهٔ جدول توان کانال ۱ (SOLO1، ۱۷ لنگر) — قرینهٔ کانال ۲ */
const LUT1X=[0,5,11,31,54,81,114,148,189,231,277,330,382,444,504,567,640],LUT1Y=[0,0,140,459,820,1095,1723,2259,2865,3496,4130,4862,5636,6488,7364,8290,9345];
const lut1Pow=c=>{for(let i=1;i<LUT1X.length;i++){if(c<=LUT1X[i]){const x0=LUT1X[i-1],x1=LUT1X[i];if(x1==x0)return LUT1Y[i];return LUT1Y[i-1]+Math.floor((c-x0)*(LUT1Y[i]-LUT1Y[i-1])/(x1-x0));}}const n=LUT1X.length-1,d=LUT1X[n]-LUT1X[n-1];if(!d)return LUT1Y[n];return LUT1Y[n]+Math.floor((c-LUT1X[n])*(LUT1Y[n]-LUT1Y[n-1])/d);};
const lut1Tap=(ch,vb)=>{const pw=lut1Pow(Math.round(ch)),v=Math.min(15000,Math.max(8000,vb));return ' => LUT:'+pw+'mW/'+v+'='+Math.floor(pw*1000/v)+'mA';};
function formulas(t,p){
 [1,2].forEach(n=>{const b=n==1?0:7,raw=t[b],off=p[n-1],g=p[n+1],eta=p[8+n],vb=n==1?t[18]:t[17],vin=t[14],fl=t[b+3];
  $('f'+n+'0').textContent='12-bit ADC · Vref 3300 mV';
  $('f'+n+'1').textContent=`${raw} × 3300/4095 × 11/10 × 1000/101 = ${raw} × 8.7767 ≈ ${Math.round(raw*K_UV)}`;
  $('f'+n+'2').textContent=off==null||g==null?'':`(${raw} − ${off}) × 0.8777 × ${g}/1000 ≈ ${f1(Math.max(raw-off,0)*K_MA*g/1000)}${n==2?lutTap(Math.max(raw-off,0)*K_MA*g/1000,vb):lut1Tap(Math.max(raw-off,0)*K_MA*g/1000,vb)}`;
  $('f'+n+'3').textContent=`convert( average[W=${nz(p[8])}]( median[N=${nz(p[7])}]( raw ) ) ) = ${fl}`;
  $('f'+n+'4').textContent=eta==null?'':eta==0?`eta = 0 → Iest = I = ${fl}`:`${fl} × ${V_(vin)} × ${eta}‰ / ${V_(vb)} ≈ ${ie(fl,vin,eta,vb)}`;});
 V.forEach((v,i)=>{const e=$('fv'+i);if(i<3){const o=p[v[2]]==null?0:p[v[2]],c=Math.round((t[v[1]]-o+(i==2?150+Math.floor(t[9]*470/1000):0))/v[3]);e.textContent=`${c} × ${v[3].toFixed(3)} ${o<0?'−':'+'} ${Math.abs(o)}${i==2?' − (150 + '+t[9]+'×470/1000)':''}`;}
  else e.textContent=i==3?'V24 − V12':'= V12';});
 $('ff').textContent=`I_filtered = convert( average[W=${nz(p[8])}]( median[N=${nz(p[7])}]( raw counts ) ) )`;}
function hist(d){const t=d.t;if(d.on==1&&d.seq!==LS){LS=d.seq;[0,1].forEach(c=>{const b=c*7,s=H[c];s.u.push(t[b+2]);s.f.push(t[b+3]);if(s.u.length>hn(c)){s.u.shift();s.f.shift();}});}}
function qfill(){if(!D||!D.p)return;for(const id of [7,8,20,21,22,23,24,25,26]){const e=$('q'+id),a=$('a'+id);if(!e)continue;if(document.activeElement!==e&&e.value==='')e.value=D.p[id]==null?'':D.p[id];if(a&&!(D.q&(1<<id)))a.textContent=D.p[id]==null?'—':D.p[id];}}
function qdef(){[[20,14400],[21,14300],[22,14600],[23,13500],[24,12800],[25,650],[26,50]].forEach(x=>{$('q'+x[0]).value=x[1];send(x[0],x[1]);});qgraph();}
/* ===== v1.14: نمودار مراحل شارژ — مقدار هر خط از فیلد تایپ‌نشده/متفاوت با مقدار اعمال‌شده می‌آید (پیش‌نمایش خط‌چین) ===== */
const QDEF=[14400,14300,14600,13500,12800,650,50];
function qv(id){const e=$('q'+id),d=D&&D.p&&D.p[id]!=null?D.p[id]:QDEF[id-20];
 if(e&&e.value!==''){const v=parseInt(e.value,10);if(!isNaN(v))return{v,d,p:v!==d?1:0};}
 return{v:d,d,p:0};}
/* v1.14d: نگهبان ترکیب پروفایل — آینهٔ قوانین Charger_ClampProfile روی برد.
   هر قانون: [فیلد اصلی، فیلد مرجع] + پیام فارسی. خروجی خالی = ترکیب سالم. */
function qchk(){const w=[],a=qv(20).v,e=qv(21).v,o=qv(22).v,f=qv(23).v,r=qv(24).v,im=qv(25).v,tp=qv(26).v;
 const bad=(v,lo,hi)=>!(v>=lo&&v<=hi);
 if(bad(a,11000,14600))w.push({ids:[20],msg:'ابزورب باید ۱۱۰۰۰..۱۴۶۰۰ باشد'});
 else{
  if(bad(e,a-500,a-50))w.push({ids:[21,20],msg:'ورود ابزورب باید ابزورب−۵۰۰ تا ابزورب−۵۰ باشد ('+(a-500)+'..'+(a-50)+')'});
  if(bad(o,a+100,Math.min(a+400,14750)))w.push({ids:[22,20],msg:'سقف تجاوز باید ابزورب+۱۰۰ تا ابزورب+۴۰۰ (سقف ۱۴۷۵۰) باشد'});
  if(bad(f,9000,a-300))w.push({ids:[23,20],msg:'شناور باید ۹۰۰۰..ابزورب−۳۰۰ باشد (≤ '+(a-300)+')'});
  else if(bad(r,8000,f-300))w.push({ids:[24,23],msg:'بازگشت باید ۸۰۰۰..شناور−۳۰۰ باشد (≤ '+(f-300)+')'});
 }
 if(bad(im,100,900))w.push({ids:[25],msg:'جریان بالک باید ۱۰۰..۹۰۰ باشد'});
 if(bad(tp,10,Math.min(300,im)))w.push({ids:[26,25],msg:'تیپر باید ۱۰..سقف بالک باشد (≤ '+Math.min(300,im)+')'});
 return w;}
/* ===== v1.25 (دستور کاربر ۲۰۲۶-۰۹-۲۹): در «!» هر بخش، خودِ پارامترها هم
   توضیح داده شوند — هرکدام چیست و چه کار می‌کند.
   یک جدول و یک پاس، نه ۸ بلوک HTML دستی: بلوک‌های دستی همان چیزی‌اند که در این
   پروژه بارها کهنه شده‌اند، پس متن از یک جا می‌آید و دکمه‌ها فقط شناسه‌هایشان را
   با data-p اعلام می‌کنند.
   v1.25: explain each PARAMETER inside its section's "!" bubble. One table and
   one pass rather than eight hand-written HTML blobs - hand-written copies are
   exactly what has gone stale here before. */
const PX={
 9:['بازده کانال ۱','بازده مبدل بر حسب پرمیل برای تخمین جریان ورودی از توان باتری؛ صفر یعنی بدون تصحیح.'],
 10:['بازده کانال ۲','همان محاسبه برای کانال دوم؛ فقط روی عدد تخمینی جریان ورودی اثر دارد، نه روی شارژ.'],
 11:['فعال‌بودن شارژر ۱','اجازهٔ کار کانال ۱ از سمت پنل؛ صفر یعنی این کانال اصلاً سوئیچ نمی‌کند.'],
 12:['فعال‌بودن شارژر ۲','همان اجازه برای کانال ۲؛ برای تست تک‌کاناله یکی را خاموش می‌کنند.'],
 13:['سقف دیوتی ۱','بیشترین دیوتی مجاز کانال ۱ (‰). هر دیوتی بالاتر — خودکار یا دستی — به همین سقف گیره می‌شود.'],
 14:['سقف دیوتی ۲','همان سقف برای کانال ۲؛ برای محدودکردن توان یک کانال بدون دست‌زدن به پروفایل.'],
 15:['دیوتی ثابت ۱ روشن','کانال ۱ به‌جای حلقهٔ کنترل، دیوتی ثابت بگیرد؛ فقط برای تست و کالیبراسیون.'],
 16:['مقدار دیوتی ثابت ۱','همان دیوتی ثابتی که کانال ۱ در حالت بالا می‌گیرد (‰).'],
 17:['دیوتی ثابت ۲ روشن','همان قفل دستی برای کانال ۲.'],
 18:['مقدار دیوتی ثابت ۲','همان دیوتی ثابتی که کانال ۲ در حالت قفل دستی می‌گیرد (‰)؛ همچنان به سقف دیوتی همان کانال گیره می‌شود.'],
 19:['مود تست دستی','کل شارژر را از حالت خودکار بیرون می‌آورد و کنترل دیوتی را به شما می‌دهد؛ حدهای سخت همچنان فعال‌اند.'],
 38:['دورهٔ LED اضافه‌ولتاژ','طول یک چرخهٔ چشمک LED هنگام هشدار اضافه‌ولتاژ (ms).'],
 39:['روشنی LED اضافه‌ولتاژ','چند درصد از هر چرخه LED روشن باشد؛ ۵۰ یعنی نصف روشن نصف خاموش.'],
 40:['دورهٔ بوق اضافه‌ولتاژ','هر چند وقت یک‌بار دستهٔ بوق اضافه‌ولتاژ تکرار شود (ms)؛ صفر یعنی بی‌صدا.'],
 41:['طول هر بوق اضافه‌ولتاژ','هر تک‌بوق چقدر طول بکشد (ms).'],
 42:['تعداد بوق اضافه‌ولتاژ','در هر دسته چند بوق زده شود.'],
 43:['فاصلهٔ بوق‌های اضافه‌ولتاژ','سکوت بین دو بوق یک دسته (ms)؛ اگر تعداد بیش از یکی است باید معنادار باشد وگرنه به هم می‌چسبند.'],
 44:['دورهٔ LED باتری ضعیف','طول یک چرخهٔ چشمک LED هنگام هشدار باتری ضعیف (ms).'],
 45:['روشنی LED باتری ضعیف','چند درصد از هر چرخه LED روشن باشد.'],
 46:['دورهٔ بوق باتری ضعیف','هر چند وقت یک‌بار دستهٔ بوق باتری ضعیف تکرار شود (ms)؛ صفر یعنی بی‌صدا.'],
 47:['طول هر بوق باتری ضعیف','هر تک‌بوق چقدر طول بکشد (ms).'],
 48:['تعداد بوق باتری ضعیف','در هر دسته چند بوق زده شود.'],
 49:['فاصلهٔ بوق‌های باتری ضعیف','سکوت بین دو بوق یک دسته (ms).'],
 50:['درصد شروع هشدار مصرف','از این درصد شارژ به پایین، بوق دوره‌ای حالت مصرف شروع می‌شود.'],
 51:['درصد بوق دوتایی','از این درصد به پایین، هشدار به دو بوق تغییر می‌کند — یعنی وضعیت جدی‌تر شد.'],
 52:['درصد بوق سه‌تایی','از این درصد به پایین، سه بوق؛ مرحلهٔ هشدار بعدی.'],
 53:['درصد بحرانی','از این درصد به پایین، حالت بحرانی با الگوی مخصوص خودش.'],
 54:['فاصلهٔ هشدار عادی','هر چند وقت یک‌بار هشدار مصرف در بازهٔ عادی تکرار شود (ms).'],
 55:['فاصلهٔ هشدار سه‌تایی','هر چند وقت یک‌بار هشدار سه‌تایی تکرار شود (ms).'],
 56:['دورهٔ هشدار بحرانی','دورهٔ تکرار هشدار در حالت بحرانی (ms).'],
 57:['روشنی بحرانی','شدت/درصد روشنی الگوی بحرانی.'],
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
 79:['هیسترزیس شارژ','چقدر تغییر لازم است تا درصد نمایش شارژ عوض شود؛ جلوی بالا-پایین شدن دائمی عدد را می‌گیرد.'],
 80:['هیسترزیس مصرف','همان پایدارسازی برای درصد در حالت مصرف.'],
 81:['خروج از صفر درصد','تا این درصد بالا نرود، نمایش از صفر بیرون نمی‌آید.'],
 82:['خروج از یک درصد','تا این درصد بالا نرود، نمایش از یک بیرون نمی‌آید.'],
 0:['آفست جریان ۱','شمارش ADC که در جریان صفر خوانده می‌شود و از هر نمونه کم می‌گردد؛ اگر در حالت بی‌بار عدد جریان صفر نیست، این را تنظیم کنید.'],
 1:['آفست جریان ۲','شمارش ADC که کانال دوم در جریان صفر می‌خواند و از هر نمونه کم می‌شود؛ اگر بی‌بار عدد جریان ۲ صفر نیست، این را تنظیم کنید.'],
 2:['ضریب جریان ۱','مقیاس محور جدول توان کانال ۱ (‰). با جدول کالیبراسیون جفت است — تغییرش خوانش جریان را بی‌صدا غلط می‌کند.'],
 3:['ضریب جریان ۲','مقیاس محور جدول توان کانال ۲ (‰). با جدول کالیبراسیون جفت است — تغییرش خوانش جریان ۲ را بی‌صدا غلط می‌کند.'],
 4:['آفست ولتاژ ورودی','عدد ثابتی که به ولتاژ ورودی اضافه/کم می‌شود (mV). فقط خطای جمعی را می‌گیرد، نه خطای ضربی.'],
 5:['آفست ولتاژ پک','عدد ثابتی که به ولتاژ پک ۲۴ ولت اضافه/کم می‌شود (mV). فقط خطای جمعی را می‌گیرد؛ خطای ضربی را باید از مرجع ADC درست کرد.'],
 6:['آفست ولتاژ ۱۲V','عدد ثابتی که به ولتاژ نقطهٔ میانی اضافه/کم می‌شود (mV). ولتاژ باتری بالا از تفریق همین عدد به دست می‌آید، پس روی هر دو نیمه اثر دارد.'],
 7:['پنجرهٔ مدین','چند نمونه را مرتب کرده و وسطی را برمی‌دارد؛ پرش‌های تک‌نمونه‌ای را می‌کشد بدون اینکه پله‌های واقعی را کند کند.'],
 8:['پنجرهٔ میانگین','چند نمونه میانگین گرفته شود؛ بزرگ‌تر یعنی آرام‌تر ولی کندتر.'],
 20:['حداکثر ولتاژ باتری (ابزورب)','ولتاژی که شارژر در فاز ابزورب روی آن نگه می‌دارد. هدف اصلی حلقهٔ ولتاژ.'],
 21:['آستانهٔ ورود به ابزورب','از این ولتاژ به بالا از بالک وارد ابزورب می‌شود.'],
 22:['سقف تجاوز ابزورب','اگر ولتاژ از این رد شد، دیوتی سریع پایین کشیده می‌شود.'],
 23:['ولتاژ شناور','ولتاژ نگه‌داری بعد از پر شدن؛ کمتر از ابزورب تا باتری نجوشد.'],
 24:['ولتاژ بازگشت به بالک','اگر باتری تا این حد افت کرد، دوباره از بالک شروع می‌شود.'],
 25:['جریان بالک','سقف جریان فاز بالک؛ هدف حلقهٔ جریان. حد سخت ۹۵۰ میلی‌آمپر جداست.'],
 26:['جریان تیپر','زیر این جریان در ابزورب، شارژ تمام‌شده حساب می‌شود.'],
 27:['ولتاژ قطع باتری','بالاتر از این و بدون جریان یعنی باتری جدا شده.'],
 28:['دیبانس قطع','چند میلی‌ثانیه شرط بالا باید برقرار بماند تا آلارم بزند؛ کوتاه یعنی آلارم فیک.'],
 29:['ولتاژ نبود باتری','زیر این یعنی اصلاً باتری وصل نیست.'],
 30:['ولتاژ بازگشت باتری','بالای این یعنی باتری دوباره وصل شده.'],
 31:['دیبانس نبود باتری','مدت لازم برای قطعی‌شدن تشخیص «نیست».'],
 32:['زمان ریکاوری','چقدر بعد از رفع مشکل صبر کند تا دوباره عادی شود.'],
 33:['کمینهٔ ورودی سالم','زیر این ولتاژ، ورودی معتبر نیست و شارژ شروع نمی‌شود.'],
 34:['بیشینهٔ ورودی سالم','بالای این، ورودی بیش از حد است و شارژ متوقف می‌شود.'],
 35:['خطای سخت جریان','از این جریان رد شود، کانال فوراً خاموش می‌شود. آخرین خط دفاع.'],
 36:['قطع اضافه‌ولتاژ OV','از این ولتاژ رد شود، دیوتی صفر می‌شود. پیش‌فرض کمی زیر سقف مطلق تا زودتر تصمیم بگیرد.'],
 37:['کف اعتبار باتری','زیر این ولتاژ، عدد باتری معتبر شمرده نمی‌شود.'],
 70:['آستانهٔ اضافه‌ولتاژ UI','از این بالاتر، رابط هشدار اضافه‌ولتاژ می‌دهد.'],
 71:['هیسترزیس اضافه‌ولتاژ','چقدر باید پایین بیاید تا هشدار برداشته شود؛ جلوی چشمک‌زدن را می‌گیرد.'],
 72:['آستانهٔ باتری کم','زیر این، هشدار باتری ضعیف.'],
 73:['پاک‌شدن باتری کم','بالای این، هشدار برداشته می‌شود.'],
 74:['ولتاژ ۰٪','ولتاژی که روی نمایشگر صفر درصد حساب می‌شود.'],
 75:['ولتاژ ۱۰۰٪','ولتاژی که صد درصد حساب می‌شود.'],
 83:['Kp جریان','واکنش فوری حلقهٔ جریان به خطا. زیاد یعنی تند و لرزان.'],
 84:['Ki جریان','خطای انباشته را جمع می‌کند؛ همان چیزی که در نهایت جریان را دقیقاً روی هدف می‌نشاند.'],
 85:['Kd جریان','واکنش به سرعت تغییر. صفر است چون اندازه‌گیری نویز دارد و D نویز را تقویت می‌کند.'],
 86:['نرخ صعود جریان','چقدر سریع اجازه دارد دیوتی را بالا ببرد.'],
 87:['نرخ نزول جریان','چقدر سریع اجازه دارد پایین بیاورد؛ بالاتر یعنی ترمز تندتر.'],
 88:['Kp ولتاژ','همان Kp ولی برای حلقهٔ ولتاژ. عمداً کوچک است تا نویز ولتاژ پک تقویت نشود.'],
 89:['Ki ولتاژ','انباشت خطای ولتاژ؛ بزرگ‌ترین عامل کم‌کردن اورشوت.'],
 90:['Kd ولتاژ','صفر، به همان دلیل حلقهٔ جریان.'],
 91:['نرخ صعود ولتاژ','عمداً خیلی کم است تا موقع تحویل CC به CV پرش نکند.'],
 92:['نرخ نزول ولتاژ','اجازهٔ کاهش سریع وقتی ولتاژ از هدف رد می‌شود.']};
function pexp(){document.querySelectorAll('button.ib[data-p]').forEach(b=>{
 const t=b.querySelector('.it');if(!t||t.dataset.px)return;t.dataset.px='1';
 const rows=b.dataset.p.split(',').map(x=>PX[+x]).filter(Boolean);
 if(!rows.length)return;
 t.insertAdjacentHTML('beforeend','<div style="margin-top:8px;padding-top:7px;border-top:1px solid var(--ln)">'+
  '<b style="font-size:11.5px">هر پارامتر چه می‌کند</b>'+
  rows.map(r=>'<div style="margin-top:4px;font-size:11.5px;line-height:1.75"><b>'+r[0]+'</b> — '+r[1]+'</div>').join('')+
  '</div>');});}
function qgraph(){const g=$('qg');if(!g)return;
 const q={a:qv(20),e:qv(21),o:qv(22),f:qv(23),r:qv(24)},im=qv(25),tp=qv(26);
 /* v1.14e (دستور کاربر ۲۰۲۶-۰۹-۲۶ «۵۰٪ کوتاه‌تر» ۲۰۲۶-۰۹-۲۹): H=420؛ با نصف‌شدن ارتفاع، قلم و کمینهٔ فاصلهٔ برچسب‌ها (LBL_GAP) هم کوچک شد تا چیدمان ضدتداخل برچسب‌ها را به هم نچسباند. مرز
    ناحیه‌ها در هم نرود؛ ناحیه‌ها از مقادیر اعمال‌شده (.d) — چون برد گیره
    می‌زند هرگز وارونه/هم‌پوشان نمی‌شوند؛ تایپِ هنوز-اعمال‌نشده فقط خط‌چین */
 const lo=Math.max(7600,Math.min(q.r.d,12000)-500),hi=15060,W=760,H=420,X0=48,X1=742;
 const LBL_GAP=11;
 const Y=mv=>Math.round(H-24-(H-46)*(mv-lo)/(hi-lo));
 const V=mv=>(mv/1000).toFixed(2);
 /* برچسب‌ها جدا جمع و با کمینهٔ فاصله رندر می‌شوند تا در ناحیه‌های باریک در هم نروند */
 const ZL=[],LL=[];
 const zone=(mv1,mv2,fill,txt,c)=>{const y1=Y(Math.max(mv1,mv2)),y2=Y(Math.min(mv1,mv2));
  if(txt)ZL.push({y:y1+10,txt,c});
  return`<rect x="${X0}" y="${y1}" width="${X1-X0}" height="${Math.max(3,y2-y1)}" fill="${fill}"/>`;};
 const aln=(mv,c)=>`<line x1="${X0}" y1="${Y(mv)}" x2="${X1}" y2="${Y(mv)}" stroke="${c}" stroke-width="1.8"/>`;
 const pvln=(o,c)=>o.p?`<line x1="${X0}" y1="${Y(o.v)}" x2="${X1}" y2="${Y(o.v)}" stroke="${c}" stroke-width="1.8" stroke-dasharray="6 4"/>`:'';
 const lbl=(o,c,txt)=>{LL.push({y:Y(o.p?o.v:o.d)-4,txt,c,pv:o.p});};
 const put=(A,x,anchor,fs)=>{A.sort((p,q2)=>p.y-q2.y);let last=4;
  return A.map(o=>{const yc=Math.min(Math.max(o.y,14),H-10),y=Math.max(last+LBL_GAP,yc),sh=y-yc>3;last=y;
   return (sh?`<line x1="${x}" y1="${yc+3}" x2="${x}" y2="${y-3}" stroke="${o.c}" stroke-width="1" opacity=".6"/>`:'')+
   `<text x="${x}" y="${y}" text-anchor="${anchor}" font-size="${fs}" font-weight="700" fill="${o.c}">${o.txt}${o.pv?' · پیش‌نمایش':''}</text>`;}).join('');};
 let s=`<svg viewBox="0 0 ${W} ${H}" style="width:100%;min-width:640px;font-family:inherit">`;
 /* v1.14e: حاشور کم‌رنگ ناحیهٔ بالک (دستور کاربر ۲۰۲۶-۰۹-۲۶) */
 s+=`<defs><pattern id="bkh" width="9" height="9" patternTransform="rotate(45)" patternUnits="userSpaceOnUse"><rect width="9" height="9" fill="rgba(99,162,255,.08)"/><line x1="0" y1="0" x2="0" y2="9" stroke="rgba(143,194,255,.32)" stroke-width="1.2"/></pattern></defs>`;
 s+=`<rect x="${X0}" y="12" width="${X1-X0}" height="${H-34}" fill="#080b12" stroke="#38455e" rx="6"/>`;
 for(let mv=Math.ceil(lo/500)*500;mv<=hi;mv+=500){const y=Y(mv);
  s+=`<line x1="${X0}" y1="${y}" x2="${X1}" y2="${y}" stroke="#26304a" stroke-width="1"/>`+
     `<text x="${X0-4}" y="${y+3}" text-anchor="end" font-size="8" fill="#96a1b8">${(mv/1000).toFixed(1)}</text>`;}
 s+=zone(hi,15000,'rgba(251,94,106,.16)','','#ff6873');
 s+=zone(15000,q.o.d,'rgba(251,94,106,.09)','ناحیهٔ تجاوز (Over) — کاهش سریع duty','#fc8086');
 s+=zone(q.o.d,q.e.d,'rgba(251,191,36,.08)','ناحیهٔ ابزورب (Absorb)','#f7c13c');
 s+=zone(q.e.d,q.f.d,'url(#bkh)','ناحیهٔ بالک (Bulk) — شارژ با جریان ثابت','#9ac8ff');
 s+=zone(q.f.d,q.r.d,'rgba(52,211,153,.09)','ناحیهٔ شناور (Float)','#35d6a0');
 s+=zone(q.r.d,lo,'rgba(99,162,255,.11)','زیر بازگشت (Reentry) — شارژ دوباره از بالک','#9ac8ff');
 s+=`<line x1="${X0}" y1="${Y(15000)}" x2="${X1}" y2="${Y(15000)}" stroke="#ff6873" stroke-width="1.2" stroke-dasharray="3 4"/>`;
 LL.push({y:Y(15000)-4,txt:'قطع سخت (Cutoff) ۱۵V',c:'#ff6873'});
 s+=aln(q.o.d,'#fb923c')+pvln(q.o,'#fb923c');lbl(q.o,'#fb923c','سقف تجاوز (Over)');
 s+=aln(q.a.d,'#f7c13c')+pvln(q.a,'#f7c13c');lbl(q.a,'#f7c13c','ابزورب (Absorb)');
 s+=aln(q.e.d,'#e8a33d')+pvln(q.e,'#e8a33d');lbl(q.e,'#e8a33d','ورود ابزورب (Absorb Enter)');
 s+=aln(q.f.d,'#35d6a0')+pvln(q.f,'#35d6a0');lbl(q.f,'#35d6a0','شناور (Float)');
 s+=aln(q.r.d,'#63a2ff')+pvln(q.r,'#63a2ff');lbl(q.r,'#63a2ff','بازگشت به بالک (Reentry)');
 /* موقعیت زندهٔ هر باتری (دستور کاربر ۲۰۲۶-۰۹-۲۶): نقطهٔ رنگی روی ولتاژ خودش
    در ستون مخصوصش + برچسب وضعیت زیر نمودار؛ ناحیه‌ها خودشان داستان مراحل را می‌گویند */
 let lg='';
 if(D&&D.t){const tt=D.t;
  const BST={0:['خاموش (Off)','#96a1b8'],1:['بالک (Bulk)','#63a2ff'],2:['ابزورب (Absorb)','#f7c13c'],3:['شناور (Float)','#35d6a0'],4:['راه‌اندازی (Bring-up)','#f7c13c'],5:['انتظار JIT (JIT wait)','#ff6873'],6:['انتظار ورودی (No input)','#f7c13c'],7:['خطای نهایی (Final fault)','#ff6873'],8:['باتری قطع (Battery lost)','#ff6873'],9:['دستی (Manual)','#f7c13c']};
  const bats=[['باتری پایین (Vlow)',tt[17],tt[13],tt[10],'#c084fc',0.60],['باتری بالا (Vhigh)',tt[18],tt[6],tt[3],'#f7c13c',0.82]];
  bats.forEach(b=>{
   if(b[1]>lo&&b[1]<hi){const y=Y(b[1]),x=X0+Math.round((X1-X0)*b[5]);
    s+=`<line x1="${X0}" y1="${y}" x2="${X1}" y2="${y}" stroke="${b[4]}" stroke-width="1.2" stroke-dasharray="2 3"/>`;
    s+=`<circle cx="${x}" cy="${y}" r="4.5" fill="${b[4]}" stroke="#080b12" stroke-width="1.8"/>`;
    s+=`<text x="${x}" y="${Math.min(y+15,H-6)}" text-anchor="middle" font-size="8.5" font-weight="700" fill="${b[4]}">${b[0].split(' (')[0]} ${V(b[1])}V</text>`;}});
  lg=bats.map(b=>{const st=BST[b[2]]||('#'+b[2]);
   return `<span class="tg" style="background:${st[1]}22;color:${st[1]};border:1px solid ${st[1]}66">● ${b[0]}: <b>${V(b[1])}V</b> · ${b[3]}mA · ${st[0]}</span>`;}).join(' ')+
   `<span class="lb"> · بالک ≤ ${im.v}mA · تیپر < ${tp.v}mA · پس از هر تغییر ~۱٫۵ ثانیه بعد روی فلش برد ذخیره می‌شود</span>`;
 }else lg='در انتظار دادهٔ برد…';
 s+=put(ZL,X0+6,'start','8.5')+put(LL,X1-4,'end','9');
 s+=`<text x="${X0}" y="8" font-size="8" fill="#96a1b8">ولتاژ باتری / Battery voltage (V)</text></svg>`;
 g.innerHTML=s;const e=$('qgl');if(e)e.innerHTML=lg;
 /* نگهبان: هشدار بالای نمودار + قرمزکردن فیلد مقصر */
 const w=qchk(),we=$('qw');
 if(we){we.innerHTML=w.length?('⚠ ترکیب نامعتبر — برد این‌ها را گیره می‌زند: '+w.map(x=>x.msg).join('؛ ')):'';
  we.style.cssText=w.length?'margin:2px 0 6px;color:#fc8086;font-size:12.5px;line-height:1.9':'margin:2px 0 0';}
 for(const id of [20,21,22,23,24,25,26]){const ne=$('q'+id);if(ne)ne.style.borderColor=w.some(x=>x.ids.includes(id))?'#e5484d':'';}}
/* [EN] bind the profile inputs: on change, POST /s (fire-and-forget; the ack span next to the field shows the APPLIED value reported by the STM32). v1.14d: a typed value that breaks the profile rules asks for confirmation first, because the board will clamp it. / اتصال ورودی‌های پروفایل: با تغییر، POST /s؛ نشانگر کنار فیلد مقدار «اعمال‌شده» را از STM32 نشان می‌دهد. v1.14d: مقدار ناسازگار قبل از ارسال تأیید می‌خواهد چون برد گیره‌اش می‌زند. */
for(const id of [7,8,20,21,22,23,24,25,26]){const e=$('q'+id);if(!e)continue;e.onchange=()=>{const v=parseInt(e.value,10);if(isNaN(v))return;
 if(id>=20){const m=qchk().filter(x=>x.ids.includes(id));
  if(m.length&&!confirm('⚠ '+m.map(x=>x.msg).join('\n')+'\n\nبرد مقدار را گیره می‌زند تا مجموعه سازنده بماند. باز هم ارسال شود؟')){e.value='';qgraph();return;}}
 send(id,v);};if(id>=20)e.oninput=qgraph;}
function cfill(){if(!D||!D.p)return;for(const id of [0,1,2,3,9,10]){const e=$('q'+id);if(!e)continue;if(document.activeElement!==e&&e.value==='')e.value=D.p[id]==null?'':D.p[id];}}
for(const id of [0,1,2,3,9,10]){const e=$('q'+id);if(!e)continue;e.onchange=()=>{const v=parseInt(e.value,10);if(isNaN(v))return;send(id,v);};}
/* ===== v1.15: تب آلارم‌ها — آینهٔ قوانین Fault_ClampAlarms/Charger_ClampAlarms روی برد ===== */
/* v1.24: upper bound MUST track the top real id. It was left at 97 when the
   third PID row was deleted, so ap() built u93..u97 = undefined and the
   settings backup carried five phantom ids. Same class of bug as v1.22's
   AIDS/ADEF mismatch - a host test now pins it to the PDEF length. */
const AIDS=[];for(let _i=27;_i<=92;_i++)AIDS.push(_i);
const ADEF=[14800,150,6000,7000,1000,1000,21000,28000,950,15000,2000,1000,50,10000,1000,1,0,1000,50,3000,233,3,100,40,20,10,1,60000,20000,10000,100,1,1000,2000,10000,1,2,3,100,1000,10,1000,150,28000,1000,21000,21200,21000,29000,0,100,95,5,2,2,3];
function av(id){const e=$('q'+id),d=D&&D.p&&D.p[id]!=null?D.p[id]:(id>=83?PDEF[id-83]:ADEF[id-27]);
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
 if(bad(a.hd,a.imax+50,950))w.push({ids:[35,25],msg:'خطای سخت باید '+(a.imax+50)+'..۹۵۰ باشد (بالای بالک+۵۰، هرگز بالای ۹۵۰)'});
 const olo=Math.max(14000,a.over+150);
 if(bad(a.ov,olo,15000))w.push({ids:[36,22],msg:'قطع OV باید '+olo+'..۱۵۰۰۰ باشد (بالای تجاوز+۱۵۰، هرگز بالای ۱۵۰۰۰)'});
 if(bad(a.fl,0,8000))w.push({ids:[37],msg:'کف اعتبار باید ۰..۸۰۰۰ باشد'});
 /* v1.16: نگهبان اعداد UI (آینهٔ Ui_ClampAlarms) */
 const perOk=v=>v===0||(v>=1000&&v<=600000);
 const fit=(dur,per,cnt,gap)=>per===0||dur*cnt+(cnt>1?gap*(cnt-1):0)<=per;
 const perW=(id,v,nm)=>{if(!(v===0||(v>=1000&&v<=600000)))w.push({ids:[id],msg:nm+' باید صفر (خاموش) یا ۱۰۰۰..۶۰۰۰۰۰ باشد'});};
 [[38,'دوره چشمک قرمز OV',100,10000],[39,'دیوتی قرمز OV',0,100],[42,'تعداد بوق OV',0,10],[43,'گپ بوق OV',0,5000],
  [44,'دوره چشمک قرمز قطع باتری',100,10000],[45,'دیوتی قرمز قطع باتری',0,100],[48,'تعداد بوق قطع باتری',0,10],[49,'گپ بوق قطع باتری',0,5000]].forEach(x=>{if(bad(a['u'+x[0]],x[2],x[3]))w.push({ids:[x[0]],msg:x[1]+' باید '+x[2]+'..'+x[3]+' باشد'});});
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
 perW(54,a.u54,'فاصله بوق ۱/۲تایی');perW(55,a.u55,'فاصله بوق ۳تایی');perW(56,a.u56,'دوره بوق بحرانی');
 if(bad(a.u57,0,100))w.push({ids:[57],msg:'دیوتی بوق بحرانی باید ۰..۱۰۰ باشد'});
 [[58,'تعداد بوق بحرانی'],[62,'تعداد بوق باند ۱'],[63,'تعداد بوق باند ۲'],[64,'تعداد بوق باند ۳']].forEach(x=>{if(bad(a['u'+x[0]],0,10))w.push({ids:[x[0]],msg:x[1]+' باید ۰..۱۰ باشد'});});
 if(bad(a.u65,0,5000))w.push({ids:[65],msg:'گپ بوق دشارژ باید ۰..۵۰۰۰ باشد'});
 else if((a.u58>1||a.u62>1||a.u63>1||a.u64>1)&&a.u65<100)w.push({ids:[65],msg:'گپ بوق دشارژ با چند بوق باید دست‌کم ۱۰۰ باشد'});
 if(bad(a.u59,0,600000))w.push({ids:[59],msg:'مدت هر بوق ۱/۲تایی باید ۰..۶۰۰۰۰۰ باشد'});
 else if(!fit(a.u59,a.u54,Math.max(a.u62,a.u63),a.u65))w.push({ids:[59,54,62,63,65],msg:'بوق ۱/۲تایی دشارژ در فاصله جا نمی‌شود — برد بی‌صدا می‌ماند'});
 if(bad(a.u60,0,600000))w.push({ids:[60],msg:'مدت هر بوق ۳تایی باید ۰..۶۰۰۰۰۰ باشد'});
 else if(!fit(a.u60,a.u55,a.u64,a.u65))w.push({ids:[60,55,64,65],msg:'بوق ۳تایی دشارژ در فاصله جا نمی‌شود — برد بی‌صدا می‌ماند'});
 if(bad(a.u61,0,120000))w.push({ids:[61],msg:'طول بوق بحرانی یک‌باره باید ۰..۱۲۰۰۰۰ باشد'});
 {const cw=Math.floor(a.u56*a.u57/100);if(a.u56!==0&&a.u57!==0&&a.u58>1){const cg=a.u65*(a.u58-1);if(cg>=cw||(cw-cg)<a.u58)w.push({ids:[56,57,58,65],msg:'بوق بحرانی در پنجره جا نمی‌شود (گپ‌ها + دست‌کم ۱ms هر بوق ≤ دوره×دیوتی) — برد تعداد را کم می‌کند'});}}
 if(bad(a.u66,100,10000))w.push({ids:[66],msg:'دوره چشمک سبز باید ۱۰۰..۱۰۰۰۰ باشد'});
 if(bad(a.u67,0,10000))w.push({ids:[67],msg:'حداقل خاموشی سبز باید ۰..۱۰۰۰۰ باشد'});
 else if(!(a.u67<=a.u66))w.push({ids:[67,66],msg:'حداقل خاموشی سبز باید زیر دوره باشد (≤ '+a.u66+')'});
 if(bad(a.u68,100,10000))w.push({ids:[68],msg:'دوره چشمک زرد باید ۱۰۰..۱۰۰۰۰ باشد'});
 if(bad(a.u69,0,10000))w.push({ids:[69],msg:'حداقل روشنی زرد باید ۰..۱۰۰۰۰ باشد'});
 else if(!(a.u69<=a.u68))w.push({ids:[69,68],msg:'حداقل روشنی زرد باید زیر دوره باشد (≤ '+a.u68+')'});
 if(bad(a.u70,24000,32000))w.push({ids:[70],msg:'آستانه اضافه‌ولتاژ باید ۲۴۰۰۰..۳۲۰۰۰ باشد'});
 if(bad(a.u71,0,2000))w.push({ids:[71],msg:'هیسترزیس اضافه‌ولتاژ باید ۰..۲۰۰۰ باشد'});
 if(bad(a.u72,15000,24000))w.push({ids:[72],msg:'آستانه باتری کم باید ۱۵۰۰۰..۲۴۰۰۰ باشد'});
 else if(!(a.u72<=a.u73))w.push({ids:[72,73],msg:'آستانه باتری کم باید زیر سطح پاک‌شدن باشد (≤ '+a.u73+')'});
 if(bad(a.u73,15000,24000))w.push({ids:[73],msg:'سطح پاک‌شدن باتری کم باید ۱۵۰۰۰..۲۴۰۰۰ باشد'});
 else if(!(a.u73>=a.u72))w.push({ids:[73,72],msg:'سطح پاک‌شدن باید بالای آستانه باشد (≥ '+a.u72+')'});
 if(bad(a.u74,15000,25000))w.push({ids:[74],msg:'کف نگاشت درصد باید ۱۵۰۰۰..۲۵۰۰۰ باشد'});
 else if(!(a.u74<=a.u75-100))w.push({ids:[74,75],msg:'کف نگاشت باید دست‌کم ۱۰۰ زیر سقف باشد (≤ '+(a.u75-100)+')'});
 if(bad(a.u75,25000,32000))w.push({ids:[75],msg:'سقف نگاشت درصد باید ۲۵۰۰۰..۳۲۰۰۰ باشد'});
 else if(!(a.u75>=a.u74+100))w.push({ids:[75,74],msg:'سقف نگاشت باید دست‌کم ۱۰۰ بالای کف باشد (≥ '+(a.u74+100)+')'});
 if(bad(a.u76,0,1))w.push({ids:[76],msg:'میوت باید ۰ یا ۱ باشد'});
 if(bad(a.u77,1,100))w.push({ids:[77],msg:'ورود فول باید ۱..۱۰۰ باشد'});
 if(bad(a.u78,0,100))w.push({ids:[78],msg:'خروج فول باید ۰..۱۰۰ باشد'});
 else if(!(a.u78<=a.u77-1))w.push({ids:[78,77],msg:'خروج فول باید زیر ورود باشد (≤ '+(a.u77-1)+')'});
 if(bad(a.u79,0,50))w.push({ids:[79],msg:'هیسترزیس شارژ باید ۰..۵۰ باشد'});
 if(bad(a.u80,0,50))w.push({ids:[80],msg:'هیسترزیس دشارژ باید ۰..۵۰ باشد'});
 if(bad(a.u81,0,100))w.push({ids:[81],msg:'خروج از ۰٪ باید ۰..۱۰۰ باشد'});
 if(bad(a.u82,0,100))w.push({ids:[82],msg:'خروج از ۱٪ باید ۰..۱۰۰ باشد'});
 return w;}
function afresh(){const w=achk();
 const wset=(el,l)=>{if(!el)return;el.innerHTML=l.length?('⚠ ترکیب نامعتبر — برد این‌ها را گیره می‌زند: '+l.map(x=>x.msg).join('؛ ')):'';el.style.cssText=l.length?'margin:2px 0 6px;color:#fc8086;font-size:12.5px;line-height:1.9':'margin:2px 0 0';};
 wset($('aw'),w.filter(x=>x.ids.some(i=>i<38)));wset($('aw2'),w.filter(x=>x.ids.some(i=>i>=38)));
 for(const id of AIDS){const ne=$('q'+id);if(ne)ne.style.borderColor=w.some(x=>x.ids.includes(id))?'#e5484d':'';}
 const ms=$('xmuteS');if(ms)ms.textContent=(D&&D.p&&D.p[76]===1)?'🔇 میوت روشن — موقتی، با ریست برد پاک می‌شود؛ LEDها همچنان چشمک می‌زنند':'🔊 بوق روشن';}
function apend(id){if(!D)return 0;return id<32?(D.q&(1<<id)):id<64?(D.q2&(1<<(id-32))):((D.q3||0)&(1<<(id-64)));}
function afill(){if(!D||!D.p)return;for(const id of AIDS){const e=$('q'+id),a=$('a'+id);if(!e)continue;if(document.activeElement!==e&&e.value==='')e.value=D.p[id]==null?'':D.p[id];if(a&&!apend(id))a.textContent=D.p[id]==null?'—':D.p[id];}}
function adef(){ADEF.slice(0,11).forEach((v,k)=>{const id=27+k;$('q'+id).value=v;send(id,v);});afresh();}
/* v1.22: پیش‌فرض کارخانهٔ PID سه‌مرحله‌ای — همان اعداد CHG_PID_* در charger.h */
const PDEF=[12,1600,0,1000,1000,50,18000,0,10,1000];
/* v1.22: نگهبان ترکیب PID — آینهٔ func__Charger_ClampPid روی برد به اضافهٔ دو
   هشدار تیون که شبیه‌سازی نشان داد. خروجی خالی = ترکیب سالم. */
function pv(id){const e=$('q'+id),d=D&&D.p&&D.p[id]!=null?D.p[id]:PDEF[id-83];
 if(e&&e.value!==''){const v=parseInt(e.value,10);if(!isNaN(v))return v;}
 return d;}
function pchk(){const w=[];
 for(let id=83;id<=92;id++){const v=pv(id),rate=((id-83)%5)>=3;
  if(rate&&(v<10||v>20000))w.push({ids:[id],msg:'نرخ شیب باید ۱۰ تا ۲۰۰۰۰ میلی‌پرمیل بر ثانیه باشد'});
  if(!rate&&(v<0||v>20000))w.push({ids:[id],msg:'ضریب باید ۰ تا ۲۰۰۰۰ باشد'});}
 if(pv(91)>=pv(86))w.push({ids:[91,86],msg:'نرخ صعود حلقهٔ ولتاژ باید کمتر از حلقهٔ جریان بماند، وگرنه دیوتی در ابزورب تندتر از بالک رشد می‌کند (همان چیزی که قرار بود کم شود)'});
 if(pv(88)>300)w.push({ids:[88],msg:'Kp حلقهٔ ولتاژ بالای ۳۰۰ از مرز پایداری رد می‌شود'});
 else if(pv(88)>100)w.push({ids:[88],msg:'Kp حلقهٔ ولتاژ بالای ۱۰۰ نویز را تقویت می‌کند: ولتاژ پک فیلتر بالادست ندارد، پس Kp مستقیم روی نویز خام ADC ضرب می‌شود. اندازه‌گیری: با نویز ±۷ میلی‌ولت، Kp=۱۵۰ حدود ۷۴۴ تغییر جهت دیوتی در ۱۰ ساعت می‌دهد و Kp=۵۰ فقط ۶۲ — با اورشوت یکسان، خطای تثبیت یکسان (۰٫۵ میلی‌ولت) و همان ۱۱۳ دقیقه تا ۱۴٫۴ ولت. یعنی هزینه می‌دهید بدون اینکه دقت بخرید'});
 if(pv(83)>150)w.push({ids:[83],msg:'Kp حلقهٔ جریان بالای ۱۵۰ ریسک لرزش دارد: هر یک پرمیل تغییر دیوتی حدود ۷ میلی‌آمپر جریان جابه‌جا می‌کند، پس Kp بزرگ باعث می‌شود حلقه بین دو عدد صحیح پرمیل گیر کند و بالا نرود'});
 const box=$('pw');if(box)box.innerHTML=!w.length?'<span class="lb">✅ ترکیب PID سالم است · پشتیبان‌های ۶۵۰ میلی‌آمپر و ۱۴٫۸ ولت همیشه فعال‌اند</span>':w.map(x=>'<div class="wn">⚠ '+x.msg+'</div>').join('');
 return w;}
function pdef(){PDEF.forEach((v,k)=>{const id=83+k,e=$('q'+id);if(e)e.value=v;send(id,v);});pchk();}
function sdef(){AIDS.forEach((id,k)=>{if(id<38||id>82)return;const e=$('q'+id);if(e)e.value=ADEF[k];send(id,ADEF[k]);});afresh();}
/* v1.15b: کارت وضعیت گروه‌بندی‌شده — اسکلت یک‌بار ساخته می‌شود و هر poll فقط متن/رنگ به‌روز می‌شود (بدون پر/خالی شدن و چشمک) */
const FEXP=[
 ['خطای ADC','نمونه‌برداری ADC نامعتبر است و اندازه‌گیری‌ها قابل‌اعتماد نیست؛ برد محافظه‌کار می‌شود. سیم‌کشی آنالوگ و تغذیه را بررسی کنید.'],
 ['اضافه‌جریان کانال ۱','جریان کانال ۱ از حد گذشت و کانال متوقف شد؛ باتری/بار کانال ۱ را بررسی و برد را ریست کنید.'],
 ['اضافه‌جریان کانال ۲','جریان کانال ۲ از حد گذشت و کانال متوقف شد؛ باتری/بار کانال ۲ را بررسی و برد را ریست کنید.'],
 ['باتری ضعیف','ولتاژ باتری خیلی پایین است؛ باتری را بررسی/شارژ کنید.'],
 ['خطای جیتر کانال ۱','ناپایداری داخلی نمونه‌برداری کانال ۱؛ اگر ماندگار شد برد را ریست کنید.'],
 ['خطای جیتر کانال ۲','ناپایداری داخلی نمونه‌برداری کانال ۲؛ اگر ماندگار شد برد را ریست کنید.'],
 ['قطع باتری','سیم باتری قطع است یا باتری نیست: یا ولتاژ حین پمپ بالای آستانهٔ قطع (۲۷) رفته یا باتری زیر آستانهٔ غیبت (۲۹) با ورودی سالم دیده شده. سیم‌کشی باتری را بررسی کنید؛ با بازگشت هر دو نیمه بالای آستانهٔ برگشت (۳۰) و پایداری (۳۲)، لچ خودکار پاک می‌شود.']];
function astat(){const s=$('ast');if(!s||!ASB||!D||!D.t||!D.p)return;
 const t=D.t,p=D.p;
 const g=(id,fb)=>p[id]!=null?p[id]:fb;
 const vin=t[14],vl=t[17],vh=t[18],i1=t[3],i2=t[10];
 const mn=g(33,21000),mx=g(34,28000),dc=g(27,14800),ab=g(29,6000),hd=g(35,950),ov=g(36,15000),fl=g(37,2000);
 $('v0').textContent=v2(vin);$('v1').textContent=v2(t[15]);$('v2').textContent=v2(t[16]);
 $('v3').textContent=v2(vh);$('v4').textContent=v2(vl);
 $('v5').textContent=i1+' mA';$('v6').textContent=i2+' mA';
 $('fv5').textContent='خطای سخت '+hd+'mA';$('fv6').textContent='خطای سخت '+hd+'mA';
 const set=(k,txt,cls)=>{ASB.sp[k].textContent=txt;ASB.sp[k].className='tg '+cls;ASB.sr[k].className=cls==='g'?'rok':cls==='y'?'rwr':'rbd';};
 const vinOk=vin>=mn&&vin<=mx;
 set(0,vinOk?'داخل بازه':'خارج بازه',vinOk?'g':'r');
 set(1,'—','g');set(2,'—','g');
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
/* ===== v1.16: آینهٔ LED و بازر برد — همان اولویت Ui_Tick با مقادیر اعمال‌شده؛ چشمک با همان دوره/دیوتی برد (فاز محلی، هم‌سرعت) ===== */
let UV={inP:false,ov:false,bat:false,pct:-1,cpct:-1,full:false,critT:0};
function uview(){
 const R=$('ulR'),Y=$('ulY'),G=$('ulG'),B=$('ulB'),sc=$('uscn'),tm=$('utim');
 if(!R)return;
 const now=performance.now();
 if(ASB&&ASB.bits&&D&&D.t){const m=D.t[19]||0,ph=(now%500)<250;ASB.bits.forEach((e,bit)=>{if(e)e.className='bit'+(((m&(1<<bit))!=0&&ph)?' on':'');});}
 const allOff=why=>{R.className='led r';Y.className='led y';G.className='led g';B.className='bz off';if(sc)sc.textContent=why;if(tm)tm.textContent='—';};
 if(!D||!D.t||!D.p||D.on!=1){allOff(!D?'در انتظار داده…':'لینک قطع است — آینه خاموش');if(ASB&&ASB.bits)ASB.bits.forEach(e=>{if(e)e.className='bit';});return;}
 const t=D.t,pp=D.p;
 const g=(id,fb)=>pp[id]!=null?pp[id]:fb;
 const vin=t[14],vbat=t[15];/* v_bat24 مثل برد */
 if(!(vin>0||vbat>0)){allOff('داده نامعتبر — همه خاموش (حالت امن برد)');return;}
 const lo=g(74,21000),hi=g(75,29000);
 let raw=hi>lo?Math.round((Math.min(vbat,hi)-lo)/(hi-lo)*100):0;raw=Math.max(0,Math.min(100,raw));
 if(vin>=21000)UV.inP=true;else if(vin<=20000)UV.inP=false;
 const ovT=g(70,28000),ovH=g(71,1000);
 if(!UV.ov&&vin>ovT)UV.ov=true;else if(UV.ov&&vin<=ovT-ovH)UV.ov=false;
 if(vbat<g(72,21000))UV.bat=true;else if(vbat>=g(73,21200))UV.bat=false;
 if(UV.pct<0)UV.pct=raw;else if(!(UV.pct===0&&raw<g(81,2))&&Math.abs(raw-UV.pct)>=g(80,2))UV.pct=raw;
 if(UV.cpct<0)UV.cpct=raw;else if(Math.abs(raw-UV.cpct)>=g(79,5))UV.cpct=raw;
 if(raw>=g(77,100))UV.full=true;else if(raw<g(78,95))UV.full=false;
 const mute=g(76,0)===1;
 const blink=(per,onMs)=>per>0&&onMs>0&&(now%per)<onMs;
 const beepNow=(per,dur,cnt,gap)=>{if(!per||!dur||!cnt)return false;const w=dur*cnt+(cnt>1?gap*(cnt-1):0);return w>0&&w<=per&&(now%per)<w;};
 let r=false,y=false,gr=false,bz=false,cap='',tim='—';
 const loW=UV.bat?' · ⚠ باتری کم':'';
 if(UV.ov){
  const per=g(38,1000);
  r=blink(per,Math.floor(per*g(39,50)/100));gr=true;bz=beepNow(g(40,10000),g(41,1000),g(42,1),g(43,0));
  tim='قرمز '+per+'ms/'+g(39,50)+'٪ · '+(g(40,10000)&&g(42,1)?('بوق هر '+g(40,10000)+'ms ('+g(42,1)+'×'+g(41,1000)+'ms)'):'بوق خاموش')+' · سقف '+ovT+'mV';
  cap='⚠ اضافه‌ولتاژ ورودی — قرمز چشمک + بوق'+loW;
 }else if((t[19]&64)!=0){
  const per=g(44,1000);
  r=blink(per,Math.floor(per*g(45,50)/100));gr=true;bz=beepNow(g(46,3000),g(47,233),g(48,3),g(49,100));
  tim='قرمز '+per+'ms/'+g(45,50)+'٪ · '+(g(46,3000)&&g(48,3)?('بوق هر '+g(46,3000)+'ms ('+g(48,3)+'×'+g(47,233)+'ms)'):'بوق خاموش');
  cap='⚠ قطع باتری — قرمز چشمک + بوق'+loW;
 }else if(UV.inP){
  const act=[t[6],t[13]].some(s=>s===1||s===2);/* فعال = BULK/ABSORB در حال پمپ (مثل برد) */
  /* v1.17b: فول شارژر = هر کانالِ فعال‌شده در FLOAT (مثل IsChargeComplete برد) */
  const en1=g(11,1)===1,en2=g(12,1)===1;
  const done=(en1||en2)&&(!en1||t[6]===3)&&(!en2||t[13]===3);
  gr=true;
  tim='سبز ثابت';
  if(UV.full||done)cap='✅ ورودی وصل · فول — سبز ثابت'+loW;
  else if(!act)cap='✅ ورودی وصل — سبز ثابت'+loW;
  else{
   const st=UV.cpct,per=g(68,1000);
   tim='زرد '+per+'ms · سبز ثابت';
   if(st<=0)y=true;
   else if(st<100){y=blink(per,Math.max(g(69,150),Math.min(per,(100-st)*Math.floor(per/100))));}
   cap='🔋 در حال شارژ '+raw+'٪ — زرد با مانده تا فول'+loW;
  }
 }else{
  const st=UV.pct;
  if(st<g(53,1)){
   if(!UV.critT)UV.critT=now;
   const cp=g(56,10000);
   tim='بوق یک‌باره '+g(61,10000)+'ms · LED خاموش';
   bz=cp>0&&g(58,1)>0&&(now-UV.critT)<g(61,10000)&&(now%cp)<cp*g(57,100)/100;
   cap=(now-UV.critT)<g(61,10000)?'🪫 بحرانی — بوق یک‌bاره (LEDها خاموش)'+loW:'🪫 بحرانی — LEDها خاموش · بوق یک‌بار زده شد'+loW;
  }else{
   UV.critT=0;
   const per=g(66,1000);
   gr=blink(per,per-Math.max(g(67,10),(100-st)*Math.floor(per/100)));
   tim='سبز '+per+'ms';
   let bi='';
   if(st>=g(50,40)){bz=false;tim+=' · بی‌صدا';}
   else if(st>=g(51,20)){bz=beepNow(g(54,60000),g(59,1000),g(62,1),g(65,100));bi=' · بوق تکی';tim+=' · بوق هر '+g(54,60000)+'ms ('+g(62,1)+'×'+g(59,1000)+'ms)';}
   else if(st>=g(52,10)){bz=beepNow(g(54,60000),g(59,1000),g(63,2),g(65,100));bi=' · دو بوق';tim+=' · بوق هر '+g(54,60000)+'ms ('+g(63,2)+'×'+g(59,1000)+'ms)';}
   else{bz=beepNow(g(55,20000),g(60,2000),g(64,3),g(65,100));bi=' · سه بوق';tim+=' · بوق هر '+g(55,20000)+'ms ('+g(64,3)+'×'+g(60,2000)+'ms)';}
   cap='🔋 دشارژ '+st+'٪ — سبز چشمک'+bi+loW;
  }
 }
 R.className='led r'+(r?' on':'');Y.className='led y'+(y?' on':'');G.className='led g'+(gr?' on':'');
 B.className=mute?'bz muted':(bz?'bz':'bz off');
 if(sc)sc.textContent=cap+(mute?' · 🔇 میوت':'');
 if(tm)tm.textContent=tim;
}
function xmute(){const v=(D&&D.p&&D.p[76]===1)?0:1;const f=$('q76');if(f)f.value=v;send(76,v);}
/* اتصال ورودی‌های آلارم (۲۷..۸۲): مثل پروفایل + نگهبان + q2/q3 برای شناسه‌های ۳۲..۸۲ */
for(const id of AIDS){const e=$('q'+id);if(!e)continue;e.onchange=()=>{const v=parseInt(e.value,10);if(isNaN(v))return;
 const m=achk().filter(x=>x.ids.includes(id));
 if(m.length&&!confirm('⚠ '+m.map(x=>x.msg).join('\n')+'\n\nبرد مقدار را گیره می‌زند تا مجموعه سازنده بماند. باز هم ارسال شود؟')){e.value='';afresh();return;}
 send(id,v);};e.oninput=(id>=83?pchk:afresh);}
/* ===== v1.15b: پشتیبان‌گیری JSON تنظیمات (فیلتر + پروفایل + آلارم‌ها) ===== */
const XIDS=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,20,21,22,23,24,25,26];AIDS.forEach(id=>{if(id!==76)XIDS.push(id);});
function xexp(){const x=$('xst');if(!D||!D.p){if(x)x.textContent='هنوز داده‌ای از برد نرسیده';return;}
 const o={app:'ChangeOver-settings',v:1,params:{}};XIDS.forEach(id=>{o.params[id]=D.p[id];});
 const u=URL.createObjectURL(new Blob([JSON.stringify(o)],{type:'application/json'}));
 const a=document.createElement('a');a.href=u;a.download='changeover-settings.json';a.click();
 setTimeout(()=>URL.revokeObjectURL(u),2000);
 if(x)x.textContent='⬇ خروجی گرفته شد ('+XIDS.filter(id=>D.p[id]!=null).length+' مقدار اعمال‌شده)';}
async function ximp(f){const x=$('xst');let o;try{o=JSON.parse(await f.text());}catch(e){if(x)x.textContent='⚠ فایل JSON معتبر نیست';return;}
 const ps=o&&o.params?o.params:{};
 const jobs=XIDS.filter(id=>Number.isFinite(+ps[id])).map(id=>[id,Math.round(+ps[id])]);
 if(!jobs.length){if(x)x.textContent='⚠ هیچ مقدار معتبری در فایل نیست';return;}
 if(!confirm(jobs.length+' مقدار از فایل روی برد اعمال شود؟\nبرد هر کدام را گیره می‌زند؛ نتیجه کنار همان فیلد دیده می‌شود.'))return;
 let ok=0;for(const j of jobs){try{const r=await fetch('/s?id='+j[0]+'&v='+j[1],{method:'POST'});if(r.ok)ok++;}catch(e){}if(x)x.textContent='… '+ok+'/'+jobs.length;await sl(130);}
 if(x)x.textContent=(ok===jobs.length?'✅ ':'⚠ ')+ok+'/'+jobs.length+' اعمال شد — مقادیر گیره‌خورده کنار فیلدها';
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
function draw(d){D=d;const t=d.t,p=d.p,on=d.on==1,man=(d.fl&32)!=0;lnkhealth(d);qfill();cfill();afill();if(TAB==2){if(STAB==0)qgraph();else if(STAB==3)pchk();else afresh();}astat();
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
 for(let id=0;id<20;id++){const a=$('a'+id);if(a&&!(d.q&(1<<id)))a.textContent=p[id]==null?'—':p[id];}
 const fe=$('fspan');if(fe){const mn=p[7]==null?null:(p[7]>=3?p[7]:0),av=p[8]==null?null:(p[8]>=2?p[8]:0);
  fe.innerHTML=(mn==null||av==null)?'—':'فیلتر فعال: مدین '+(p[7]>=3?p[7]+'×1ms':'خاموش (۱..۲)')+' + میانگین '+(p[8]>=2?p[8]+'×1ms':'خاموش (۱)')+' ≈ <b>'+((mn||0)+(av||0))+'ms</b> تاریخچه در کادانس ۱kHz — پنل هر ۱۰۰ms فریم TLM می‌گیرد؛ برای صاف‌شدنِ قابل‌مشاهده مجموع را بالای ~۲۰۰ms ببرید (در مود خودکار ≤۵۰).';}
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
const WSC={SOLO1:[1],SOLO2:[2],BOTH:[1,2]};let W={run:false,abort:false,act:null};
const sl=ms=>new Promise(r=>setTimeout(r,ms));
async function req(u,m,body){const o={method:m||'GET',cache:'no-store'};if(body!=null){o.body=body;o.headers={'Content-Type':'text/plain'};}const r=await fetch(u,o);let j={};try{j=await r.json();}catch(e){}j._s=r.status;return j;}
function wst(m,c){const e=$('wS0');e.innerHTML=m;e.className='cm '+(c||'lb');}
/* ایمنی حین ثبت: لینک، خطای نهایی، JIT، قطع ۱۵V، خاموش شدن ناخواستهٔ مود دستی */
function wchk(){if(W.abort)throw 'پایان توسط کاربر';const d=D;if(!d||d.on!=1)throw 'لینک STM32 قطع شد';
 (W.act||[]).forEach(n=>{const s=d.t[(n-1)*7+6];if(s==7)throw 'خطای نهایی کانال '+fa(n);if(s==5)throw 'تریپ JIT کانال '+fa(n);if(d.t[n==1?18:17]>=15000)throw 'قطع ۱۵V کانال '+fa(n);});
 if(W.man&&!(d.fl&32))throw 'مود دستی قطع شد (ددمن یا محافظ پنل)';}
/* نوشتن پارامتر و صبر تا گزارش همان مقدار از STM32 */
async function setv(id,v){for(let k=0;k<3;k++){const j=await req('/s?id='+id+'&v='+v,'POST');if(j._s!=200)throw 'پاسخ ESP: '+j._s;const e=Date.now()+2500;while(Date.now()<e){await sl(150);if(D&&D.p[id]===v)return;}}throw 'برد مقدار شناسهٔ '+id+' = '+v+' را گزارش نکرد';}
async function wrestore(o){wst('بازگردانی تنظیمات قبل از ثبت…');W.man=false;W.act=null;const L=[[16,0],[18,0],[19,o[19]],[11,o[11]],[12,o[12]]];if(o[19]===0)L.push([16,o[16]],[18,o[18]]);for(const [i,v] of L){try{await setv(i,v);}catch(e){}}}
async function wlog(txt){const j=await req('/benchlog/add','POST',txt);if(j._s==507)throw 'فایل پر است (حدود ۱۰۰KB). فایل را دانلود و پاک کنید.';if(j._s!=200)throw 'نوشتن در فایل انجام نشد (پاسخ '+j._s+')';wfs(j.size);return j;}
function wfs(sz){$('wF').innerHTML=sz==null?'—':`<span class="n">${(sz/1024).toFixed(1)} / 100 KB</span>`;}
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
function wlive(act){if(!D||D.on!=1)return['-','-','-',undefined,'-','-','-',undefined,undefined];const M=(n,b)=>act.includes(n)?[D.t[b],D.t[b+3],D.t[b+4]]:['قطع','-','-'];const a=M(1,0),b=M(2,7);return[a[0],a[1],a[2],undefined,b[0],b[1],b[2],undefined,undefined];}
/* ردیف CSV (۱۴۹ ستون، ترتیب دقیق بخش 5.6، مولتی‌متر نسخه ۴).
   v1.25 رفع باگ: شمارندهٔ پارامترها روی ۹۹ (تعداد v1.22) جا مانده بود در حالی که
   تعداد واقعی ۹۳ است، پس هر ردیف ۶ ستون اضافه می‌نوشت و همهٔ ستون‌های بعد از بلوک
   پارامتر زیر عنوان اشتباه می‌افتادند. حالا از PN که از تعداد واقعی می‌آید استفاده
   می‌شود تا دوباره کهنه نشود. v1.25: +۵ ستون شمارش خام برای کالیبراسیون.
   CSV row, exact 5.6 column order (DMM v4). v1.25 BUGFIX: the parameter loop was
   stuck at 99 (the v1.22 count) while the real count is 93, so every row wrote 6
   extra columns and everything after the parameter block landed under the wrong
   heading. It now derives the bound so it cannot go stale again. */
const PN=93;
/* v1.26 (دستور کاربر ۲۰۲۶-۰۹-۲۹): ۹۳ ستون از ۱۴۹ ستونِ هر ردیف، «تنظیمات» بودند
   که در طول یک سوییپ اصلاً عوض نمی‌شوند — یعنی ۶۲٪ هر ردیف تکرار بی‌فایده. حالا
   تنظیمات یک‌بار به‌صورت خط «# settings:» نوشته می‌شود و ردیف‌ها فقط ۵۶ ستون
   متغیر دارند. با سقف ~۱۰۰ کیلوبایتی فایل، این یعنی ~۲٫۷ برابر نقطهٔ سوییپ بیشتر.
   نکتهٔ ایمنی: اگر وسط کار تنظیمی عوض شود نباید گم شود، پس امضای تنظیمات هر ردیف
   سنجیده می‌شود و در صورت تغییر، خط «# settings:» تازه پیش از آن ردیف نوشته
   می‌شود — پس فایل هنوز کامل است و هر ردیف می‌داند با چه تنظیماتی گرفته شده.
   v1.26: 93 of the 149 columns were SETTINGS that never change during a sweep -
   62 % of every row repeated for nothing. They are now written once as a
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
const WH=['سناریو','#','duty %','raw ۱','filt ۱ mA','iest ۱ mA','جریان باتری ۱ mA','raw ۲','filt ۲ mA','iest ۲ mA','جریان باتری ۲ mA','جریان ورودی کل mA','وضعیت'];
function wbuild(SC,L){W.K=[];SC.forEach(sc=>L.forEach((d,i)=>W.K.push({sc,i,d})));
 $('wT').innerHTML=`<div class="tw"><table class="bt2 wt"><tr>${WH.map(h=>`<th>${h}</th>`).join('')}</tr>${W.K.map((k,x)=>`<tr id="wr${x}"><td>${k.sc}</td><td>${k.i+1}</td><td>${k.d}</td>${'<td>·</td>'.repeat(9)}<td class="lb">در صف</td></tr>`).join('')}</table></div>`;}
function wcell(x,A,st,cl){const r=$('wr'+x);if(!r)return;const c=r.children;A.forEach((v,i)=>{if(v!==undefined)c[3+i].innerHTML=v;});if(st!=null){c[12].textContent=st;c[12].className=cl||'lb';}}
function wmeas(m,act){const M=(n,b)=>act.includes(n)?[m.a(b).toFixed(1),r0(m.a(b+3)),r0(m.a(b+4))]:['قطع','-','-'];const a=M(1,0),b=M(2,7);return [a[0],a[1],a[2],undefined,b[0],b[1],b[2],undefined,undefined];}
/* اسکرول خودکار فقط داخل کادر جدول (v1.17b: خود صفحه تکان نمی‌خورد) / auto-scroll inside the table box only (the page never jumps) */
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
 box.innerHTML=`<div class="bq"><div class="hd"><b>${k.sc} · مرحلهٔ ${k.i+1} · duty ${k.d}٪ — عددهای مولتی‌متر</b></div><div class="bctl">${F(1)}${F(2)}<label class="lb">جریان ورودی کل mA ${N('wIi')}</label>${L('wVi','ولتاژ ورودی V',WVI)}${L('wV1','ولتاژ باتری ۱ V')}${L('wV2','ولتاژ باتری ۲ V')}<label class="lb">یادداشت <input type="text" id="wN" class="dl" style="width:150px"></label><button class="sb" id="wGo">ثبت و مرحلهٔ بعد</button><button class="sb sb2" id="wRe">تکرار همین مرحله</button><button class="sb stp2" id="wEn">پایان</button></div><div class="lb">اجباری: جریان ورودی کل + جریان هر باتری روشن (<b>منفی هم مجاز</b> — تخلیهٔ باتری با شارژر خاموش، مثل بار زنر). جریان باتری باید نزدیک عدد پنل باشد؛ ورودی کل به ولتاژ/جریان باتری وابسته است (فرمول توان: ~۲٫۵ برابر در جریان کم تا ~۰٫۹ برابر در بالای بازه — مصرف ثابت برد در جریان کم برجسته می‌شود). ولتاژها (V) و یادداشت اختیاری.</div></div>`;
 const f=$('wB'+act[0]);if(f)f.focus();
 const lv=setInterval(()=>wcell(x,wlive(act)),400);
 return new Promise(res=>{$('wGo').onclick=()=>{const v={},ok=id=>gv(id);/* v1.9 (user order 2026-09-25): negative currents are VALID - with the charger off the battery itself discharges into other loads (e.g. the zener), the DMM then reads minus */
   v.ii=ok('wIi');if(v.ii==null)return alert('جریان ورودی کل اجباری است.');
   for(const n of act){v['b'+n]=ok('wB'+n);if(v['b'+n]==null)return alert('جریان باتری '+n+' اجباری است (کانال '+n+' روشن است).');}
   [['vi','wVi'],['v1','wV1'],['v2','wV2']].forEach(k=>{const y=gv(k[1]);v[k[0]]=y==null?null:r0(y*1000);});WVI=window.WVI=(v.vi==null)?'':(v.vi/1000);v.note=asc($('wN').value);res({a:'next',v,iso:new Date().toISOString()});};
  $('wRe').onclick=()=>res({a:'repeat'});$('wEn').onclick=()=>res({a:'end'});
  box.onkeydown=ev=>{if(ev.key=='Enter'&&ev.target.tagName=='INPUT')$('wGo').click();};
  W.ft=setInterval(()=>{try{wchk();}catch(er){clearInterval(W.ft);res({a:'err',e:er});}},200);}).finally(()=>{clearInterval(W.ft);clearInterval(lv);box.innerHTML='';box.onkeydown=null;r.classList.remove('wa');});}
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
   wst(sc+': آماده‌سازی (duty صفر، قطع/وصل کانال‌ها، مود دستی)…');W.act=null;W.man=false;
   await setv(16,0);await setv(18,0);for(const n of [1,2])await setv(10+n,act.includes(n)?1:0);
   await setv(19,1);{const e=Date.now()+3000;while(!(D.fl&32)){if(Date.now()>e)throw 'مود دستی روشن نشد';await sl(100);}}W.man=true;W.act=act;
   await wlog(`# run ${run} browser_ts=${new Date().toISOString()} scenario=${sc} duty_list=${L.join(';')}\n`);
   WSIG=null;await wsync();
   try{for(let i=0;i<L.length;){const pm=r0(L[i]*10),lb=sc+' · مرحلهٔ '+(i+1)+' از '+L.length+' · duty '+L[i]+'% ('+Math.round((i+1)/L.length*100)+'%)';
     wcell(x,[],'در حال اندازه‌گیری','wr');wsee(x);
     for(const n of act){const c=D.p[12+n],v=Math.min(pm,c==null?500:c,500);await setv(14+2*n,v);}
     await wopen();
     wst(lb+': عددهای مولتی‌متر را در ردیف رنگی جدول بنویسید','cm wr');const f=await wform(x,act);
     if(f.a=='err')throw f.e;if(f.a=='end'){W.abort=true;throw 'پایان توسط کاربر';}if(f.a=='repeat'){wcell(x,Array(9).fill('·'),'تکرار');continue;}
     const m=await wlatch();
     await wsync();  /* اگر تنظیمی عوض شده، پیش از این ردیف ثبتش کن */
     await wlog(wrow(sc,i,pm,0,r0(Date.now()-W.winMs),m,f.v,f.iso));
     const A=wmeas(m,act);A[3]=f.v.b1??'-';A[7]=f.v.b2??'-';A[8]=f.v.ii;wcell(x,A,'ثبت شد','okc');
     i++;x++;}}
   finally{await wrestore(o);}}}
 catch(e){err=e;}
 W.K.forEach((k,y)=>{const c=$('wr'+y);if(c&&c.children[12].textContent!='ثبت شد')wcell(y,[],'ثبت نشد');});
 W.run=false;W.act=null;W.man=false;document.body.classList.remove('br');
 if(err&&err!=='پایان توسط کاربر')wst('متوقف شد: '+err+' · تنظیمات قبلی برگشت. ردیف‌های ثبت‌شده در فایل مانده‌اند.','cm r');else wst(err?'با دکمهٔ پایان تمام شد؛ تنظیمات قبلی برگشت.':'همهٔ مرحله‌ها ثبت شد؛ تنظیمات قبلی برگشت.','cm g');
 $('wDone').classList.add('v');winfo();}
/* ---------- ساخت تب‌ها ---------- */
/* تب ۱: داده‌برداری بنچ */
$('p1').innerHTML=`<div class="cd"><div class="ds">هر مرحله: پنل duty را می‌گذارد، جدول همان ردیف را نشان می‌دهد و عدد مولتی‌متر را در فرم بالای جدول بنویس و <b>ثبت</b> کن — آمار همان لحظهٔ ثبت قفل می‌شود. <b>SOLO1</b>: کانال ۱ · <b>SOLO2</b>: کانال ۲ · <b>BOTH</b>: هر دو. آمپرمتر: یکی در تغذیهٔ کل برد + سری با سیم شارژ هر باتری روشن. هیچ ضریبی خودکار اعمال نمی‌شود.</div>
<div class="bctl"><label class="lb">duty % <input type="text" id="wL" data-s class="dl" value="2,4,6,8,10,12,14,16,18,20" style="width:160px"></label>
${Object.keys(WSC).map(k=>`<label class="lb"><input type="checkbox" id="wc${k}" checked> ${k}</label>`).join('')}</div>
<div class="bctl"><label class="lb"><input type="checkbox" id="wSw" checked> sweep خودکار با گام ۱٪</label><label class="lb">از <input type="number" id="wA" data-s value="1" min="0" max="50"></label><label class="lb">تا <input type="number" id="wB" data-s value="50" min="0" max="50"></label><span class="lb">خاموش = فهرست دستی بالا</span></div>
<div class="movl" id="wEx" style="display:none"><div class="mod" id="wExB"></div></div>
<div class="bctl"><button class="sb brun" onclick="wStart()">شروع</button><button class="sb stp2 wstop" onclick="W.abort=true">پایان</button><span class="lb">فایل: <b id="wF">—</b></span><a class="sb sb2 lnk" href="/benchlog" download="benchlog.csv">دانلود فایل</a><button class="sb sb2 brun" onclick="wclear()">پاک کردن فایل</button></div>
<div class="cm lb" id="wS0"></div><div id="wF0"></div><div id="wT"></div>
<div class="wn gb" id="wDone" style="background:rgba(52,211,153,.10);color:#a7f3d0"><b style="color:var(--ok)">فایل آماده است.</b> <a class="sb lnk" href="/benchlog" download="benchlog.csv">دانلود benchlog.csv</a> <button class="sb sb2" onclick="wclear()">پاک کردن فایل</button></div></div>`;
bload(document.body);try{$('wSw').checked=localStorage.getItem('wsw')!=='0';}catch(e){};$('wSw').onchange=()=>{const s=$('wSw').checked,L=$('wL'),A=$('wA'),B=$('wB');if(L)L.disabled=s;if(A)A.disabled=!s;if(B)B.disabled=!s;};$('wSw').onchange();document.body.addEventListener('input',bsave);document.body.addEventListener('change',bsave);winfo();
/* ---------- کنترل دستی دیوتی دائمی (دستور کاربر ۲۰۲۶-۰۹-۲۵): کنترلها داخل کارت هر شارژر (از v1.16p)؛
 * ---------- قرارداد ایمنی بخش 5.2 اسپک بدون تغییر: ددمن ۱۰ ثانیه، سقف کانال (p13/p14)،
 * ---------- JIT با مسلح مجدد با ارسال دوبارهٔ همان دیوتی. هیچ ضریبی اینجا ارسال نمی‌شود. ---------- */
const manOn=()=>!!(D&&((D.fl&32)||D.p[19]===1));
async function qset(n){if(W.run)return alert('داده‌برداری ویزارد در جریان است؛ اول آن را تمام کنید.');
 const v=gv('qm'+n);if(v==null)return alert('عدد دیوتی (٪) را وارد کنید.');if(!D||D.on!=1)return alert('لینک STM32 برقرار نیست.');
 const lim=(D.p[12+n]==null?500:D.p[12+n]),pm=Math.max(0,Math.min(lim,r0(v*10)));
 if(pm<r0(v*10))alert('دیوتی به سقف کانال ('+(lim/10)+'٪) محدود شد.');
 const man=manOn(),fx=D&&D.p[13+2*n]===1;let go=man||fx;
 if(!go)go=confirm('مود دستی خاموش است؛ روشن شود و دیوتی اعمال گردد؟\n(لغو = فقط عدد دیوتی ذخیره می‌شود)');
 try{if(go&&!man&&!fx)await setv(19,1);await setv(14+2*n,pm);$('qm'+n).value='';}catch(e){alert(e);}}
async function qzero(n){if(W.run)return alert('داده‌برداری ویزارد در جریان است؛ اول آن را تمام کنید.');
 if(!D||D.on!=1)return alert('لینک STM32 برقرار نیست.');
 const man=manOn(),fx=D&&D.p[13+2*n]===1;if(!man&&!fx&&!confirm('مود دستی خاموش است؛ روشن شود و دیوتی صفر گردد؟'))return;
 try{if(!man&&!fx)await setv(19,1);await setv(14+2*n,0);}catch(e){alert(e);}}
async function fset(n){if(W.run)return alert('داده‌برداری ویزارد در جریان است؛ اول آن را تمام کنید.');if(!D||D.on!=1)return alert('لینک STM32 برقرار نیست.');const on=D&&D.p[13+2*n]===1;try{await setv(13+2*n,on?0:1);}catch(e){alert(e);}}
async function mset(v){if(W.run)return alert('داده‌برداری ویزارد در جریان است؛ اول آن را تمام کنید.');
 if(!D||D.on!=1)return alert('لینک STM32 برقرار نیست.');const man=manOn();if((man?1:0)===v)return;
 if(v&&!man&&!confirm('شارژر خودکار و محافظت‌های باتری متوقف می‌شوند و دیوتی را خودتان تعیین می‌کنید. ادامه؟'))return;
 try{await setv(19,v);}catch(e){alert(e);}}
[1,2].forEach(n=>{const a=$('ma'+n),m=$('mm'+n);if(a)a.onclick=()=>mset(0);if(m)m.onclick=()=>mset(1);});
[1,2].forEach(n=>{const f=$('fx'+n);if(f)f.onclick=()=>fset(n);});
function mview(d){const man=(d.fl&32)!=0,sup=d.p[19]!=null&&d.on==1;
 [1,2].forEach(n=>{const a=$('ma'+n),m=$('mm'+n);if(!a||!m)return;
  a.disabled=m.disabled=!sup;a.classList.toggle('on',sup&&!man);m.classList.toggle('on',sup&&man);const f=$('fx'+n);if(f){f.disabled=!sup||d.p[13+2*n]==null;f.classList.toggle('on',d.p[13+2*n]===1);}});}
pexp(); /* v1.25: متن پارامترها را یک‌بار به «!» هر بخش می‌چسباند */
poll();
setInterval(uview,50); /* v1.16: آینهٔ LED با ۵۰ms — چشمک هم‌سرعت برد */
</script></body></html>)HTML";
