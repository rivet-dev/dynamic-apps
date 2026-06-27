import { motionValue, spring } from 'motion-dom';
import { createAnimationsFromSequence } from '../sequence/create.mjs';
import { animateSubject } from './subject.mjs';

function animateSequence(sequence, options, scope) {
    const animations = [];
    /**
     * Pre-process: replace function segments with MotionValue segments,
     * subscribe callbacks immediately
     */
    const processedSequence = sequence.map((segment) => {
        if (Array.isArray(segment) && typeof segment[0] === "function") {
            const callback = segment[0];
            const mv = motionValue(0);
            mv.on("change", callback);
            if (segment.length === 1) {
                return [mv, [0, 1]];
            }
            else if (segment.length === 2) {
                return [mv, [0, 1], segment[1]];
            }
            else {
                return [mv, segment[1], segment[2]];
            }
        }
        return segment;
    });
    const animationDefinitions = createAnimationsFromSequence(processedSequence, options, scope, { spring });
    animationDefinitions.forEach(({ keyframes, transition }, subject) => {
        animations.push(...animateSubject(subject, keyframes, transition));
    });
    return animations;
}

export { animateSequence };
//# sourceMappingURL=sequence.mjs.map
