/**
 * Splash Screen Manager
 * 
 * Displays a splash screen during app initialization and hides it when the app
 * is ready. This prevents flash of unthemed content while the UI loads and theme
 * is applied.
 */

/** Hides the splash screen with a fade-out animation. */
export function hideSplashScreen(): void {
  const splash = document.getElementById("splash-screen");
  if (!splash) return;

  // Add fade-out class for animation
  splash.classList.add("splash-hidden");

  // Remove from DOM after animation completes (300ms)
  setTimeout(() => {
    splash.remove();
  }, 300);
}

/** Initializes the splash screen (currently just ensures it's visible). */
export function initSplashScreen(): void {
  const splash = document.getElementById("splash-screen");
  if (!splash) return;

  // Ensure splash is visible (should be by default)
  splash.style.display = "flex";
}
