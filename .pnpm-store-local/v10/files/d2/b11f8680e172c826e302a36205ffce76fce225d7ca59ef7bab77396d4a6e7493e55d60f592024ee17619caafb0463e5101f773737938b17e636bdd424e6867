import { BareError } from "../core/bare-error.js";
import { assert, DEV } from "../util/assert.js";
import { INT_SAFE_MAX_BYTE_COUNT, TOO_LARGE_NUMBER } from "../util/constants.js";
import { readU8, writeU8 } from "./fixed-primitive.js";
import { readUint, writeUintSafe, writeUintTruncated } from "./uint.js";
export function readInt(bc) {
    const zigZag = readUint(bc);
    return (zigZag >> BigInt(1)) ^ -(zigZag & BigInt(1));
}
export function writeInt(bc, x) {
    // truncate to mimic DataView#setBigInt64
    // this is useful when assertions are skipped
    const truncated = BigInt.asIntN(64, x);
    if (DEV) {
        assert(truncated === x, TOO_LARGE_NUMBER);
    }
    const zigZag = (truncated >> BigInt(63)) ^ (truncated << BigInt(1));
    writeUintTruncated(bc, zigZag);
}
export function readIntSafe(bc) {
    const firstByte = readU8(bc);
    let result = (firstByte & 0x7f) >> 1;
    if (firstByte >= 0x80) {
        let shiftMul = /* 2**6 */ 0x40;
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
            throw new BareError(bc.offset, "must be canonical");
        }
        if (byteCount === INT_SAFE_MAX_BYTE_COUNT &&
            (byte > 0x1f || firstByte === 0xff)) {
            // First byte must not be equal to 0xff in order to exclude -2**53
            // Number.MIN_SAFE_INTEGER equals to -(2**53 - 1)
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
    let first7Bits = ((zigZag & 0x3f) << 1) | sign;
    zigZag = Math.floor(zigZag / /* 2**6 */ 0x40);
    if (zigZag > 0) {
        if (!Number.isSafeInteger(x)) {
            if (DEV) {
                assert(false, TOO_LARGE_NUMBER);
            }
            // keep only the remaining 53 - 6 = 47 bits
            // this is useful when assertions are skipped
            const low = zigZag & 0x7fff;
            const high = ((zigZag / 0x8000) >>> 0) * 0x8000;
            if (first7Bits === 0x7f && low === 0x7fff && high === 4294967295) {
                // maps -2**53 to Number.MIN_SAFE_INTEGER
                // this is useful when assertions are skipped
                first7Bits &= ~0b10;
            }
            zigZag = high + low;
        }
        writeU8(bc, 0x80 | first7Bits);
        writeUintSafe(bc, zigZag);
    }
    else {
        writeU8(bc, first7Bits);
    }
}
