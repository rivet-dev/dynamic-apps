//! Copyright (c) 2022 Victorien Elvinger
//! Licensed under the MIT License (https://mit-license.org/)
import { BareError } from "../core/bare-error.js";
import { check, reserve } from "../core/byte-cursor.js";
import { assert, DEV } from "../util/assert.js";
import { INVALID_UTF8_STRING, TEXT_DECODER_THRESHOLD, TEXT_ENCODER_THRESHOLD, } from "../util/constants.js";
import { isU32 } from "../util/validator.js";
import { readUnsafeU8FixedArray, writeU8FixedArray } from "./u8-array.js";
import { readUintSafe32, writeUintSafe32 } from "./uint.js";
export function readString(bc) {
    return readFixedString(bc, readUintSafe32(bc));
}
export function writeString(bc, x) {
    if (x.length < TEXT_ENCODER_THRESHOLD) {
        const byteLen = utf8ByteLength(x);
        writeUintSafe32(bc, byteLen);
        reserve(bc, byteLen);
        writeUtf8Js(bc, x);
    }
    else {
        const strBytes = UTF8_ENCODER.encode(x);
        writeUintSafe32(bc, strBytes.length);
        writeU8FixedArray(bc, strBytes);
    }
}
export function readFixedString(bc, byteLen) {
    if (DEV) {
        assert(isU32(byteLen));
    }
    if (byteLen < TEXT_DECODER_THRESHOLD) {
        return readUtf8Js(bc, byteLen);
    }
    try {
        return UTF8_DECODER.decode(readUnsafeU8FixedArray(bc, byteLen));
    }
    catch (_cause) {
        throw new BareError(bc.offset, INVALID_UTF8_STRING);
    }
}
export function writeFixedString(bc, x) {
    if (x.length < TEXT_ENCODER_THRESHOLD) {
        const byteLen = utf8ByteLength(x);
        reserve(bc, byteLen);
        writeUtf8Js(bc, x);
    }
    else {
        writeU8FixedArray(bc, UTF8_ENCODER.encode(x));
    }
}
function readUtf8Js(bc, byteLen) {
    // `check` asserts that `byteLen` is a `u32`
    check(bc, byteLen);
    let result = "";
    const bytes = bc.bytes;
    let offset = bc.offset;
    const upperOffset = offset + byteLen;
    while (offset < upperOffset) {
        let codePoint = bytes[offset++];
        if (codePoint > 0x7f) {
            let malformed = true;
            const byte1 = codePoint;
            if (offset < upperOffset && codePoint < 0xe0) {
                // 110x_xxxx 10xx_xxxx
                const byte2 = bytes[offset++];
                codePoint = ((byte1 & 0x1f) << 6) | (byte2 & 0x3f);
                malformed =
                    codePoint >> 7 === 0 || // non-canonical char
                        byte1 >> 5 !== 0b110 || // invalid tag
                        byte2 >> 6 !== 0b10; // invalid tag
            }
            else if (offset + 1 < upperOffset && codePoint < 0xf0) {
                // 1110_xxxx 10xx_xxxx 10xx_xxxx
                const byte2 = bytes[offset++];
                const byte3 = bytes[offset++];
                codePoint =
                    ((byte1 & 0xf) << 12) |
                        ((byte2 & 0x3f) << 6) |
                        (byte3 & 0x3f);
                malformed =
                    codePoint >> 11 === 0 || // non-canonical char or missing data
                        codePoint >> 11 === 0x1b || // surrogate char (0xD800 <= codePoint <= 0xDFFF)
                        byte1 >> 4 !== 0b1110 || // invalid tag
                        byte2 >> 6 !== 0b10 || // invalid tag
                        byte3 >> 6 !== 0b10; // invalid tag
            }
            else if (offset + 2 < upperOffset) {
                // 1110_xxxx 10xx_xxxx 10xx_xxxx 10xx_xxxx
                const byte2 = bytes[offset++];
                const byte3 = bytes[offset++];
                const byte4 = bytes[offset++];
                codePoint =
                    ((byte1 & 0x7) << 18) |
                        ((byte2 & 0x3f) << 12) |
                        ((byte3 & 0x3f) << 6) |
                        (byte4 & 0x3f);
                malformed =
                    codePoint >> 16 === 0 || // non-canonical char or missing data
                        codePoint > 0x10ffff || // too large code point
                        byte1 >> 3 !== 0b11110 || // invalid tag
                        byte2 >> 6 !== 0b10 || // invalid tag
                        byte3 >> 6 !== 0b10 || // invalid tag
                        byte4 >> 6 !== 0b10; // invalid tag
            }
            if (malformed) {
                throw new BareError(bc.offset, INVALID_UTF8_STRING);
            }
        }
        result += String.fromCodePoint(codePoint);
    }
    bc.offset = offset;
    return result;
}
function writeUtf8Js(bc, s) {
    const bytes = bc.bytes;
    let offset = bc.offset;
    let i = 0;
    while (i < s.length) {
        const codePoint = s.codePointAt(i++); // i is a valid index
        if (codePoint < 0x80) {
            bytes[offset++] = codePoint;
        }
        else {
            if (codePoint < 0x800) {
                bytes[offset++] = 0xc0 | (codePoint >> 6);
            }
            else {
                if (codePoint < 65536) {
                    bytes[offset++] = 0xe0 | (codePoint >> 12);
                }
                else {
                    bytes[offset++] = 0xf0 | (codePoint >> 18);
                    bytes[offset++] = 0x80 | ((codePoint >> 12) & 0x3f);
                    i++; // surrogate pair encoded as two ucs2 chars
                }
                bytes[offset++] = 0x80 | ((codePoint >> 6) & 0x3f);
            }
            bytes[offset++] = 0x80 | (codePoint & 0x3f);
        }
    }
    bc.offset = offset;
}
function utf8ByteLength(s) {
    let result = s.length;
    for (let i = 0; i < s.length; i++) {
        const codePoint = s.codePointAt(i); // i is a valid index
        if (codePoint > 0x7f) {
            result++;
            if (codePoint > 0x7ff) {
                result++;
                if (codePoint > 65535) {
                    i++; // surrogate pair encoded as two ucs2 chars
                }
            }
        }
    }
    return result;
}
/**
 * UTF-8 decoding and encoding using API that is supported in Node >= 12 and
 * modern browsers:
 * https://developer.mozilla.org/en-US/docs/Web/API/TextEncoder/write
 * https://developer.mozilla.org/en-US/docs/Web/API/TextDecoder/read
 *
 * If you're running in an environment where it's not available,
 * please use a polyfill, such as:
 * https://github.com/anonyco/FastestSmallestTextEncoderDecoder
 */
const UTF8_DECODER = /* @__PURE__ */ new TextDecoder("utf-8", { fatal: true });
const UTF8_ENCODER = /* @__PURE__ */ new TextEncoder();
