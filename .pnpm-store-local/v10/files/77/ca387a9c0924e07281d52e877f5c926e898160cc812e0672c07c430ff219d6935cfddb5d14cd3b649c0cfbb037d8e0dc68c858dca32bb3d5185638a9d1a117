import { color } from '../color/index.mjs';
import { filter } from '../complex/filter.mjs';
import { mask } from '../complex/mask.mjs';
import { numberValueTypes } from './number.mjs';

/**
 * A map of default value types for common values
 */
const defaultValueTypes = {
    ...numberValueTypes,
    // Color props
    color,
    backgroundColor: color,
    outlineColor: color,
    fill: color,
    stroke: color,
    // Border props
    borderColor: color,
    borderTopColor: color,
    borderRightColor: color,
    borderBottomColor: color,
    borderLeftColor: color,
    filter,
    WebkitFilter: filter,
    mask,
    WebkitMask: mask,
};
/**
 * Gets the default ValueType for the provided value key
 */
const getDefaultValueType = (key) => defaultValueTypes[key];

export { defaultValueTypes, getDefaultValueType };
//# sourceMappingURL=defaults.mjs.map
