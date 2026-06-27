"use strict";
import { check, reserve } from "../core/byte-cursor.js";
import { DEV, assert } from "../util/assert.js";
import { IS_LITTLE_ENDIAN_PLATFORM } from "../util/constants.js";
import { isU32 } from "../util/validator.js";
import { readFixedData } from "./data.js";
import {
  readU16,
  readUintSafe32,
  writeU16,
  writeUintSafe32
} from "./primitive.js";
import { writeU8FixedArray } from "./u8-array.js";
export const readU16FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? readU16FixedArrayLe : readU16FixedArrayBe;
export function readU16Array(bc) {
  return readU16FixedArray(bc, readUintSafe32(bc));
}
function readU16FixedArrayLe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  const byteCount = len * 2;
  return new Uint16Array(readFixedData(bc, byteCount));
}
function readU16FixedArrayBe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  check(bc, len * 2);
  const result = new Uint16Array(len);
  for (let i = 0; i < len; i++) {
    result[i] = readU16(bc);
  }
  return result;
}
export const writeU16FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? writeU16FixedArrayLe : writeU16FixedArrayBe;
export function writeU16Array(bc, x) {
  writeUintSafe32(bc, x.length);
  if (x.length !== 0) {
    writeU16FixedArray(bc, x);
  }
}
function writeU16FixedArrayLe(bc, x) {
  writeU8FixedArray(bc, new Uint8Array(x.buffer, x.byteOffset, x.byteLength));
}
function writeU16FixedArrayBe(bc, x) {
  reserve(bc, x.length * 2);
  for (let i = 0; i < x.length; i++) {
    writeU16(bc, x[i]);
  }
}
