import fs from "node:fs";
import path from "node:path";
import { createRequire } from "node:module";

const require = createRequire(import.meta.url);
const sharp = require("sharp");

const repoRoot = path.resolve(import.meta.dirname, "..");
const outputRoot = path.join(repoRoot, "assets", "aether-visual");
const brandRoot = path.join(outputRoot, "brand");
const iconRoot = path.join(outputRoot, "icons");
const canonicalMark = path.join(brandRoot, "master", "aether-mascot-canonical-v1.png");
let cleanMarkCache;

const palette = {
  canvas: "#0A0710",
  surface: "#14101D",
  surfaceRaised: "#1D1729",
  surfaceOverlay: "#261E35",
  border: "#3A2D50",
  text: "#F6F2FC",
  textMuted: "#A89DB8",
  primary: "#A94DFF",
  primaryStrong: "#7C24E8",
  primarySoft: "#D9B5FF",
  focus: "#64E7F0",
  success: "#4DDAA4",
  warning: "#F3B84C",
  danger: "#FF617D",
  axisX: "#F05B78",
  axisY: "#65D47A",
  axisZ: "#5B9CFF",
};

const S = {
  plus: '<path d="M12 5v14M5 12h14"/>',
  minus: '<path d="M5 12h14"/>',
  check: '<path d="m5 12 4 4 10-10"/>',
  x: '<path d="m6 6 12 12M18 6 6 18"/>',
  chevronRight: '<path d="m9 5 7 7-7 7"/>',
  chevronLeft: '<path d="m15 5-7 7 7 7"/>',
  chevronDown: '<path d="m5 9 7 7 7-7"/>',
  eye: '<path d="M2.5 12s3.4-5 9.5-5 9.5 5 9.5 5-3.4 5-9.5 5-9.5-5-9.5-5Z"/><circle cx="12" cy="12" r="2.4"/>',
  cube: '<path d="m12 2.8 8 4.5v9.4l-8 4.5-8-4.5V7.3l8-4.5Z"/><path d="m4.4 7.5 7.6 4.3 7.6-4.3M12 11.8v9"/>',
  folder: '<path d="M3 7.2V19h18V6.8H11l-2-2H3v2.4Z"/>',
  document: '<path d="M6 2.8h8l4 4V21H6V2.8Z"/><path d="M14 3v4h4"/>',
  nodes: '<circle cx="5" cy="6" r="2"/><circle cx="19" cy="6" r="2"/><circle cx="12" cy="18" r="2"/><path d="M7 6h10M6.5 7.5l4.3 8.7M17.5 7.5l-4.3 8.7"/>',
};

