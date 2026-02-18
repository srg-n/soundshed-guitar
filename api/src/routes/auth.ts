import { Hono } from "hono";
import { createSession, insertAuthToken, revokeSession, upsertUserByEmail, useAuthToken } from "../lib/db";
import { sendMagicCodeEmail } from "../lib/email";
import { fail, ok, safeJson } from "../lib/http";
import { verifyTurnstileToken } from "../lib/turnstile";
import { optionalAuth } from "../middleware/session";
import { Env } from "../types/env";
import { addSeconds, buildSessionCookie, clearSessionCookie, isEmail, parsePositiveInt, randomCode, sha256 } from "../lib/utils";

type StartBody = { email?: string; turnstileToken?: string };
type VerifyBody = { email?: string; code?: string };

export function authRoutes() {
  const app = new Hono<{ Bindings: Env; Variables: { auth?: { userId: string; email: string; role: string; sessionId: string } } }>();

  app.post("/start", async (c) => {
    const body = await safeJson<StartBody>(c.req.raw);
    const email = body?.email?.trim().toLowerCase();
    const turnstileToken = body?.turnstileToken?.trim() ?? "";
    if (!email || !isEmail(email)) {
      return fail(c, "INVALID_EMAIL", "A valid email is required", 422);
    }

    const remoteIp = c.req.header("CF-Connecting-IP") ?? undefined;
    const turnstile = await verifyTurnstileToken(c.env, turnstileToken, remoteIp);
    if (!turnstile.ok) {
      return fail(c, "INVALID_TURNSTILE", turnstile.reason, 422);
    }

    const code = randomCode(18);
    const tokenHash = await sha256(code);
    const ttl = parsePositiveInt(c.env.MAGIC_LINK_TTL_SECONDS, 900);
    const expiresAt = addSeconds(new Date(), ttl);
    await insertAuthToken(c.env.DB, email, tokenHash, expiresAt);

    const expiresInMinutes = Math.max(1, Math.floor(ttl / 60));
    try {
      await sendMagicCodeEmail(c.env, email, code, expiresInMinutes);
    } catch (error) {
      return fail(c, "EMAIL_SEND_FAILED", error instanceof Error ? error.message : "Could not send sign-in email", 502);
    }

    return ok(c, {
      email,
      expiresAt: expiresAt.toISOString()
    });
  });

  app.post("/verify", async (c) => {
    const body = await safeJson<VerifyBody>(c.req.raw);
    const email = body?.email?.trim().toLowerCase();
    const code = body?.code?.trim();

    if (!email || !isEmail(email) || !code) {
      return fail(c, "INVALID_REQUEST", "email and code are required", 422);
    }

    const tokenHash = await sha256(code);
    const isValid = await useAuthToken(c.env.DB, email, tokenHash, new Date().toISOString());
    if (!isValid) {
      return fail(c, "INVALID_TOKEN", "Code is invalid or expired", 401);
    }

    const user = await upsertUserByEmail(c.env.DB, email);
    const sessionTtl = parsePositiveInt(c.env.SESSION_TTL_SECONDS, 2592000);
    const sessionExpiresAt = addSeconds(new Date(), sessionTtl);
    const sessionId = await createSession(c.env.DB, user.id, sessionExpiresAt);

    c.header("Set-Cookie", buildSessionCookie(c.env.COOKIE_NAME, sessionId, sessionExpiresAt));
    return ok(c, {
      sessionId,
      user: {
        id: user.id,
        email: user.email,
        role: user.role
      }
    });
  });

  app.get("/me", optionalAuth, async (c) => {
    const auth = c.get("auth");
    if (!auth) {
      return ok(c, { user: null });
    }
    return ok(c, {
      user: {
        id: auth.userId,
        email: auth.email,
        role: auth.role
      }
    });
  });

  app.post("/logout", optionalAuth, async (c) => {
    const auth = c.get("auth");
    if (auth?.sessionId) {
      await revokeSession(c.env.DB, auth.sessionId);
    }
    c.header("Set-Cookie", clearSessionCookie(c.env.COOKIE_NAME));
    return ok(c, { loggedOut: true });
  });

  return app;
}
