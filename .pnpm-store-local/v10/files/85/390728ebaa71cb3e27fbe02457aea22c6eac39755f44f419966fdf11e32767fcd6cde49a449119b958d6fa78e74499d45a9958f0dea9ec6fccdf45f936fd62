"use strict";
import { BareError } from "../core/bare-error.js";
import { check, reserve } from "../core/byte-cursor.js";
import { DEV, assert } from "../util/assert.js";
import {
  INT_SAFE_MAX_BYTE_COUNT,
  NON_CANONICAL_REPRESENTATION,
  TOO_LARGE_NUMBER,
  UINT_MAX_BYTE_COUNT,
  UINT_SAFE32_MAX_BYTE_COUNT
} from "../util/constants.js";
import {
  isI8,
  isI16,
  isI32,
  isI64,
  isU8,
  isU16,
  isU32,
  isU64,
  isU64Safe
} from "../util/validator.js";
export function readBool(bc) {
  const val = readU8(bc);
  if (val > 1) {
    bc.offset--;
    throw new BareError(bc.offset, "a bool must be equal to 0 or 1");
  }
  return val !== 0;
}
export function writeBool(bc, x) {
  writeU8(bc, x ? 1 : 0);
}
export function readF32(bc) {
  check(bc, 4);
  const result = bc.view.getFloat32(bc.offset, true);
  bc.offset += 4;
  return result;
}
export function writeF32(bc, x) {
  reserve(bc, 4);
  bc.view.setFloat32(bc.offset, x, true);
  if (DEV) {
    assert(
      Number.isNaN(x) || Math.abs(bc.view.getFloat32(bc.offset, true) - x) <= Number.EPSILON,
      TOO_LARGE_NUMBER
    );
  }
  bc.offset += 4;
}
export function readF64(bc) {
  check(bc, 8);
  const result = bc.view.getFloat64(bc.offset, true);
  bc.offset += 8;
  return result;
}
export function writeF64(bc, x) {
  reserve(bc, 8);
  bc.view.setFloat64(bc.offset, x, true);
  bc.offset += 8;
}
export function readI8(bc) {
  check(bc, 1);
  return bc.view.getInt8(bc.offset++);
}
export function writeI8(bc, x) {
  if (DEV) {
    assert(isI8(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 1);
  bc.view.setInt8(bc.offset++, x);
}
export function readI16(bc) {
  check(bc, 2);
  const result = bc.view.getInt16(bc.offset, true);
  bc.offset += 2;
  return result;
}
export function writeI16(bc, x) {
  if (DEV) {
    assert(isI16(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 2);
  bc.view.setInt16(bc.offset, x, true);
  bc.offset += 2;
}
export function readI32(bc) {
  check(bc, 4);
  const result = bc.view.getInt32(bc.offset, true);
  bc.offset += 4;
  return result;
}
export function writeI32(bc, x) {
  if (DEV) {
    assert(isI32(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 4);
  bc.view.setInt32(bc.offset, x, true);
  bc.offset += 4;
}
export function readI64(bc) {
  check(bc, 8);
  const result = bc.view.getBigInt64(bc.offset, true);
  bc.offset += 8;
  return result;
}
export function writeI64(bc, x) {
  if (DEV) {
    assert(isI64(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 8);
  bc.view.setBigInt64(bc.offset, x, true);
  bc.offset += 8;
}
export function readI64Safe(bc) {
  const result = readU32(bc) + readI32(bc) * /* 2**32 */
  4294967296;
  if (!Number.isSafeInteger(result)) {
    bc.offset -= 8;
    throw new BareError(bc.offset, TOO_LARGE_NUMBER);
  }
  return result;
}
export function writeI64Safe(bc, x) {
  if (DEV) {
    assert(Number.isSafeInteger(x), TOO_LARGE_NUMBER);
  }
  let lowest32 = x >>> 0;
  writeU32(bc, lowest32);
  let highest32 = x / /* 2**32 */
  4294967296 | 0;
  if (x < 0) {
    highest32 = ~(Math.abs(highest32) & /* 2**21-1 */
    2097151) >>> 0;
    if (lowest32 === 0) {
      if (highest32 === 2097151) {
        lowest32 = 1;
      } else {
        highest32++;
      }
    }
  }
  writeU32(bc, highest32);
}
export function readInt(bc) {
  const zigZag = readUint(bc);
  return zigZag >> BigInt(1) ^ -(zigZag & BigInt(1));
}
export function writeInt(bc, x) {
  const truncated = BigInt.asIntN(64, x);
  if (DEV) {
    assert(truncated === x, TOO_LARGE_NUMBER);
  }
  const zigZag = truncated >> BigInt(63) ^ truncated << BigInt(1);
  writeTruncatedUint(bc, zigZag);
}
export function readIntSafe(bc) {
  const firstByte = readU8(bc);
  let result = (firstByte & 127) >> 1;
  if (firstByte >= 128) {
    let shiftMul = (
      /* 2**6 */
      64
    );
    let byteCount = 1;
    let byte;
    do {
      byte = readU8(bc);
      result += (byte & 127) * shiftMul;
      shiftMul *= /* 2**7 */
      128;
      byteCount++;
    } while (byte >= 128 && byteCount < INT_SAFE_MAX_BYTE_COUNT);
    if (byte === 0) {
      bc.offset -= byteCount - 1;
      throw new BareError(bc.offset, "must be canonical");
    }
    if (byteCount === INT_SAFE_MAX_BYTE_COUNT && (byte > 31 || firstByte === 255)) {
      bc.offset -= byteCount - 1;
      throw new BareError(bc.offset, TOO_LARGE_NUMBER);
    }
  }
  const isNeg = (firstByte & 1) === 1;
  if (isNeg) {
    result = -result - 1;
  }
  return result;
}
export function writeIntSafe(bc, x) {
  const sign = x < 0 ? 1 : 0;
  let zigZag = x < 0 ? -(x + 1) : x;
  let first7Bits = (zigZag & 63) << 1 | sign;
  zigZag = Math.floor(zigZag / /* 2**6 */
  64);
  if (zigZag > 0) {
    if (!Number.isSafeInteger(x)) {
      if (DEV) {
        assert(false, TOO_LARGE_NUMBER);
      }
      const low = zigZag & 32767;
      const high = (zigZag / 32768 >>> 0) * 32768;
      if (first7Bits === 127 && low === 32767 && high === 4294967295) {
        first7Bits &= ~2;
      }
      zigZag = high + low;
    }
    writeU8(bc, 128 | first7Bits);
    writeUintSafe(bc, zigZag);
  } else {
    writeU8(bc, first7Bits);
  }
}
export function readU8(bc) {
  check(bc, 1);
  return bc.bytes[bc.offset++];
}
export function writeU8(bc, x) {
  if (DEV) {
    assert(isU8(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 1);
  bc.bytes[bc.offset++] = x;
}
export function readU16(bc) {
  check(bc, 2);
  const result = bc.view.getUint16(bc.offset, true);
  bc.offset += 2;
  return result;
}
export function writeU16(bc, x) {
  if (DEV) {
    assert(isU16(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 2);
  bc.view.setUint16(bc.offset, x, true);
  bc.offset += 2;
}
export function readU32(bc) {
  check(bc, 4);
  const result = bc.view.getUint32(bc.offset, true);
  bc.offset += 4;
  return result;
}
export function writeU32(bc, x) {
  if (DEV) {
    assert(isU32(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 4);
  bc.view.setUint32(bc.offset, x, true);
  bc.offset += 4;
}
export function readU64(bc) {
  check(bc, 8);
  const result = bc.view.getBigUint64(bc.offset, true);
  bc.offset += 8;
  return result;
}
export function writeU64(bc, x) {
  if (DEV) {
    assert(isU64(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 8);
  bc.view.setBigUint64(bc.offset, x, true);
  bc.offset += 8;
}
export function readU64Safe(bc) {
  const result = readU32(bc) + readU32(bc) * /* 2**32 */
  4294967296;
  if (!isU64Safe(result)) {
    bc.offset -= 8;
    throw new BareError(bc.offset, TOO_LARGE_NUMBER);
  }
  return result;
}
export function writeU64Safe(bc, x) {
  if (DEV) {
    assert(isU64Safe(x), TOO_LARGE_NUMBER);
  }
  writeU32(bc, x >>> 0);
  writeU32(bc, x / /* 2**32 */
  4294967296 & /* 2**21-1 */
  2097151);
}
export function readUint(bc) {
  let low = readU8(bc);
  if (low >= 128) {
    low &= 127;
    let shiftMul = 128;
    let byteCount = 1;
    let byte;
    do {
      byte = readU8(bc);
      low += (byte & 127) * shiftMul;
      shiftMul *= /* 2**7 */
      128;
      byteCount++;
    } while (byte >= 128 && byteCount < 7);
    let height = 0;
    shiftMul = 1;
    while (byte >= 128 && byteCount < UINT_MAX_BYTE_COUNT) {
      byte = readU8(bc);
      height += (byte & 127) * shiftMul;
      shiftMul *= /* 2**7 */
      128;
      byteCount++;
    }
    if (byte === 0 || byteCount === UINT_MAX_BYTE_COUNT && byte > 1) {
      bc.offset -= byteCount;
      throw new BareError(bc.offset, NON_CANONICAL_REPRESENTATION);
    }
    return BigInt(low) + (BigInt(height) << BigInt(7 * 7));
  }
  return BigInt(low);
}
export function writeUint(bc, x) {
  const truncated = BigInt.asUintN(64, x);
  if (DEV) {
    assert(truncated === x, TOO_LARGE_NUMBER);
  }
  writeTruncatedUint(bc, truncated);
}
function writeTruncatedUint(bc, x) {
  let tmp = Number(BigInt.asUintN(7 * 7, x));
  let rest = Number(x >> BigInt(7 * 7));
  let byteCount = 0;
  while (tmp >= 128 || rest !== 0) {
    writeU8(bc, 128 | tmp & 127);
    tmp = Math.floor(tmp / /* 2**7 */
    128);
    byteCount++;
    if (byteCount === 7) {
      tmp = rest;
      rest = 0;
    }
  }
  writeU8(bc, tmp);
}
export function readUintSafe32(bc) {
  let result = readU8(bc);
  if (result >= 128) {
    result &= 127;
    let shift = 7;
    let byteCount = 1;
    let byte;
    do {
      byte = readU8(bc);
      result += (byte & 127) << shift >>> 0;
      shift += 7;
      byteCount++;
    } while (byte >= 128 && byteCount < UINT_SAFE32_MAX_BYTE_COUNT);
    if (byte === 0) {
      bc.offset -= byteCount - 1;
      throw new BareError(
        bc.offset - byteCount + 1,
        NON_CANONICAL_REPRESENTATION
      );
    }
    if (byteCount === UINT_SAFE32_MAX_BYTE_COUNT && byte > 15) {
      bc.offset -= byteCount - 1;
      throw new BareError(bc.offset, TOO_LARGE_NUMBER);
    }
  }
  return result;
}
export function writeUintSafe32(bc, x) {
  if (DEV) {
    assert(isU32(x), TOO_LARGE_NUMBER);
  }
  let zigZag = x >>> 0;
  while (zigZag >= 128) {
    writeU8(bc, 128 | x & 127);
    zigZag >>>= 7;
  }
  writeU8(bc, zigZag);
}
export function readUintSafe(bc) {
  let result = readU8(bc);
  if (result >= 128) {
    result &= 127;
    let shiftMul = (
      /* 2**7 */
      128
    );
    let byteCount = 1;
    let byte;
    do {
      byte = readU8(bc);
      result += (byte & 127) * shiftMul;
      shiftMul *= /* 2**7 */
      128;
      byteCount++;
    } while (byte >= 128 && byteCount < INT_SAFE_MAX_BYTE_COUNT);
    if (byte === 0) {
      bc.offset -= byteCount - 1;
      throw new BareError(
        bc.offset - byteCount + 1,
        NON_CANONICAL_REPRESENTATION
      );
    }
    if (byteCount === INT_SAFE_MAX_BYTE_COUNT && byte > 15) {
      bc.offset -= byteCount - 1;
      throw new BareError(bc.offset, TOO_LARGE_NUMBER);
    }
  }
  return result;
}
export function writeUintSafe(bc, x) {
  if (DEV) {
    assert(isU64Safe(x), TOO_LARGE_NUMBER);
  }
  let byteCount = 1;
  let zigZag = x;
  while (zigZag >= 128 && byteCount < INT_SAFE_MAX_BYTE_COUNT) {
    writeU8(bc, 128 | zigZag & 127);
    zigZag = Math.floor(zigZag / /* 2**7 */
    128);
    byteCount++;
  }
  if (byteCount === INT_SAFE_MAX_BYTE_COUNT) {
    zigZag &= 15;
  }
  writeU8(bc, zigZag);
}
