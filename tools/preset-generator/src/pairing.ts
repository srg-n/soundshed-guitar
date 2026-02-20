import { Pairing, ResourceCandidate } from "./types.js";

function overlapScore(a: string[], b: string[]): number {
  if (a.length === 0 || b.length === 0) {
    return 0;
  }
  const setA = new Set(a.map((v) => v.toLowerCase()));
  const setB = new Set(b.map((v) => v.toLowerCase()));
  let overlap = 0;
  for (const tag of setA) {
    if (setB.has(tag)) {
      overlap += 1;
    }
  }
  return overlap / Math.max(setA.size, setB.size);
}

export function buildPairings(opts: {
  nams: ResourceCandidate[];
  irs: ResourceCandidate[];
  minPopularity: number;
  maxPairsPerNam: number;
}): Pairing[] {
  const filteredNams = opts.nams.filter((n) => n.popularity >= opts.minPopularity);
  const filteredIrs = opts.irs.filter((i) => i.popularity >= opts.minPopularity);

  const all: Pairing[] = [];
  for (const nam of filteredNams) {
    const scored: Pairing[] = filteredIrs.map((ir) => {
      const tagMatch = overlapScore(nam.tags, ir.tags);
      const categoryMatch = nam.category.toLowerCase() === ir.category.toLowerCase() ? 1 : 0;
      const popularityMean = (nam.popularity + ir.popularity) / 2;
      const score = 0.35 * tagMatch + 0.35 * categoryMatch + 0.3 * popularityMean;
      const reasons = [
        `tagMatch=${tagMatch.toFixed(2)}`,
        `categoryMatch=${categoryMatch.toFixed(2)}`,
        `popularity=${popularityMean.toFixed(2)}`
      ];
      return { nam, ir, score, reasons };
    });

    scored.sort((a, b) => b.score - a.score);
    all.push(...scored.slice(0, opts.maxPairsPerNam));
  }

  all.sort((a, b) => b.score - a.score);
  return all;
}
