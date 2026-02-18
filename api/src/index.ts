import { Hono } from "hono";
import { cors } from "hono/cors";
import { authRoutes } from "./routes/auth";
import { discoveryRoutes } from "./routes/discovery";
import { fail } from "./lib/http";
import { healthRoutes } from "./routes/health";
import { itemRoutes } from "./routes/items";
import { packRoutes } from "./routes/packs";
import { uploadRoutes } from "./routes/uploads";
import { Env } from "./types/env";

type Variables = {
  auth?: { userId: string; email: string; role: string; sessionId: string };
};

const app = new Hono<{ Bindings: Env; Variables: Variables }>();

app.use(
  "*",
  cors({
    origin: "*",
    allowMethods: ["GET", "POST", "PUT", "PATCH", "OPTIONS"],
    allowHeaders: ["content-type", "x-session-id"]
  })
);

app.route("/", healthRoutes());
app.route("/v1/auth", authRoutes());
app.route("/v1", discoveryRoutes());
app.route("/v1/items", itemRoutes());
app.route("/v1/packs", packRoutes());
app.route("/v1/uploads", uploadRoutes());

app.notFound((c) => fail(c, "NOT_FOUND", "Route not found", 404));

app.onError((error, c) => {
  return fail(c, "INTERNAL_ERROR", error.message || "Unexpected error", 500);
});

export default app;
