import { supportsViewTimeline, supportsScrollTimeline } from 'motion-dom';

function canUseNativeTimeline(target) {
    if (typeof window === "undefined")
        return false;
    return target ? supportsViewTimeline() : supportsScrollTimeline();
}

export { canUseNativeTimeline };
//# sourceMappingURL=can-use-native-timeline.mjs.map
