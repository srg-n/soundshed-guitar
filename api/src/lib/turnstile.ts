import { Env } from "../types/env";

type TurnstileResponse = {
  success: boolean;
  "error-codes"?: string[];
};

export async function verifyTurnstileToken(
  env: Env,
  token: string,
  remoteIp?: string
): Promise<{ ok: true } | { ok: false; reason: string }> {
  const isProduction = env.ENVIRONMENT === "production";

  if (!token) {
    if (isProduction) {
      return { ok: false, reason: "turnstile token is required" };
    }
    console.warn("[turnstile] Missing token in development, bypassing verification");
    return { ok: true };
  }

  if (!env.TURNSTILE_SECRET_KEY) {
    if (isProduction) {
      return { ok: false, reason: "turnstile is not configured" };
    }
    console.warn("[turnstile] TURNSTILE_SECRET_KEY missing in development, bypassing verification");
    return { ok: true };
  }

  const form = new URLSearchParams();
  form.set("secret", env.TURNSTILE_SECRET_KEY);
  form.set("response", token);
  if (remoteIp) {
    form.set("remoteip", remoteIp);
  }

  const response = await fetch("https://challenges.cloudflare.com/turnstile/v0/siteverify", {
    method: "POST",
    headers: {
      "Content-Type": "application/x-www-form-urlencoded"
    },
    body: form.toString()
  });

  if (!response.ok) {
    return { ok: false, reason: `turnstile verify failed: ${response.status}` };
  }

  const payload = (await response.json()) as TurnstileResponse;
  if (!payload.success) {
    const codes = payload["error-codes"]?.join(",") ?? "unknown_error";
    return { ok: false, reason: `turnstile rejected: ${codes}` };
  }

  return { ok: true };
}
