export type ItemVisibility = "public" | "unlisted" | "private";

export const allowedItemVisibilities = new Set<ItemVisibility>(["public", "unlisted", "private"]);

export type ItemConfig = {
  description: string | null;
  visibility: ItemVisibility;
  tags: string[] | null;
  appMinVersion: string | null;
  appMaxVersion: string | null;
  payloadAssetId: string | null;
  privatePayloadAssetId: string | null;
  manifestAssetId: string | null;
  thumbnailAssetId: string | null;
  previewAssetId: string | null;
};

export type PackConfig = {
  description: string | null;
  zipAssetId: string | null;
  thumbnailAssetId: string | null;
};

function defaultItemConfig(): ItemConfig {
  return {
    description: null,
    visibility: "public",
    tags: null,
    appMinVersion: null,
    appMaxVersion: null,
    payloadAssetId: null,
    privatePayloadAssetId: null,
    manifestAssetId: null,
    thumbnailAssetId: null,
    previewAssetId: null,
  };
}

function defaultPackConfig(): PackConfig {
  return {
    description: null,
    zipAssetId: null,
    thumbnailAssetId: null,
  };
}

function parseObjectJson(raw: string | null | undefined): Record<string, unknown> | null {
  if (!raw) {
    return null;
  }

  try {
    const parsed = JSON.parse(raw) as unknown;
    return parsed && typeof parsed === "object" && !Array.isArray(parsed)
      ? parsed as Record<string, unknown>
      : null;
  } catch {
    return null;
  }
}

function stringOrNull(value: unknown): string | null {
  return typeof value === "string" ? value : null;
}

function stringArrayOrNull(value: unknown): string[] | null {
  return Array.isArray(value) ? value.filter((item): item is string => typeof item === "string") : null;
}

export function parseItemConfig(configJson: string | null | undefined): ItemConfig {
  const defaults = defaultItemConfig();
  const parsed = parseObjectJson(configJson);
  if (!parsed) {
    return defaults;
  }

  const visibility = parsed.visibility;
  return {
    description: stringOrNull(parsed.description),
    visibility: typeof visibility === "string" && allowedItemVisibilities.has(visibility as ItemVisibility)
      ? visibility as ItemVisibility
      : defaults.visibility,
    tags: stringArrayOrNull(parsed.tags),
    appMinVersion: stringOrNull(parsed.appMinVersion),
    appMaxVersion: stringOrNull(parsed.appMaxVersion),
    payloadAssetId: stringOrNull(parsed.payloadAssetId),
    privatePayloadAssetId: stringOrNull(parsed.privatePayloadAssetId),
    manifestAssetId: stringOrNull(parsed.manifestAssetId),
    thumbnailAssetId: stringOrNull(parsed.thumbnailAssetId),
    previewAssetId: stringOrNull(parsed.previewAssetId),
  };
}

export function stringifyItemConfig(config: ItemConfig): string {
  return JSON.stringify(config);
}

export function parsePackConfig(configJson: string | null | undefined): PackConfig {
  const defaults = defaultPackConfig();
  const parsed = parseObjectJson(configJson);
  if (!parsed) {
    return defaults;
  }

  return {
    description: stringOrNull(parsed.description),
    zipAssetId: stringOrNull(parsed.zipAssetId),
    thumbnailAssetId: stringOrNull(parsed.thumbnailAssetId),
  };
}

export function stringifyPackConfig(config: PackConfig): string {
  return JSON.stringify(config);
}
