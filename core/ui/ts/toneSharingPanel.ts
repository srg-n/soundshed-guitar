type ToneSharingUser = {
  id: string;
  email: string;
  role: string;
};

type ToneSharingItem = {
  id: string;
  title: string;
  type: string;
  moderationStatus?: string;
};

type ToneSharingPack = {
  id: string;
  title: string;
  moderationStatus?: string;
};

type ToneSharingRow = {
  id: string;
  slug: string;
  title: string;
  items: Array<{ id: string; kind: "item" | "pack"; title: string; type: string | null }>;
};

const storageKeys = {
  apiBase: "toneSharing.apiBase",
  sessionId: "toneSharing.sessionId"
};

const state = {
  apiBase: "https://api.soundshed.com/v1",
  sessionId: "",
  user: null as ToneSharingUser | null,
  myItems: [] as ToneSharingItem[]
};

let browseMode: "featured" | "items" | "packs" | "mine" = "featured";

function element<T extends HTMLElement>(id: string): T | null {
  return document.getElementById(id) as T | null;
}

function setText(id: string, value: string): void {
  const target = element<HTMLElement>(id);
  if (target) {
    target.textContent = value;
  }
}

function normalizeBase(input: string): string {
  const trimmed = input.trim();
  if (!trimmed) {
    return "https://api.soundshed.com/v1";
  }
  return trimmed.endsWith("/") ? trimmed.slice(0, -1) : trimmed;
}

async function apiFetch<T = unknown>(path: string, init: RequestInit = {}): Promise<T> {
  const headers = new Headers(init.headers ?? {});
  if (state.sessionId) {
    headers.set("x-session-id", state.sessionId);
  }
  const response = await fetch(`${state.apiBase}${path}`, {
    ...init,
    headers,
    credentials: "include"
  });

  const payload = await response.json().catch(() => null);
  if (!response.ok || !payload || payload.ok === false) {
    const message = payload?.error?.message || `Request failed (${response.status})`;
    throw new Error(message);
  }

  return payload.data as T;
}

function currentTurnstileToken(): string {
  return element<HTMLInputElement>("tone-sharing-turnstile-token")?.value.trim() ?? "";
}

async function loadAuthSession(): Promise<void> {
  try {
    const data = await apiFetch<{ user: ToneSharingUser | null }>("/auth/me");
    state.user = data.user;
    if (data.user) {
      setText("tone-sharing-auth-status", `Signed in as ${data.user.email}`);
      await loadMine();
    } else {
      setText("tone-sharing-auth-status", "Signed out");
      renderPackItemSelection([]);
    }
  } catch (error) {
    setText("tone-sharing-auth-status", `Auth check failed: ${(error as Error).message}`);
  }
}

async function sendCode(): Promise<void> {
  const email = element<HTMLInputElement>("tone-sharing-email")?.value.trim() ?? "";
  if (!email) {
    setText("tone-sharing-auth-status", "Enter an email address");
    return;
  }

  setText("tone-sharing-auth-status", "Sending code...");
  try {
    await apiFetch("/auth/start", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ email, turnstileToken: currentTurnstileToken() })
    });
    setText("tone-sharing-auth-status", "Code sent. Check your email.");
  } catch (error) {
    setText("tone-sharing-auth-status", `Send code failed: ${(error as Error).message}`);
  }
}

async function verifyCode(): Promise<void> {
  const email = element<HTMLInputElement>("tone-sharing-email")?.value.trim() ?? "";
  const code = element<HTMLInputElement>("tone-sharing-code")?.value.trim() ?? "";
  if (!email || !code) {
    setText("tone-sharing-auth-status", "Enter email and code");
    return;
  }

  setText("tone-sharing-auth-status", "Signing in...");
  try {
    const data = await apiFetch<{ sessionId?: string; user: ToneSharingUser }>("/auth/verify", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ email, code })
    });

    state.user = data.user;
    state.sessionId = data.sessionId ?? "";
    if (state.sessionId) {
      localStorage.setItem(storageKeys.sessionId, state.sessionId);
    }
    setText("tone-sharing-auth-status", `Signed in as ${data.user.email}`);
    await Promise.all([loadBrowse(), loadMine()]);
  } catch (error) {
    setText("tone-sharing-auth-status", `Sign-in failed: ${(error as Error).message}`);
  }
}

