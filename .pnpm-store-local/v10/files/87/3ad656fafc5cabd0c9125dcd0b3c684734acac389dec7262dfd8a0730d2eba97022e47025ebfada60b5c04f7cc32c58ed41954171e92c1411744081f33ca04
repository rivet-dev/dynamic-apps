//! Copyright (c) 2022 Victorien Elvinger
//! Licensed under the MIT License (https://mit-license.org/)
import { assert, DEV } from "../util/assert.js";
import { isU32 } from "../util/validator.js";
import { readU8Array, readU8FixedArray, writeU8Array, writeU8FixedArray, } from "./u8-array.js";
export function readData(bc) {
    return readU8Array(bc).buffer;
}
export function writeData(bc, x) {
    writeU8Array(bc, new Uint8Array(x));
}
export function readFixedData(bc, len) {
    if (DEV) {
        assert(isU32(len));
    }
    return readU8FixedArray(bc, len).buffer;
}
export function writeFixedData(bc, x) {
    writeU8FixedArray(bc, new Uint8Array(x));
}
