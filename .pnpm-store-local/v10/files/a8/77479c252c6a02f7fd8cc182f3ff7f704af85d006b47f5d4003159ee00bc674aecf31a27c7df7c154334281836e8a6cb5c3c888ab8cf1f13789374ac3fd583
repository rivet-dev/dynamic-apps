"use client";
import { attachFollow, isMotionValue } from 'motion-dom';
import { useContext, useInsertionEffect } from 'react';
import { MotionConfigContext } from '../context/MotionConfigContext.mjs';
import { useMotionValue } from './use-motion-value.mjs';
import { useTransform } from './use-transform.mjs';

function useFollowValue(source, options = {}) {
    const { isStatic } = useContext(MotionConfigContext);
    const getFromSource = () => (isMotionValue(source) ? source.get() : source);
    // isStatic will never change, allowing early hooks return
    if (isStatic) {
        return useTransform(getFromSource);
    }
    const value = useMotionValue(getFromSource());
    useInsertionEffect(() => {
        return attachFollow(value, source, options);
    }, [value, JSON.stringify(options)]);
    return value;
}

export { useFollowValue };
//# sourceMappingURL=use-follow-value.mjs.map
