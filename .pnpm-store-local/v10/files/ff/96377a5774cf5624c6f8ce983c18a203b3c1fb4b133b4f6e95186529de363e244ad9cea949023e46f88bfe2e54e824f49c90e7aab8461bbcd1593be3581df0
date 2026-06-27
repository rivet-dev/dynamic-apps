import { motionValue } from './index.mjs';
import { JSAnimation } from '../animation/JSAnimation.mjs';
import { isMotionValue } from './utils/is-motion-value.mjs';
import { frame } from '../frameloop/frame.mjs';

/**
 * Create a `MotionValue` that animates to its latest value using any transition type.
 * Can either be a value or track another `MotionValue`.
 *
 * ```jsx
 * const x = motionValue(0)
 * const y = followValue(x, { type: "spring", stiffness: 300 })
 * // or with tween
 * const z = followValue(x, { type: "tween", duration: 0.5, ease: "easeOut" })
 * ```
 *
 * @param source - Initial value or MotionValue to track
 * @param options - Animation transition options
 * @returns `MotionValue`
 *
 * @public
 */
function followValue(source, options) {
    const initialValue = isMotionValue(source) ? source.get() : source;
    const value = motionValue(initialValue);
    attachFollow(value, source, options);
    return value;
}
/**
 * Attach an animation to a MotionValue that will animate whenever the value changes.
 * Similar to attachSpring but supports any transition type (spring, tween, inertia, etc.)
 *
 * @param value - The MotionValue to animate
 * @param source - Initial value or MotionValue to track
 * @param options - Animation transition options
 * @returns Cleanup function
 *
 * @public
 */
function attachFollow(value, source, options = {}) {
    const initialValue = value.get();
    let activeAnimation = null;
    let latestValue = initialValue;
    let latestSetter;
    const unit = typeof initialValue === "string"
        ? initialValue.replace(/[\d.-]/g, "")
        : undefined;
    const stopAnimation = () => {
        if (activeAnimation) {
            activeAnimation.stop();
            activeAnimation = null;
        }
        value.animation = undefined;
    };
    const startAnimation = () => {
        const currentValue = asNumber(value.get());
        const targetValue = asNumber(latestValue);
        // Don't animate if we're already at the target
        if (currentValue === targetValue) {
            stopAnimation();
            return;
        }
        // Use the running animation's analytical velocity for accuracy,
        // falling back to the MotionValue's velocity for the initial animation.
        // This prevents systematic velocity loss at high frame rates (240hz+).
        const velocity = activeAnimation
            ? activeAnimation.getGeneratorVelocity()
            : value.getVelocity();
        stopAnimation();
        activeAnimation = new JSAnimation({
            keyframes: [currentValue, targetValue],
            velocity,
            // Default to spring if no type specified (matches useSpring behavior)
            type: "spring",
            restDelta: 0.001,
            restSpeed: 0.01,
            ...options,
            onUpdate: latestSetter,
        });
    };
    // Use a stable function reference so the frame loop Set deduplicates
    // multiple calls within the same frame (e.g. rapid mouse events)
    const scheduleAnimation = () => {
        startAnimation();
        value.animation = activeAnimation ?? undefined;
        value["events"].animationStart?.notify();
        activeAnimation?.then(() => {
            value.animation = undefined;
            value["events"].animationComplete?.notify();
        });
    };
    value.attach((v, set) => {
        latestValue = v;
        latestSetter = (latest) => set(parseValue(latest, unit));
        frame.postRender(scheduleAnimation);
    }, stopAnimation);
    if (isMotionValue(source)) {
        let skipNextAnimation = options.skipInitialAnimation === true;
        const removeSourceOnChange = source.on("change", (v) => {
            if (skipNextAnimation) {
                skipNextAnimation = false;
                value.jump(parseValue(v, unit), false);
            }
            else {
                value.set(parseValue(v, unit));
            }
        });
        const removeValueOnDestroy = value.on("destroy", removeSourceOnChange);
        return () => {
            removeSourceOnChange();
            removeValueOnDestroy();
        };
    }
    return stopAnimation;
}
function parseValue(v, unit) {
    return unit ? v + unit : v;
}
function asNumber(v) {
    return typeof v === "number" ? v : parseFloat(v);
}

export { attachFollow, followValue };
//# sourceMappingURL=follow-value.mjs.map
