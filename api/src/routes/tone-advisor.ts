import { Hono } from "hono";
import { fail, ok } from "../lib/http";
import { Env } from "../types/env";

// Allowed characters for band/song input — prevents prompt injection while preserving typical names
const SAFE_INPUT_RE = /[^a-zA-Z0-9 ',.\-&!()]/g;

const TONE_SYSTEM_PROMPT = `You are an expert guitar tone consultant with encyclopedic knowledge of the gear used by famous bands and artists. When given a band name and/or song title, suggest realistic guitar tone setups that authentically capture the associated sound.

Respond ONLY with a valid JSON object matching this exact structure, with no extra text before or after:
{
  "combinations": [
    {
      "name": "Short name for this tone setup",
      "description": "Brief description of the tone character and context",
      "amp": "Specific amplifier model name (e.g. Marshall JCM800 2203)",
      "cabinet": "Speaker cabinet model (e.g. Marshall 1960A 4x12)",
      "pedals": ["Pedal name e.g. ProCo RAT", "Boss DS-1"],
      "effects": [
        { "type": "effect category", "name": "specific unit or plugin name", "settings": { "param": "value" } }
      ]
    }
  ]
}

Provide 2-3 distinct, historically accurate combinations. Pedals and effects arrays may be empty. Use real product names where known.`;

type ToneEffect = {
  type: string;
  name: string;
  settings?: Record<string, string | number>;
};

type ToneCombination = {
  name: string;
  description: string;
  amp: string;
  cabinet: string;
  pedals: string[];
  effects: ToneEffect[];
};

type AiTextResponse = {
  response?: string;
};

function sanitiseInput(raw: string): string {
  return raw.replace(SAFE_INPUT_RE, "").trim().slice(0, 120);
}

function extractCombinations(text: string): ToneCombination[] {
  // Pull the first {...} block from the model response in case it adds preamble
  const match = text.match(/\{[\s\S]*\}/);
  if (!match) return [];

  const parsed = JSON.parse(match[0]) as { combinations?: unknown[] };
  if (!Array.isArray(parsed.combinations)) return [];

  return parsed.combinations
    .filter((c): c is ToneCombination => typeof c === "object" && c !== null)
    .map((c) => ({
      name: typeof c.name === "string" ? c.name : "Unknown Setup",
      description: typeof c.description === "string" ? c.description : "",
      amp: typeof c.amp === "string" ? c.amp : "",
      cabinet: typeof c.cabinet === "string" ? c.cabinet : "",
      pedals: Array.isArray(c.pedals)
        ? c.pedals.filter((p): p is string => typeof p === "string")
        : [],
      effects: Array.isArray(c.effects)
        ? c.effects.filter((e): e is ToneEffect => typeof e === "object" && e !== null)
        : []
    }));
}

export function toneAdvisorRoutes() {
  const app = new Hono<{ Bindings: Env }>();

  /**
   * GET /v1/tone-advisor?band=Metallica&song=Enter+Sandman
   *
   * Returns 2-3 suggested amp/cab/pedal/effects combinations for the given
   * band and/or song using Cloudflare Workers AI.
   */
  app.get("/tone-advisor", async (c) => {
    const band = sanitiseInput(c.req.query("band") ?? "");
    const song = sanitiseInput(c.req.query("song") ?? "");

    if (!band && !song) {
      return fail(
        c,
        "MISSING_PARAMS",
        "At least one of 'band' or 'song' query parameters is required",
        400
      );
    }

    const queryParts: string[] = [];
    if (band) queryParts.push(`Band: ${band}`);
    if (song) queryParts.push(`Song: "${song}"`);
    const userPrompt = `What guitar amp, cabinet, pedals and effects setups are most associated with — ${queryParts.join(", ")}?`;

    let combinations: ToneCombination[];

    try {
      const result = (await c.env.AI.run("@cf/meta/llama-3.1-8b-instruct" as Parameters<Ai["run"]>[0], {
        messages: [
          { role: "system", content: TONE_SYSTEM_PROMPT },
          { role: "user", content: userPrompt }
        ],
        max_tokens: 1200
      })) as AiTextResponse;

      const responseText = result.response ?? "";
      combinations = extractCombinations(responseText);
    } catch (err) {
      const message = err instanceof Error ? err.message : "AI request failed";
      return fail(c, "AI_ERROR", message, 502);
    }

    if (combinations.length === 0) {
      return fail(c, "AI_PARSE_ERROR", "AI response did not contain valid tone combinations", 502);
    }

    return ok(c, {
      query: {
        ...(band ? { band } : {}),
        ...(song ? { song } : {})
      },
      combinations,
      generatedAt: new Date().toISOString()
    });
  });

  return app;
}
