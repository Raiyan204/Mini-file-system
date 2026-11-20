#ifndef GUI_ASSETS_H
#define GUI_ASSETS_H

static const char *kGuiHTML = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Unmad Mini File System OS</title>
<style>
  :root {
    --bg-dark: #0f172a;
    --bg-card: #1e293b;
    --bg-input: #0b1120;
    --border: #334155;
    --text-main: #f8fafc;
    --text-muted: #94a3b8;
    --accent: #38bdf8;
    --accent-hover: #0ea5e9;
    --danger: #ef4444;
    --success: #22c55e;
    --warning: #f59e0b;
    --purple: #a855f7;
    --font-mono: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background-color: var(--bg-dark);
    color: var(--text-main);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    height: 100vh;
    display: flex;
    flex-direction: column;
    overflow: hidden;
    user-select: none;
  }

  /* Header */
  header {
    background-color: var(--bg-card);
    border-bottom: 1px solid var(--border);
    padding: 10px 18px;
    display: flex;
    justify-content: space-between;
    align-items: center;
    flex-shrink: 0;
  }
  .brand { display: flex; align-items: center; gap: 10px; }
  .logo {
    background: linear-gradient(135deg, #38bdf8, #818cf8);
    color: #0f172a;
    font-weight: 900;
    font-size: 14px;
    padding: 5px 10px;
    border-radius: 6px;
    letter-spacing: 1px;
  }
  .title-group h1 { font-size: 16px; font-weight: 700; }
  .title-group p { font-size: 11px; color: var(--text-muted); font-family: var(--font-mono); }
  .status-badges { display: flex; gap: 8px; }
  .badge {
    background-color: var(--bg-input);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 4px 10px;
    font-size: 11px;
    font-family: var(--font-mono);
    display: flex;
    align-items: center;
    gap: 6px;
  }
  .badge-dot { width: 8px; height: 8px; border-radius: 50%; background: var(--success); }

  /* Main Layout */
  .main-container {
    display: flex;
    flex: 1;
    overflow: hidden;
  }

  /* Left Column: File Explorer */
  .left-pane {
    width: 320px;
    background-color: #131d31;
    border-right: 1px solid var(--border);
    display: flex;
    flex-direction: column;
    flex-shrink: 0;
  }
  .pane-header {
    padding: 10px 14px;
    font-size: 12px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    color: var(--text-muted);
    border-bottom: 1px solid var(--border);
    display: flex;
    justify-content: space-between;
    align-items: center;
  }
  .cwd-bar {
    padding: 8px 12px;
    background-color: var(--bg-input);
    border-bottom: 1px solid var(--border);
    display: flex;
    align-items: center;
    gap: 6px;
    font-family: var(--font-mono);
    font-size: 12px;
  }
  .cwd-bar button {
    background: #1e293b;
    border: 1px solid var(--border);
    color: var(--text-main);
    padding: 3px 8px;
    border-radius: 4px;
    cursor: pointer;
    font-size: 11px;
  }
  .cwd-bar button:hover { background: #334155; }
  .cwd-path { color: var(--accent); font-weight: 600; flex: 1; overflow: hidden; text-overflow: ellipsis; }

  .file-list {
    flex: 1;
    overflow-y: auto;
    padding: 8px;
    display: flex;
    flex-direction: column;
    gap: 4px;
  }
  .file-item {
    display: flex;
    align-items: center;
    padding: 7px 10px;
    border-radius: 6px;
    font-size: 13px;
    cursor: pointer;
    transition: background 0.15s;
    border: 1px solid transparent;
  }
  .file-item:hover { background-color: var(--bg-card); }
  .file-item.selected { background-color: #1e3a5f; border-color: var(--accent); }
  .file-icon { margin-right: 8px; font-size: 15px; }
  .file-name { flex: 1; font-weight: 500; font-family: var(--font-mono); font-size: 12px; }
  .file-meta { font-size: 11px; color: var(--text-muted); font-family: var(--font-mono); }

  .explorer-actions {
    padding: 10px;
    border-top: 1px solid var(--border);
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 6px;
  }
  button.action-btn {
    background-color: var(--bg-card);
    border: 1px solid var(--border);
    color: var(--text-main);
    padding: 6px 10px;
    border-radius: 6px;
    font-size: 11px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.15s;
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 5px;
  }
  button.action-btn:hover { background-color: #334155; border-color: var(--accent); }
  button.action-btn.btn-danger:hover { background-color: #7f1d1d; border-color: var(--danger); }

  /* Center Column: Inspector & Block Map & Hex */
  .center-pane {
    flex: 1;
    display: flex;
    flex-direction: column;
    overflow: hidden;
  }

  .tabs-bar {
    display: flex;
    background-color: var(--bg-card);
    border-bottom: 1px solid var(--border);
    padding: 0 10px;
  }
  .tab-btn {
    background: transparent;
    border: none;
    border-bottom: 2px solid transparent;
    color: var(--text-muted);
    padding: 10px 16px;
    font-size: 12px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.15s;
  }
  .tab-btn:hover { color: var(--text-main); }
  .tab-btn.active { color: var(--accent); border-bottom-color: var(--accent); }

  .tab-content {
    flex: 1;
    overflow: hidden;
    display: none;
    flex-direction: column;
    padding: 14px;
    gap: 12px;
  }
  .tab-content.active { display: flex; }

  /* Block Map View */
  .map-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    font-size: 12px;
  }
  .legend { display: flex; gap: 12px; font-size: 11px; font-family: var(--font-mono); }
  .legend-item { display: flex; align-items: center; gap: 5px; }
  .legend-box { width: 12px; height: 12px; border-radius: 2px; }

  .block-grid {
    flex: 1;
    display: grid;
    grid-template-columns: repeat(64, 1fr);
    gap: 2px;
    overflow-y: auto;
    background-color: var(--bg-input);
    padding: 8px;
    border-radius: 6px;
    border: 1px solid var(--border);
  }
  .block-cell {
    aspect-ratio: 1;
    border-radius: 1px;
    background-color: #1e293b;
    cursor: pointer;
    transition: transform 0.1s;
  }
  .block-cell:hover { transform: scale(1.8); z-index: 10; outline: 1px solid #fff; }
  .block-cell.blk-sb { background-color: var(--purple); }
  .block-cell.blk-bm { background-color: var(--warning); }
  .block-cell.blk-in { background-color: var(--accent); }
  .block-cell.blk-used { background-color: var(--success); }
  .block-cell.blk-free { background-color: #1e293b; }

  .block-info-card {
    background-color: var(--bg-card);
    border: 1px solid var(--border);
    padding: 8px 14px;
    border-radius: 6px;
    font-family: var(--font-mono);
    font-size: 12px;
    display: flex;
    justify-content: space-between;
  }

  /* Hex View */
  .hex-controls {
    display: flex;
    align-items: center;
    gap: 10px;
    font-family: var(--font-mono);
    font-size: 12px;
  }
  .hex-controls select, .hex-controls input {
    background-color: var(--bg-input);
    border: 1px solid var(--border);
    color: var(--text-main);
    padding: 4px 10px;
    border-radius: 4px;
    font-family: var(--font-mono);
  }
  .hex-container {
    flex: 1;
    background-color: var(--bg-input);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 10px;
    overflow: auto;
    font-family: var(--font-mono);
    font-size: 12px;
    line-height: 1.5;
    color: #38bdf8;
    white-space: pre;
  }

  /* Inode & Metadata Inspector */
  .metadata-card {
    background-color: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 14px;
    font-family: var(--font-mono);
    font-size: 12px;
    line-height: 1.6;
    overflow-y: auto;
    white-space: pre-wrap;
  }

  /* Bottom Console */
  .console-pane {
    height: 220px;
    background-color: #070d17;
    border-top: 1px solid var(--border);
    display: flex;
    flex-direction: column;
    flex-shrink: 0;
  }
  .console-header {
    background-color: #0c1424;
    padding: 6px 12px;
    font-size: 11px;
    font-family: var(--font-mono);
    color: var(--text-muted);
    border-bottom: 1px solid #1a273e;
    display: flex;
    justify-content: space-between;
    align-items: center;
  }
  .console-output {
    flex: 1;
    overflow-y: auto;
    padding: 8px 12px;
    font-family: var(--font-mono);
    font-size: 12px;
    line-height: 1.4;
    white-space: pre-wrap;
    color: #e2e8f0;
  }
  .console-input-row {
    display: flex;
    align-items: center;
    background-color: #0c1424;
    border-top: 1px solid #1a273e;
    padding: 6px 10px;
  }
  .console-prompt {
    font-family: var(--font-mono);
    font-size: 12px;
    color: var(--accent);
    font-weight: 700;
    margin-right: 8px;
    white-space: nowrap;
  }
  .console-input {
    flex: 1;
    background: transparent;
    border: none;
    outline: none;
    color: #fff;
    font-family: var(--font-mono);
    font-size: 12px;
  }

  /* Modal Dialog */
  .modal-overlay {
    position: fixed;
    top: 0; left: 0; right: 0; bottom: 0;
    background: rgba(0,0,0,0.7);
    display: none;
    align-items: center;
    justify-content: center;
    z-index: 1000;
  }
  .modal-card {
    background: var(--bg-card);
    border: 1px solid var(--border);
    border-radius: 8px;
    width: 440px;
    padding: 18px;
    display: flex;
    flex-direction: column;
    gap: 12px;
  }
  .modal-card h3 { font-size: 14px; font-weight: 700; }
  .modal-input {
    background: var(--bg-input);
    border: 1px solid var(--border);
    color: #fff;
    padding: 8px 12px;
    border-radius: 6px;
    font-family: var(--font-mono);
    font-size: 12px;
    outline: none;
  }
  .modal-textarea {
    background: var(--bg-input);
    border: 1px solid var(--border);
    color: #fff;
    padding: 8px 12px;
    border-radius: 6px;
    font-family: var(--font-mono);
    font-size: 12px;
    outline: none;
    height: 100px;
    resize: vertical;
  }
  .modal-actions { display: flex; justify-content: flex-end; gap: 8px; }
</style>
</head>
<body>

<header>
  <div class="brand">
    <div class="logo">UNMAD FS</div>
    <div class="title-group">
      <h1>Mini File System Simulator</h1>
      <p>1 MB Virtual Disk (2,048 Blocks &times; 512 B)</p>
    </div>
  </div>
  <div class="status-badges">
    <div class="badge"><div class="badge-dot"></div><span id="badge-magic">Magic: UNMD</span></div>
    <div class="badge"><span id="badge-blocks">Free Blocks: 2028 / 2048</span></div>
    <div class="badge"><span id="badge-inodes">Free Inodes: 126 / 128</span></div>
  </div>
</header>

<div class="main-container">
  <!-- Left Column: File Explorer -->
  <div class="left-pane">
    <div class="pane-header">
      <span>File Explorer</span>
      <button class="action-btn" onclick="runCmd('ls')" style="padding:2px 6px;">↻ Refresh</button>
    </div>
    <div class="cwd-bar">
      <button onclick="runCmd('cd /')" title="Go to Root">🏠 /</button>
      <button onclick="runCmd('cd ..')" title="Go to Parent">⬆ ..</button>
      <div class="cwd-path" id="ui-cwd">/</div>
    </div>
    <div class="file-list" id="file-list-el">
      <!-- File items populated dynamically -->
    </div>
    <div class="explorer-actions">
      <button class="action-btn" onclick="openCreateModal()">📄 + File</button>
      <button class="action-btn" onclick="openMkdirModal()">📁 + Folder</button>
      <button class="action-btn" onclick="openWriteModal()">✏️ Write Text</button>
      <button class="action-btn btn-danger" onclick="deleteSelected()">🗑️ Delete</button>
    </div>
  </div>

  <!-- Center Column: Inspector, Block Map, Hex View -->
  <div class="center-pane">
    <div class="tabs-bar">
      <button class="tab-btn active" onclick="switchTab('map')">Visual Block Map (2,048)</button>
      <button class="tab-btn" onclick="switchTab('hex')">Raw Hex Dump (xxd)</button>
      <button class="tab-btn" onclick="switchTab('info')">Metadata & Inode Inspector</button>
    </div>

    <!-- Tab 1: Visual Block Map -->
    <div id="tab-map" class="tab-content active">
      <div class="map-header">
        <span style="font-weight:600;">Disk Allocation Map (First 256 Blocks Shown)</span>
        <div class="legend">
          <div class="legend-item"><div class="legend-box" style="background:var(--purple)"></div> Superblock (Blk 0)</div>
          <div class="legend-item"><div class="legend-box" style="background:var(--warning)"></div> Bitmap (Blk 1)</div>
          <div class="legend-item"><div class="legend-box" style="background:var(--accent)"></div> Inodes (Blk 2-17)</div>
          <div class="legend-item"><div class="legend-box" style="background:var(--success)"></div> Used Data</div>
          <div class="legend-item"><div class="legend-box" style="background:#1e293b;border:1px solid #334155;"></div> Free</div>
        </div>
      </div>
      <div class="block-grid" id="block-grid-el">
        <!-- 256 or 512 block cells -->
      </div>
      <div class="block-info-card" id="block-hover-info">
        Hover or click on any block to inspect its details & jump to Hex Viewer
      </div>
    </div>

    <!-- Tab 2: Raw Hex Dump -->
    <div id="tab-hex" class="tab-content">
      <div class="hex-controls">
        <label>Block Number (0 .. 2047):</label>
        <input type="number" id="hex-blk-input" value="0" min="0" max="2047" style="width:80px;" onchange="loadHex(this.value)">
        <button class="action-btn" onclick="loadHex(document.getElementById('hex-blk-input').value)">Inspect Block</button>
        <button class="action-btn" onclick="loadHex(0)">Blk 0 (Superblock)</button>
        <button class="action-btn" onclick="loadHex(1)">Blk 1 (Bitmap)</button>
        <button class="action-btn" onclick="loadHex(2)">Blk 2 (Inode 0..7)</button>
      </div>
      <div class="hex-container" id="hex-view-el">Loading block 0...</div>
    </div>

    <!-- Tab 3: Metadata & Inode Inspector -->
    <div id="tab-info" class="tab-content">
      <div class="metadata-card" id="meta-view-el">Select a file or folder from the left pane or type "info &lt;name&gt;" to inspect its inode.</div>
    </div>
  </div>
</div>

<!-- Bottom Console -->
<div class="console-pane">
  <div class="console-header">
    <span>Interactive Shell Console</span>
    <div>
      <button class="action-btn" style="padding:2px 8px;font-size:10px;" onclick="runCmd('df')">df</button>
      <button class="action-btn" style="padding:2px 8px;font-size:10px;" onclick="runCmd('map')">map</button>
      <button class="action-btn" style="padding:2px 8px;font-size:10px;" onclick="runCmd('format')">format</button>
      <button class="action-btn" style="padding:2px 8px;font-size:10px;" onclick="clearConsole()">Clear</button>
    </div>
  </div>
  <div class="console-output" id="console-output-el"></div>
  <div class="console-input-row">
    <span class="console-prompt" id="console-prompt-el">unmad-fs:/> </span>
    <input type="text" class="console-input" id="console-input-el" placeholder="Type a command (format, create, write, read, ls, mkdir, cd, rmdir, info, df, map, hex)..." autofocus>
  </div>
</div>

<!-- Modal: Create File -->
<div class="modal-overlay" id="modal-create">
  <div class="modal-card">
    <h3>Create New File</h3>
    <input type="text" class="modal-input" id="create-filename" placeholder="Filename (max 27 chars, e.g. hello.txt)">
    <div class="modal-actions">
      <button class="action-btn" onclick="closeModals()">Cancel</button>
      <button class="action-btn" style="background:var(--accent);color:#0f172a;" onclick="submitCreate()">Create</button>
    </div>
  </div>
</div>

<!-- Modal: Mkdir -->
<div class="modal-overlay" id="modal-mkdir">
  <div class="modal-card">
    <h3>Create Directory</h3>
    <input type="text" class="modal-input" id="mkdir-name" placeholder="Directory name (e.g. docs)">
    <div class="modal-actions">
      <button class="action-btn" onclick="closeModals()">Cancel</button>
      <button class="action-btn" style="background:var(--accent);color:#0f172a;" onclick="submitMkdir()">Create</button>
    </div>
  </div>
</div>

<!-- Modal: Write Text -->
<div class="modal-overlay" id="modal-write">
  <div class="modal-card">
    <h3>Write Text to File</h3>
    <input type="text" class="modal-input" id="write-filename" placeholder="Target filename">
    <textarea class="modal-textarea" id="write-text" placeholder="Type text to write to disk..."></textarea>
    <div class="modal-actions">
      <button class="action-btn" onclick="closeModals()">Cancel</button>
      <button class="action-btn" style="background:var(--accent);color:#0f172a;" onclick="submitWrite()">Write to Disk</button>
    </div>
  </div>
</div>

<script>
  let selectedFile = null;
  let cmdHistory = [];
  let historyIdx = -1;
  let currentState = null;

  function sendBackend(msg) {
    if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.unmadFs) {
      window.webkit.messageHandlers.unmadFs.postMessage(msg);
    }
  }

  function appendConsole(text, isCmd = false) {
    const el = document.getElementById("console-output-el");
    if (isCmd) {
      el.textContent += "\n" + text + "\n";
    } else {
      el.textContent += text;
    }
    el.scrollTop = el.scrollHeight;
  }

  function clearConsole() {
    document.getElementById("console-output-el").textContent = "";
  }

  function runCmd(cmdStr) {
    appendConsole("unmad-fs:" + (currentState ? currentState.cwd : "/") + "> " + cmdStr, true);
    sendBackend({ action: "cmd", command: cmdStr });
  }

  // Backend response handler
  window.onBackendResult = function(data) {
    if (data.output) {
      appendConsole(data.output);
    }
    if (data.state) {
      renderState(data.state);
    }
    if (data.hex) {
      document.getElementById("hex-view-el").textContent = data.hex;
    }
  };

  function renderState(state) {
    currentState = state;
    document.getElementById("ui-cwd").textContent = state.cwd;
    document.getElementById("console-prompt-el").textContent = "unmad-fs:" + state.cwd + "> ";
    document.getElementById("badge-magic").textContent = "Magic: " + state.superblock.magic + " (UNMD)";
    document.getElementById("badge-blocks").textContent = "Free Blocks: " + state.superblock.free_blocks + " / " + state.superblock.total_blocks;
    document.getElementById("badge-inodes").textContent = "Free Inodes: " + state.superblock.free_inodes + " / " + state.superblock.total_inodes;

    // Render file list
    const listEl = document.getElementById("file-list-el");
    listEl.innerHTML = "";
    if (!state.entries || state.entries.length === 0) {
      listEl.innerHTML = "<div style='color:var(--text-muted);font-size:12px;padding:12px;text-align:center;'>(Directory is empty)</div>";
    } else {
      state.entries.forEach(ent => {
        const item = document.createElement("div");
        item.className = "file-item" + (selectedFile === ent.name ? " selected" : "");
        const isDir = ent.type === "DIR";
        item.innerHTML = `
          <div class="file-icon">${isDir ? "📁" : "📄"}</div>
          <div class="file-name">${ent.name}</div>
          <div class="file-meta">${isDir ? "DIR" : ent.size + " B"}</div>
        `;
        item.onclick = () => selectItem(ent);
        item.ondblclick = () => {
          if (isDir) {
            runCmd("cd " + ent.name);
          } else {
            runCmd("read " + ent.name);
          }
        };
        listEl.appendChild(item);
      });
    }

    // Render block map (first 256 blocks)
    renderBlockGrid(state);
  }

  function selectItem(ent) {
    selectedFile = ent.name;
    document.querySelectorAll(".file-item").forEach(el => el.classList.remove("selected"));
    event.currentTarget.classList.add("selected");
    runCmd("info " + ent.name);
    // Switch to info tab
    document.getElementById("meta-view-el").textContent = 
      `Inspecting '${ent.name}' (Inode ${ent.inode})\n` +
      `Type        : ${ent.type}\n` +
      `Size        : ${ent.size} bytes\n` +
      `Permissions : ${ent.perm}\n\nLoading direct and indirect pointers from disk...`;
    switchTab("info");
  }

  function renderBlockGrid(state) {
    const gridEl = document.getElementById("block-grid-el");
    gridEl.innerHTML = "";
    const usedSet = new Set(state.used_blocks || []);

    for (let b = 0; b < 256; b++) {
      const cell = document.createElement("div");
      cell.className = "block-cell";
      let typeDesc = "Free Data Block";
      if (b === 0) { cell.classList.add("blk-sb"); typeDesc = "Superblock (FS_MAGIC: UNMD)"; }
      else if (b === 1) { cell.classList.add("blk-bm"); typeDesc = "Free-Space Bitmap"; }
      else if (b >= 2 && b <= 17) { cell.classList.add("blk-in"); typeDesc = `Inode Table (Inodes ${ (b-2)*8 }..${ (b-2)*8+7 })`; }
      else if (usedSet.has(b)) { cell.classList.add("blk-used"); typeDesc = "Used Data Block"; }
      else { cell.classList.add("blk-free"); }

      cell.onmouseover = () => {
        document.getElementById("block-hover-info").innerHTML = 
          `<strong>Block ${b}</strong> | Offset: 0x${(b * 512).toString(16).padStart(6, '0')} | ${typeDesc}`;
      };
      cell.onclick = () => {
        document.getElementById("hex-blk-input").value = b;
        loadHex(b);
        switchTab("hex");
      };
      gridEl.appendChild(cell);
    }
  }

  function loadHex(blk) {
    sendBackend({ action: "getHex", block: parseInt(blk, 10) });
  }

  function switchTab(tabId) {
    document.querySelectorAll(".tab-btn").forEach(b => b.classList.remove("active"));
    document.querySelectorAll(".tab-content").forEach(c => c.classList.remove("active"));
    document.querySelector(`[onclick="switchTab('${tabId}')"]`).classList.add("active");
    document.getElementById(`tab-${tabId}`).classList.add("active");
  }

  // Modals
  function openCreateModal() { document.getElementById("modal-create").style.display = "flex"; document.getElementById("create-filename").focus(); }
  function openMkdirModal() { document.getElementById("modal-mkdir").style.display = "flex"; document.getElementById("mkdir-name").focus(); }
  function openWriteModal() {
    if (selectedFile) document.getElementById("write-filename").value = selectedFile;
    document.getElementById("modal-write").style.display = "flex";
    document.getElementById("write-text").focus();
  }
  function closeModals() {
    document.querySelectorAll(".modal-overlay").forEach(m => m.style.display = "none");
  }
  function submitCreate() {
    const f = document.getElementById("create-filename").value.trim();
    if (f) runCmd("create " + f);
    closeModals();
  }
  function submitMkdir() {
    const d = document.getElementById("mkdir-name").value.trim();
    if (d) runCmd("mkdir " + d);
    closeModals();
  }
  function submitWrite() {
    const f = document.getElementById("write-filename").value.trim();
    const t = document.getElementById("write-text").value;
    if (f && t) runCmd(`write ${f} ${t}`);
    closeModals();
  }
  function deleteSelected() {
    if (!selectedFile) {
      alert("Please select a file or folder to delete.");
      return;
    }
    runCmd("delete " + selectedFile);
  }

  // Console input
  const inputEl = document.getElementById("console-input-el");
  inputEl.addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
      const val = inputEl.value.trim();
      if (val) {
        cmdHistory.push(val);
        historyIdx = cmdHistory.length;
        runCmd(val);
        inputEl.value = "";
      }
    } else if (e.key === "ArrowUp") {
      if (historyIdx > 0) {
        historyIdx--;
        inputEl.value = cmdHistory[historyIdx];
      }
    } else if (e.key === "ArrowDown") {
      if (historyIdx < cmdHistory.length - 1) {
        historyIdx++;
        inputEl.value = cmdHistory[historyIdx];
      } else {
        historyIdx = cmdHistory.length;
        inputEl.value = "";
      }
    }
  });

  // Initial load
  window.onload = function() {
    sendBackend({ action: "getState" });
    loadHex(0);
    appendConsole("=== Welcome to Unmad Mini File System (MU CSE 61) ===\nType 'help' or commands below to interact with disk.img.\n");
  };
</script>
</body>
</html>
)rawhtml";

#endif // GUI_ASSETS_H
