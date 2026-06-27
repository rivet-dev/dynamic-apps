var __spreadArray = (this && this.__spreadArray) || function (to, from, pack) {
    if (pack || arguments.length === 2) for (var i = 0, l = from.length, ar; i < l; i++) {
        if (ar || !(i in from)) {
            if (!ar) ar = Array.prototype.slice.call(from, 0, i);
            ar[i] = from[i];
        }
    }
    return to.concat(ar || Array.prototype.slice.call(from));
};
/**
 * Traverses a CSS selector AST, calling visitor functions for each node.
 *
 * @param node - The root AST node to start traversal from
 * @param visitor - Visitor function or object with enter/exit hooks
 *
 * @example
 * ```typescript
 * import { createParser, traverse } from 'css-selector-parser';
 *
 * const parse = createParser();
 * const selector = parse('div.foo > span#bar');
 *
 * // Simple visitor function
 * traverse(selector, (node, context) => {
 *   console.log(node.type, context.parents.length);
 * });
 *
 * // Visitor with enter/exit hooks
 * traverse(selector, {
 *   enter(node, context) {
 *     if (node.type === 'ClassName') {
 *       console.log('Found class:', node.name);
 *     }
 *   },
 *   exit(node, context) {
 *     console.log('Leaving:', node.type);
 *   }
 * });
 *
 * // Skip subtrees
 * traverse(selector, {
 *   enter(node, context) {
 *     if (node.type === 'PseudoClass') {
 *       // Don't visit children of pseudo-classes
 *       return false;
 *     }
 *   }
 * });
 * ```
 */
export function traverse(node, visitor) {
    var visitorObj = typeof visitor === 'function' ? { enter: visitor } : visitor;
    var state = {
        visitor: visitorObj,
        parents: []
    };
    visitNode(node, state, undefined, undefined, undefined);
}
/**
 * Visits a single node and its children.
 */
function visitNode(node, state, parent, key, index) {
    var context = {
        node: node,
        parent: parent,
        parents: __spreadArray([], state.parents, true),
        key: key,
        index: index
    };
    // Call enter hook
    var skipChildren = false;
    if (state.visitor.enter) {
        var result = state.visitor.enter(node, context);
        if (result === false) {
            skipChildren = true;
        }
    }
    // Visit children unless skipped
    if (!skipChildren) {
        state.parents.push(node);
        visitChildren(node, state);
        state.parents.pop();
    }
    // Call exit hook
    if (state.visitor.exit) {
        state.visitor.exit(node, context);
    }
}
/**
 * Visits all children of a node based on its type.
 */
function visitChildren(node, state) {
    switch (node.type) {
        case 'Selector':
            visitSelector(node, state);
            break;
        case 'Rule':
            visitRule(node, state);
            break;
        case 'TagName':
            visitTagName(node, state);
            break;
        case 'WildcardTag':
            visitWildcardTag(node, state);
            break;
        case 'Attribute':
            visitAttribute(node, state);
            break;
        case 'PseudoClass':
            visitPseudoClass(node, state);
            break;
        case 'PseudoElement':
            visitPseudoElement(node, state);
            break;
        case 'FormulaOfSelector':
            visitFormulaOfSelector(node, state);
            break;
        // Leaf nodes with no children
        case 'Id':
        case 'ClassName':
        case 'NamespaceName':
        case 'WildcardNamespace':
        case 'NoNamespace':
        case 'NestingSelector':
        case 'String':
        case 'Formula':
        case 'Substitution':
            // No children to visit
            break;
    }
}
function visitSelector(node, state) {
    node.rules.forEach(function (rule, index) {
        visitNode(rule, state, node, 'rules', index);
    });
}
function visitRule(node, state) {
    node.items.forEach(function (item, index) {
        visitNode(item, state, node, 'items', index);
    });
    if (node.nestedRule) {
        visitNode(node.nestedRule, state, node, 'nestedRule', undefined);
    }
}
function visitTagName(node, state) {
    if (node.namespace) {
        visitNode(node.namespace, state, node, 'namespace', undefined);
    }
}
function visitWildcardTag(node, state) {
    if (node.namespace) {
        visitNode(node.namespace, state, node, 'namespace', undefined);
    }
}
function visitAttribute(node, state) {
    if (node.namespace) {
        visitNode(node.namespace, state, node, 'namespace', undefined);
    }
    if (node.value) {
        visitNode(node.value, state, node, 'value', undefined);
    }
}
function visitPseudoClass(node, state) {
    if (node.argument) {
        visitNode(node.argument, state, node, 'argument', undefined);
    }
}
function visitPseudoElement(node, state) {
    if (node.argument) {
        visitNode(node.argument, state, node, 'argument', undefined);
    }
}
function visitFormulaOfSelector(node, state) {
    visitNode(node.selector, state, node, 'selector', undefined);
}
