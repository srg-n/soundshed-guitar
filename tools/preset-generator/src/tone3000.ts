import { cachedJsonFetch } from "./cache.js";
import { slugify } from "./fs-utils.js";
import { GeneratorConfig, ResourceCandidate } from "./types.js";
import type { CacheStats } from "./cache.js";

type Tone3000ListResponse = {
  items?: unknown[];
  data?: unknown[];
  tones?: unknown[];
  results?: unknown[];
};

type Tone3000SessionResponse = {
  access_token?: string;
  refresh_token?: string;
  expires_in?: number;
  token_type?: string;
  scope?: string;
};

type Tone3000ModelsResponse = {
  models?: unknown[];
  data?: unknown[];
  results?: unknown[];
};

type Tone3000ToneSummary = {
  id: string;
  name: string;
  category: string;
  tags: string[];
  popularity: number;
};

type Tone3000ModelSummary = {
  id: string;
  name: string;
  modelUrl: string;
};

// Keep generator endpoints aligned with core/ui/ts/tone3000.ts and tone3000Browser.ts
const TONE3000_SESSION_PATH = "auth/session";

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function randomDelayMs(minMs: number, maxMs: number): number {
  return Math.floor(minMs + Math.random() * (maxMs - minMs));
}
const TONE3000_SEARCH_PATH = "tones/search";
const TONE3000_MODELS_PATH = "models";

function baseUrlWithTrailingSlash(rawBaseUrl: string): string {
  return rawBaseUrl.endsWith("/") ? rawBaseUrl : `${rawBaseUrl}/`;
}

let cachedSession: { accessToken: string; expiresAtMs: number } | null = null;
let inFlightSessionRequest: Promise<string> | null = null;
let useApiKeyBearerFallback = false;

function normalizeCandidate(input: unknown, kind: "nam" | "ir"): ResourceCandidate | null {
  if (!input || typeof input !== "object") {
    return null;
  }
  const row = input as Record<string, unknown>;
  // Tone3000 API uses "title" as the primary display field.
  const name = typeof row.title === "string" ? row.title : typeof row.name === "string" ? row.name : null;
  if (!name) {
    return null;
  }
  const externalId = typeof row.id === "string" ? row.id : typeof row.slug === "string" ? row.slug : undefined;
  const category = typeof row.category === "string"
    ? row.category
    : typeof row.gear === "string"
      ? row.gear
      : "general";

  const downloadsCount = typeof row.downloads_count === "number" ? row.downloads_count : 0;
  const popularityRaw = typeof row.popularity === "number"
    ? row.popularity
    : typeof row.rating === "number"
      ? row.rating
      : downloadsCount;
  const popularity = popularityRaw > 1
    ? Math.max(0, Math.min(1, Math.log10(popularityRaw + 1) / 5))
    : Math.max(0, Math.min(1, popularityRaw));

  const downloadUrl = typeof row.downloadUrl === "string"
    ? row.downloadUrl
    : typeof row.url === "string"
      ? row.url
      : undefined;

  const tags = Array.isArray(row.tags)
    ? row.tags
      .map((v) => {
        if (typeof v === "string") {
          return v;
        }
        if (v && typeof v === "object" && typeof (v as { name?: unknown }).name === "string") {
          return (v as { name: string }).name;
        }
        return null;
      })
      .filter((v): v is string => typeof v === "string")
    : [];
  const id = `${kind}:${slugify(externalId ?? name)}`;

  return {
    id,
    kind,
    name,
    category,
    tags,
    popularity,
    externalId,
    downloadUrl,
    source: "tone3000"
  };
}

function normalizeToneSummary(input: unknown): Tone3000ToneSummary | null {
  if (!input || typeof input !== "object") {
    return null;
  }

  const row = input as Record<string, unknown>;
  const idRaw = row.id;
  const id = typeof idRaw === "string" || typeof idRaw === "number" ? String(idRaw) : null;
  if (!id) {
    return null;
  }

  // Tone3000 API uses "title" as the primary display field (name is optional/absent).
  const name = typeof row.title === "string"
    ? row.title
    : typeof row.name === "string"
      ? row.name
      : `tone-${id}`;

  const category = typeof row.category === "string"
    ? row.category
    : typeof row.gear === "string"
      ? row.gear
      : "general";

  const downloadsCount = typeof row.downloads_count === "number" ? row.downloads_count : 0;
  const popularityRaw = typeof row.popularity === "number"
    ? row.popularity
    : typeof row.rating === "number"
      ? row.rating
      : downloadsCount;
  const popularity = popularityRaw > 1
    ? Math.max(0, Math.min(1, Math.log10(popularityRaw + 1) / 5))
    : Math.max(0, Math.min(1, popularityRaw));

  const tags = Array.isArray(row.tags)
    ? row.tags
      .map((v) => {
        if (typeof v === "string") {
          return v;
        }
        if (v && typeof v === "object" && typeof (v as { name?: unknown }).name === "string") {
          return (v as { name: string }).name;
        }
        return null;
      })
      .filter((v): v is string => typeof v === "string")
    : [];

  return {
    id,
    name,
    category,
    tags,
    popularity
  };
}