const icons = {
  transport: {
    play: '<path d="m8 5 11 7-11 7V5Z"/>',
    pause: '<path d="M8 5v14M16 5v14"/>',
    stop: '<rect x="6" y="6" width="12" height="12" rx="1.5"/>',
    step: '<path d="m6 5 9 7-9 7V5ZM18 5v14"/>',
    restart: '<path d="M4.5 8.5A8 8 0 1 1 4 15M4.5 8.5V3.8M4.5 8.5h4.7"/>',
    record: '<circle cx="12" cy="12" r="6.5"/>',
  },
  actions: {
    undo: '<path d="M9 7 4 12l5 5M5 12h8a6 6 0 0 1 6 6"/>',
    redo: '<path d="m15 7 5 5-5 5M19 12h-8a6 6 0 0 0-6 6"/>',
    save: '<path d="M4 3h13l3 3v15H4V3Z"/><path d="M8 3v6h8V3M8 21v-7h8v7"/>',
    add: `<circle cx="12" cy="12" r="9"/>${S.plus}`,
    remove: `<circle cx="12" cy="12" r="9"/>${S.minus}`,
    close: S.x,
    confirm: S.check,
    duplicate: '<rect x="8" y="8" width="11" height="11" rx="2"/><path d="M16 8V5a2 2 0 0 0-2-2H5a2 2 0 0 0-2 2v9a2 2 0 0 0 2 2h3"/>',
    copy: '<rect x="9" y="8" width="10" height="12" rx="2"/><path d="M15 8V5a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h3"/>',
    paste: '<path d="M9 5h6M9 3h6v4H9V3Z"/><path d="M7 5H4v16h16V5h-3"/>',
    delete: '<path d="M4 7h16M9 7V4h6v3M7 7l1 14h8l1-14M10 11v6M14 11v6"/>',
    search: '<circle cx="10.5" cy="10.5" r="6.5"/><path d="m15.5 15.5 5 5"/>',
    filter: '<path d="M3 5h18l-7 8v6l-4 2v-8L3 5Z"/>',
    settings: '<circle cx="12" cy="12" r="3"/><path d="M12 2.8v3M12 18.2v3M2.8 12h3M18.2 12h3M5.5 5.5l2.1 2.1M16.4 16.4l2.1 2.1M18.5 5.5l-2.1 2.1M7.6 16.4l-2.1 2.1"/>',
    refresh: '<path d="M20 7v5h-5M4 17v-5h5"/><path d="M18.5 9A7.5 7.5 0 0 0 5.7 6.2L4 8M5.5 15A7.5 7.5 0 0 0 18.3 17.8L20 16"/>',
    more: '<circle cx="5" cy="12" r="1"/><circle cx="12" cy="12" r="1"/><circle cx="19" cy="12" r="1"/>',
    download: '<path d="M12 3v12M7 10l5 5 5-5M4 20h16"/>',
    upload: '<path d="M12 16V4M7 9l5-5 5 5M4 20h16"/>',
  },
  modes: {
    select: '<path d="M5 3 18 13l-6 .8-3.5 6.2L5 3Z"/>',
    move: '<path d="M12 2v20M2 12h20M12 2l-3 3M12 2l3 3M22 12l-3-3M22 12l-3 3M12 22l-3-3M12 22l3-3M2 12l3-3M2 12l3 3"/>',
    rotate: '<path d="M20 8V3l-2.2 2.2A8 8 0 1 0 20 15"/>',
    scale: '<path d="M4 9V4h5M15 4h5v5M20 15v5h-5M9 20H4v-5M4 4l6 6M20 4l-6 6M20 20l-6-6M4 20l6-6"/>',
    edit: '<path d="m4 16-1 5 5-1L19 9l-4-4L4 16ZM13 7l4 4"/>',
    sculpt: '<path d="M5 20c5-1 3-6 7-8 3-1 2-5 7-8M4 16c3 0 4 2 4 5M11 9c1 3 3 4 6 4"/>',
    paint: '<path d="m14 4 6 6-9 9H5v-6l9-9ZM12 6l6 6"/><path d="M5 19c-1 0-2 .8-2 2h5"/>',
    uv: '<rect x="3" y="3" width="18" height="18" rx="2"/><path d="M3 12h18M12 3c-3 5-3 13 0 18M12 3c3 5 3 13 0 18"/>',
    rig: '<circle cx="12" cy="4" r="2"/><path d="M12 6v6M7 9l5 3 5-3M12 12l-4 8M12 12l4 8"/>',
    animate: '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l4 2"/><path d="M4 5 2 8M20 5l2 3"/>',
    terrain: '<path d="M2 19 8 9l4 6 3-4 7 8H2Z"/><path d="m6 15 2-3 2 3"/>',
    flow: S.nodes,
    cinema: '<rect x="3" y="6" width="14" height="12" rx="2"/><path d="m17 10 4-2v8l-4-2M7 10h5M7 14h3"/>',
    joint: '<circle cx="7" cy="12" r="3"/><circle cx="17" cy="12" r="3"/><path d="M10 12h4"/>',
  },
  viewport: {
    camera: '<path d="M4 7h4l2-2h4l2 2h4v12H4V7Z"/><circle cx="12" cy="13" r="4"/>',
    perspective: '<path d="M5 5h14l-3 14H8L5 5Z"/><path d="M7 9h10M9 5l1 14M15 5l-1 14"/>',
    orthographic: '<rect x="5" y="5" width="14" height="14"/><path d="M5 10h14M10 5v14"/>',
    front: `${S.cube}<path d="M8 12h8M12 8v8"/>`,
    side: `${S.cube}<path d="m12 12 5-3v6l-5-3Z"/>`,
    top: `${S.cube}<path d="m12 6 4 2-4 2-4-2 4-2Z"/>`,
    grid: '<path d="M3 3h18v18H3V3ZM3 9h18M3 15h18M9 3v18M15 3v18"/>',
    snap: '<path d="M5 4v9a7 7 0 0 0 14 0V4M5 8h5M14 8h5"/>',
    local: '<circle cx="12" cy="12" r="8"/><path d="M12 12V6M12 12l5 2M12 12l-4 4"/>',
    global: '<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3c3 3 3 15 0 18M12 3c-3 3-3 15 0 18"/>',
    pivot: '<circle cx="12" cy="12" r="3"/><path d="M12 2v7M12 15v7M2 12h7M15 12h7"/>',
    center: '<circle cx="12" cy="12" r="7"/><circle cx="12" cy="12" r="2"/>',
    focus: '<path d="M8 3H3v5M16 3h5v5M21 16v5h-5M8 21H3v-5"/><circle cx="12" cy="12" r="3"/>',
    frameAll: '<rect x="4" y="4" width="16" height="16" rx="2"/><path d="M8 8h3v3H8V8ZM13 13h3v3h-3v-3Z"/>',
    wireframe: `${S.cube}<path d="m4 7.3 8 4.5 8-4.5M4 16.7l8-4.9 8 4.9"/>`,
    lighting: '<circle cx="12" cy="12" r="4"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M5 5l2 2M17 17l2 2M19 5l-2 2M7 17l-2 2"/>',
    visible: S.eye,
    hidden: `${S.eye}<path d="M4 4l16 16"/>`,
    lock: '<rect x="5" y="10" width="14" height="11" rx="2"/><path d="M8 10V7a4 4 0 0 1 8 0v3"/>',
    unlock: '<rect x="5" y="10" width="14" height="11" rx="2"/><path d="M16 10V7a4 4 0 0 0-7-2"/>',
  },
  scene: {
    scene: `${S.cube}<circle cx="4" cy="7" r="1"/><circle cx="20" cy="7" r="1"/>`,
    object: S.cube,
    child: '<circle cx="6" cy="5" r="2"/><circle cx="18" cy="18" r="2"/><path d="M6 7v5h12v4"/>',
    expand: S.chevronRight,
    collapse: S.chevronDown,
    hierarchy: '<circle cx="12" cy="4" r="2"/><circle cx="5" cy="19" r="2"/><circle cx="12" cy="19" r="2"/><circle cx="19" cy="19" r="2"/><path d="M12 6v6M5 17v-5h14v5M12 12v5"/>',
    prefab: `${S.cube}<path d="m8 4 8 16M16 4 8 20"/>`,
    component: '<rect x="5" y="5" width="14" height="14" rx="3"/><path d="M9 2v3M15 2v3M9 19v3M15 19v3M2 9h3M2 15h3M19 9h3M19 15h3"/>',
    tag: '<path d="M3 4h8l10 10-7 7L3 10V4Z"/><circle cx="8" cy="9" r="1.5"/>',
    layer: '<path d="m12 3 9 5-9 5-9-5 9-5Z"/><path d="m3 12 9 5 9-5M3 16l9 5 9-5"/>',
    favorite: '<path d="m12 3 2.8 5.7 6.2.9-4.5 4.4 1.1 6.2-5.6-3-5.6 3 1.1-6.2L3 9.6l6.2-.9L12 3Z"/>',
    pin: '<path d="m8 3 8 8-2 2 3 4-1 1-4-3-2 2-8-8 2-2 4 1 2-2-2-3Z"/><path d="m9 15-5 5"/>',
    rename: '<path d="M4 5h9M8.5 5v14M4 19h9M15 16l5-5-3-3-5 5v3h3Z"/>',
  },
  assets: {
    folder: S.folder,
    image: '<rect x="3" y="4" width="18" height="16" rx="2"/><circle cx="8" cy="9" r="2"/><path d="m4 18 5-5 3 3 3-4 5 6"/>',
    mesh: `${S.cube}<path d="M4 7.3h16M8 9.6v9M16 9.6v9"/>`,
    material: '<circle cx="12" cy="12" r="9"/><path d="M5.5 17.5 18.5 6.5M7 7h.1M16.5 16.5h.1"/>',
    shader: '<path d="M4 17 10 5h4l6 12M7 13h10"/><path d="M6 20h12"/>',
    audio: '<path d="M9 18V6l10-2v12"/><circle cx="6" cy="18" r="3"/><circle cx="16" cy="16" r="3"/>',
    animation: '<rect x="3" y="5" width="18" height="14" rx="2"/><path d="M7 5v14M17 5v14M3 10h4M17 10h4M3 15h4M17 15h4"/><path d="m10 9 5 3-5 3V9Z"/>',
    script: '<path d="m8 7-5 5 5 5M16 7l5 5-5 5M14 4l-4 16"/>',
    font: '<path d="M4 19 10 4h4l6 15M7 14h10"/>',
    sceneAsset: `${S.document}<path d="m9 13 3-2 3 2v4l-3 2-3-2v-4Z"/>`,
    prefabAsset: `${S.document}<path d="m9 13 3-2 3 2-3 2-3-2ZM9 13v4l3 2 3-2v-4"/>`,
    import: `<path d="M4 20h16"/>${S.download}`,
    gridView: '<rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/>',
    listView: '<path d="M8 6h13M8 12h13M8 18h13"/><circle cx="4" cy="6" r="1"/><circle cx="4" cy="12" r="1"/><circle cx="4" cy="18" r="1"/>',
    preview: `${S.eye}<path d="M12 7V4M12 20v-3"/>`,
  },
  physics: {
    rigidbody: `${S.cube}<path d="M7 4.5 17 19.5M17 4.5 7 19.5"/>`,
    colliderBox: '<rect x="4" y="4" width="16" height="16" rx="1"/><rect x="7" y="7" width="10" height="10" stroke-dasharray="2 2"/>',
    colliderSphere: '<circle cx="12" cy="12" r="8"/><ellipse cx="12" cy="12" rx="4" ry="8" stroke-dasharray="2 2"/><path d="M4 12h16"/>',
    colliderCapsule: '<rect x="7" y="3" width="10" height="18" rx="5"/><path d="M7 8h10M7 16h10" stroke-dasharray="2 2"/>',
    colliderMesh: `${S.cube}<path d="M4 7.3 12 21M20 7.3 12 21M4 16.7l16-9.4" stroke-dasharray="2 2"/>`,
    trigger: '<circle cx="12" cy="12" r="8" stroke-dasharray="3 2"/><path d="M12 7v5l3 3"/>',
    jointPoint: '<circle cx="12" cy="12" r="3"/><path d="M12 2v7M12 15v7M2 12h7M15 12h7"/>',
    jointHinge: '<circle cx="7" cy="12" r="3"/><circle cx="17" cy="12" r="3"/><path d="M10 12h4M12 5v14"/>',
    jointSlider: '<rect x="5" y="9" width="14" height="6" rx="3"/><path d="M2 12h20M2 12l3-3M2 12l3 3M22 12l-3-3M22 12l-3 3"/>',
    jointDistance: '<circle cx="5" cy="12" r="2"/><circle cx="19" cy="12" r="2"/><path d="M7 12h10M9 9l-2 3 2 3M15 9l2 3-2 3"/>',
    raycast: '<path d="M3 18 19 4M14 4h5v5"/><circle cx="8" cy="14" r="2"/><path d="M18 13v7M14.5 16.5h7"/>',
    gravity: '<path d="M12 3v15M7 13l5 5 5-5"/><path d="M4 21h16"/>',
  },
  flow: {
    event: '<path d="M13 2 5 13h6l-1 9 8-12h-6l1-8Z"/>',
    action: '<rect x="4" y="5" width="16" height="14" rx="3"/><path d="m9 9 6 3-6 3V9Z"/>',
    data: '<path d="m12 3 8 4.5v9L12 21l-8-4.5v-9L12 3Z"/><path d="M8 12h8"/>',
    variable: '<path d="M4 7h5l3 10 3-10h5"/><path d="M4 4h16M4 20h16"/>',
    node: S.nodes,
    connect: '<circle cx="5" cy="12" r="2"/><circle cx="19" cy="12" r="2"/><path d="M7 12c4-7 6 7 10 0"/>',
    disconnect: '<circle cx="5" cy="12" r="2"/><circle cx="19" cy="12" r="2"/><path d="M7 12c2-4 3-2 4 0M13 12c1 2 2 4 4 0M10 8l4 8"/>',
    macro: '<rect x="3" y="5" width="18" height="14" rx="3"/><path d="M7 9h4v6H7V9ZM15 9h2v6h-2"/>',
    breakpoint: '<circle cx="12" cy="12" r="7"/><circle cx="12" cy="12" r="3" fill="currentColor" stroke="none"/>',
    watch: S.eye,
    validate: `<path d="M12 3 4 6v6c0 5 3.4 8 8 9 4.6-1 8-4 8-9V6l-8-3Z"/>${S.check}`,
    error: '<circle cx="12" cy="12" r="9"/><path d="M12 7v6M12 17h.01"/>',
  },
  console: {
    trace: '<path d="M4 5h16v14H4V5Z"/><path d="m7 9 3 3-3 3M12 15h5"/>',
    debug: '<path d="M9 7h6M8 11h8v5a4 4 0 0 1-8 0v-5ZM5 12h3M16 12h3M6 7l2 2M18 7l-2 2M12 3v4"/>',
    info: '<circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7h.01"/>',
    warning: '<path d="m12 3 10 18H2L12 3Z"/><path d="M12 9v5M12 17h.01"/>',
    error: '<circle cx="12" cy="12" r="9"/>${S.x}',
    fatal: '<path d="M8 3h8l5 5v8l-5 5H8l-5-5V8l5-5Z"/><path d="M12 7v6M12 17h.01"/>',
    clear: '<path d="m4 15 8-10 8 6-8 10H6l-2-6Z"/><path d="m9 17 6-8M14 21h7"/>',
    collapseAll: '<path d="m5 8 7-5 7 5M5 16l7 5 7-5"/><path d="M5 12h14"/>',
    expandAll: '<path d="m5 5 7 5 7-5M5 19l7-5 7 5"/>',
  },
  panels: {
    assets: S.folder,
    console: '<rect x="3" y="5" width="18" height="14" rx="2"/><path d="m7 9 3 3-3 3M12 15h5"/>',
    flow: S.nodes,
    timeline: '<path d="M3 6h18M3 12h18M3 18h18"/><circle cx="8" cy="6" r="2"/><circle cx="15" cy="12" r="2"/><circle cx="11" cy="18" r="2"/>',
    inspector: '<rect x="4" y="3" width="16" height="18" rx="2"/><path d="M8 8h8M8 12h5M8 16h8"/>',
    profiler: '<path d="M3 20V10M8 20V5M13 20v-8M18 20V3M22 20H2"/>',
    build: '<path d="M14 4a5 5 0 0 0-6 6L3 15l6 6 5-5a5 5 0 0 0 6-6l-3 3-4-4 3-3-2-2Z"/>',
  },
  system: {
    thermal: '<path d="M9 4a3 3 0 0 1 6 0v9a5 5 0 1 1-6 0V4Z"/><path d="M12 7v9"/>',
    battery: '<rect x="3" y="7" width="17" height="10" rx="2"/><path d="M20 10h2v4h-2M7 10v4M11 10v4M15 10v4"/>',
    memory: '<rect x="5" y="5" width="14" height="14" rx="2"/><path d="M9 2v3M15 2v3M9 19v3M15 19v3M2 9h3M2 15h3M19 9h3M19 15h3M9 9h6v6H9V9Z"/>',
    fps: '<path d="M4 18V6h7M4 12h5M13 18V6h4a3 3 0 0 1 0 6h-4M17 12l4 6"/>',
    power: '<path d="M12 2v10M7 5.5a8 8 0 1 0 10 0"/>',
    android: '<rect x="5" y="8" width="14" height="11" rx="2"/><path d="M8 8a4 4 0 0 1 8 0M8 4 6 2M16 4l2-2M8 12h.01M16 12h.01M7 19v3M17 19v3M3 10v7M21 10v7"/>',
    package: '<path d="m12 3 9 5-9 5-9-5 9-5Z"/><path d="m3 8v8l9 5 9-5V8M12 13v8"/>',
    cloud: '<path d="M7 18H5a4 4 0 0 1-.5-8A7 7 0 0 1 18 8a5 5 0 0 1 1 10h-3"/><path d="M12 21V11M8 15l4-4 4 4"/>',
    plugin: '<path d="M8 3h8v5h5v8h-5v5H8v-5H3V8h5V3Z"/><path d="M10 8h4v8h-4V8Z"/>',
    help: '<circle cx="12" cy="12" r="9"/><path d="M9.5 9a2.7 2.7 0 1 1 3.4 2.6c-.9.3-.9 1-.9 2M12 17h.01"/>',
    menu: '<path d="M4 7h16M4 12h16M4 17h16"/>',
    safeArea: '<rect x="3" y="3" width="18" height="18" rx="3"/><rect x="6" y="6" width="12" height="12" rx="1" stroke-dasharray="2 2"/>',
  },
};

