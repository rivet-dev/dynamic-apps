//! Copyright (c) 2022 Victorien Elvinger
//! Licensed under the MIT License (https://mit-license.org/)
import { check, reserve } from "../core/byte-cursor.js";
import { assert, DEV } from "../util/assert.js";
import { IS_LITTLE_ENDIAN_PLATFORM } from "../util/constants.js";
import { isU32 } from "../util/validator.js";
import { readFixedData } from "./data.js";
import { readF64, writeF64 } from "./fixed-primitive.js";
import { writeU8FixedArray } from "./u8-array.js";
import { readUintSafe32, writeUintSafe32 } from "./uint.js";
export const readF64FixedArray = IS_LITTLE_ENDIAN_PLATFORM
    ? readF64FixedArrayLe
    : readF64FixedArrayBe;
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
export const writeF64FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? writeF64FixedArrayLe : writeF64FixedArrayBe;
function writeF64FixedArrayLe(bc, x) {
    writeU8FixedArray(bc, new Uint8Array(x.buffer, x.byteOffset, x.byteLength));
}
function writeF64FixedArrayBe(bc, x) {
    reserve(bc, x.length * 8);
    for (let i = 0; i < x.length; i++) {
        writeF64(bc, x[i]);
    }
}
export function readF64Array(bc) {
    return readF64FixedArray(bc, readUintSafe32(bc));
}
export function writeF64Array(bc, x) {
    writeUintSafe32(bc, x.length);
    if (x.length > 0) {
        writeF64FixedArray(bc, x);
    }
}