async function signOut(): Promise<void> {
  try {
    await apiFetch("/auth/logout", { method: "POST" });
  } catch {
  }

  state.sessionId = "";
  state.user = null;
  localStorage.removeItem(storageKeys.sessionId);
  setText("tone-sharing-auth-status", "Signed out");
  await loadBrowse();
}

function renderFeedRows(rows: ToneSharingRow[]): void {
  const feed = element<HTMLElement>("tone-sharing-feed");
  if (!feed) {
    return;
  }

  if (!rows.length) {
    feed.innerHTML = `<div class="tone-sharing-status">No content yet. Publish the first tone.</div>`;
    return;
  }

  feed.innerHTML = rows
    .map(
      (row) => `
        <div class="tone-sharing-row">
          <div class="tone-sharing-row-title">${row.title}</div>
          <div class="tone-sharing-row-track">
            ${row.items
              .map(
                (item) => `
                  <div class="tone-sharing-card-item" data-kind="${item.kind}" data-id="${item.id}">
                    <div class="tone-sharing-card-item-title">${item.title}</div>
                    <div class="tone-sharing-card-item-meta">${item.kind === "item" ? item.type ?? "preset" : "pack"}</div>
                    <div class="tone-sharing-card-item-actions">
                      <button type="button" data-action="open">Open</button>
                      <button type="button" data-action="download">Download</button>
                    </div>
                  </div>
                `
              )
              .join("")}
          </div>
        </div>
      `
    )
    .join("");
}

function buildSingleRow(title: string, entries: Array<{ id: string; title: string; type?: string }>, kind: "item" | "pack"): ToneSharingRow {
  return {
    id: `generated-${title.toLowerCase().replace(/\s+/g, "-")}`,
    slug: `generated-${title.toLowerCase().replace(/\s+/g, "-")}`,
    title,
    items: entries.map((entry) => ({ id: entry.id, kind, title: entry.title, type: entry.type ?? null }))
  };
}

async function loadBrowse(): Promise<void> {
  const feed = element<HTMLElement>("tone-sharing-feed");
  if (!feed) {
    return;
  }
  feed.innerHTML = `<div class="tone-sharing-status">Loading...</div>`;

  try {
    if (browseMode === "featured") {
      const home = await apiFetch<{ rows: ToneSharingRow[] }>("/home");
      renderFeedRows(home.rows);
      return;
    }

    if (browseMode === "items") {
      const items = await apiFetch<{ items: ToneSharingItem[] }>("/items?page=1&pageSize=36");
      renderFeedRows([buildSingleRow("Latest Presets", items.items, "item")]);
      return;
    }

    if (browseMode === "packs") {
      const packs = await apiFetch<{ packs: ToneSharingPack[] }>("/packs?page=1&pageSize=36");
      renderFeedRows([buildSingleRow("Latest Packs", packs.packs, "pack")]);
      return;
    }

    await loadMine();
  } catch (error) {
    feed.innerHTML = `<div class="tone-sharing-status">Load failed: ${(error as Error).message}</div>`;
  }
}

async function loadMine(): Promise<void> {
  const feed = element<HTMLElement>("tone-sharing-feed");
  if (!feed) {
    return;
  }

  if (!state.user) {
    if (browseMode === "mine") {
      feed.innerHTML = `<div class="tone-sharing-status">Sign in to view your content.</div>`;
    }
    return;
  }

  try {
    const [itemsData, packsData] = await Promise.all([
      apiFetch<{ items: ToneSharingItem[] }>("/items/me/list"),
      apiFetch<{ packs: ToneSharingPack[] }>("/packs/me/list")
    ]);

    state.myItems = itemsData.items;
    renderPackItemSelection(itemsData.items);

    if (browseMode === "mine") {
      renderFeedRows([
        buildSingleRow("My Presets", itemsData.items, "item"),
        buildSingleRow("My Packs", packsData.packs, "pack")
      ]);
    }
  } catch (error) {
    if (browseMode === "mine") {
      feed.innerHTML = `<div class="tone-sharing-status">Load failed: ${(error as Error).message}</div>`;
    }
  }
}

function renderPackItemSelection(items: ToneSharingItem[]): void {
  const host = element<HTMLElement>("tone-sharing-pack-items");
  if (!host) {
    return;
  }

  if (!items.length) {
    host.innerHTML = `<div class="tone-sharing-select-item">Publish an item first.</div>`;
    return;
  }

  host.innerHTML = items
    .map(
      (item) => `
        <label class="tone-sharing-select-item">
          <input type="checkbox" data-pack-item-id="${item.id}" />
          <span>${item.title}</span>
        </label>
      `
    )
    .join("");
}

