"use strict";
import { DEV, assert } from "../util/assert.js";
import { isU32 } from "../util/validator.js";
import { readFixedData } from "./data.js";
import { readUintSafe32, writeUintSafe32 } from "./primitive.js";
import { writeU8FixedArray } from "./u8-array.js";
export function readU8ClampedArray(bc) {
  return readU8ClampedFixedArray(bc, readUintSafe32(bc));
}
export function writeU8ClampedArray(bc, x) {
  writeUintSafe32(bc, x.length);
  writeU8ClampedFixedArray(bc, x);
}
export function readU8ClampedFixedArray(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  return new Uint8ClampedArray(readFixedData(bc, len));
}
export function writeU8ClampedFixedArray(bc, x) {
  writeU8FixedArray(bc, new Uint8Array(x.buffer, x.byteOffset, x.byteLength));
}
