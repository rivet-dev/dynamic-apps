import { GroupAnimation } from '../animation/GroupAnimation.mjs';
import { copyBoxInto } from '../projection/geometry/copy.mjs';
import { createBox } from '../projection/geometry/models.mjs';
import { HTMLProjectionNode } from '../projection/node/HTMLProjectionNode.mjs';
import { HTMLVisualElement } from '../render/html/HTMLVisualElement.mjs';
import { visualElementStore } from '../render/store.mjs';
import { resolveElements } from '../utils/resolve-elements.mjs';
import { frame } from '../frameloop/frame.mjs';

const layoutSelector = "[data-layout], [data-layout-id]";
const noop = () => { };
function snapshotFromTarget(projection) {
    const target = projection.targetWithTransforms || projection.target;
    if (!target)
        return undefined;
    const measuredBox = createBox();
    const layoutBox = createBox();
    copyBoxInto(measuredBox, target);
    copyBoxInto(layoutBox, target);
    return {
        animationId: projection.root?.animationId ?? 0,
        measuredBox,
        layoutBox,
        latestValues: projection.animationValues || projection.latestValues || {},
        source: projection.id,
    };
}
class LayoutAnimationBuilder {
    constructor(scope, updateDom, defaultOptions) {
        this.sharedTransitions = new Map();
        this.notifyReady = noop;
        this.rejectReady = noop;
        this.scope = scope;
        this.updateDom = updateDom;
        this.defaultOptions = defaultOptions;
        this.readyPromise = new Promise((resolve, reject) => {
            this.notifyReady = resolve;
            this.rejectReady = reject;
        });
        frame.postRender(() => {
            this.start().then(this.notifyReady).catch(this.rejectReady);
        });
    }
    shared(id, transition) {
        this.sharedTransitions.set(id, transition);
        return this;
    }
    then(resolve, reject) {
        return this.readyPromise.then(resolve, reject);
    }
    async start() {
        const beforeElements = collectLayoutElements(this.scope);
        const beforeRecords = this.buildRecords(beforeElements);
        beforeRecords.forEach(({ projection }) => {
            const hasCurrentAnimation = Boolean(projection.currentAnimation);
            const isSharedLayout = Boolean(projection.options.layoutId);
            if (hasCurrentAnimation && isSharedLayout) {
                const snapshot = snapshotFromTarget(projection);
                if (snapshot) {
                    projection.snapshot = snapshot;
                }
                else if (projection.snapshot) {
                    projection.snapshot = undefined;
                }
            }
            else if (projection.snapshot &&
                (projection.currentAnimation || projection.isProjecting())) {
                projection.snapshot = undefined;
            }
            projection.isPresent = true;
            projection.willUpdate();
        });
        await this.updateDom();
        const afterElements = collectLayoutElements(this.scope);
        const afterRecords = this.buildRecords(afterElements);
        this.handleExitingElements(beforeRecords, afterRecords);
        afterRecords.forEach(({ projection }) => {
            const instance = projection.instance;
            const resumeFromInstance = projection.resumeFrom
                ?.instance;
            if (!instance || !resumeFromInstance)
                return;
            if (!("style" in instance))
                return;
            const currentTransform = instance.style.transform;
            const resumeFromTransform = resumeFromInstance.style.transform;
            if (currentTransform &&
                resumeFromTransform &&
                currentTransform === resumeFromTransform) {
                instance.style.transform = "";
                instance.style.transformOrigin = "";
            }
        });
        afterRecords.forEach(({ projection }) => {
            projection.isPresent = true;
        });
        const root = getProjectionRoot(afterRecords, beforeRecords);
        root?.didUpdate();
        await new Promise((resolve) => {
            frame.postRender(() => resolve());
        });
        const animations = collectAnimations(afterRecords);
        const animation = new GroupAnimation(animations);
        return animation;
    }
    buildRecords(elements) {
        const records = [];
        const recordMap = new Map();
        for (const element of elements) {
            const parentRecord = findParentRecord(element, recordMap, this.scope);
            const { layout, layoutId } = readLayoutAttributes(element);
            const override = layoutId
                ? this.sharedTransitions.get(layoutId)
                : undefined;
            const transition = override || this.defaultOptions;
            const record = getOrCreateRecord(element, parentRecord?.projection, {
                layout,
                layoutId,
                animationType: typeof layout === "string" ? layout : "both",
                transition: transition,
            });
            recordMap.set(element, record);
            records.push(record);
        }
        return records;
    }
    handleExitingElements(beforeRecords, afterRecords) {
        const afterElementsSet = new Set(afterRecords.map((record) => record.element));
        beforeRecords.forEach((record) => {
            if (afterElementsSet.has(record.element))
                return;
            // For shared layout elements, relegate to set up resumeFrom
            // so the remaining element animates from this position
            if (record.projection.options.layoutId) {
                record.projection.isPresent = false;
                record.projection.relegate();
            }
            record.visualElement.unmount();
            visualElementStore.delete(record.element);
        });
        // Clear resumeFrom on EXISTING nodes that point to unmounted projections
        // This prevents crossfade animation when the source element was removed entirely
        // But preserve resumeFrom for NEW nodes so they can animate from the old position
        // Also preserve resumeFrom for lead nodes that were just promoted via relegate
        const beforeElementsSet = new Set(beforeRecords.map((record) => record.element));
        afterRecords.forEach(({ element, projection }) => {
            if (beforeElementsSet.has(element) &&
                projection.resumeFrom &&
                !projection.resumeFrom.instance &&
                !projection.isLead()) {
                projection.resumeFrom = undefined;
                projection.snapshot = undefined;
            }
        });
    }
}
function parseAnimateLayoutArgs(scopeOrUpdateDom, updateDomOrOptions, options) {
    // animateLayout(updateDom)
    if (typeof scopeOrUpdateDom === "function") {
        return {
            scope: document,
            updateDom: scopeOrUpdateDom,
            defaultOptions: updateDomOrOptions,
        };
    }
    // animateLayout(scope, updateDom, options?)
    const elements = resolveElements(scopeOrUpdateDom);
    const scope = elements[0] || document;
    return {
        scope,
        updateDom: updateDomOrOptions,
        defaultOptions: options,
    };
}
function collectLayoutElements(scope) {
    const elements = Array.from(scope.querySelectorAll(layoutSelector));
    if (scope instanceof Element && scope.matches(layoutSelector)) {
        if (!elements.includes(scope)) {
            elements.unshift(scope);
        }
    }
    return elements;
}
function readLayoutAttributes(element) {
    const layoutId = element.getAttribute("data-layout-id") || undefined;
    const rawLayout = element.getAttribute("data-layout");
    let layout;
    if (rawLayout === "" || rawLayout === "true") {
        layout = true;
    }
    else if (rawLayout) {
        layout = rawLayout;
    }
    return {
        layout,
        layoutId,
    };
}
function createVisualState() {
    return {
        latestValues: {},
        renderState: {
            transform: {},
            transformOrigin: {},
            style: {},
            vars: {},
        },
    };
}
function getOrCreateRecord(element, parentProjection, projectionOptions) {
    const existing = visualElementStore.get(element);
    const visualElement = existing ??
        new HTMLVisualElement({
            props: {},
            presenceContext: null,
            visualState: createVisualState(),
        }, { allowProjection: true });
    if (!existing || !visualElement.projection) {
        visualElement.projection = new HTMLProjectionNode(visualElement.latestValues, parentProjection);
    }
    visualElement.projection.setOptions({
        ...projectionOptions,
        visualElement,
    });
    if (!visualElement.current) {
        visualElement.mount(element);
    }
    else if (!visualElement.projection.instance) {
        // Mount projection if VisualElement is already mounted but projection isn't
        // This happens when animate() was called before animateLayout()
        visualElement.projection.mount(element);
    }
    if (!existing) {
        visualElementStore.set(element, visualElement);
    }
    return {
        element,
        visualElement,
        projection: visualElement.projection,
    };
}
function findParentRecord(element, recordMap, scope) {
    let parent = element.parentElement;
    while (parent) {
        const record = recordMap.get(parent);
        if (record)
            return record;
        if (parent === scope)
            break;
        parent = parent.parentElement;
    }
    return undefined;
}
function getProjectionRoot(afterRecords, beforeRecords) {
    const record = afterRecords[0] || beforeRecords[0];
    return record?.projection.root;
}
function collectAnimations(afterRecords) {
    const animations = new Set();
    afterRecords.forEach((record) => {
        const animation = record.projection.currentAnimation;
        if (animation)
            animations.add(animation);
    });
    return Array.from(animations);
}

export { LayoutAnimationBuilder, parseAnimateLayoutArgs };
//# sourceMappingURL=LayoutAnimationBuilder.mjs.map
