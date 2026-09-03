/* ClaimLedger atlas — 3D spectral field, leave-one-out bridges, year slices. */
(() => {
  const FIELDS = {
    cs: "#6ea8fe",
    stat: "#e6b35a",
    math: "#d48cff",
    physics: "#ff8b6b",
    qbio: "#7dffb3",
    "q-bio": "#7dffb3",
    path: "#aaaaaa",
    cycle: "#aaaaaa",
    k: "#aaaaaa",
  };

  const state = {
    embedding: null,
    bridges: null,
    meta: null,
    ledger: null,
    timeline: null,
    dim: 3,
    onlyBridges: false,
    selected: null,
    hover: null,
    query: "",
    yearIdx: -1,
    rot: 0.55,
    tilt: 0.62,
    drag: null,
    auto: true,
    knn: [],
    stars: [],
    layoutCache: null,
    peerMax: 1,
  };

  const $ = (id) => document.getElementById(id);
  const canvas = $("atlas");
  const ctx = canvas.getContext("2d", { alpha: false });

  async function load() {
    const [emb, br, meta, led, tl] = await Promise.all([
      fetch("data/embedding.json").then((r) => r.json()),
      fetch("data/bridges.json").then((r) => r.json()),
      fetch("data/graph_meta.json").then((r) => r.json()),
      fetch("data/ledger.json").then((r) => r.json()),
      fetch("data/timeline.json")
        .then((r) => (r.ok ? r.json() : { slices: [] }))
        .catch(() => ({ slices: [] })),
    ]);
    state.embedding = emb;
    state.bridges = br;
    state.meta = meta;
    state.ledger = led;
    state.timeline = tl;
    buildKnn(emb.nodes, 3);
    seedStars(220);
    peerScale();
    applySlice(-1);
    renderLegend();
    renderBridges();
    paintDetail(null);
    setupTimeline();
    const demo = new URLSearchParams(location.search).get("demo");
    if (demo === "1" && state.bridges.bridges.length) {
      select(state.bridges.bridges[0].id);
    }
    resize();
    loop();
  }

  function seedStars(n) {
    const stars = [];
    for (let i = 0; i < n; i++) {
      stars.push({
        x: Math.random(),
        y: Math.random(),
        r: Math.random() * 1.1 + 0.2,
        a: 0.08 + Math.random() * 0.22,
      });
    }
    state.stars = stars;
  }

  function buildKnn(nodes, k) {
    const coords = nodes.map((n) => n.x || [0, 0]);
    const knn = nodes.map(() => []);
    const N = nodes.length;
    for (let i = 0; i < N; i++) {
      const di = [];
      const a = coords[i];
      for (let j = 0; j < N; j++) {
        if (i === j) continue;
        const b = coords[j];
        let s = 0;
        const d = Math.min(a.length, b.length, 3);
        for (let t = 0; t < d; t++) {
          const u = (a[t] || 0) - (b[t] || 0);
          s += u * u;
        }
        di.push([s, j]);
      }
      di.sort((p, q) => p[0] - q[0]);
      knn[i] = di.slice(0, k).map((p) => p[1]);
    }
    state.knn = knn;
  }

  function peerScale() {
    let m = 1;
    for (const a of state.ledger?.accounts || []) {
      m = Math.max(m, Math.abs(a.debit_cents || 0), Math.abs(a.credit_cents || 0), Math.abs(a.net_cents || 0));
    }
    state.peerMax = m;
  }

  function bridgeSet() {
    const s = new Set();
    (state.bridges?.bridges || []).forEach((b) => s.add(b.id));
    return s;
  }

  function account(id) {
    return (state.ledger?.accounts || []).find((a) => a.id === id) || null;
  }

  function nodeById(id) {
    return (state.embedding?.nodes || []).find((n) => n.id === id) || null;
  }

  function currentSlice() {
    const slices = state.timeline?.slices || [];
    if (!slices.length) return null;
    if (state.yearIdx < 0 || state.yearIdx >= slices.length) return slices[slices.length - 1];
    return slices[state.yearIdx];
  }

  function currentYear() {
    const sl = currentSlice();
    if (sl) return sl.year;
    const years = (state.embedding?.nodes || []).map((n) => n.year).filter((y) => y > 0);
    return years.length ? Math.max(...years) : 9999;
  }

  function applySlice(idx) {
    const slices = state.timeline?.slices || [];
    state.yearIdx = slices.length ? (idx < 0 ? slices.length - 1 : idx) : -1;
    const sl = currentSlice();
    const full = state.meta || {};
    $("stat-n").textContent = sl ? sl.n : full.n;
    $("stat-m").textContent = sl ? sl.m : full.undirected_edges;
    $("stat-l2").textContent = Number(sl ? sl.lambda2 : full.algebraic_connectivity).toFixed(4);
    $("stat-pair").textContent = `${state.bridges.pair_a} ↔ ${state.bridges.pair_b}`;
    $("stat-year").textContent = sl ? sl.year : "—";
    if ($("year-label")) $("year-label").textContent = sl ? sl.year : "—";
    if ($("year-bridge")) {
      $("year-bridge").textContent = sl && sl.top_bridge_id ? `bridge ${sl.top_bridge_id}` : "";
    }
  }

  function setupTimeline() {
    const slices = state.timeline?.slices || [];
    const box = $("timebox");
    const sl = $("year");
    if (!slices.length) {
      box.hidden = true;
      return;
    }
    box.hidden = false;
    sl.min = 0;
    sl.max = slices.length - 1;
    sl.value = slices.length - 1;
    sl.addEventListener("input", () => {
      applySlice(Number(sl.value));
      state.layoutCache = null;
    });
  }

  function renderLegend() {
    const box = $("legend");
    box.innerHTML = "";
    const seen = new Map();
    for (const n of state.embedding.nodes) {
      seen.set(n.field, (seen.get(n.field) || 0) + 1);
    }
    for (const [field, count] of [...seen.entries()].sort()) {
      const el = document.createElement("span");
      el.innerHTML = `<i style="background:${FIELDS[field] || "#ccc"}"></i>${field} ${count}`;
      box.appendChild(el);
    }
    const br = document.createElement("span");
    br.innerHTML = `<i style="background:#e6b35a;box-shadow:0 0 8px #e6b35a"></i>bridge`;
    box.appendChild(br);
  }

  function renderBridges() {
    const ul = $("bridges");
    ul.innerHTML = "";
    const method = state.bridges.method || "";
    if (method === "leave-one-out") {
      $("bridge-meta").textContent =
        "True leave-one-out Δλ₂ on the Rayleigh × participation shortlist. Largest Δλ₂ = the vertex whose deletion drops algebraic connectivity most.";
    }
    for (const b of state.bridges.bridges.slice(0, 16)) {
      const li = document.createElement("li");
      li.dataset.id = b.id;
      const node = nodeById(b.id);
      const dlt =
        b.delta_lambda2 == null || Number.isNaN(b.delta_lambda2)
          ? ""
          : ` · Δλ₂ ${Number(b.delta_lambda2).toFixed(4)}`;
      li.innerHTML = `<span class="rank">#${b.rank}</span>${b.id}
        <div class="meta">${(node && node.category) || ""} · score ${b.score.toFixed(4)}${dlt}</div>`;
      li.addEventListener("click", () => select(b.id));
      ul.appendChild(li);
    }
  }

  function paintSpark(id) {
    const acc = account(id);
    const spark = $("spark");
    if (!acc) {
      spark.hidden = true;
      return;
    }
    spark.hidden = false;
    const cap = state.peerMax || 1;
    $("bar-dr").style.width = `${Math.min(100, (100 * acc.debit_cents) / cap)}%`;
    $("bar-cr").style.width = `${Math.min(100, (100 * acc.credit_cents) / cap)}%`;
    $("bar-net").style.width = `${Math.min(100, (100 * Math.abs(acc.net_cents)) / cap)}%`;
    $("lab-dr").textContent = acc.debit_cents;
    $("lab-cr").textContent = acc.credit_cents;
    $("lab-net").textContent = acc.net_cents;
    const node = nodeById(id);
    const field = node?.field;
    const peers = (state.embedding?.nodes || [])
      .filter((n) => n.field === field)
      .map((n) => account(n.id))
      .filter(Boolean);
    if (!peers.length) {
      $("spark-peer").textContent = "";
      return;
    }
    const nets = peers.map((p) => p.net_cents);
    const mean = nets.reduce((s, v) => s + v, 0) / nets.length;
    const role = acc.net_cents > 0 ? "NET CREDITOR — idea supplier" : acc.net_cents < 0 ? "NET DEBTOR — idea importer" : "FLAT";
    $("spark-peer").textContent = `${role} · field ${field} mean net ${mean.toFixed(0)} · n=${peers.length}`;
  }

  function paintDetail(id) {
    const pre = $("detail");
    if (!id) {
      pre.textContent =
        "CLAIMLEDGER  TRIAL BALANCE\n" +
        "SELECT A PAPER IN THE ATLAS OR A BRIDGE AT RIGHT.\n" +
        `BOOK ${state.ledger?.balanced ? "BALANCED" : "OUT OF BALANCE"}  ` +
        `DR ${state.ledger?.total_debit_cents}  CR ${state.ledger?.total_credit_cents}\n` +
        `METHOD ${state.bridges?.method || "—"}`;
      $("spark").hidden = true;
      return;
    }
    const node = nodeById(id);
    const br = (state.bridges.bridges || []).find((b) => b.id === id);
    const acc = account(id);
    const dlt =
      br && br.delta_lambda2 != null && !Number.isNaN(br.delta_lambda2)
        ? `  Δλ2 ${Number(br.delta_lambda2).toFixed(5)}`
        : "";
    const lines = [
      `ACCOUNT  ${id}`,
      node ? `TITLE    ${node.title}` : "",
      node ? `FIELD    ${node.field} / ${node.category}   YEAR ${node.year}` : "",
      node ? `DEGREE   ${node.degree}   CLUSTER ${node.cluster}` : "",
      br
        ? `BRIDGE   rank ${br.rank}  score ${br.score.toFixed(5)}${dlt}\n         ${br.explanation}`
        : "BRIDGE   (not in top set)",
      acc
        ? `LEDGER   DR ${acc.debit_cents}  CR ${acc.credit_cents}  NET ${acc.net_cents}`
        : "LEDGER   no postings",
    ];
    pre.textContent = lines.filter(Boolean).join("\n");
    paintSpark(id);
    for (const li of document.querySelectorAll(".bridge-list li")) {
      li.classList.toggle("on", li.dataset.id === id);
    }
  }

  function select(id) {
    state.selected = id;
    paintDetail(id);
  }

  function resize() {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const r = canvas.getBoundingClientRect();
    canvas.width = Math.max(1, Math.floor(r.width * dpr));
    canvas.height = Math.max(1, Math.floor(r.height * dpr));
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    state.layoutCache = null;
  }

  function project(node) {
    const x = node.x || [0, 0, 0];
    let X = x[0] || 0;
    let Y = x[1] || 0;
    let Z = node.z != null ? node.z : x[2] || 0;
    if (state.dim === 3) {
      const cy = Math.cos(state.rot);
      const sy = Math.sin(state.rot);
      const cx = Math.cos(state.tilt);
      const sx = Math.sin(state.tilt);
      const x1 = X * cy + Z * sy;
      const z1 = -X * sy + Z * cy;
      const y1 = Y * cx - z1 * sx;
      const z2 = Y * sx + z1 * cx;
      const focal = 1.85;
      const p = focal / (focal + z2 + 0.55);
      return { X: x1 * p, Y: y1 * p, depth: z2 };
    }
    return { X, Y, depth: 0 };
  }

  function layout() {
    const nodes = state.embedding?.nodes || [];
    if (!nodes.length) return { pts: [], w: 1, h: 1 };
    const pts = nodes.map((n, i) => ({ n, i, ...project(n) }));
    let minX = Infinity,
      maxX = -Infinity,
      minY = Infinity,
      maxY = -Infinity;
    for (const p of pts) {
      minX = Math.min(minX, p.X);
      maxX = Math.max(maxX, p.X);
      minY = Math.min(minY, p.Y);
      maxY = Math.max(maxY, p.Y);
    }
    const r = canvas.getBoundingClientRect();
    const pad = 42;
    const sx = (r.width - pad * 2) / Math.max(1e-9, maxX - minX);
    const sy = (r.height - pad * 2) / Math.max(1e-9, maxY - minY);
    const s = Math.min(sx, sy);
    for (const p of pts) {
      p.px = pad + (p.X - minX) * s + (r.width - pad * 2 - (maxX - minX) * s) / 2;
      p.py = pad + (maxY - p.Y) * s + (r.height - pad * 2 - (maxY - minY) * s) / 2;
    }
    pts.sort((a, b) => a.depth - b.depth);
    return { pts, w: r.width, h: r.height };
  }

  function qmatch(n) {
    if (!state.query) return 0;
    const q = state.query;
    if (n.id.toLowerCase().includes(q)) return 2;
    if ((n.title || "").toLowerCase().includes(q)) return 1;
    return 0;
  }

  function draw() {
    const r = canvas.getBoundingClientRect();
    ctx.fillStyle = "#05060a";
    ctx.fillRect(0, 0, r.width, r.height);
    const g = ctx.createRadialGradient(r.width * 0.5, r.height * 0.42, 20, r.width * 0.5, r.height * 0.42, r.width * 0.62);
    g.addColorStop(0, "#141825");
    g.addColorStop(1, "#05060a");
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, r.width, r.height);

    for (const s of state.stars) {
      ctx.globalAlpha = s.a;
      ctx.fillStyle = "#d8d2c4";
      ctx.fillRect(s.x * r.width, s.y * r.height, s.r, s.r);
    }
    ctx.globalAlpha = 1;

    if (!state.embedding) return;
    const { pts } = layout();
    state.layoutCache = pts;
    const bridges = bridgeSet();
    const year = currentYear();
    const yearBridge = currentSlice()?.top_bridge_id;
    const searching = !!state.query;

    const byIndex = new Array(state.embedding.nodes.length);
    for (const p of pts) {
      byIndex[p.i] = p;
    }
    ctx.lineWidth = 0.7;
    for (let i = 0; i < byIndex.length; i++) {
      const a = byIndex[i];
      if (!a) continue;
      const aliveA = (a.n.year || 0) <= year;
      if (!aliveA) continue;
      if (state.onlyBridges && !bridges.has(a.n.id)) continue;
      for (const j of state.knn[i] || []) {
        const b = byIndex[j];
        if (!b) continue;
        if ((b.n.year || 0) > year) continue;
        if (state.onlyBridges && !bridges.has(b.n.id)) continue;
        ctx.globalAlpha = 0.14;
        ctx.beginPath();
        ctx.moveTo(a.px, a.py);
        ctx.lineTo(b.px, b.py);
        ctx.strokeStyle = FIELDS[a.n.field] || "#888";
        ctx.stroke();
      }
    }

    const centroids = new Map();
    for (const p of pts) {
      if ((p.n.year || 0) > year) continue;
      const c = centroids.get(p.n.field) || { x: 0, y: 0, n: 0 };
      c.x += p.px;
      c.y += p.py;
      c.n += 1;
      centroids.set(p.n.field, c);
    }
    ctx.globalAlpha = 0.4;
    ctx.fillStyle = "#8b8478";
    ctx.font = "12px IBM Plex Mono, monospace";
    for (const [field, c] of centroids) {
      ctx.fillText(field, c.x / c.n - 10, c.y / c.n);
    }

    for (const p of pts) {
      const isB = bridges.has(p.n.id);
      const alive = (p.n.year || 0) <= year;
      if (state.onlyBridges && !isB) continue;
      const hit = qmatch(p.n);
      if (searching && !hit && !isB && state.selected !== p.n.id) {
        ctx.globalAlpha = 0.08;
      } else {
        ctx.globalAlpha = alive ? 1 : 0.12;
      }
      const col = FIELDS[p.n.field] || "#c8c2b6";
      const sel = state.selected === p.n.id;
      const yearTop = yearBridge && p.n.id === yearBridge;
      const rad = (isB ? 4.4 : 2.0 + Math.min(3, (p.n.degree || 0) / 18)) * (alive ? 1 : 0.7);
      if ((isB || sel || yearTop || hit) && alive) {
        ctx.beginPath();
        ctx.arc(p.px, p.py, rad + (yearTop ? 9 : 6), 0, Math.PI * 2);
        ctx.fillStyle = sel || yearTop ? "rgba(230,179,90,0.28)" : hit ? "rgba(255,255,255,0.16)" : "rgba(230,179,90,0.12)";
        ctx.fill();
      }
      ctx.beginPath();
      ctx.arc(p.px, p.py, sel ? rad + 1.4 : rad, 0, Math.PI * 2);
      ctx.fillStyle = isB || yearTop ? "#e6b35a" : col;
      ctx.fill();
    }
    ctx.globalAlpha = 1;
  }

  function hit(mx, my) {
    const pts = state.layoutCache || layout().pts;
    let best = null;
    let bestD = 14;
    const year = currentYear();
    for (const p of pts) {
      if (state.onlyBridges && !bridgeSet().has(p.n.id)) continue;
      const d = Math.hypot(p.px - mx, p.py - my);
      if (d < bestD) {
        bestD = d;
        best = p;
      }
    }
    if (best && (best.n.year || 0) > year && !bridgeSet().has(best.n.id)) {
      // still allow picking a dimmed node
    }
    return best;
  }

  function showHover(p, ev) {
    const el = $("hover");
    if (!p) {
      el.hidden = true;
      return;
    }
    el.hidden = false;
    const sl = currentSlice();
    const mark = sl && sl.top_bridge_id === p.n.id ? " · year bridge" : "";
    el.innerHTML = `<b>${p.n.id}</b> ${p.n.year || ""}${mark}<br>${p.n.title || ""}`;
    const hud = canvas.getBoundingClientRect();
    el.style.left = `${Math.min(hud.width - 240, p.px + 12)}px`;
    el.style.top = `${Math.max(8, p.py - 28)}px`;
    void ev;
  }

  function loop() {
    if (state.dim === 3 && state.auto && !state.drag) {
      state.rot += 0.0032;
    }
    draw();
    requestAnimationFrame(loop);
  }

  canvas.addEventListener("click", (e) => {
    const r = canvas.getBoundingClientRect();
    const p = hit(e.clientX - r.left, e.clientY - r.top);
    if (p) select(p.n.id);
  });

  canvas.addEventListener("pointerdown", (e) => {
    state.drag = { x: e.clientX, y: e.clientY, rot: state.rot, tilt: state.tilt };
    state.auto = false;
    canvas.setPointerCapture(e.pointerId);
  });
  canvas.addEventListener("pointermove", (e) => {
    const r = canvas.getBoundingClientRect();
    if (state.drag && state.dim === 3) {
      state.rot = state.drag.rot + (e.clientX - state.drag.x) * 0.008;
      state.tilt = Math.max(-1.2, Math.min(1.2, state.drag.tilt + (e.clientY - state.drag.y) * 0.006));
    } else {
      const p = hit(e.clientX - r.left, e.clientY - r.top);
      state.hover = p ? p.n.id : null;
      showHover(p, e);
    }
  });
  canvas.addEventListener("pointerup", () => {
    state.drag = null;
  });
  canvas.addEventListener("pointerleave", () => {
    showHover(null);
  });

  $("btn-2d").addEventListener("click", () => {
    state.dim = 2;
    $("btn-2d").classList.add("active");
    $("btn-3d").classList.remove("active");
  });
  $("btn-3d").addEventListener("click", () => {
    state.dim = 3;
    state.auto = true;
    $("btn-3d").classList.add("active");
    $("btn-2d").classList.remove("active");
  });
  $("btn-bridges").addEventListener("click", (e) => {
    state.onlyBridges = !state.onlyBridges;
    e.currentTarget.classList.toggle("active", state.onlyBridges);
  });
  $("search").addEventListener("input", (e) => {
    state.query = e.target.value.trim().toLowerCase();
    if (state.query) {
      const hitN = (state.embedding?.nodes || []).find((n) => qmatch(n));
      if (hitN) select(hitN.id);
    }
  });

  function cycleBridge(dir) {
    const list = state.bridges?.bridges || [];
    if (!list.length) return;
    let i = list.findIndex((b) => b.id === state.selected);
    if (i < 0) i = dir > 0 ? -1 : 0;
    i = (i + dir + list.length) % list.length;
    select(list[i].id);
  }

  document.addEventListener("keydown", (e) => {
    const typing = document.activeElement === $("search");
    if (e.key === "/" && !typing) {
      e.preventDefault();
      $("search").focus();
      $("search").select();
      return;
    }
    if (e.key === "Escape") {
      $("sheet").hidden = true;
      if (typing) {
        $("search").blur();
        $("search").value = "";
        state.query = "";
      }
      return;
    }
    if (e.key === "?" && !typing) {
      e.preventDefault();
      $("sheet").hidden = !$("sheet").hidden;
      return;
    }
    if (typing) return;
    if (e.key === "[") cycleBridge(-1);
    if (e.key === "]") cycleBridge(1);
    if (e.key === "2") $("btn-2d").click();
    if (e.key === "3") $("btn-3d").click();
    if (e.key === "b") $("btn-bridges").click();
  });

  $("sheet").addEventListener("click", (e) => {
    if (e.target.id === "sheet") $("sheet").hidden = true;
  });

  window.addEventListener("resize", resize);
  load().catch((err) => {
    $("detail").textContent =
      "FAILED TO LOAD docs/data/*.json\n" +
      err +
      "\nServe via Pages or: python3 -m http.server --directory docs 8000";
  });
})();