function normalizeModelSummary(input: unknown): Tone3000ModelSummary | null {
  if (!input || typeof input !== "object") {
    return null;
  }
  const row = input as Record<string, unknown>;
  const idRaw = row.id;
  const id = typeof idRaw === "string" || typeof idRaw === "number" ? String(idRaw) : null;
  const modelUrl = typeof row.model_url === "string"
    ? row.model_url
    : typeof row.url === "string"
      ? row.url
      : null;
  if (!id || !modelUrl) {
    return null;
  }

  const name = typeof row.name === "string" ? row.name : `model-${id}`;
  return { id, name, modelUrl };
}

async function fetchToneModels(opts: {
  config: GeneratorConfig;
  stats: CacheStats;
  cacheRoot: string;
  accessToken: string;
  toneId: string;
}): Promise<Tone3000ModelSummary[]> {
  const url = new URL(TONE3000_MODELS_PATH, baseUrlWithTrailingSlash(opts.config.tone3000.apiBaseUrl));
  url.searchParams.set("tone_id", opts.toneId);
  url.searchParams.set("page", "1");
  url.searchParams.set("page_size", "100");

  const key = `GET:${url.toString()}:auth:${opts.config.tone3000.apiKeyEnvVar}`;
  const payload = await cachedJsonFetch<Tone3000ModelsResponse>({
    cacheRoot: opts.cacheRoot,
    key,
    ttlSeconds: opts.config.cache.apiTtlSeconds,
    refresh: opts.config.cache.refresh,
    stats: opts.stats,
    fetcher: async () => {
      const response = await fetch(url, {
        headers: {
          Authorization: `Bearer ${opts.accessToken}`
        }
      });
      if (!response.ok) {
        const body = await response.text();
        const snippet = body.slice(0, 400).replace(/\s+/g, " ").trim();
        throw new Error(
          `Tone3000 models request failed: ${response.status} ${response.statusText} url=${url.toString()} body=${snippet}`
        );
      }
      return (await response.json()) as Tone3000ModelsResponse;
    }
  });

  const rows = Array.isArray(payload.models)
    ? payload.models
    : Array.isArray(payload.data)
      ? payload.data
      : Array.isArray(payload.results)
        ? payload.results
        : [];

  return rows
    .map((row) => normalizeModelSummary(row))
    .filter((model): model is Tone3000ModelSummary => model !== null);
}

async function getAccessToken(config: GeneratorConfig, apiKey: string): Promise<string> {
  if (useApiKeyBearerFallback) {
    return apiKey;
  }

  if (cachedSession && Date.now() < cachedSession.expiresAtMs - 30_000) {
    return cachedSession.accessToken;
  }

  if (inFlightSessionRequest) {
    return inFlightSessionRequest;
  }

  inFlightSessionRequest = (async () => {
    try {
      const sessionUrl = new URL(TONE3000_SESSION_PATH, baseUrlWithTrailingSlash(config.tone3000.apiBaseUrl));
      const response = await fetch(sessionUrl, {
        method: "POST",
        headers: {
          "Content-Type": "application/json"
        },
        body: JSON.stringify({ api_key: apiKey })
      });

      if (!response.ok) {
        const contentType = response.headers.get("content-type") ?? "";
        const body = await response.text();
        const snippet = body.slice(0, 400).replace(/\s+/g, " ").trim();

        const looksLikeCheckpoint =
          response.status === 403 && (
            contentType.toLowerCase().includes("text/html") ||
            snippet.includes("Vercel Security Checkpoint") ||
            snippet.startsWith("<!DOCTYPE html")
          );

        if (looksLikeCheckpoint) {
          // Browser challenge blocks Node session bootstrap; use API key bearer fallback.
          useApiKeyBearerFallback = true;
          console.warn("Warning: Tone3000 session bootstrap blocked by Vercel checkpoint. Falling back to direct API key bearer auth.");
          return apiKey;
        }

        throw new Error(
          `Tone3000 session request failed: ${response.status} ${response.statusText} url=${sessionUrl.toString()} body=${snippet}`
        );
      }

      const data = (await response.json()) as Tone3000SessionResponse;
      if (!data.access_token || typeof data.expires_in !== "number") {
        throw new Error("Tone3000 session response missing access_token or expires_in");
      }

      cachedSession = {
        accessToken: data.access_token,
        expiresAtMs: Date.now() + data.expires_in * 1000
      };

      return data.access_token;
    } finally {
      inFlightSessionRequest = null;
    }
  })();

  return inFlightSessionRequest;
}

export async function getTone3000AccessToken(config: GeneratorConfig): Promise<string> {
  const apiKey = process.env[config.tone3000.apiKeyEnvVar];
  if (!apiKey) {
    throw new Error(`Missing API key environment variable: ${config.tone3000.apiKeyEnvVar}`);
  }
  return getAccessToken(config, apiKey);
}