function ensureDir(dir) {
  fs.mkdirSync(dir, { recursive: true });
}

function svgDocument(body, color = palette.text) {
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="${color}" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round">${body}</svg>`;
}

function safeId(category, name) {
  return `${category}-${name.replace(/[A-Z]/g, m => `-${m.toLowerCase()}`)}`;
}

async function generateIcons() {
  const manifest = [];
  const sizes = [24, 32, 48, 64];
  const states = { neutral: palette.text, active: palette.primarySoft };

  // O pacote público é exclusivamente PNG. Diretórios vetoriais de execuções
  // anteriores são removidos para não deixar formatos ambíguos na entrega.
  fs.rmSync(path.join(iconRoot, "svg"), { recursive: true, force: true });
  fs.rmSync(path.join(iconRoot, "sprite"), { recursive: true, force: true });

  for (const [category, entries] of Object.entries(icons)) {
    for (const [rawName, body] of Object.entries(entries)) {
      const id = safeId(category, rawName);
      manifest.push({ id, category, name: rawName, format: "png", sizes, states: Object.keys(states), transparent: true });

      for (const [state, color] of Object.entries(states)) {
        for (const size of sizes) {
          const pngDir = path.join(iconRoot, "png", state, String(size));
          ensureDir(pngDir);
          await sharp(Buffer.from(svgDocument(body, color)))
            .resize(size, size)
            .png({ compressionLevel: 9 })
            .toFile(path.join(pngDir, `${id}.png`));
        }
      }
    }
  }

  fs.writeFileSync(path.join(iconRoot, "manifest.json"), `${JSON.stringify({ version: 1, icons: manifest }, null, 2)}\n`, "utf8");
  return manifest;
}

