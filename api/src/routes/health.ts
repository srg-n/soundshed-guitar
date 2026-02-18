import { Hono } from "hono";
import { ok } from "../lib/http";
import { Env } from "../types/env";

export function healthRoutes() {
  const app = new Hono<{ Bindings: Env }>();

  app.get("/health", async (c) => {
    const probe = await c.env.DB.prepare("SELECT 1 AS ok").first<{ ok: number }>();
    return ok(c, {
      status: "ok",
      db: probe?.ok === 1 ? "ok" : "error",
      now: new Date().toISOString()
    });
  });

  return app;
}
