import { describe, expect, it } from "vitest";
import { shouldMarkSignalPathNodeConfigUpdateDirty } from "../ts/signalPathConfigUpdates";

describe("shouldMarkSignalPathNodeConfigUpdateDirty", () => {
  it("marks explicit dirty updates dirty", () => {
    expect(shouldMarkSignalPathNodeConfigUpdateDirty({ dirty: true, persist: false })).toBe(true);
  });

  it("preserves explicit clean updates", () => {
    expect(shouldMarkSignalPathNodeConfigUpdateDirty({ dirty: false, persist: true })).toBe(false);
  });

  it("keeps transient updates clean", () => {
    expect(shouldMarkSignalPathNodeConfigUpdateDirty({ persist: false })).toBe(false);
  });

  it("treats legacy config updates as persisted changes", () => {
    expect(shouldMarkSignalPathNodeConfigUpdateDirty({})).toBe(true);
    expect(shouldMarkSignalPathNodeConfigUpdateDirty({ persist: true })).toBe(true);
  });
});