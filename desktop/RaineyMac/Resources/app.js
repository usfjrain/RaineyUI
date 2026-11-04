const systems = [
  system("nes", "NES", "8-BIT", "native", ["nes", "zip"], "assets/nes.png"),
  system("snes", "SNES", "16-BIT", "native", ["sfc", "smc", "zip"], "assets/snes.png"),
  system("genesis", "Genesis", "16-BIT", "native", ["gen", "md", "bin", "zip"], "assets/sega genesis.png"),
  system("tg16", "TurboGrafx-16", "PC-E", "native", ["pce", "cue", "chd", "zip"]),
  system("neogeo", "Neo Geo", "MVS", "native", ["zip", "7z"]),
  system("saturn", "Sega Saturn", "CD", "good", ["cue", "bin", "chd", "iso"]),
  system("playstation", "PlayStation", "PS1", "native", ["cue", "bin", "chd", "iso"], "assets/ps1.png"),
  system("n64", "Nintendo 64", "64", "good", ["z64", "n64", "v64", "zip"], "assets/nintendo 64.png"),
  system("gba", "Game Boy Advance", "GBA", "native", ["gba", "zip"], "assets/gba cut.png"),
  system("nds", "Nintendo DS", "DS", "good", ["nds", "zip"]),
  system("dreamcast", "Dreamcast", "DC", "good", ["gdi", "chd", "cdi"], "assets/sega dreamcast.png"),
  system("psp", "PSP", "PSP", "good", ["iso", "cso"], "assets/psp.png"),
  system("gamecube", "GameCube", "GC", "heavy", ["iso", "gcm", "rvz", "ciso", "gcz"], "assets/gamecube.png"),
  system("wii", "Wii", "WII", "heavy", ["iso", "wbfs", "rvz", "wad"], "assets/wii.png"),
  system("ps2", "PlayStation 2", "PS2", "heavy", ["iso", "chd", "cue", "bin"], "assets/ps2.png"),
  system("xbox", "Xbox", "XB", "heavy", ["iso", "xiso"]),
  system("xbox360", "Xbox 360", "360", "experimental", ["iso", "xex"], "assets/xbox 360.png"),
  system("ps3", "PlayStation 3", "PS3", "experimental", ["iso", "pkg", "rap", "sfb", "elf", "self"]),
  system("3ds", "Nintendo 3DS", "3DS", "experimental", ["3ds", "cia", "cci"]),
  system("arcade", "Arcade", "COIN", "good", ["zip", "7z"], "assets/arcade.png"),
  system("android", "Android Games", "PLAY", "external", ["apk"], "assets/android.png")
];

let state = {
  screen: "splash",
  selectedSystem: 0,
  mode: "grid",
  scanlines: true,
  retroArch: {
    enabled: false,
    allowFallback: true,
    appPath: "/Applications/RetroArch.app",
    cores: {}
  },
  games: [],
  folders: {},
  selectedGameId: null
};

const defaultRetroArchCores = {
  nes: "mesen_libretro.dylib",
  snes: "snes9x_libretro.dylib",
  genesis: "genesis_plus_gx_libretro.dylib",
  tg16: "mednafen_pce_fast_libretro.dylib",
  neogeo: "fbneo_libretro.dylib",
  saturn: "mednafen_saturn_libretro.dylib",
  playstation: "pcsx_rearmed_libretro.dylib; swanstation_libretro.dylib; mednafen_psx_hw_libretro.dylib; mednafen_psx_libretro.dylib; beetle_psx_hw_libretro.dylib",
  n64: "mupen64plus_next_libretro.dylib",
  gba: "mgba_libretro.dylib",
  nds: "desmume_libretro.dylib",
  dreamcast: "flycast_libretro.dylib",
  psp: "ppsspp_libretro.dylib",
  gamecube: "dolphin_libretro.dylib",
  wii: "dolphin_libretro.dylib",
  ps2: "pcsx2_libretro.dylib",
  arcade: "fbneo_libretro.dylib"
};

function system(id, name, logo, capability, extensions, asset = null) {
  return { id, name, logo, capability, extensions, asset };
}

function nativePost(payload) {
  if (window.webkit?.messageHandlers?.rainey) {
    window.webkit.messageHandlers.rainey.postMessage(payload);
  } else {
    showStatus("homeStatus", "This desktop UI is running outside the Mac app wrapper.", "error");
  }
}

