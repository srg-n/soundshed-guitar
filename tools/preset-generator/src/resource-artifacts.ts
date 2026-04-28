import { cp } from "node:fs/promises";
import path from "node:path";
import { cachedResourceDownload, type CacheStats } from "./cache.js";
import { ensureDir, nowIso, sha256Hex, slugify, writeJsonFile } from "./fs-utils.js";
import { getTone3000AccessToken } from "./tone3000.js";
import type { GeneratorConfig, PresetV2, ResourceCandidate, ResourceIndex, ResourceIndexItem, RunManifest } from "./types.js";

export type ResolvedResource = {
  hash: string;
  ext: string;
  blobPath: string;
};

function fallbackResourceExt(resource: ResourceCandidate): string {
  return resource.fileExt ?? (resource.kind === "nam" ? "nam" : "wav");
}

export function assertDownloadableResourcesResolved(
  resources: ResourceCandidate[],
  resolved: Map<string, ResolvedResource>
): void {
  const missing = resources.filter((resource) => resource.downloadUrl && !resolved.has(resource.id));
  if (missing.length === 0) {
    return;
  }

  throw new Error(
    [
      `Resource download failed for ${missing.length} selected resource(s).`,
      ...missing.map((resource) => `- ${resource.id}: ${resource.downloadUrl}`),
    ].join("\n")
  );
}

export async function materializeResourceCache(opts: {
  config: GeneratorConfig;
  cacheRoot: string;
  resources: ResourceCandidate[];
  stats: CacheStats;
}): Promise<Map<string, ResolvedResource>> {
  const map = new Map<string, ResolvedResource>();

  const needsTone3000Auth = opts.resources.some((resource) => resource.source === "tone3000" && !!resource.downloadUrl);
  const tone3000AccessToken = needsTone3000Auth ? await getTone3000AccessToken(opts.config) : null;
  const failures: string[] = [];

  for (const resource of opts.resources) {
    if (!resource.downloadUrl) {
      continue;
    }
    const fallbackExt = fallbackResourceExt(resource);
    const headers = resource.source === "tone3000" && tone3000AccessToken
      ? { Authorization: `Bearer ${tone3000AccessToken}` }
      : undefined;
    try {
      const resolved = await cachedResourceDownload({
        cacheRoot: opts.cacheRoot,
        url: resource.downloadUrl,
        fallbackExt,
        headers,
        stats: opts.stats
      });
      map.set(resource.id, resolved);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${resource.id}: ${message}`);
    }
  }

  if (failures.length > 0) {
    throw new Error(`Resource download failed for ${failures.length} selected resource(s):\n${failures.join("\n")}`);
  }

  assertDownloadableResourcesResolved(opts.resources, map);
  return map;
}

export function buildResourceIndex(opts: {
  resources: ResourceCandidate[];
  resolved: Map<string, ResolvedResource>;
}): ResourceIndex {
  assertDownloadableResourcesResolved(opts.resources, opts.resolved);

  const items: ResourceIndexItem[] = opts.resources.map((resource) => {
    const cached = opts.resolved.get(resource.id);
    const hash = cached?.hash ?? resource.sha256 ?? sha256Hex(resource.id);
    const ext = cached?.ext ?? fallbackResourceExt(resource);
    const fileName = `${hash}.${ext}`;
    return {
      resourceId: resource.id,
      resourceType: resource.kind,
      provider: resource.source,
      contentHash: hash,
      fileExt: ext,
      filePath: `content/${resource.source}/${fileName}`,
      displayName: resource.name,
      originalFileName: `${slugify(resource.name)}.${ext}`
    };
  });

  return {
    schemaVersion: 1,
    updatedAt: nowIso(),
    items
  };
}

export async function writeRunArtifacts(opts: {
  runRoot: string;
  presets: PresetV2[];
  index: ResourceIndex;
  resources: ResourceCandidate[];
  manifest: RunManifest;
  resolved: Map<string, ResolvedResource>;
}): Promise<void> {
  assertDownloadableResourcesResolved(opts.resources, opts.resolved);

  const presetsDir = path.join(opts.runRoot, "presets");
  const indexPath = path.join(opts.runRoot, "resources", "indexes", "resources-index.json");
  const contentDir = path.join(opts.runRoot, "resources", "content");

  await ensureDir(presetsDir);
  await ensureDir(path.dirname(indexPath));
  await ensureDir(contentDir);

  for (const preset of opts.presets) {
    const filePath = path.join(presetsDir, `${preset.id}.json`);
    await writeJsonFile(filePath, preset);
  }

  for (const resource of opts.resources) {
    const cached = opts.resolved.get(resource.id);
    if (!cached) {
      continue;
    }
    const destination = path.join(contentDir, resource.source, `${cached.hash}.${cached.ext}`);
    await ensureDir(path.dirname(destination));
    await cp(cached.blobPath, destination);
  }

  await writeJsonFile(indexPath, opts.index);
  await writeJsonFile(path.join(opts.runRoot, "manifest.run.json"), opts.manifest);
}
