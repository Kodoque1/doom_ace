/* =====================================================================
   ANATOMY: three flat figures of the real mechanism.
   1. a depth: pick which one runs, and see what points where;
   2. a switch: its stages, one at a time, with step controls;
   3. the ring: each depth's wall shows the next depth's buffer.
   ===================================================================== */
(function(){
var NS="http://www.w3.org/2000/svg",NP=RUNX.patches||406;
function E(t,a,p,txt){var e=document.createElementNS(NS,t);for(var k in a)e.setAttribute(k,a[k]);if(txt!==undefined)e.textContent=txt;if(p)p.appendChild(e);return e;}
function H(t,cls,p,html){var e=document.createElement(t);if(cls)e.className=cls;if(html!==undefined)e.innerHTML=html;if(p)p.appendChild(e);return e;}

/* --- the board both SVG figures draw on: pictures of the parts, few words --- */
var CX=[10,220,430],CW=194,CY=232,PTRS=["screen_buffer","screens[0]","mainzone"],PTO=["screen","screen","heap"],SOCK=[[470,66],[470,100],[470,134]];
/* each part's rectangle inside a card: code left, data right, the snapshot drawer below */
var BLK={ace:[10,40,76,56],stack:[108,40,76,56],heap:[10,110,76,56],screen:[108,110,76,56],snap:[10,180,174,54]};   /* a 22-unit gap in the middle carries the pointers */
function bc(i,b){var q=BLK[b];return {x:CX[i]+q[0],y:CY+q[1],w:q[2],h:q[3]};}
/* routes run in the gaps: the left bus (x 4), the band above the cards, each card's gutters */
function viaLeft(x0,y0,i,blk,off){var b=bc(i,blk),gx=CX[i]-(off||4),y=b.y+b.h/2;return [[x0,y0],[x0,214],[gx,214],[gx,y],[b.x,y]];}
/* patches leave the code on the left, run under everything, and come up between the depths:
   one straight entry, through the depth's left side into its ACE */
function fromCodeTo(i,blk,dy){var b=bc(i,blk),gx=CX[i]-8,y=b.y+b.h/2+(dy||0);if(!i)return [[10,64],[4,64],[4,y],[b.x,y]];return [[10,64],[4,64],[4,490],[gx,490],[gx,y],[b.x,y]];}
function viaRight(x0,y0,i,blk,off){var b=bc(i,blk),gx=CX[i]+CW+(off||4),y=b.y+b.h/2;return [[x0,y0],[x0,210],[gx,210],[gx,y],[b.x+b.w,y]];}
/* the three pointers: each leaves its socket down its own lane, out through the bottom of the
   data, and into the depth straight down through its top, along the gap in its middle. The
   routes nest, so none crosses another: the upper the socket, the outer its lane, the lower
   its turn, and the further right it enters */
var HOT=[15,65,82,136,154,198,227,247];   /* grid cells standing for the bytes that differ */
var LANE=[604,584,564],BAND=[206,201,196],ENTRY=[104,99,94];
function ptrRoute(k,p){var sx=SOCK[k][0]+14,sy=SOCK[k][1],lx=LANE[k],hy=BAND[k],ex=CX[p]+ENTRY[k];
  if(k===2){var h=bc(p,"heap"),y=h.y+h.h/2;return [[sx,sy],[lx,sy],[lx,hy],[ex,hy],[ex,y],[h.x+h.w,y]];}
  var sc=bc(p,"screen"),yy=sc.y+sc.h/2+(k===0?-8:8);return [[sx,sy],[lx,sy],[lx,hy],[ex,hy],[ex,yy],[sc.x,yy]];}

/* pictograms */
function icListing(g,x,y,w,h,col,seed){var n=Math.floor(h/7);for(var i=0;i<n;i++){var ind=((i*7+seed)%5===0)?10:2,len=(w-ind-6)*(0.35+0.6*(((i*37+seed*11)%17)/17));
  E("rect",{x:x+ind,y:y+3+i*7,width:len,height:3,fill:col,opacity:0.55},g);}}
function icPlug(g,x,y,col){var p=E("g",{transform:"translate("+x+" "+y+")"},g);E("rect",{x:0,y:-5,width:12,height:10,rx:2,fill:col},p);
  E("rect",{x:-6,y:-3,width:6,height:2,fill:col},p);E("rect",{x:-6,y:1,width:6,height:2,fill:col},p);return p;}
function icGrid(g,x,y,cols,rows,c,col,op){var r=E("g",{},g);for(var j=0;j<rows;j++)for(var i=0;i<cols;i++)
  E("rect",{x:x+i*c,y:y+j*c,width:c-2,height:c-2,fill:col,opacity:op*(0.5+0.5*(((i*13+j*7)%5)/5))},r);return r;}
function icTrays(g,x,y,w,n,col){for(var i=0;i<n;i++){E("rect",{x:x+i*2,y:y-i*8,width:w-i*4,height:6,rx:1,fill:col,opacity:0.35+0.13*i},g);}}
function icHeap(g,x,y,col){[[0,0,24,14],[26,0,14,14],[42,0,26,14],[0,16,36,14],[38,16,30,14],[0,32,16,14],[18,32,50,14]].forEach(function(q,i){
  E("rect",{x:x+q[0],y:y+q[1],width:q[2],height:q[3],fill:col,opacity:i%3===1?0.2:0.5},g);});}
function icPic(g,x,y,w,h,col){E("rect",{x:x,y:y,width:w,height:h,fill:"#0b0807",stroke:col,"stroke-width":1},g);
  for(var i=1;i<4;i++){var s=i/4;E("rect",{x:x+w/2-w*0.45*s,y:y+h*0.42-h*0.36*s,width:w*0.9*s,height:h*0.72*s,fill:"none",stroke:col,opacity:0.3+0.2*i},g);}
  E("rect",{x:x+w*0.64,y:y+h*0.14,width:w*0.28,height:h*0.36,fill:"#2a1d16",stroke:"#eadccb","stroke-width":0.6},g);
  E("rect",{x:x,y:y+h*0.82,width:w,height:h*0.18,fill:"#1a1210"},g);E("rect",{x:x+3,y:y+h*0.86,width:w*0.3,height:h*0.09,fill:"#ffd27a",opacity:0.8},g);}
function icChip(g,x,y,s,col){var r=E("g",{},g);for(var i=0;i<6;i++){var o=s*0.15+i*s*0.14;
    [[x+o,y-6,2,6],[x+o,y+s,2,6],[x-6,y+o,6,2],[x+s,y+o,6,2]].forEach(function(q){E("rect",{x:q[0],y:q[1],width:q[2],height:q[3],fill:"#c9a24a"},r);});}
  E("rect",{x:x,y:y,width:s,height:s,rx:2,fill:"#2a2a2e",stroke:"#5a5a60"},r);var die=E("rect",{x:x+s*0.22,y:y+s*0.22,width:s*0.56,height:s*0.56,fill:col},r);
  E("circle",{cx:x+5,cy:y+5,r:1.8,fill:"#c9a24a"},r);return die;}
function icNeedle(g,x,y,col,len){var p=E("g",{transform:"translate("+x+" "+y+")"},g);E("rect",{x:-len/2,y:-1.2,width:len-6,height:2.4,fill:col},p);
  E("path",{d:"M"+(len/2)+" 0 L"+(len/2-7)+" -4 L"+(len/2-7)+" 4Z",fill:col},p);E("circle",{cx:-len/2,cy:0,r:2.6,fill:col},p);return p;}
function icRegs(g,col){for(var i=0;i<4;i++)E("rect",{x:-12,y:-9+i*5,width:24,height:3.5,fill:col},g);}

function Board(svg){
  var o={},g=E("g",{},svg),i;
  /* Doom's code: a listing, with the places ACE patched plugged into it */
  E("rect",{x:10,y:28,width:300,height:74,class:"a-box"},g);icListing(g,16,32,180,66,"#bfb2a2",3);icListing(g,200,32,104,66,"#bfb2a2",8);
  o.ticks=[];for(i=0;i<9;i++)o.ticks.push(icPlug(g,40+i*30,40+((i*5)%3)*18,"#888"));
  E("text",{x:12,y:20,class:"a-s a-dim"},g,"DOOM2.EXE code · "+fmt(NP)+" patches");
  /* the CPU */
  o.cpu=icChip(g,30,124,48,DCCSS[0]);E("text",{x:54,y:194,class:"a-s","text-anchor":"middle"},g,"CPU");
  /* the host's safe: never in a snapshot. It keeps each depth's saved ESP and its pin values */
  E("rect",{x:112,y:114,width:198,height:70,rx:4,class:"a-box a-host"},g);E("circle",{cx:136,cy:149,r:13,fill:"none",stroke:"#f1b23c","stroke-width":2},g);
  E("path",{d:"M136 149 L136 139",stroke:"#f1b23c","stroke-width":2},g);for(i=0;i<8;i++){var an=i*Math.PI/4;E("rect",{x:136+16*Math.cos(an)-1,y:149+16*Math.sin(an)-1,width:2,height:2,fill:"#f1b23c"},g);}
  E("text",{x:136,y:178,class:"a-s a-hud","text-anchor":"middle"},g,"host");
  o.esp=[0,1,2].map(function(d){var x=160+d*49,r=E("rect",{x:x,y:122,width:44,height:24,rx:2,class:"a-slot"},g),n=E("g",{},g);
    var pins=E("g",{},g);for(var k=0;k<3;k++)E("rect",{x:x+6+k*12,y:156,width:8,height:16,rx:1,fill:DCCSS[d],opacity:0.75},pins);
    return {r:r,n:n,x:x};});
  /* Doom's and ACE's data: one live sheet of bytes, three of them pointers */
  E("rect",{x:330,y:28,width:300,height:152,class:"a-box"},g);o.grid=icGrid(g,338,36,23,11,12.4,DCCSS[0],0.5);
  o.slot=PTRS.map(function(n,k){var x=SOCK[k][0],y=SOCK[k][1];E("rect",{x:x-128,y:y-12,width:146,height:24,fill:"#100b09",opacity:0.85},g);
    E("text",{x:x-122,y:y+5,class:"a-s a-code"},g,n);return E("circle",{cx:x+8,cy:y,r:6,fill:"#c9a24a",stroke:"#0b0807"},g);});
  E("text",{x:332,y:20,class:"a-s a-dim"},g,"Doom + ACE data");
  /* the depths */
  o.card=CX.map(function(x,d){var c={},col=DCCSS[d];c.box=E("rect",{x:x,y:CY,width:CW,height:244,class:"a-card"},g);
    E("text",{x:x+10,y:CY+24,class:"a-t",fill:col},g,"depth "+d);c.icon=E("g",{},g);
    var B=function(n){return bc(d,n);},b;
    b=B("ace");icListing(g,b.x+4,b.y+2,b.w-8,b.h-16,col,d+1);E("text",{x:b.x+4,y:b.y+b.h-2,class:"a-s a-lab"},g,"ACE");
    b=B("stack");icTrays(g,b.x+10,b.y+34,b.w-24,5,col);E("text",{x:b.x+4,y:b.y+b.h-2,class:"a-s a-lab"},g,"stack");
    b=B("heap");icHeap(g,b.x+6,b.y+2,col);E("text",{x:b.x+4,y:b.y+b.h-2,class:"a-s a-lab"},g,"heap");
    b=B("screen");icPic(g,b.x+4,b.y+2,b.w-8,b.h-18,col);E("text",{x:b.x+4,y:b.y+b.h-2,class:"a-s a-lab"},g,"screen");
    b=B("snap");c.drawer=E("rect",{x:b.x,y:b.y,width:b.w,height:b.h,rx:3,class:"a-drawer",stroke:col},g);
    E("rect",{x:b.x+b.w/2-16,y:b.y+b.h-9,width:32,height:4,rx:2,fill:col,opacity:0.7},g);
    c.drawerIn=E("g",{},g);icGrid(c.drawerIn,b.x+8,b.y+7,9,3,9,col,0.6);icPlug(c.drawerIn,b.x+104,b.y+14,col);icPlug(c.drawerIn,b.x+126,b.y+26,col);icPlug(c.drawerIn,b.x+148,b.y+14,col);
    E("text",{x:b.x+b.w-4,y:b.y+b.h-2,class:"a-s a-lab","text-anchor":"end"},g,"snapshot");
    return c;});
  o.arrows=E("g",{},svg);o.fly=E("g",{},svg);
  return o;
}
function line(g,pts,col,head){var d="M"+pts.map(function(p){return p[0]+" "+p[1];}).join(" L");E("path",{d:d,class:"a-arrow",stroke:col},g);
  if(head!==false){var a=pts[pts.length-2],b=pts[pts.length-1],dx=b[0]-a[0],dy=b[1]-a[1],L=Math.sqrt(dx*dx+dy*dy)||1,ux=dx/L,uy=dy/L;
    E("path",{d:"M"+b[0]+" "+b[1]+" L"+(b[0]-ux*8-uy*4)+" "+(b[1]-uy*8+ux*4)+" L"+(b[0]-ux*8+uy*4)+" "+(b[1]-uy*8-ux*4)+"Z",fill:col},g);}}
/* st: cur (holds the CPU), data, patch, ptr (who owns each), espIn (which depth's ESP is in the CPU) */
function paint(o,st){
  o.ticks.forEach(function(t){t.querySelectorAll("rect").forEach(function(r){r.setAttribute("fill",DCCSS[st.patch]);});});
  /* most of Doom's data is the same in every depth: only the bytes that differ, and the pins, are the running depth's */
  o.grid.querySelectorAll("rect").forEach(function(r,i){var hot=HOT.indexOf(i)>=0;r.setAttribute("fill",hot?DCCSS[st.data]:"#8f8577");if(hot)r.setAttribute("opacity",1);});
  o.slot.forEach(function(c){c.setAttribute("fill",DCCSS[st.ptr]);});o.cpu.setAttribute("fill",DCCSS[st.cur]);
  o.esp.forEach(function(s,d){while(s.n.firstChild)s.n.removeChild(s.n.firstChild);var inCpu=d===st.espIn;s.r.setAttribute("stroke",inCpu?"#3d2c23":DCCSS[d]);
    s.r.setAttribute("stroke-dasharray",inCpu?"3 3":"");if(!inCpu)icNeedle(s.n,s.x+22,134,DCCSS[d],30);});
  o.card.forEach(function(c,d){var run=d===st.cur;c.box.setAttribute("stroke",run?DCCSS[d]:"#3d2c23");c.box.setAttribute("class","a-card"+(run?" a-run":""));
    while(c.icon.firstChild)c.icon.removeChild(c.icon.firstChild);var x=CX[d]+CW-28,y=CY+10;
    if(run)E("path",{d:"M"+x+" "+y+" L"+(x+16)+" "+(y+9)+" L"+x+" "+(y+18)+"Z",fill:DCCSS[d]},c.icon);
    else{E("rect",{x:x,y:y,width:5,height:18,fill:"#6f5f50"},c.icon);E("rect",{x:x+10,y:y,width:5,height:18,fill:"#6f5f50"},c.icon);}
    c.drawer.setAttribute("class","a-drawer"+(run?"":" a-full"));c.drawerIn.setAttribute("opacity",run?0.25:1);});
  while(o.arrows.firstChild)o.arrows.removeChild(o.arrows.firstChild);
  PTRS.forEach(function(n,k){line(o.arrows,ptrRoute(k,st.ptr),DCCSS[st.ptr]);});
  line(o.arrows,fromCodeTo(st.patch,"ace"),DCCSS[st.patch]);
}
function newSvg(host,label){var w=H("div","a-scroll",host),s=E("svg",{viewBox:"0 0 640 498",role:"img","aria-label":label,class:"a-svg"});w.appendChild(s);return s;}

/* --- figure 1: a depth --- */
(function(){var host=$("fig-depth");if(!host)return;
  var svg=newSvg(host,"One depth's memory, the shared code and data, and what points where"),o=Board(svg);
  var ctl=H("div","a-ctl",host,"<span>Running:</span>");
  var bs=[0,1,2].map(function(d){var b=H("button","",ctl,"depth "+d);b.type="button";b.style.color=DCCSS[d];b.addEventListener("click",function(){set(d);});return b;});
  function set(d){paint(o,{cur:d,data:d,patch:d,ptr:d,espIn:d});bs.forEach(function(b,q){b.setAttribute("aria-pressed",q===d?"true":"false");});}
  set(0);
})();

/* --- figure 2: a switch --- */
(function(){var host=$("fig-switch");if(!host)return;
  var chips=H("ol","a-stages",host),names=["⏱ 4 frames","save","load","pin","stack","▶ runs"];
  var chipEl=names.map(function(n){return H("li","",chips,n);});
  var svg=newSvg(host,"A switch, stage by stage"),bd=Board(svg),say=H("p","a-say",host),ctl=H("div","a-ctl",host);
  say.setAttribute("aria-live","polite");
  ctl.innerHTML='<button type="button" data-c="back" aria-label="One stage back">◀</button><button type="button" data-c="pause" aria-label="Pause">❚❚</button><button type="button" data-c="step" aria-label="One stage forward">▶|</button><button type="button" data-c="slower" aria-label="Slower">◀◀ slower</button><span class="m3-speed" aria-live="polite">¼×</span><button type="button" data-c="faster" aria-label="Faster">faster ▶▶</button>';
  var o0=0,s=0,t=0,dur=[2.6,3.4,3.4,3.0,3.8,2.2],paused=false,stepping=false,speed=0.25,SP=[0.25,0.5,1,2],toks=[];
  function n0(){return (o0+1)%3;}
  /* the picture at the start of each stage (and at its end, which is the next one's start) */
  function stateAt(o,st){var n=(o+1)%3;return st<=1?{cur:o,data:o,patch:o,ptr:o,espIn:o}:st===2?{cur:o,data:o,patch:o,ptr:o,espIn:o}:
    st===3?{cur:o,data:n,patch:n,ptr:o,espIn:o}:st===4?{cur:o,data:n,patch:n,ptr:n,espIn:o}:{cur:n,data:n,patch:n,ptr:n,espIn:n};}
  function text(o,st){var n=(o+1)%3;return [
    "Depth "+o+" has finished its fourth frame, so at the frame boundary the host's counter asks for a switch.",
    "<b>Save:</b> every block of depth "+o+"'s data that changed since its last save goes into its snapshot, along with the bytes at the "+fmt(NP)+" patched places.",
    "<b>Load:</b> depth "+n+"'s data and patch bytes come back out of its snapshot; on a depth's very first entry, the snapshot is filled from the live state instead.",
    "<b>Pin:</b> whatever the snapshot held, the host points the three pointers at depth "+n+"'s own heap and screen.",
    "<b>Stack:</b> depth "+o+"'s callee-saved registers go onto its own stack and its ESP into the host's safe, then depth "+n+"'s ESP is loaded and its registers popped, so it returns into the switch it left last time.",
    "Depth "+n+" runs until its own fourth frame."][st];}
  /* a token: a pictogram of what moves, following a route */
  function tok(pts,kind,col,delay,len){var g=E("g",{class:"a-tok"},bd.fly);
    if(kind==="data")icGrid(g,-14,-10,4,3,7,col,1);else if(kind==="patch")icPlug(g,-3,0,col);else if(kind==="regs")icRegs(g,col);else icNeedle(g,0,0,col,28);
    g.style.opacity=0;toks.push({g:g,p:pts,d:delay,l:len||1.6});}
  function at(pts,f){var L=[],tot=0;for(var i=1;i<pts.length;i++){var l=Math.abs(pts[i][0]-pts[i-1][0])+Math.abs(pts[i][1]-pts[i-1][1]);L.push(l);tot+=l;}
    var want=f*tot;for(i=1;i<pts.length;i++){if(want<=L[i-1]||i===pts.length-1){var q=L[i-1]?Math.min(1,want/L[i-1]):1;return [pts[i-1][0]+(pts[i][0]-pts[i-1][0])*q,pts[i-1][1]+(pts[i][1]-pts[i-1][1])*q];}want-=L[i-1];}}
  function rev(p){return p.slice().reverse();}
  function start(){var o=o0,n=n0();clear();t=0;paint(bd,stateAt(o,s));
    chipEl.forEach(function(c,i){c.className=i===s?"on":"";});say.innerHTML=text(o,s);
    var cO=DCCSS[o],cN=DCCSS[n];
    if(s===1){tok(viaRight(480,180,o,"snap"),"data",cO,0);tok(fromCodeTo(o,"snap",8),"patch",cO,0.5);}
    if(s===2){tok(rev(viaRight(480,180,n,"snap")),"data",cN,0);tok(rev(fromCodeTo(n,"snap",8)),"patch",cN,0.5);}
    if(s===3){PTRS.forEach(function(p,k){tok([[166+n*49+k*12,170],[166+n*49+k*12,190],[320,190],[320,SOCK[k][1]],[SOCK[k][0]-6,SOCK[k][1]]],"pin",cN,k*0.35,1.2);});}
    if(s===4){tok(viaRight(54,190,o,"stack"),"regs",cO,0,1.3);tok([[78,140],[182+o*49-20,140]],"pin",cO,0.5,0.8);
      tok([[182+n*49-20,140],[78,140]],"pin",cN,1.6,0.8);tok(rev(viaRight(54,190,n,"stack")),"regs",cN,2.0,1.3);}}
  function clear(){while(bd.fly.firstChild)bd.fly.removeChild(bd.fly.firstChild);toks=[];}
  function finish(){paint(bd,s<5?stateAt(o0,s+1):stateAt(n0(),0));clear();}
  function next(){finish();s++;if(s>5){s=0;o0=n0();}start();}
  function back(){paused=true;stepping=false;label();s--;if(s<0){s=5;o0=(o0+2)%3;}start();}
  function label(){ctl.querySelector('[data-c=pause]').textContent=paused?"▶":"❚❚";ctl.querySelector('.m3-speed').textContent=(speed===0.25?"¼":speed===0.5?"½":speed)+"×";ctl.querySelector('[data-c=slower]').disabled=speed===SP[0];ctl.querySelector('[data-c=faster]').disabled=speed===SP[SP.length-1];}
  ctl.addEventListener("click",function(e){var b=e.target.closest("button");if(!b)return;var c=b.getAttribute("data-c");
    if(c==="pause"){paused=!paused;stepping=false;}if(c==="step"){paused=false;stepping=true;}
    if(c==="slower")speed=SP[Math.max(0,SP.indexOf(speed)-1)];if(c==="faster")speed=SP[Math.min(SP.length-1,SP.indexOf(speed)+1)];if(c==="back")back();label();});
  var last=0,seen=false;
  if("IntersectionObserver" in window)new IntersectionObserver(function(es){seen=es[0].isIntersecting;},{root:document}).observe(host);else seen=true;
  function frame(now){var dt=Math.min(0.1,(now-(last||now))/1000);last=now;
    if(!paused&&seen){t+=dt*speed;toks.forEach(function(k){var f=(t-k.d)/k.l;k.g.style.opacity=f<0||f>1.15?0:1;if(f>=0){var p=at(k.p,Math.min(1,f));k.g.setAttribute("transform","translate("+p[0]+" "+p[1]+")");}});
      if(t>=dur[s]){next();if(stepping){stepping=false;paused=true;label();}}}
    requestAnimationFrame(frame);}
  start();
  if(RM){paused=true;label();}
  requestAnimationFrame(frame);
})();

/* --- figure 3: the ring of pictures --- */
(function(){var cv=$("fig-ring");if(!cv)return;var host=cv.closest(".anat"),g=cv.getContext("2d"),W=cv.width,Hh=cv.height;
  var V=[0,1,2].map(function(){var c=document.createElement("canvas");c.width=320;c.height=208;return c;}),f=[0,0,0],run=0,inSlice=0,view=0,paused=false,tk=0;
  var BX=[20,230,440],BY=58,BW=180,BH=117,MX=230,MY=242;
  for(var d=2;d>=0;d--)drawView(V[d],d,100,0,false,false,V[(d+1)%3]);
  function arrow(pts,col,lab,lx,ly){g.strokeStyle=col;g.fillStyle=col;g.lineWidth=2;g.beginPath();g.moveTo(pts[0][0],pts[0][1]);for(var i=1;i<pts.length;i++)g.lineTo(pts[i][0],pts[i][1]);g.stroke();
    var a=pts[pts.length-2],b=pts[pts.length-1],dx=b[0]-a[0],dy=b[1]-a[1],L=Math.sqrt(dx*dx+dy*dy)||1,ux=dx/L,uy=dy/L;
    g.beginPath();g.moveTo(b[0],b[1]);g.lineTo(b[0]-ux*9-uy*5,b[1]-uy*9+ux*5);g.lineTo(b[0]-ux*9+uy*5,b[1]-uy*9-ux*5);g.fill();
    if(lab){g.font="17px VT323, monospace";g.fillText(lab,lx,ly);}}
  function wallX(d){return BX[d]+BW*0.64+2;}
  function show(){g.clearRect(0,0,W,Hh);g.imageSmoothingEnabled=false;g.textBaseline="alphabetic";
    for(var d=0;d<3;d++){g.drawImage(V[d],BX[d],BY,BW,BH);g.strokeStyle=d===run?DCCSS[d]:"#3d2c23";g.lineWidth=d===run?3:1;g.strokeRect(BX[d]-1.5,BY-1.5,BW+3,BH+3);
      g.font="21px VT323, monospace";g.fillStyle=DCCSS[d];g.fillText("depth "+d,BX[d],BY-12);
      /* running: a play sign, and the pen drawing; suspended: a pause sign */
      var ix=BX[d]+BW-22,iy=BY-30;if(d===run){g.beginPath();g.moveTo(ix,iy);g.lineTo(ix+16,iy+9);g.lineTo(ix,iy+18);g.closePath();g.fill();}
      else{g.fillStyle="#6f5f50";g.fillRect(ix,iy,5,18);g.fillRect(ix+10,iy,5,18);}}
    /* each wall shows the next depth's buffer; the last one wraps to the first */
    arrow([[BX[1]-2,BY+40],[BX[0]+BW+2,BY+40]],DCCSS[1]);arrow([[BX[2]-2,BY+40],[BX[1]+BW+2,BY+40]],DCCSS[2]);
    arrow([[BX[0]+BW/2,BY-2],[BX[0]+BW/2,12],[wallX(2)+26,12],[wallX(2)+26,BY-4]],DCCSS[0]);

    /* the monitor */
    var vx=BX[view]+BW/2;arrow([[vx,BY+BH+2],[vx,MY-22],[MX+BW/2,MY-22],[MX+BW/2,MY-4]],DCCSS[view]);
    g.fillStyle="#2f231c";g.fillRect(MX-10,MY-8,BW+20,BH+16);g.fillRect(MX+BW/2-14,MY+BH+8,28,12);g.fillRect(MX+BW/2-40,MY+BH+20,80,6);
    g.drawImage(V[view],MX,MY,BW,BH);
    /* the y key moves the viewer to the next depth */
    var kx=MX+BW+40,ky=MY+40;g.fillStyle="#2f231c";g.fillRect(kx,ky,44,44);g.fillStyle="#4a382c";g.fillRect(kx+4,ky+4,36,30);
    g.font="30px VT323, monospace";g.fillStyle="#f1b23c";g.fillText("y",kx+15,ky+28);
    g.strokeStyle="#f1b23c";g.lineWidth=2;g.beginPath();g.moveTo(kx+56,ky+22);g.lineTo(kx+86,ky+22);g.stroke();
    g.beginPath();g.moveTo(kx+86,ky+22);g.lineTo(kx+78,ky+17);g.lineTo(kx+78,ky+27);g.fillStyle="#f1b23c";g.fill();
    g.font="19px VT323, monospace";g.fillText("next",kx+94,ky+28);
    /* a wall: where the arrows land */
    for(var w=0;w<3;w++){var lx=wallX(w)+2,ly=BY+BH*0.43+26;g.fillStyle="rgba(11,8,7,.85)";g.fillRect(lx-2,ly-13,40,17);g.font="15px VT323, monospace";g.fillStyle="#d8c39a";g.fillText("wall",lx+4,ly);}}
  function tick(){f[run]++;drawView(V[run],run,100-(f[run]*3)%88,f[run],false,false,V[(run+1)%3]);if(++inSlice>=4){inSlice=0;run=(run+1)%3;}show();}
  for(var i=0;i<6;i++)tick();
  var ctl=H("div","a-ctl",host);ctl.innerHTML='<button type="button" data-c="pause" aria-label="Pause">❚❚</button><button type="button" class="tp" data-c="y"><b>y</b> · move to the next depth</button>';
  ctl.addEventListener("click",function(e){var b=e.target.closest("button");if(!b)return;var c=b.getAttribute("data-c");
    if(c==="pause"){paused=!paused;b.textContent=paused?"▶":"❚❚";}if(c==="y"){view=(view+1)%3;show();}});
  if(!RM)setInterval(function(){if(!paused)tick();},450);else paused=true;
})();
})();
