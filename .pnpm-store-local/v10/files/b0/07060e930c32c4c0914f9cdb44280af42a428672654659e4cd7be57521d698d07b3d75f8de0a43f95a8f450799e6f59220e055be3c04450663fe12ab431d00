//! Copyright (c) 2022 Victorien Elvinger
//! Licensed under the MIT License (https://mit-license.org/)
import { assert, DEV } from "../util/assert.js";
import { TOO_LARGE_NUMBER } from "../util/constants.js";
import { isU32 } from "../util/validator.js";
export function Config({ initialBufferLength = 1024, maxBufferLength = 1024 * 1024 * 32 /* 32 MiB */, }) {
    if (DEV) {
        assert(isU32(initialBufferLength), TOO_LARGE_NUMBER);
        assert(isU32(maxBufferLength), TOO_LARGE_NUMBER);
        assert(initialBufferLength <= maxBufferLength, "initialBufferLength must be lower than or equal to maxBufferLength");
    }
    return {
        initialBufferLength,
        maxBufferLength,
    };
}
export const DEFAULT_CONFIG = /* @__PURE__ */ Config({});
