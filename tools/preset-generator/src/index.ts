import { cp, mkdir, readdir, readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import JSZip from "jszip";
import { cachedResourceDownload, type CacheStats } from "./cache.js";
import { ensureDir, nowIso, readJsonFile, sha256Hex, slugify, writeJsonFile } from "./fs-utils.js";
import { buildPresetsFromPairings } from "./generator.js";
import { buildPairings } from "./pairing.js";
import { getTone3000AccessToken, loadTone3000Candidates } from "./tone3000.js";
import {
  type GeneratorConfig,
  type PresetV2,
  type ResourceCandidate,
  type ResourceIndex,
  type ResourceIndexItem,
  type RunManifest,
  type SeedFile
} from "./types.js";
import { validatePreset, validateResourceRefs } from "./validate.js";

const GENERATOR_VERSION = "0.1.0";

async function loadConfig(configPath: string): Promise<GeneratorConfig> {
  return readJsonFile<GeneratorConfig>(configPath);
}

function argValue(flag: string, argv: string[]): string | null {
  const idx = argv.indexOf(flag);
  if (idx < 0 || idx + 1 >= argv.length) {
    return null;
  }
  return argv[idx + 1];
}

function currentRunId(prefix: string): string {
  const stamp = new Date().toISOString().replace(/[-:TZ.]/g, "").slice(0, 14);
  return `${prefix}-${stamp}`;
}

async function readSeedCandidates(seedPath: string): Promise<ResourceCandidate[]> {
  const seed = await readJsonFile<SeedFile>(seedPath);
  return seed.resources;
}

function splitCandidates(all: ResourceCandidate[]): { nams: ResourceCandidate[]; irs: ResourceCandidate[] } {
  return {
    nams: all.filter((r) => r.kind === "nam"),
    irs: all.filter((r) => r.kind === "ir")
  };
}

async function materializeResourceCache(opts: {
  config: GeneratorConfig;
  cacheRoot: string;
  resources: ResourceCandidate[];
  stats: CacheStats;
}): Promise<Map<string, { hash: string; ext: string; blobPath: string }>> {
  const map = new Map<string, { hash: string; ext: string; blobPath: string }>();

  const needsTone3000Auth = opts.resources.some((resource) => resource.source === "tone3000" && !!resource.downloadUrl);
  const tone3000AccessToken = needsTone3000Auth ? await getTone3000AccessToken(opts.config) : null;

  for (const resource of opts.resources) {
    if (!resource.downloadUrl) {
      continue;
    }
    const fallbackExt = resource.kind === "nam" ? "nam" : "wav";
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
      console.warn(`Warning: resource download skipped for ${resource.id}: ${message}`);
    }
  }
  return map;
}