async function fetchList(opts: {
  config: GeneratorConfig;
  stats: CacheStats;
  kind: "nam" | "ir";
  cacheRoot: string;
}): Promise<ResourceCandidate[]> {
  const apiKey = process.env[opts.config.tone3000.apiKeyEnvVar];
  if (!apiKey) {
    throw new Error(`Missing API key environment variable: ${opts.config.tone3000.apiKeyEnvVar}`);
  }

  const accessToken = await getAccessToken(opts.config, apiKey);

  const url = new URL(TONE3000_SEARCH_PATH, baseUrlWithTrailingSlash(opts.config.tone3000.apiBaseUrl));
  url.searchParams.set("page", "1");
  url.searchParams.set("page_size", String(opts.config.tone3000.limit));
  url.searchParams.set("sort", "downloads-all-time");
  // Use gear param as the browser does; "ir" maps directly, NAM covers all non-IR gear types.
  // Do not send platform as a query param - the browser only uses that field for client-side filtering.
  if (opts.kind === "ir") {
    url.searchParams.set("gear", "ir");
  }

  const key = `GET:${url.toString()}:auth:${opts.config.tone3000.apiKeyEnvVar}`;
  const payload = await cachedJsonFetch<Tone3000ListResponse>({
    cacheRoot: opts.cacheRoot,
    key,
    ttlSeconds: opts.config.cache.apiTtlSeconds,
    refresh: opts.config.cache.refresh,
    stats: opts.stats,
    fetcher: async () => {
      const response = await fetch(url, {
        headers: {
          Authorization: `Bearer ${accessToken}`
        }
      });
      if (!response.ok) {
        const body = await response.text();
        const snippet = body.slice(0, 400).replace(/\s+/g, " ").trim();
        throw new Error(
          `Tone3000 API request failed: ${response.status} ${response.statusText} url=${url.toString()} body=${snippet}`
        );
      }
      return (await response.json()) as Tone3000ListResponse;
    }
  });

  // Match browser key priority: tones -> results -> items -> data
  const toneRows = Array.isArray(payload.tones)
    ? payload.tones
    : Array.isArray(payload.results)
      ? payload.results
      : Array.isArray(payload.items)
        ? payload.items
        : Array.isArray(payload.data)
          ? payload.data
          : [];

  // Filter client-side by platform to separate NAM and IR results, mirroring the
  // browser's post-fetch filtering in tone3000Browser.ts.
  const filteredRows = opts.kind === "ir"
    ? toneRows
    : toneRows.filter((row) => {
        if (!row || typeof row !== "object") return true;
        const r = row as Record<string, unknown>;
        const platform = typeof r.platform === "string" ? r.platform.toLowerCase() : "";
        const gear = typeof r.gear === "string" ? r.gear.toLowerCase() : "";
        return platform !== "ir" && gear !== "ir";
      });

  const tones = filteredRows
    .map((row) => normalizeToneSummary(row))
    .filter((tone): tone is Tone3000ToneSummary => tone !== null);

  const candidates: ResourceCandidate[] = [];
  for (const tone of tones) {
    let models: Tone3000ModelSummary[] = [];
    try {
      const missCountBefore = opts.stats.apiMisses;
      models = await fetchToneModels({
        config: opts.config,
        stats: opts.stats,
        cacheRoot: opts.cacheRoot,
        accessToken,
        toneId: tone.id
      });
      // Only delay when a real network request was made (cache miss), to avoid rate limiting.
      if (opts.stats.apiMisses > missCountBefore) {
        const delayMs = randomDelayMs(
          opts.config.tone3000.requestDelayMinMs,
          opts.config.tone3000.requestDelayMaxMs
        );
        await sleep(delayMs);
      }
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      // Continue generation even if one tone's model list fails.
      console.warn(`Warning: tone model discovery failed for tone ${tone.id}: ${message}`);
      continue;
    }

    for (const model of models) {
      candidates.push({
        id: `${opts.kind}:${slugify(`${tone.id}-${model.id}`)}`,
        kind: opts.kind,
        name: `${tone.name} - ${model.name}`,
        category: tone.category,
        tags: tone.tags,
        popularity: tone.popularity,
        externalId: model.id,
        downloadUrl: model.modelUrl,
        source: "tone3000"
      });
    }
  }

  return candidates;
}

export async function loadTone3000Candidates(opts: {
  config: GeneratorConfig;
  stats: CacheStats;
  cacheRoot: string;
}): Promise<{ nams: ResourceCandidate[]; irs: ResourceCandidate[] }> {
  if (!opts.config.tone3000.enabled) {
    return { nams: [], irs: [] };
  }

  const [nams, irs] = await Promise.all([
    fetchList({
      config: opts.config,
      stats: opts.stats,
      kind: "nam",
      cacheRoot: opts.cacheRoot
    }),
    fetchList({
      config: opts.config,
      stats: opts.stats,
      kind: "ir",
      cacheRoot: opts.cacheRoot
    })
  ]);

  return { nams, irs };
}
