// Simulation der RC_Common-Teile, die alle Module gleich haben:
// /api/uartroles, /api/otaupdate, /api/backup/*, /api/funk, /api/funk/reset.
// Gleiche Datei in allen Repos (pruefstand/mock_common.js).
PST.common = function (o) {
  // o: { backupData(): Objekt fuer eine Sicherung, funkRole(): 'Zentrale'|'Teilnehmer'|null,
  //      funkCfg(): {id,ch,pwr,rate,back}, name(): Geraetename, addr(): Modul-Adresse }
  var backups = [{ name: '001_2026-11-18_2010.json', size: 1830, auto: 0 }, { name: '002_auto_2026-11-19_0815.json', size: 1830, auto: 1 }];
  var store = {};
  var funkReset = 0;
  function funkStatus() {
    var role = o.funkRole ? o.funkRole() : null;
    var c = o.funkCfg ? o.funkCfg() : { id: 1, ch: 1, pwr: 20, rate: 10, back: true };
    if (!role) return { role: 'aus', name: o.name(), addr_nr: o.addr(), addr: '0x' + (0xC0 + o.addr()).toString(16).toUpperCase(),
      mac: '24:6F:28:AA:10:01', id: c.id, ch: c.ch, pwr: c.pwr, rate: c.rate, back: c.back, connected: false, lost_ms: 0,
      loss: 0, rssi: 0, conflict: false, dup_addr: false,
      own: { ch_cable: 0, ch_funk: 0, frames_out: 0, frames_in: 0, sent: 0, noack: 0, qfull: 0, senderr: 0, crc: 0, stale: 0, rate_drop: 0 },
      total: { frames_out: 0, frames_in: 0, sent: 0, wr_bytes: 0, wr_resync: 0, back_drop: 0, no_peer: 0 }, peers: [] };
    var z = role === 'Zentrale';
    var k = funkReset ? 0 : 1;
    return { role: role, name: o.name(), addr_nr: o.addr(), addr: '0x' + (0xC0 + o.addr()).toString(16).toUpperCase(),
      mac: '24:6F:28:AA:10:01', id: c.id, ch: c.ch, pwr: c.pwr, rate: c.rate, back: c.back, connected: true, lost_ms: 0,
      loss: 0.2, rssi: -48, conflict: false, dup_addr: false,
      own: { ch_cable: z ? 250 : 0, ch_funk: z ? 0 : 99, frames_out: z ? 260 : 12, frames_in: z ? 12 : 260, sent: z ? 260 : 12,
        noack: 0, qfull: 0, senderr: 0, crc: 0, stale: 0, rate_drop: z ? 150 : 0 },
      total: { frames_out: 51234 * k, frames_in: 2210 * k, sent: 51234 * k, wr_bytes: 812345 * k, wr_resync: 0, back_drop: 0, no_peer: 0 },
      peers: [{ name: z ? 'Sound Mini' : 'Zentrale', role: z ? 'Teilnehmer' : 'Zentrale', partner: true, addr: z ? '0xC2' : '0xC0',
        mac: '24:6F:28:AA:10:02', pps: z ? 12 : 99, loss: 0.2, rssi: -48, alive: true, drops: 0, gap: 38, up: '12 min' }] };
  }
  return {
    handle: function (m, path, q, body) {
      if (path === '/api/uartroles') return PST.json({ roles: [{ id: 0, name: 'Aus', baud: 0 }, { id: 1, name: 'CRSF', baud: 420000 },
        { id: 2, name: 'S.Port', baud: 57600 }, { id: 3, name: 'GPS', baud: 1 }, { id: 4, name: 'Raw', baud: 115200 }],
        bauds: [1, 9600, 19200, 38400, 57600, 115200, 230400, 420000], auto: 1 });
      if (path === '/api/otaupdate') { PST.note('Firmware-Update simuliert: Modul würde jetzt neu starten.'); return PST.text('OK'); }
      if (path === '/api/backup/list') return PST.json({ ok: true, files: backups, free: 1048576 });
      if (path === '/api/backup/save') {
        var nr = backups.length + 1, name = ('00' + nr).slice(-3) + (q.auto === '1' ? '_auto_' : '_') + (q.when || '2026-11-20_0900') + '.json';
        store[name] = body; backups.push({ name: name, size: (body || '').length, auto: q.auto === '1' ? 1 : 0 });
        return PST.json({ ok: true, file: name });
      }
      if (path === '/api/backup/get' && q.file) {
        if (store[q.file]) return PST.text(store[q.file]);
        return PST.json(o.backupData ? o.backupData() : {});
      }
      if (path === '/api/backup/get' && !q.file) return PST.json({ ok: false, error: 'bad_name' }, 400);
      if (path === '/api/backup/delete') { backups = backups.filter(function (b) { return b.name !== q.file; }); return PST.json({ ok: true }); }
      if (path === '/api/funk') return PST.json(funkStatus());
      if (path === '/api/funk/reset') { funkReset = 1; return PST.json({ ok: true }); }
      return null;
    },
    funkSummary: function () {
      var role = o.funkRole ? o.funkRole() : null;
      if (!role) return null;
      return { role: role, connected: true, peers: 1, loss: 0.2, rssi: -48, conflict: false, dup_addr: false };
    }
  };
};
// Werte fuer die Lua-Ordner "Funk" (gleich in allen Modulen mit Funk)
PST.funkLua = function (cfg, getMode, setMode) {
  var PWR = [2, 5, 8, 11, 15, 20], RATE = [0, 5, 10, 20, 50];
  return {
    get: {
      luaGetFunkMode: function () { return getMode(); },
      luaGetFunk: function (i) { return [cfg.id, cfg.ch, Math.max(0, PWR.indexOf(cfg.pwr)), Math.max(0, RATE.indexOf(cfg.rate)), cfg.back ? 1 : 0][i]; }
    },
    set: {
      luaSetFunkMode: function (a, v) { setMode(v); },
      luaSetFunk: function (i, v) { if (i === 0) cfg.id = v; if (i === 1) cfg.ch = v; if (i === 2) cfg.pwr = PWR[v]; if (i === 3) cfg.rate = RATE[v]; if (i === 4) cfg.back = !!v; }
    },
    views: {
      luaShowFunkFolder: function (it, v) { var m = getMode(); v.name = m ? 'Funk (' + (m === 1 ? 'Zentrale' : 'Teilnehmer') + ', verbunden)' : 'Funk (aus)'; },
      luaViewFunkSet: function (it, v) { if (!getMode()) v.hidden = true; },
      luaViewFunkRate: function (it, v) { if (getMode() !== 1) v.hidden = true; },
      luaViewFunkBack: function (it, v) { if (getMode() !== 2) v.hidden = true; },
      luaViewFunkInfo: function (it, v) { v.text = getMode() ? '1 Gegenst. 0.2% -48dBm' : 'Funk aus'; }
    }
  };
};
PST.mergeLua = function (dst) {
  for (var i = 1; i < arguments.length; i++) {
    var s = arguments[i];
    ['get', 'set', 'views'].forEach(function (k) { dst[k] = dst[k] || {}; PST.merge(dst[k], s[k] || {}); });
  }
  return dst;
};

