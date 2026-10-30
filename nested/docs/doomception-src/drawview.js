function drawView(c,d,health,frame,half,garbled,wall){
  var g=c.getContext("2d"),W=c.width,H=c.height,col=DCCSS[d],sway=Math.sin(frame*0.35)*10,hb=Math.round(H*0.78);
  var top=half?Math.floor(hb/2):0;
  g.save();g.beginPath();g.rect(0,top,W,H-top);g.clip();
  var gr=g.createLinearGradient(0,0,0,hb);gr.addColorStop(0,"#120c0a");gr.addColorStop(0.5,"#2a1d16");gr.addColorStop(1,"#120c0a");g.fillStyle=gr;g.fillRect(0,0,W,hb);
  var cx=W/2+sway,cy=hb/2;g.strokeStyle=col;g.lineWidth=2;g.globalAlpha=0.9;
  for(var i=1;i<6;i++){var s=i/6,x0=cx-(W*0.55)*s,x1=cx+(W*0.55)*s,y0=cy-(hb*0.5)*s,y1=cy+(hb*0.5)*s;g.globalAlpha=0.25+0.12*i;g.strokeRect(x0,y0,x1-x0,y1-y0);}
  g.globalAlpha=1;g.fillStyle=col;g.fillRect(cx-7,cy-4,14,12);g.fillStyle="#0b0807";g.fillRect(cx-3,cy-1,2,2);g.fillRect(cx+1,cy-1,2,2);
  /* the screen-wall: a wall texture that is another depth's last frame (the ring) */
  var wx=Math.round(W*0.64+sway*0.4),wy=Math.round(hb*0.16),ww=Math.round(W*0.3),wh=Math.round(hb*0.5);
  g.fillStyle="#0b0807";g.fillRect(wx-2,wy-2,ww+4,wh+4);if(wall){g.drawImage(wall,wx,wy,ww,wh);g.strokeStyle="#eadccb";g.lineWidth=1;g.strokeRect(wx-1.5,wy-1.5,ww+3,wh+3);}else{g.fillStyle="#2a1d16";g.fillRect(wx,wy,ww,wh);}
  g.fillStyle="#1a1210";g.fillRect(0,hb,W,H-hb);g.fillStyle=col;g.fillRect(0,hb,W,2);
  g.font="bold "+Math.round(H*0.15)+"px VT323, monospace";g.textBaseline="middle";g.fillStyle="#ffd27a";g.fillText("HEALTH "+health+"%",6,hb+(H-hb)/2);
  g.fillStyle="#bfb2a2";g.textAlign="right";g.fillText("f"+frame,W-6,hb+(H-hb)/2);g.textAlign="left";
  if(garbled){for(var j=0;j<60;j++){g.fillStyle=j%2?"#ff4a32":"#000";g.fillRect(Math.random()*W,Math.random()*H,8+Math.random()*30,2+Math.random()*6);}}
  g.restore();
  if(half){g.fillStyle="rgba(255,74,50,.85)";g.fillRect(0,top-1,W,2);}
}
