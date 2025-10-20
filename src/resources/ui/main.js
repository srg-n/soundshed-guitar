const presetListElement = document.getElementById("preset-list");
const presetDetailsElement = document.getElementById("preset-details");
const presetSearchElement = document.getElementById("preset-search");
const appRootElement = document.getElementById("app");

const notificationElement = document.createElement("div");
notificationElement.id = "notification";
notificationElement.className = "notification";
if (appRootElement && typeof appRootElement.prepend === "function") {
  appRootElement.prepend(notificationElement);
} else {
  document.body.prepend(notificationElement);
}

const REMOTE_BASE_URL = window.NAM_REMOTE_BASE_URL ?? "";

const DEFAULT_PRESETS = window.NAM_DEFAULT_PRESETS ?? [
  {
    id: "factory-clean",
    name: "Factory Clean",
    category: "Factory",
    description: "Balanced clean tone with plenty of headroom.",
    namModelId: "",
    irId: "",
    fxChain: [],
    attachments: [],
    parameters: [
      { id: "input_trim", value: 0.0 },
      { id: "output_trim", value: 0.0 },
      { id: "drive", value: 0.15 },
      { id: "tone", value: 0.55 },
      { id: "gate_enabled", value: 0.0 },
      { id: "gate_threshold", value: -60.0 },
    ],
  },
  {
    id: "factory-breakup",
    name: "Edge of Breakup",
    category: "Factory",
    description: "Touch-sensitive crunch that works great with single coils.",
    namModelId: "",
    irId: "",
    fxChain: [],
    attachments: [],
    parameters: [
      { id: "input_trim", value: -3.0 },
      { id: "output_trim", value: 0.0 },
      { id: "drive", value: 0.45 },
      { id: "tone", value: 0.5 },
      { id: "gate_enabled", value: 0.0 },
      { id: "gate_threshold", value: -55.0 },
    ],
  },
  {
    id: "factory-highgain",
    name: "Saturated Lead",
    category: "Factory",
    description: "Tight high-gain lead preset with a gentle noise gate.",
    namModelId: "",
    irId: "",
    fxChain: ["noise_gate"],
    attachments: [],
    parameters: [
      { id: "input_trim", value: -6.0 },
      { id: "output_trim", value: -3.0 },
      { id: "drive", value: 0.85 },
      { id: "tone", value: 0.65 },
      { id: "gate_enabled", value: 1.0 },
      { id: "gate_threshold", value: -50.0 },
    ],
  },
];

const uiState = {
  presets: [],
  filteredPresets: [],
  activePresetId: null,
  presetCache: new Map(),
};

window.NAMBridge = {
  postMessage(message) {
    if (window.IPlugSendMsg) {
      window.IPlugSendMsg(message);
    }
  },
};

function clonePreset(preset) {
  return JSON.parse(JSON.stringify(preset));
}

function clearNotification() {
  notificationElement.textContent = "";
  notificationElement.classList.remove("visible");
}

function showNotification(message, detail = "") {
  const resolvedMessage = detail ? `${message}: ${detail}` : message;
  notificationElement.textContent = resolvedMessage;
  notificationElement.classList.add("visible");
}

function renderPresetList(presets) {
  if (!presets.length) {
    presetListElement.innerHTML = '<p class="empty">No presets available.</p>';
    return;
  }

  presetListElement.innerHTML = presets
    .map(
      (preset) => `
        <article class="preset ${preset.id === uiState.activePresetId ? "active" : ""}" data-id="${preset.id}">
          <header>
            <h3>${preset.name}</h3>
            <span>${preset.category ?? ""}</span>
          </header>
          <p>${preset.description ?? ""}</p>
        </article>
      `,
    )
    .join("");

  presetListElement.querySelectorAll("article.preset").forEach((element) => {
    element.addEventListener("click", async () => {
      const presetId = element.getAttribute("data-id");
      if (!presetId) {
        return;
      }
      await applyPresetFromLibrary(presetId);
    });
  });
}

