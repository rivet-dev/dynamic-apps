"use strict";
import { check, reserve } from "../core/byte-cursor.js";
import { DEV, assert } from "../util/assert.js";
import { IS_LITTLE_ENDIAN_PLATFORM } from "../util/constants.js";
import { isU32 } from "../util/validator.js";
import { readFixedData } from "./data.js";
import {
  readI16,
  readUintSafe32,
  writeI16,
  writeUintSafe32
} from "./primitive.js";
import { writeU8FixedArray } from "./u8-array.js";
export const readI16FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? readI16FixedArrayLe : readI16FixedArrayBe;
export function readI16Array(bc) {
  return readI16FixedArray(bc, readUintSafe32(bc));
}
function readI16FixedArrayLe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  const byteCount = len * 2;
  return new Int16Array(readFixedData(bc, byteCount));
}
function readI16FixedArrayBe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  check(bc, len * 2);
  const result = new Int16Array(len);
  for (let i = 0; i < len; i++) {
    result[i] = readI16(bc);
  }
  return result;
}
export const writeI16FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? writeI16FixedArrayLe : writeI16FixedArrayBe;
export function writeI16Array(bc, x) {
  writeUintSafe32(bc, x.length);
  if (x.length !== 0) {
    writeI16FixedArray(bc, x);
  }
}
function writeI16FixedArrayLe(bc, x) {
  writeU8FixedArray(bc, new Uint8Array(x.buffer, x.byteOffset, x.byteLength));
}
function writeI16FixedArrayBe(bc, x) {
  reserve(bc, x.length * 2);
  for (let i = 0; i < x.length; i++) {
    writeI16(bc, x[i]);
  }
}
