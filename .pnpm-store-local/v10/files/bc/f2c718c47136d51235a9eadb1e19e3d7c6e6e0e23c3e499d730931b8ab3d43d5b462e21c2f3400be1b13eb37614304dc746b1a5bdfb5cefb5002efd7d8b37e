import { color } from '../color/index.mjs';
import { colorRegex } from '../utils/color-regex.mjs';
import { floatRegex } from '../utils/float-regex.mjs';
import { sanitize } from '../utils/sanitize.mjs';

function test(v) {
    return (isNaN(v) &&
        typeof v === "string" &&
        (v.match(floatRegex)?.length || 0) +
            (v.match(colorRegex)?.length || 0) >
            0);
}
const NUMBER_TOKEN = "number";
const COLOR_TOKEN = "color";
const VAR_TOKEN = "var";
const VAR_FUNCTION_TOKEN = "var(";
const SPLIT_TOKEN = "${}";
// this regex consists of the `singleCssVariableRegex|rgbHSLValueRegex|digitRegex`
const complexRegex = /var\s*\(\s*--(?:[\w-]+\s*|[\w-]+\s*,(?:\s*[^)(\s]|\s*\((?:[^)(]|\([^)(]*\))*\))+\s*)\)|#[\da-f]{3,8}|(?:rgb|hsl)a?\((?:-?[\d.]+%?[,\s]+){2}-?[\d.]+%?\s*(?:[,/]\s*)?(?:\b\d+(?:\.\d+)?|\.\d+)?%?\)|-?(?:\d+(?:\.\d+)?|\.\d+)/giu;
function analyseComplexValue(value) {
    const originalValue = value.toString();
    const values = [];
    const indexes = {
        color: [],
        number: [],
        var: [],
    };
    const types = [];
    let i = 0;
    const tokenised = originalValue.replace(complexRegex, (parsedValue) => {
        if (color.test(parsedValue)) {
            indexes.color.push(i);
            types.push(COLOR_TOKEN);
            values.push(color.parse(parsedValue));
        }
        else if (parsedValue.startsWith(VAR_FUNCTION_TOKEN)) {
            indexes.var.push(i);
            types.push(VAR_TOKEN);
            values.push(parsedValue);
        }
        else {
            indexes.number.push(i);
            types.push(NUMBER_TOKEN);
            values.push(parseFloat(parsedValue));
        }
        ++i;
        return SPLIT_TOKEN;
    });
    const split = tokenised.split(SPLIT_TOKEN);
    return { values, split, indexes, types };
}
function parseComplexValue(v) {
    return analyseComplexValue(v).values;
}
function buildTransformer({ split, types }) {
    const numSections = split.length;
    return (v) => {
        let output = "";
        for (let i = 0; i < numSections; i++) {
            output += split[i];
            if (v[i] !== undefined) {
                const type = types[i];
                if (type === NUMBER_TOKEN) {
                    output += sanitize(v[i]);
                }
                else if (type === COLOR_TOKEN) {
                    output += color.transform(v[i]);
                }
                else {
                    output += v[i];
                }
            }
        }
        return output;
    };
}
function createTransformer(source) {
    return buildTransformer(analyseComplexValue(source));
}
const convertNumbersToZero = (v) => typeof v === "number" ? 0 : color.test(v) ? color.getAnimatableNone(v) : v;
/**
 * Convert a parsed value to its zero equivalent, but preserve numbers
 * that act as divisors in CSS calc() expressions.
 *
 * analyseComplexValue extracts numbers from CSS strings and puts the
 * surrounding text into a `split` template array. For example:
 *   "calc(var(--gap) / 5)"  →  values: [var(--gap), 5]
 *                               split:  ["calc(", " / ", ")"]
 *
 * When building a zero-equivalent for animation, naively zeroing all
 * numbers turns the divisor into 0 → "calc(var(--gap) / 0)" → NaN.
 * We detect this by checking whether the text preceding a number
 * (split[i]) ends with "/" — the CSS calc division operator.
 */
const convertToZero = (value, splitBefore) => {
    if (typeof value === "number") {
        return splitBefore?.trim().endsWith("/") ? value : 0;
    }
    return convertNumbersToZero(value);
};
function getAnimatableNone(v) {
    const info = analyseComplexValue(v);
    const transformer = buildTransformer(info);
    return transformer(info.values.map((value, i) => convertToZero(value, info.split[i])));
}
const complex = {
    test,
    parse: parseComplexValue,
    createTransformer,
    getAnimatableNone,
};

export { analyseComplexValue, complex };
//# sourceMappingURL=index.mjs.map
