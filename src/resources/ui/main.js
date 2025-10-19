const presetListElement = document.getElementById("preset-list");
const presetDetailsElement = document.getElementById("preset-details");
const presetSearchElement = document.getElementById("preset-search");

const uiState = {
  presets: [],
  activePresetId: null,
};

window.NAMBridge = {
  postMessage(message) {
    if (window.IPlugSendMsg) {
      window.IPlugSendMsg(message);
    }
  },
};

function renderPresetList(presets) {
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
    element.addEventListener("click", () => {
      const presetId = element.getAttribute("data-id");
      if (!presetId) {
        return;
      }
      window.NAMBridge.postMessage(
        JSON.stringify({
          type: "loadPreset",
          presetId,
        }),
      );
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
          <span>${attachment.hash}</span>
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
    <button id="download-preset">Download</button>
  `;

  const downloadButton = document.getElementById("download-preset");
  if (downloadButton) {
    downloadButton.addEventListener("click", () => {
      window.NAMBridge.postMessage(
        JSON.stringify({
          type: "downloadPreset",
          presetId: preset.id,
        }),
      );
    });
  }
}

function handleIncomingMessage(message) {
  const payload = JSON.parse(message);
  switch (payload.type) {
    case "state":
      uiState.presets = payload.presets ?? [];
      uiState.activePresetId = payload.activePresetId ?? null;
      renderPresetList(uiState.presets);
      renderPresetDetails(
        uiState.presets.find((preset) => preset.id === uiState.activePresetId) ?? null,
      );
      break;
    case "presetLoaded":
      uiState.activePresetId = payload.preset?.id ?? null;
      if (payload.preset) {
        const existingIndex = uiState.presets.findIndex((preset) => preset.id === payload.preset.id);
        if (existingIndex >= 0) {
          uiState.presets.splice(existingIndex, 1, payload.preset);
        } else {
          uiState.presets.push(payload.preset);
        }
      }
      renderPresetList(uiState.presets);
      renderPresetDetails(payload.preset ?? null);
      break;
    case "presetSearchResults":
      renderPresetList(payload.presets ?? []);
      break;
    default:
      console.warn("Unknown message type", payload.type);
  }
}

presetSearchElement?.addEventListener("input", (event) => {
  const message = {
    type: "search",
    query: event.target.value,
  };

  window.NAMBridge.postMessage(JSON.stringify(message));
});

window.IPlugReceiveData = (message) => {
  handleIncomingMessage(message);
};

renderPresetList([]);
renderPresetDetails(null);
