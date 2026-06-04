import type { Tone3000Model, Tone3000Tone } from "./tone3000ApiTypes.js";
import type { Tone3000Architecture } from "./tone3000ApiTypes.js";

export const TONE3000_API_BASE = "https://www.tone3000.com/api/v1";
export const TONE3000_SESSION_URL = `${TONE3000_API_BASE}/auth/session`;

export type Tone3000PaginatedLike = {
  page?: unknown;
  current_page?: unknown;
  total?: unknown;
  total_count?: unknown;
  count?: unknown;
  total_pages?: unknown;
  totalPages?: unknown;
  pages?: unknown;
};

function asRecord(value: unknown): Record<string, unknown> | null {
  if (!value || typeof value !== "object") {
    return null;
  }
  return value as Record<string, unknown>;
}

export function extractTone3000Tones(payload: unknown): Tone3000Tone[] {
  if (Array.isArray(payload)) {
    return payload as Tone3000Tone[];
  }

  const obj = asRecord(payload);
  if (!obj) {
    return [];
  }

  if (Array.isArray(obj.tones)) return obj.tones as Tone3000Tone[];
  if (Array.isArray(obj.results)) return obj.results as Tone3000Tone[];
  if (Array.isArray(obj.items)) return obj.items as Tone3000Tone[];
  if (Array.isArray(obj.data)) return obj.data as Tone3000Tone[];
  return [];
}

export function extractTone3000Models(payload: unknown): Tone3000Model[] {
  if (Array.isArray(payload)) {
    return payload as Tone3000Model[];
  }

  const obj = asRecord(payload);
  if (!obj) {
    return [];
  }

  if (Array.isArray(obj.models)) return obj.models as Tone3000Model[];
  if (Array.isArray(obj.data)) return obj.data as Tone3000Model[];
  if (Array.isArray(obj.results)) return obj.results as Tone3000Model[];
  return [];
}

export function buildTone3000ModelsUrl(
  toneId: string | number,
  page = 1,
  pageSize = 100,
  architecture?: Tone3000Architecture,
): string {
  const params = new URLSearchParams({
    tone_id: String(toneId),
    page: String(page),
    page_size: String(pageSize),
  });
  if (architecture) {
    params.set("architecture", architecture);
  }
  return `${TONE3000_API_BASE}/models?${params.toString()}`;
}

export function buildTone3000SearchUrl(params: URLSearchParams): string {
  return `${TONE3000_API_BASE}/tones/search?${params.toString()}`;
}

export function parseTone3000Pagination(
  data: Tone3000PaginatedLike | undefined,
  currentPage: number,
  pageSize: number,
): { page: number; totalPages: number; total: number | null } {
  const page = typeof data?.page === "number"
    ? data.page
    : typeof data?.current_page === "number"
      ? data.current_page
      : currentPage;

  const total = typeof data?.total === "number"
    ? data.total
    : typeof data?.total_count === "number"
      ? data.total_count
      : typeof data?.count === "number"
        ? data.count
        : null;

  const totalPages = typeof data?.total_pages === "number"
    ? data.total_pages
    : typeof data?.totalPages === "number"
      ? data.totalPages
      : typeof data?.pages === "number"
        ? data.pages
        : total
          ? Math.max(1, Math.ceil(total / pageSize))
          : currentPage;

  return { page, totalPages: totalPages || currentPage, total };
}
