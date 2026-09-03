/* ClaimLedger atlas — spectral embedding + COBOL drill-down. */
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
    dim: 2,
    onlyBridges: false,
    selected: null,
    rot: 0.35,
    tilt: 0.7,
    drag: null,
    knn: [],
  };

  const $ = (id) => document.getElementById(id);
  const canvas = $("atlas");
  const ctx = canvas.getContext("2d");

  async function load() {
    const [emb, br, meta, led] = await Promise.all([
      fetch("data/embedding.json").then((r) => r.json()),
      fetch("data/bridges.json").then((r) => r.json()),
      fetch("data/graph_meta.json").then((r) => r.json()),
      fetch("data/ledger.json").then((r) => r.json()),
    ]);
    state.embedding = emb;
    state.bridges = br;
    state.meta = meta;
    state.ledger = led;
    buildKnn(emb.nodes, 3);
    $("stat-n").textContent = meta.n;
    $("stat-m").textContent = meta.undirected_edges;
    $("stat-l2").textContent = Number(meta.algebraic_connectivity).toFixed(4);
    $("stat-pair").textContent = `${br.pair_a} ↔ ${br.pair_b}`;
    renderLegend();
    renderBridges();
    paintDetail(null);
    const demo = new URLSearchParams(location.search).get("demo");
    if (demo === "1" && state.bridges.bridges.length) {
      select(state.bridges.bridges[0].id);
    }
    resize();
    loop();
  }

  function buildKnn(nodes, k) {
    const coords = nodes.map((n) => n.x || [0, 0]);
    const knn = nodes.map(() => []);
    for (let i = 0; i < nodes.length; i++) {
      const di = [];
      const a = coords[i];
      for (let j = 0; j < nodes.length; j++) {
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

  function bridgeSet() {
    const s = new Set();
    (state.bridges?.bridges || []).forEach((b) => s.add(b.id));
    return s;
  }

  function account(id) {
    return (state.ledger?.accounts || []).find((a) => a.id === id) || null;
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
      for (const b of state.bridges.bridges.slice(0, 16)) {
      const li = document.createElement("li");
      li.dataset.id = b.id;
      const node = state.embedding.nodes.find((n) => n.id === b.id);
      li.innerHTML = `<span class="rank">#${b.rank}</span>${b.id}
        <div class="meta">${(node && node.category) || ""} · score ${b.score.toFixed(4)} · H=${b.participation_entropy.toFixed(2)}</div>`;
      li.addEventListener("click", () => select(b.id));
      ul.appendChild(li);
    }
  }

  function paintDetail(id) {
    const pre = $("detail");
    if (!id) {
      pre.textContent =
        "CLAIMLEDGER  TRIAL BALANCE\n" +
        "SELECT A PAPER IN THE ATLAS OR A BRIDGE AT RIGHT.\n" +
        `BOOK ${state.ledger?.balanced ? "BALANCED" : "OUT OF BALANCE"}  ` +
        `DR ${state.ledger?.total_debit_cents}  CR ${state.ledger?.total_credit_cents}`;
      return;
    }
    const node = state.embedding.nodes.find((n) => n.id === id);
    const br = (state.bridges.bridges || []).find((b) => b.id === id);
    const acc = account(id);
    const lines = [
      `ACCOUNT  ${id}`,
      node ? `TITLE    ${node.title}` : "",
      node ? `FIELD    ${node.field} / ${node.category}   YEAR ${node.year}` : "",
      node ? `DEGREE   ${node.degree}   CLUSTER ${node.cluster}` : "",
      br
        ? `BRIDGE   rank ${br.rank}  score ${br.score.toFixed(5)}\n         ${br.explanation}`
        : "BRIDGE   (not in top set)",
      acc
        ? `LEDGER   DR ${acc.debit_cents}  CR ${acc.credit_cents}  NET ${acc.net_cents}\n         ${
            acc.net_cents > 0 ? "NET CREDITOR — idea supplier" : acc.net_cents < 0 ? "NET DEBTOR — idea importer" : "FLAT"
          }`
        : "LEDGER   no postings",
    ];
    pre.textContent = lines.filter(Boolean).join("\n");
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
  }

  function project(node) {
    const x = node.x || [0, 0, 0];
    let X = x[0] || 0;
    let Y = x[1] || 0;
    let Z = x[2] || 0;
    if (state.dim === 3) {
      const c = Math.cos(state.rot);
      const s = Math.sin(state.rot);
      const x2 = X * c - Z * s;
      const z2 = X * s + Z * c;
      Y = Y * Math.cos(state.tilt) - z2 * Math.sin(state.tilt);
      X = x2;
    }
    return { X, Y };
  }

  function layout() {
    const nodes = state.embedding?.nodes || [];
    if (!nodes.length) return { pts: [], w: 1, h: 1 };
    const pts = nodes.map((n) => ({ n, ...project(n) }));
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    for (const p of pts) {
      minX = Math.min(minX, p.X);
      maxX = Math.max(maxX, p.X);
      minY = Math.min(minY, p.Y);
      maxY = Math.max(maxY, p.Y);
    }
    const r = canvas.getBoundingClientRect();
    const pad = 36;
    const sx = (r.width - pad * 2) / Math.max(1e-9, maxX - minX);
    const sy = (r.height - pad * 2) / Math.max(1e-9, maxY - minY);
    const s = Math.min(sx, sy);
    for (const p of pts) {
      p.px = pad + (p.X - minX) * s + (r.width - pad * 2 - (maxX - minX) * s) / 2;
      p.py = pad + (maxY - p.Y) * s + (r.height - pad * 2 - (maxY - minY) * s) / 2;
    }
    return { pts, w: r.width, h: r.height };
  }

  function draw() {
    const r = canvas.getBoundingClientRect();
    ctx.clearRect(0, 0, r.width, r.height);
    if (!state.embedding) return;
    const { pts } = layout();
    const bridges = bridgeSet();

    // faint field-center labels
    ctx.font = "11px IBM Plex Mono, monospace";
    ctx.globalAlpha = 0.35;

    ctx.globalAlpha = 0.18;
    ctx.lineWidth = 0.7;
    for (let i = 0; i < pts.length; i++) {
      const a = pts[i];
      if (state.onlyBridges && !bridges.has(a.n.id)) continue;
      for (const j of state.knn[i] || []) {
        const b = pts[j];
        if (!b) continue;
        if (state.onlyBridges && !bridges.has(b.n.id)) continue;
        ctx.beginPath();
        ctx.moveTo(a.px, a.py);
        ctx.lineTo(b.px, b.py);
        ctx.strokeStyle = FIELDS[a.n.field] || "#888";
        ctx.stroke();
      }
    }

    const centroids = new Map();
    for (const p of pts) {
      const c = centroids.get(p.n.field) || { x: 0, y: 0, n: 0 };
      c.x += p.px;
      c.y += p.py;
      c.n += 1;
      centroids.set(p.n.field, c);
    }
    ctx.globalAlpha = 0.45;
    ctx.fillStyle = "#8b8478";
    ctx.font = "12px IBM Plex Mono, monospace";
    for (const [field, c] of centroids) {
      ctx.fillText(field, c.x / c.n - 10, c.y / c.n);
    }

    ctx.globalAlpha = 1;
    for (const p of pts) {
      const isB = bridges.has(p.n.id);
      if (state.onlyBridges && !isB) continue;
      const col = FIELDS[p.n.field] || "#c8c2b6";
      const sel = state.selected === p.n.id;
      const rad = isB ? 4.2 : 2.1 + Math.min(3, (p.n.degree || 0) / 18);
      if (isB || sel) {
        ctx.beginPath();
        ctx.arc(p.px, p.py, rad + 6, 0, Math.PI * 2);
        ctx.fillStyle = sel ? "rgba(230,179,90,0.25)" : "rgba(230,179,90,0.12)";
        ctx.fill();
      }
      ctx.beginPath();
      ctx.arc(p.px, p.py, sel ? rad + 1.4 : rad, 0, Math.PI * 2);
      ctx.fillStyle = isB ? "#e6b35a" : col;
      ctx.fill();
    }
  }

  function hit(mx, my) {
    const { pts } = layout();
    let best = null;
    let bestD = 12;
    for (const p of pts) {
      const d = Math.hypot(p.px - mx, p.py - my);
      if (d < bestD) {
        bestD = d;
        best = p.n.id;
      }
    }
    return best;
  }

  function loop() {
    draw();
    requestAnimationFrame(loop);
  }

  canvas.addEventListener("click", (e) => {
    const r = canvas.getBoundingClientRect();
    const id = hit(e.clientX - r.left, e.clientY - r.top);
    if (id) select(id);
  });

  canvas.addEventListener("pointerdown", (e) => {
    state.drag = { x: e.clientX, y: e.clientY, rot: state.rot, tilt: state.tilt };
    canvas.setPointerCapture(e.pointerId);
  });
  canvas.addEventListener("pointermove", (e) => {
    if (!state.drag || state.dim !== 3) return;
    state.rot = state.drag.rot + (e.clientX - state.drag.x) * 0.008;
    state.tilt = state.drag.tilt + (e.clientY - state.drag.y) * 0.006;
  });
  canvas.addEventListener("pointerup", () => {
    state.drag = null;
  });

  $("btn-2d").addEventListener("click", () => {
    state.dim = 2;
    $("btn-2d").classList.add("active");
    $("btn-3d").classList.remove("active");
  });
  $("btn-3d").addEventListener("click", () => {
    state.dim = 3;
    $("btn-3d").classList.add("active");
    $("btn-2d").classList.remove("active");
  });
  $("btn-bridges").addEventListener("click", (e) => {
    state.onlyBridges = !state.onlyBridges;
    e.currentTarget.classList.toggle("active", state.onlyBridges);
  });

  window.addEventListener("resize", resize);
  load().catch((err) => {
    $("detail").textContent =
      "FAILED TO LOAD docs/data/*.json\n" +
      err +
      "\nServe via Pages or: python3 -m http.server --directory docs 8000";
  });
})();
