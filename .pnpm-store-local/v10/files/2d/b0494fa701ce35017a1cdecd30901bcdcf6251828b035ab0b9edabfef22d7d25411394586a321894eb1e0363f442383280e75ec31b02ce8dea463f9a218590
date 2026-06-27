"use client";
import { jsx } from 'react/jsx-runtime';
import { invariant } from 'motion-utils';
import { forwardRef, useRef, useEffect } from 'react';
import { ReorderContext } from '../../context/ReorderContext.mjs';
import { motion } from '../../render/components/motion/proxy.mjs';
import { useConstant } from '../../utils/use-constant.mjs';
import { checkReorder } from './utils/check-reorder.mjs';

function ReorderGroupComponent({ children, as = "ul", axis = "y", onReorder, values, ...props }, externalRef) {
    const Component = useConstant(() => motion[as]);
    const order = [];
    const isReordering = useRef(false);
    const groupRef = useRef(null);
    invariant(Boolean(values), "Reorder.Group must be provided a values prop", "reorder-values");
    const context = {
        axis,
        groupRef,
        registerItem: (value, layout) => {
            // If the entry was already added, update it rather than adding it again
            const idx = order.findIndex((entry) => value === entry.value);
            if (idx !== -1) {
                order[idx].layout = layout[axis];
            }
            else {
                order.push({ value: value, layout: layout[axis] });
            }
            order.sort(compareMin);
        },
        updateOrder: (item, offset, velocity) => {
            if (isReordering.current)
                return;
            const newOrder = checkReorder(order, item, offset, velocity);
            if (order !== newOrder) {
                isReordering.current = true;
                // Find which two values swapped and apply that swap
                // to the full values array. This preserves unmeasured
                // items (e.g. in virtualized lists).
                const newValues = [...values];
                for (let i = 0; i < newOrder.length; i++) {
                    if (order[i].value !== newOrder[i].value) {
                        const a = values.indexOf(order[i].value);
                        const b = values.indexOf(newOrder[i].value);
                        if (a !== -1 && b !== -1) {
                            [newValues[a], newValues[b]] = [newValues[b], newValues[a]];
                        }
                        break;
                    }
                }
                onReorder(newValues);
            }
        },
    };
    useEffect(() => {
        isReordering.current = false;
    });
    // Combine refs if external ref is provided
    const setRef = (element) => {
        groupRef.current = element;
        if (typeof externalRef === "function") {
            externalRef(element);
        }
        else if (externalRef) {
            externalRef.current = element;
        }
    };
    /**
     * Disable browser scroll anchoring on the group container.
     * When items reorder, scroll anchoring can cause the browser to adjust
     * the scroll position, which interferes with drag position calculations.
     */
    const groupStyle = {
        overflowAnchor: "none",
        ...props.style,
    };
    return (jsx(Component, { ...props, style: groupStyle, ref: setRef, ignoreStrict: true, children: jsx(ReorderContext.Provider, { value: context, children: children }) }));
}
const ReorderGroup = /*@__PURE__*/ forwardRef(ReorderGroupComponent);
function compareMin(a, b) {
    return a.layout.min - b.layout.min;
}

export { ReorderGroup, ReorderGroupComponent };
//# sourceMappingURL=Group.mjs.map
