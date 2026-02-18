# Soundshed Presets API (Cloudflare Worker)

## Quickstart

1. Install dependencies

```bash
npm install
```

2. Update `wrangler.toml` bindings and IDs.

Set `ENVIRONMENT` to `development` for local and `production` in deployed environments.

3. Set SendGrid API key as a Worker secret

```bash
wrangler secret put SENDGRID_API_KEY
```

4. Set Turnstile secret key as a Worker secret

```bash
wrangler secret put TURNSTILE_SECRET_KEY
```

5. Apply schema migration to local D1

```bash
npm run d1:migrate
```

6. Start local dev server

```bash
npm run dev
```

## Implemented endpoints

- `GET /health`
- `POST /v1/auth/start`
- `POST /v1/auth/verify`
- `GET /v1/auth/me`
- `POST /v1/auth/logout`
- `GET /v1/home`
- `GET /v1/rows/:slug`
- `GET /v1/search`
- `GET /v1/items`
- `POST /v1/items`
- `GET /v1/items/me/list`
- `PATCH /v1/items/:itemId`
- `POST /v1/items/:itemId/submit`
- `POST /v1/items/:itemId/publish`
- `GET /v1/items/:itemId`
- `GET /v1/items/:itemId/download`
- `GET /v1/packs`
- `POST /v1/packs`
- `GET /v1/packs/me/list`
- `PATCH /v1/packs/:packId`
- `POST /v1/packs/:packId/items`
- `POST /v1/packs/:packId/submit`
- `POST /v1/packs/:packId/publish`
- `GET /v1/packs/:packId`
- `GET /v1/packs/:packId/download`
- `POST /v1/uploads/init`
- `PUT /v1/uploads/:uploadId`
- `POST /v1/uploads/complete`

## Notes

- `POST /v1/auth/start` sends the one-time code through SendGrid.
- `POST /v1/auth/start` requires `turnstileToken` in the request body.
- `POST /v1/uploads/init` requires `turnstileToken` in the request body.
- Ensure your sender domain/email is verified in SendGrid before use.
- If `SENDGRID_API_KEY` is not set in `development`, auth falls back to logging the one-time code in Worker logs.
- If `SENDGRID_API_KEY` is not set in `production`, `/v1/auth/start` fails with an email configuration error.
- If `TURNSTILE_SECRET_KEY` is not set in `development`, Turnstile checks are bypassed with a warning log.
- If `TURNSTILE_SECRET_KEY` is not set in `production`, Turnstile-protected endpoints fail.
- Upload path currently stores binary through Worker endpoint, not direct pre-signed R2 URL.
