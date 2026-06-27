import { ScrollOffset } from '../offsets/presets.mjs';

/**
 * Maps from ProgressIntersection pairs used by Motion's preset offsets to
 * ViewTimeline named ranges. Returns undefined for unrecognised patterns,
 * which signals the caller to fall back to JS-based scroll tracking.
 */
const presets = [
    [ScrollOffset.Enter, "entry"],
    [ScrollOffset.Exit, "exit"],
    [ScrollOffset.Any, "cover"],
    [ScrollOffset.All, "contain"],
];
const stringToProgress = {
    start: 0,
    end: 1,
};
function parseStringOffset(s) {
    const parts = s.trim().split(/\s+/);
    if (parts.length !== 2)
        return undefined;
    const a = stringToProgress[parts[0]];
    const b = stringToProgress[parts[1]];
    if (a === undefined || b === undefined)
        return undefined;
    return [a, b];
}
function normaliseOffset(offset) {
    if (offset.length !== 2)
        return undefined;
    const result = [];
    for (const item of offset) {
        if (Array.isArray(item)) {
            result.push(item);
        }
        else if (typeof item === "string") {
            const parsed = parseStringOffset(item);
            if (!parsed)
                return undefined;
            result.push(parsed);
        }
        else {
            return undefined;
        }
    }
    return result;
}
function matchesPreset(offset, preset) {
    const normalised = normaliseOffset(offset);
    if (!normalised)
        return false;
    for (let i = 0; i < 2; i++) {
        const o = normalised[i];
        const p = preset[i];
        if (o[0] !== p[0] || o[1] !== p[1])
            return false;
    }
    return true;
}
function offsetToViewTimelineRange(offset) {
    if (!offset) {
        return { rangeStart: "contain 0%", rangeEnd: "contain 100%" };
    }
    for (const [preset, name] of presets) {
        if (matchesPreset(offset, preset)) {
            return { rangeStart: `${name} 0%`, rangeEnd: `${name} 100%` };
        }
    }
    return undefined;
}

export { offsetToViewTimelineRange };
//# sourceMappingURL=offset-to-range.mjs.map
