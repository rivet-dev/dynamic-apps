"use client";
import { useContext, useMemo } from 'react';
import { useConstant } from '../../utils/use-constant.mjs';
import { useUnmountEffect } from '../../utils/use-unmount-effect.mjs';
import { useReducedMotionConfig } from '../../utils/reduced-motion/use-reduced-motion-config.mjs';
import { MotionConfigContext } from '../../context/MotionConfigContext.mjs';
import { createScopedAnimate } from '../animate/index.mjs';

function useAnimate() {
    const scope = useConstant(() => ({
        current: null, // Will be hydrated by React
        animations: [],
    }));
    const reduceMotion = useReducedMotionConfig() ?? undefined;
    const { skipAnimations } = useContext(MotionConfigContext);
    const animate = useMemo(() => createScopedAnimate({ scope, reduceMotion, skipAnimations }), [scope, reduceMotion, skipAnimations]);
    useUnmountEffect(() => {
        scope.animations.forEach((animation) => animation.stop());
        scope.animations.length = 0;
    });
    return [scope, animate];
}

export { useAnimate };
//# sourceMappingURL=use-animate.mjs.map