async function generateBrandAssets() {
  if (!fs.existsSync(canonicalMark)) throw new Error(`Marca canônica ausente: ${canonicalMark}`);
  const cleanMark = await cleanCanonicalMark();
  const markSizes = [64, 128, 256, 512, 1024];
  for (const size of markSizes) {
    const dir = path.join(brandRoot, "mark", String(size));
    ensureDir(dir);
    await sharp(cleanMark).resize({
      width: size,
      height: size,
      fit: "contain",
      background: { r: 0, g: 0, b: 0, alpha: 0 },
    }).png({ compressionLevel: 9 }).toFile(path.join(dir, "aether-mark.png"));
  }

  const foregroundDir = path.join(brandRoot, "android");
  ensureDir(foregroundDir);
  const foreground = await sharp(cleanMark).resize({
    width: 690,
    height: 690,
    fit: "contain",
    background: { r: 0, g: 0, b: 0, alpha: 0 },
  }).png().toBuffer();
  await sharp({ create: { width: 1024, height: 1024, channels: 4, background: { r: 0, g: 0, b: 0, alpha: 0 } } })
    .composite([{ input: foreground, gravity: "center" }])
    .png({ compressionLevel: 9 })
    .toFile(path.join(foregroundDir, "adaptive-foreground-1024.png"));

  const launcherBg = Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024"><defs><radialGradient id="g" cx="50%" cy="42%"><stop offset="0" stop-color="#352052"/><stop offset=".55" stop-color="#171022"/><stop offset="1" stop-color="#09060F"/></radialGradient></defs><rect width="1024" height="1024" rx="224" fill="url(#g)"/><circle cx="512" cy="470" r="310" fill="none" stroke="#A94DFF" stroke-opacity=".18" stroke-width="2"/></svg>`);
  const launcher = await sharp(launcherBg).composite([{ input: foreground, gravity: "center" }]).png().toBuffer();
  await sharp(launcher).toFile(path.join(foregroundDir, "launcher-master-1024.png"));
  for (const [density, size] of Object.entries({ mdpi: 48, hdpi: 72, xhdpi: 96, xxhdpi: 144, xxxhdpi: 192 })) {
    await sharp(launcher).resize(size, size).png({ compressionLevel: 9 }).toFile(path.join(foregroundDir, `launcher-${density}-${size}.png`));
  }
  await sharp(launcher).resize(512, 512).png({ compressionLevel: 9 }).toFile(path.join(foregroundDir, "play-store-512.png"));

  await createSplash(1920, 1080, "landscape");
  await createSplash(1080, 1920, "portrait");
  await createFeatureGraphic();
}

