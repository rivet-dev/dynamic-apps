"use strict";
export function isI8(val) {
  return val === val << 24 >> 24;
}
export function isI16(val) {
  return val === val << 16 >> 16;
}
export function isI32(val) {
  return val === (val | 0);
}
export function isI64(val) {
  return val === BigInt.asIntN(64, val);
}
export function isU8(val) {
  return val === (val & 255);
}
export function isU16(val) {
  return val === (val & 65535);
}
export function isU32(val) {
  return val === val >>> 0;
}
export function isU64(val) {
  return val === BigInt.asUintN(64, val);
}
export function isU64Safe(val) {
  return Number.isSafeInteger(val) && val >= 0;
}
