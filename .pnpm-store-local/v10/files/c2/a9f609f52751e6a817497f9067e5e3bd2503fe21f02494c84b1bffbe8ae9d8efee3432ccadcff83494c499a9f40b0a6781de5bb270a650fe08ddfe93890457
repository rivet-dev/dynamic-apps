//! Copyright (c) 2022 Victorien Elvinger
//! Licensed under the MIT License (https://mit-license.org/)
import { check, reserve } from "../core/byte-cursor.js";
import { assert, DEV } from "../util/assert.js";
import { IS_LITTLE_ENDIAN_PLATFORM } from "../util/constants.js";
import { isU32 } from "../util/validator.js";
import { readFixedData } from "./data.js";
import { readI64, writeI64 } from "./fixed-primitive.js";
import { writeU8FixedArray } from "./u8-array.js";
import { readUintSafe32, writeUintSafe32 } from "./uint.js";
export const readI64FixedArray = IS_LITTLE_ENDIAN_PLATFORM
    ? readI64FixedArrayLe
    : readI64FixedArrayBe;
export function readI64Array(bc) {
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
export const writeI64FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? writeI64FixedArrayLe : writeI64FixedArrayBe;
export function writeI64Array(bc, x) {
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
