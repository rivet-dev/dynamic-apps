/**
 * Take an array of times that represent repeated keyframes. For instance
 * if we have original times of [0, 0.5, 1] then our repeated times will
 * be [0, 0.5, 1, 1, 1.5, 2]. Loop over the times and scale them back
 * down to a 0-1 scale.
 *
 * `repeatDelayUnits` is the repeatDelay expressed in units of a single
 * iteration's duration, so the total span equals `(repeat + 1) + repeat * repeatDelayUnits`.
 */
function normalizeTimes(times, repeat, repeatDelayUnits = 0) {
    const totalUnits = repeat + 1 + repeat * repeatDelayUnits;
    for (let i = 0; i < times.length; i++) {
        times[i] = times[i] / totalUnits;
    }
}

export { normalizeTimes };
//# sourceMappingURL=normalize-times.mjs.map
