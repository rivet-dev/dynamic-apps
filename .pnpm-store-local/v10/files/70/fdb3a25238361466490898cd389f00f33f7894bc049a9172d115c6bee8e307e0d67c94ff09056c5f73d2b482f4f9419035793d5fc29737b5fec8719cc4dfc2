import { Config } from "./core/config.js";
export * from "./ast/bare-ast.js";
export * from "./ast/bare-configure.js";
export * from "./ast/bare-normalization.js";
export * from "./core/compiler-error.js";
export * from "./core/config.js";
export * from "./generator/js-generator.js";
export * from "./parser/bare-parser.js";
/**
 * Turn the schema `content` into a target language, taking `conf` into account.
 *
 * @example
 * ```js
 * const input = "type Person struct { name: str }"
 * const tsOutput = transform(input)
 * ```
 *
 * @example
 * ```js
 * const input = "type Person struct { name: str }"
 * const dtsOutput = transform(input, { generator: "dts" })
 * ```
 *
 * @throws {@link CompilerError} if parsing failed.
 * @throws {@link ConfigError} if the code generator cannot be determinate or
 * the format of the schema is not a supported.
 */
export declare function transform(content: string, conf?: Partial<Config>): string;
