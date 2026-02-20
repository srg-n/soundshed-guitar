# Soundshed Preset Generator (MVP)

Node.js + TypeScript CLI for generating Soundshed-compatible presets and pack artifacts from Tone3000 and/or seed files.

## Commands

- `npm run generate`
- `npm run validate`
- `npm run pack`
- `npx tsx src/index.ts probe --config config/default.json`

## Config

Default config path: `config/default.json`

Tone3000 endpoint paths are fixed in code to mirror the UI integration (`/auth/session`, `/tones/search`).
Config only controls the API base URL, API key env var name, and request limits.

Key features:
- API response caching (TTL)
- Resource blob caching and deduplication by hash
- Deterministic NAM + IR pairing
- PresetV2 output with valid signal chains

Use `probe` to verify configured Tone3000 endpoints and inspect response status/body before running full generation.

## Output

Default output root: `output/`
- `runs/<runId>/presets/*.json`
- `runs/<runId>/resources/indexes/resources-index.json`
- `runs/<runId>/manifest.run.json`
- `cache/api/`
- `cache/resources/`

## Notes

This MVP uses deterministic naming and pairing. AI enrichment can be layered later.
