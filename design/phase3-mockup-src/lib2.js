// Phase 2 kit helpers, updated to DESIGN_SYSTEM.md tokens
const P=(cx,cy,r,a)=>[cx+r*Math.sin(a),cy-r*Math.cos(a)];
function arc(cx,cy,r,a0,a1){const[x0,y0]=P(cx,cy,r,a0),[x1,y1]=P(cx,cy,r,a1);const lg=Math.abs(a1-a0)>Math.PI?1:0;const sw=a1>a0?1:0;return `M${x0},${y0} A${r},${r} 0 ${lg} ${sw} ${x1},${y1}`}
let gid=0;
function knob(el){const v=+el.dataset.v,c=el.dataset.c,s=+(el.dataset.s||46),bi=el.dataset.bi!==undefined,nl=el.dataset.nl!==undefined;const R=s/2,cx=R+5,cy=R+5,A=Math.PI*0.75,a=-A+v*2*A,a0=bi?0:-A;const g='kg'+(gid++);
const tip=P(cx,cy,R*0.62,a);
el.innerHTML=`${nl?'':`<div class="lbl">${el.dataset.l||''}</div>`}<svg width="${s+10}" height="${s+10}" style="margin-top:3px;display:block">
<defs><linearGradient id="${g}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4F4F4F"/><stop offset="1" stop-color="#454545"/></linearGradient></defs>
<path d="${arc(cx,cy,R+2.5,-A,A)}" stroke="#212121" stroke-width="3.5" fill="none" stroke-linecap="round"/>
<path d="${arc(cx,cy,R+2.5,Math.min(a0,a),Math.max(a0,a)+0.0001)}" stroke="${c}" stroke-width="3.5" fill="none" stroke-linecap="round"/>
<circle cx="${cx}" cy="${cy}" r="${R-2}" fill="url(#${g})" stroke="rgba(0,0,0,.2)"/>
<polygon points="${tip.join(',')} ${P(cx,cy,R*0.30,a-0.42).join(',')} ${P(cx,cy,R*0.30,a+0.42).join(',')}" fill="${c}"/></svg>
<div class="val" style="color:${el.dataset.vc||c}">${el.dataset.t||''}</div>`;}
function fader(el){const h=+(el.dataset.h||120),v=+el.dataset.v,c=el.dataset.c||'#00A985';const y=(1-v)*(h-12);
el.innerHTML=`<div class="rl" style="color:#EDEDED">${el.dataset.l||''}</div><svg width="26" height="${h}" style="display:block;margin:6px auto 0"><rect x="11" y="0" width="4" height="${h}" rx="2" fill="#141414"/><rect x="11" y="${y+6}" width="4" height="${h-y-6}" rx="2" fill="${c}"/><rect x="2" y="${y}" width="22" height="12" rx="2.5" fill="#262626" stroke="${c}" stroke-width="1.5"/><rect x="7" y="${y+5.25}" width="12" height="1.5" fill="${c}"/></svg><div class="val" style="color:#FFFFFF;text-align:center">${el.dataset.t||''}</div>`}
function meter(el){const h=+(el.dataset.h||120),vs=(el.dataset.v||'.6,.62').split(',').map(Number),w=+(el.dataset.w||9);const g='mg'+(gid++);
let bars=vs.map((v,i)=>`<rect x="${i*(w+2)}" y="0" width="${w}" height="${h}" rx="1.5" fill="#141414"/><rect x="${i*(w+2)}" y="${h*(1-v)}" width="${w}" height="${h*v}" rx="1.5" fill="url(#${g})"/>`).join('');
el.innerHTML=`<div class="rl">${el.dataset.l||''}</div><svg width="${vs.length*(w+2)-2}" height="${h}" style="display:block;margin:6px auto 0"><defs><linearGradient id="${g}" x1="0" y1="1" x2="0" y2="0"><stop offset="0" stop-color="#00FF96"/><stop offset=".55" stop-color="#7BE84F"/><stop offset="1" stop-color="#FFCF07"/></linearGradient></defs>${bars}</svg><div class="val" style="color:#8D8D8D;text-align:center">${el.dataset.t||''}</div>`}
function chrome(name,acc,hdr){document.querySelectorAll('[data-chrome]').forEach(e=>{e.innerHTML=`<div class="tstrip"><span class="f">FANCY</span><span style="color:${acc}">${name}</span></div>`+(hdr||`<img class="hdrimg" src="header760.png">`)});}
window.addEventListener('DOMContentLoaded',()=>{document.querySelectorAll('[data-k]').forEach(knob);document.querySelectorAll('[data-f]').forEach(fader);document.querySelectorAll('[data-m]').forEach(meter);});
const LOGS=f=>Math.log10(f);