function renderPresetDetails(preset) {
  if (!preset) {
    presetDetailsElement.innerHTML = "<p>Select a preset to see details.</p>";
    return;
  }

  const attachments = (preset.attachments ?? [])
    .map(
      (attachment) => `
        <li>
          <strong>${attachment.type}</strong>
          <span>${attachment.hash ?? ""}</span>
        </li>
      `,
    )
    .join("");

  const fxChain = (preset.fxChain ?? [])
    .map((stage) => `<li>${stage}</li>`)
    .join("");

  presetDetailsElement.innerHTML = `
    <h2>${preset.name}</h2>
    <p>${preset.description ?? ""}</p>
    <section>
      <h3>FX Chain</h3>
      <ul>${fxChain}</ul>
    </section>
    <section>
      <h3>Attachments</h3>
      <ul>${attachments}</ul>
    </section>
    <button id="apply-preset">Apply Preset</button>
  `;

  const applyButton = document.getElementById("apply-preset");
  if (applyButton) {
    applyButton.addEventListener("click", async () => {
      await applyPresetFromLibrary(preset.id);
    });
  }
}

function handleIncomingMessage(message) {
  const payload = JSON.parse(message);
  switch (payload.type) {
    case "state": {
      uiState.activePresetId = payload.activePresetId ?? null;
      if (payload.preset) {
        uiState.presetCache.set(payload.preset.id, payload.preset);
        if (!uiState.presets.some((preset) => preset.id === payload.preset.id)) {
          uiState.presets = [payload.preset, ...uiState.presets];
          filterPresets(presetSearchElement?.value ?? "");
        }
      }
      renderPresetList(uiState.filteredPresets);
      const preset = payload.preset ?? uiState.presetCache.get(uiState.activePresetId) ?? null;
      renderPresetDetails(preset ? clonePreset(preset) : null);
      clearNotification();
      break;
    }
    case "presetLoaded": {
      const preset = payload.preset;
      if (preset) {
        uiState.activePresetId = preset.id;
        uiState.presetCache.set(preset.id, preset);
        renderPresetDetails(clonePreset(preset));
      }
      renderPresetList(uiState.filteredPresets);
      clearNotification();
      break;
    }
    case "error": {
      console.error("Plugin error", payload);
      showNotification(payload.message ?? "An error occurred", payload.detail ?? "");
      break;
    }
    default:
      console.warn("Unknown message type", payload.type);
  }
}

function filterPresets(query) {
  const normalized = query.trim().toLowerCase();
  if (!normalized) {
    uiState.filteredPresets = uiState.presets.slice();
  } else {
    uiState.filteredPresets = uiState.presets.filter((preset) => {
      const tokens = [preset.name, preset.category, preset.description];
      return tokens.some((token) => token && token.toLowerCase().includes(normalized));
    });
  }
  renderPresetList(uiState.filteredPresets);
}

async function loadPresetMetadata(presetId) {
  if (uiState.presetCache.has(presetId)) {
    return clonePreset(uiState.presetCache.get(presetId));
  }

  const localPreset = uiState.presets.find((preset) => preset.id === presetId);
  if (localPreset) {
    uiState.presetCache.set(localPreset.id, localPreset);
    return clonePreset(localPreset);
  }

  if (!REMOTE_BASE_URL) {
    throw new Error("Remote preset service is not configured.");
  }

  const baseUrl = REMOTE_BASE_URL.replace(/\/$/, "");
  const response = await fetch(`${baseUrl}/presets/${encodeURIComponent(presetId)}`);
  if (!response.ok) {
    throw new Error(`Failed to fetch preset ${presetId}: ${response.status}`);
  }

  const data = await response.json();
  const preset = Array.isArray(data) ? data[0] : data;
  if (!preset) {
    throw new Error(`Preset ${presetId} not found`);
  }

  uiState.presetCache.set(preset.id, preset);
  return clonePreset(preset);
}