async function cleanCanonicalMark() {
  if (cleanMarkCache) return cleanMarkCache;
  const { data, info } = await sharp(canonicalMark).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
  // Imagens generativas podem deixar alpha residual (1–10) nas bordas. Ao
  // redimensionar, esse resíduo vira linhas escuras. Zerar só alpha quase
  // transparente é uma limpeza técnica, sem alterar a arte visível.
  for (let i = 0; i < data.length; i += 4) {
    if (data[i + 3] < 16) {
      data[i] = 0;
      data[i + 1] = 0;
      data[i + 2] = 0;
      data[i + 3] = 0;
    }
  }
  cleanMarkCache = await sharp(data, { raw: info })
    .trim({ background: { r: 0, g: 0, b: 0, alpha: 0 }, threshold: 10 })
    .png()
    .toBuffer();
  return cleanMarkCache;
}

async function createSplash(width, height, orientation) {
  const markWidth = orientation === "landscape" ? 420 : 520;
  const mark = await sharp(await cleanCanonicalMark()).resize({
    width: markWidth,
    height: markWidth,
    fit: "contain",
    background: { r: 0, g: 0, b: 0, alpha: 0 },
  }).png().toBuffer();
  const fontSize = orientation === "landscape" ? 76 : 88;
  const baseline = orientation === "landscape" ? 850 : 1420;
  const bg = Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}"><defs><radialGradient id="r" cx="50%" cy="43%"><stop offset="0" stop-color="#28163E"/><stop offset=".45" stop-color="#120C1B"/><stop offset="1" stop-color="#08050D"/></radialGradient><linearGradient id="line" x1="0" x2="1"><stop stop-color="#A94DFF" stop-opacity="0"/><stop offset=".5" stop-color="#A94DFF" stop-opacity=".55"/><stop offset="1" stop-color="#64E7F0" stop-opacity="0"/></linearGradient></defs><rect width="100%" height="100%" fill="url(#r)"/><circle cx="${width / 2}" cy="${height * .43}" r="${Math.min(width, height) * .32}" fill="none" stroke="#A94DFF" stroke-opacity=".12"/><path d="M${width * .12} ${height * .72}H${width * .88}" stroke="url(#line)"/><text x="50%" y="${baseline}" fill="#F6F2FC" font-family="Segoe UI,Arial,sans-serif" font-size="${fontSize}" font-weight="700" font-style="italic" letter-spacing="12" text-anchor="middle">AETHER</text></svg>`);
  const top = Math.round(height * .43 - markWidth / 2);
  const left = Math.round(width / 2 - markWidth / 2);
  const outDir = path.join(brandRoot, "splash");
  ensureDir(outDir);
  await sharp(bg).composite([{ input: mark, left, top }]).png({ compressionLevel: 9 }).toFile(path.join(outDir, `aether-splash-${orientation}-${width}x${height}.png`));
}

