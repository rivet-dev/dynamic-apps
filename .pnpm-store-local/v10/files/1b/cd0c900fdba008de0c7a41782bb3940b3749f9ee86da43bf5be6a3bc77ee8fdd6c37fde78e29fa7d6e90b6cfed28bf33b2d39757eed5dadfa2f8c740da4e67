import { AstEntity } from './ast.js';
/**
 * Visitor function that is called for each node during traversal.
 * Return `false` to skip visiting children of this node.
 * Return `true` or `undefined` to continue traversal normally.
 */
export type VisitorFunction = (node: AstEntity, context: TraversalContext) => void | boolean | undefined;
/**
 * Visitor object with optional enter and exit hooks.
 * - `enter`: Called when first visiting a node (before its children)
 * - `exit`: Called when leaving a node (after its children)
 */
export interface Visitor {
    /**
     * Called when entering a node (before visiting its children).
     * Return `false` to skip visiting children of this node.
     */
    enter?: VisitorFunction;
    /**
     * Called when exiting a node (after visiting its children).
     */
    exit?: VisitorFunction;
}
/**
 * Context information provided to visitor functions during traversal.
 */
export interface TraversalContext {
    /** The current node being visited */
    node: AstEntity;
    /** Parent node (undefined for root) */
    parent?: AstEntity;
    /** Path of parent nodes from root to current node */
    parents: AstEntity[];
    /** Property name in parent that references this node */
    key?: string;
    /** Array index if this node is in an array */
    index?: number;
}
/**
 * Options for controlling traversal behavior.
 */
export interface TraverseOptions {
    /** Custom visitor implementation */
    visitor?: Visitor | VisitorFunction;
}
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
export declare function traverse(node: AstEntity, visitor: Visitor | VisitorFunction): void;
