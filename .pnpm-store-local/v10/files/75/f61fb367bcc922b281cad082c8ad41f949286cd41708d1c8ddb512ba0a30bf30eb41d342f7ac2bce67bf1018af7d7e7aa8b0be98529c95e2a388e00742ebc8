"use strict";
export { DEV } from "#dev";
export class AssertionError extends Error {
  constructor() {
    super(...arguments);
    this.name = "AssertionError";
  }
}
const V8Error = Error;
export function assert(test, message = "") {
  if (!test) {
    const e = new AssertionError(message);
    if (V8Error.captureStackTrace) {
      V8Error.captureStackTrace(e, assert);
    }
    throw e;
  }
}
