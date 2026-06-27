"use strict";
//! Copyright (c) 2022 Victorien Elvinger
//! Licensed under the MIT License (https://mit-license.org/)
import { configure } from "./ast/bare-configure.js";
import { normalize } from "./ast/bare-normalization.js";
import { checkSemantic } from "./ast/bare-semantic-checker.js";
import { Config } from "./core/config.js";
import { generateBare } from "./generator/bare-generator.js";
import { generate } from "./generator/js-generator.js";
import { parse } from "./parser/bare-parser.js";
export * from "./ast/bare-ast.js";
export * from "./ast/bare-configure.js";
export * from "./ast/bare-normalization.js";
export * from "./core/compiler-error.js";
export * from "./core/config.js";
export * from "./generator/js-generator.js";
export * from "./parser/bare-parser.js";
export function transform(content, conf = {}) {
  const completedConfig = Config(conf);
  const schema = parse(content, completedConfig);
  const configured = configure(schema, completedConfig);
  checkSemantic(configured, completedConfig);
  if (completedConfig.generator === "bare") {
    return generateBare(schema);
  }
  const normalizedSchema = normalize(configured);
  return generate(normalizedSchema, completedConfig);
}
