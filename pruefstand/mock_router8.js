// Simulierter CRSF-Router8 (Firmware RC_CRSF_Router_RP2350). Keine
// Weboberflaeche, nur das Lua-Menue. Startwerte wie cfgDefaults()
// (router_config.cpp), Port-Anzahl, Namen und Pins aus dem Code.
PST.register('mock_router8', function (variant, ctx) {
  var K = ctx.variant.consts || {}, N = K.PORT_COUNT || 7;
  function fresh() {
    var c = { moduleAddr: 15, voltMode: 0, voltCorr: 0, tempSend: 0, ledRgb: 1, baud: [], rc: [], ls: [], tele: [] };
    for (var i = 0; i < N; i++) { c.baud.push(K.PORT_BAUD_DEFAULT); c.rc.push(1); c.ls.push(1); c.tele.push(0); }
    return c;
  }
  var c = fresh(), boot = JSON.parse(JSON.stringify(c)), restart = 0;
  // Beispiel: an Port 1 und 2 haengt je ein Modul
  var DEV = [['RC-Multiswitch@0'], ['Sound-Mini@1'], [], [], [], [], []];
  var t0 = Date.now();
  function cnt(a) { var t = (Date.now() - t0) / 1000; return [Math.round(t * 150), Math.round(t * 24), 0, Math.round(t * 3), 0][a]; }
  var lua = {
    get: {
      luaGetPort: function (a) { return c.baud[a]; }, luaGetRc: function (a) { return c.rc[a]; }, luaGetLs: function (a) { return c.ls[a]; },
      luaGetTele: function (a) { return c.tele[a]; }, luaGetAddr: function () { return c.moduleAddr; },
      luaGetVMode: function () { return c.voltMode; }, luaGetVCorr: function () { return c.voltCorr + 5; }, luaGetTSend: function () { return c.tempSend; },
      luaGetLed: function () { return c.ledRgb; }, luaGetSearch: function () { return 0; }, luaGetRestart: function () { return restart; }
    },
    set: {
      luaSetPort: function (a, v) { c.baud[a] = v; }, luaSetRc: function (a, v) { c.rc[a] = v; }, luaSetLs: function (a, v) { c.ls[a] = v; },
      luaSetTele: function (a, v) { c.tele[a] = v; }, luaSetAddr: function (a, v) { c.moduleAddr = v; },
      luaSetVMode: function (a, v) { c.voltMode = v; }, luaSetVCorr: function (a, v) { c.voltCorr = v - 5; }, luaSetTSend: function (a, v) { c.tempSend = v; },
      luaSetLed: function (a, v) { c.ledRgb = v; },
      luaSetSearch: function (a, v) { if (v) PST.note('Geräte werden gesucht (simuliert).'); },
      luaSetRestart: function (a, v) { if (v) { restart = 1; setTimeout(function () { restart = 0; PST.reboot(); }, 300); } }
    },
    views: {
      luaShowVersion: function (it, v) { v.text = ctx.variant.version; },
      luaShowName: function (it, v) { v.text = K.DEVICE_NAME; },
      luaViewVCorr: function (it, v) { if (!c.voltMode) v.hidden = true; },
      luaShowVolt: function (it, v) { v.text = (c.voltMode ? 7.84 + c.voltCorr / 10 : 0).toFixed(2) + ' V'; },
      luaShowTemp: function (it, v) { v.text = '38.2 C'; },
      luaShowCount: function (it, v) { v.text = String(cnt(it.arg)); },
      luaShowDup: function (it, v) { v.text = 'nein'; },
      luaShowPorts: function (it, v) { var s = []; for (var i = 0; i < N; i++) s.push(!boot.baud[i] ? '-' : DEV[i].length ? String(i + 1) : '.'); v.text = s.join(' '); },
      luaShowDevices: function (it, v) { var i = it.arg; v.text = !boot.baud[i] ? 'aus' : DEV[i].length ? DEV[i].join(', ') : 'kein Geraet'; }
    }
  };
  return {
    handle: function () { return null; }, get: lua.get, set: lua.set, views: lua.views,
    deviceName: function () { return K.DEVICE_NAME; },
    reboot: function () { boot = JSON.parse(JSON.stringify(c)); },
    hw: function () {
      var r = [['Board', 'Waveshare RP2350-Zero'], ['Eingang', 'RX ' + K.PIN_UPLINK_RX + ' / TX ' + K.PIN_UPLINK_TX]];
      for (var i = 1; i <= N; i++) r.push(['Port ' + i, 'RX ' + K['PIN_P' + i + '_RX'] + ' / TX ' + K['PIN_P' + i + '_TX']]);
      return r.concat([['Status-LED', 'GPIO ' + K.PIN_STATUS_LED], ['Spannungsmessung', 'GPIO ' + K.PIN_VSENSE]]);
    }
  };
});