window.raineyNative = {
  onScanResult(result) {
    const existing = state.games.filter(game => game.system !== result.systemId);
    state.games = existing.concat(result.games || []);
    if (result.folder) state.folders[result.systemId] = result.folder;
    saveLibrary();
    showStatus("libraryStatus", `Scanned ${result.games.length} game(s) from ${result.folder.name}.`, "success");
    render();
  },
  onContentLoaded(result) {
    const incoming = result.games || [];
    const ids = new Set(incoming.map(game => game.id));
    state.games = state.games.filter(game => !ids.has(game.id)).concat(incoming);
    saveLibrary();
    showStatus("libraryStatus", `Added ${incoming.length} game(s) to ${systemName(result.systemId)}.`, "success");
    render();
  },
  onLibraryLoaded(payload) {
    if (payload && typeof payload === "object") {
      state.games = Array.isArray(payload.games) ? payload.games : [];
      state.folders = payload.folders || {};
      state.scanlines = payload.scanlines !== false;
      state.retroArch = {
        ...state.retroArch,
        ...(payload.retroArch || {}),
        cores: payload.retroArch?.cores || state.retroArch.cores || {}
      };
    }
    render();
  },
  onRetroArchAppPicked(result) {
    state.retroArch.appPath = result.path || "";
    state.retroArch.enabled = true;
    saveLibrary();
    showStatus("settingsStatus", "RetroArch app selected.", "success");
    render();
  },
  onRetroArchCorePicked(result) {
    state.retroArch.cores[result.systemId] = result.path || "";
    saveLibrary();
    showStatus("settingsStatus", `RetroArch core set for ${systemName(result.systemId)}.`, "success");
    render();
  },
  onMessage(payload) {
    const target = state.screen === "settings" ? "settingsStatus" : state.screen === "detail" ? "detailStatus" : "libraryStatus";
    showStatus(target, payload.message || "Done.", payload.kind || "info");
  }
};

function saveLibrary() {
  nativePost({
    action: "saveLibrary",
    payload: JSON.stringify({
      games: state.games,
      folders: state.folders,
      scanlines: state.scanlines,
      retroArch: state.retroArch
    })
  });
}

function show(id) {
  state.screen = id;
  document.querySelectorAll(".view").forEach(view => view.classList.toggle("active", view.id === id));
  render();
}

function currentSystem() {
  return systems[state.selectedSystem];
}

function systemName(systemId) {
  return systems.find(item => item.id === systemId)?.name || systemId;
}

function currentGames() {
  return state.games.filter(game => game.system === currentSystem().id);
}

function selectedGame() {
  return state.games.find(game => game.id === state.selectedGameId) || currentGames()[0] || state.games[0];
}

function shouldUseRetroArch(game) {
  if (state.retroArch.enabled) return true;
  if (game.system === "playstation" && ["chd", "cue", "iso"].includes(game.ext)) return true;
  return false;
}

function colorFor(text, offset) {
  let hash = 0;
  for (let i = 0; i < text.length; i++) hash = ((hash << 5) - hash) + text.charCodeAt(i);
  const r = 40 + Math.abs((hash >> (offset + 2)) % 130);
  const g = 70 + Math.abs((hash >> offset) % 150);
  const b = 110 + Math.abs(hash % 140);
  return `rgb(${r}, ${g}, ${b})`;
}

function initials(text) {
  const parts = text.trim().split(/\s+/).filter(Boolean);
  return (parts.length ? parts.slice(0, 3).map(part => part[0].toUpperCase()).join("") : "R").slice(0, 3);
}

function art(label, compact = false, asset = null) {
  if (asset) {
    return `<div class="art asset-art"><img src="${escapeHtml(asset)}" alt="${escapeHtml(label)}"></div>`;
  }
  return `<div class="art generated" style="--a:${colorFor(label, 4)};--b:${colorFor(label, 9)}">
    <div><strong>${escapeHtml(initials(label))}</strong>${compact ? "" : `<span>${escapeHtml(label.toUpperCase())}</span>`}</div>
  </div>`;
}

function renderHome() {
  const carousel = document.getElementById("carousel");
  carousel.innerHTML = systems.map((item, index) => `
    <article class="system-card ${index === state.selectedSystem ? "selected" : ""}" data-system="${index}" tabindex="0">
      ${art(item.logo, false, item.asset)}
      <div>
        <h3>${escapeHtml(item.name)}</h3>
        <div class="capability ${item.capability}">${capabilityLabel(item.capability)}</div>
      </div>
    </article>
  `).join("");

  carousel.querySelectorAll(".system-card").forEach(card => {
    card.addEventListener("click", () => {
      state.selectedSystem = Number(card.dataset.system);
      show("library");
    });
    card.addEventListener("focus", () => {
      state.selectedSystem = Number(card.dataset.system);
      renderHome();
    });
  });

  requestAnimationFrame(() => {
    const selected = carousel.querySelector(".system-card.selected");
    if (selected && state.screen === "home") {
      selected.scrollIntoView({ behavior: "smooth", inline: "center", block: "nearest" });
    }
  });
}

