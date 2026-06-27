//! Copyright (c) 2022 Victorien Elvinger
//! Licensed under the MIT License (https://mit-license.org/)
import { check, reserve } from "../core/byte-cursor.js";
import { assert, DEV } from "../util/assert.js";
import { IS_LITTLE_ENDIAN_PLATFORM } from "../util/constants.js";
import { isU32 } from "../util/validator.js";
import { readFixedData } from "./data.js";
import { readI32, writeI32 } from "./fixed-primitive.js";
import { writeU8FixedArray } from "./u8-array.js";
import { readUintSafe32, writeUintSafe32 } from "./uint.js";
export const readI32FixedArray = IS_LITTLE_ENDIAN_PLATFORM
    ? readI32FixedArrayLe
    : readI32FixedArrayBe;
export function readI32Array(bc) {
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
export const writeI32FixedArray = IS_LITTLE_ENDIAN_PLATFORM ? writeI32FixedArrayLe : writeI32FixedArrayBe;
export function writeI32Array(bc, x) {
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
