import { type ByteCursor } from "../core/byte-cursor.ts";
export declare function readU8Array(bc: ByteCursor): Uint8Array<ArrayBuffer>;
export declare function writeU8Array(bc: ByteCursor, x: Uint8Array): void;
export declare function readU8FixedArray(bc: ByteCursor, len: number): Uint8Array<ArrayBuffer>;
export declare function writeU8FixedArray(bc: ByteCursor, x: Uint8Array): void;
/**
 * Advance `bc` by `len` bytes and return a view of the read bytes.
 *
 * WARNING: The returned array should not be modified.
 */
export declare function readUnsafeU8FixedArray(bc: ByteCursor, len: number): Uint8Array;
