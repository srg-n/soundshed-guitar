import { Context } from "hono";
import type { StatusCode } from "hono/utils/http-status";

export function ok(c: Context, data: unknown, status: StatusCode = 200): Response {
  return c.newResponse(JSON.stringify({ ok: true, data }), status, {
    "content-type": "application/json"
  });
}

export function fail(c: Context, code: string, message: string, status: StatusCode = 400): Response {
  return c.newResponse(JSON.stringify({ ok: false, error: { code, message } }), status, {
    "content-type": "application/json"
  });
}

export async function safeJson<T>(request: Request): Promise<T | null> {
  try {
    return (await request.json()) as T;
  } catch {
    return null;
  }
}
