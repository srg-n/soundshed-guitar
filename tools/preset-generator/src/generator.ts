import { randomUUID } from "node:crypto";
import { PresetV2, Pairing } from "./types.js";

function bounded(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, value));
}

function pickUseCase(index: number): string {
  const useCases = ["Rhythm", "Lead", "Crunch", "Ambient", "Edge"];
  return useCases[index % useCases.length];
}

function titleFromPair(pair: Pairing, idx: number): string {
  const genre = pair.nam.category || "Modern";
  const useCase = pickUseCase(idx);
  return `${genre} ${pair.nam.name} ${useCase}`.replace(/\s+/g, " ").trim();
}

export function buildPresetsFromPairings(opts: {
  pairings: Pairing[];
  maxPresets: number;
  includeDelayAndReverbRate: number;
}): PresetV2[] {
  const chosen = opts.pairings.slice(0, opts.maxPresets);
  return chosen.map((pair, idx) => {
    const includePostFx = (idx % 10) / 10 < opts.includeDelayAndReverbRate;

    const nodes: PresetV2["graph"]["nodes"] = [
      { id: "in", type: "input" },
      {
        id: "gate",
        type: "dynamics_gate",
        params: {
          thresholdDb: bounded(-58 + idx % 6, -80, 0),
          attackMs: 1,
          releaseMs: 50
        }
      },
      {
        id: "amp",
        type: "amp_nam",
        resource: { resourceType: "nam", resourceId: pair.nam.id },
        params: {
          inputGain: bounded(-2 + (idx % 8), -24, 24),
          outputGain: 0
        }
      },
      {
        id: "cab",
        type: "cab_ir",
        resource: { resourceType: "ir", resourceId: pair.ir.id },
        params: {
          mix: 1,
          outputGain: 0,
          quality: 1
        }
      }
    ];

    const edges: PresetV2["graph"]["edges"] = [
      { from: "in", to: "gate" },
      { from: "gate", to: "amp" },
      { from: "amp", to: "cab" }
    ];

    if (includePostFx) {
      nodes.push({
        id: "delay",
        type: "delay_digital",
        params: {
          timeMs: 280 + (idx % 5) * 60,
          feedback: 0.24,
          mix: 0.16
        }
      });
      nodes.push({
        id: "verb",
        type: "reverb_room",
        params: {
          decay: 0.52,
          size: 0.45,
          damping: 0.5,
          preDelay: 10,
          mix: 0.18
        }
      });
      edges.push({ from: "cab", to: "delay" });
      edges.push({ from: "delay", to: "verb" });
      nodes.push({ id: "out", type: "output" });
      edges.push({ from: "verb", to: "out" });
    } else {
      nodes.push({ id: "out", type: "output" });
      edges.push({ from: "cab", to: "out" });
    }

    return {
      id: `gen-${randomUUID()}`,
      name: titleFromPair(pair, idx),
      version: 2,
      author: "Soundshed Generator",
      category: pair.nam.category,
      tags: Array.from(new Set(["generated", ...pair.nam.tags, ...pair.ir.tags])).slice(0, 12),
      description: `Generated from ${pair.nam.name} + ${pair.ir.name}.`,
      global: {
        inputTrim: 0,
        outputTrim: 0
      },
      graph: {
        nodes,
        edges
      }
    };
  });
}
