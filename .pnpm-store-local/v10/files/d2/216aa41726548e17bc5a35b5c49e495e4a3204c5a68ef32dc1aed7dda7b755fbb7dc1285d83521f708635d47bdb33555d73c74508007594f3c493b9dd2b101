//! Copyright (c) 2022 Victorien Elvinger
//! Licensed under the MIT License (https://mit-license.org/)
import { assert, DEV } from "../util/assert.js";
import { TOO_LARGE_BUFFER } from "../util/constants.js";
import { isU32 } from "../util/validator.js";
import { BareError } from "./bare-error.js";
/**
 * @invariant `bytes.buffer === view.buffer`
 * @invariant `bytes.byteOffset === view.byteOffset`
 * @invariant `bytes.byteLength === view.byteLength`
 * @invariant `0 <= offset <= bytes.byteLength`
 * @invariant `bytes.byteLength <= config.maxBufferLength`
 *
 * ```txt
 * |         {bytes,view}.buffer                      |
 *        |          bytes                    |
 *        |          view                     |
 *        |<------ offset ------>|
 *        |<----------- config.maxBufferLength ------------>|
 * ```
 *
 * @sealed
 */
export class ByteCursor {
    /**
     * @throws {BareError} Buffer exceeds `config.maxBufferLength`
     */
    constructor(bytes, config) {
        /**
         * Read and write Offset in {@link view} and {@link bytes}
         */
        this.offset = 0;
        if (bytes.length > config.maxBufferLength) {
            throw new BareError(0, TOO_LARGE_BUFFER);
        }
        this.bytes = bytes;
        this.config = config;
        this.view = new DataView(bytes.buffer, bytes.byteOffset, bytes.length);
    }
}
/**
 * Check that `min` number of bytes are available.
 *
 * @throws {BareError} bytes are missing.
 */
export function check(bc, min) {
    // We try to keep this function as small as possible to allow inling.
    if (DEV) {
        assert(isU32(min));
    }
    if (bc.offset + min > bc.bytes.length) {
        throw new BareError(bc.offset, "missing bytes");
    }
}
/**
 * Reserve `min` number of bytes.
 *
 * @throws {BareError} Buffer exceeds `config.maxBufferLength`.
 */
export function reserve(bc, min) {
    // We try to keep this function as small as possible to allow inling.
    if (DEV) {
        assert(isU32(min));
    }
    const minLen = (bc.offset + min) | 0;
    if (minLen > bc.bytes.length) {
        grow(bc, minLen);
    }
}
/**
 * Grow the underlying buffer of `bc` such that its length is
 * greater or equal to `minLen`.
 *
 * @throws {BareError} Buffer exceeds `config.maxBufferLength`.
 */
function grow(bc, minLen) {
    if (minLen > bc.config.maxBufferLength) {
        throw new BareError(0, TOO_LARGE_BUFFER);
    }
    //
    // |     bytes,view}.buffer         |
    //     |        bytes        |
    //     |        view         |
    //     |<-- offset -->|<-- min -->|
    //     |<-------- minLen -------->|
    //     |         new view                         |
    //     |         new bytes                        |
    //     |         new {bytes,view}.buffer          |
    //     |<------------- config.maxBufferLength -------------->|
    //
    const buffer = bc.bytes.buffer;
    let newBytes;
    if (isEs2024ArrayBufferLike(buffer) &&
        // Make sure that the view covers the end of the buffer.
        // If it is not the case, this indicates that the user don't want
        // to override the trailing bytes.
        bc.bytes.byteOffset + bc.bytes.byteLength === buffer.byteLength &&
        bc.bytes.byteLength + minLen <= buffer.maxByteLength) {
        const newLen = Math.min(minLen << 1, bc.config.maxBufferLength, buffer.maxByteLength);
        if (buffer instanceof ArrayBuffer) {
            buffer.resize(newLen);
        }
        else {
            buffer.grow(newLen);
        }
        newBytes = new Uint8Array(buffer, bc.bytes.byteOffset, newLen);
    }
    else {
        const newLen = Math.min(minLen << 1, bc.config.maxBufferLength);
        newBytes = new Uint8Array(newLen);
        newBytes.set(bc.bytes);
    }
    bc.bytes = newBytes;
    bc.view = new DataView(newBytes.buffer);
}
function isEs2024ArrayBufferLike(buffer) {
    return "maxByteLength" in buffer;
}
