import { Env } from "../types/env";

function escapeHtml(value: string): string {
  return value
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/\"/g, "&quot;")
    .replace(/'/g, "&#39;");
}

export async function sendMagicCodeEmail(
  env: Env,
  recipientEmail: string,
  code: string,
  expiresInMinutes: number
): Promise<void> {
  const isProduction = env.ENVIRONMENT === "production";

  if (!env.SENDGRID_API_KEY) {
    if (isProduction) {
      throw new Error("SENDGRID_API_KEY is required in production");
    }
    console.warn(
      `[auth] SENDGRID_API_KEY is not set. Dev fallback active. email=${recipientEmail} code=${code} expiresInMinutes=${expiresInMinutes}`
    );
    return;
  }

  const subject = "Your Soundshed sign-in code";
  const safeCode = escapeHtml(code);
  const html = `
    <div style="font-family:Arial,sans-serif;max-width:560px;margin:0 auto;padding:16px;">
      <h2 style="margin:0 0 12px;">Sign in to Soundshed</h2>
      <p style="margin:0 0 12px;">Use this one-time code:</p>
      <div style="font-size:24px;font-weight:700;letter-spacing:2px;margin:12px 0 16px;">${safeCode}</div>
      <p style="margin:0 0 8px;">This code expires in ${expiresInMinutes} minutes.</p>
      <p style="margin:0;color:#666;">If you did not request this code, you can ignore this email.</p>
    </div>
  `;

  const text = [
    "Sign in to Soundshed",
    "",
    `Your one-time code: ${code}`,
    `Expires in ${expiresInMinutes} minutes.`,
    "",
    "If you did not request this code, you can ignore this email."
  ].join("\n");

  const response = await fetch("https://api.sendgrid.com/v3/mail/send", {
    method: "POST",
    headers: {
      Authorization: `Bearer ${env.SENDGRID_API_KEY}`,
      "Content-Type": "application/json"
    },
    body: JSON.stringify({
      personalizations: [
        {
          to: [{ email: recipientEmail }]
        }
      ],
      from: {
        email: env.SENDGRID_FROM_EMAIL,
        name: env.SENDGRID_FROM_NAME
      },
      subject,
      content: [
        { type: "text/plain", value: text },
        { type: "text/html", value: html }
      ]
    })
  });

  if (!response.ok) {
    const errorBody = await response.text();
    throw new Error(`SendGrid failed: ${response.status} ${errorBody}`);
  }
}
