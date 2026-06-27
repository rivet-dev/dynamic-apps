import { MotionValueState } from '../MotionValueState.mjs';

function createEffect(addValue) {
    const stateCache = new WeakMap();
    return (subject, values) => {
        const state = stateCache.get(subject) ?? new MotionValueState();
        stateCache.set(subject, state);
        const subscriptions = [];
        for (const key in values) {
            const value = values[key];
            const remove = addValue(subject, state, key, value);
            subscriptions.push(remove);
        }
        return () => {
            for (const cancel of subscriptions)
                cancel();
        };
    };
}

export { createEffect };
//# sourceMappingURL=create-effect.mjs.map
