"use strict";
import { DEV, assert } from "../util/assert.js";
import { TOO_LARGE_BUFFER } from "../util/constants.js";
import { isU32 } from "../util/validator.js";
import { BareError } from "./bare-error.js";
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
export function check(bc, min) {
  if (DEV) {
    assert(isU32(min));
  }
  if (bc.offset + min > bc.bytes.length) {
    throw new BareError(bc.offset, "missing bytes");
  }
}
export function reserve(bc, min) {
  if (DEV) {
    assert(isU32(min));
  }
  const minLen = bc.offset + min | 0;
  if (minLen > bc.bytes.length) {
    if (minLen > bc.config.maxBufferLength) {
      throw new BareError(0, TOO_LARGE_BUFFER);
    }
    const newLen = Math.min(minLen << 1, bc.config.maxBufferLength);
    const newBytes = new Uint8Array(newLen);
    newBytes.set(bc.bytes);
    bc.bytes = newBytes;
    bc.view = new DataView(newBytes.buffer);
  }
}
