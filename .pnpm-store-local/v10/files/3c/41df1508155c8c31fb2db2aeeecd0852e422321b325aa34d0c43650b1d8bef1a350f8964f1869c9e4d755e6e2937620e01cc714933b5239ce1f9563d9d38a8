//! Copyright (c) 2022 Victorien Elvinger
//! Licensed under the MIT License (https://mit-license.org/)
import { BareError } from "../core/bare-error.js";
import { check, reserve } from "../core/byte-cursor.js";
import { assert, DEV } from "../util/assert.js";
import { TOO_LARGE_NUMBER } from "../util/constants.js";
import { isI8, isI16, isI32, isI64, isU8, isU16, isU32, isU64, isU64Safe, } from "../util/validator.js";
export function readBool(bc) {
    const val = readU8(bc);
    if (val > 1) {
        bc.offset--;
        throw new BareError(bc.offset, "a bool must be equal to 0 or 1");
    }
    return val > 0;
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
        assert(Number.isNaN(x) ||
            Math.abs(bc.view.getFloat32(bc.offset, true) - x) <=
                Number.EPSILON, TOO_LARGE_NUMBER);
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
    const result = readU32(bc) + readI32(bc) * /* 2**32 */ 4294967296;
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
    let highest32 = (x / /* 2**32 */ 4294967296) | 0;
    if (x < 0) {
        // get two's complement representation of the highest 21bits
        highest32 = ~(Math.abs(highest32) & /* 2**21-1 */ 2097151) >>> 0;
        if (lowest32 === 0) {
            if (highest32 === 2097151) {
                // maps -2**53 to Number.MIN_SAFE_INTEGER
                // this is useful when assertions are skipped
                lowest32 = 1;
            }
            else {
                highest32++;
            }
        }
    }
    writeU32(bc, highest32);
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
    const result = readU32(bc) + readU32(bc) * /* 2**32 */ 4294967296;
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
    writeU32(bc, (x / /* 2**32 */ 4294967296) & /* 2**21-1 */ 2097151);
}