function buildResourceIndex(opts: {
  resources: ResourceCandidate[];
  resolved: Map<string, { hash: string; ext: string; blobPath: string }>;
}): ResourceIndex {
  const items: ResourceIndexItem[] = opts.resources.map((resource) => {
    const cached = opts.resolved.get(resource.id);
    const hash = cached?.hash ?? sha256Hex(resource.id);
    const ext = cached?.ext ?? (resource.kind === "nam" ? "nam" : "wav");
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

async function writeRunArtifacts(opts: {
  runRoot: string;
  presets: PresetV2[];
  index: ResourceIndex;
  resources: ResourceCandidate[];
  manifest: RunManifest;
  resolved: Map<string, { hash: string; ext: string; blobPath: string }>;
}): Promise<void> {
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

async function generate(configPath: string): Promise<void> {
  const config = await loadConfig(configPath);
  const runId = currentRunId(config.runIdPrefix);
  const runRoot = path.resolve(path.dirname(configPath), "..", config.outputRoot, "runs", runId);
  const cacheRoot = path.resolve(path.dirname(configPath), "..", config.outputRoot, "cache");

  const stats: CacheStats = {
    apiHits: 0,
    apiMisses: 0,
    resourceHits: 0,
    resourceMisses: 0
  };

  const seedPath = path.resolve(path.dirname(configPath), "..", config.seedFile);
  const seedCandidates = await readSeedCandidates(seedPath);
  const toneCandidates = await loadTone3000Candidates({ config, stats, cacheRoot });

  const combined = [...seedCandidates, ...toneCandidates.nams, ...toneCandidates.irs];
  const byId = new Map<string, ResourceCandidate>();
  for (const resource of combined) {
    byId.set(resource.id, resource);
  }

  const split = splitCandidates(Array.from(byId.values()));
  const pairings = buildPairings({
    nams: split.nams,
    irs: split.irs,
    minPopularity: config.generation.minPopularity,
    maxPairsPerNam: config.generation.maxPairsPerNam
  });

  const presets = buildPresetsFromPairings({
    pairings,
    maxPresets: config.generation.maxPresets,
    includeDelayAndReverbRate: config.generation.includeDelayAndReverbRate
  });

  const selectedResourceIds = new Set<string>();
  for (const preset of presets) {
    for (const node of preset.graph.nodes) {
      if (node.resource) {
        selectedResourceIds.add(node.resource.resourceId);
      }
    }
  }
  const selectedResources = Array.from(selectedResourceIds)
    .map((id) => byId.get(id))
    .filter((v): v is ResourceCandidate => v !== undefined);

  const resolved = await materializeResourceCache({
    config,
    cacheRoot,
    resources: selectedResources,
    stats
  });

  const index = buildResourceIndex({ resources: selectedResources, resolved });

  const allErrors: string[] = [];
  for (const preset of presets) {
    const schemaErrors = validatePreset(preset);
    const refErrors = validateResourceRefs(preset, index);
    allErrors.push(...schemaErrors.map((e) => `${preset.id}: ${e}`));
    allErrors.push(...refErrors.map((e) => `${preset.id}: ${e}`));
  }
  if (allErrors.length > 0) {
    throw new Error(`Validation failed with ${allErrors.length} issue(s):\n${allErrors.join("\n")}`);
  }

  const manifest: RunManifest = {
    runId,
    createdAt: nowIso(),
    generatorVersion: GENERATOR_VERSION,
    presetCount: presets.length,
    resourceCount: selectedResources.length,
    cache: stats,
    presets: presets.map((preset) => ({
      id: preset.id,
      name: preset.name,
      hash: sha256Hex(JSON.stringify(preset))
    })),
    resources: selectedResources.map((resource) => {
      const cached = resolved.get(resource.id);
      return {
        id: resource.id,
        kind: resource.kind,
        hash: cached?.hash ?? sha256Hex(resource.id)
      };
    })
  };

  await writeRunArtifacts({
    runRoot,
    presets,
    index,
    resources: selectedResources,
    manifest,
    resolved
  });

  await writeJsonFile(path.join(runRoot, "pack-manifest.json"), {
    formatVersion: 1,
    packId: config.pack.packId,
    packVersion: config.pack.packVersion,
    minimumAppVersion: config.pack.minimumAppVersion,
    createdAt: nowIso(),
    items: {
      presets: manifest.presets,
      resources: manifest.resources
    }
  });

  console.log(`Generated ${presets.length} presets in ${runRoot}`);
  console.log(`API cache hits/misses: ${stats.apiHits}/${stats.apiMisses}`);
  console.log(`Resource cache hits/misses: ${stats.resourceHits}/${stats.resourceMisses}`);
}

async function validate(configPath: string): Promise<void> {
  const config = await loadConfig(configPath);
  const runsDir = path.resolve(path.dirname(configPath), "..", config.outputRoot, "runs");
  const runIds = await readdir(runsDir);
  if (runIds.length === 0) {
    throw new Error(`No run directories found under ${runsDir}`);
  }
  runIds.sort();
  const latest = runIds[runIds.length - 1];
  const runRoot = path.join(runsDir, latest);

  const presetDir = path.join(runRoot, "presets");
  const presetFiles = (await readdir(presetDir)).filter((f) => f.endsWith(".json"));
  const index = await readJsonFile<ResourceIndex>(path.join(runRoot, "resources", "indexes", "resources-index.json"));

  const errors: string[] = [];
  for (const file of presetFiles) {
    const preset = await readJsonFile<PresetV2>(path.join(presetDir, file));
    errors.push(...validatePreset(preset).map((e) => `${file}: ${e}`));
    errors.push(...validateResourceRefs(preset, index).map((e) => `${file}: ${e}`));
  }

  if (errors.length > 0) {
    throw new Error(`Validation failed with ${errors.length} issue(s):\n${errors.join("\n")}`);
  }

  console.log(`Validated ${presetFiles.length} preset(s) from ${latest}`);
}

async function pack(configPath: string): Promise<void> {
  const config = await loadConfig(configPath);
  const runsDir = path.resolve(path.dirname(configPath), "..", config.outputRoot, "runs");
  const runIds = await readdir(runsDir);
  if (runIds.length === 0) {
    throw new Error(`No run directories found under ${runsDir}`);
  }
  runIds.sort();
  const latest = runIds[runIds.length - 1];
  const runRoot = path.join(runsDir, latest);

  const zip = new JSZip();

  async function addDirRecursive(relativePath: string): Promise<void> {
    const absolute = path.join(runRoot, relativePath);
    const entries = await readdir(absolute, { withFileTypes: true });
    for (const entry of entries) {
      const childRelative = path.posix.join(relativePath.replace(/\\/g, "/"), entry.name);
      const childAbsolute = path.join(absolute, entry.name);
      if (entry.isDirectory()) {
        await addDirRecursive(childRelative);
      } else {
        const content = await readFile(childAbsolute);
        zip.file(childRelative, content);
      }
    }
  }

  await addDirRecursive("presets");
  await addDirRecursive(path.join("resources", "indexes"));
  await addDirRecursive(path.join("resources", "content"));
  zip.file("manifest.run.json", await readFile(path.join(runRoot, "manifest.run.json")));
  zip.file("pack-manifest.json", await readFile(path.join(runRoot, "pack-manifest.json")));

  const zipBytes = await zip.generateAsync({ type: "nodebuffer", compression: "DEFLATE" });
  const packName = `${config.pack.packId}-${config.pack.packVersion}-${latest}.zip`;
  const outputPath = path.join(runRoot, packName);
  await mkdir(path.dirname(outputPath), { recursive: true });
  await writeFile(outputPath, zipBytes);

  console.log(`Packed run ${latest} to ${outputPath}`);
}

async function probe(configPath: string): Promise<void> {
  const config = await loadConfig(configPath);
  const apiKey = process.env[config.tone3000.apiKeyEnvVar];
  if (!apiKey) {
    throw new Error(`Missing API key environment variable: ${config.tone3000.apiKeyEnvVar}`);
  }

  const apiBaseUrl = config.tone3000.apiBaseUrl.endsWith("/")
    ? config.tone3000.apiBaseUrl
    : `${config.tone3000.apiBaseUrl}/`;

  const sessionUrl = new URL("auth/session", apiBaseUrl);
  const sessionResponse = await fetch(sessionUrl, {
    method: "POST",
    headers: {
      "Content-Type": "application/json"
    },
    body: JSON.stringify({ api_key: apiKey })
  });

  const sessionText = await sessionResponse.text();
  const sessionSnippet = sessionText.slice(0, 300).replace(/\s+/g, " ").trim();
  console.log(`[probe] session status=${sessionResponse.status} url=${sessionUrl.toString()}`);
  console.log(`[probe] session body=${sessionSnippet}`);

  if (!sessionResponse.ok) {
    return;
  }

  const sessionData = JSON.parse(sessionText) as { access_token?: string };
  if (!sessionData.access_token) {
    console.log("[probe] session response missing access_token");
    return;
  }

  // Use gear param for probing, matching tone3000.ts discovery logic.
  // IR uses gear=ir; NAM omits the gear param (covers amp/preamp/pedal/full-rig).
  const targets = [
    { label: "models", gear: null },
    { label: "irs", gear: "ir" }
  ];

  for (const target of targets) {
    const url = new URL("tones/search", apiBaseUrl);
    url.searchParams.set("page", "1");
    url.searchParams.set("page_size", "1");
    url.searchParams.set("sort", "downloads-all-time");
    if (target.gear) {
      url.searchParams.set("gear", target.gear);
    }

    const response = await fetch(url, {
      headers: {
        Authorization: `Bearer ${sessionData.access_token}`
      }
    });
    const text = await response.text();
    const snippet = text.slice(0, 300).replace(/\s+/g, " ").trim();
    console.log(`[probe] ${target.label} status=${response.status} url=${url.toString()}`);
    console.log(`[probe] ${target.label} body=${snippet}`);
  }
}

async function main(): Promise<void> {
  const [, , command, ...argv] = process.argv;
  const configPath = argValue("--config", argv) ?? "config/default.json";
  const resolvedConfigPath = path.resolve(process.cwd(), configPath);

  if (command === "generate") {
    await generate(resolvedConfigPath);
    return;
  }
  if (command === "validate") {
    await validate(resolvedConfigPath);
    return;
  }
  if (command === "pack") {
    await pack(resolvedConfigPath);
    return;
  }
  if (command === "probe") {
    await probe(resolvedConfigPath);
    return;
  }

  throw new Error("Usage: tsx src/index.ts <generate|validate|pack|probe> --config <path>");
}

main().catch((error) => {
  const message = error instanceof Error ? error.message : String(error);
  console.error(message);
  process.exitCode = 1;
});
