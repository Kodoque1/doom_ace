import sys
V="/tmp/claude-0/-home-user/b9ce7b71-9ea8-5a5f-8922-73aa4e51e9e4/scratchpad/v2/"
P_="/tmp/claude-0/-home-user/b9ce7b71-9ea8-5a5f-8922-73aa4e51e9e4/scratchpad/parts/"
R=lambda n:open(n).read()
head=R(P_+"old_head.txt").replace("<title>DOOMATRYOSHKA</title>","<title>Doomception</title>")+R(P_+"new.css")+R(P_+"m3d.css")+R(V+"v2.css")+"</style>\n"
helpers="\n".join(R(P_+"old_helpers.js").split("\n")[:129])+"\n"
js=helpers+R(P_+"k3d.js")+"\n"+R(V+"drawview.js")+"\n"+R(V+"v2.js")+"\n"+R(V+"anat.js")+"\n})();\n"
run=open("/home/user/doom_ace/nested/docs/mtx-run.json").read().replace("</","<\\/")
out=head+"\n"+R(V+"body.html")+'\n<script type="application/json" id="mtxrun">'+run+'</script>\n<script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>\n<script>'+js+"</script>\n"
open("/tmp/claude-0/-home-user/b9ce7b71-9ea8-5a5f-8922-73aa4e51e9e4/scratchpad/doomception.html","w").write(out)
print(len(out))
