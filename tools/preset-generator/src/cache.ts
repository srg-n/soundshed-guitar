import { writeFile, readFile } from "node:fs/promises";
import path from "node:path";
import { ensureDir, fileExists, sha256Hex } from "./fs-utils.js";

export type CacheStats = {
  apiHits: number;
  apiMisses: number;
  resourceHits: number;
  resourceMisses: number;
};

type ApiCacheRecord<T> = {
  fetchedAtMs: number;
  ttlSeconds: number;
  payload: T;
};

type ResourceCacheIndex = {
  byUrl: Record<string, { hash: string; ext: string }>;
};

export async function cachedJsonFetch<T>(opts: {
  cacheRoot: string;
  key: string;
  ttlSeconds: number;
  refresh: boolean;
  fetcher: () => Promise<T>;
  stats: CacheStats;
}): Promise<T> {
  const keyHash = sha256Hex(opts.key);
  const cachePath = path.join(opts.cacheRoot, "api", `${keyHash}.json`);

  if (!opts.refresh && (await fileExists(cachePath))) {
    const record = JSON.parse(await readFile(cachePath, "utf8")) as ApiCacheRecord<T>;
    const ageMs = Date.now() - record.fetchedAtMs;
    if (ageMs <= record.ttlSeconds * 1000) {
      opts.stats.apiHits += 1;
      return record.payload;
    }
  }

  const payload = await opts.fetcher();
  const record: ApiCacheRecord<T> = {
    fetchedAtMs: Date.now(),
    ttlSeconds: opts.ttlSeconds,
    payload
  };
  await ensureDir(path.dirname(cachePath));
  await writeFile(cachePath, JSON.stringify(record, null, 2), "utf8");
  opts.stats.apiMisses += 1;
  return payload;
}

async function loadResourceIndex(cacheRoot: string): Promise<ResourceCacheIndex> {
  const indexPath = path.join(cacheRoot, "resources", "index.json");
  if (!(await fileExists(indexPath))) {
    return { byUrl: {} };
  }
  return JSON.parse(await readFile(indexPath, "utf8")) as ResourceCacheIndex;
}

async function saveResourceIndex(cacheRoot: string, index: ResourceCacheIndex): Promise<void> {
  const indexPath = path.join(cacheRoot, "resources", "index.json");
  await ensureDir(path.dirname(indexPath));
  await writeFile(indexPath, JSON.stringify(index, null, 2), "utf8");
}

export async function cachedResourceDownload(opts: {
  cacheRoot: string;
  url: string;
  fallbackExt: string;
  headers?: Record<string, string>;
  stats: CacheStats;
}): Promise<{ hash: string; ext: string; blobPath: string }> {
  const index = await loadResourceIndex(opts.cacheRoot);
  const cached = index.byUrl[opts.url];
  if (cached) {
    const existingPath = path.join(opts.cacheRoot, "resources", "blobs", `${cached.hash}.${cached.ext}`);
    if (await fileExists(existingPath)) {
      opts.stats.resourceHits += 1;
      return { hash: cached.hash, ext: cached.ext, blobPath: existingPath };
    }
  }

  const response = await fetch(opts.url, {
    headers: opts.headers
  });
  if (!response.ok) {
    throw new Error(`Failed resource download: ${response.status} ${response.statusText} (${opts.url})`);
  }
  const bytes = new Uint8Array(await response.arrayBuffer());
  const hash = sha256Hex(bytes);
  const ext = guessExt(opts.url, opts.fallbackExt);
  const blobPath = path.join(opts.cacheRoot, "resources", "blobs", `${hash}.${ext}`);

  if (!(await fileExists(blobPath))) {
    await ensureDir(path.dirname(blobPath));
    await writeFile(blobPath, bytes);
  }

  index.byUrl[opts.url] = { hash, ext };
  await saveResourceIndex(opts.cacheRoot, index);
  opts.stats.resourceMisses += 1;
  return { hash, ext, blobPath };
}

function guessExt(url: string, fallbackExt: string): string {
  const match = /\.([a-zA-Z0-9]+)(?:\?|$)/.exec(url);
  if (!match) {
    return fallbackExt;
  }
  return match[1].toLowerCase();
}
