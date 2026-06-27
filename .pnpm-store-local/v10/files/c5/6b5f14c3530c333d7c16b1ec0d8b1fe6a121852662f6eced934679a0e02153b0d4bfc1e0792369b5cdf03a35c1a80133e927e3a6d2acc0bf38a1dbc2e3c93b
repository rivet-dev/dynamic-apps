const browserColorFunctions = /^(?:oklch|oklab|lab|lch|color|color-mix|light-dark)\(/;
function hasBrowserOnlyColors(keyframes) {
    for (let i = 0; i < keyframes.length; i++) {
        if (typeof keyframes[i] === "string" &&
            browserColorFunctions.test(keyframes[i])) {
            return true;
        }
    }
    return false;
}

export { hasBrowserOnlyColors };
//# sourceMappingURL=is-browser-color.mjs.map
