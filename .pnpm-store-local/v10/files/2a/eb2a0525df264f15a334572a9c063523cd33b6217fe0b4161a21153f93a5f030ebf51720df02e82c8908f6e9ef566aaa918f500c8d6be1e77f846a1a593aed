"use strict";
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __export = (target, all) => {
  for (var name in all)
    __defProp(target, name, { get: all[name], enumerable: true });
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

// src/index.ts
var index_exports = {};
__export(index_exports, {
  AssertionError: () => AssertionError,
  BareError: () => BareError,
  ByteCursor: () => ByteCursor,
  Config: () => Config,
  DEFAULT_CONFIG: () => DEFAULT_CONFIG,
  DEV: () => DEV,
  assert: () => assert,
  check: () => check,
  isI16: () => isI16,
  isI32: () => isI32,
  isI64: () => isI64,
  isI8: () => isI8,
  isU16: () => isU16,
  isU32: () => isU32,
  isU64: () => isU64,
  isU64Safe: () => isU64Safe,
  isU8: () => isU8,
  readBool: () => readBool,
  readData: () => readData,
  readF32: () => readF32,
  readF32Array: () => readF32Array,
  readF32FixedArray: () => readF32FixedArray,
  readF64: () => readF64,
  readF64Array: () => readF64Array,
  readF64FixedArray: () => readF64FixedArray,
  readFixedData: () => readFixedData,
  readFixedString: () => readFixedString,
  readI16: () => readI16,
  readI16Array: () => readI16Array,
  readI16FixedArray: () => readI16FixedArray,
  readI32: () => readI32,
  readI32Array: () => readI32Array,
  readI32FixedArray: () => readI32FixedArray,
  readI64: () => readI64,
  readI64Array: () => readI64Array,
  readI64FixedArray: () => readI64FixedArray,
  readI64Safe: () => readI64Safe,
  readI8: () => readI8,
  readI8Array: () => readI8Array,
  readI8FixedArray: () => readI8FixedArray,
  readInt: () => readInt,
  readIntSafe: () => readIntSafe,
  readString: () => readString,
  readU16: () => readU16,
  readU16Array: () => readU16Array,
  readU16FixedArray: () => readU16FixedArray,
  readU32: () => readU32,
  readU32Array: () => readU32Array,
  readU32FixedArray: () => readU32FixedArray,
  readU64: () => readU64,
  readU64Array: () => readU64Array,
  readU64FixedArray: () => readU64FixedArray,
  readU64Safe: () => readU64Safe,
  readU8: () => readU8,
  readU8Array: () => readU8Array,
  readU8ClampedArray: () => readU8ClampedArray,
  readU8ClampedFixedArray: () => readU8ClampedFixedArray,
  readU8FixedArray: () => readU8FixedArray,
  readUint: () => readUint,
  readUintSafe: () => readUintSafe,
  readUintSafe32: () => readUintSafe32,
  readUnsafeU8FixedArray: () => readUnsafeU8FixedArray,
  reserve: () => reserve,
  writeBool: () => writeBool,
  writeData: () => writeData,
  writeF32: () => writeF32,
  writeF32Array: () => writeF32Array,
  writeF32FixedArray: () => writeF32FixedArray,
  writeF64: () => writeF64,
  writeF64Array: () => writeF64Array,
  writeF64FixedArray: () => writeF64FixedArray,
  writeFixedData: () => writeFixedData,
  writeFixedString: () => writeFixedString,
  writeI16: () => writeI16,
  writeI16Array: () => writeI16Array,
  writeI16FixedArray: () => writeI16FixedArray,
  writeI32: () => writeI32,
  writeI32Array: () => writeI32Array,
  writeI32FixedArray: () => writeI32FixedArray,
  writeI64: () => writeI64,
  writeI64Array: () => writeI64Array,
  writeI64FixedArray: () => writeI64FixedArray,
  writeI64Safe: () => writeI64Safe,
  writeI8: () => writeI8,
  writeI8Array: () => writeI8Array,
  writeI8FixedArray: () => writeI8FixedArray,
  writeInt: () => writeInt,
  writeIntSafe: () => writeIntSafe,
  writeString: () => writeString,
  writeU16: () => writeU16,
  writeU16Array: () => writeU16Array,
  writeU16FixedArray: () => writeU16FixedArray,
  writeU32: () => writeU32,
  writeU32Array: () => writeU32Array,
  writeU32FixedArray: () => writeU32FixedArray,
  writeU64: () => writeU64,
  writeU64Array: () => writeU64Array,
  writeU64FixedArray: () => writeU64FixedArray,
  writeU64Safe: () => writeU64Safe,
  writeU8: () => writeU8,
  writeU8Array: () => writeU8Array,
  writeU8ClampedArray: () => writeU8ClampedArray,
  writeU8ClampedFixedArray: () => writeU8ClampedFixedArray,
  writeU8FixedArray: () => writeU8FixedArray,
  writeUint: () => writeUint,
  writeUintSafe: () => writeUintSafe,
  writeUintSafe32: () => writeUintSafe32,
  writeUintTruncated: () => writeUintTruncated
});
module.exports = __toCommonJS(index_exports);

// imports/dev.node.js
var DEV = process.env.NODE_ENV === "development";

// src/util/assert.ts
var V8Error = Error;
function assert(test, message = "") {
  if (!test) {
    const e = new AssertionError(message);
    V8Error.captureStackTrace?.(e, assert);
    throw e;
  }
}
var AssertionError = class extends Error {
  constructor() {
    super(...arguments);
    this.name = "AssertionError";
  }
};

// src/util/validator.ts
function isI8(val) {
  return val === val << 24 >> 24;
}
function isI16(val) {
  return val === val << 16 >> 16;
}
function isI32(val) {
  return val === (val | 0);
}
function isI64(val) {
  return val === BigInt.asIntN(64, val);
}
function isU8(val) {
  return val === (val & 255);
}
function isU16(val) {
  return val === (val & 65535);
}
function isU32(val) {
  return val === val >>> 0;
}
function isU64(val) {
  return val === BigInt.asUintN(64, val);
}
function isU64Safe(val) {
  return Number.isSafeInteger(val) && val >= 0;
}

// src/util/constants.ts
var TEXT_DECODER_THRESHOLD = 256;
var TEXT_ENCODER_THRESHOLD = 256;
var INT_SAFE_MAX_BYTE_COUNT = 8;
var UINT_MAX_BYTE_COUNT = 10;
var UINT_SAFE32_MAX_BYTE_COUNT = 5;
var INVALID_UTF8_STRING = "invalid UTF-8 string";
var NON_CANONICAL_REPRESENTATION = "must be canonical";
var TOO_LARGE_BUFFER = "too large buffer";
var TOO_LARGE_NUMBER = "too large number";
var IS_LITTLE_ENDIAN_PLATFORM = /* @__PURE__ */ new DataView(Uint16Array.of(1).buffer).getUint8(0) === 1;

// src/core/bare-error.ts
var BareError = class extends Error {
  constructor(offset, issue, opts) {
    super(`(byte:${offset}) ${issue}`);
    this.name = "BareError";
    this.issue = issue;
    this.offset = offset;
    this.cause = opts?.cause;
  }
};

// src/core/byte-cursor.ts
var ByteCursor = class {
  /**
   * @throws {BareError} Buffer exceeds `config.maxBufferLength`
   */
  constructor(bytes, config) {
    /**
     * Read and write Offset in {@link view} and {@link bytes}
     */
    this.offset = 0;
    if (bytes.length > config.maxBufferLength) {
      throw new BareError(0, TOO_LARGE_BUFFER);
    }
    this.bytes = bytes;
    this.config = config;
    this.view = new DataView(bytes.buffer, bytes.byteOffset, bytes.length);
  }
};
function check(bc, min) {
  if (DEV) {
    assert(isU32(min));
  }
  if (bc.offset + min > bc.bytes.length) {
    throw new BareError(bc.offset, "missing bytes");
  }
}
function reserve(bc, min) {
  if (DEV) {
    assert(isU32(min));
  }
  const minLen = bc.offset + min | 0;
  if (minLen > bc.bytes.length) {
    grow(bc, minLen);
  }
}
function grow(bc, minLen) {
  if (minLen > bc.config.maxBufferLength) {
    throw new BareError(0, TOO_LARGE_BUFFER);
  }
  const buffer = bc.bytes.buffer;
  let newBytes;
  if (isEs2024ArrayBufferLike(buffer) && // Make sure that the view covers the end of the buffer.
  // If it is not the case, this indicates that the user don't want
  // to override the trailing bytes.
  bc.bytes.byteOffset + bc.bytes.byteLength === buffer.byteLength && bc.bytes.byteLength + minLen <= buffer.maxByteLength) {
    const newLen = Math.min(
      minLen << 1,
      bc.config.maxBufferLength,
      buffer.maxByteLength
    );
    if (buffer instanceof ArrayBuffer) {
      buffer.resize(newLen);
    } else {
      buffer.grow(newLen);
    }
    newBytes = new Uint8Array(buffer, bc.bytes.byteOffset, newLen);
  } else {
    const newLen = Math.min(minLen << 1, bc.config.maxBufferLength);
    newBytes = new Uint8Array(newLen);
    newBytes.set(bc.bytes);
  }
  bc.bytes = newBytes;
  bc.view = new DataView(newBytes.buffer);
}
function isEs2024ArrayBufferLike(buffer) {
  return "maxByteLength" in buffer;
}

// src/codec/fixed-primitive.ts
function readBool(bc) {
  const val = readU8(bc);
  if (val > 1) {
    bc.offset--;
    throw new BareError(bc.offset, "a bool must be equal to 0 or 1");
  }
  return val > 0;
}
function writeBool(bc, x) {
  writeU8(bc, x ? 1 : 0);
}
function readF32(bc) {
  check(bc, 4);
  const result = bc.view.getFloat32(bc.offset, true);
  bc.offset += 4;
  return result;
}
function writeF32(bc, x) {
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
function readF64(bc) {
  check(bc, 8);
  const result = bc.view.getFloat64(bc.offset, true);
  bc.offset += 8;
  return result;
}
function writeF64(bc, x) {
  reserve(bc, 8);
  bc.view.setFloat64(bc.offset, x, true);
  bc.offset += 8;
}
function readI8(bc) {
  check(bc, 1);
  return bc.view.getInt8(bc.offset++);
}
function writeI8(bc, x) {
  if (DEV) {
    assert(isI8(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 1);
  bc.view.setInt8(bc.offset++, x);
}
function readI16(bc) {
  check(bc, 2);
  const result = bc.view.getInt16(bc.offset, true);
  bc.offset += 2;
  return result;
}
function writeI16(bc, x) {
  if (DEV) {
    assert(isI16(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 2);
  bc.view.setInt16(bc.offset, x, true);
  bc.offset += 2;
}
function readI32(bc) {
  check(bc, 4);
  const result = bc.view.getInt32(bc.offset, true);
  bc.offset += 4;
  return result;
}
function writeI32(bc, x) {
  if (DEV) {
    assert(isI32(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 4);
  bc.view.setInt32(bc.offset, x, true);
  bc.offset += 4;
}
function readI64(bc) {
  check(bc, 8);
  const result = bc.view.getBigInt64(bc.offset, true);
  bc.offset += 8;
  return result;
}
function writeI64(bc, x) {
  if (DEV) {
    assert(isI64(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 8);
  bc.view.setBigInt64(bc.offset, x, true);
  bc.offset += 8;
}
function readI64Safe(bc) {
  const result = readU32(bc) + readI32(bc) * /* 2**32 */
  4294967296;
  if (!Number.isSafeInteger(result)) {
    bc.offset -= 8;
    throw new BareError(bc.offset, TOO_LARGE_NUMBER);
  }
  return result;
}
function writeI64Safe(bc, x) {
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
function readU8(bc) {
  check(bc, 1);
  return bc.bytes[bc.offset++];
}
function writeU8(bc, x) {
  if (DEV) {
    assert(isU8(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 1);
  bc.bytes[bc.offset++] = x;
}
function readU16(bc) {
  check(bc, 2);
  const result = bc.view.getUint16(bc.offset, true);
  bc.offset += 2;
  return result;
}
function writeU16(bc, x) {
  if (DEV) {
    assert(isU16(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 2);
  bc.view.setUint16(bc.offset, x, true);
  bc.offset += 2;
}
function readU32(bc) {
  check(bc, 4);
  const result = bc.view.getUint32(bc.offset, true);
  bc.offset += 4;
  return result;
}
function writeU32(bc, x) {
  if (DEV) {
    assert(isU32(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 4);
  bc.view.setUint32(bc.offset, x, true);
  bc.offset += 4;
}
function readU64(bc) {
  check(bc, 8);
  const result = bc.view.getBigUint64(bc.offset, true);
  bc.offset += 8;
  return result;
}
function writeU64(bc, x) {
  if (DEV) {
    assert(isU64(x), TOO_LARGE_NUMBER);
  }
  reserve(bc, 8);
  bc.view.setBigUint64(bc.offset, x, true);
  bc.offset += 8;
}
function readU64Safe(bc) {
  const result = readU32(bc) + readU32(bc) * /* 2**32 */
  4294967296;
  if (!isU64Safe(result)) {
    bc.offset -= 8;
    throw new BareError(bc.offset, TOO_LARGE_NUMBER);
  }
  return result;
}
function writeU64Safe(bc, x) {
  if (DEV) {
    assert(isU64Safe(x), TOO_LARGE_NUMBER);
  }
  writeU32(bc, x >>> 0);
  writeU32(bc, x / /* 2**32 */
  4294967296 & /* 2**21-1 */
  2097151);
}

// src/codec/uint.ts
function readUint(bc) {
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
function writeUint(bc, x) {
  const truncated = BigInt.asUintN(64, x);
  if (DEV) {
    assert(truncated === x, TOO_LARGE_NUMBER);
  }
  writeUintTruncated(bc, truncated);
}
function writeUintTruncated(bc, x) {
  let tmp = Number(BigInt.asUintN(7 * 7, x));
  let rest = Number(x >> BigInt(7 * 7));
  let byteCount = 0;
  while (tmp >= 128 || rest > 0) {
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
function readUintSafe32(bc) {
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
function writeUintSafe32(bc, x) {
  if (DEV) {
    assert(isU32(x), TOO_LARGE_NUMBER);
  }
  let zigZag = x >>> 0;
  while (zigZag >= 128) {
    writeU8(bc, 128 | zigZag & 127);
    zigZag >>>= 7;
  }
  writeU8(bc, zigZag);
}
function readUintSafe(bc) {
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
function writeUintSafe(bc, x) {
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

// src/codec/u8-array.ts
function readU8Array(bc) {
  return readU8FixedArray(bc, readUintSafe32(bc));
}
function writeU8Array(bc, x) {
  writeUintSafe32(bc, x.length);
  writeU8FixedArray(bc, x);
}
function readU8FixedArray(bc, len) {
  return readUnsafeU8FixedArray(bc, len).slice();
}
function writeU8FixedArray(bc, x) {
  const len = x.length;
  if (len > 0) {
    reserve(bc, len);
    bc.bytes.set(x, bc.offset);
    bc.offset += len;
  }
}
function readUnsafeU8FixedArray(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  check(bc, len);
  const offset = bc.offset;
  bc.offset += len;
  return bc.bytes.subarray(offset, offset + len);
}

// src/codec/data.ts
function readData(bc) {
  return readU8Array(bc).buffer;
}
function writeData(bc, x) {
  writeU8Array(bc, new Uint8Array(x));
}
function readFixedData(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  return readU8FixedArray(bc, len).buffer;
}
function writeFixedData(bc, x) {
  writeU8FixedArray(bc, new Uint8Array(x));
}

// src/codec/f32-array.ts
var readF32FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? readF32FixedArrayLe : readF32FixedArrayBe;
function readF32FixedArrayLe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  const byteLen = len * 4;
  const result = new Float32Array(readFixedData(bc, byteLen));
  return result;
}
function readF32FixedArrayBe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  check(bc, len * 4);
  const result = new Float32Array(len);
  for (let i = 0; i < len; i++) {
    result[i] = readF32(bc);
  }
  return result;
}
var writeF32FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? writeF32FixedArrayLe : writeF32FixedArrayBe;
function writeF32FixedArrayLe(bc, x) {
  writeU8FixedArray(bc, new Uint8Array(x.buffer, x.byteOffset, x.byteLength));
}
function writeF32FixedArrayBe(bc, val) {
  reserve(bc, val.length * 4);
  for (let i = 0; i < val.length; i++) {
    writeF32(bc, val[i]);
  }
}
function readF32Array(bc) {
  return readF32FixedArray(bc, readUintSafe32(bc));
}
function writeF32Array(bc, x) {
  writeUintSafe32(bc, x.length);
  if (x.length > 0) {
    writeF32FixedArray(bc, x);
  }
}

// src/codec/f64-array.ts
var readF64FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? readF64FixedArrayLe : readF64FixedArrayBe;
function readF64FixedArrayLe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  const byteLen = len * 8;
  const result = new Float64Array(readFixedData(bc, byteLen));
  return result;
}
function readF64FixedArrayBe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  check(bc, len * 8);
  const result = new Float64Array(len);
  for (let i = 0; i < len; i++) {
    result[i] = readF64(bc);
  }
  return result;
}
var writeF64FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? writeF64FixedArrayLe : writeF64FixedArrayBe;
function writeF64FixedArrayLe(bc, x) {
  writeU8FixedArray(bc, new Uint8Array(x.buffer, x.byteOffset, x.byteLength));
}
function writeF64FixedArrayBe(bc, x) {
  reserve(bc, x.length * 8);
  for (let i = 0; i < x.length; i++) {
    writeF64(bc, x[i]);
  }
}
function readF64Array(bc) {
  return readF64FixedArray(bc, readUintSafe32(bc));
}
function writeF64Array(bc, x) {
  writeUintSafe32(bc, x.length);
  if (x.length > 0) {
    writeF64FixedArray(bc, x);
  }
}

// src/codec/i8-array.ts
function readI8Array(bc) {
  return readI8FixedArray(bc, readUintSafe(bc));
}
function writeI8Array(bc, x) {
  writeUintSafe32(bc, x.length);
  writeI8FixedArray(bc, x);
}
function readI8FixedArray(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  return new Int8Array(readFixedData(bc, len));
}
function writeI8FixedArray(bc, x) {
  writeU8FixedArray(bc, new Uint8Array(x.buffer, x.byteOffset, x.byteLength));
}

// src/codec/i16-array.ts
var readI16FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? readI16FixedArrayLe : readI16FixedArrayBe;
function readI16Array(bc) {
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
var writeI16FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? writeI16FixedArrayLe : writeI16FixedArrayBe;
function writeI16Array(bc, x) {
  writeUintSafe32(bc, x.length);
  if (x.length > 0) {
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

// src/codec/i32-array.ts
var readI32FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? readI32FixedArrayLe : readI32FixedArrayBe;
function readI32Array(bc) {
  return readI32FixedArray(bc, readUintSafe32(bc));
}
function readI32FixedArrayLe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  const byteCount = len * 4;
  return new Int32Array(readFixedData(bc, byteCount));
}
function readI32FixedArrayBe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  check(bc, len * 4);
  const result = new Int32Array(len);
  for (let i = 0; i < len; i++) {
    result[i] = readI32(bc);
  }
  return result;
}
var writeI32FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? writeI32FixedArrayLe : writeI32FixedArrayBe;
function writeI32Array(bc, x) {
  writeUintSafe32(bc, x.length);
  if (x.length > 0) {
    writeI32FixedArray(bc, x);
  }
}
function writeI32FixedArrayLe(bc, x) {
  writeU8FixedArray(bc, new Uint8Array(x.buffer, x.byteOffset, x.byteLength));
}
function writeI32FixedArrayBe(bc, x) {
  reserve(bc, x.length * 4);
  for (let i = 0; i < x.length; i++) {
    writeI32(bc, x[i]);
  }
}

// src/codec/i64-array.ts
var readI64FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? readI64FixedArrayLe : readI64FixedArrayBe;
function readI64Array(bc) {
  return readI64FixedArray(bc, readUintSafe32(bc));
}
function readI64FixedArrayLe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  const byteCount = len * 8;
  return new BigInt64Array(readFixedData(bc, byteCount));
}
function readI64FixedArrayBe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  check(bc, len * 8);
  const result = new BigInt64Array(len);
  for (let i = 0; i < len; i++) {
    result[i] = readI64(bc);
  }
  return result;
}
var writeI64FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? writeI64FixedArrayLe : writeI64FixedArrayBe;
function writeI64Array(bc, x) {
  writeUintSafe32(bc, x.length);
  if (x.length > 0) {
    writeI64FixedArray(bc, x);
  }
}
function writeI64FixedArrayLe(bc, x) {
  writeU8FixedArray(bc, new Uint8Array(x.buffer, x.byteOffset, x.byteLength));
}
function writeI64FixedArrayBe(bc, x) {
  reserve(bc, x.length * 8);
  for (let i = 0; i < x.length; i++) {
    writeI64(bc, x[i]);
  }
}

// src/codec/int.ts
function readInt(bc) {
  const zigZag = readUint(bc);
  return zigZag >> BigInt(1) ^ -(zigZag & BigInt(1));
}
function writeInt(bc, x) {
  const truncated = BigInt.asIntN(64, x);
  if (DEV) {
    assert(truncated === x, TOO_LARGE_NUMBER);
  }
  const zigZag = truncated >> BigInt(63) ^ truncated << BigInt(1);
  writeUintTruncated(bc, zigZag);
}
function readIntSafe(bc) {
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
function writeIntSafe(bc, x) {
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

// src/codec/string.ts
function readString(bc) {
  return readFixedString(bc, readUintSafe32(bc));
}
function writeString(bc, x) {
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
function readFixedString(bc, byteLen) {
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
function writeFixedString(bc, x) {
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
var UTF8_DECODER = /* @__PURE__ */ new TextDecoder("utf-8", { fatal: true });
var UTF8_ENCODER = /* @__PURE__ */ new TextEncoder();

// src/codec/u8-clamped-array.ts
function readU8ClampedArray(bc) {
  return readU8ClampedFixedArray(bc, readUintSafe32(bc));
}
function writeU8ClampedArray(bc, x) {
  writeUintSafe32(bc, x.length);
  writeU8ClampedFixedArray(bc, x);
}
function readU8ClampedFixedArray(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  return new Uint8ClampedArray(readFixedData(bc, len));
}
function writeU8ClampedFixedArray(bc, x) {
  writeU8FixedArray(bc, new Uint8Array(x.buffer, x.byteOffset, x.byteLength));
}

// src/codec/u16-array.ts
var readU16FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? readU16FixedArrayLe : readU16FixedArrayBe;
function readU16Array(bc) {
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
var writeU16FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? writeU16FixedArrayLe : writeU16FixedArrayBe;
function writeU16Array(bc, x) {
  writeUintSafe32(bc, x.length);
  if (x.length > 0) {
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

// src/codec/u32-array.ts
var readU32FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? readU32FixedArrayLe : readU32FixedArrayBe;
function readU32Array(bc) {
  return readU32FixedArray(bc, readUintSafe32(bc));
}
function readU32FixedArrayLe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  const byteCount = len * 4;
  return new Uint32Array(readFixedData(bc, byteCount));
}
function readU32FixedArrayBe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  check(bc, len * 4);
  const result = new Uint32Array(len);
  for (let i = 0; i < len; i++) {
    result[i] = readU32(bc);
  }
  return result;
}
var writeU32FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? writeU32FixedArrayLe : writeU32FixedArrayBe;
function writeU32Array(bc, x) {
  writeUintSafe32(bc, x.length);
  if (x.length > 0) {
    writeU32FixedArray(bc, x);
  }
}
function writeU32FixedArrayLe(bc, x) {
  writeU8FixedArray(bc, new Uint8Array(x.buffer, x.byteOffset, x.byteLength));
}
function writeU32FixedArrayBe(bc, x) {
  reserve(bc, x.length * 4);
  for (let i = 0; i < x.length; i++) {
    writeU32(bc, x[i]);
  }
}

// src/codec/u64-array.ts
var readU64FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? readU64FixedArrayLe : readU64FixedArrayBe;
function readU64Array(bc) {
  return readU64FixedArray(bc, readUintSafe32(bc));
}
function readU64FixedArrayLe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  const byteCount = len * 8;
  return new BigUint64Array(readFixedData(bc, byteCount));
}
function readU64FixedArrayBe(bc, len) {
  if (DEV) {
    assert(isU32(len));
  }
  check(bc, len * 8);
  const result = new BigUint64Array(len);
  for (let i = 0; i < len; i++) {
    result[i] = readU64(bc);
  }
  return result;
}
var writeU64FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? writeU64FixedArrayLe : writeU64FixedArrayBe;
function writeU64Array(bc, x) {
  writeUintSafe32(bc, x.length);
  if (x.length > 0) {
    writeU64FixedArray(bc, x);
  }
}
function writeU64FixedArrayLe(bc, x) {
  writeU8FixedArray(bc, new Uint8Array(x.buffer, x.byteOffset, x.byteLength));
}
function writeU64FixedArrayBe(bc, x) {
  reserve(bc, x.length * 8);
  for (let i = 0; i < x.length; i++) {
    writeU64(bc, x[i]);
  }
}

// src/core/config.ts
function Config({
  initialBufferLength = 1024,
  maxBufferLength = 1024 * 1024 * 32
}) {
  if (DEV) {
    assert(isU32(initialBufferLength), TOO_LARGE_NUMBER);
    assert(isU32(maxBufferLength), TOO_LARGE_NUMBER);
    assert(
      initialBufferLength <= maxBufferLength,
      "initialBufferLength must be lower than or equal to maxBufferLength"
    );
  }
  return {
    initialBufferLength,
    maxBufferLength
  };
}
var DEFAULT_CONFIG = /* @__PURE__ */ Config({});
// Annotate the CommonJS export names for ESM import in node:
0 && (module.exports = {
  AssertionError,
  BareError,
  ByteCursor,
  Config,
  DEFAULT_CONFIG,
  DEV,
  assert,
  check,
  isI16,
  isI32,
  isI64,
  isI8,
  isU16,
  isU32,
  isU64,
  isU64Safe,
  isU8,
  readBool,
  readData,
  readF32,
  readF32Array,
  readF32FixedArray,
  readF64,
  readF64Array,
  readF64FixedArray,
  readFixedData,
  readFixedString,
  readI16,
  readI16Array,
  readI16FixedArray,
  readI32,
  readI32Array,
  readI32FixedArray,
  readI64,
  readI64Array,
  readI64FixedArray,
  readI64Safe,
  readI8,
  readI8Array,
  readI8FixedArray,
  readInt,
  readIntSafe,
  readString,
  readU16,
  readU16Array,
  readU16FixedArray,
  readU32,
  readU32Array,
  readU32FixedArray,
  readU64,
  readU64Array,
  readU64FixedArray,
  readU64Safe,
  readU8,
  readU8Array,
  readU8ClampedArray,
  readU8ClampedFixedArray,
  readU8FixedArray,
  readUint,
  readUintSafe,
  readUintSafe32,
  readUnsafeU8FixedArray,
  reserve,
  writeBool,
  writeData,
  writeF32,
  writeF32Array,
  writeF32FixedArray,
  writeF64,
  writeF64Array,
  writeF64FixedArray,
  writeFixedData,
  writeFixedString,
  writeI16,
  writeI16Array,
  writeI16FixedArray,
  writeI32,
  writeI32Array,
  writeI32FixedArray,
  writeI64,
  writeI64Array,
  writeI64FixedArray,
  writeI64Safe,
  writeI8,
  writeI8Array,
  writeI8FixedArray,
  writeInt,
  writeIntSafe,
  writeString,
  writeU16,
  writeU16Array,
  writeU16FixedArray,
  writeU32,
  writeU32Array,
  writeU32FixedArray,
  writeU64,
  writeU64Array,
  writeU64FixedArray,
  writeU64Safe,
  writeU8,
  writeU8Array,
  writeU8ClampedArray,
  writeU8ClampedFixedArray,
  writeU8FixedArray,
  writeUint,
  writeUintSafe,
  writeUintSafe32,
  writeUintTruncated
});
//! Copyright (c) 2022 Victorien Elvinger
//! Licensed under the MIT License (https://mit-license.org/)
