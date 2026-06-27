"use strict";
import { BareError } from "../core/bare-error.js";
import { check, reserve } from "../core/byte-cursor.js";
import { DEV, assert } from "../util/assert.js";
import {
  INVALID_UTF8_STRING,
  TEXT_DECODER_THRESHOLD,
  TEXT_ENCODER_THRESHOLD
} from "../util/constants.js";
import { isU32 } from "../util/validator.js";
import { readUintSafe32, writeUintSafe32 } from "./primitive.js";
import { readUnsafeU8FixedArray, writeU8FixedArray } from "./u8-array.js";
export function readString(bc) {
  return readFixedString(bc, readUintSafe32(bc));
}
export function writeString(bc, x) {
  if (x.length < TEXT_ENCODER_THRESHOLD) {
    const byteLen = utf8ByteLength(x);
    writeUintSafe32(bc, byteLen);
    reserve(bc, byteLen);
    writeUtf8Js(bc, x);
  } else {
    const strBytes = UTF8_ENCODER.encode(x);
    writeUintSafe32(bc, strBytes.length);
    writeU8FixedArray(bc, strBytes);
  }
}
export function readFixedString(bc, byteLen) {
  if (DEV) {
    assert(isU32(byteLen));
  }
  if (byteLen < TEXT_DECODER_THRESHOLD) {
    return readUtf8Js(bc, byteLen);
  }
  try {
    return UTF8_DECODER.decode(readUnsafeU8FixedArray(bc, byteLen));
  } catch (_cause) {
    throw new BareError(bc.offset, INVALID_UTF8_STRING);
  }
}
export function writeFixedString(bc, x) {
  if (x.length < TEXT_ENCODER_THRESHOLD) {
    const byteLen = utf8ByteLength(x);
    reserve(bc, byteLen);
    writeUtf8Js(bc, x);
  } else {
    writeU8FixedArray(bc, UTF8_ENCODER.encode(x));
  }
}
function readUtf8Js(bc, byteLen) {
  check(bc, byteLen);
  let result = "";
  const bytes = bc.bytes;
  let offset = bc.offset;
  const upperOffset = offset + byteLen;
  while (offset < upperOffset) {
    let codePoint = bytes[offset++];
    if (codePoint > 127) {
      let malformed = true;
      const byte1 = codePoint;
      if (offset < upperOffset && codePoint < 224) {
        const byte2 = bytes[offset++];
        codePoint = (byte1 & 31) << 6 | byte2 & 63;
        malformed = codePoint >> 7 === 0 || // non-canonical char
        byte1 >> 5 !== 6 || // invalid tag
        byte2 >> 6 !== 2;
      } else if (offset + 1 < upperOffset && codePoint < 240) {
        const byte2 = bytes[offset++];
        const byte3 = bytes[offset++];
        codePoint = (byte1 & 15) << 12 | (byte2 & 63) << 6 | byte3 & 63;
        malformed = codePoint >> 11 === 0 || // non-canonical char or missing data
        codePoint >> 11 === 27 || // surrogate char (0xD800 <= codePoint <= 0xDFFF)
        byte1 >> 4 !== 14 || // invalid tag
        byte2 >> 6 !== 2 || // invalid tag
        byte3 >> 6 !== 2;
      } else if (offset + 2 < upperOffset) {
        const byte2 = bytes[offset++];
        const byte3 = bytes[offset++];
        const byte4 = bytes[offset++];
        codePoint = (byte1 & 7) << 18 | (byte2 & 63) << 12 | (byte3 & 63) << 6 | byte4 & 63;
        malformed = codePoint >> 16 === 0 || // non-canonical char or missing data
        codePoint > 1114111 || // too large code point
        byte1 >> 3 !== 30 || // invalid tag
        byte2 >> 6 !== 2 || // invalid tag
        byte3 >> 6 !== 2 || // invalid tag
        byte4 >> 6 !== 2;
      }
      if (malformed) {
        throw new BareError(bc.offset, INVALID_UTF8_STRING);
      }
    }
    result += String.fromCodePoint(codePoint);
  }
  bc.offset = offset;
  return result;
}
function writeUtf8Js(bc, s) {
  const bytes = bc.bytes;
  let offset = bc.offset;
  let i = 0;
  while (i < s.length) {
    const codePoint = s.codePointAt(i++);
    if (codePoint < 128) {
      bytes[offset++] = codePoint;
    } else {
      if (codePoint < 2048) {
        bytes[offset++] = 192 | codePoint >> 6;
      } else {
        if (codePoint < 65536) {
          bytes[offset++] = 224 | codePoint >> 12;
        } else {
          bytes[offset++] = 240 | codePoint >> 18;
          bytes[offset++] = 128 | codePoint >> 12 & 63;
          i++;
        }
        bytes[offset++] = 128 | codePoint >> 6 & 63;
      }
      bytes[offset++] = 128 | codePoint & 63;
    }
  }
  bc.offset = offset;
}
function utf8ByteLength(s) {
  let result = s.length;
  for (let i = 0; i < s.length; i++) {
    const codePoint = s.codePointAt(i);
    if (codePoint > 127) {
      result++;
      if (codePoint > 2047) {
        result++;
        if (codePoint > 65535) {
          i++;
        }
      }
    }
  }
  return result;
}
const UTF8_DECODER = /* @__PURE__ */ new TextDecoder("utf-8", { fatal: true });
const UTF8_ENCODER = /* @__PURE__ */ new TextEncoder();