// Allgemeine Simulation aus den Beispielantworten, die build.py aus dem
// Firmware-Code erzeugt (ctx.variant.api: {"GET /api/config": {...}}).
// GET liefert den aktuellen Stand, POST mit JSON- oder Formular-Daten traegt
// jedes Feld dort ein, wo es in einer GET-Antwort vorkommt (auch verschachtelt).
// opts.patch: Startwerte ueberschreiben, opts.alias: {"post_feld": "get_feld"},
// opts.routes: eigene Behandlung je "METHODE /pfad" oder "/pfad".
PST.autoMock = function (ctx, opts) {
  opts = opts || {};
  var api = JSON.parse(JSON.stringify(ctx.variant.api || {}));
  function deepPatch(dst, src) {
    Object.keys(src || {}).forEach(function (k) {
      if (src[k] && typeof src[k] === 'object' && !Array.isArray(src[k]) && dst[k] && typeof dst[k] === 'object') deepPatch(dst[k], src[k]);
      else dst[k] = src[k];
    });
  }
  Object.keys(opts.patch || {}).forEach(function (r) { if (api[r]) deepPatch(api[r], opts.patch[r]); });
  function setDeep(o, key, val, done) {
    if (!o || typeof o !== 'object') return done;
    if (Array.isArray(o)) { o.forEach(function (x) { done = setDeep(x, key, val, done); }); return done; }
    Object.keys(o).forEach(function (k) {
      if (k === key && (typeof o[k] !== 'object' || o[k] === null)) {
        var t = typeof o[k];
        o[k] = t === 'number' ? (val === true ? 1 : val === false ? 0 : Number(val)) : t === 'boolean' ? (val === true || val === 1 || val === '1' || val === 'true') : val;
        done = true;
      } else if (o[k] && typeof o[k] === 'object') done = setDeep(o[k], key, val, done);
    });
    return done;
  }
  var M = {
    api: api,
    get: function (path) { return api['GET ' + path]; },
    apply: function (body) {
      var d = PST.parseBody(body);
      Object.keys(d).forEach(function (k) {
        var key = (opts.alias || {})[k] || k;
        Object.keys(api).forEach(function (r) { setDeep(api[r], key, d[k], false); });
      });
      return d;
    },
    handle: function (m, path, q, body) {
      var h = (opts.routes || {})[m + ' ' + path] || (opts.routes || {})[path];
      if (h) { var r = h(q, body, m); if (r) return r; }
      if (api[m + ' ' + path]) return PST.json(api[m + ' ' + path]);
      if (m === 'GET' && api['ANY ' + path]) return PST.json(api['ANY ' + path]);
      if (m !== 'GET') {
        M.apply(body);
        if (api['ANY ' + path]) return PST.json(api['ANY ' + path]);
        return PST.json({ ok: true });
      }
      return null;
    }
  };
  return M;
};

