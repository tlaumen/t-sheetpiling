"use strict";

/* ---------- state ---------------------------------------------------- */

const defaultLayer = (top, bottom) => ({
  top, bottom, gamma: 18, phi: 30, c: 0, k1: 15000, k2: 6000, k3: 2000,
});

const state = {
  pile: { EI: 100000, top: 0.5, tip: -10.0 },
  left: [defaultLayer(-3.0, -15.0)],
  right: [defaultLayer(0.0, -15.0)],
  supports: [{ level: -1.0 }],
  n_nodes: 61,
};

let result = null;
let solveInFlight = false;
let solvePending = false;

/* ---------- tab switching --------------------------------------------- */

document.querySelectorAll(".tab-btn").forEach((btn) => {
  btn.addEventListener("click", () => activateTab(btn.dataset.tab));
});
document.getElementById("empty-to-input").addEventListener("click", (e) => {
  e.preventDefault();
  activateTab("input");
});
document.getElementById("goto-interact").addEventListener("click", () => activateTab("interact"));

function activateTab(name) {
  document.querySelectorAll(".tab-btn").forEach((b) => b.classList.toggle("active", b.dataset.tab === name));
  document.getElementById("tab-input").classList.toggle("active", name === "input");
  document.getElementById("tab-interact").classList.toggle("active", name === "interact");
  if (name === "interact") {
    const empty = state.supports.length === 0;
    document.getElementById("interact-empty").hidden = !empty;
    document.getElementById("interact-body").style.display = empty ? "none" : "grid";
    if (!empty) {
      renderCrossSection();
      solve();
    }
  }
}

/* ---------- input form rendering --------------------------------------- */

function numberField(label, value, onChange, step) {
  step = step || "0.1";
  const wrap = document.createElement("label");
  wrap.textContent = label;
  const input = document.createElement("input");
  input.type = "number";
  input.step = step;
  input.value = value;
  input.addEventListener("input", () => onChange(parseFloat(input.value)));
  wrap.appendChild(input);
  return wrap;
}

function renderPileRow() {
  const row = document.getElementById("pile-row");
  row.innerHTML = "";
  row.appendChild(numberField("EI [kNm²]", state.pile.EI, (v) => (state.pile.EI = v), "1000"));
  row.appendChild(numberField("top level", state.pile.top, (v) => (state.pile.top = v)));
  row.appendChild(numberField("tip level", state.pile.tip, (v) => (state.pile.tip = v)));
}

function renderLayerList(containerId, side) {
  const container = document.getElementById(containerId);
  container.innerHTML = "";
  state[side].forEach((layer, idx) => {
    const row = document.createElement("div");
    row.className = "layer-row";
    const fields = [
      ["top", "top"], ["bottom", "bottom"], ["gamma", "gamma"],
      ["phi [deg]", "phi"], ["c [kPa]", "c"],
      ["k1", "k1"], ["k2", "k2"], ["k3", "k3"],
    ];
    fields.forEach(([label, key]) => {
      row.appendChild(numberField(label, layer[key], (v) => (layer[key] = v), key.charAt(0) === "k" ? "100" : "0.1"));
    });
    const removeBtn = document.createElement("button");
    removeBtn.className = "remove-btn";
    removeBtn.textContent = "remove";
    removeBtn.addEventListener("click", () => {
      state[side].splice(idx, 1);
      renderLayerList(containerId, side);
    });
    row.appendChild(removeBtn);
    container.appendChild(row);
  });
}

function renderSupportList() {
  const container = document.getElementById("support-list");
  container.innerHTML = "";
  state.supports.forEach((sup, idx) => {
    const row = document.createElement("div");
    row.className = "support-row";
    row.appendChild(numberField("level", sup.level, (v) => (sup.level = v)));
    const typeLabel = document.createElement("label");
    typeLabel.textContent = "type";
    const typeSpan = document.createElement("div");
    typeSpan.style.fontFamily = "var(--mono)";
    typeSpan.style.fontSize = "13px";
    typeSpan.style.padding = "6px 0";
    typeSpan.textContent = "fixed-horizontal";
    typeLabel.appendChild(typeSpan);
    row.appendChild(typeLabel);
    const removeBtn = document.createElement("button");
    removeBtn.className = "remove-btn";
    removeBtn.textContent = "remove";
    removeBtn.addEventListener("click", () => {
      state.supports.splice(idx, 1);
      renderSupportList();
    });
    row.appendChild(removeBtn);
    container.appendChild(row);
  });
}

