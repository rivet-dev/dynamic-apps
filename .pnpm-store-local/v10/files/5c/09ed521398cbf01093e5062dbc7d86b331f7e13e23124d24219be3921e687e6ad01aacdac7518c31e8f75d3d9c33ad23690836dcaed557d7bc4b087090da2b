//! Copyright (c) 2022 Victorien Elvinger
//! Licensed under the MIT License (https://mit-license.org/)
import { check, reserve } from "../core/byte-cursor.js";
import { assert, DEV } from "../util/assert.js";
import { IS_LITTLE_ENDIAN_PLATFORM } from "../util/constants.js";
import { isU32 } from "../util/validator.js";
import { readFixedData } from "./data.js";
import { readF32, writeF32 } from "./fixed-primitive.js";
import { writeU8FixedArray } from "./u8-array.js";
import { readUintSafe32, writeUintSafe32 } from "./uint.js";
export const readF32FixedArray = IS_LITTLE_ENDIAN_PLATFORM
    ? readF32FixedArrayLe
    : readF32FixedArrayBe;
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
export const writeF32FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? writeF32FixedArrayLe : writeF32FixedArrayBe;
function writeF32FixedArrayLe(bc, x) {
    writeU8FixedArray(bc, new Uint8Array(x.buffer, x.byteOffset, x.byteLength));
}
function writeF32FixedArrayBe(bc, val) {
    reserve(bc, val.length * 4);
    for (let i = 0; i < val.length; i++) {
        writeF32(bc, val[i]);
    }
}
export function readF32Array(bc) {
    return readF32FixedArray(bc, readUintSafe32(bc));
}
export function writeF32Array(bc, x) {
    writeUintSafe32(bc, x.length);
    if (x.length > 0) {
        writeF32FixedArray(bc, x);
    }
}