function capabilityLabel(value) {
  return {
    native: "Great on this Mac",
    good: "Good target",
    heavy: "Heavy target",
    experimental: "Experimental",
    external: "External"
  }[value] || value;
}

function renderLibrary() {
  const system = currentSystem();
  const games = currentGames();
  document.getElementById("libraryTitle").textContent = system.name;
  document.getElementById("librarySubtitle").textContent = `${games.length} game${games.length === 1 ? "" : "s"} | ${system.extensions.map(ext => "." + ext).join(", ")}`;
  document.getElementById("modeButton").textContent = state.mode === "grid" ? "List" : "Grid";
  renderRail("recentRail", "Recently Played", games.filter(game => game.recent), "Games launched for this system will appear here.");
  renderRail("favoriteRail", "Favorites", games.filter(game => game.favorite), "Mark games as favorites from the detail page.");

  const body = document.getElementById("libraryBody");
  if (!games.length) {
    body.innerHTML = `<div class="empty"><div><div class="empty-mark">${escapeHtml(system.logo.slice(0, 2))}</div><h2>No ${escapeHtml(system.name)} games yet</h2><p>Pick a folder or use Load Content for this system.</p><br><button id="emptyPick">Pick Folder</button></div></div>`;
    document.getElementById("emptyPick").addEventListener("click", pickCurrentFolder);
    return;
  }

  body.innerHTML = state.mode === "grid"
    ? `<div class="grid">${games.map(cardHtml).join("")}</div>`
    : `<div class="list">${games.map(listHtml).join("")}</div>`;
  body.querySelectorAll("[data-game]").forEach(card => {
    card.addEventListener("click", () => openGame(card.dataset.game));
  });
}

function renderRail(id, title, list, emptyText) {
  const node = document.getElementById(id);
  node.innerHTML = `<h3>${title}</h3>` + (
    list.length
      ? `<div class="rail-items">${list.slice(0, 12).map(game => `
          <article class="rail-card" data-game="${escapeHtml(game.id)}" tabindex="0">
            ${art(game.title, true)}
            <div>
              <h4 class="${game.favorite ? "favorite" : ""}">${game.favorite ? "* " : ""}${escapeHtml(game.title)}</h4>
              <p class="meta">${escapeHtml(game.ext.toUpperCase())}</p>
            </div>
          </article>
        `).join("")}</div>`
      : `<p class="meta">${escapeHtml(emptyText)}</p>`
  );
  node.querySelectorAll("[data-game]").forEach(card => card.addEventListener("click", () => openGame(card.dataset.game)));
}

function cardHtml(game) {
  return `<article class="game-card" data-game="${escapeHtml(game.id)}" tabindex="0">
    ${art(game.title, true)}
    <h4 class="${game.favorite ? "favorite" : ""}">${game.favorite ? "* " : ""}${escapeHtml(game.title)}</h4>
    <p class="meta">.${escapeHtml(game.ext)}</p>
  </article>`;
}

function listHtml(game) {
  return `<article class="game-list-item" data-game="${escapeHtml(game.id)}" tabindex="0">
    ${art(game.title, true)}
    <div>
      <h4 class="${game.favorite ? "favorite" : ""}">${game.favorite ? "* " : ""}${escapeHtml(game.title)}</h4>
      <p class="meta">${escapeHtml(game.path || "")}</p>
    </div>
  </article>`;
}

function openGame(id) {
  state.selectedGameId = id;
  show("detail");
}

