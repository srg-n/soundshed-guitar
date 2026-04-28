import { describe, expect, it } from "vitest";
import { parseItemConfig, parsePackConfig, stringifyItemConfig } from "../src/lib/content-config";

describe("content config parsing", () => {
  it("defaults malformed item config safely", () => {
    expect(parseItemConfig("not json")).toEqual({
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
    });
  });

  it("normalizes item config fields", () => {
    const config = parseItemConfig(JSON.stringify({
      description: "A preset",
      visibility: "private",
      tags: ["rock", 7, "lead"],
      appMinVersion: "1.0.0",
      appMaxVersion: 2,
      payloadAssetId: "asset-1",
      privatePayloadAssetId: "private-1",
      manifestAssetId: "manifest-1",
      thumbnailAssetId: "thumb-1",
      previewAssetId: "preview-1",
    }));

    expect(config).toEqual({
      description: "A preset",
      visibility: "private",
      tags: ["rock", "lead"],
      appMinVersion: "1.0.0",
      appMaxVersion: null,
      payloadAssetId: "asset-1",
      privatePayloadAssetId: "private-1",
      manifestAssetId: "manifest-1",
      thumbnailAssetId: "thumb-1",
      previewAssetId: "preview-1",
    });
    expect(JSON.parse(stringifyItemConfig(config))).toMatchObject({ visibility: "private" });
  });

  it("normalizes pack config fields", () => {
    expect(parsePackConfig(JSON.stringify({
      description: "Pack",
      zipAssetId: "zip-1",
      thumbnailAssetId: 12,
    }))).toEqual({
      description: "Pack",
      zipAssetId: "zip-1",
      thumbnailAssetId: null,
    });
  });
});
