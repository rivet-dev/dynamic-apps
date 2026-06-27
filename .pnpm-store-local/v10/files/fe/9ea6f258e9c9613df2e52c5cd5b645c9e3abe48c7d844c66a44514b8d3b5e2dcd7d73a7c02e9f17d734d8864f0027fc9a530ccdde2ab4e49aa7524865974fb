import { complex } from '../complex/index.mjs';
import { filter } from '../complex/filter.mjs';
import { mask } from '../complex/mask.mjs';
import { getDefaultValueType } from '../maps/defaults.mjs';

const customTypes = /*@__PURE__*/ new Set([filter, mask]);
function getAnimatableNone(key, value) {
    let defaultValueType = getDefaultValueType(key);
    if (!customTypes.has(defaultValueType))
        defaultValueType = complex;
    // If value is not recognised as animatable, ie "none", create an animatable version origin based on the target
    return defaultValueType.getAnimatableNone
        ? defaultValueType.getAnimatableNone(value)
        : undefined;
}

export { getAnimatableNone };
//# sourceMappingURL=animatable-none.mjs.map