document.getElementById("add-left").addEventListener("click", () => {
  const last = state.left.length ? state.left[state.left.length - 1].bottom : 0;
  state.left.push(defaultLayer(last, last - 5));
  renderLayerList("left-layers", "left");
});
document.getElementById("add-right").addEventListener("click", () => {
  const last = state.right.length ? state.right[state.right.length - 1].bottom : 0;
  state.right.push(defaultLayer(last, last - 5));
  renderLayerList("right-layers", "right");
});
document.getElementById("add-support").addEventListener("click", () => {
  state.supports.push({ level: (state.pile.top + state.pile.tip) / 2 });
  renderSupportList();
});

renderPileRow();
renderLayerList("left-layers", "left");
renderLayerList("right-layers", "right");
renderSupportList();

/* ---------- geometry mapping (shared by cross-section and charts) ----- */

const PLOT_TOP = 20;
const PLOT_BOTTOM = 460;

function levelToY(level) {
  const t = (state.pile.top - level) / (state.pile.top - state.pile.tip);
  return PLOT_TOP + t * (PLOT_BOTTOM - PLOT_TOP);
}
function yToLevel(y) {
  const t = (y - PLOT_TOP) / (PLOT_BOTTOM - PLOT_TOP);
  return state.pile.top - t * (state.pile.top - state.pile.tip);
}

/* ---------- cross-section rendering + drag interaction ----------------- */

const CX = 180, LEFT_X0 = 20, LEFT_X1 = 170, RIGHT_X0 = 190, RIGHT_X1 = 340;

function svgEl(tag, attrs) {
  const el = document.createElementNS("http://www.w3.org/2000/svg", tag);
  for (const k in attrs) el.setAttribute(k, attrs[k]);
  return el;
}

function renderCrossSection() {
  const svg = document.getElementById("cross-section");
  svg.innerHTML = "";

  state.left.forEach((layer) => {
    const y0 = levelToY(layer.top), y1 = levelToY(layer.bottom);
    svg.appendChild(svgEl("rect", {
      x: LEFT_X0, y: y0, width: LEFT_X1 - LEFT_X0, height: Math.max(0, y1 - y0),
      fill: "var(--left-soil)", stroke: "var(--line-strong)", "stroke-width": 0.5,
    }));
  });
  state.right.forEach((layer) => {
    const y0 = levelToY(layer.top), y1 = levelToY(layer.bottom);
    svg.appendChild(svgEl("rect", {
      x: RIGHT_X0, y: y0, width: RIGHT_X1 - RIGHT_X0, height: Math.max(0, y1 - y0),
      fill: "var(--right-soil)", stroke: "var(--line-strong)", "stroke-width": 0.5,
    }));
  });

  svg.appendChild(svgEl("line", {
    x1: CX, y1: levelToY(state.pile.top), x2: CX, y2: levelToY(state.pile.tip),
    stroke: "var(--ink)", "stroke-width": 3,
  }));

  state.supports.forEach((sup, idx) => {
    const y = levelToY(sup.level);
    svg.appendChild(svgEl("line", {
      x1: LEFT_X0, y1: y, x2: RIGHT_X1, y2: y,
      stroke: "var(--ink-soft)", "stroke-width": 1, "stroke-dasharray": "3 3",
    }));
    const handle = svgEl("circle", {
      cx: CX, cy: y, r: 8, fill: "var(--accent)", stroke: "var(--panel)", "stroke-width": 2,
      style: "cursor: ns-resize",
    });
    handle.addEventListener("pointerdown", (e) => startDrag(e, idx));
    svg.appendChild(handle);

    const label = svgEl("text", {
      x: RIGHT_X1 + 8, y: y + 4, "font-size": 11, "font-family": "var(--mono)", fill: "var(--ink-soft)",
    });
    label.textContent = sup.level.toFixed(2);
    svg.appendChild(label);
  });
}

