import { describe, expect, it } from "vitest";
import { createGoogleDriveBackend } from "../src/index.js";

describe("@secure-exec/google-drive", () => {
	it("serializes a native google_drive mount descriptor", () => {
		expect(
			createGoogleDriveBackend({
				credentials: {
					clientEmail: "service-account@example.com",
					privateKey: "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----",
				},
				folderId: "folder-123",
				keyPrefix: "agentos/test",
				chunkSize: 16,
				inlineThreshold: 8,
			}),
		).toEqual({
			id: "google_drive",
			config: {
				credentials: {
					clientEmail: "service-account@example.com",
					privateKey: "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----",
				},
				folderId: "folder-123",
				keyPrefix: "agentos/test",
				chunkSize: 16,
				inlineThreshold: 8,
			},
		});
	});
});
