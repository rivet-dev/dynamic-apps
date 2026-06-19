import { describe, expect, test } from "vitest";
import {
	encodeLengthPrefixedPayload,
	LENGTH_PREFIX_BYTES,
	tryDecodeLengthPrefixedPayload,
} from "../src/framing.js";

describe("length-prefixed framing", () => {
	test("encodes payload length as a four-byte big-endian prefix", () => {
		const encoded = encodeLengthPrefixedPayload(Uint8Array.from([1, 2, 3]));

		expect(encoded.readUInt32BE(0)).toBe(3);
		expect([...encoded.subarray(LENGTH_PREFIX_BYTES)]).toEqual([1, 2, 3]);
	});

	test("waits for complete prefixes and payloads", () => {
		expect(tryDecodeLengthPrefixedPayload(Uint8Array.from([0, 0]))).toBeNull();

		const encoded = encodeLengthPrefixedPayload(Uint8Array.from([4, 5, 6]));
		expect(tryDecodeLengthPrefixedPayload(encoded.subarray(0, 5))).toBeNull();
	});

	test("decodes one payload and returns remaining bytes", () => {
		const first = encodeLengthPrefixedPayload(Uint8Array.from([7, 8]));
		const second = encodeLengthPrefixedPayload(Uint8Array.from([9]));
		const decoded = tryDecodeLengthPrefixedPayload(
			Buffer.concat([first, second]),
		);

		expect(decoded).not.toBeNull();
		expect([...(decoded?.payload ?? [])]).toEqual([7, 8]);
		expect(Buffer.from(decoded?.remaining ?? []).equals(second)).toBe(true);
	});

	// Adversarial coverage (VECTORS.md E2 / browser TS frame-length cap): a
	// hostile peer declares an enormous frame length. The decoder must NOT
	// allocate or slice based on the *declared* length; it only ever yields a
	// payload once that many real bytes have actually arrived. An oversized
	// declared length therefore returns null (keep waiting) and never produces
	// an out-of-bounds read or an unbounded allocation from the length field.
	test("an oversized declared length never over-reads or over-allocates", () => {
		// declaredLength = 0xFFFFFFFF (~4 GiB) but only a handful of real bytes.
		const hostile = Buffer.alloc(LENGTH_PREFIX_BYTES + 8);
		hostile.writeUInt32BE(0xffffffff, 0);
		hostile.fill(0x41, LENGTH_PREFIX_BYTES);

		// No 4 GiB allocation, no throw: the decoder simply waits for bytes that
		// will never come and reports "incomplete".
		expect(tryDecodeLengthPrefixedPayload(hostile)).toBeNull();

		// A frame is only ever emitted when the buffer truly holds frameEnd
		// bytes, and the emitted payload length equals the declared length and
		// is bounded by the bytes actually supplied.
		const honest = encodeLengthPrefixedPayload(Uint8Array.from([1, 2, 3, 4]));
		const decoded = tryDecodeLengthPrefixedPayload(honest);
		expect(decoded?.payload.length).toBe(4);
		expect(decoded?.payload.length).toBeLessThanOrEqual(honest.length);
	});

	test("a declared length larger than the buffer is treated as incomplete", () => {
		// Claim 1 KiB but provide only 10 payload bytes.
		const buf = Buffer.alloc(LENGTH_PREFIX_BYTES + 10);
		buf.writeUInt32BE(1024, 0);
		expect(tryDecodeLengthPrefixedPayload(buf)).toBeNull();
	});
});
