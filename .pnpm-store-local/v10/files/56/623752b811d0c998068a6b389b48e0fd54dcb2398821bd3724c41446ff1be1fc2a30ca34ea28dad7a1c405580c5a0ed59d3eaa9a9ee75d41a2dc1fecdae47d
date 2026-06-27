"use strict";
//! Copyright (c) 2022 Victorien Elvinger
//! Licensed under the MIT License (https://mit-license.org/)
import * as ast from "./bare-ast.js";
export function normalize(schema) {
  const n = { defs: [], dedup: /* @__PURE__ */ new Map(), aliasCount: 0 };
  const defs = n.defs;
  for (const def of schema.defs) {
    const type = normalizeSubtypes(n, def.type);
    if (def.type !== type) {
      const { alias, internal, comment, offset } = def;
      defs.push({ alias, internal, type, comment, offset });
    } else {
      defs.push(def);
    }
  }
  return defs.length > schema.defs.length ? { defs, filename: schema.filename, offset: schema.offset } : schema;
}
function normalizeSubtypes(n, type) {
  if (type.types != null && type.types.length > 0) {
    const types = type.types.map((t) => maybeAlias(n, t));
    if (type.types.some((t, i) => t !== types[i])) {
      const { tag, data, extra, offset } = type;
      return { tag, data, types, extra, offset };
    }
  }
  return type;
}
function maybeAlias(n, type) {
  switch (type.tag) {
    case "list": {
      if (!type.extra?.typedArray) {
        return genAlias(n, type);
      }
      break;
    }
    case "map":
    case "optional":
    case "union":
      return genAlias(n, type);
  }
  return normalizeSubtypes(n, type);
}
function genAlias(n, type) {
  const stringifiedType = JSON.stringify(ast.withoutOffset(type));
  let alias = n.dedup.get(stringifiedType);
  if (alias == null) {
    const normalized = normalizeSubtypes(n, type);
    alias = `${n.aliasCount++}`;
    n.defs.push({
      alias,
      internal: true,
      type: normalized,
      comment: "",
      offset: normalized.offset
    });
    n.dedup.set(stringifiedType, alias);
  }
  const offset = type.offset;
  return { tag: "alias", data: alias, types: null, extra: null, offset };
}
