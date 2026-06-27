import { BareError } from "../core/bare-error.js";
import { assert, DEV } from "../util/assert.js";
import { INT_SAFE_MAX_BYTE_COUNT, NON_CANONICAL_REPRESENTATION, TOO_LARGE_NUMBER, UINT_MAX_BYTE_COUNT, UINT_SAFE32_MAX_BYTE_COUNT, } from "../util/constants.js";
import { isU32, isU64Safe } from "../util/validator.js";
import { readU8, writeU8 } from "./fixed-primitive.js";
export function readUint(bc) {
    let low = readU8(bc);
    if (low >= 0x80) {
        low &= 0x7f;
        let shiftMul = 0x80;
        let byteCount = 1;
        let byte;
        do {
            byte = readU8(bc);
            low += (byte & 0x7f) * shiftMul;
            shiftMul *= /* 2**7 */ 0x80;
            byteCount++;
        } while (byte >= 0x80 && byteCount < 7);
        let height = 0;
        shiftMul = 1;
        while (byte >= 0x80 && byteCount < UINT_MAX_BYTE_COUNT) {
            byte = readU8(bc);
            height += (byte & 0x7f) * shiftMul;
            shiftMul *= /* 2**7 */ 0x80;
            byteCount++;
        }
        if (byte === 0 || (byteCount === UINT_MAX_BYTE_COUNT && byte > 1)) {
            bc.offset -= byteCount;
            throw new BareError(bc.offset, NON_CANONICAL_REPRESENTATION);
        }
        return BigInt(low) + (BigInt(height) << BigInt(7 * 7));
    }
    return BigInt(low);
}
export function writeUint(bc, x) {
    // truncate to mimic DataView#setBigUint64
    // this is useful when assertions are skipped
    const truncated = BigInt.asUintN(64, x);
    if (DEV) {
        assert(truncated === x, TOO_LARGE_NUMBER);
    }
    writeUintTruncated(bc, truncated);
}
export function writeUintTruncated(bc, x) {
    // For better performances, we decompose `x` into two safe uint.
    let tmp = Number(BigInt.asUintN(7 * 7, x));
    let rest = Number(x >> BigInt(7 * 7));
    let byteCount = 0;
    while (tmp >= 0x80 || rest > 0) {
        writeU8(bc, 0x80 | (tmp & 0x7f));
        tmp = Math.floor(tmp / /* 2**7 */ 0x80);
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
    if (result >= 0x80) {
        result &= 0x7f;
        let shift = 7;
        let byteCount = 1;
        let byte;
        do {
            byte = readU8(bc);
            result += ((byte & 0x7f) << shift) >>> 0;
            shift += 7;
            byteCount++;
        } while (byte >= 0x80 && byteCount < UINT_SAFE32_MAX_BYTE_COUNT);
        if (byte === 0) {
            bc.offset -= byteCount - 1;
            throw new BareError(bc.offset - byteCount + 1, NON_CANONICAL_REPRESENTATION);
        }
        if (byteCount === UINT_SAFE32_MAX_BYTE_COUNT && byte > 0xf) {
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
    // truncate to mimic other int encoders
    // this is useful when assertions are skipped
    let zigZag = x >>> 0;
    while (zigZag >= 0x80) {
        writeU8(bc, 0x80 | (zigZag & 0x7f));
        zigZag >>>= 7;
    }
    writeU8(bc, zigZag);
}
export function readUintSafe(bc) {
    let result = readU8(bc);
    if (result >= 0x80) {
        result &= 0x7f;
        let shiftMul = /* 2**7 */ 0x80;
        let byteCount = 1;
        let byte;
        do {
            byte = readU8(bc);
            result += (byte & 0x7f) * shiftMul;
            shiftMul *= /* 2**7 */ 0x80;
            byteCount++;
        } while (byte >= 0x80 && byteCount < INT_SAFE_MAX_BYTE_COUNT);
        if (byte === 0) {
            bc.offset -= byteCount - 1;
            throw new BareError(bc.offset - byteCount + 1, NON_CANONICAL_REPRESENTATION);
        }
        if (byteCount === INT_SAFE_MAX_BYTE_COUNT && byte > 0xf) {
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
    while (zigZag >= 0x80 && byteCount < INT_SAFE_MAX_BYTE_COUNT) {
        writeU8(bc, 0x80 | (zigZag & 0x7f));
        zigZag = Math.floor(zigZag / /* 2**7 */ 0x80);
        byteCount++;
    }
    if (byteCount === INT_SAFE_MAX_BYTE_COUNT) {
        // truncate to mimic other int encoders
        // this is useful when assertions are skipped
        zigZag &= 0x0f;
    }
    writeU8(bc, zigZag);
}
