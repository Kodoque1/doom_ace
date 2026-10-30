/* =====================================================================
   3D toolkit shared by the 3D scenes: one look, one camera behaviour
   ===================================================================== */
var DCHEX=[0xe0553c,0x7fd23e,0x6c9cf0],DCCSS=["#e0553c","#7fd23e","#6c9cf0"],HUDHEX=0xf1b23c;
/* Doom-like grit for every 3D surface: a shared greyscale plate texture,
   tinted by each material's colour, with world-sized UVs so it never smears. */
var GRIT=null,GRATE=null;
var TEXK={};
function gritTex(T,kind){
  var N=64,c=document.createElement("canvas");c.width=c.height=N;var g=c.getContext("2d"),x,y,i;
  var im=g.createImageData(N,N),dd=im.data;
  function h(a,b,s){var q=(a*374761393+b*668265263+s*982451653)|0;q=Math.imul(q^(q>>>13),1274126177);return ((q^(q>>>16))>>>0)/4294967295;}
  for(y=0;y<N;y++)for(x=0;x<N;x++){
    var n=0.62+0.22*h(x>>2,y>>2,3)+0.16*h(x,y,5),v=n,px=x%32,py=y%32;
    if(kind==="grate"){v=((x%8<2)||(y%8<2))?0.35:n*0.8;}
    else if(kind==="code"){var row=y>>3,len=8+Math.floor(h(row,1,11)*40),ind=(h(row,2,12)>0.6?6:2);
      v=0.42*n;if(y%8>=3&&y%8<=4&&x>=ind&&x<ind+len&&h(x>>2,row,13)>0.18)v=1.15;if(y%8>=3&&y%8<=4&&x<ind+2&&x>=ind)v=1.3;}
    else if(kind==="data"){var cx=x%8,cy=y%8;v=(cx===0||cy===0)?0.3:(0.55+0.55*h(x>>3,y>>3,21))*(0.9+0.1*n);}
    else if(kind==="slot"){var cx2=x%8,cy2=y%8;v=(cx2===0||cy2===0)?0.3:(0.55+0.5*h(x>>3,y>>3,21));if((x+y)%6<1)v*=0.55;if(y%16===0)v=1.3;}
    else if(kind==="stack"){v=(y%12===0)?1.25:(y%12===1?0.3:0.55+0.2*n);if(x%32===0)v=0.35;}
    else if(kind==="heap"){var bw=[10,22,14,18],ox=0,bi=0;while(ox+bw[bi%4]<=x){ox+=bw[bi%4];bi++;}var by=(y+bi*7)%20;
      v=(x===ox||by===0)?0.3:0.45+0.5*h(bi,(y+bi*7)/20|0,31);}
    else if(kind==="raw"){v=0.35+0.15*n;if(h(x,y,41)>0.72)v=1.0+0.2*h(x,y,42);}
    else if(kind==="host"){v=0.55+0.25*n;if(px===0||py===0)v=0.3;if((px===5||px===26)&&(py===5||py===26))v=1.3;
      if(px>9&&px<14&&py>9&&py<22)v=py<15?1.3:0.4;if(px>17&&px<22&&py>9&&py<22)v=py<17?0.4:1.3;}
    else if(kind==="keys"){var kx=x%16,ky=y%16;v=(kx<2||ky<2)?0.25:(kx<4||ky<4?1.1:0.7+0.15*n);}
    else if(kind==="screen"){var bar=Math.floor(x/8);v=(N-y)<(10+bar*5+8*h(bar,0,51))?0.9+0.3*h(bar,1,52):0.25+0.1*n;}
    else{v=n;if(px===0||py===0)v=0.38;else if(px===1||py===1)v=1.05;
      if((px===4||px===27)&&(py===4||py===27))v=1.25;
      if(h(x,y,9)>0.985)v*=0.55;
      if(Math.abs((x+y*0.35)%23-11)<0.5&&h(x>>3,y>>3,7)>0.6)v*=0.7;}
    v=Math.max(0,Math.min(1.3,v*(0.88+0.12*h(x,y,77))));i=(y*N+x)*4;dd[i]=dd[i+1]=dd[i+2]=Math.min(255,v*200);dd[i+3]=255;}
  g.putImageData(im,0,0);
  var t=new T.CanvasTexture(c);t.wrapS=t.wrapT=T.RepeatWrapping;t.magFilter=T.NearestFilter;t.minFilter=T.LinearMipmapLinearFilter;return t;}