function startDrag(e, idx) {
  e.preventDefault();
  const svg = document.getElementById("cross-section");
  const move = (ev) => {
    const rect = svg.getBoundingClientRect();
    const scale = 480 / rect.height; /* viewBox height / rendered height */
    const yInViewBox = (ev.clientY - rect.top) * scale;
    let level = yToLevel(yInViewBox);
    level = Math.min(state.pile.top, Math.max(state.pile.tip, level));
    state.supports[idx].level = Math.round(level * 100) / 100;
    renderCrossSection();
    requestSolve();
  };
  const up = () => {
    document.removeEventListener("pointermove", move);
    document.removeEventListener("pointerup", up);
  };
  document.addEventListener("pointermove", move);
  document.addEventListener("pointerup", up);
}

/* ---------- solving (coalesced so drag events never queue up) --------- */

function requestSolve() {
  if (solveInFlight) { solvePending = true; return; }
  solve();
}

async function solve() {
  solveInFlight = true;
  const payload = {
    pile: state.pile, left: state.left, right: state.right,
    supports: state.supports, n_nodes: state.n_nodes,
  };
  try {
    const res = await fetch("/solve", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    result = await res.json();
    renderCharts();
    updateStatus();
  } catch (err) {
    document.getElementById("status").textContent = "request failed: " + err;
    document.getElementById("status").className = "status bad";
  } finally {
    solveInFlight = false;
    if (solvePending) { solvePending = false; requestSolve(); }
  }
}

function updateStatus() {
  const el = document.getElementById("status");
  if (result.error) {
    el.textContent = "error: " + result.error;
    el.className = "status bad";
    return;
  }
  let Mmax = 0;
  result.M.forEach((m) => { if (Math.abs(m) > Math.abs(Mmax)) Mmax = m; });
  let Vmax = 0;
  result.V.forEach((v) => { if (Math.abs(v) > Math.abs(Vmax)) Vmax = v; });
  el.textContent = (result.converged ? "converged" : "NOT converged") + " \u00b7 " +
    result.iterations + " iterations \u00b7 Mmax = " + Mmax.toFixed(2) + " kNm \u00b7 Vmax = " + Vmax.toFixed(2) + " kN";
  el.className = "status " + (result.converged ? "ok" : "bad");
}

/* ---------- force diagrams ------------------------------------------- */

function renderChart(svgId, values, unitLabel) {
  const svg = document.getElementById(svgId);
  svg.innerHTML = "";
  if (!result || result.error) return;

  const W = 320, CX2 = W / 2;
  let maxAbs = 1e-6;
  values.forEach((v) => { if (Math.abs(v) > maxAbs) maxAbs = Math.abs(v); });
  const xScale = (W / 2 - 30) / maxAbs;

  svg.appendChild(svgEl("line", {
    x1: CX2, y1: PLOT_TOP, x2: CX2, y2: PLOT_BOTTOM, stroke: "var(--line-strong)", "stroke-width": 1,
  }));

  const points = result.z.map((z, i) => (CX2 + values[i] * xScale) + "," + levelToY(z)).join(" ");
  const fillPoints = CX2 + "," + levelToY(result.z[0]) + " " + points + " " +
    CX2 + "," + levelToY(result.z[result.z.length - 1]);
  svg.appendChild(svgEl("polyline", { points: fillPoints, fill: "var(--accent)", opacity: 0.08, stroke: "none" }));
  svg.appendChild(svgEl("polyline", { points: points, fill: "none", stroke: "var(--accent)", "stroke-width": 1.6 }));

  let extreme = 0, extremeZ = 0;
  values.forEach((v, i) => { if (Math.abs(v) > Math.abs(extreme)) { extreme = v; extremeZ = result.z[i]; } });
  const ey = levelToY(extremeZ);
  svg.appendChild(svgEl("circle", { cx: CX2 + extreme * xScale, cy: ey, r: 3, fill: "var(--accent)" }));

  const label = svgEl("text", {
    x: 10, y: PLOT_BOTTOM + 14, "font-size": 11, "font-family": "var(--mono)", fill: "var(--ink-soft)",
  });
  label.textContent = "max " + extreme.toFixed(1) + " " + unitLabel + " @ z=" + extremeZ.toFixed(2);
  svg.appendChild(label);
}

function renderCharts() {
  if (!result || result.error) return;
  renderChart("chart-V", result.V, "kN");
  renderChart("chart-M", result.M, "kNm");
}
