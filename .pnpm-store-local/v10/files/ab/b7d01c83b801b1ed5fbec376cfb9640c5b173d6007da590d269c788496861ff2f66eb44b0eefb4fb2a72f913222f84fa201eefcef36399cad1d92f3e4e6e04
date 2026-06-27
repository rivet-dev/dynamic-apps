import { mixNumber } from '../../utils/mix/number.mjs';

const SCALE_PRECISION = 0.0001;
const SCALE_MIN = 1 - SCALE_PRECISION;
const SCALE_MAX = 1 + SCALE_PRECISION;
const TRANSLATE_PRECISION = 0.01;
const TRANSLATE_MIN = 0 - TRANSLATE_PRECISION;
const TRANSLATE_MAX = 0 + TRANSLATE_PRECISION;
function calcLength(axis) {
    return axis.max - axis.min;
}
function isNear(value, target, maxDistance) {
    return Math.abs(value - target) <= maxDistance;
}
function calcAxisDelta(delta, source, target, origin = 0.5) {
    delta.origin = origin;
    delta.originPoint = mixNumber(source.min, source.max, delta.origin);
    delta.scale = calcLength(target) / calcLength(source);
    delta.translate =
        mixNumber(target.min, target.max, delta.origin) - delta.originPoint;
    if ((delta.scale >= SCALE_MIN && delta.scale <= SCALE_MAX) ||
        isNaN(delta.scale)) {
        delta.scale = 1.0;
    }
    if ((delta.translate >= TRANSLATE_MIN &&
        delta.translate <= TRANSLATE_MAX) ||
        isNaN(delta.translate)) {
        delta.translate = 0.0;
    }
}
function calcBoxDelta(delta, source, target, origin) {
    calcAxisDelta(delta.x, source.x, target.x, origin ? origin.originX : undefined);
    calcAxisDelta(delta.y, source.y, target.y, origin ? origin.originY : undefined);
}
function calcRelativeAxis(target, relative, parent, anchor = 0) {
    const anchorPoint = anchor
        ? mixNumber(parent.min, parent.max, anchor)
        : parent.min;
    target.min = anchorPoint + relative.min;
    target.max = target.min + calcLength(relative);
}
function calcRelativeBox(target, relative, parent, anchor) {
    calcRelativeAxis(target.x, relative.x, parent.x, anchor?.x);
    calcRelativeAxis(target.y, relative.y, parent.y, anchor?.y);
}
function calcRelativeAxisPosition(target, layout, parent, anchor = 0) {
    const anchorPoint = anchor
        ? mixNumber(parent.min, parent.max, anchor)
        : parent.min;
    target.min = layout.min - anchorPoint;
    target.max = target.min + calcLength(layout);
}
function calcRelativePosition(target, layout, parent, anchor) {
    calcRelativeAxisPosition(target.x, layout.x, parent.x, anchor?.x);
    calcRelativeAxisPosition(target.y, layout.y, parent.y, anchor?.y);
}

export { calcAxisDelta, calcBoxDelta, calcLength, calcRelativeAxis, calcRelativeAxisPosition, calcRelativeBox, calcRelativePosition, isNear };
//# sourceMappingURL=delta-calc.mjs.map
