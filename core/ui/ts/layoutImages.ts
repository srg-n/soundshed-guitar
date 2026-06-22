/**
 * Lazy loader for layout background images.
 *
 * Layout images (base64-encoded backgrounds) are heavy and only needed by the
 * layout designer/manager, so they are no longer shipped in the startup
 * `layoutLibraryLoaded` payload. This module requests them on demand the first
 * time the designer or manager is shown, and lets callers force a refresh after
 * the layout library is reloaded (e.g. after saving a new image).
 */

import { postMessage } from "./bridge.js";

let loaded = false;
let pending = false;

/**
 * Request the layout image set if it has not been loaded yet.
 * @param force Re-request even if already loaded (used after a library reload).
 */
export function ensureLayoutImagesLoaded(force = false): void {
  if (pending) return;
  if (loaded && !force) return;
  pending = true;
  postMessage({ type: "requestLayoutImages" });
}

/** Called when a `layoutImagesLoaded` message arrives. */
export function markLayoutImagesLoaded(): void {
  pending = false;
  loaded = true;
}

/** Whether the layout image set has been received at least once. */
export function areLayoutImagesLoaded(): boolean {
  return loaded;
}
