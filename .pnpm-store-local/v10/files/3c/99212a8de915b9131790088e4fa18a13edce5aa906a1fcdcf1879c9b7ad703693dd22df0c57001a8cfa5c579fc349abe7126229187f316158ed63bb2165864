import { resolveTransition } from './resolve-transition.mjs';

function getValueTransition(transition, key) {
    const valueTransition = transition?.[key] ??
        transition?.["default"] ??
        transition;
    if (valueTransition !== transition) {
        return resolveTransition(valueTransition, transition);
    }
    return valueTransition;
}

export { getValueTransition };
//# sourceMappingURL=get-value-transition.mjs.map
