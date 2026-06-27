# css-selector-parser

[![npm](https://img.shields.io/npm/v/css-selector-parser)](https://www.npmjs.com/package/css-selector-parser)
[![npm bundle size](https://img.shields.io/bundlephobia/minzip/css-selector-parser)](https://bundlephobia.com/package/css-selector-parser)
[![NPM License](https://img.shields.io/npm/l/css-selector-parser)](https://github.com/mdevils/css-selector-parser/blob/master/LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/mdevils/css-selector-parser)](https://github.com/mdevils/css-selector-parser/stargazers)

A high-performance CSS selector parser with advanced features for modern web development.

## Features

- 🚀 **Fast and memory-efficient** parsing for all CSS selectors
- 🌳 **AST-based** object model for programmatic manipulation
- 🚶 **AST traversal** with visitor pattern for analyzing and transforming selectors
- 📊 **Full compliance** with all CSS selector specifications
- 🧪 **Comprehensive test coverage**
- 📚 **Well-documented API** with TypeScript support
- 🔄 **Two-way conversion** between CSS selectors and AST
- 🧩 **Modular support** for various CSS specifications
- 🎮 **[Interactive Playground](https://mdevils.github.io/css-selector-parser/)** - Try it in your browser!

## Playground

**[🎮 Launch Interactive Playground](https://mdevils.github.io/css-selector-parser/)**

Test CSS selectors in your browser with syntax highlighting, real-time AST visualization, and configurable parser options.

## Supported CSS Selector Standards

- `css1`: [W3C CSS1 Specification](https://www.w3.org/TR/CSS1/)
- `css2`: [W3C CSS2 Specification](https://www.w3.org/TR/CSS2/)
- `css3`/`selectors-3`: [W3C Selectors Level 3](https://www.w3.org/TR/selectors-3/)
- `selectors-4`: [W3C Selectors Level 4](https://www.w3.org/TR/selectors-4/)
- `latest`: refers to `selectors-4`
- `progressive`: `latest` + accepts unknown pseudo-classes, pseudo-elements and attribute case sensitivity modifiers

## Migration Guides

- [Migrating from 1.x to 3.x](CHANGELOG.md#migrating-from-1x-to-3x)
- [Migrating from 2.x to 3.x](CHANGELOG.md#migrating-from-2x-to-3x)
- [Migrating from 1.x to 2.x](CHANGELOG.md#220)

See [Changelog](CHANGELOG.md) for release details.

## Installation

```bash
npm install css-selector-parser
# or
yarn add css-selector-parser
# or
pnpm add css-selector-parser
```

## Usage

### Parsing Selectors

```javascript
import { createParser } from 'css-selector-parser';

const parse = createParser();
const selector = parse('a[href^="/"], .container:has(nav) > a[href]:nth-child(2)::before');

console.log(selector);
```

This produces an AST (Abstract Syntax Tree) output:

```javascript
({
    type: 'Selector',
    rules: [
        {
            type: 'Rule',
            items: [
                { type: 'TagName', name: 'a' },
                {
                    type: 'Attribute',
                    name: 'href',
                    operator: '^=',
                    value: { type: 'String', value: '/' }
                }
            ]
        },
        {
            type: 'Rule',
            items: [
                { type: 'ClassName', name: 'container' },
                {
                    type: 'PseudoClass',
                    name: 'has',
                    argument: {
                        type: 'Selector',
                        rules: [
                            {
                                type: 'Rule',
                                items: [ { type: 'TagName', name: 'nav' } ]
                            }
                        ]
                    }
                }
            ],
            nestedRule: {
                type: 'Rule',
                items: [
                    { type: 'TagName', name: 'a' },
                    { type: 'Attribute', name: 'href' },
                    {
                        type: 'PseudoClass',
                        name: 'nth-child',
                        argument: { type: 'Formula', a: 0, b: 2 }
                    },
                    {
                        type: 'PseudoElement',
                        name: 'before'
                    }
                ],
                combinator: '>'
            }
        }
    ]
})
```

### Building and Rendering Selectors

```javascript
import { ast, render } from 'css-selector-parser';

const selector = ast.selector({
    rules: [
        ast.rule({
            items: [
                ast.tagName({name: 'a'}),
                ast.attribute({name: 'href', operator: '^=', value: ast.string({value: '/'})})
            ]
        }),
        ast.rule({
            items: [
                ast.className({name: 'container'}),
                ast.pseudoClass({
                    name: 'has',
                    argument: ast.selector({
                        rules: [ast.rule({items: [ast.tagName({name: 'nav'})]})]
                    })
                })
            ],
            nestedRule: ast.rule({
                combinator: '>',
                items: [
                    ast.tagName({name: 'a'}),
                    ast.attribute({name: 'href'}),
                    ast.pseudoClass({
                        name: 'nth-child',
                        argument: ast.formula({a: 0, b: 2})
                    }),
                    ast.pseudoElement({name: 'before'})
                ]
            })
        })
    ]
});

console.log(render(selector)); // a[href^="/"], .container:has(nav) > a[href]:nth-child(2)::before
```

### Traversing the AST

The `traverse` function allows you to walk through the AST and visit each node, making it easy to analyze or transform selectors.

```javascript
import { createParser, traverse } from 'css-selector-parser';

const parse = createParser();
const selector = parse('div.foo > span#bar:hover::before');

// Simple visitor function - called for each node
traverse(selector, (node, context) => {
    console.log(node.type, context.parents.length);
});

// Visitor with enter/exit hooks
traverse(selector, {
    enter(node, context) {
        console.log('Entering:', node.type);
        if (node.type === 'ClassName') {
            console.log('Found class:', node.name);
        }
    },
    exit(node, context) {
        console.log('Leaving:', node.type);
    }
});

// Skip visiting children of specific nodes
traverse(selector, (node) => {
    if (node.type === 'PseudoClass') {
        // Don't visit children of pseudo-classes
        return false;
    }
});

// Practical example: collect all class names
const classNames = [];
traverse(selector, (node) => {
    if (node.type === 'ClassName') {
        classNames.push(node.name);
    }
});
console.log(classNames); // ['foo']

// Access parent information
traverse(selector, (node, context) => {
    console.log({
        type: node.type,
        parent: context.parent?.type,
        depth: context.parents.length,
        key: context.key,
        index: context.index
    });
});
```

The traversal context provides:
- `node`: The current AST node being visited
- `parent`: The parent node (undefined for root)
- `parents`: Array of all ancestor nodes from root to current
- `key`: Property name in parent that references this node
- `index`: Array index if this node is in an array

## CSS Modules Support

CSS Modules are specifications that add new selectors or modify existing ones. This parser supports various CSS modules that can be included in your syntax definition:

```javascript
import { createParser } from 'css-selector-parser';

// Create a parser with specific CSS modules enabled
const parse = createParser({
    syntax: 'selectors-4',
    modules: ['css-position-3', 'css-scoping-1']
});
```

### Supported CSS Modules

| Module | Description |
|--------|-------------|
| `css-position-1/2/3/4` | Position-related pseudo-classes |
| `css-scoping-1` | Shadow DOM selectors (`:host`, `:host-context()`, `::slotted()`) |
| `css-pseudo-4` | Modern pseudo-elements (`::selection`, `::backdrop`, etc.) |
| `css-shadow-parts-1` | `::part()` for styling shadow DOM components |
| `css-nesting-1` | CSS Nesting selector (`&`) |

The `latest` syntax automatically includes all modules marked as current specifications.

## API Documentation

- [Complete API Documentation](docs/modules.md)
- [Parsing CSS Selectors](docs/modules.md#createParser)
- [Constructing CSS AST](docs/modules.md#ast)
- [Rendering CSS AST](docs/modules.md#render)
- [Traversing CSS AST](docs/modules.md#traverse)

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

MIT

## Security Contact

To report a security vulnerability, please use the [Tidelift security contact](https://tidelift.com/security). Tidelift will coordinate the fix and disclosure.

## Sponsorship

If you find this project useful, please consider [sponsoring the developer](https://github.com/sponsors/mdevils) or [supporting on Patreon](https://patreon.com/mdevils).
