import assert from "node:assert/strict";
import { test } from "node:test";
import { assertDownloadableResourcesResolved, buildResourceIndex, type ResolvedResource } from "../src/resource-artifacts.js";
import type { ResourceCandidate } from "../src/types.js";

function resource(overrides: Partial<ResourceCandidate> = {}): ResourceCandidate {
  return {
    id: "nam:test-model",
    kind: "nam",
    name: "Test Model",
    category: "test",
    tags: ["test"],
    popularity: 1,
    source: "tone3000",
    downloadUrl: "https://example.test/model.nam",
    ...overrides,
  };
}

test("downloadable resources must have a resolved cached blob", () => {
  assert.throws(
    () => assertDownloadableResourcesResolved([resource()], new Map()),
    /Resource download failed for 1 selected resource/
  );
});

test("resource index uses resolved blob metadata for downloadable resources", () => {
  const resolved = new Map<string, ResolvedResource>([
    ["nam:test-model", { hash: "abc123", ext: "nam", blobPath: "cache/resources/blobs/abc123.nam" }],
  ]);

  const index = buildResourceIndex({ resources: [resource()], resolved });

  assert.equal(index.items.length, 1);
  assert.equal(index.items[0]?.contentHash, "abc123");
  assert.equal(index.items[0]?.filePath, "content/tone3000/abc123.nam");
});

test("non-downloadable seed resources keep explicit metadata", () => {
  const index = buildResourceIndex({
    resources: [
      resource({
        id: "ir:seed-cab",
        kind: "ir",
        source: "seed",
        downloadUrl: undefined,
        sha256: "seedhash",
        fileExt: "wav",
      }),
    ],
    resolved: new Map(),
  });

  assert.equal(index.items[0]?.contentHash, "seedhash");
  assert.equal(index.items[0]?.filePath, "content/seed/seedhash.wav");
});