function texK(T,kind){if(!kind)return GRIT;if(!TEXK[kind])TEXK[kind]=gritTex(T,kind);return TEXK[kind];}
function gritUV(geo,w,h,d,k){var uv=geo.attributes.uv,dims=[[d,h],[d,h],[w,d],[w,d],[w,h],[w,h]];k=k||0.9;
  for(var f=0;f<6;f++)for(var j=0;j<4;j++){var ix=f*4+j;uv.setXY(ix,uv.getX(ix)*dims[f][0]*k,uv.getY(ix)*dims[f][1]*k);}uv.needsUpdate=true;}
function gritScene(T,scene,rd){
  if(!GRIT){GRIT=gritTex(T,"plate");GRATE=gritTex(T,"grate");}
  scene.fog=new T.FogExp2(0x0c0806,0.0085);
  var torch=new T.PointLight(0xff8a3c,0.9,60,1.6);torch.position.set(0,7,4);scene.add(torch);
  return torch;}
function K3(stage,cv,o){
  var T=window.THREE;if(!T)return null;var rd;
  try{rd=new T.WebGLRenderer({canvas:cv,antialias:true,alpha:true});}catch(e){return null;}
  rd.setPixelRatio(Math.min(2,window.devicePixelRatio||1));
  var scene=new T.Scene(),cam=new T.PerspectiveCamera(o.fov||38,2,0.1,400),k={T:T,scene:scene,cam:cam,frames:[],t:0};
  scene.add(new T.AmbientLight(0xffe6cc,0.55));var dl=new T.DirectionalLight(0xfff1dd,0.7);dl.position.set(-6,14,10);scene.add(dl);
  var torch=gritScene(T,scene,rd);k.frames.push(function(dt,now){torch.intensity=0.75+0.18*Math.sin(now/90)+0.1*Math.sin(now/37);});
  k.mat=function(c,e,op,map){return new T.MeshStandardMaterial({color:c,map:map||GRIT,roughness:.85,metalness:.15,emissive:c,emissiveIntensity:e===undefined?0.1:e,transparent:true,opacity:op===undefined?1:op});};
  k.box=function(w,h,d,c,x,y,z,e,op,kind){var geo=new T.BoxGeometry(w,h,d);gritUV(geo,w,h,d,kind==='screen'?0.45:0.9);var m=new T.Mesh(geo,k.mat(c,e,op,(h<0.1&&w>5)?GRATE:texK(T,kind)));m.position.set(x,y+h/2,z);
    m.add(new T.LineSegments(new T.EdgesGeometry(m.geometry),new T.LineBasicMaterial({color:0x000000,transparent:true,opacity:.45})));(k.layer||scene).add(m);m.userData={h:h,y0:y,rise:1,cur:1};return m;};
  /* memory is drawn as a floor plan: a flat, see-through plate with a glowing outline */
  k.plate=function(w,d,c,x,y,z,kind,op){var geo=new T.BoxGeometry(w,0.04,d);gritUV(geo,w,0.04,d,0.9);
    var m=new T.Mesh(geo,new T.MeshStandardMaterial({color:c,map:texK(T,kind),transparent:true,opacity:op||0.5,emissive:c,emissiveIntensity:0.35,roughness:1,metalness:0,depthWrite:false}));
    m.position.set(x,y+0.02,z);var e=new T.LineSegments(new T.EdgesGeometry(geo),new T.LineBasicMaterial({color:c}));m.add(e);
    m.userData={h:0.04,y0:y,rise:1,cur:1,edge:e};(k.layer||scene).add(m);return m;};
  /* labels size their canvas to the text, so nothing is ever cut off */
  /* visibility check: labels that overlap on screen give way to more important ones */
  var labels=[],dcT=0,vp=new T.Vector3(),wp=new T.Vector3();
  function shown(o){for(;o;o=o.parent)if(o.visible===false)return false;return true;}
  function declutter(){var H=stage.clientHeight,W=stage.clientWidth,f=Math.tan(cam.fov*Math.PI/360)*2,boxes=[],live=[];
    labels.forEach(function(s){if(!s.parent||!shown(s)){return;}s.getWorldPosition(wp);vp.copy(wp).project(cam);if(vp.z>1){s.material.opacity=0;return;}
      var px=H/(f*cam.position.distanceTo(wp)),w=s.scale.x*px,h=s.scale.y*px*0.8;live.push({s:s,x:(vp.x+1)*W/2,y:(1-vp.y)*H/2,w:w,h:h,p:s.userData.pri||1});});
    live.sort(function(a,b){return b.p-a.p;});
    live.forEach(function(L){var hit=boxes.some(function(B){return Math.abs(L.x-B.x)*2<L.w+B.w-4&&Math.abs(L.y-B.y)*2<L.h+B.h-2;});
      L.s.material.opacity=hit?0:1;if(!hit)boxes.push(L);});}
  k.frames.push(function(dt){dcT+=dt;if(dcT>0.2){dcT=0;declutter();}});
  function drawLabel(c,txt,col){var g=c.getContext("2d");g.font="56px VT323, 'IBM Plex Mono', monospace";
    var w=Math.ceil(g.measureText(txt).width+34),W=Math.max(256,w+12);if(c.width!==W){c.width=W;g=c.getContext("2d");}else g.clearRect(0,0,c.width,c.height);
    g.font="56px VT323, 'IBM Plex Mono', monospace";g.textAlign="center";g.textBaseline="middle";var cx=W/2;
    g.fillStyle="rgba(14,9,7,.88)";g.fillRect(cx-w/2,10,w,76);g.strokeStyle="rgba(241,178,60,.55)";g.lineWidth=3;g.strokeRect(cx-w/2+1.5,11.5,w-3,73);g.fillStyle="rgba(255,255,255,.06)";g.fillRect(cx-w/2+3,13,w-6,4);
    g.fillStyle=col||"#eadccb";g.fillText(txt,cx,50);return W;}
  k.label=function(txt,x,y,z,col,size){
    var c=document.createElement("canvas");c.width=640;c.height=96;var W=drawLabel(c,txt,col);
    var s=new T.Sprite(new T.SpriteMaterial({map:new T.CanvasTexture(c),transparent:true,depthTest:false}));
    var q=(size||1)*1.75;s.userData.q=q;s.userData.pri=s.userData.pri||(size||1);labels.push(s);s.position.set(x,y,z);s.scale.set(q*W/96*0.5,q*0.5,1);s.renderOrder=10;(k.layer||scene).add(s);return s;};
  /* change a label's text in place: no new sprite, so it can tick every frame boundary */
  k.setLabel=function(s,txt,col){var c=s.material.map.image,W0=c.width,W=drawLabel(c,txt,col);
    if(W!==W0){s.material.map.dispose();s.material.map=new T.CanvasTexture(c);s.material.needsUpdate=true;}else s.material.map.needsUpdate=true;
    var q=s.userData.q||1.75;s.scale.set(q*W/96*0.5,q*0.5,1);};
  k.arc=function(a,b,c,h){var cu=new T.CubicBezierCurve3(a,new T.Vector3(a.x,a.y+h,a.z),new T.Vector3(b.x,b.y+h,b.z),b);
    var m=new T.Mesh(new T.TubeGeometry(cu,60,0.04,6,false),new T.MeshBasicMaterial({color:c,transparent:true,opacity:0}));m.userData={curve:cu,want:0};scene.add(m);return m;};
  k.show=function(m,on){m.userData.rise=on?1:0;};
  k.fadeArc=function(m,v){m.userData.want=v;};
  var dots=[],flows=[];
  k.flow=function(arc,col,dir){flows.push({arc:arc,col:col,dir:dir||1,t0:performance.now()});};
  var F={x:(o.look||[0,0.8,0])[0],y:(o.look||[0,0.8,0])[1],z:(o.look||[0,0.8,0])[2],r:o.r||22},G={x:F.x,y:F.y,z:F.z,r:F.r};
  k.focus=function(x,y,z,r){G={x:x,y:y,z:z,r:r};if(RM)F={x:x,y:y,z:z,r:r};};
  var drag=0,dragging=false,lx=0;
  stage.addEventListener("pointerdown",function(e){dragging=true;lx=e.clientX;stage.setPointerCapture(e.pointerId);});
  stage.addEventListener("pointermove",function(e){if(dragging){drag+=(e.clientX-lx)*0.008;lx=e.clientX;}});
  stage.addEventListener("pointerup",function(){dragging=false;});
  function size(){var w=stage.clientWidth,h=stage.clientHeight;rd.setSize(w,h,false);cam.aspect=w/h;cam.updateProjectionMatrix();}
  window.addEventListener("resize",size);size();
  var vis=false,last=null;
  function frame(now){
    if(!vis){last=null;return;}
    if(last===null)last=now;var dt=Math.min(0.05,(now-last)/1000);last=now;k.t+=dt;
    var e=Math.min(1,dt*2.2);F.x+=(G.x-F.x)*e;F.y+=(G.y-F.y)*e;F.z+=(G.z-F.z)*e;F.r+=(G.r-F.r)*e;
    var narrow=window.innerWidth<620,r=F.r*(narrow?(o.narrowK||1.45):1),a=drag+(RM?0:Math.sin(k.t*0.18)*(o.sway||0.25))+(o.yaw||0);
    cam.position.set(F.x+Math.sin(a)*r,F.y+r*(o.tilt||0.42),F.z+Math.cos(a)*r);cam.lookAt(F.x,F.y,F.z);
    scene.traverse(function(m){var u=m.userData;if(!u)return;
      if(u.rise!==undefined&&u.h!==undefined){u.cur+=(Math.max(0.001,u.rise)-u.cur)*Math.min(1,dt*4);m.scale.y=u.cur;m.position.y=u.y0+u.h*u.cur/2;m.visible=u.cur>0.01;}
      if(u.want!==undefined){m.material.opacity+=(u.want-m.material.opacity)*Math.min(1,dt*4);}});
    dots.forEach(function(m){m.visible=false;});var di=0;
    flows=flows.filter(function(f){var p=(now-f.t0)/1300;if(p>1.2)return false;
      for(var j=0;j<4;j++){var q=p-j*0.08;if(q<0||q>1)continue;var u=f.dir>0?q:1-q;
        if(!dots[di]){dots[di]=new T.Mesh(new T.SphereGeometry(0.12,10,8),new T.MeshBasicMaterial({color:0xffffff}));scene.add(dots[di]);}
        var m=dots[di++];m.visible=true;m.material.color.setHex(f.col);m.position.copy(f.arc.userData.curve.getPoint(u));if(f.arc.parent&&f.arc.parent!==scene)m.position.add(f.arc.parent.position);}
      return true;});
    k.frames.forEach(function(fn){fn(dt,now);});
    rd.render(scene,cam);requestAnimationFrame(frame);
  }
  if("IntersectionObserver" in window)new IntersectionObserver(function(es){es.forEach(function(e){var was=vis;vis=e.isIntersecting;if(vis&&!was)requestAnimationFrame(frame);});}).observe(stage);
  else{vis=true;requestAnimationFrame(frame);}
  return k;
}
function after3d(fn){var done=false;function go(){if(!done){done=true;fn();}}
  if(document.fonts&&document.fonts.ready){document.fonts.ready.then(go);setTimeout(go,2500);}else go();}
var RUN=null;try{RUN=JSON.parse(document.getElementById("mtxrun").textContent);}catch(e){}
function hx(v){return "0x"+(v>>>0).toString(16).toUpperCase();}
