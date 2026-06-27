import { complex } from './index.mjs';

const mask = {
    ...complex,
    getAnimatableNone: (v) => {
        const parsed = complex.parse(v);
        const transformer = complex.createTransformer(v);
        return transformer(parsed.map((v) => typeof v === "number" ? 0 : typeof v === "object" ? { ...v, alpha: 1 } : v));
    },
};

export { mask };
//# sourceMappingURL=mask.mjs.map
