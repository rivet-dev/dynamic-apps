#!/usr/bin/env node
// Frame validator + golden-pixel regression check (milestone M0).
//   node check.mjs <frame.bin> <golden.json> [--bless]
// Validates the v0 header, samples fixed coordinates on the raw RGBA payload, and compares them
// to golden.json (or writes it with --bless). Operates on raw bytes only — no PNG in the loop.

import { readFileSync, writeFileSync, existsSync } from "node:fs";

const [, , binPath, goldenPath, mode] = process.argv;
if (!binPath || !goldenPath) {
  console.error("usage: node check.mjs <frame.bin> <golden.json> [--bless]");
  process.exit(2);
}

const EXPECT_W = 640;
const EXPECT_H = 480;
// Coordinates chosen to hit distinct regions: panel bg, accent dot, wallpaper, inactive title
// bar, active title bar, window body, cursor tip.
const COORDS = [
  [2, 4],
  [10, 11],
  [320, 30],
  [80, 105],
  [520, 175],
  [400, 300],
  [300, 235],
];

const buf = readFileSync(binPath);

function fail(msg) {
  console.error(`FAIL: ${msg}`);
  process.exit(1);
}

if (buf.length < 12) fail(`frame too small (${buf.length} bytes)`);
const magic = buf.subarray(0, 4).toString("latin1");
if (magic !== "SXFB") fail(`bad magic ${JSON.stringify(magic)} (want "SXFB")`);
const w = buf.readUInt32LE(4);
const h = buf.readUInt32LE(8);
if (w !== EXPECT_W || h !== EXPECT_H) fail(`bad dims ${w}x${h} (want ${EXPECT_W}x${EXPECT_H})`);
const expectedLen = 12 + w * h * 4;
if (buf.length !== expectedLen) fail(`bad length ${buf.length} (want ${expectedLen})`);

function px(x, y) {
  const o = 12 + (y * w + x) * 4;
  return [buf[o], buf[o + 1], buf[o + 2], buf[o + 3]];
}

const samples = {};
for (const [x, y] of COORDS) samples[`${x},${y}`] = px(x, y);

if (mode === "--bless") {
  writeFileSync(goldenPath, JSON.stringify(samples, null, 2) + "\n");
  console.error(`blessed golden -> ${goldenPath}`);
  process.exit(0);
}

if (!existsSync(goldenPath)) fail(`no golden file at ${goldenPath} (run with --bless first)`);
const golden = JSON.parse(readFileSync(goldenPath, "utf8"));
let mismatches = 0;
for (const key of Object.keys(golden)) {
  const got = samples[key];
  const want = golden[key];
  if (!got || JSON.stringify(got) !== JSON.stringify(want)) {
    console.error(`  pixel ${key}: got ${JSON.stringify(got)} want ${JSON.stringify(want)}`);
    mismatches++;
  }
}
if (mismatches) fail(`${mismatches} golden-pixel mismatch(es)`);
console.error(`header OK (${w}x${h}), ${COORDS.length} golden pixels match`);
