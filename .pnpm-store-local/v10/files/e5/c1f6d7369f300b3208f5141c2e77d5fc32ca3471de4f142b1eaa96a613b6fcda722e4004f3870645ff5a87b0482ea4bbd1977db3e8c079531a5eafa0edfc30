import { velocityPerSecond } from 'motion-utils';

const velocitySampleDuration = 5; // ms
function getGeneratorVelocity(resolveValue, t, current) {
    const prevT = Math.max(t - velocitySampleDuration, 0);
    return velocityPerSecond(current - resolveValue(prevT), t - prevT);
}

export { getGeneratorVelocity };
//# sourceMappingURL=velocity.mjs.map