function resolveAttachmentUrl(attachment) {
  const candidates = [
    attachment.downloadUrl,
    attachment.url,
    attachment.href,
    attachment.filePath,
    attachment.path,
  ].filter(Boolean);

  const baseUrl = REMOTE_BASE_URL.replace(/\/$/, "");

  for (const candidate of candidates) {
    if (typeof candidate !== "string") {
      continue;
    }
    if (/^https?:\/\//i.test(candidate)) {
      return candidate;
    }

    if (candidate.startsWith("/")) {
      return baseUrl ? `${baseUrl}${candidate}` : null;
    }

    return baseUrl ? `${baseUrl}/${candidate}` : null;
  }

  return null;
}

function arrayBufferToBase64(buffer) {
  const bytes = new Uint8Array(buffer);
  let binary = "";
  const chunkSize = 0x8000;
  for (let offset = 0; offset < bytes.length; offset += chunkSize) {
    const slice = bytes.subarray(offset, offset + chunkSize);
    binary += String.fromCharCode(...slice);
  }
  return btoa(binary);
}

async function enrichAttachment(attachment) {
  if (attachment.data) {
    return attachment;
  }

  const url = resolveAttachmentUrl(attachment);
  if (!url) {
    return attachment;
  }

  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`Failed to fetch attachment from ${url}`);
  }

  const buffer = await response.arrayBuffer();
  return {
    ...attachment,
    data: arrayBufferToBase64(buffer),
  };
}

async function applyPresetFromLibrary(presetId) {
  try {
    clearNotification();
    const preset = await loadPresetMetadata(presetId);
    const attachments = await Promise.all((preset.attachments ?? []).map(enrichAttachment));
    const presetPayload = {
      ...preset,
      attachments,
    };

    uiState.presetCache.set(presetPayload.id, clonePreset(presetPayload));

    uiState.activePresetId = presetPayload.id;
    renderPresetList(uiState.filteredPresets);
    renderPresetDetails(clonePreset(presetPayload));
    window.NAMBridge.postMessage(
      JSON.stringify({
        type: "loadPreset",
        preset: presetPayload,
      }),
    );
  } catch (error) {
    console.error("Failed to apply preset", error);
    showNotification("Failed to apply preset", error instanceof Error ? error.message : "Unknown error");
  }
}

async function loadPresetIndex() {
  try {
    if (!REMOTE_BASE_URL) {
      throw new Error("Remote preset service disabled");
    }

    const response = await fetch(`${REMOTE_BASE_URL.replace(/\/$/, "")}/presets`);
    if (!response.ok) {
      throw new Error(`Failed to fetch presets index: ${response.status}`);
    }

    const data = await response.json();
    const presets = Array.isArray(data) ? data : data.presets ?? [];
    uiState.presets = presets.length ? presets : DEFAULT_PRESETS.slice();
    uiState.filteredPresets = uiState.presets.slice();
    uiState.presets.forEach((preset) => {
      uiState.presetCache.set(preset.id, preset);
    });
    renderPresetList(uiState.filteredPresets);
  } catch (error) {
    console.error("Failed to load preset index", error);
    uiState.presets = DEFAULT_PRESETS.slice();
    uiState.filteredPresets = uiState.presets.slice();
    uiState.presets.forEach((preset) => {
      uiState.presetCache.set(preset.id, preset);
    });
    renderPresetList(uiState.filteredPresets);
  }
}

async function initialize() {
  alert(REMOTE_BASE_URL);
  if (REMOTE_BASE_URL) {
    await loadPresetIndex();
  } else {
    uiState.presets = DEFAULT_PRESETS.slice();
    uiState.filteredPresets = uiState.presets.slice();
    uiState.presets.forEach((preset) => {
      uiState.presetCache.set(preset.id, preset);
    });
    renderPresetList(uiState.filteredPresets);
  }
  window.NAMBridge.postMessage(JSON.stringify({ type: "requestState" }));
}

presetSearchElement?.addEventListener("input", (event) => {
  filterPresets(event.target.value ?? "");
});

window.IPlugReceiveData = (message) => {
  handleIncomingMessage(message);
};

renderPresetList([]);
renderPresetDetails(null);
initialize();
