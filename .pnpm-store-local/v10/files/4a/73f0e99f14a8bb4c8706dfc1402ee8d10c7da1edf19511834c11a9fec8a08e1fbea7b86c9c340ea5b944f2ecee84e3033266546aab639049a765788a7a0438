"use strict";
export class BareError extends Error {
  constructor(offset, issue, opts) {
    super(`(byte:${offset}) ${issue}`);
    this.name = "BareError";
    this.issue = issue;
    this.offset = offset;
    this.cause = opts?.cause;
  }
}