function renderDetail() {
  const game = selectedGame();
  if (!game) {
    show("library");
    return;
  }
  const system = systems.find(item => item.id === game.system);
  document.getElementById("detailBody").innerHTML = `
    <div class="art generated detail-art" style="--a:${colorFor(game.title, 4)};--b:${colorFor(game.title, 9)}">
      <div><strong>${escapeHtml(initials(game.title))}</strong><span>${escapeHtml(game.title.toUpperCase())}</span></div>
    </div>
    <div class="detail-copy">
      <h2>${escapeHtml(game.title)}</h2>
      <div class="system-name">${escapeHtml(system?.name || game.system)}</div>
      <div class="actions">
        <button id="playButton">Play</button>
        <button id="favoriteButton" class="${game.favorite ? "active" : ""}">${game.favorite ? "Favorited" : "Favorite"}</button>
        <button id="finderButton">Finder</button>
        <button data-action="library">Back</button>
      </div>
      <div class="metadata">
        <div><b>File:</b> ${escapeHtml(game.path || "Unknown")}</div>
        <div><b>Year:</b> ${escapeHtml(game.year || "Unknown")}</div>
        <div><b>Genre:</b> ${escapeHtml(game.genre || "Unknown")}</div>
        <div><b>Developer:</b> ${escapeHtml(game.developer || "Unknown")}</div>
        <div><b>Description:</b> Local metadata placeholder. Artwork import and emulator-specific launch profiles can be added later.</div>
      </div>
      <div id="detailStatus" class="status"></div>
    </div>`;

  document.getElementById("playButton").addEventListener("click", () => {
    if (game.system === "playstation" && game.ext === "bin") {
      showStatus("detailStatus", "For PlayStation games, launch the .cue, .chd, or .iso file. Raw .bin tracks often will not boot by themselves.", "error");
      return;
    }
    game.recent = true;
    game.lastPlayedAt = new Date().toISOString();
    saveLibrary();
    if (shouldUseRetroArch(game)) {
      const coreSetting = state.retroArch.cores[game.system] || defaultRetroArchCores[game.system] || "";
      nativePost({
        action: "launchRetroArch",
        romPath: game.path,
        appPath: state.retroArch.appPath,
        coreSetting,
        allowFallback: state.retroArch.allowFallback || !state.retroArch.enabled
      });
    } else {
      nativePost({ action: "launch", path: game.path });
    }
  });
  document.getElementById("favoriteButton").addEventListener("click", () => {
    game.favorite = !game.favorite;
    saveLibrary();
    renderDetail();
  });
  document.getElementById("finderButton").addEventListener("click", () => nativePost({ action: "showInFinder", path: game.path }));
  bindActions();
}

function renderSettings() {
  document.getElementById("settingsGrid").innerHTML = `
    <div class="setting-row">
      <div class="toggle ${state.scanlines ? "" : "off"}" id="scanlineToggle">
        <span>Scanline overlay</span>
        <span class="switch"><span></span></span>
      </div>
      <div class="meta">CRT-style overlay for the launcher</div>
    </div>
    <div class="setting-row">
      <div>
        <strong>Launch mode</strong>
        <p class="meta">${state.retroArch.enabled ? "RetroArch first, default macOS launcher as fallback." : "Default macOS launcher. Associate each extension with your emulator."}</p>
      </div>
      <button id="helpLaunch">Help</button>
    </div>
    <div class="setting-row">
      <div class="toggle ${state.retroArch.enabled ? "" : "off"}" id="retroArchToggle">
        <span>Use RetroArch first</span>
        <span class="switch"><span></span></span>
      </div>
      <div class="meta">Optional. Rainey does not bundle RetroArch or cores.</div>
    </div>
    <div class="setting-row">
      <div>
        <strong>RetroArch app</strong>
        <p class="meta">${escapeHtml(state.retroArch.appPath || "No app selected")}</p>
      </div>
      <button id="retroArchAppButton">Choose App</button>
    </div>
    <div class="setting-row">
      <div class="toggle ${state.retroArch.allowFallback ? "" : "off"}" id="retroArchFallbackToggle">
        <span>Fallback launcher</span>
        <span class="switch"><span></span></span>
      </div>
      <div class="meta">If RetroArch or a core is missing, open with the default macOS app.</div>
    </div>
    <div class="setting-section-title">RetroArch cores per system</div>
    ${systems.map(item => `
      <div class="setting-row core-row">
        <div>
          <strong>${escapeHtml(item.name)}</strong>
          <input data-core-input="${escapeHtml(item.id)}" value="${escapeHtml(state.retroArch.cores[item.id] || defaultRetroArchCores[item.id] || "")}" placeholder="core name or /path/to/core.dylib">
          <p class="meta">Use a core name or choose a local .dylib core.</p>
        </div>
        <button data-core-pick="${escapeHtml(item.id)}">Choose Core</button>
      </div>
    `).join("")}
    <div class="setting-section-title">ROM folders</div>
    ${systems.map(item => `
      <div class="setting-row">
        <div>
          <strong>${escapeHtml(item.name)}</strong>
          <p class="meta">${escapeHtml(state.folders[item.id]?.path || "No folder selected")} | ${item.extensions.map(ext => "." + ext).join(", ")}</p>
        </div>
        <button data-pick="${escapeHtml(item.id)}">Pick Folder</button>
      </div>
    `).join("")}`;

  document.getElementById("scanlineToggle").addEventListener("click", () => {
    state.scanlines = !state.scanlines;
    saveLibrary();
    render();
  });
  document.getElementById("helpLaunch").addEventListener("click", () => {
    showStatus("settingsStatus", "RetroArch support uses RetroArch.app plus a per-system libretro .dylib core. If disabled or missing, Rainey can fall back to default macOS file associations.", "info");
  });
  document.getElementById("retroArchToggle").addEventListener("click", () => {
    state.retroArch.enabled = !state.retroArch.enabled;
    saveLibrary();
    render();
  });
  document.getElementById("retroArchFallbackToggle").addEventListener("click", () => {
    state.retroArch.allowFallback = !state.retroArch.allowFallback;
    saveLibrary();
    render();
  });
  document.getElementById("retroArchAppButton").addEventListener("click", () => {
    nativePost({ action: "chooseRetroArchApp" });
  });
  document.querySelectorAll("[data-core-pick]").forEach(button => {
    button.addEventListener("click", () => {
      nativePost({ action: "chooseRetroArchCore", systemId: button.dataset.corePick });
    });
  });
  document.querySelectorAll("[data-core-input]").forEach(input => {
    input.addEventListener("change", () => {
      state.retroArch.cores[input.dataset.coreInput] = input.value.trim();
      saveLibrary();
      showStatus("settingsStatus", `RetroArch core setting updated for ${systemName(input.dataset.coreInput)}.`, "success");
    });
  });
  document.querySelectorAll("[data-pick]").forEach(button => {
    button.addEventListener("click", () => pickFolderFor(button.dataset.pick));
  });
}

