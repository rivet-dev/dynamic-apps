"use client";
import { transform } from 'motion-dom';
import { useConstant } from '../utils/use-constant.mjs';
import { useCombineMotionValues } from './use-combine-values.mjs';
import { useComputed } from './use-computed.mjs';

function useTransform(input, inputRangeOrTransformer, outputRangeOrMap, options) {
    if (typeof input === "function") {
        return useComputed(input);
    }
    /**
     * Detect if outputRangeOrMap is an output map (object with keys)
     * rather than an output range (array).
     */
    const isOutputMap = outputRangeOrMap !== undefined &&
        !Array.isArray(outputRangeOrMap) &&
        typeof inputRangeOrTransformer !== "function";
    if (isOutputMap) {
        return useMapTransform(input, inputRangeOrTransformer, outputRangeOrMap, options);
    }
    const outputRange = outputRangeOrMap;
    const transformer = typeof inputRangeOrTransformer === "function"
        ? inputRangeOrTransformer
        : transform(inputRangeOrTransformer, outputRange, options);
    const result = Array.isArray(input)
        ? useListTransform(input, transformer)
        : useListTransform([input], ([latest]) => transformer(latest));
    const inputAccelerate = !Array.isArray(input)
        ? input.accelerate
        : undefined;
    if (inputAccelerate &&
        !inputAccelerate.isTransformed &&
        typeof inputRangeOrTransformer !== "function" &&
        Array.isArray(outputRangeOrMap) &&
        options?.clamp !== false) {
        result.accelerate = {
            ...inputAccelerate,
            times: inputRangeOrTransformer,
            keyframes: outputRangeOrMap,
            isTransformed: true,
            ...(options?.ease ? { ease: options.ease } : {}),
        };
    }
    return result;
}
function useListTransform(values, transformer) {
    const latest = useConstant(() => []);
    return useCombineMotionValues(values, () => {
        latest.length = 0;
        const numValues = values.length;
        for (let i = 0; i < numValues; i++) {
            latest[i] = values[i].get();
        }
        return transformer(latest);
    });
}
function useMapTransform(inputValue, inputRange, outputMap, options) {
    /**
     * Capture keys once to ensure hooks are called in consistent order.
     */
    const keys = useConstant(() => Object.keys(outputMap));
    const output = useConstant(() => ({}));
    for (const key of keys) {
        output[key] = useTransform(inputValue, inputRange, outputMap[key], options);
    }
    return output;
}

export { useTransform };
//# sourceMappingURL=use-transform.mjs.map
