import { PresetV2, ResourceIndex } from "./types.js";

export function validatePreset(preset: PresetV2): string[] {
  const errors: string[] = [];

  if (!preset.id) {
    errors.push("missing id");
  }
  if (!preset.name) {
    errors.push("missing name");
  }
  if (preset.version !== 2) {
    errors.push("version must be 2");
  }

  const nodeIds = new Set(preset.graph.nodes.map((n) => n.id));
  if (!nodeIds.has("in") || !nodeIds.has("out")) {
    errors.push("graph must include in/out nodes");
  }

  for (const edge of preset.graph.edges) {
    if (!nodeIds.has(edge.from) || !nodeIds.has(edge.to)) {
      errors.push(`edge references unknown node: ${edge.from} -> ${edge.to}`);
    }
  }

  for (const node of preset.graph.nodes) {
    if (node.type === "amp_nam" && (!node.resource || node.resource.resourceType !== "nam")) {
      errors.push(`amp node ${node.id} missing NAM resource`);
    }
    if (node.type === "cab_ir" && (!node.resource || node.resource.resourceType !== "ir")) {
      errors.push(`cab node ${node.id} missing IR resource`);
    }
  }

  return errors;
}

export function validateResourceRefs(preset: PresetV2, index: ResourceIndex): string[] {
  const errors: string[] = [];
  const ids = new Set(index.items.map((i) => `${i.resourceType}:${i.resourceId}`));
  for (const node of preset.graph.nodes) {
    if (!node.resource) {
      continue;
    }
    const key = `${node.resource.resourceType}:${node.resource.resourceId}`;
    if (!ids.has(key)) {
      errors.push(`missing resource index entry for ${key}`);
    }
  }
  return errors;
}
