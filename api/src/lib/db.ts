import { randomId, toIso } from "./utils";

export type UserRow = {
  id: string;
  email: string;
  role: "user" | "curator" | "admin";
};

export type SessionRow = {
  id: string;
  user_id: string;
  expires_at: string;
  revoked_at: string | null;
};

export async function findUserByEmail(db: D1Database, email: string): Promise<UserRow | null> {
  const result = await db.prepare("SELECT id, email, role FROM users WHERE email = ?").bind(email).first<UserRow>();
  return result ?? null;
}

export async function findUserById(db: D1Database, userId: string): Promise<UserRow | null> {
  const result = await db.prepare("SELECT id, email, role FROM users WHERE id = ?").bind(userId).first<UserRow>();
  return result ?? null;
}

export async function createUser(db: D1Database, email: string): Promise<UserRow> {
  const userId = randomId("usr");
  await db
    .prepare("INSERT INTO users (id, email, role, created_at, updated_at) VALUES (?, ?, 'user', CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)")
    .bind(userId, email)
    .run();
  return { id: userId, email, role: "user" };
}

export async function upsertUserByEmail(db: D1Database, email: string): Promise<UserRow> {
  const existing = await findUserByEmail(db, email);
  if (existing) return existing;
  return createUser(db, email);
}

export async function insertAuthToken(db: D1Database, email: string, tokenHash: string, expiresAt: Date): Promise<string> {
  const id = randomId("atk");
  await db
    .prepare("INSERT INTO auth_tokens (id, email, token_hash, purpose, expires_at, created_at) VALUES (?, ?, ?, 'login', ?, CURRENT_TIMESTAMP)")
    .bind(id, email, tokenHash, toIso(expiresAt))
    .run();
  return id;
}

export async function useAuthToken(
  db: D1Database,
  email: string,
  tokenHash: string,
  nowIso: string
): Promise<boolean> {
  const token = await db
    .prepare(
      "SELECT id FROM auth_tokens WHERE email = ? AND token_hash = ? AND used_at IS NULL AND expires_at > ? ORDER BY created_at DESC LIMIT 1"
    )
    .bind(email, tokenHash, nowIso)
    .first<{ id: string }>();

  if (!token) return false;

  await db
    .prepare("UPDATE auth_tokens SET used_at = CURRENT_TIMESTAMP WHERE id = ? AND used_at IS NULL")
    .bind(token.id)
    .run();

  return true;
}

export async function createSession(db: D1Database, userId: string, expiresAt: Date): Promise<string> {
  const sessionId = randomId("ses");
  await db
    .prepare(
      "INSERT INTO sessions (id, user_id, expires_at, created_at, last_seen_at) VALUES (?, ?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)"
    )
    .bind(sessionId, userId, toIso(expiresAt))
    .run();
  return sessionId;
}

export async function findSession(db: D1Database, sessionId: string): Promise<SessionRow | null> {
  const session = await db
    .prepare("SELECT id, user_id, expires_at, revoked_at FROM sessions WHERE id = ?")
    .bind(sessionId)
    .first<SessionRow>();
  return session ?? null;
}

export async function revokeSession(db: D1Database, sessionId: string): Promise<void> {
  await db.prepare("UPDATE sessions SET revoked_at = CURRENT_TIMESTAMP WHERE id = ?").bind(sessionId).run();
}

export async function touchSession(db: D1Database, sessionId: string): Promise<void> {
  await db.prepare("UPDATE sessions SET last_seen_at = CURRENT_TIMESTAMP WHERE id = ?").bind(sessionId).run();
}
