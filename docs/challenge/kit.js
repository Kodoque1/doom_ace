/* kit.js - runs the kit's own C programs, compiled to WebAssembly by web/build.sh, on
 * an in-memory disk. They are the same cc, as and check that `make check` uses, which
 * is why the page's verdicts match the local kit's. It works in a browser, in a
 * worker and in Node. */
var Kit = (function () {
  var enc = new TextEncoder(), dec = new TextDecoder();

  /* "you/../kit/kit.h" -> "kit/kit.h" */
  function norm(p) {
    var out = [];
    p.split("/").forEach(function (s) { if (s === "..") out.pop(); else if (s && s !== ".") out.push(s); });
    return out.join("/");
  }

  /* A disk: path -> bytes */
  function Disk(init) { this.f = new Map(); if (init) for (var k in init) this.put(k, init[k]); }
  Disk.prototype.put = function (p, data) { this.f.set(norm(p), typeof data === "string" ? enc.encode(data) : data); };
  Disk.prototype.get = function (p) { return this.f.get(norm(p)); };
  Disk.prototype.text = function (p) { var d = this.get(p); return d ? dec.decode(d) : null; };

  function Exit(code) { this.code = code; }

  /* The imports every program gets: files on `disk`, the arguments, and output. */
  function imports(disk, args, out, getMem) {
    var H = [null, null, null];
    function mem() { return new Uint8Array(getMem().buffer); }
    function str(p, n) { return dec.decode(mem().slice(p, p + n)); }
    return {
      handles: H,
      kit: {
        open: function (p, n, w) {
          var path = norm(str(p, n));
          if (w) { H.push({ path: path, w: true, chunks: [] }); return H.length - 1; }
          var d = disk.get(path);
          if (!d) return -1;
          H.push({ path: path, data: d });
          return H.length - 1;
        },
        size: function (h) { return H[h] && H[h].data ? H[h].data.length : 0; },
        read: function (h, pos, dst, n) {
          var d = H[h] && H[h].data;
          if (!d || pos >= d.length) return 0;
          var k = Math.min(n, d.length - pos);
          mem().set(d.subarray(pos, pos + k), dst);
          return k;
        },
        write: function (h, src, n) {
          var bytes = mem().slice(src, src + n);
          if (h === 1 || h === 2) out(h, dec.decode(bytes));
          else if (H[h] && H[h].w) H[h].chunks.push(bytes);
        },
        close: function (h) { commit(H[h]); H[h] = null; },
        exit: function (code) { throw new Exit(code); },
        argc: function () { return args.length; },
        arg: function (i, dst, max) {
          var b = enc.encode(args[i]).slice(0, max - 1);
          var m = mem(); m.set(b, dst); m[dst + b.length] = 0;
          return b.length;
        }
      }
    };
    function commit(h) {
      if (!h || !h.w || h.done) return;
      var n = 0; h.chunks.forEach(function (c) { n += c.length; });
      var all = new Uint8Array(n), o = 0;
      h.chunks.forEach(function (c) { all.set(c, o); o += c.length; });
      disk.put(h.path, all);
      h.done = true;
    }
  }

  /* Run a program: a compiled WebAssembly.Module, its arguments (argv[0] first).
     Resolves to { code, out, err }. A fresh instance each time: the programs keep
     their state in globals. */
  function run(module, disk, args) {
    var outS = "", errS = "", inst;
    var imp = imports(disk, args, function (h, s) { if (h === 1) outS += s; else errS += s; }, function () { return inst.exports.memory; });
    return WebAssembly.instantiate(module, { kit: imp.kit }).then(function (i) {
      inst = i;
      var code = 0;
      try { inst.exports.start(); }
      catch (e) { if (e instanceof Exit) code = e.code; else { errS += String(e); code = 99; } }
      /* files a program left open are still written */
      imp.handles.forEach(function (h) {
        if (h && h.w && !h.done) {
          var n = 0; h.chunks.forEach(function (c) { n += c.length; });
          var all = new Uint8Array(n), o = 0; h.chunks.forEach(function (c) { all.set(c, o); o += c.length; });
          disk.put(h.path, all);
        }
      });
      return { code: code, out: outS, err: errS };
    });
  }

  /* The machine, for stepping: one instance kept alive. */
  function Sim(module, disk) {
    var self = this, inst;
    this.console = "";
    var imp = imports(disk, ["sim"], function (h, s) { self.console += s; }, function () { return inst.exports.memory; });
    this.ready = WebAssembly.instantiate(module, { kit: imp.kit }).then(function (i) { inst = i; self.x = i.exports; return self; });
  }
  Sim.prototype.boot = function (args) {
    var a = new Int32Array(this.x.memory.buffer, this.x.sim_args(), 8);
    for (var i = 0; i < 8; i++) a[i] = args[i] || 0;
    this.console = "";
    return this.x.sim_boot();
  };
  Sim.prototype.mem = function () { return new Uint8Array(this.x.memory.buffer, this.x.sim_mem(), 65536); };
  Sim.prototype.save = function () { var p = this.x.sim_state(), n = this.x.sim_state_size(); return new Uint8Array(this.x.memory.buffer, p, n).slice(); };
  Sim.prototype.restore = function (s) { new Uint8Array(this.x.memory.buffer, this.x.sim_state(), s.length).set(s); this.x.sim_clear_events(); };
  /* read and clear the event log */
  Sim.prototype.events = function () {
    var n = this.x.sim_nevents(), v = new Uint32Array(this.x.memory.buffer, this.x.sim_events(), n * 7), out = [];
    for (var i = 0; i < n; i++) out.push({ kind: v[i * 7], a: v[i * 7 + 1], b: v[i * 7 + 2], c: v[i * 7 + 3], sp: v[i * 7 + 4], step: v[i * 7 + 5] + v[i * 7 + 6] * 4294967296 });
    this.x.sim_clear_events();
    return out;
  };
  Sim.prototype.writes = function () {
    var n = this.x.sim_nwrites(), v = new Uint16Array(this.x.memory.buffer, this.x.sim_writes(), 256);
    return { n: n, a: v };
  };
  Sim.prototype.fault = function () {
    var p = this.x.sim_fault(), m = new Uint8Array(this.x.memory.buffer), e = p;
    while (m[e]) e++;
    return dec.decode(m.slice(p, e));
  };

  return { Disk: Disk, run: run, Sim: Sim, norm: norm };
})();
if (typeof module !== "undefined") module.exports = Kit;
