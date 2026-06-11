import { describe, expect, it } from "vitest";
import { sanitizePresetForArchive } from "../ts/presets.js";

describe("sanitizePresetForArchive", () => {
  it("removes legacy and global signal-chain state from shared/export payloads", () => {
    const preset = {
      id: "preset-1",
      name: "Shared Preset",
      global: {
        inputTrim: -3,
        outputTrim: 1,
        outputVolume: 0.8,
        autoLevelInput: true,
        autoLevelOutput: false,
        transpose: 2,
      },
      globals: {
        inputTrim: -3,
        outputTrim: 1,
        outputVolume: 0.8,
        autoLevelInput: true,
        autoLevelOutput: false,
        transpose: 2,
      },
      globalSignalChain: {
        inputGain: -3,
        outputGain: 1,
        preChainGraph: { nodes: [], edges: [] },
        postChainGraph: { nodes: [], edges: [] },
      },
      graph: {
        nodes: [{ id: "input", type: "input" }, { id: "amp", type: "amp" }],
        edges: [{ from: "input", to: "amp" }],
      },
    } as any;

    const cleaned = sanitizePresetForArchive(preset);

    expect(cleaned).not.toHaveProperty("global");
    expect(cleaned).not.toHaveProperty("globals");
    expect(cleaned).not.toHaveProperty("globalSignalChain");
    expect(cleaned.graph).toEqual(preset.graph);
  });
});
