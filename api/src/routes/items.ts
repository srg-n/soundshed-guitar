import { Hono } from "hono";
import { fail, ok, safeJson } from "../lib/http";
import { optionalAuth, requireAuth } from "../middleware/session";
import { Env } from "../types/env";
import { randomId } from "../lib/utils";

type ItemType = "preset" | "blend" | "layout" | "composite" | "combo";
type ItemVisibility = "public" | "unlisted" | "private";

type CreateItemBody = {
  type?: ItemType;
  title?: string;
  description?: string;
  visibility?: ItemVisibility;
  appMinVersion?: string;
  appMaxVersion?: string;
  payloadAssetId?: string;
  manifestAssetId?: string;
  thumbnailAssetId?: string;
  previewAssetId?: string;
};

type UpdateItemBody = Partial<CreateItemBody>;

const allowedTypes = new Set<ItemType>(["preset", "blend", "layout", "composite", "combo"]);
const allowedVisibility = new Set<ItemVisibility>(["public", "unlisted", "private"]);

type ItemRow = {
  id: string;
  creator_user_id: string;
  type: ItemType;
  title: string;
  description: string | null;
  visibility: ItemVisibility;
  moderation_status: "draft" | "pending_review" | "approved" | "rejected";
  app_min_version: string | null;
  app_max_version: string | null;
  payload_asset_id: string | null;
  manifest_asset_id: string | null;
  thumbnail_asset_id: string | null;
  preview_asset_id: string | null;
  published_at: string | null;
  created_at: string;
  updated_at: string;
};

function toItemResponse(item: ItemRow) {
  return {
    id: item.id,
    creatorUserId: item.creator_user_id,
    type: item.type,
    title: item.title,
    description: item.description,
    visibility: item.visibility,
    moderationStatus: item.moderation_status,
    appMinVersion: item.app_min_version,
    appMaxVersion: item.app_max_version,
    payloadAssetId: item.payload_asset_id,
    manifestAssetId: item.manifest_asset_id,
    thumbnailAssetId: item.thumbnail_asset_id,
    previewAssetId: item.preview_asset_id,
    publishedAt: item.published_at,
    createdAt: item.created_at,
    updatedAt: item.updated_at
  };
}

function downloadFileName(base: string, ext: string): string {
  const normalized = base
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9\-_ ]/g, "")
    .replace(/\s+/g, "-")
    .slice(0, 80);
  const safe = normalized.length > 0 ? normalized : "preset";
  return `${safe}.${ext}`;
}

