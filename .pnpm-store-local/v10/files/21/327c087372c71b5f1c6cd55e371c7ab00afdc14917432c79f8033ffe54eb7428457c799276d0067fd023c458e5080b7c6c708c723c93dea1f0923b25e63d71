import { isDragActive } from './drag/state/is-active.mjs';
import { setupGesture } from './utils/setup.mjs';

function isValidHover(event) {
    return !(event.pointerType === "touch" || isDragActive());
}
/**
 * Create a hover gesture. hover() is different to .addEventListener("pointerenter")
 * in that it has an easier syntax, filters out polyfilled touch events, interoperates
 * with drag gestures, and automatically removes the "pointerennd" event listener when the hover ends.
 *
 * @public
 */
function hover(elementOrSelector, onHoverStart, options = {}) {
    const [elements, eventOptions, cancel] = setupGesture(elementOrSelector, options);
    elements.forEach((element) => {
        let isPressed = false;
        let deferredHoverEnd = false;
        let hoverEndCallback;
        const removePointerLeave = () => {
            element.removeEventListener("pointerleave", onPointerLeave);
        };
        const endHover = (event) => {
            if (hoverEndCallback) {
                hoverEndCallback(event);
                hoverEndCallback = undefined;
            }
            removePointerLeave();
        };
        const onPointerUp = (event) => {
            isPressed = false;
            window.removeEventListener("pointerup", onPointerUp);
            window.removeEventListener("pointercancel", onPointerUp);
            if (deferredHoverEnd) {
                deferredHoverEnd = false;
                endHover(event);
            }
        };
        const onPointerDown = () => {
            isPressed = true;
            window.addEventListener("pointerup", onPointerUp, eventOptions);
            window.addEventListener("pointercancel", onPointerUp, eventOptions);
        };
        const onPointerLeave = (leaveEvent) => {
            if (leaveEvent.pointerType === "touch")
                return;
            if (isPressed) {
                deferredHoverEnd = true;
                return;
            }
            endHover(leaveEvent);
        };
        const onPointerEnter = (enterEvent) => {
            if (!isValidHover(enterEvent))
                return;
            deferredHoverEnd = false;
            const onHoverEnd = onHoverStart(element, enterEvent);
            if (typeof onHoverEnd !== "function")
                return;
            hoverEndCallback = onHoverEnd;
            element.addEventListener("pointerleave", onPointerLeave, eventOptions);
        };
        element.addEventListener("pointerenter", onPointerEnter, eventOptions);
        element.addEventListener("pointerdown", onPointerDown, eventOptions);
    });
    return cancel;
}

export { hover };
//# sourceMappingURL=hover.mjs.map
