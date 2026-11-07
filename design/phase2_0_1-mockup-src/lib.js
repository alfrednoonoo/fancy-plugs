const P=(cx,cy,r,a)=>[cx+r*Math.sin(a),cy-r*Math.cos(a)];
function arc(cx,cy,r,a0,a1){const[x0,y0]=P(cx,cy,r,a0),[x1,y1]=P(cx,cy,r,a1);const lg=Math.abs(a1-a0)>Math.PI?1:0;const sw=a1>a0?1:0;return `M${x0},${y0} A${r},${r} 0 ${lg} ${sw} ${x1},${y1}`}
function knob(el){const v=+el.dataset.v,c=el.dataset.c,s=+(el.dataset.s||44),bi=el.dataset.bi!==undefined;const R=s/2,cx=R+4,cy=R+4,A=Math.PI*0.75,a=-A+v*2*A,a0=bi?0:-A;
const[tx,ty]=P(cx,cy,R*0.55,a);const t1=P(cx,cy,R*0.75,a),t2=P(tx,ty,0,0);
const w=R*0.22,b1=P(tx,ty,w,a+Math.PI/2),b2=P(tx,ty,w,a-Math.PI/2),tip=P(cx,cy,R*0.78,a);
el.innerHTML=`<div class="lbl">${el.dataset.l||''}</div><svg width="${s+8}" height="${s+8}" style="margin-top:4px">
<defs><linearGradient id="g${Math.random().toString(36).slice(2)}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4F4F4F"/><stop offset="1" stop-color="#454545"/></linearGradient></defs>
<path d="${arc(cx,cy,R+1.5,-A,A)}" stroke="#212121" stroke-width="3" fill="none" stroke-linecap="round"/>
<path d="${arc(cx,cy,R+1.5,Math.min(a0,a),Math.max(a0,a)+0.0001)}" stroke="${c}" stroke-width="3" fill="none" stroke-linecap="round"/>
<circle cx="${cx}" cy="${cy}" r="${R-3}" fill="#4B4B4B"/>
<polygon points="${tip.join(',')} ${P(cx,cy,R*0.48,a-0.32).join(',')} ${P(cx,cy,R*0.48,a+0.32).join(',')}" fill="${c}"/></svg>
<div class="val" style="color:${c}">${el.dataset.t||''}</div>`;}
function fader(el){const h=+(el.dataset.h||120),v=+el.dataset.v,c=el.dataset.c||'#00A884';const y=(1-v)*(h-10);
el.innerHTML=`<div class="cap">${el.dataset.l||''}</div><svg width="24" height="${h}"><rect x="10" y="0" width="4" height="${h}" rx="2" fill="#141414"/><rect x="10" y="${y+5}" width="4" height="${h-y-5}" rx="2" fill="${c}"/><rect x="2" y="${y}" width="20" height="10" rx="2" fill="#4B4B4B"/><rect x="5" y="${y+4.25}" width="14" height="1.5" fill="${c}"/></svg><div class="val" style="color:#EDEDED;margin:0">${el.dataset.t||''}</div>`}
function meter(el){const h=+(el.dataset.h||120),vs=(el.dataset.v||'.6,.62').split(',').map(Number);
let bars=vs.map((v,i)=>`<rect x="${i*11}" y="0" width="8" height="${h}" rx="1.5" fill="#141414"/><rect x="${i*11}" y="${h*(1-v)}" width="8" height="${h*v}" rx="1.5" fill="url(#mg)"/>`).join('');
el.innerHTML=`<div class="cap" style="text-align:center">${el.dataset.l||''}</div><svg width="${vs.length*11}" height="${h}"><defs><linearGradient id="mg" x1="0" y1="1" x2="0" y2="0"><stop offset="0" stop-color="#0DFC90"/><stop offset=".35" stop-color="#3CF473"/><stop offset=".6" stop-color="#82E64D"/><stop offset=".8" stop-color="#C7D924"/><stop offset="1" stop-color="#F7D10A"/></linearGradient></defs>${bars}</svg><div class="val" style="color:#EDEDED;margin:0">${el.dataset.t||''}</div>`}
window.addEventListener('DOMContentLoaded',()=>{document.querySelectorAll('[data-k]').forEach(knob);document.querySelectorAll('[data-f]').forEach(fader);document.querySelectorAll('[data-m]').forEach(meter);});
