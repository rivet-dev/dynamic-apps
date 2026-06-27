/**
 * Pipe
 * Compose other transformers to run linearily
 * pipe(min(20), max(40))
 * @param  {...functions} transformers
 * @return {function}
 */
const pipe = (...transformers) => transformers.reduce((a, b) => (v) => b(a(v)));

export { pipe };
//# sourceMappingURL=pipe.mjs.map
