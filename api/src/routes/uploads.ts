import { Hono } from "hono";
import { fail, ok, safeJson } from "../lib/http";
import { requireAuth } from "../middleware/session";
import { verifyTurnstileToken } from "../lib/turnstile";
import { Env } from "../types/env";
import { randomId } from "../lib/utils";

type UploadInitBody = {
  kind?: "item_payload" | "item_manifest" | "pack_zip" | "thumbnail" | "preview_audio";
  mimeType?: string;
  byteSize?: number;
  turnstileToken?: string;
};

type UploadCompleteBody = {
  uploadId?: string;
};

const allowedKinds = new Set(["item_payload", "item_manifest", "pack_zip", "thumbnail", "preview_audio"]);

export function uploadRoutes() {
  const app = new Hono<{ Bindings: Env; Variables: { auth?: { userId: string; email: string; role: string; sessionId: string } } }>();

  app.post("/init", requireAuth, async (c) => {
    const body = await safeJson<UploadInitBody>(c.req.raw);
    const kind = body?.kind;
    const mimeType = body?.mimeType?.trim();
    const byteSize = body?.byteSize ?? 0;
    const turnstileToken = body?.turnstileToken?.trim() ?? "";

    if (!kind || !allowedKinds.has(kind)) {
      return fail(c, "INVALID_KIND", "Invalid upload kind", 422);
    }
    if (!mimeType || byteSize <= 0) {
      return fail(c, "INVALID_METADATA", "mimeType and byteSize are required", 422);
    }

    const remoteIp = c.req.header("CF-Connecting-IP") ?? undefined;
    const turnstile = await verifyTurnstileToken(c.env, turnstileToken, remoteIp);
    if (!turnstile.ok) {
      return fail(c, "INVALID_TURNSTILE", turnstile.reason, 422);
    }

    const auth = c.get("auth");
    if (!auth) {
      return fail(c, "UNAUTHORIZED", "Authentication required", 401);
    }

    const uploadId = randomId("upl");
    const r2Key = `users/${auth.userId}/drafts/${uploadId}/payload.bin`;

    return ok(c, {
      uploadId,
      kind,
      mimeType,
      byteSize,
      uploadUrl: `/v1/uploads/${uploadId}`,
      r2Key
    });
  });

  app.put("/:uploadId", requireAuth, async (c) => {
    const uploadId = c.req.param("uploadId");
    const auth = c.get("auth");
    if (!auth) {
      return fail(c, "UNAUTHORIZED", "Authentication required", 401);
    }

    const mimeType = c.req.header("content-type") ?? "application/octet-stream";
    const body = await c.req.arrayBuffer();
    if (body.byteLength === 0) {
      return fail(c, "EMPTY_BODY", "Upload payload is empty", 422);
    }

    const r2Key = `users/${auth.userId}/drafts/${uploadId}/payload.bin`;
    await c.env.ASSETS.put(r2Key, body, {
      httpMetadata: {
        contentType: mimeType
      }
    });

    const existing = await c.env.DB.prepare("SELECT id FROM assets WHERE id = ?").bind(uploadId).first<{ id: string }>();
    if (existing) {
      await c.env.DB
        .prepare("UPDATE assets SET r2_key = ?, mime_type = ?, byte_size = ?, uploaded_at = CURRENT_TIMESTAMP WHERE id = ?")
        .bind(r2Key, mimeType, body.byteLength, uploadId)
        .run();
    } else {
      await c.env.DB
        .prepare(
          "INSERT INTO assets (id, owner_user_id, r2_key, kind, mime_type, byte_size, uploaded_at) VALUES (?, ?, ?, 'item_payload', ?, ?, CURRENT_TIMESTAMP)"
        )
        .bind(uploadId, auth.userId, r2Key, mimeType, body.byteLength)
        .run();
    }

    return ok(c, {
      uploadId,
      r2Key,
      byteSize: body.byteLength
    });
  });

  app.post("/complete", requireAuth, async (c) => {
    const body = await safeJson<UploadCompleteBody>(c.req.raw);
    const uploadId = body?.uploadId?.trim();
    if (!uploadId) {
      return fail(c, "INVALID_UPLOAD_ID", "uploadId is required", 422);
    }

    const asset = await c.env.DB
      .prepare("SELECT id, owner_user_id, r2_key, byte_size FROM assets WHERE id = ?")
      .bind(uploadId)
      .first<{ id: string; owner_user_id: string; r2_key: string; byte_size: number }>();

    const auth = c.get("auth");
    if (!asset || !auth || asset.owner_user_id !== auth.userId) {
      return fail(c, "NOT_FOUND", "Upload not found", 404);
    }

    const object = await c.env.ASSETS.head(asset.r2_key);
    if (!object) {
      return fail(c, "MISSING_OBJECT", "Uploaded object not found in storage", 409);
    }

    return ok(c, {
      assetId: asset.id,
      r2Key: asset.r2_key,
      byteSize: asset.byte_size
    });
  });

  return app;
}
