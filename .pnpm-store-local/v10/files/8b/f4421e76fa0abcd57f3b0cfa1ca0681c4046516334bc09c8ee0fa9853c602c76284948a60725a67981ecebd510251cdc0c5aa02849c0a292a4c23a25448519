//! Copyright (c) 2022 Victorien Elvinger
//! Licensed under the MIT License (https://mit-license.org/)
/**
 * Can `val` be stored as a signed integer in 8 bits?
 */
export function isI8(val) {
    return val === (val << 24) >> 24;
}
/**
 * Can `val` be stored as a signed integer in 16 bits?
 */
export function isI16(val) {
    return val === (val << 16) >> 16;
}
/**
 * Can `val` be stored as a signed integer in 32 bits?
 */
export function isI32(val) {
    return val === (val | 0);
}
/**
 * Can `val` be stored as a signed integer in 64 bits?
 */
export function isI64(val) {
    return val === BigInt.asIntN(64, val);
}
/**
 * Can `val` be stored as an unsigned integer in 8 bits?
 */
export function isU8(val) {
    return val === (val & 0xff);
}
/**
 * Can `val` be stored as an unsigned integer in 16 bits?
 */
export function isU16(val) {
    return val === (val & 0xffff);
}
/**
 * Can `val` be stored as an unsigned integer in 32 bits?
 */
export function isU32(val) {
    return val === val >>> 0;
}
/**
 * Can `val` be stored as an unsigned integer in 64 bits?
 */
export function isU64(val) {
    return val === BigInt.asUintN(64, val);
}
/**
 * Is `val` an unsigned integer that can be safely represented as a float?
 */
export function isU64Safe(val) {
    return Number.isSafeInteger(val) && val >= 0;
}