// Gemeinsamer Teil der Soundmodule (Sound Mini, ESP32-RC-Sound, Multiswitch-
// Sound): Sound-Dateien, Zuordnung, Test, Sicherungen auf SD und die
// Lua-Ordner Motor, Sounds und Sound-Zuordnung (gleiche Funktionsnamen in
// allen Sound-Firmwares).
// o: { C: Konfiguration wie GET /api/config bzw. /api/sconfig (motor, sounds,
//      sound_files), store: 'Flash'|'SD-Karte', total: Bytes, pins(): true wenn
//      die Platine PWM-/Pin-Eingaenge hat (ESP32-RC-Sound V1/V2), sport(): S.Port aktiv }
PST.soundCore = function (o) {
  var C = o.C;
  var files = [{ name: 'loop.wav', size: 182044 }, { name: 'shut.wav', size: 96020 }, { name: 'start.wav', size: 120400 },
    { name: 'sound1.wav', size: 40210 }, { name: 'sound2.wav', size: 61002 }, { name: 'Horn.wav', size: 52230 }];
  // Reihenfolge wie in der Firmware: Motor-Plaetze, dann Sound 1..N
  // (JavaScript sortiert Zahlen-Schluessel sonst nach vorne)
  var SLOTS = Object.keys(C.sound_files).filter(function (k) { return isNaN(+k); })
    .concat(Object.keys(C.sound_files).filter(function (k) { return !isNaN(+k); }).sort(function (a, b) { return a - b; }));
  var LABEL = SLOTS.map(function (k) { return k === 'loop' ? 'Loop' : k === 'shut' ? 'Standgas' : k === 'start' ? 'Start' : 'Sound ' + k; });
  var backups = [{ name: 'config_0001_v' + String(o.version || '').replace('.', '') + '.json', size: 2210 }];
  var testOn = {};
  function pins() { return o.pins ? o.pins() : false; }
  function used() { return files.reduce(function (a, f) { return a + Math.ceil(f.size / 4096) * 4096 + 8192; }, 8192); }
  function fsize(n) { var f = files.filter(function (x) { return x.name.toLowerCase() === String(n).toLowerCase(); })[0]; return f ? f.size : 0; }
  function soundCfg(id) { return id === 0 ? C.motor : C.sounds[id - 1]; }
  function slotOf(key) { return SLOTS.indexOf(String(key)); }
  var S = {
    files: function () { return files; }, slots: SLOTS, labels: LABEL, used: used, fsize: fsize, testOn: testOn,
    soundFiles: function () {
      var s = {};
      SLOTS.forEach(function (k) { var f = C.sound_files[k]; s[k] = { file: f, exists: !!f && fsize(f) > 0, size: fsize(f) }; });
      return { ok: true, slots: s };
    },
    soundLib: function () {
      return { ok: true, maxlen: 24, store: o.store, total: o.total, used: used(), more: false,
        files: files.slice().sort(function (a, b) { return a.name.localeCompare(b.name); }),
        slots: SLOTS.map(function (k, i) { return { key: k, label: LABEL[i], file: C.sound_files[k] }; }) };
    },
    sportJson: function () {
      var on = o.sport ? o.sport() : false;
      function pk(x) { return { online: x, cells: x ? 3 : 0, total: x ? 11.82 : 0, min: x ? 3.93 : 0, soc: x ? 78 : 0, cv: x ? [3.95, 3.94, 3.93, 0, 0, 0] : [0, 0, 0, 0, 0, 0] }; }
      return { s0: pk(on), s1: pk(false), pid0: (C.sport || {}).poll_id0 || 'A1', pid1: (C.sport || {}).poll_id1 || '22' };
    }
  };
  S.routes = {
    '/api/soundfiles': function () { return PST.json(S.soundFiles()); },
    '/api/soundlib': function () { return PST.json(S.soundLib()); },
    '/api/sound': function (q, b) {
      var d = PST.parseBody(b), id = +d.id, s = soundCfg(id);
      if (!s) return PST.json({ error: 'bad request' }, 400);
      if (d.test) { PST.note((id ? 'Sound ' + id : 'Motor') + ' wird abgespielt (simuliert).'); return PST.json({ ok: true }); }
      ['source', 'vol', 'mode'].forEach(function (k) { if (d[k] != null) s[k] = +d[k]; });
      if (id === 0) {
        if (d.motor_mode != null) C.motor.mode = +d.motor_mode;
        ['speed_src', 'toggle', 'rpm_min', 'rpm_max', 'shutdown_s', 'ramp', 'deadband'].forEach(function (k) { if (d[k] != null) C.motor[k] = +d[k]; });
      }
      return PST.json({ ok: true });
    },
    '/api/soundmap': function (q, b) {
      var d = PST.parseBody(b), i = slotOf(d.slot);
      if (i < 0) return PST.json({ error: 'bad request' }, 400);
      C.sound_files[SLOTS[i]] = d.file || '';
      return PST.json({ ok: true });
    },
    '/api/soundupload': function (q) {
      var n = q.name || 'neu.wav';
      if (!/^[A-Za-z0-9_\-]+\.wav$/i.test(n)) return PST.json({ ok: false, error: 'bad_name' }, 400);
      var existed = !!fsize(n);
      files = files.filter(function (f) { return f.name !== n; }); files.push({ name: n, size: 48000 });
      if (q.slot && slotOf(q.slot) >= 0) C.sound_files[q.slot] = n;
      return PST.json({ ok: true, existed: existed, file: n });
    },
    '/api/sounddelete': function (q) { files = files.filter(function (f) { return f.name !== q.name; }); return PST.json({ ok: true }); },
    '/api/sdexport': function () {
      var n = 'config_' + ('000' + (backups.length + 1)).slice(-4) + '_v' + String(o.version || '').replace('.', '') + '.json';
      backups.push({ name: n, size: JSON.stringify(C).length, data: JSON.parse(JSON.stringify(C)) });
      return PST.json({ ok: true, file: 'backup/' + n });
    },
    '/api/sdlist': function () { return PST.json({ ok: true, files: backups.map(function (b) { return { name: b.name, size: b.size }; }) }); },
    '/api/sdconfig': function (q) {
      var b = backups.filter(function (x) { return x.name === q.file; })[0];
      if (!b) return PST.json({ ok: false, error: 'not_found' }, 404);
      return PST.json(b.data || C);
    },
    '/api/sddelete': function (q) { backups = backups.filter(function (x) { return x.name !== q.file; }); return PST.json({ ok: true }); },
    '/sport': function () { return PST.json(S.sportJson()); }
  };
  // ── Lua (Motor, Sounds, Sound-Zuordnung) ──
  function src(i) { return soundCfg(i).source; }
  function setSrc(i, v) { soundCfg(i).source = v; }
  function typ(c) { if (c === 999) return 8; if (c === 200) return 7; if (c >= 80 && c <= 95) return 6; if (c >= 70 && c <= 77) return 5;
    if (c >= 60 && c <= 65) return 4; if (c >= 50 && c <= 55) return 3; if (c >= 40 && c <= 45) return 2; if (c >= 20 && c <= 35) return 1; if (c >= 0 && c <= 15) return 0; return 8; }
  function nr(c) { var B = [[80, 95], [70, 77], [60, 65], [50, 55], [40, 45], [20, 35], [0, 15]];
    for (var i = 0; i < B.length; i++) if (c >= B[i][0] && c <= B[i][1]) return c - B[i][0] + 1; return 1; }
  function setEin(t, n) { var i = n > 0 ? n - 1 : 0, base = [0, 20, 40, 50, 60, 70, 80], lim = [15, 15, 5, 5, 5, 7, 15];
    if (t <= 6) return base[t] + Math.min(i, lim[t]); return t === 7 ? 200 : 999; }
  function selVal(c) { var t = typ(c); if (pins()) return t; if (t >= 2 && t <= 4) return 5; return t === 5 ? 2 : t === 6 ? 3 : t === 7 ? 4 : t >= 8 ? 5 : t; }
  function selWrite(v, old) { var t = pins() ? v : ([0, 1, 5, 6, 7, 8][v] != null ? [0, 1, 5, 6, 7, 8][v] : 8); return setEin(t, nr(old)); }
  function maxNr(t) { return t === 0 || t === 1 ? 16 : t >= 2 && t <= 4 ? 6 : t === 5 ? 8 : t === 6 ? 16 : 1; }
  function lock(v) { if (v.value < 1 || v.value > 16) v.value = 1; v.min = v.max = v.value; v.hidden = true; }
  function srcText(cfg) { var N = ['L', 'H', 'PWM-L', 'PWM-H', 'Pin', 'EK', 'MKan', 'Dauer', 'Aus'], t = typ(cfg);
    if (!pins() && t >= 2 && t <= 4) t = 8; return t <= 6 ? N[t] + ' ' + nr(cfg) : N[t]; }
  function spdTyp(c) { return c >= 0 && c <= 15 ? 0 : c >= 20 && c <= 25 ? 1 : 2; }
  function spdNr(c) { return c >= 0 && c <= 15 ? c + 1 : c >= 20 && c <= 25 ? c - 19 : 1; }
  function setSpd(t, n) { var i = n > 0 ? n - 1 : 0; return t === 0 ? Math.min(i, 15) : t === 1 ? 20 + Math.min(i, 5) : 999; }
  function chView(s, v) { var t = typ(src(s)); if (t === 7 || t === 8 || (!pins() && t >= 2 && t <= 4)) lock(v); else v.max = maxNr(t); }
  var sndSel = 1, mapSlot = 3;
  function fileList() {
    var cur = C.sound_files[SLOTS[mapSlot]], names = [];
    if (cur) names.push(cur);
    files.slice().sort(function (a, b) { return a.name.localeCompare(b.name); }).forEach(function (f) { if (f.name.toLowerCase() !== String(cur).toLowerCase()) names.push(f.name); });
    return names;
  }
  function shortName(n) { var s = n.replace(/\.wav$/i, ''); return s.length > 12 ? s.slice(0, 11) + '~' : s; }
  var M = C.motor;
  S.srcText = srcText;
  S.lua = {
    get: {
      luaGetThrottleMode: function () { return M.mode; }, luaGetToggle: function () { return M.toggle; },
      luaGetMinSpeed: function () { return M.rpm_min; }, luaGetMaxSpeed: function () { return Math.max(50, Math.min(255, M.rpm_max >> 1)); },
      luaGetShutdown: function () { return M.shutdown_s; }, luaGetRamp: function () { return M.ramp; }, luaGetDeadBand: function () { return M.deadband; },
      luaGetSpdSrc: function () { var t = spdTyp(M.speed_src); return !pins() ? (t === 0 ? 0 : 1) : t; },
      luaGetSpdChannel: function () { return spdNr(M.speed_src); },
      luaGetSource: function (s) { return selVal(src(s)); }, luaGetChannel: function (s) { return nr(src(s)); },
      luaGetVolume: function (s) { return soundCfg(s).vol; }, luaGetPlayMode: function (s) { return soundCfg(s).mode; },
      luaGetSndNr: function () { return sndSel - 1; }, luaGetZero: function () { return 0; }, luaGetSndZero: function () { return 0; },
      luaGetSelSource: function () { return selVal(src(sndSel)); }, luaGetSelChannel: function () { return nr(src(sndSel)); },
      luaGetSelVolume: function () { return soundCfg(sndSel).vol; }, luaGetSelPlayMode: function () { return soundCfg(sndSel).mode; },
      luaGetSelTest: function () { return testOn[sndSel] ? 1 : 0; },
      luaGetMapSlot: function () { return mapSlot; }, luaGetMapFile: function () { return C.sound_files[SLOTS[mapSlot]] ? 1 : 0; }
    },
    set: {
      luaSetThrottleMode: function (a, v) { M.mode = v; }, luaSetToggle: function (a, v) { M.toggle = v; },
      luaSetMinSpeed: function (a, v) { M.rpm_min = v; }, luaSetMaxSpeed: function (a, v) { M.rpm_max = Math.max(100, Math.min(510, v * 2)); },
      luaSetShutdown: function (a, v) { M.shutdown_s = v; }, luaSetRamp: function (a, v) { M.ramp = v; }, luaSetDeadBand: function (a, v) { M.deadband = v; },
      luaSetSpdSrc: function (a, v) { var t = !pins() && v === 1 ? 2 : v; M.speed_src = setSpd(t, spdNr(M.speed_src)); },
      luaSetSpdChannel: function (a, v) { M.speed_src = setSpd(spdTyp(M.speed_src), v); },
      luaSetSource: function (s, v) { setSrc(s, selWrite(v, src(s))); }, luaSetChannel: function (s, v) { setSrc(s, setEin(typ(src(s)), v)); },
      luaSetVolume: function (s, v) { soundCfg(s).vol = v; }, luaSetPlayMode: function (s, v) { soundCfg(s).mode = v; },
      luaSetSndNr: function (a, v) { sndSel = v + 1; },
      luaSetSelSource: function (a, v) { setSrc(sndSel, selWrite(v, src(sndSel))); }, luaSetSelChannel: function (a, v) { setSrc(sndSel, setEin(typ(src(sndSel)), v)); },
      luaSetSelVolume: function (a, v) { soundCfg(sndSel).vol = v; }, luaSetSelPlayMode: function (a, v) { soundCfg(sndSel).mode = v; },
      luaSetSelTest: function (a, v) { testOn[sndSel] = v === 1; if (v) PST.note('Sound ' + sndSel + ' wird abgespielt (simuliert).'); },
      luaSetMapSlot: function (a, v) { mapSlot = v; },
      luaSetMapFile: function (a, v) { var l = fileList(); C.sound_files[SLOTS[mapSlot]] = v === 0 ? '' : (l[v - 1] || ''); }
    },
    views: {
      luaShowMotorFolder: function (it, v) { v.name = 'Motor (' + srcText(M.source) + ')'; },
      luaShowSource: function (it, v) { v.text = pins() ? 'L;H;PWM-L;PWM-H;Pin;EK;MKan;Dauer;Aus' : 'L;H;EK;MKan;Dauer;Aus'; },
      luaShowSpdSrc: function (it, v) { v.text = pins() ? 'BUS Kanal;PWM Pin;Deaktiviert' : 'BUS Kanal;Deaktiviert'; },
      luaShowSpdChannel: function (it, v) { var t = spdTyp(M.speed_src); if (t === 2 || (!pins() && t === 1)) lock(v); else v.max = t === 1 ? 6 : 16; },
      luaShowChannel: function (it, v) { chView(it.arg, v); },
      luaShowSelChannel: function (it, v) { chView(sndSel, v); },
      luaShowSndName: function (it, v) { var f = (C.sound_files[String(sndSel)] || '').replace(/\.wav$/i, ''); v.text = f ? f.slice(0, 20) : '(leer)'; v.max = 0; v.value = 0; },
      luaViewMapFile: function (it, v) { var l = fileList(); v.text = ['(leer)'].concat(l.map(shortName)).join(';'); v.max = l.length; v.value = C.sound_files[SLOTS[mapSlot]] ? 1 : 0; },
      luaShowMapSize: function (it, v) { var f = C.sound_files[SLOTS[mapSlot]]; v.text = !f ? '-' : (fsize(f) ? Math.ceil(fsize(f) / 1024) + ' KB' : 'Datei fehlt'); v.max = 0; }
    }
  };
  return S;
};
