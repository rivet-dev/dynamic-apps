import type { Config } from "./config.ts";
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
export declare class ByteCursor {
    bytes: Uint8Array;
    readonly config: Config;
    /**
     * Read and write Offset in {@link view} and {@link bytes}
     */
    offset: number;
    view: DataView;
    /**
     * @throws {BareError} Buffer exceeds `config.maxBufferLength`
     */
    constructor(bytes: Uint8Array, config: Config);
}
/**
 * Check that `min` number of bytes are available.
 *
 * @throws {BareError} bytes are missing.
 */
export declare function check(bc: ByteCursor, min: number): void;
/**
 * Reserve `min` number of bytes.
 *
 * @throws {BareError} Buffer exceeds `config.maxBufferLength`.
 */
export declare function reserve(bc: ByteCursor, min: number): void;