function pickCurrentFolder() {
  pickFolderFor(currentSystem().id);
}

function pickFolderFor(systemId) {
  const system = systems.find(item => item.id === systemId);
  if (!system) return;
  nativePost({ action: "pickFolder", systemId: system.id, extensions: system.extensions });
}

function showStatus(id, message, kind = "info") {
  const node = document.getElementById(id);
  if (!node) return;
  node.className = `status show ${kind === "error" ? "error" : kind === "success" ? "success" : ""}`;
  node.textContent = message;
}

function render() {
  renderHome();
  renderLibrary();
  renderSettings();
  if (state.screen === "detail") renderDetail();
  document.body.classList.toggle("scanlines", state.scanlines);
  bindActions();
}

function bindActions() {
  document.querySelectorAll("[data-action='home']").forEach(button => button.onclick = () => show("home"));
  document.querySelectorAll("[data-action='library']").forEach(button => button.onclick = () => show("library"));
  document.querySelectorAll("[data-action='settings']").forEach(button => button.onclick = () => show("settings"));
}

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

document.getElementById("modeButton").addEventListener("click", () => {
  state.mode = state.mode === "grid" ? "list" : "grid";
  renderLibrary();
});

document.getElementById("pickFolderButton").addEventListener("click", pickCurrentFolder);
document.getElementById("loadContentButton").addEventListener("click", () => {
  const system = currentSystem();
  nativePost({ action: "pickFiles", systemId: system.id, extensions: system.extensions });
});
document.getElementById("saveButton").addEventListener("click", () => {
  saveLibrary();
  showStatus("settingsStatus", "Desktop library saved.", "success");
});

document.getElementById("carouselWrap").addEventListener("wheel", event => {
  if (Math.abs(event.deltaY) > Math.abs(event.deltaX)) {
    event.currentTarget.scrollLeft += event.deltaY;
    event.preventDefault();
  }
}, { passive: false });

document.addEventListener("keydown", event => {
  if (event.key === "Escape" || event.key === "Backspace") {
    if (state.screen === "detail") show("library");
    else if (state.screen === "library" || state.screen === "settings") show("home");
  }
  if (event.key === "Enter") {
    if (state.screen === "home") show("library");
    else if (state.screen === "detail") document.getElementById("playButton")?.click();
  }
  if (event.key.toLowerCase() === "s") {
    state.scanlines = !state.scanlines;
    saveLibrary();
    render();
  }
  if (event.key === "ArrowRight" && state.screen === "home") {
    state.selectedSystem = Math.min(systems.length - 1, state.selectedSystem + 1);
    renderHome();
  }
  if (event.key === "ArrowLeft" && state.screen === "home") {
    state.selectedSystem = Math.max(0, state.selectedSystem - 1);
    renderHome();
  }
  if (event.key.toLowerCase() === "m") show("settings");
});

nativePost({ action: "loadLibrary" });
setTimeout(() => show("home"), 1450);
render();
