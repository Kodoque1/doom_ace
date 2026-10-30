
(function(){
"use strict";
var RM = window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;
function $(id){return document.getElementById(id);}
function hex(v,n){v=v>>>0;var s=v.toString(16).toUpperCase();while(s.length<(n||8))s="0"+s;return "0x"+s;}
function fly(root,fromEl,toEl,html,cls,delay,dur){
  if(RM||!root||!fromEl||!toEl||!root.animate)return 0;
  var R=root.getBoundingClientRect(),a=fromEl.getBoundingClientRect(),b=toEl.getBoundingClientRect();
  var g=document.createElement("div");g.className="ghost"+(cls?" "+cls:"");g.innerHTML=html;
  g.style.left=(a.left-R.left)+"px";g.style.top=(a.top-R.top)+"px";g.style.minWidth=Math.min(a.width,180)+"px";
  root.appendChild(g);
  var dx=b.left-a.left+(b.width-Math.min(a.width,180))/2,dy=b.top-a.top;dur=dur||750;
  var an=g.animate([{transform:"translate(0,0)",opacity:0},{transform:"translate(0,0)",opacity:1,offset:0.12},{transform:"translate("+dx+"px,"+dy+"px)",opacity:1,offset:0.85},{transform:"translate("+dx+"px,"+dy+"px)",opacity:0}],{duration:dur,delay:delay||0,easing:"cubic-bezier(.5,0,.3,1)",fill:"both"});
  an.onfinish=function(){g.remove();};
  return (delay||0)+dur*0.85;
}
/* The sequence player. Reader-paced: Next / Back, with Play optional. A step
   can be a prediction: the scene stops, asks, and reveals the answer when the
   reader picks one (or presses Next). Back replays the earlier steps instantly. */
var SEQS=[];
document.addEventListener("keydown",function(e){
  if(e.key!=="ArrowRight"&&e.key!=="ArrowLeft")return;
  var t=e.target;if(t&&(t.tagName==="INPUT"||t.tagName==="TEXTAREA"||t.isContentEditable))return;
  var best=null;SEQS.forEach(function(q){if(q.ratio>0.5&&(!best||q.ratio>best.ratio))best=q;});
  if(!best)return;e.preventDefault();best[e.key==="ArrowRight"?"next":"back"]();
});
function Seq(root,o){
  var steps=[],i=0,timer=null,playing=false,want=false,vis=false,wait=null,self=this,scrolly=false,chain=null;
  var bar=document.createElement("div");bar.className="seqc";
  bar.innerHTML='<button data-q="back" aria-label="Back one step">◀ Back</button><button class="primary" data-q="next">Next ▶</button><button data-q="play">Play</button><button data-q="restart">Restart</button><input type="range" class="scrub" data-q="scrub" min="0" max="1" value="0" aria-label="Scrub through the steps"><span class="hudtxt" data-q="pos"></span>';
  (o.controlsIn||root).appendChild(bar);
  var pq=document.createElement("div");pq.className="predict";pq.hidden=true;pq.setAttribute("aria-live","polite");bar.parentNode.insertBefore(pq,bar.nextSibling);
  self.bar=bar;
  var bPlay=bar.querySelector('[data-q=play]'),pos=bar.querySelector('[data-q=pos]'),scrub=bar.querySelector('[data-q=scrub]');
  var sl=document.createElement("details");sl.className="steplist";sl.innerHTML="<summary>All steps, as a list</summary><ol></ol>";
  function listSteps(){var ol=sl.querySelector("ol"),h="",any=false;
    steps.forEach(function(s,k){var t=s.predict?"<em>Predict:</em> "+s.predict.q:(s.say||s.cap||"");if(t&&!s.predict)any=true;if(t)h+='<li value="'+(k+1)+'"><button data-g="'+(k+1)+'">'+t+'</button></li>';});
    ol.innerHTML=h;sl.hidden=!any;
    ol.querySelectorAll("button").forEach(function(b){b.addEventListener("click",function(){want=false;var g=+b.getAttribute("data-g");if(steps[g-1]&&steps[g-1].predict){replay(g-1);one();}else replay(g);});});}
  function mark(){sl.querySelectorAll("li").forEach(function(li){li.classList.toggle("now",+li.value===i);});}
  function label(){scrub.max=steps.length;scrub.value=i;mark();bPlay.textContent=playing?"Pause":"Play";pos.textContent=(i===0&&!wait?"press Next or → · ":"")+"step "+Math.min(i,steps.length)+" / "+steps.length;}
  function exec(s){if(s.run)s.run();if(s.say!==undefined&&o.say)o.say.innerHTML=s.say;if(o.onStep)o.onStep(i);}
  function ask(p){
    wait=p;bar.parentNode.insertBefore(pq,bar.nextSibling);pq.hidden=false;
    pq.innerHTML='<div class="pq-q">Predict: '+p.q+'</div><div class="pq-o">'+p.options.map(function(t,k){return '<button data-k="'+k+'">'+t+'</button>';}).join("")+'</div><div class="pq-a"></div>';
    pq.querySelectorAll("button").forEach(function(b){b.addEventListener("click",function(){reveal(+b.getAttribute("data-k"));});});
  }
  function reveal(k){
    var p=wait;if(!p)return;wait=null;
    pq.querySelectorAll("button").forEach(function(b,j){b.disabled=true;if(j===p.a)b.className="ok";else if(j===k)b.className="no";});
    pq.querySelector(".pq-a").innerHTML=(k===p.a?"<b class='good'>Right.</b> ":k<0?"":"<b class='bad'>Not this time.</b> ")+p.why+" Press <b>Next</b> to see it.";
    label();
  }
  function one(){
    if(wait){reveal(-1);return "wait";}
    if(i>=steps.length){if(o.loop){i=0;if(o.reset)o.reset();if(o.onLoop)o.onLoop();}else{stop();return false;}}
    var s=steps[i++];
    if(s.predict&&scrolly){label();return s;}
    if(s.predict){ask(s.predict);stop();label();return "wait";}
    pq.hidden=true;exec(s);label();return s;
  }
  function tick(){timer=null;if(!playing)return;var s=one();if(s===false||s==="wait")return;timer=setTimeout(tick,RM?Math.max(s.ms||600,900):(s.ms||600));}
  function play(){if(playing)return;playing=true;label();tick();}
  function stop(){playing=false;if(timer){clearTimeout(timer);timer=null;}label();}
  function replay(n){
    var keep=RM;RM=true;stop();wait=null;pq.hidden=true;i=0;if(o.reset)o.reset();if(o.say)o.say.innerHTML=o.intro||"";
    while(i<n){var s=steps[i++];if(!s.predict)exec(s);}
    RM=keep;label();
  }
  self.load=function(list){stop();steps=list;i=0;wait=null;pq.hidden=true;if(o.reset)o.reset();if(o.say)o.say.innerHTML=o.intro||"";listSteps();(o.listIn||root).appendChild(sl);label();
    var anchors=[];steps.forEach(function(s,k){if(s.say||s.predict)anchors.push(k);});
    if(!scrolly&&o.say&&o.scrolly!==false&&anchors.length>1&&anchors.length<=24)makeScrolly(anchors);};
  function runTo(n){if(chain){clearTimeout(chain);chain=null;}stop();
    if(n<=i-1||n<i){replay(n);return;}
    if(n-i>3){replay(n-1);}
    (function go(){if(i>=n)return;var s=one();if(s===false)return;chain=setTimeout(go,RM?0:Math.min((s&&s.ms)||600,650));})();}
  function makeScrolly(anchors){
    scrolly=true;root.classList.add("scrolly");
    var st=document.createElement("div");st.className="sc-stage";
    var col=document.createElement("div");col.className="sc-steps";
    var keep=[];[].slice.call(root.children).forEach(function(c){
      if(c===bar||c===pq||c===sl||c===o.say||c.tagName==="FIGCAPTION"||c.classList.contains("console-head"))keep.push(c);else st.appendChild(c);});
    var head=root.querySelector(".console-head");if(head)col.appendChild(head);
    anchors.forEach(function(k,j){var s=steps[k],d=document.createElement("div");d.className="stp";
      var upto=(j+1<anchors.length?anchors[j+1]:steps.length);
      if(s.predict){d.classList.add("stp-q");var P=s.predict;
        d.innerHTML='<div class="pq-q">Predict: '+P.q+'</div><div class="pq-o">'+P.options.map(function(t,z){return '<button data-k="'+z+'">'+t+'</button>';}).join("")+'</div><div class="pq-a"></div>';
        d.querySelectorAll("button").forEach(function(b){b.addEventListener("click",function(){var z=+b.getAttribute("data-k");d.querySelectorAll("button").forEach(function(x,y){x.disabled=true;if(y===P.a)x.className="ok";else if(y===z)x.className="no";});
          d.querySelector(".pq-a").innerHTML=(z===P.a?"<b class='good'>Right.</b> ":"<b class='bad'>Not this time.</b> ")+P.why+" Keep scrolling to see it.";});});}
      else d.innerHTML=s.say;
      d.setAttribute("data-upto",upto);col.appendChild(d);});
    var fc=root.querySelector("figcaption");if(fc)col.appendChild(fc);
    var tail=document.createElement("div");tail.className="stp-tail";col.appendChild(tail);
    root.appendChild(st);root.appendChild(col);
    bar.classList.add("sc-bar");st.appendChild(bar);if(o.say)o.say.hidden=true;sl.hidden=true;pq.hidden=true;
    var stps=[].slice.call(col.querySelectorAll(".stp"));self.col=col;
    /* text → scene: clicking a paragraph makes it the current step */
    col.addEventListener("click",function(e){if(e.target.closest("a,button,.lk,.j,.pq-o"))return;var st=e.target.closest(".stp");if(!st)return;var r=st.getBoundingClientRect();window.scrollBy(0,r.top+r.height/2-innerHeight/2);});
    /* the root is this document, not the top window: inside a cross-origin frame (the artifact viewer) the top-window
       root ignores rootMargin, every visible step would count as "current", and the steps would run on their own */
    if("IntersectionObserver" in window){var cb=function(es){if(Date.now()<self.quiet)return;es.forEach(function(e){if(!e.isIntersecting)return;
      stps.forEach(function(x){x.classList.toggle("on",x===e.target);});runTo(+e.target.getAttribute("data-upto"));});},io;
      try{io=new IntersectionObserver(cb,{root:document,rootMargin:"-42% 0px -48% 0px"});}catch(err){io=new IntersectionObserver(cb,{rootMargin:"-42% 0px -48% 0px"});}
      stps.forEach(function(x){io.observe(x);});}
    setTimeout(function(){window.dispatchEvent(new Event("resize"));},50);
  }
  self.goto=function(n){want=false;replay(Math.max(0,Math.min(steps.length,n)));};
  self.pos=function(){return i;};
  scrub.addEventListener("input",function(){want=false;replay(+scrub.value);});
  bPlay.addEventListener("click",function(){if(playing){want=false;stop();}else{want=true;if(wait)reveal(-1);play();}});
  /* scene → text: the scene's own Next and Back bring the matching sentence into view */
  function syncCol(){if(!self.col)return;var el=null;[].slice.call(self.col.querySelectorAll(".stp")).forEach(function(x){if(+x.getAttribute("data-upto")<=i)el=x;});
    if(!el)el=self.col.querySelector(".stp");if(!el)return;self.quiet=Date.now()+700;[].slice.call(self.col.querySelectorAll(".stp")).forEach(function(x){x.classList.toggle("on",x===el);});
    var r=el.getBoundingClientRect();window.scrollBy(0,r.top+r.height/2-innerHeight/2);}
  function nextF(){want=false;stop();one();syncCol();}
  bar.querySelector('[data-q=next]').addEventListener("click",nextF);
  function backF(){want=false;var n=wait?i-1:i-1;
    if(wait){wait=null;}
    while(n>0&&steps[n-1]&&steps[n-1].predict)n--;
    replay(Math.max(0,n));syncCol();}
  bar.querySelector('[data-q=back]').addEventListener("click",backF);
  var reg={root:root,ratio:0,next:nextF,back:backF};SEQS.push(reg);
  bar.querySelector('[data-q=restart]').addEventListener("click",function(){self.load(steps);});
  if("IntersectionObserver" in window)new IntersectionObserver(function(es){es.forEach(function(e){vis=e.isIntersecting;reg.ratio=e.intersectionRatio;if(vis&&want)play();else if(!vis)stop();});},{threshold:[0,0.25,0.5,0.75,1]}).observe(root);
  else vis=true;
  label();
}
function esc(s){return String(s).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;");}
