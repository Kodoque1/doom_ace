import sys
V="/tmp/claude-0/-home-user/b9ce7b71-9ea8-5a5f-8922-73aa4e51e9e4/scratchpad/v2/"
P_="/tmp/claude-0/-home-user/b9ce7b71-9ea8-5a5f-8922-73aa4e51e9e4/scratchpad/parts/"
R=lambda n:open(n).read()
head=R(P_+"old_head.txt").replace("<title>DOOMATRYOSHKA</title>",'<title>Doomception</title>\n<meta name="description" content="Three Doom games running inside one DOOM2.EXE, each showing the next on a wall: how it works, and a challenge to write the switch yourself.">\n<meta property="og:type" content="article">\n<meta property="og:title" content="Doomception">\n<meta property="og:description" content="Three Doom games in one DOOM2.EXE. Each one shows the next on a wall.">\n<meta property="og:url" content="https://kodoque1.github.io/doom_ace/">\n<meta property="og:image" content="https://kodoque1.github.io/doom_ace/og.png">\n<meta property="og:image:width" content="1200">\n<meta property="og:image:height" content="630">\n<meta name="twitter:card" content="summary_large_image">\n<meta name="twitter:creator" content="@KODOQUE">\n<meta name="twitter:image" content="https://kodoque1.github.io/doom_ace/og.png">')+R(P_+"new.css")+R(P_+"m3d.css")+R(V+"v2.css")+"</style>\n"
helpers="\n".join(R(P_+"old_helpers.js").split("\n")[:129])+"\n"
js=helpers+R(P_+"k3d.js")+"\n"+R(V+"drawview.js")+"\n"+R(V+"v2.js")+"\n"+R(V+"anat.js")+"\n})();\n"
run=open("/home/user/doom_ace/nested/docs/mtx-run.json").read().replace("</","<\\/")
out=head+"\n"+R(V+"body.html")+'\n<script type="application/json" id="mtxrun">'+run+'</script>\n<script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>\n<script>'+js+"</script>\n"
open("/tmp/claude-0/-home-user/b9ce7b71-9ea8-5a5f-8922-73aa4e51e9e4/scratchpad/doomception.html","w").write(out)
print(len(out))
