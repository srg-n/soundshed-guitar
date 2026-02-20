export type ResourceKind = "nam" | "ir";

export type GeneratorConfig = {
  runIdPrefix: string;
  outputRoot: string;
  seedFile: string;
  generation: {
    maxPresets: number;
    maxPairsPerNam: number;
    minPopularity: number;
    includeDelayAndReverbRate: number;
  };
  pack: {
    packId: string;
    packVersion: string;
    minimumAppVersion: string;
  };
  cache: {
    apiTtlSeconds: number;
    refresh: boolean;
  };
  tone3000: {
    enabled: boolean;
    apiBaseUrl: string;
    apiKeyEnvVar: string;
    limit: number;
    requestDelayMinMs: number;
    requestDelayMaxMs: number;
  };
};

export type ResourceCandidate = {
  id: string;
  kind: ResourceKind;
  name: string;
  category: string;
  tags: string[];
  popularity: number;
  externalId?: string;
  downloadUrl?: string;
  source: "seed" | "tone3000";
  sha256?: string;
  fileExt?: string;
};

export type Pairing = {
  nam: ResourceCandidate;
  ir: ResourceCandidate;
  score: number;
  reasons: string[];
};

export type GraphNode = {
  id: string;
  type: string;
  params?: Record<string, number>;
  resource?: {
    resourceType: ResourceKind;
    resourceId: string;
  };
};

export type GraphEdge = {
  from: string;
  to: string;
};

export type PresetV2 = {
  id: string;
  name: string;
  version: 2;
  author: string;
  category: string;
  tags: string[];
  description: string;
  global: {
    inputTrim: number;
    outputTrim: number;
  };
  graph: {
    nodes: GraphNode[];
    edges: GraphEdge[];
  };
};

export type ResourceIndexItem = {
  resourceId: string;
  resourceType: ResourceKind;
  provider: string;
  contentHash: string;
  fileExt: string;
  filePath: string;
  displayName: string;
  originalFileName: string;
};

export type ResourceIndex = {
  schemaVersion: number;
  updatedAt: string;
  items: ResourceIndexItem[];
};

export type RunManifest = {
  runId: string;
  createdAt: string;
  generatorVersion: string;
  presetCount: number;
  resourceCount: number;
  cache: {
    apiHits: number;
    apiMisses: number;
    resourceHits: number;
    resourceMisses: number;
  };
  presets: Array<{ id: string; name: string; hash: string }>;
  resources: Array<{ id: string; kind: ResourceKind; hash: string }>;
};

export type SeedFile = {
  resources: ResourceCandidate[];
};
