const animationMaps = new WeakMap();
const animationMapKey = (name, pseudoElement = "") => `${name}:${pseudoElement}`;
function getAnimationMap(element) {
    let map = animationMaps.get(element);
    if (!map) {
        map = new Map();
        animationMaps.set(element, map);
    }
    return map;
}

export { animationMapKey, getAnimationMap };
//# sourceMappingURL=active-animations.mjs.map
