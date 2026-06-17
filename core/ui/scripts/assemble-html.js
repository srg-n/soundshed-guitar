#!/usr/bin/env node
/**
 * Simple HTML assembler for ui-components.
 * Replaces markers like:
 *   <!--#include:header-icon-bar.html-->
 *   <!--#include:ui-components/control-bar.html-->
 *   <!--#include:main-content/visualizer.html-->
 *
 * Usage:
 *   node scripts/assemble-html.js
 *
 * It reads index.template.html if present, else index.html as input.
 * Writes the result to index.html (in-place for the ui root, matching WebView expectations).
 *
 * Run automatically as part of `npm run build`.
 */

const fs = require('fs');
const path = require('path');

const ROOT = __dirname.replace(/[\\/]scripts$/, '');
const COMPONENTS_DIR = path.join(ROOT, 'ui-components');
const TEMPLATE_CANDIDATES = [
  path.join(ROOT, 'index.template.html'),
  path.join(ROOT, 'index.html'),
];
const OUTPUT = path.join(ROOT, 'index.html');

function findTemplate() {
  for (const p of TEMPLATE_CANDIDATES) {
    if (fs.existsSync(p)) return p;
  }
  throw new Error('No index.template.html or index.html found');
}

function resolveInclude(includePath) {
  // Allow both "foo.html" and "ui-components/foo.html" or "main-content/bar.html"
  const candidates = [
    path.join(COMPONENTS_DIR, includePath),
    path.join(COMPONENTS_DIR, path.basename(includePath)),
    path.join(ROOT, includePath),
  ];
  for (const c of candidates) {
    if (fs.existsSync(c)) return c;
  }
  return null;
}

function processIncludes(html, seen = new Set()) {
  const re = /<!--\s*#include:\s*([^\s>]+?)\s*-->/g;
  return html.replace(re, (match, includeSpec) => {
    const filePath = resolveInclude(includeSpec);
    if (!filePath) {
      console.warn(`[assemble-html] Warning: include not found: ${includeSpec}`);
      return match;
    }
    const abs = path.resolve(filePath);
    if (seen.has(abs)) {
      console.warn(`[assemble-html] Warning: circular include detected for ${includeSpec}`);
      return '';
    }
    seen.add(abs);
    let content = fs.readFileSync(filePath, 'utf8');
    // Recurse for nested includes
    content = processIncludes(content, seen);
    seen.delete(abs);
    return content;
  });
}

function main() {
  const inputPath = findTemplate();
  let html = fs.readFileSync(inputPath, 'utf8');

  const before = html.length;
  html = processIncludes(html);

  // If this was the template, or we did work, write output
  fs.writeFileSync(OUTPUT, html, 'utf8');
  const after = html.length;

  console.log(`[assemble-html] Assembled ${path.basename(inputPath)} -> ${path.relative(ROOT, OUTPUT)} (${before} -> ${after} bytes)`);
}

main();