async function createFeatureGraphic() {
  const width = 1024, height = 500;
  const mark = await sharp(await cleanCanonicalMark()).resize({
    width: 350,
    height: 350,
    fit: "contain",
    background: { r: 0, g: 0, b: 0, alpha: 0 },
  }).png().toBuffer();
  const bg = Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}"><defs><linearGradient id="g" x1="0" y1="0" x2="1" y2="1"><stop stop-color="#08050D"/><stop offset=".55" stop-color="#171022"/><stop offset="1" stop-color="#2B1745"/></linearGradient></defs><rect width="1024" height="500" fill="url(#g)"/><path d="M0 410C270 320 520 560 1024 330" fill="none" stroke="#A94DFF" stroke-opacity=".24" stroke-width="2"/><path d="M0 445C320 340 590 590 1024 360" fill="none" stroke="#64E7F0" stroke-opacity=".12"/><text x="515" y="220" fill="#F6F2FC" font-family="Segoe UI,Arial,sans-serif" font-size="78" font-weight="750" font-style="italic" letter-spacing="8">AETHER</text><text x="520" y="270" fill="#A89DB8" font-family="Segoe UI,Arial,sans-serif" font-size="24" letter-spacing="3">MOBILE CREATION ENGINE</text></svg>`);
  const outDir = path.join(brandRoot, "store");
  ensureDir(outDir);
  await sharp(bg).composite([{ input: mark, left: 95, top: 70 }]).png({ compressionLevel: 9 }).toFile(path.join(outDir, "feature-graphic-1024x500.png"));
}