async function uploadAndPublishItem(): Promise<void> {
  const title = element<HTMLInputElement>("tone-sharing-item-title")?.value.trim() ?? "";
  const type = (element<HTMLSelectElement>("tone-sharing-item-type")?.value ?? "preset") as ToneSharingItem["type"];
  const description = element<HTMLTextAreaElement>("tone-sharing-item-description")?.value.trim() ?? "";
  const file = element<HTMLInputElement>("tone-sharing-item-file")?.files?.[0] ?? null;

  if (!state.user) {
    setText("tone-sharing-upload-status", "Sign in first.");
    return;
  }
  if (!title || !file) {
    setText("tone-sharing-upload-status", "Title and file are required.");
    return;
  }

  setText("tone-sharing-upload-status", "Uploading...");

  try {
    const init = await apiFetch<{ uploadId: string }>("/uploads/init", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        kind: "item_payload",
        mimeType: file.type || "application/octet-stream",
        byteSize: file.size,
        turnstileToken: currentTurnstileToken()
      })
    });

    const uploadResponse = await fetch(`${state.apiBase}/uploads/${init.uploadId}`, {
      method: "PUT",
      headers: {
        "content-type": file.type || "application/octet-stream",
        ...(state.sessionId ? { "x-session-id": state.sessionId } : {})
      },
      body: file,
      credentials: "include"
    });

    const uploadPayload = await uploadResponse.json();
    if (!uploadResponse.ok || uploadPayload?.ok === false) {
      throw new Error(uploadPayload?.error?.message || `Upload failed (${uploadResponse.status})`);
    }

    const complete = await apiFetch<{ assetId: string }>("/uploads/complete", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ uploadId: init.uploadId })
    });

    const item = await apiFetch<{ item: ToneSharingItem }>("/items", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        type,
        title,
        description,
        payloadAssetId: complete.assetId
      })
    });

    await apiFetch(`/items/${item.item.id}/publish`, { method: "POST" });

    setText("tone-sharing-upload-status", "Published successfully.");
    await Promise.all([loadBrowse(), loadMine()]);
  } catch (error) {
    setText("tone-sharing-upload-status", `Publish failed: ${(error as Error).message}`);
  }
}

async function createAndPublishPack(): Promise<void> {
  if (!state.user) {
    setText("tone-sharing-pack-status", "Sign in first.");
    return;
  }

  const title = element<HTMLInputElement>("tone-sharing-pack-title")?.value.trim() ?? "";
  const description = element<HTMLTextAreaElement>("tone-sharing-pack-description")?.value.trim() ?? "";
  if (!title) {
    setText("tone-sharing-pack-status", "Pack title is required.");
    return;
  }

  const itemIds = Array.from(document.querySelectorAll<HTMLInputElement>("#tone-sharing-pack-items input[data-pack-item-id]:checked")).map(
    (input) => input.dataset.packItemId || ""
  );

  if (!itemIds.length) {
    setText("tone-sharing-pack-status", "Select at least one item.");
    return;
  }

  setText("tone-sharing-pack-status", "Publishing pack...");

  try {
    const pack = await apiFetch<{ pack: ToneSharingPack }>("/packs", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ title, description })
    });

    await apiFetch(`/packs/${pack.pack.id}/items`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ itemIds })
    });

    await apiFetch(`/packs/${pack.pack.id}/publish`, { method: "POST" });
    setText("tone-sharing-pack-status", "Pack published successfully.");
    await Promise.all([loadBrowse(), loadMine()]);
  } catch (error) {
    setText("tone-sharing-pack-status", `Pack publish failed: ${(error as Error).message}`);
  }
}

