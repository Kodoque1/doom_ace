/* =====================================================================
   DOOMCEPTION: the model and nothing else.
   One shared code block and one shared data block, a private memory per
   depth, one CPU, and a switch whose state sits outside every depth.
   ===================================================================== */
var RUNX=RUN||{};
function ringNext(d,n){return (d+1)%n;}
function fmt(n){return Number(n).toLocaleString("en-US");}

/* --- the numbers on the page come from the run --- */
(function(){
  if(RUNX.copyless&&RUNX.copyless.length)$("nCopied").textContent=fmt(RUNX.copyless[0].save);
  if(RUNX.patches&&$("nPatch"))$("nPatch").textContent=fmt(RUNX.patches);
  if(RUNX.snapshot_bytes)$("nSnap").textContent=fmt(RUNX.snapshot_bytes)+" bytes";
  
  var d1=RUNX.depths&&RUNX.depths[1]||{},li=[];
  if(RUNX.depth_count)li.push("<b>"+RUNX.depth_count+"</b> depths running, one CPU");
  if(RUNX.snapshot_bytes)li.push("<b>"+(RUNX.copyless&&RUNX.copyless.length?fmt(RUNX.copyless[0].save):"?")+"</b> of <b>"+fmt(RUNX.snapshot_bytes)+"</b> bytes copied on a switch");
  if(RUNX.patches)li.push("<b>"+fmt(RUNX.patches)+"</b> code patches per depth");
  if(d1.zone)li.push("<b>"+Math.round((d1.zone[1]-d1.zone[0])/1048576)+" MB</b> heap and <b>"+Math.round((RUNX.stack_pool?RUNX.stack_pool.per_depth:65536)/1024)+" KB</b> stack per depth");
  li.push("<b>30.0</b> frames/s for the depth on screen, <b>15.2</b> for the others");
  $("measured").innerHTML=li.map(function(x){return "<li>"+x+"</li>";}).join("");
})();

/* --- the looping picture, and the teleport: y moves you into the depth on the wall --- */
(function(){var out=$("hero");if(!out)return;var g=out.getContext("2d"),W=out.width,H=out.height,
    V=[0,1,2].map(function(){var c=document.createElement("canvas");c.width=320;c.height=208;return c;}),t=0,f=[0,0,0],v=0,fx=null;
  /* the wall's rectangle inside a depth's picture, as drawView places it */
  function wall(d){var c=V[d],w=c.width,h=c.height,hb=Math.round(h*0.78),sway=Math.sin(f[d]*0.35)*10;return {x:Math.round(w*0.64+sway*0.4),y:Math.round(hb*0.16),w:Math.round(w*0.3),h:Math.round(hb*0.5)};}
  function tag(){g.font="bold 22px VT323, monospace";g.fillStyle="rgba(11,8,7,.75)";g.fillRect(10,10,200,30);g.fillStyle=DCCSS[v];g.fillText("you are in depth "+v,18,32);}
  function show(){g.imageSmoothingEnabled=false;
    if(fx&&fx.k==="zoom"){var p=Math.min(1,(performance.now()-fx.t0)/700),e=p*p*(3-2*p),r=wall(v),c=V[v];
      g.drawImage(c,r.x*e,r.y*e,c.width+(r.w-c.width)*e,c.height+(r.h-c.height)*e,0,0,W,H);
      if(p>=1){v=(v+1)%3;fx={k:"sweep",t0:performance.now()};}return;}
    g.drawImage(V[v],0,0,W,H);
    if(fx&&fx.k==="sweep"){var q=Math.min(1,(performance.now()-fx.t0)/550),y=q*H;g.fillStyle=DCCSS[v];g.globalAlpha=0.85;g.fillRect(0,y-6,W,12);g.globalAlpha=0.35;g.fillRect(0,0,W,y);g.globalAlpha=1;if(q>=1)fx=null;}
    tag();}
  function tick(){var d=t%3;f[d]++;drawView(V[d],d,100-(f[d]*3)%88,f[d],false,false,V[ringNext(d,3)]);t++;}
  for(var i=0;i<9;i++)tick();show();
  function go(){if(fx)return;if(RM){v=(v+1)%3;show();return;}fx={k:"zoom",t0:performance.now()};}
  if(!RM){setInterval(tick,160);(function loop(){show();requestAnimationFrame(loop);})();}
  var b=$("teleport");if(b)b.addEventListener("click",go);out.addEventListener("click",go);
  document.addEventListener("keydown",function(e){if((e.key!=="y"&&e.key!=="Y")||/INPUT|TEXTAREA/.test((e.target||{}).tagName||""))return;var r=out.getBoundingClientRect();if(r.bottom>0&&r.top<innerHeight)go();});
})();

