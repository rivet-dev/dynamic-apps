"use strict";
//! Copyright (c) 2022 Victorien Elvinger
//! Licensed under the MIT License (https://mit-license.org/)
import { dent, toConstantCase } from "../utils/formatting.js";
export function generateBare(schema) {
  let result = "";
  for (const def of schema.defs) {
    if (!def.internal) {
      result += `${genAliased(def)}

`;
    }
  }
  return result.trim();
}
function genAliased(type) {
  return `${genDoc(type.comment)}type ${type.alias} ${genType(type.type)}`;
}
function genType(type) {
  switch (type.tag) {
    case "alias":
      return type.data;
    case "data":
      return genData(type);
    case "enum":
      return genEnum(type);
    case "list":
      return genList(type);
    case "map":
      return genMap(type);
    case "optional":
      return genOptional(type);
    case "struct":
      return genStruct(type);
    case "union":
      return genUnion(type);
    default:
      return type.tag;
  }
}
function genData(type) {
  return `data${genOptionalLength(type.data)}`;
}
function genEnum(type) {
  let body = "";
  for (const enumVal of type.data) {
    body += `${genEnumVal(enumVal)}
`;
  }
  return dent`
        enum {
            ${body.trim()}
        }
    `;
}
function genEnumVal(type) {
  return `${genDoc(type.comment)}${toConstantCase(type.name)} = ${type.val}`;
}
function genList(type) {
  return `list<${genType(type.types[0])}>${genOptionalLength(type.data)}`;
}
function genMap(type) {
  const keyTypedef = genType(type.types[0]);
  const valTypedef = genType(type.types[1]);
  return `map<${keyTypedef}><${valTypedef}>`;
}
function genOptional(type) {
  return `optional<${genType(type.types[0])}>`;
}
function genStruct(type) {
  const fields = type.data;
  let body = "";
  for (let i = 0; i < fields.length; i++) {
    body += `${genStructField(fields[i], type.types[i])}
`;
  }
  return dent`
        struct {
            ${body.trim()}
        }
    `;
}
function genStructField(field, type) {
  return `${genDoc(field.comment)}${field.name}: ${genType(type)}`;
}
function genUnion(type) {
  const tags = type.data;
  let body = "";
  for (let i = 0; i < tags.length; i++) {
    body += `${genUnionMember(tags[i], type.types[i])}
`;
  }
  return dent`
        union {
            ${body.trim()}
        }
    `;
}
function genUnionMember(tag, type) {
  return `| ${genType(type)} = ${tag.val}`;
}
function genOptionalLength(length) {
  return length != null ? `[${length.val}]` : "";
}
function genDoc(comment) {
  return comment !== "" ? `#${comment.trimEnd().replace(/\n/g, "\n#")}
` : "";
}