async function downloadAsset(kind: "item" | "pack", id: string): Promise<void> {
  const path = kind === "item" ? `/items/${id}/download` : `/packs/${id}/download`;
  const response = await fetch(`${state.apiBase}${path}`, {
    headers: state.sessionId ? { "x-session-id": state.sessionId } : {},
    credentials: "include"
  });

  if (!response.ok) {
    throw new Error(`Download failed (${response.status})`);
  }

  const blob = await response.blob();
  const disposition = response.headers.get("content-disposition") || "";
  const fileMatch = disposition.match(/filename=\"?([^\"]+)\"?/i);
  const fileName = fileMatch?.[1] || `${kind}-${id}`;

  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = fileName;
  document.body.appendChild(anchor);
  anchor.click();
  anchor.remove();
  URL.revokeObjectURL(url);
}

function bindBrowseActions(): void {
  const feed = element<HTMLElement>("tone-sharing-feed");
  if (!feed) {
    return;
  }

  feed.addEventListener("click", async (event) => {
    const target = event.target as HTMLElement;
    const button = target.closest("button[data-action]") as HTMLButtonElement | null;
    const card = target.closest(".tone-sharing-card-item") as HTMLElement | null;
    if (!button || !card) {
      return;
    }

    const kind = (card.dataset.kind ?? "item") as "item" | "pack";
    const id = card.dataset.id ?? "";
    if (!id) {
      return;
    }

    if (button.dataset.action === "open") {
      const detailsPath = kind === "item" ? `/items/${id}` : `/packs/${id}`;
      try {
        const details = await apiFetch(detailsPath);
        const title = kind === "item" ? (details as { item?: { title?: string } })?.item?.title : (details as { pack?: { title?: string } })?.pack?.title;
        setText("tone-sharing-upload-status", `Opened ${kind}: ${title ?? id}`);
      } catch (error) {
        setText("tone-sharing-upload-status", `Open failed: ${(error as Error).message}`);
      }
      return;
    }

    if (button.dataset.action === "download") {
      try {
        await downloadAsset(kind, id);
      } catch (error) {
        setText("tone-sharing-upload-status", `Download failed: ${(error as Error).message}`);
      }
    }
  });
}

function bindBrowseModeButtons(): void {
  const modes: Array<{ id: string; mode: typeof browseMode }> = [
    { id: "tone-sharing-browse-featured", mode: "featured" },
    { id: "tone-sharing-browse-items", mode: "items" },
    { id: "tone-sharing-browse-packs", mode: "packs" },
    { id: "tone-sharing-browse-mine", mode: "mine" }
  ];

  const setActive = () => {
    for (const entry of modes) {
      const button = element<HTMLButtonElement>(entry.id);
      if (button) {
        button.classList.toggle("active", entry.mode === browseMode);
      }
    }
  };

  for (const entry of modes) {
    const button = element<HTMLButtonElement>(entry.id);
    if (!button) {
      continue;
    }
    button.addEventListener("click", async () => {
      browseMode = entry.mode;
      setActive();
      if (browseMode === "mine") {
        await loadMine();
      } else {
        await loadBrowse();
      }
    });
  }

  setActive();
}

function restoreLocalState(): void {
  const storedBase = localStorage.getItem(storageKeys.apiBase);
  const storedSession = localStorage.getItem(storageKeys.sessionId);
  if (storedBase) {
    state.apiBase = normalizeBase(storedBase);
  }
  if (storedSession) {
    state.sessionId = storedSession;
  }

  const apiInput = element<HTMLInputElement>("tone-sharing-api-base");
  if (apiInput) {
    apiInput.value = state.apiBase;
  }
}

function bindTopControls(): void {
  const refreshButton = element<HTMLButtonElement>("tone-sharing-refresh");
  const apiInput = element<HTMLInputElement>("tone-sharing-api-base");
  if (refreshButton && apiInput) {
    refreshButton.addEventListener("click", async () => {
      state.apiBase = normalizeBase(apiInput.value);
      localStorage.setItem(storageKeys.apiBase, state.apiBase);
      await loadAuthSession();
      await Promise.all([loadBrowse(), loadMine()]);
    });
  }

  element<HTMLButtonElement>("tone-sharing-send-code")?.addEventListener("click", () => {
    void sendCode();
  });
  element<HTMLButtonElement>("tone-sharing-verify")?.addEventListener("click", () => {
    void verifyCode();
  });
  element<HTMLButtonElement>("tone-sharing-logout")?.addEventListener("click", () => {
    void signOut();
  });
  element<HTMLButtonElement>("tone-sharing-upload-item")?.addEventListener("click", () => {
    void uploadAndPublishItem();
  });
  element<HTMLButtonElement>("tone-sharing-create-pack")?.addEventListener("click", () => {
    void createAndPublishPack();
  });
}

export function initializeToneSharingPanel(): void {
  if (!element("panel-sharing")) {
    return;
  }

  restoreLocalState();
  bindTopControls();
  bindBrowseModeButtons();
  bindBrowseActions();

  void (async () => {
    await loadAuthSession();
    await Promise.all([loadBrowse(), loadMine()]);
  })();
}