/* --- the scene --- */
function Model(rootId,build){after3d(function(){
  var root=$(rootId),stage=root.querySelector(".m3-stage"),cv=stage.querySelector("canvas");
  var k=K3(stage,cv,{r:14.5,tilt:0.62,look:[-1.2,0.8,0.3],sway:0.08,narrowK:1.35});
  if(!k){stage.insertAdjacentHTML("beforeend","<p class='m3-hint'>This scene needs WebGL.</p>");return;}
  var T=k.T,V=function(x,y,z){return new T.Vector3(x,y,z);},M={};
  function L(t,x,y,z,c,s,p){var l=k.label(t,x,y,z,c||"#d8c39a",(s||0.6)*1.3);if(p)l.userData.pri=p;return l;}
  function glow(m,c,e){m.material.color.setHex(c);if(m.material.emissive){m.material.emissive.setHex(c);m.material.emissiveIntensity=e===undefined?0.3:e;}if(m.userData&&m.userData.edge)m.userData.edge.material.color.setHex(c);}
  var NEU=0x8f8577;
  k.box(22,0.05,11,0x1a1310,-1,-0.08,0,0);
  /* shared: Doom's code and Doom's data */
  var code=k.plate(4.2,1.7,NEU,-2.4,0,-2.4,"code",0.55),data=k.plate(4.2,1.7,NEU,2.4,0,-2.4,"data",0.55);
  var lab={code:L("Doom's code",-2.4,0.4,-3.5),data:L("Doom's data",2.4,0.4,-3.5),shared:L("shared by every depth, fixed addresses",0,0.3,-1.3,"#a89884",0.45)};
  var tiles=[0,1,2,3,4,5].map(function(i){var m=new T.Mesh(new T.BoxGeometry(0.34,0.08,0.34),k.mat(0x3d332a,0.05));m.position.set(-3.9+i*0.6,0.06,-2.0);k.scene.add(m);return m;});
  lab.patch=L("patched places",-2.4,0.3,-1.75,"#a89884",0.4);
  var dataTint=new T.Mesh(new T.BoxGeometry(4.0,0.06,1.5),k.mat(NEU,0.2,0.0));dataTint.position.set(2.4,0.05,-2.4);k.scene.add(dataTint);
  /* each depth's private memory */
  var RX=[-4.6,0,4.6],RZ=2.0,R=[];
  for(var d=0;d<3;d++)(function(d){var x=RX[d],o={},G=new T.Group();k.scene.add(G);k.layer=G;o.G=G;
    o.base=k.plate(4.0,2.6,DCHEX[d],x,0,RZ,null,0.16);
    o.title=L("depth "+d+"'s own memory",x,0.35,RZ+1.6,DCCSS[d],0.55,5);
    o.ace=k.plate(1.0,0.7,DCHEX[d],x-1.25,0.02,RZ-0.6,"code",0.8);o.aceL=L("ACE code",x-1.25,0.32,RZ-0.6,DCCSS[d],0.38);
    o.stack=k.plate(0.7,0.7,DCHEX[d],x-1.25,0.02,RZ+0.5,"stack",0.8);o.stackL=L("stack",x-1.25,0.32,RZ+0.5,DCCSS[d],0.38);
    o.col=new T.Mesh(new T.BoxGeometry(0.5,1,0.5),k.mat(DCHEX[d],0.35));o.col.position.set(x-1.25,0.15,RZ+0.5);o.col.scale.y=0.3;G.add(o.col);
    o.heap=k.plate(1.4,1.0,DCHEX[d],x+0.15,0.02,RZ+0.5,"heap",0.7);o.heapL=L("heap",x+0.15,0.32,RZ+0.5,DCCSS[d],0.38);
    o.saved=k.plate(1.0,0.6,DCHEX[d],x+1.45,0.02,RZ+0.7,"slot",0.5);o.savedL=L("saved",x+1.45,0.3,RZ+0.7,DCCSS[d],0.36);
    o.cv=document.createElement("canvas");o.cv.width=160;o.cv.height=104;o.tex=new T.CanvasTexture(o.cv);
    o.pic=new T.Mesh(new T.PlaneGeometry(2.7,1.76),new T.MeshBasicMaterial({map:o.tex,side:T.DoubleSide}));o.pic.position.set(x+0.6,1.25,RZ-0.85);G.add(o.pic);
    o.picL=L("screen buffer",x+0.6,2.35,RZ-0.85,DCCSS[d],0.38);
    o.warn=L("",x,2.6,RZ-0.4,"#ff8a76",0.48,7);o.warn.visible=false;
    k.layer=null;R.push(o);})(d);
  /* the switch's own state, outside every depth */
  var SX=-8.6,sw=k.plate(2.6,2.8,HUDHEX,SX,0,0.2,"host",0.5);
  lab.sw=L("the switch's state",SX,0.4,-1.5,"#f1b23c",0.5,5);lab.sw2=L("outside every depth",SX,0.3,1.95,"#c99a4a",0.38);
  var swRows=[0,1,2].map(function(d){var m=new T.Mesh(new T.BoxGeometry(1.8,0.08,0.4),k.mat(0x3d332a,0.05));m.position.set(SX,0.06,-0.5+d*0.6);k.scene.add(m);return m;});
  lab.esp=L("saved stack pointers",SX,0.32,1.25,"#f4dca8",0.36);
  var pinRow=k.box(1.8,0.1,0.3,HUDHEX,SX,0.04,-1.0,0.3);lab.pins=L("pin values",SX+1.6,0.3,-1.0,"#f4dca8",0.34);
  /* the CPU, raised above the floor */
  /* the CPU: a pin-grid chip on a board, raised above the memory it reads and writes */
  var CY=3.0,CZ=-0.4;k.box(2.9,0.07,2.1,0x1d3a22,0,CY-0.07,CZ,0.08);
  [[-1.3,-0.9],[1.3,-0.9],[-1.3,0.9],[1.3,0.9]].forEach(function(q){k.box(0.1,CY-0.07,0.1,0x4a4540,q[0],0,CZ+q[1],0.05);});
  var cpu=k.box(1.7,0.16,1.7,0x2a2a2e,0,CY,CZ,0.04);
  for(var pi=0;pi<8;pi++){var po=-0.7+pi*0.2;
    k.box(0.05,0.03,0.2,0xc9a24a,po,CY+0.02,CZ-0.92,0.3);k.box(0.05,0.03,0.2,0xc9a24a,po,CY+0.02,CZ+0.92,0.3);
    k.box(0.2,0.03,0.05,0xc9a24a,-0.92,CY+0.02,CZ+po,0.3);k.box(0.2,0.03,0.05,0xc9a24a,0.92,CY+0.02,CZ+po,0.3);}
  k.box(0.12,0.02,0.12,0xc9a24a,-0.68,CY+0.16,CZ-0.68,0.4);
  var chip=new T.Mesh(new T.BoxGeometry(0.95,0.06,0.95),k.mat(DCHEX[0],0.6));chip.position.set(0,CY+0.19,CZ);k.scene.add(chip);
  lab.cpu=L("CPU",0,CY+0.75,CZ,"#ffffff",0.6,6);
  var PN=["mainzone","screen_buffer","screens[0]"],sock=[3.0,3.6,4.2].map(function(x){return k.box(0.34,0.12,0.24,0xc9a24a,x,0.04,-1.8,0.35);});
  var ptr=new T.Group(),ptrMat=new T.MeshBasicMaterial({color:0xffffff,transparent:true,opacity:0.6});k.scene.add(ptr);
  lab.ptr=L("pointers: mainzone, screen_buffer, screens[0]",3.6,0.75,-1.2,"#a89884",0.36);
  var banner=document.createElement("div");banner.className="m3-sim";banner.hidden=true;stage.appendChild(banner);
  var status=document.createElement("div");status.className="m3-status";stage.appendChild(status);

  /* moving things: right-angled routes, one glyph per event */
  function route(a,b,yc){var p=[a,V(a.x,yc,a.z),V(b.x,yc,a.z),V(b.x,yc,b.z),b],c=new T.CurvePath();for(var j=0;j<4;j++)if(p[j].distanceTo(p[j+1])>1e-3)c.add(new T.LineCurve3(p[j],p[j+1]));if(!c.curves.length)c.add(new T.LineCurve3(a,V(b.x,b.y+0.01,b.z)));return c;}
  var toks=[],EV=[],clock=0,paused=false,speed=1,stepUnits=-1,H=[];
  function after(ms,fn){if(RM){fn();return;}EV.push({t:clock+ms/1000,fn:fn});}
  /* controls: back one step, pause, one step, speed (each frame and each switch step is one step) */
  var ctl=document.createElement("div");ctl.className="m3-ctl";ctl.innerHTML='<button type="button" data-c="back" aria-label="One step back">◀</button><button type="button" data-c="pause" aria-label="Pause">❚❚</button><button type="button" data-c="step" aria-label="One step forward">▶|</button><button type="button" data-c="speed">speed 1×</button>';stage.appendChild(ctl);
  ["pointerdown","pointermove"].forEach(function(ev){ctl.addEventListener(ev,function(e){e.stopPropagation();});});
  var SPEEDS=[0.5,1,2];function ctlLabel(){ctl.querySelector('[data-c=pause]').textContent=paused?"▶":"❚❚";ctl.querySelector('[data-c=speed]').textContent="speed "+(speed===0.5?"½":speed)+"×";}
  ctl.addEventListener("click",function(e){var b=e.target.closest("button");if(!b)return;e.stopPropagation();var c=b.getAttribute("data-c");
    if(c==="pause"){paused=!paused;stepUnits=-1;}if(c==="step"){stepUnits=1;paused=false;}if(c==="speed")speed=SPEEDS[(SPEEDS.indexOf(speed)+1)%SPEEDS.length];if(c==="back")back();ctlLabel();});
  /* what moves looks like what it is: an instruction, a sheet of data, a CALL patch, a
     pointer, the 4 saved registers, or the picture itself */
  function tokMesh(kind,c,txt,tex){var G=new T.Group(),m,i;
    if(kind==="insn"){G.add(new T.Mesh(new T.BoxGeometry(0.36,0.07,0.22),k.mat(0xeadccb,0.5)));txt=txt||"MOV";}
    else if(kind==="call"){G.add(new T.Mesh(new T.BoxGeometry(0.4,0.08,0.24),k.mat(c,0.7)));txt=txt||"CALL";}
    else if(kind==="data"){for(i=0;i<3;i++){m=new T.Mesh(new T.BoxGeometry(0.4,0.025,0.3),k.mat(i===2?c:0xd8c39a,0.45));m.position.set(i*0.03,i*0.05,-i*0.03);G.add(m);}}
    else if(kind==="regs"){for(i=0;i<4;i++){m=new T.Mesh(new T.BoxGeometry(0.44,0.05,0.16),k.mat(c,0.55));m.position.y=i*0.075;G.add(m);}}
    else if(kind==="pin"){m=new T.Mesh(new T.CylinderGeometry(0.03,0.03,0.42,6),k.mat(c,0.8));m.rotation.x=Math.PI/2;G.add(m);
      m=new T.Mesh(new T.ConeGeometry(0.08,0.18,8),k.mat(c,0.9));m.rotation.x=Math.PI/2;m.position.z=0.29;G.add(m);G.userData.aim=1;}
    else if(kind==="pic"){m=new T.Mesh(new T.PlaneGeometry(0.84,0.55),new T.MeshBasicMaterial({map:tex,side:T.DoubleSide}));G.add(m);
      m=new T.Mesh(new T.PlaneGeometry(0.92,0.63),new T.MeshBasicMaterial({color:c,side:T.DoubleSide}));m.position.z=-0.01;G.add(m);}
    if(txt){var l=k.label(txt,0,0,0,"#ffffff",0.34);l.userData.pri=8;k.scene.remove(l);G.add(l);l.position.set(0,kind==="pic"?0.45:0.3,0);}
    return G;}
  function send(kind,c,a,b,yc,dur,txt,tex){if(RM)return;var m=tokMesh(kind,c,txt,tex),path=route(a,b,yc===undefined?1.6:yc);m.scale.setScalar(1.7);m.position.copy(a);k.scene.add(m);toks.push({m:m,p:path,t:0,d:dur||0.6});}
  function P(o,y){return V(o.position.x,y===undefined?0.25:y,o.position.z);}
  /* mainzone -> the depth's heap; screen_buffer and screens[0] -> its screen buffer */
  function sockP(i){return V(sock[i].position.x,0.2,sock[i].position.z);}
  function ptrTarget(i,d){var o=R[d];return i===0?P(o.heap,0.1):V(o.pic.position.x+(i===1?-0.4:0.4),0.42,o.pic.position.z);}
  function rewirePtr(d){while(ptr.children.length){var c=ptr.children.pop();c.geometry.dispose();}ptrMat.color.setHex(DCHEX[d]);
    for(var i=0;i<3;i++)ptr.add(new T.Mesh(new T.TubeGeometry(route(sockP(i),ptrTarget(i,d),0.7+i*0.12),40,0.025,5,false),ptrMat));}

  /* the model's state */
  var S;
  function fresh(){S={n:1,cur:0,next:0,swap:true,loop:false,turns:false,f:0,t:0,phase:"run",beat:0,frames:[0,0,0],hp:[100,67,34],own:[100,67,34],
    dataOwner:0,patchOwner:-1,ptrOwner:0,sim:false,show:{}};}
  fresh();
  function vis(){var s=S.show;Object.keys(lab).forEach(function(q){lab[q].visible=!!s[q];});
    R.forEach(function(o,d){var on=d<S.n;o.G.visible=on;o.saved.visible=o.savedL.visible=!!(on&&S.swap&&S.turns&&s.saved);o.aceL.visible=!!s.ace;o.ace.visible=d===0?S.patchOwner>=0||S.n>1:true;
      o.stackL.visible=!!s.stack;o.heapL.visible=!!s.heap;o.picL.visible=!!s.pic;o.base.material.opacity=d===S.cur?0.32:0.12;});
    tiles.forEach(function(m){glow(m,S.patchOwner<0?0x3d332a:DCHEX[S.patchOwner],S.patchOwner<0?0.05:0.6);});
    glow(dataTint,DCHEX[S.dataOwner],0.25);dataTint.material.opacity=S.n>1?0.35:0;
    glow(chip,DCHEX[S.cur],0.6);ptr.visible=!!s.ptr;if(s.ptr)rewirePtr(S.ptrOwner);
    swRows.forEach(function(m,d){var on=S.turns&&d<S.n&&d!==S.cur;glow(m,on?DCHEX[d]:0x3d332a,on?0.5:0.05);});
    sw.visible=lab.sw.visible;pinRow.visible=sw.visible;lab.pins.visible=sw.visible&&!!s.sw;swRows.forEach(function(m){m.visible=sw.visible;});}
  function draw(d){var tgt=S.n>1?S.ptrOwner:d,o=R[tgt];drawView(o.cv,d,S.hp[d],S.frames[d],false,false,S.loop&&S.n>1?R[ringNext(d,S.n)].cv:null);o.tex.needsUpdate=true;}
  function flash(d){var m=R[d].pic.material;m.color.setHex(0xffffff);var t0=performance.now();(function f(){var p=(performance.now()-t0)/400;if(p<1){var v=0.6+0.4*p;m.color.setRGB(v+0.4*(1-p),v+0.4*(1-p),v);requestAnimationFrame(f);}else m.color.setHex(0xffffff);})();}
  function warn(){var d=S.cur,w=[];if(S.sim){if(S.dataOwner!==d)w.push("reads depth "+S.dataOwner+"'s data");if(S.patchOwner>=0&&S.patchOwner!==d)w.push("runs depth "+S.patchOwner+"'s ACE");if(S.ptrOwner!==d)w.push("draws into depth "+S.ptrOwner+"'s picture");}
    R.forEach(function(o,q){o.warn.visible=q===d&&w.length>0;if(q===d&&w.length)k.setLabel(o.warn,"⚠ depth "+d+" "+w.join(", "),"#ff8a76");});}
  function say(t){status.textContent=t;}
  /* one frame of the running depth: fetch code, read and write data, draw its picture */
  function frame(){var d=S.cur;S.frames[d]++;
    send("insn",0,P(code),V(-0.5,CY+0.3,CZ),2.2,0.45);
    after(300,function(){send("data",0xd8c39a,P(data),V(0.5,CY+0.3,CZ),2.4,0.4);});
    after(620,function(){var tg=S.n>1?S.ptrOwner:d;send("pic",DCHEX[d],V(0,CY+0.3,CZ),P(R[tg].pic,0.9),2.6,0.45,null,R[d].tex);});
    if(S.sim){S.hp[d]=S.hp[S.dataOwner];}S.hp[d]=S.hp[d]-3<12?100:S.hp[d]-3;S.own[d]=S.hp[d];S.dataOwner=d;
    var wl=S.loop&&S.n>1;if(wl){var nx=ringNext(d,S.n),wp=function(o){return V(o.pic.position.x+0.78,1.57,o.pic.position.z+0.02);};
      after(400,function(){send("pic",DCHEX[nx],P(R[nx].pic,1.25),V(0.6,CY+0.3,CZ),2.6,0.3,"d"+nx+"'s frame",R[nx].tex);});
      after(700,function(){send("pic",DCHEX[nx],V(0.6,CY+0.3,CZ),wp(R[S.ptrOwner]),2.6,0.3,"→ wall",R[nx].tex);});}
    after(1000,function(){draw(d);flash(S.n>1?S.ptrOwner:d);});warn();vis();
    say("depth "+d+" runs · frame "+(S.f+1)+(S.turns&&S.n>1?" of 4":""));}
  /* the switch: save and load the data and the patches, re-point the pointers, hand over the CPU */
  var PT=[1,2,4];   /* the patched places a CALL travels to */
  var BEATS=[
    function(o,n){say("switch: depth "+o+"'s copy of Doom's data → its snapshot, depth "+n+"'s back (only bytes that differ)");send("data",DCHEX[o],P(data),P(R[o].saved),1.4,0.6);after(500,function(){send("data",DCHEX[n],P(R[n].saved),P(data),1.4,0.6);});},
    function(o,n){say("switch: depth "+o+"'s patches out of Doom's code, depth "+n+"'s in");
      PT.forEach(function(t,i){after(i*120,function(){send("call",DCHEX[o],P(tiles[t],0.15),P(R[o].saved),1.8,0.55);});after(520+i*120,function(){send("call",DCHEX[n],P(R[n].saved),P(tiles[t],0.15),1.8,0.55);});});},
    function(o,n){say("switch: the host pins Doom's three pointers to depth "+n+"'s heap and screen");
      PN.forEach(function(nm,i){after(i*180,function(){send("pin",DCHEX[n],V(SX,0.3,-1.0),sockP(i),2.2,0.7,nm);});});},
    function(o,n){say("switch: depth "+o+"'s 4 registers → its own stack, its ESP → the host; depth "+n+"'s come back");
      send("regs",DCHEX[o],V(0,CY+0.2,CZ),P(R[o].stack,0.4),2.9,0.55,"4 registers");after(350,function(){send("pin",DCHEX[o],V(-0.9,CY+0.2,CZ),P(swRows[o],0.2),3.2,0.5,"ESP");});
      after(800,function(){send("pin",DCHEX[n],P(swRows[n],0.2),V(-0.9,CY+0.2,CZ),3.2,0.5,"ESP");send("regs",DCHEX[n],P(R[n].stack,0.4),V(0,CY+0.2,CZ),2.9,0.55);});}];
  function beatEnd(b,o,n){if(b===0){S.dataOwner=n;S.hp[n]=S.own[n];}if(b===1)S.patchOwner=n;if(b===2)S.ptrOwner=n;if(b===3){S.cur=n;}vis();}
  var FR=1.4,BT=1.5;
  function snapState(){H.push({S:JSON.stringify(S),st:status.textContent,cv:R.map(function(o){return o.cv.getContext("2d").getImageData(0,0,o.cv.width,o.cv.height);})});if(H.length>40)H.shift();}
  /* before a step starts: pause here if stepping, otherwise remember the state so Back can return to it */
  function gate(){if(stepUnits===0){paused=true;stepUnits=-1;ctlLabel();return true;}if(stepUnits>0)stepUnits--;snapState();return false;}
  function back(){var h=H.pop();if(!h)return;paused=true;stepUnits=-1;toks.forEach(function(o){k.scene.remove(o.m);});toks=[];EV=[];
    S=JSON.parse(h.S);R.forEach(function(o,d){o.cv.getContext("2d").putImageData(h.cv[d],0,0);o.tex.needsUpdate=true;});vis();warn();say("◀ back: "+h.st);}
  k.frames.push(function(dt){if(paused)return;dt*=speed;clock+=dt;
    EV=EV.filter(function(e){if(e.t<=clock){e.fn();return false;}return true;});
    toks=toks.filter(function(o){o.t+=dt/o.d;var p=Math.min(1,o.t),e=p<0.5?2*p*p:1-2*(1-p)*(1-p);o.m.position.copy(o.p.getPointAt(e));if(o.m.userData.aim&&e<0.97)o.m.lookAt(o.p.getPointAt(Math.min(1,e+0.03)));if(p>=1){k.scene.remove(o.m);return false;}return true;});
    S.t+=dt;
    if(S.phase==="run"){if(S.t>=FR){if(gate())return;S.t=0;frame();S.f++;if(S.turns&&S.n>1&&S.f>=4){S.f=0;S.next=(S.cur+1)%S.n;S.phase="switch";S.beat=S.swap?0:3;BEATS[S.beat](S.cur,S.next);}}}
    else if(S.t>=BT){if(gate())return;S.t=0;var o=S.cur,n=S.next;beatEnd(S.beat,o,n);
      if(S.beat>=3||!S.swap&&S.beat>=3){S.phase="run";}else{S.beat++;if(!S.swap)S.beat=3;BEATS[S.beat](o,n);}}});
  /* what each step changes */
  function drawAll(){for(var q=0;q<S.n;q++){var o=R[q];drawView(o.cv,q,S.hp[q],S.frames[q],false,false,S.loop&&S.n>1?R[ringNext(q,S.n)].cv:null);o.tex.needsUpdate=true;}}
  M.set=function(o){Object.keys(o).forEach(function(q){S[q]=o[q];});banner.hidden=!S.sim;banner.textContent="⚠ SIMULATED: what would happen without the switch's swaps";
    if(o.boot!==undefined){var d=o.boot;S.patchOwner=d;S.ptrOwner=d;S.dataOwner=d;S.cur=d;S.f=0;S.phase="run";}
    if(o.show)S.show=o.show;drawAll();vis();warn();};
  M.look=function(x,y,z,r){k.focus(x,y,z,r);};
  M.home=function(){k.focus(-1.2,0.8,0.3,14.5);};
  M.reset=function(){toks.forEach(function(o){k.scene.remove(o.m);});toks=[];EV=[];H=[];fresh();R.forEach(function(o,d){o.col.scale.y=0.3;drawView(o.cv,d,S.hp[d],0);o.tex.needsUpdate=true;});vis();warn();say("");M.home();};
  M.reset();
  build(M,root);
});}

var ALL={code:1,data:1,shared:1,cpu:1,sw:1,sw2:1,saved:1,ptr:1,patch:1,esp:1};
/* the model, running on its own */
Model("model",function(M){M.set({n:3,turns:true,swap:true,loop:true,show:ALL});});