function generateTokens() {
  const tokensDir = path.join(outputRoot, "tokens");
  ensureDir(tokensDir);
  const tokens = {
    version: 1,
    color: palette,
    typography: {
      ui: { family: "Inter, Roboto, system-ui, sans-serif", weights: [400, 500, 600, 700] },
      mono: { family: "JetBrains Mono, Roboto Mono, monospace", weights: [400, 500, 600] },
    },
    spacingDp: [0, 4, 8, 12, 16, 20, 24, 32, 40, 48],
    radiusDp: { xs: 4, sm: 6, md: 10, lg: 16, pill: 999 },
    touchTargetDp: { minimum: 44, android: 48, modeDock: 64 },
    iconDp: { compact: 18, standard: 24, prominent: 32, touch: 48 },
    stroke: { icon: 1.75, divider: 1, focus: 2 },
    motionMs: { instant: 80, fast: 150, panel: 240, deliberate: 320 },
    elevation: { panel: 1, overlay: 2, radial: 3 },
  };
  fs.writeFileSync(path.join(tokensDir, "aether.tokens.json"), `${JSON.stringify(tokens, null, 2)}\n`, "utf8");
}

function generateCatalog(manifest) {
  const cards = manifest.map(i => `<article><img src="png/neutral/32/${i.id}.png" alt=""><code>${i.id}</code></article>`).join("\n");
  const catalog = `<!doctype html><html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Aether Icon Catalog</title><style>:root{color-scheme:dark;font-family:Inter,system-ui,sans-serif;background:${palette.canvas};color:${palette.text}}body{margin:0;padding:32px}header{max-width:900px;margin:0 auto 28px}h1{font-size:28px;margin:0 0 8px}p{color:${palette.textMuted};margin:0}.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(180px,1fr));gap:10px;max-width:1400px;margin:auto}article{display:flex;align-items:center;gap:12px;min-height:56px;padding:10px 12px;background:${palette.surface};border:1px solid ${palette.border};border-radius:10px}img{width:28px;height:28px;object-fit:contain;flex:none}article:hover{border-color:${palette.primary};background:${palette.surfaceRaised}}code{font-size:12px;color:${palette.textMuted};overflow-wrap:anywhere}</style></head><body><header><h1>Aether Icon System</h1><p>${manifest.length} ícones semânticos PNG · fundo transparente · 24/32/48/64 px</p></header><main class="grid">${cards}</main></body></html>`;
  fs.writeFileSync(path.join(iconRoot, "catalog.html"), catalog, "utf8");
}

