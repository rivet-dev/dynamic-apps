"use strict";
import { DEV, assert } from "../util/assert.js";
import { isU32 } from "../util/validator.js";
import { readFixedData } from "./data.js";
import { readUintSafe, writeUintSafe32 } from "./primitive.js";
import { writeU8FixedArray } from "./u8-array.js";
export function readI8Array(bc) {
  return readI8FixedArray(bc, readUintSafe(bc));
}
export function writeI8Array(bc, x) {
  writeUintSafe32(bc, x.length);
  writeI8FixedArray(bc, x);
}
export function readI8FixedArray(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  return new Int8Array(readFixedData(bc, len));
}
export function writeI8FixedArray(bc, x) {
  writeU8FixedArray(bc, new Uint8Array(x.buffer, x.byteOffset, x.byteLength));
}