export function itemRoutes() {
  const app = new Hono<{ Bindings: Env; Variables: { auth?: { userId: string; email: string; role: string; sessionId: string } } }>();

  app.get("/", async (c) => {
    const page = Math.max(1, Number.parseInt((c.req.query("page") ?? "1").trim(), 10) || 1);
    const pageSizeRaw = Number.parseInt((c.req.query("pageSize") ?? "24").trim(), 10) || 24;
    const pageSize = Math.min(100, Math.max(1, pageSizeRaw));
    const offset = (page - 1) * pageSize;

    const type = (c.req.query("type") ?? "").trim();
    const query = (c.req.query("q") ?? "").trim();
    const taxonomy = (c.req.query("taxonomy") ?? "").trim();

    const params: unknown[] = [];
    let sql = `
      SELECT DISTINCT i.id, i.creator_user_id, i.type, i.title, i.description, i.visibility, i.moderation_status,
             i.app_min_version, i.app_max_version, i.payload_asset_id, i.manifest_asset_id, i.thumbnail_asset_id, i.preview_asset_id,
             i.published_at, i.created_at, i.updated_at
      FROM items i
      LEFT JOIN item_taxonomies it ON it.item_id = i.id
      LEFT JOIN taxonomies t ON t.id = it.taxonomy_id
      WHERE i.moderation_status = 'approved'
    `;

    if (type.length > 0) {
      sql += " AND i.type = ?";
      params.push(type);
    }
    if (query.length > 0) {
      sql += " AND i.title LIKE ?";
      params.push(`%${query}%`);
    }
    if (taxonomy.length > 0) {
      sql += " AND t.slug = ?";
      params.push(taxonomy);
    }

    sql += " ORDER BY i.published_at DESC, i.updated_at DESC LIMIT ? OFFSET ?";
    params.push(pageSize, offset);

    const rows = await c.env.DB.prepare(sql).bind(...params).all<ItemRow>();
    return ok(c, {
      page,
      pageSize,
      items: rows.results.map(toItemResponse)
    });
  });

  app.get("/me/list", requireAuth, async (c) => {
    const auth = c.get("auth");
    if (!auth) {
      return fail(c, "UNAUTHORIZED", "Authentication required", 401);
    }

    const status = (c.req.query("status") ?? "").trim();
    const type = (c.req.query("type") ?? "").trim();

    const params: unknown[] = [auth.userId];
    let sql = `
      SELECT id, creator_user_id, type, title, description, visibility, moderation_status,
             app_min_version, app_max_version, payload_asset_id, manifest_asset_id, thumbnail_asset_id, preview_asset_id,
             published_at, created_at, updated_at
      FROM items
      WHERE creator_user_id = ?
    `;

    if (status.length > 0) {
      sql += " AND moderation_status = ?";
      params.push(status);
    }
    if (type.length > 0) {
      sql += " AND type = ?";
      params.push(type);
    }

    sql += " ORDER BY updated_at DESC LIMIT 200";

    const rows = await c.env.DB.prepare(sql).bind(...params).all<ItemRow>();
    return ok(c, { items: rows.results.map(toItemResponse) });
  });

  app.post("/", requireAuth, async (c) => {
    const auth = c.get("auth");
    if (!auth) {
      return fail(c, "UNAUTHORIZED", "Authentication required", 401);
    }

    const body = await safeJson<CreateItemBody>(c.req.raw);
    const type = body?.type;
    const title = body?.title?.trim();
    const visibility = body?.visibility ?? "public";

    if (!type || !allowedTypes.has(type)) {
      return fail(c, "INVALID_TYPE", "Invalid item type", 422);
    }
    if (!title) {
      return fail(c, "INVALID_TITLE", "title is required", 422);
    }
    if (!allowedVisibility.has(visibility)) {
      return fail(c, "INVALID_VISIBILITY", "Invalid visibility", 422);
    }

    const itemId = randomId("itm");
    await c.env.DB
      .prepare(
        `INSERT INTO items (
          id, creator_user_id, type, title, description, visibility, moderation_status,
          app_min_version, app_max_version, payload_asset_id, manifest_asset_id, thumbnail_asset_id, preview_asset_id,
          created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, 'draft', ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)`
      )
      .bind(
        itemId,
        auth.userId,
        type,
        title,
        body?.description?.trim() ?? null,
        visibility,
        body?.appMinVersion?.trim() ?? null,
        body?.appMaxVersion?.trim() ?? null,
        body?.payloadAssetId?.trim() ?? null,
        body?.manifestAssetId?.trim() ?? null,
        body?.thumbnailAssetId?.trim() ?? null,
        body?.previewAssetId?.trim() ?? null
      )
      .run();

    const created = await c.env.DB
      .prepare(
        `SELECT id, creator_user_id, type, title, description, visibility, moderation_status,
                app_min_version, app_max_version, payload_asset_id, manifest_asset_id, thumbnail_asset_id, preview_asset_id,
                published_at, created_at, updated_at
         FROM items WHERE id = ?`
      )
      .bind(itemId)
      .first<ItemRow>();

    return ok(c, { item: created ? toItemResponse(created) : null }, 201);
  });

  app.patch("/:itemId", requireAuth, async (c) => {
    const auth = c.get("auth");
    if (!auth) {
      return fail(c, "UNAUTHORIZED", "Authentication required", 401);
    }

    const itemId = c.req.param("itemId");
    const existing = await c.env.DB
      .prepare(
        `SELECT id, creator_user_id, type, title, description, visibility, moderation_status,
                app_min_version, app_max_version, payload_asset_id, manifest_asset_id, thumbnail_asset_id, preview_asset_id,
                published_at, created_at, updated_at
         FROM items WHERE id = ?`
      )
      .bind(itemId)
      .first<ItemRow>();

    if (!existing) {
      return fail(c, "NOT_FOUND", "Item not found", 404);
    }
    if (existing.creator_user_id !== auth.userId) {
      return fail(c, "FORBIDDEN", "You do not own this item", 403);
    }

    const body = await safeJson<UpdateItemBody>(c.req.raw);
    if (!body) {
      return fail(c, "INVALID_BODY", "Invalid request body", 422);
    }

    const type = body.type ?? existing.type;
    const title = body.title !== undefined ? body.title.trim() : existing.title;
    const visibility = body.visibility ?? existing.visibility;

    if (!allowedTypes.has(type)) {
      return fail(c, "INVALID_TYPE", "Invalid item type", 422);
    }
    if (!title) {
      return fail(c, "INVALID_TITLE", "title cannot be empty", 422);
    }
    if (!allowedVisibility.has(visibility)) {
      return fail(c, "INVALID_VISIBILITY", "Invalid visibility", 422);
    }

    await c.env.DB
      .prepare(
        `UPDATE items SET
          type = ?,
          title = ?,
          description = ?,
          visibility = ?,
          app_min_version = ?,
          app_max_version = ?,
          payload_asset_id = ?,
          manifest_asset_id = ?,
          thumbnail_asset_id = ?,
          preview_asset_id = ?,
          updated_at = CURRENT_TIMESTAMP
         WHERE id = ?`
      )
      .bind(
        type,
        title,
        body.description !== undefined ? body.description?.trim() ?? null : existing.description,
        visibility,
        body.appMinVersion !== undefined ? body.appMinVersion?.trim() ?? null : existing.app_min_version,
        body.appMaxVersion !== undefined ? body.appMaxVersion?.trim() ?? null : existing.app_max_version,
        body.payloadAssetId !== undefined ? body.payloadAssetId?.trim() ?? null : existing.payload_asset_id,
        body.manifestAssetId !== undefined ? body.manifestAssetId?.trim() ?? null : existing.manifest_asset_id,
        body.thumbnailAssetId !== undefined ? body.thumbnailAssetId?.trim() ?? null : existing.thumbnail_asset_id,
        body.previewAssetId !== undefined ? body.previewAssetId?.trim() ?? null : existing.preview_asset_id,
        itemId
      )
      .run();

    const updated = await c.env.DB
      .prepare(
        `SELECT id, creator_user_id, type, title, description, visibility, moderation_status,
                app_min_version, app_max_version, payload_asset_id, manifest_asset_id, thumbnail_asset_id, preview_asset_id,
                published_at, created_at, updated_at
         FROM items WHERE id = ?`
      )
      .bind(itemId)
      .first<ItemRow>();

    return ok(c, { item: updated ? toItemResponse(updated) : null });
  });

  app.post("/:itemId/submit", requireAuth, async (c) => {
    const auth = c.get("auth");
    if (!auth) {
      return fail(c, "UNAUTHORIZED", "Authentication required", 401);
    }

    const itemId = c.req.param("itemId");
    const item = await c.env.DB
      .prepare("SELECT id, creator_user_id, moderation_status FROM items WHERE id = ?")
      .bind(itemId)
      .first<{ id: string; creator_user_id: string; moderation_status: string }>();

    if (!item) {
      return fail(c, "NOT_FOUND", "Item not found", 404);
    }
    if (item.creator_user_id !== auth.userId) {
      return fail(c, "FORBIDDEN", "You do not own this item", 403);
    }

    await c.env.DB
      .prepare("UPDATE items SET moderation_status = 'pending_review', updated_at = CURRENT_TIMESTAMP WHERE id = ?")
      .bind(itemId)
      .run();

    return ok(c, { itemId, moderationStatus: "pending_review" });
  });

  app.post("/:itemId/publish", requireAuth, async (c) => {
    const auth = c.get("auth");
    if (!auth) {
      return fail(c, "UNAUTHORIZED", "Authentication required", 401);
    }

    const itemId = c.req.param("itemId");
    const item = await c.env.DB
      .prepare("SELECT id, creator_user_id, payload_asset_id FROM items WHERE id = ?")
      .bind(itemId)
      .first<{ id: string; creator_user_id: string; payload_asset_id: string | null }>();

    if (!item) {
      return fail(c, "NOT_FOUND", "Item not found", 404);
    }
    if (item.creator_user_id !== auth.userId) {
      return fail(c, "FORBIDDEN", "You do not own this item", 403);
    }
    if (!item.payload_asset_id) {
      return fail(c, "MISSING_PAYLOAD", "Cannot publish without payloadAssetId", 422);
    }

    await c.env.DB
      .prepare(
        "UPDATE items SET moderation_status = 'approved', published_at = COALESCE(published_at, CURRENT_TIMESTAMP), updated_at = CURRENT_TIMESTAMP WHERE id = ?"
      )
      .bind(itemId)
      .run();

    return ok(c, { itemId, moderationStatus: "approved" });
  });

  app.get("/:itemId", optionalAuth, async (c) => {
    const itemId = c.req.param("itemId");
    const auth = c.get("auth");

    const item = await c.env.DB
      .prepare(
        `SELECT id, creator_user_id, type, title, description, visibility, moderation_status,
                app_min_version, app_max_version, payload_asset_id, manifest_asset_id, thumbnail_asset_id, preview_asset_id,
                published_at, created_at, updated_at
         FROM items WHERE id = ?`
      )
      .bind(itemId)
      .first<ItemRow>();

    if (!item) {
      return fail(c, "NOT_FOUND", "Item not found", 404);
    }

    const isOwner = auth?.userId === item.creator_user_id;
    const isPublic = item.moderation_status === "approved";
    if (!isOwner && !isPublic) {
      return fail(c, "NOT_FOUND", "Item not found", 404);
    }

    return ok(c, { item: toItemResponse(item) });
  });

  app.get("/:itemId/download", optionalAuth, async (c) => {
    const itemId = c.req.param("itemId");
    const auth = c.get("auth");

    const item = await c.env.DB
      .prepare("SELECT id, creator_user_id, title, moderation_status, payload_asset_id FROM items WHERE id = ?")
      .bind(itemId)
      .first<{
        id: string;
        creator_user_id: string;
        title: string;
        moderation_status: string;
        payload_asset_id: string | null;
      }>();

    if (!item) {
      return fail(c, "NOT_FOUND", "Item not found", 404);
    }

    const isOwner = auth?.userId === item.creator_user_id;
    const isPublic = item.moderation_status === "approved";
    if (!isOwner && !isPublic) {
      return fail(c, "NOT_FOUND", "Item not found", 404);
    }
    if (!item.payload_asset_id) {
      return fail(c, "MISSING_PAYLOAD", "Item payload is not available", 409);
    }

    const asset = await c.env.DB
      .prepare("SELECT r2_key, mime_type FROM assets WHERE id = ?")
      .bind(item.payload_asset_id)
      .first<{ r2_key: string; mime_type: string }>();

    if (!asset) {
      return fail(c, "ASSET_NOT_FOUND", "Payload asset not found", 404);
    }

    const object = await c.env.ASSETS.get(asset.r2_key);
    if (!object || !object.body) {
      return fail(c, "OBJECT_NOT_FOUND", "Payload object not found", 404);
    }

    await c.env.DB
      .prepare("INSERT INTO downloads (id, user_id, item_id, pack_id, created_at) VALUES (?, ?, ?, NULL, CURRENT_TIMESTAMP)")
      .bind(randomId("dwl"), auth?.userId ?? null, itemId)
      .run();

    await c.env.DB.prepare("UPDATE items SET download_count = download_count + 1 WHERE id = ?").bind(itemId).run();

    const contentType = object.httpMetadata?.contentType ?? asset.mime_type ?? "application/octet-stream";
    const fileName = downloadFileName(item.title, "preset");

    return new Response(object.body, {
      status: 200,
      headers: {
        "content-type": contentType,
        "content-disposition": `attachment; filename=\"${fileName}\"`
      }
    });
  });

  return app;
}
