/* ═══════════════════════════════════════════════════════════════
   ESP · Color Detector — client app
   All color detection happens here, in the browser. The device only
   receives tiny RGB updates and drives the LED.
   ═══════════════════════════════════════════════════════════════ */
(() => {
  'use strict';

  /* ── tiny DOM helper ─────────────────────────────────────────── */
  const $ = (id) => document.getElementById(id);

  /* ── state ──────────────────────────────────────────────────── */
  const S = {
    secure: window.isSecureContext !== false,
    hasDevice: false,
    board: 'ESP32',
    wsPort: 0,          // >0 on boards that serve WS on a separate insecure port (ESP8266)
    source: 'idle',     // idle | camera | photo
    locked: false,
    bright: 100,
    lin: [0.003, 0.004, 0.008],   // smoothed linear-light RGB (starts near #10131a)
    display: [16, 19, 26],        // current sRGB shown on the card
    hexShown: '10131a',
    hexTarget: '10131a',
    lastCommitT: 0,
    hexChangeT: 0,
    lastGlowT: 0,
    recent: [],
    ws: null,
    usePost: false,
    failCount: 0,
    recT: 0,
    lastOkT: 0,
    lastSendT: 0,
    lastSent: null,
    brightT: 0,
    pulseT: 0,
    stream: null,
    wakeLock: null,
    raf: 0,
    lastSampleT: 0,
    mh: 199, ms: 0.63, mv: 0.84, // manual picker HSV
  };

  /* ── color math ─────────────────────────────────────────────── */
  const h2 = (n) => n.toString(16).padStart(2, '0');
  const hexOf = (r, g, b) => h2(r) + h2(g) + h2(b);
  const hexToRgb = (hx) => [
    parseInt(hx.slice(0, 2), 16),
    parseInt(hx.slice(2, 4), 16),
    parseInt(hx.slice(4, 6), 16),
  ];
  const toLin = (c) => {
    const x = c / 255;
    return x <= 0.04045 ? x / 12.92 : Math.pow((x + 0.055) / 1.055, 2.4);
  };
  const toSrgb = (x) =>
    Math.max(0, Math.min(255, Math.round(
      255 * (x <= 0.0031308 ? x * 12.92 : 1.055 * Math.pow(x, 1 / 2.4) - 0.055))));

  function rgbToHsl(r, g, b) {
    const R = r / 255, G = g / 255, B = b / 255;
    const max = Math.max(R, G, B), min = Math.min(R, G, B);
    let h = 0, s = 0;
    const l = (max + min) / 2;
    if (max !== min) {
      const d = max - min;
      s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
      if (max === R) h = (G - B) / d + (G < B ? 6 : 0);
      else if (max === G) h = (B - R) / d + 2;
      else h = (R - G) / d + 4;
      h /= 6;
    }
    return [Math.round(h * 360), Math.round(s * 100), Math.round(l * 100)];
  }

  function rgbToHsv(r, g, b) {
    const R = r / 255, G = g / 255, B = b / 255;
    const max = Math.max(R, G, B), min = Math.min(R, G, B);
    let h = 0;
    const v = max, d = max - min;
    const s = max === 0 ? 0 : d / max;
    if (d !== 0) {
      if (max === R) h = (G - B) / d + (G < B ? 6 : 0);
      else if (max === G) h = (B - R) / d + 2;
      else h = (R - G) / d + 4;
      h *= 60;
    }
    return [h, s, v];
  }

  function hsvToRgb(h, s, v) {
    h = ((h % 360) + 360) % 360;
    const c = v * s;
    const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
    const m = v - c;
    let r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else { r = c; b = x; }
    return [
      Math.round((r + m) * 255),
      Math.round((g + m) * 255),
      Math.round((b + m) * 255),
    ];
  }

  function rgbToLab(r, g, b) {
    const x = toLin(r), y = toLin(g), z = toLin(b);
    let X = (x * 0.4124 + y * 0.3576 + z * 0.1805) / 0.95047;
    let Y = x * 0.2126 + y * 0.7152 + z * 0.0722;
    let Z = (x * 0.0193 + y * 0.1192 + z * 0.9505) / 1.08883;
    const f = (t) => (t > 0.008856 ? Math.cbrt(t) : 7.787 * t + 16 / 116);
    const fx = f(X), fy = f(Y), fz = f(Z);
    return [116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)];
  }

  /* nearest named color, computed once for the table */
  const NAMES = [
    ['Black', 0, 0, 0], ['Charcoal', 54, 69, 79], ['Gray', 128, 128, 128],
    ['Silver', 192, 192, 192], ['White', 255, 255, 255], ['Ivory', 255, 255, 240],
    ['Beige', 245, 245, 220], ['Tan', 210, 180, 140], ['Sand', 194, 178, 128],
    ['Coffee', 111, 78, 55], ['Brown', 150, 75, 0], ['Chocolate', 123, 63, 0],
    ['Sienna', 160, 82, 45], ['Rust', 183, 65, 14], ['Maroon', 128, 0, 0],
    ['Burgundy', 128, 0, 32], ['Red', 237, 41, 57], ['Crimson', 220, 20, 60],
    ['Salmon', 250, 128, 114], ['Coral', 255, 127, 80], ['Tomato', 255, 99, 71],
    ['Peach', 255, 218, 185], ['Orange', 255, 165, 0], ['Amber', 255, 191, 0],
    ['Gold', 255, 215, 0], ['Yellow', 255, 255, 0], ['Olive', 128, 128, 0],
    ['Lime', 191, 255, 0], ['Chartreuse', 127, 255, 0], ['Green', 0, 128, 0],
    ['Forest Green', 34, 139, 34], ['Emerald', 80, 200, 120], ['Mint', 152, 255, 152],
    ['Sea Green', 46, 139, 87], ['Teal', 0, 128, 128], ['Turquoise', 64, 224, 208],
    ['Cyan', 0, 255, 255], ['Sky Blue', 135, 206, 235], ['Azure', 0, 127, 255],
    ['Blue', 0, 0, 255], ['Navy', 0, 0, 128], ['Indigo', 75, 0, 130],
    ['Violet', 143, 0, 255], ['Lavender', 230, 230, 250], ['Purple', 128, 0, 128],
    ['Plum', 142, 69, 133], ['Orchid', 218, 112, 214], ['Magenta', 255, 0, 255],
    ['Pink', 255, 192, 203], ['Rose', 255, 7, 127],
  ].map((n) => ({ name: n[0], lab: rgbToLab(n[1], n[2], n[3]) }));

  function nearestName(r, g, b) {
    const l = rgbToLab(r, g, b);
    let best = NAMES[0], bd = Infinity;
    for (let i = 0; i < NAMES.length; i++) {
      const d0 = l[0] - NAMES[i].lab[0], d1 = l[1] - NAMES[i].lab[1], d2 = l[2] - NAMES[i].lab[2];
      const d = d0 * d0 + d1 * d1 + d2 * d2;
      if (d < bd) { bd = d; best = NAMES[i]; }
    }
    return best.name;
  }

  /* WCAG-aware ink for the color card */
  function inkFor(r, g, b) {
    const L = 0.2126 * toLin(r) + 0.7152 * toLin(g) + 0.0722 * toLin(b);
    return 1.05 / (L + 0.05) >= (L + 0.05) / 0.05 ? '#f7fafc' : '#0b0d12';
  }

  /* ── DOM refs ────────────────────────────────────────────────── */
  const video = $('video'), photo = $('photo'), stage = $('stage');
  const idle = $('idle'), btnIdleStart = $('btnIdleStart');
  const btnStart = $('btnStart'), btnStartLabel = $('btnStartLabel'), btnStartManual = $('btnStartManual');
  const splash = $('splash'), app = $('app');
  const pill = $('pill'), pillText = $('pillText');
  const hexDigits = $('hexDigits'), nameTag = $('nameTag');
  const rgbVal = $('rgbVal'), hslVal = $('hslVal');
  const card = $('card'), syncChip = $('syncChip');
  const btnLock = $('btnLock'), btnCopy = $('btnCopy'), lockLabel = $('lockLabel'), lockPath = $('lockPath');
  const recent = $('recent');
  const dockCam = $('dockCam'), dockManual = $('dockManual'), dockBright = $('dockBright');
  const scrim = $('scrim'), sheetManual = $('sheetManual'), sheetBright = $('sheetBright');
  const sv = $('sv'), hue = $('hue'), svThumb = $('svThumb'), hueThumb = $('hueThumb');
  const manualHex = $('manualHex'), btnManualSave = $('btnManualSave');
  const brightSlider = $('brightSlider'), brightVal = $('brightVal');
  const toastEl = $('toast'), filePick = $('filePick');
  const hudLive = $('hudLive'), hudBadge = $('hudBadge');
  const errCard = $('errCard'), infoCard = $('infoCard');
  const scanline = $('scanline'), retSwatch = $('retSwatch');

  /* build the six hex digit slots */
  const digits = [];
  for (let i = 0; i < 6; i++) {
    const d = document.createElement('span');
    d.className = 'dg';
    d.textContent = '0';
    hexDigits.appendChild(d);
    digits.push(d);
  }

  /* hidden sampling canvas */
  const sampleCanvas = document.createElement('canvas');
  sampleCanvas.width = 96;
  sampleCanvas.height = 96;
  const sctx = sampleCanvas.getContext('2d', { willReadFrequently: true, alpha: false });

  /* ── color sampling (center circle, trimmed mean) ─────────────── */
  function sampleSource(el) {
    try {
      sctx.drawImage(el, 0, 0, 96, 96);
    } catch (e) { return null; }
    const R = 14, c = 48, W = R * 2;
    const img = sctx.getImageData(c - R, c - R, W, W).data;
    const px = [];
    for (let y = 0; y < W; y++) {
      const dy = y - R + 0.5;
      for (let x = 0; x < W; x++) {
        const dx = x - R + 0.5;
        if (dx * dx + dy * dy > R * R) continue;
        const i = (y * W + x) * 4;
        const r = img[i], g = img[i + 1], b = img[i + 2];
        px.push([r, g, b, 0.2126 * r + 0.7152 * g + 0.0722 * b]);
      }
    }
    if (!px.length) return null;
    px.sort((a, b) => a[3] - b[3]);
    const cut = Math.floor(px.length * 0.2); // reject darkest 20 % + brightest 20 % (specular glints)
    let r = 0, g = 0, b = 0, n = 0;
    for (let i = cut; i < px.length - cut; i++) { r += px[i][0]; g += px[i][1]; b += px[i][2]; n++; }
    return [r / n, g / n, b / n];
  }

  /* ── core: set + render a color ───────────────────────────────── */
  function setColor(rgb, opts) {
    if (!rgb) return;
    const o = opts || {};
    const a = o.snap ? 1 : 0.45; // EMA in linear space
    S.lin[0] += (toLin(rgb[0]) - S.lin[0]) * a;
    S.lin[1] += (toLin(rgb[1]) - S.lin[1]) * a;
    S.lin[2] += (toLin(rgb[2]) - S.lin[2]) * a;
    render(o.send !== false);
  }

  function render(send) {
    const r = toSrgb(S.lin[0]), g = toSrgb(S.lin[1]), b = toSrgb(S.lin[2]);
    S.display = [r, g, b];
    const hex = hexOf(r, g, b);
    const css = 'rgb(' + r + ',' + g + ',' + b + ')';

    /* hero card + reticle swatch */
    document.documentElement.style.setProperty('--hero', css);
    document.documentElement.style.setProperty('--hero-ink', inkFor(r, g, b));

    /* ambient glow (throttled) */
    const now = performance.now();
    if (now - S.lastGlowT > 120) {
      S.lastGlowT = now;
      document.documentElement.style.setProperty('--glow', css);
    }

    /* hex — commits settle, morphs on change */
    if (hex !== S.hexTarget) { S.hexTarget = hex; S.hexChangeT = now; }
    if (hex !== S.hexShown &&
        (now - S.lastCommitT > 250 || now - S.hexChangeT > 350)) {
      commitHex(hex, r, g, b);
    }

    /* transport */
    if (send) sendColor(r, g, b);
  }

  function commitHex(hex, r, g, b) {
    S.lastCommitT = performance.now();
    S.hexShown = hex;
    for (let i = 0; i < 6; i++) {
      const ch = hex[i];
      if (digits[i].textContent !== ch) {
        digits[i].textContent = ch;
        digits[i].classList.remove('flip');
        void digits[i].offsetWidth;
        digits[i].classList.add('flip');
      }
    }
    const hsl = rgbToHsl(r, g, b);
    rgbVal.textContent = 'rgb(' + r + ', ' + g + ', ' + b + ')';
    hslVal.textContent = 'hsl(' + hsl[0] + ', ' + hsl[1] + '%, ' + hsl[2] + '%)';
    nameTag.textContent = nearestName(r, g, b);
  }

  /* ── transport: WebSocket with POST fallback ──────────────────── */
  function wsUrl() {
    return S.wsPort
      ? 'ws://' + location.hostname + ':' + S.wsPort + '/'
      : 'wss://' + location.host + '/ws';
  }

  function connectWS() {
    if (!S.hasDevice || S.usePost) return;
    let closed = false;
    let ws;
    try { ws = new WebSocket(wsUrl()); } catch (e) { scheduleReconnect(); return; }
    S.ws = ws;
    ws.onopen = () => {
      S.failCount = 0;
      setConn('ok', 'Connected to device');
      S.lastOkT = performance.now();
      ws.send('b:' + S.bright);
      if (S.lastSent) ws.send('c:' + S.lastSent);
    };
    ws.onmessage = (e) => {
      if (e.data === 'a') {
        S.lastOkT = performance.now();
        pulseSync();
      }
    };
    ws.onclose = () => {
      if (closed) return;
      closed = true;
      if (S.ws === ws) S.ws = null;
      scheduleReconnect();
    };
    ws.onerror = () => { /* onclose follows */ };
  }

  function scheduleReconnect() {
    S.failCount++;
    if (S.failCount >= 3 && !S.usePost) {
      S.usePost = true; // degrade gracefully to plain HTTPS POSTs
      toast('Using direct link');
    }
    setConn('wait', 'Reconnecting…');
    const delay = Math.min(500 * Math.pow(2, S.failCount), 8000) + Math.random() * 250;
    clearTimeout(S.recT);
    S.recT = setTimeout(connectWS, delay);
  }

  function sendColor(r, g, b) {
    const now = performance.now();
    if (!S.hasDevice || now - S.lastSendT < 55) return;
    if (S.lastSent) {
      const p = hexToRgb(S.lastSent);
      if (Math.abs(r - p[0]) < 3 && Math.abs(g - p[1]) < 3 && Math.abs(b - p[2]) < 3) return;
    }
    S.lastSendT = now;
    const hex = hexOf(r, g, b);
    S.lastSent = hex;
    if (S.usePost) {
      fetch('api/color', {
        method: 'POST',
        headers: { 'Content-Type': 'text/plain' },
        body: r + ',' + g + ',' + b,
      }).then(() => { S.lastOkT = performance.now(); }).catch(() => {});
    } else if (S.ws && S.ws.readyState === 1) {
      S.ws.send('c:' + hex);
    }
  }

  function sendBright() {
    if (!S.hasDevice) return;
    if (S.ws && S.ws.readyState === 1) S.ws.send('b:' + S.bright);
    else fetch('api/color', {
      method: 'POST',
      headers: { 'Content-Type': 'text/plain' },
      body: 'b:' + S.bright,
    }).then(() => { S.lastOkT = performance.now(); }).catch(() => {});
  }

  /* keep-alive + link status */
  setInterval(() => {
    const now = performance.now();
    if (S.ws && S.ws.readyState === 1) {
      if (now - S.lastSendT > 2500) S.ws.send('h'); // heartbeat when idle
    }
    if (S.hasDevice && now - S.lastOkT < 5000) setConn('ok', 'Connected to device');
    else if (S.hasDevice && S.usePost) setConn('wait', 'Reconnecting…');
  }, 2500);

  function pulseSync() {
    const now = performance.now();
    if (now - S.pulseT < 300) return;
    S.pulseT = now;
    syncChip.classList.remove('pulse');
    void syncChip.offsetWidth;
    syncChip.classList.add('pulse');
  }

  function setConn(state, text) {
    pill.dataset.state = state;
    pillText.textContent = text;
  }

  /* ── camera ───────────────────────────────────────────────────── */
  async function startCamera() {
    hideCards();
    if (!S.secure) { pickFile(); return; }
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
      showErrorCard('nocam');
      return;
    }
    let stream;
    try {
      stream = await navigator.mediaDevices.getUserMedia({
        audio: false,
        video: {
          facingMode: { ideal: 'environment' },
          width: { ideal: 1280 },
          height: { ideal: 720 },
        },
      });
    } catch (err) {
      showErrorCard(
        err && (err.name === 'NotAllowedError' || err.name === 'PermissionDeniedError')
          ? 'perm' : 'nocam');
      return;
    }
    S.stream = stream;
    video.srcObject = stream;
    try { await video.play(); } catch (e) { /* autoplay may be pending */ }
    S.source = 'camera';
    video.hidden = false;
    photo.hidden = true;
    idle.hidden = true;
    stage.classList.add('live', 'breathe');
    stage.classList.remove('locked');
    setLocked(false);
    hudLive.textContent = 'LIVE';
    dockCam.setAttribute('aria-pressed', 'true');
    runScanline();
    keepAwake(true);
    if (!S.raf) { S.lastSampleT = 0; S.raf = requestAnimationFrame(loop); }
  }

  function stopCamera() {
    if (S.stream) {
      S.stream.getTracks().forEach((t) => t.stop());
      S.stream = null;
    }
    video.srcObject = null;
    video.hidden = true;
    if (S.source !== 'photo') photo.hidden = true;
    idle.hidden = false;
    S.source = 'idle';
    stage.classList.remove('live');
    dockCam.setAttribute('aria-pressed', 'false');
    keepAwake(false);
  }

  function runScanline() {
    scanline.classList.remove('sweep');
    void scanline.offsetWidth;
    scanline.classList.add('sweep');
    setTimeout(() => scanline.classList.remove('sweep'), 1300);
  }

  /* analysis loop — ~20 fps, paused automatically when hidden */
  function loop(t) {
    S.raf = requestAnimationFrame(loop);
    if (document.hidden) return;
    if (S.source === 'camera' && !S.locked && video.readyState >= 2 && t - S.lastSampleT >= 50) {
      S.lastSampleT = t;
      const rgb = sampleSource(video);
      if (rgb) setColor(rgb, {});
    }
  }

  /* ── photo capture (insecure-context fallback) ────────────────── */
  function pickFile() {
    filePick.click();
  }

  filePick.addEventListener('change', () => {
    const file = filePick.files && filePick.files[0];
    if (!file) return;
    const url = URL.createObjectURL(file);
    photo.onload = () => {
      URL.revokeObjectURL(url);
      S.source = 'photo';
      photo.hidden = false;
      video.hidden = true;
      idle.hidden = true;
      stage.classList.add('live');
      hudLive.textContent = 'PHOTO';
      stage.classList.remove('locked');
      setLocked(false);
      const rgb = sampleSource(photo);
      if (rgb) setColor(rgb, { snap: true });
    };
    photo.src = url;
    filePick.value = '';
  });

  /* ── lock / copy / recents ────────────────────────────────────── */
  function setLocked(v) {
    S.locked = v;
    btnLock.setAttribute('aria-pressed', v ? 'true' : 'false');
    lockLabel.textContent = v ? 'Unlock' : 'Lock';
    lockPath.setAttribute('d', v ? 'M7 11V7a5 5 0 0 1 9.9-1' : 'M7 11V7a5 5 0 0 1 10 0v4');
    hudBadge.hidden = !v;
    stage.classList.toggle('locked', v);
    if (v) { addRecent(S.hexShown); haptic(8); }
  }

  async function copyHex() {
    const hex = '#' + S.hexShown;
    let ok = false;
    try {
      if (navigator.clipboard && navigator.clipboard.writeText) {
        await navigator.clipboard.writeText(hex);
        ok = true;
      }
    } catch (e) { /* fall through */ }
    if (!ok) {
      try {
        const ta = document.createElement('textarea');
        ta.value = hex;
        ta.style.position = 'fixed';
        ta.style.opacity = '0';
        document.body.appendChild(ta);
        ta.select();
        ok = document.execCommand('copy');
        ta.remove();
      } catch (e) { /* ignore */ }
    }
    toast(ok ? 'Copied ' + hex : 'Copy code: ' + hex);
    haptic(8);
  }

  function addRecent(hex) {
    if (S.recent[0] === hex) return;
    S.recent.unshift(hex);
    if (S.recent.length > 6) S.recent.pop();
    renderRecent();
  }

  function renderRecent() {
    recent.textContent = '';
    S.recent.forEach((hex) => {
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'swatch';
      b.style.background = '#' + hex;
      b.title = '#' + hex;
      b.setAttribute('aria-label', 'Apply color ' + hex);
      b.addEventListener('click', () => {
        setColor(hexToRgb(hex), { snap: true });
        toast('#' + hex);
      });
      recent.appendChild(b);
    });
    recent.hidden = S.recent.length === 0;
  }

  /* ── manual picker (HSV) ──────────────────────────────────────── */
  const svCtx = sv.getContext('2d', { alpha: false });
  const hueCtx = hue.getContext('2d', { alpha: false });
  let manualOpen = false;

  function sizeCanvas(cv, ctx) {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const w = cv.clientWidth, h = cv.clientHeight;
    if (!w || !h) return false;
    cv.width = Math.round(w * dpr);
    cv.height = Math.round(h * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    return true;
  }

  function drawHue() {
    if (!hue.clientWidth) return;
    sizeCanvas(hue, hueCtx);
    const w = hue.clientWidth, h = hue.clientHeight;
    const g = hueCtx.createLinearGradient(0, 0, w, 0);
    g.addColorStop(0, '#f00'); g.addColorStop(0.17, '#ff0');
    g.addColorStop(0.33, '#0f0'); g.addColorStop(0.5, '#0ff');
    g.addColorStop(0.67, '#00f'); g.addColorStop(0.83, '#f0f');
    g.addColorStop(1, '#f00');
    hueCtx.fillStyle = g;
    hueCtx.fillRect(0, 0, w, h);
  }

  function drawSV() {
    if (!sv.clientWidth) return;
    sizeCanvas(sv, svCtx);
    const w = sv.clientWidth, h = sv.clientHeight;
    const base = hsvToRgb(S.mh, 1, 1);
    let g = svCtx.createLinearGradient(0, 0, w, 0);
    g.addColorStop(0, '#fff');
    g.addColorStop(1, 'rgb(' + base[0] + ',' + base[1] + ',' + base[2] + ')');
    svCtx.fillStyle = g;
    svCtx.fillRect(0, 0, w, h);
    g = svCtx.createLinearGradient(0, 0, 0, h);
    g.addColorStop(0, 'rgba(0,0,0,0)');
    g.addColorStop(1, '#000');
    svCtx.fillStyle = g;
    svCtx.fillRect(0, 0, w, h);
  }

  function placeThumbs() {
    svThumb.style.left = (S.ms * 100) + '%';
    svThumb.style.top = ((1 - S.mv) * 100) + '%';
    hueThumb.style.left = ((S.mh / 360) * 100) + '%';
  }

  function applyManual() {
    placeThumbs();
    const rgb = hsvToRgb(S.mh, S.ms, S.mv);
    manualHex.textContent = '#' + hexOf(rgb[0], rgb[1], rgb[2]);
    setColor(rgb, { snap: true });
  }

  function padDrag(cv, fn) {
    let active = false;
    const move = (e) => {
      if (!active && e.type !== 'pointerdown') return;
      const r = cv.getBoundingClientRect();
      const x = Math.max(0, Math.min(1, (e.clientX - r.left) / r.width));
      const y = Math.max(0, Math.min(1, (e.clientY - r.top) / r.height));
      fn(x, y);
    };
    cv.addEventListener('pointerdown', (e) => {
      active = true;
      cv.setPointerCapture(e.pointerId);
      move(e);
    });
    cv.addEventListener('pointermove', move);
    cv.addEventListener('pointerup', () => { active = false; });
    cv.addEventListener('pointercancel', () => { active = false; });
  }

  padDrag(sv, (x, y) => { S.ms = x; S.mv = 1 - y; applyManual(); });
  padDrag(hue, (x) => { S.mh = x * 360; drawSV(); applyManual(); });

  /* keyboard support for the picker */
  sv.addEventListener('keydown', (e) => {
    const step = e.shiftKey ? 0.15 : 0.05;
    if (e.key === 'ArrowLeft') { S.ms = Math.max(0, S.ms - step); }
    else if (e.key === 'ArrowRight') { S.ms = Math.min(1, S.ms + step); }
    else if (e.key === 'ArrowUp') { S.mv = Math.min(1, S.mv + step); }
    else if (e.key === 'ArrowDown') { S.mv = Math.max(0, S.mv - step); }
    else return;
    e.preventDefault();
    applyManual();
  });
  hue.addEventListener('keydown', (e) => {
    const step = e.shiftKey ? 12 : 4;
    if (e.key === 'ArrowLeft') { S.mh = Math.max(0, S.mh - step); }
    else if (e.key === 'ArrowRight') { S.mh = Math.min(360, S.mh + step); }
    else return;
    e.preventDefault();
    drawSV();
    applyManual();
  });

  function syncManualFromColor() {
    const p = rgbToHsv(S.display[0], S.display[1], S.display[2]);
    S.mh = p[0]; S.ms = p[1]; S.mv = p[2];
    drawHue();
    drawSV();
    applyManual();
  }

  /* ── sheets ───────────────────────────────────────────────────── */
  function openSheet(el) {
    scrim.hidden = false;
    el.hidden = false;
    requestAnimationFrame(() => {
      requestAnimationFrame(() => el.classList.add('open'));
    });
    if (el === sheetManual) {
      manualOpen = true;
      syncManualFromColor();
    }
  }

  function closeSheets() {
    sheetManual.classList.remove('open');
    sheetBright.classList.remove('open');
    manualOpen = false;
    setTimeout(() => { scrim.hidden = true; }, 420);
  }

  scrim.addEventListener('click', closeSheets);
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') closeSheets();
  });

  /* ── toast + haptics ──────────────────────────────────────────── */
  let toastT = 0;
  function toast(msg) {
    toastEl.textContent = msg;
    toastEl.hidden = false;
    toastEl.classList.add('show');
    clearTimeout(toastT);
    toastT = setTimeout(() => toastEl.classList.remove('show'), 1900);
  }
  const haptic = (ms) => { if (navigator.vibrate) { navigator.vibrate(ms); } };

  /* ── error / info cards ──────────────────────────────────────── */
  const IC_WARN = '<svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="M10.29 3.86 1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/></svg>';
  const IC_IMG = '<svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><rect x="3" y="3" width="18" height="18" rx="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/></svg>';

  function fillCard(el, opts) {
    el.className = 'stageCard' + (opts.warn ? ' warn' : '');
    el.innerHTML = opts.icon +
      '<h3>' + opts.title + '</h3>' +
      '<p>' + opts.body + '</p>' +
      '<div class="acts"></div>';
    const acts = el.querySelector('.acts');
    opts.acts.forEach((a) => {
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'btn ' + (a.primary ? 'primary' : 'ghost');
      b.textContent = a.label;
      b.addEventListener('click', a.fn);
      acts.appendChild(b);
    });
    el.hidden = false;
  }

  function hideCards() {
    errCard.hidden = true;
    infoCard.hidden = true;
  }

  function showErrorCard(kind) {
    if (kind === 'perm') {
      fillCard(errCard, {
        warn: true, icon: IC_WARN,
        title: 'Camera access is off',
        body: 'Permission was declined, so the live preview cannot start. Allow camera access for this page in your browser settings, then try again.',
        acts: [
          { label: 'Try again', primary: true, fn: startCamera },
          { label: 'Use manual mode', fn: () => { hideCards(); openSheet(sheetManual); } },
        ],
      });
    } else {
      fillCard(errCard, {
        warn: true, icon: IC_WARN,
        title: 'No camera found',
        body: 'This device reports no usable camera. You can still capture a color from a photo, or pick one by hand.',
        acts: [
          { label: 'Capture a photo', primary: true, fn: pickFile },
          { label: 'Use manual mode', fn: () => { hideCards(); openSheet(sheetManual); } },
        ],
      });
    }
  }

  function showInfoCard() {
    fillCard(infoCard, {
      warn: false, icon: IC_IMG,
      title: 'Live camera needs a secure page',
      body: 'This board serves a plain HTTP page, and browsers only open the camera on secure (HTTPS) pages. You can still capture a photo or pick a color by hand. For the full live experience, use an ESP32, ESP32-S3 or ESP32-C3 — they serve HTTPS directly.',
      acts: [
        { label: 'Capture a photo', primary: true, fn: pickFile },
        { label: 'Manual mode', fn: () => { hideCards(); openSheet(sheetManual); } },
      ],
    });
  }

  /* ── wake lock + lifecycle ────────────────────────────────────── */
  async function keepAwake(on) {
    try {
      if (on) {
        if (navigator.wakeLock && !S.wakeLock) {
          S.wakeLock = await navigator.wakeLock.request('screen');
        }
      } else if (S.wakeLock) {
        await S.wakeLock.release();
        S.wakeLock = null;
      }
    } catch (e) { /* unsupported or rejected */ }
  }

  document.addEventListener('visibilitychange', () => {
    if (document.hidden) keepAwake(false);
    else if (S.source === 'camera') keepAwake(true);
  });
  window.addEventListener('pagehide', stopCamera);
  window.addEventListener('beforeunload', stopCamera);

  window.addEventListener('resize', () => {
    if (manualOpen) { drawHue(); drawSV(); }
  });

  /* ── events ──────────────────────────────────────────────────── */
  function enterApp(manual) {
    splash.classList.add('out');
    setTimeout(() => { splash.remove(); }, 600);
    app.hidden = false;
    if (manual) openSheet(sheetManual);
  }

  btnStart.addEventListener('click', () => { enterApp(false); startCamera(); });
  btnStartManual.addEventListener('click', () => enterApp(true));
  btnIdleStart.addEventListener('click', startCamera);

  btnLock.addEventListener('click', () => setLocked(!S.locked));
  btnCopy.addEventListener('click', copyHex);
  btnManualSave.addEventListener('click', () => {
    addRecent(S.hexShown);
    toast('Saved to recent');
  });

  dockCam.addEventListener('click', () => {
    if (S.source === 'camera') stopCamera();
    else startCamera();
  });
  dockManual.addEventListener('click', () => openSheet(sheetManual));
  dockBright.addEventListener('click', () => openSheet(sheetBright));

  stage.addEventListener('click', (e) => {
    if (e.target.closest('button') || e.target.closest('.stageCard')) return;
    if (S.source === 'camera') setLocked(!S.locked);
    else if (S.source !== 'photo') startCamera();
  });

  brightSlider.addEventListener('input', () => {
    const v = Number(brightSlider.value);
    S.bright = v;
    brightVal.textContent = v + '%';
    brightSlider.style.setProperty('--fill', v + '%');
    if (S.brightT) return;
    S.brightT = setTimeout(() => { S.brightT = 0; sendBright(); }, 160);
  });

  /* ── boot ─────────────────────────────────────────────────────── */
  async function boot() {
    try {
      const r = await fetch('api/status', { cache: 'no-store' });
      if (r.ok) {
        const st = await r.json();
        S.hasDevice = true;
        S.board = st.board || 'ESP32';
        S.wsPort = st.wsPort || 0;
        if (typeof st.brightness === 'number') {
          S.bright = Math.max(0, Math.min(100, st.brightness));
          brightSlider.value = S.bright;
          brightVal.textContent = S.bright + '%';
          brightSlider.style.setProperty('--fill', S.bright + '%');
        }
      }
    } catch (e) { /* dev preview — run standalone */ }

    if (!S.secure) {
      btnStartLabel.textContent = 'Capture a Photo';
      $('idleTitle').textContent = 'Live camera unavailable';
      $('idleSub').textContent = 'Capture a photo, or pick a color manually.';
      btnIdleStart.textContent = 'Capture a Photo';
      showInfoCard();
    }

    commitHex('10131a', 16, 19, 26);
    if (S.hasDevice) connectWS();
    else setConn('demo', 'Preview');
  }

  boot();
})();