async function generatePreviewSheet(manifest) {
  const columns = 8;
  const cellWidth = 224;
  const cellHeight = 66;
  const headerHeight = 112;
  const rows = Math.ceil(manifest.length / columns);
  const width = columns * cellWidth + 64;
  const height = headerHeight + rows * cellHeight + 48;
  const bodies = new Map();
  for (const [category, entries] of Object.entries(icons)) {
    for (const [name, body] of Object.entries(entries)) bodies.set(safeId(category, name), body);
  }
  const cards = manifest.map((entry, index) => {
    const x = 32 + (index % columns) * cellWidth;
    const y = headerHeight + Math.floor(index / columns) * cellHeight;
    return `<g transform="translate(${x} ${y})"><rect width="212" height="54" rx="9" fill="#14101D" stroke="#3A2D50"/><g transform="translate(15 15)" fill="none" stroke="#F6F2FC" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round">${bodies.get(entry.id)}</g><text x="55" y="32" fill="#A89DB8" font-family="Segoe UI,Arial,sans-serif" font-size="11">${entry.id}</text></g>`;
  }).join("");
  const svg = Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}"><rect width="100%" height="100%" fill="#0A0710"/><text x="32" y="46" fill="#F6F2FC" font-family="Segoe UI,Arial,sans-serif" font-size="28" font-weight="700">Aether Icon System</text><text x="32" y="76" fill="#A89DB8" font-family="Segoe UI,Arial,sans-serif" font-size="15">${manifest.length} ícones semânticos · 24×24 · stroke 1.75 · tintáveis</text>${cards}</svg>`);
  const dir = path.join(iconRoot, "preview");
  ensureDir(dir);
  await sharp(svg).png({ compressionLevel: 9 }).toFile(path.join(dir, "aether-icon-catalog.png"));
}

ensureDir(outputRoot);
generateTokens();
const manifest = await generateIcons();
generateCatalog(manifest);
await generatePreviewSheet(manifest);
await generateBrandAssets();
console.log(`Pacote visual gerado: ${outputRoot}`);
console.log(`Ícones: ${manifest.length}; PNGs: ${manifest.length * 2 * 4}`);
