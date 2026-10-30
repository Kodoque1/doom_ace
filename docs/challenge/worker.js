/* worker.js - runs the checker off the page's main thread, so the page stays live. */
importScripts("kit.js");
var mod = null;
onmessage = function (e) {
  var d = new Kit.Disk();
  Object.keys(e.data.files).forEach(function (k) { d.put(k, e.data.files[k]); });
  (mod ? Promise.resolve(mod) : WebAssembly.compile(e.data.mod)).then(function (m) {
    mod = m;
    return Kit.run(m, d, ["check", "-d", "build/disk", "-s", "build/game.sym"]);
  }).then(function (r) { postMessage(r); }, function (err) { postMessage({ code: 99, out: "", err: String(err) }); });
};
