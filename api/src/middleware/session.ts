import { MiddlewareHandler } from "hono";
import { findSession, findUserById, touchSession } from "../lib/db";
import { fail } from "../lib/http";
import { getCookie } from "../lib/utils";
import { Env } from "../types/env";

type AppVariables = {
  auth?: { userId: string; email: string; role: string; sessionId: string };
};

export const optionalAuth: MiddlewareHandler<{ Bindings: Env; Variables: AppVariables }> = async (c, next) => {
  const cookieName = c.env.COOKIE_NAME;
  const headerSessionId = c.req.header("x-session-id")?.trim() ?? "";
  const cookieSessionId = getCookie(c.req.header("cookie") ?? null, cookieName) ?? "";
  const sessionId = headerSessionId || cookieSessionId;
  if (!sessionId) {
    await next();
    return;
  }

  const session = await findSession(c.env.DB, sessionId);
  if (!session || session.revoked_at) {
    await next();
    return;
  }

  if (new Date(session.expires_at).getTime() <= Date.now()) {
    await next();
    return;
  }

  const user = await findUserById(c.env.DB, session.user_id);
  if (!user) {
    await next();
    return;
  }

  await touchSession(c.env.DB, session.id);
  c.set("auth", { userId: user.id, email: user.email, role: user.role, sessionId: session.id });
  await next();
};

export const requireAuth: MiddlewareHandler<{ Bindings: Env; Variables: AppVariables }> = async (c, next) => {
  await optionalAuth(c as never, async () => undefined);
  const auth = c.get("auth") as AppVariables["auth"];
  if (!auth) {
    c.res = fail(c, "UNAUTHORIZED", "Authentication required", 401);
    return;
  }
  await next();
};
