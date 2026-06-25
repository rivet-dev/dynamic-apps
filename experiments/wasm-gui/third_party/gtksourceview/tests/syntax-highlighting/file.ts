/*
 * Type
 */

// Predefined type
let a: any;
let a: bigint;
let a: boolean;
let a: null;
let a: number;
let a: object;
let a: string;
let a: symbol;
let a: undefined;
let a: unknown;
let a: void;
function fn(): never { throw new Error(); }

// Parenthesized type
let a: ( string );

// Type reference
let a: T;
let a: Super.Sub;
// Bullt-in constructors
let a: Array<string>;
let a: Function;
let a: RegExp;
// Built-in utility types
let a: ReadonlyArray<number>;
let a: Partial<Person>;
let a: Readonly<Person>;
// Import type
let a: import("module").ModuleType;

// Object type
let a: {
    // Property signature
    property;
    property?,
    property: string;
    readonly property?: string,

    // Call signature
    ();
    <T>(x),
    (this: void, x: number, y?: string): void;
    <T extends Function>(x, ...rest): void,

    // Construct signature
    new ();
    new <T>(x),
    new (x: number, y?: string): void;
    new <T extends Function>(x, ...rest): void,

    // Index signature
    [index: number]: string;
    readonly [prop: string]: any,

    // Method signature
    method();
    method?<T>(x),
    method?(this: void, x: number, y?: string): void;
    method<T extends Function>(x, ...rest): void,

    // Mapped type
    readonly [P in keyof T]?: T[P],
    -readonly [P in keyof T]+?: T[P];
};

// Array type
let a: string[];

// Indexed access type (lookup type)
let a: MyType["property"];

// Tuple type
let a: [string];
let a: [string, number, boolean?]; // optional element
let a: [string, ...number[]]; // rest elements

// Type query
let a: typeof Super.Sub;

// This type
let a: this;

// String literal type
let a: 'string';
let a: "string";

// Numeric literal type
let a: 123;
let a: 0b0101;
let a: 0o777;
let a: 0xfff;
// Numeric separators with BigInt
let a: 1_23n;
let a: 0b01_01n;
let a: 0o7_77n;
let a: 0xf_f_fn;

// Boolean literal type
let a: true;
let a: false;

// "unique symbol" subtype
let a: unique symbol;
let a: unique /* comment */ symbol;
let a: unique /* comment
*/ symbol;
let a: unique // comment
symbol;

// Union / intersection type
let a: string | number;
let a: string & number;

// Type predicate (user-defined type guard)
function isString(x: any): x is string {}

// "asserts" type predicate (for assertion functions)
declare function assert(value: unknown): asserts value;
declare function assertIsArrayOfStrings(obj: unknown): asserts obj is string[];
declare function assertNonNull<T>(obj: T): asserts obj is NonNullable<T>;

// Indexed type query (keyof)
let a: keyof T;

// Conditional type
let a: T extends number ? number[] : T[];

// Type inference (in conditional types)
let a: T extends (infer U)[] ? U : any;
let a: T extends (...args: any[]) => infer U ? U : any;
let a: T extends Promise<infer U> ? U : any;

// Function type
let a: () => void;
let a: <T>(x) => T;
let a: (this: void, x: number, y?: string) => string; // this parameter
let a: <T extends Function>(x, ...rest) => T;

let a: ([x, y]: [string, number], [z, w]: [string, number]) => void;
let a: ({ x: a, y: b }: { x: number, y: string }, { z: c, w: d }: { z: number, w: string }) => void;

// Constructor type
let a: new () => void;
let a: new <T>(x) => T;
let a: new (x: number, y?: string) => string;
let a: new <T extends Function>(x, ...rest) => T;

let a: new ([x, y]: [string, number], [z, w]: [string, number]) => void;
let a: new ({ x: a, y: b }: { x: number, y: string }, { z: c, w: d }: { z: number, w: string }) => void;

// Readonly array / tuple types
let a: readonly string[];
let a: readonly /* comment */ [string, string];

// Parenthesized type vs parameters list of function type
// Parenthesized
let a: (string);
let a: ((string: string) => void);
// Parameters list
let a: (string: string) => void;
let a: (string?
: string) => void;
let a: (string /* comment */ ,
string) => void;
let a: (this: void) => void;
let a: (this /* comment */ : void) => void;
let a: (...string
: string[]) => void;
// Incorrectly highlighted
let a: (string
: string) => void;
let a: (this /* comment
*/ : void) => void;
let a: (... string
: string[]) => void;
let a: (... /* comment */ string
: string[]) => void;


/*
 * Type parameters
 */

function fn<T>() {}
function fn<T, U>() {}
function fn<T = string, U extends V = any, V extends Function>() {}
function fn<T, K extends keyof T>() {}


/*
 * Type parameters (for arrow function) / type assertion (cast)
 */

// Type parameters
a = <T>(x) => x;
a = <T, U>(x) => x;
a = <T = string, U extends V = any, V extends Function>(x) => x;
a = <T, K extends keyof T>(x) => x;

// Type assertion
a = <string>obj;
a = <const>"abc"; // const assertion


/*
 * Type arguments
 */

fn<string>();
fn<string, number>();


/*
 * Type annotation
 */

let a: string;


/*
 * TypeScript-specific operators
 */

/* as operator (type assertion / cast) */

( obj as string );
( "abc" as const ); // const assertion


// Non-null assertion operator (post-fix !)
( a!.method() );


/*
 * TypeScript-specific statements and declarations
 */

/* @ts-ignore / @ts-expect-error comment pragmas */

// Valid pragmas
//@ts-ignore
/// @ts-expect-error add reason here
{
    //  @ts-ignore
}
/* @ts-expect-error*/
/*
 * @ts-ignore more reasons */

// Invalid pragmas
// @ ts-ignore
/// @TS-IGNORE
/* @ts-expect-error
 */


/* @ts-nocheck comment pragmas */

// Valid pragmas
//@ts-nocheck
/// @TS-NOCHECK text here

// Invalid pragmas
// @ ts-nocheck
//// @ts-nocheck
/* @ts-nocheck */
{
    // @ts-nocheck
}


/* Triple-slash directives */

// Valid directives
///<reference path="foo" />
/// <REFERENCE lib='es2017.string'/>
/// <amd-module name = "bar/>" />
///  <aMd-dEpEnDeNcY/>

// Invalid directives
/// comment
/// <comment />
/// <reference /
/// < reference />
/// <reference / >
/// <reference-path="foo" />
/// <reference path />
/// <reference path "foo" />
/// <reference path="/>
/// <reference path=foo />
/// <reference path='foo" />
/// <reference path="foo"name="bar" />
{
    /// <reference path="foo" />
}


/* Decorators (experimental, stage 2 proposal) */

// Class decorator
@sealed
class Greeter {
    // Property decorator
    @(Deco) /* comment */ . /* comment */ utils /* comment */ () /* comment */ . /* comment */ format /* comment */ ("Hello, %s")
    greeting: string;

    // Method decorator
    @ /* comment */ (foo = 'bar', false || validate)
    greet(@required name: string) { // Parameter decorator
        return "Hello " + name + ", " + this.greeting;
    }

    constructor(@required public greeting: string) {} // Parameter decorator

    // Accessor decorator
    @configurable<string>(false) /* comment */
    get x() { return this._x; }
}


/* Ambient declaration */

declare let a;
declare const a: number, b: string;
declare function fn();
declare function fn<T>(x): T;
declare function fn(this: void, x: number, y?: string): string;
declare function fn<T extends Function>(x, ...rest): T;
declare class MyClass extends Super implements Super.Sub {}
declare abstract class MyClass<T extends MyClass> extends Super<string> {}
declare abstract /* comment */ class MyClass<T extends MyClass> extends Super<string> {}
declare enum Color { Red, Green, Blue }
declare const enum Num { One = 1, Two, Three }
declare global {}
declare module Super.Sub {}
declare module "module" {}
declare module "module";
declare namespace Super.Sub {}


/* Enum declaration */

enum Color {
    Red,
    Green,
    Blue
}
const enum Num {
    One = 1,
    Two = '2',
    Three
}


/* Interface declaration */

interface MyObj {
    property,
    <T extends Function>(): void,
    readonly [index: number]: string,
    method?(this: void, x: number, y?: string): void
}
interface Square<T> extends Shape, PenStroke<number> {
    sideLength: number;
}


/* Module declaration */

module Super.Sub { // namespace ("internal module")
    let a = 1;
}
module "module" { // "external module"
    let a = 1;
}


/* Namespace declaration */

namespace Super.Sub {
    let a = 1;
}


/* Type alias declaration */

type Name = string;
type NameResolver = () => string;
type NameOrResolver = Name | NameResolver;
type Container<T> = { value: T };


/*
 * Modifications to existing statements and declarations
 */

/* Literals */

// Numeric separators (stage 3 proposal)
let decimal = 1_000_000;
let binary_integer = 0b1100_0011_1101_0001;
let octal_integer = 0o123_456_700;
let hex_integer = 0xFF_0C_00_FF;
// Numeric separators with BigInt
let decimal_bigint = 1_000_000n;
let binary_bigint = 0B1100_0011_1101_0001n;
let octal_bigint = 0O123_456_700n;
let hex_bigint = 0XFF_0C_00_FFn;


/* Object literal */

// Property value vs type annotation
a = {
    property: void 1,
    method(): void {}
};

// Type annotations for methods
a = {
    method(this: void, x: number, y?: string, z: number = 0, ...rest: any[]): void {},
    get property(): string {},
    set property(value: string) {}
};

// Type parameters for methods
a = {
    method<T extends Function>(x: T): T {},
    get<T extends Function>(x: T): T {}
};
// Incorrectly highlighted as keyword
a = {
    get
    <T>(x: T): T {}
};


/* Function expression / declaration */

// Type annotations
a = function (this: void, x: number, y?: string, z: number = 1, ...rest: any[]): void {};
function fn(this: void, x: number, y?: string, z: number = 1, ...rest: any[]): void {}

// Type parameters
a = function <T extends Function>(x: T): T {};
function fn<T extends Function>(x: T): T {}


/* Grouping / arrow function parameters */

// Type annotations
a = (x: number, y?: string, z: number = 0, ...rest: any[]): void => x + y;

// Type parameters
a = <T extends Function>(x: T): T => x;


/* Class expression / declaration */

// Abstract class
abstract class MyClass {}
abstract /* comment */ class MyClass {}
abstract /* comment
*/ class MyClass {} // correctly highlighted, because abstract cannot be followed by a line terminator

// Type parameters
a = class <T> {};
a = class <T extends MyClass> extends Super {};
class MyClass<T> {}
class MyClass<T extends MyClass> extends Super {}

// Extends clause with type arguments
a = class extends Super<string> {};
class MyClass extends Super<string> {}

// Implements clause
a = class implements Super.Sub {};
a = class extends Super implements Super.Sub {};
class MyClass implements Super.Sub {}
class MyClass extends Super implements Super.Sub {}

// Class properties (stage 3 proposal)
a = class {
    property;
    property = 1;
    constructor = 1;
};

// Type annotation
a = class {
    property: number;
    property: number = 1;

    constructor(x: number, y?: string) {}
    constructor(x: number = 1) {}

    method(this: void, x: number, y?: string, z: number = 1, ...rest: any[]): void {}
    get property(): string {}
    set property(value: string) {}
};

// Index members
a = class {
    [index: number]: string;
};

// Private fields (stage 3 proposal)
a = class {
    #privateprop;
    #privateprop: string = '';
    setPrivateProp(s: string) { this.#privateprop = s; }
};

// Accessibility modifiers for properties / methods
a = class {
    public property;
    protected method() {}
    private get property(): string {}
};

// Abstract properties / methods
a = abstract class {
    abstract property;
    abstract method(): void;
};

// Read-only properties / index members
a = class {
    readonly property;
    readonly [prop: string]: any;
};

// "declare" properties (no emit when useDefineForClassFields)
a = class {
    declare property;
};

// Optional properties / methods
a = class {
    property?;
    method?(): void;
};

// Definite assignment assertion for properties
a = class {
    property!;
    property!: number;
};

// Class method type parameters
a = class {
    method<T extends Function>(x: T): T {}
};

// Parameter properties for constructor
a = class {
    constructor(public x: number, private readonly y?: string) {}
    constructor(protected x: number = 1) {}

    // Accessibility / read-only modifiers do not apply to normal methods
    method(public x: number, private readonly y?: string) {}
};

// Multiple modifiers
a = class {
    public static readonly property;
    private abstract method();

    // "declare" can be in any position
    declare protected abstract readonly property;
    protected declare abstract readonly property;
    protected abstract declare readonly property;
    protected abstract readonly declare property;
};

// Modifier-named properties / methods
a = class {
    public;
    declare protected?;
    private!;
    public: number;
    protected = 1;
    private() {}
    public<T>(x: T): T { return x; }

    abstract;
    public abstract?;
    protected abstract!;
    private abstract: number;
    declare abstract = 1;
    protected abstract() {}
    private abstract<T>(x: T): T { return x; }

    readonly;
    public readonly?;
    static readonly!;
    abstract readonly: number;
    declare readonly = 1;
    protected readonly() {}
    private readonly<T>(x: T): T { return x; }

    declare;
    static declare?;
    abstract declare!;
    readonly declare: number;
    declare declare = 1;
    public declare() {}
    protected declare<T>(x: T): T { return x; }
};

// Syntax errors
// Multiple accessibility modifiers
a = class { public private property; };
a = class { private protected property; };
a = class { protected public property; };
// "static" and "abstract" both present
a = class { static abstract property; };
a = class { abstract static property; };
// "static" / "abstract" before accessibility modifier
a = class { static private property; };
a = class { abstract protected property; };
// "readonly" before accessibility modifier
a = class { readonly public property; };
// "readonly" before "static" / "abstract"
a = class { readonly static property; };
a = class { readonly abstract property; };

// Incorrectly highlighted
// Property/method name highlighted as keyword
a = class {
    public
    ;
    declare private
    ?;
    abstract readonly
    : number;
    readonly declare
    = 1;
    protected abstract
    () {}
};
// Abstract generators not allowed
abstract class Foo {
    abstract *generator(): object;
}
// Modifiers other than "readonly" do not apply to index members
a = class {
    public [prop: string]: any;
    protected [prop: string]: any;
    private [prop: string]: any;
    static [prop: string]: any;
    abstract [prop: string]: any;
    declare [prop: string]: any;
};
// Modifiers other than "readonly" do not apply to private fields
a = class {
    public #privateprop;
    protected #privateprop;
    private #privateprop;
    static #privateprop;
    abstract #privateprop;
    declare #privateprop;
};
// Constructor not highlighted as built-in method
a = class {
    constructor
    () {}
};
// "abstract" and "declare" do not apply to constructors
a = class {
    abstract constructor() {}
    declare constructor() {}
};


/* Expression */

// Type arguments for function calls
fn<string>();
fn<string, number>();
fn < string > /* comment */ ();
// Incorrectly highlighted (interpreted as less than / equal than)
fn<string
>();
fn<string>
();
fn<string> /* comment
*/ ();
fn<string> // comment
();

// Type arguments for tagged templates
myTag<string>`Template literal`;
myTag<string, number>`Template literal`;
myTag < string > /* comment */ `Template literal`;
// Incorrectly highlighted (interpreted as less than / equal than)
myTag<string
>`Template literal`;
myTag<string>
`Template literal`;
myTag<string> /* comment
*/ `Template literal`;
myTag<string> // comment
`Template literal`;

// Type assertion
a = <string>obj;
a = <const>"abc"; // const assertion


/* Export / import declaration */

// Export ambient declaration
export declare let a;
export declare const a: number, b: string;
export declare function fn();
export declare function fn<T>(x): T;
export declare function fn(this: void, x: number, y?: string): string;
export declare function fn<T extends Function>(x, ...rest): T;
export declare class MyClass extends Super implements Super.Sub {}
export declare abstract class MyClass<T extends MyClass> extends Super<string> {}
export declare abstract /* comment */ class MyClass<T extends MyClass> extends Super<string> {}
export declare enum Color { Red, Green, Blue }
export declare const enum Num { One = 1, Two, Three }
export declare module Super.Sub {}
export declare module "module" {}
export declare namespace Super.Sub {}

// Export enum declaration
export enum Color { Red, Green, Blue }
export const enum Num { One = 1, Two, Three }

// Export import alias
export import shortname = Long.Namespace.Name;

// Export interface declaration
export interface MyObj {}
export interface Square<T> extends Shape, PenStroke<number> {}

// Export module declaration
export module Super.Sub {}

// Export namespace declaration
export namespace Super.Sub {}

// Export type alias declaration
export type Name = string;
export type NameResolver = () => string;
export type NameOrResolver = Name | NameResolver;
export type Container<T> = { value: T };

// Export assignment
export = obj;

// Export as namespace (UMD module definition)
export as namespace myModule;

// Import alias
import shortname = Long.Namespace.Name;

// Import require
import mod = require("module");

// Type-only imports and exports
import type T from './mod';
import type { A, B } from './mod';
import type * as Types from './mod';
export type { T };
export type { T } from './mod';


/* Variable declaration */

// Type annotation
const a: number;
let a: number = 1, b: string;
var { a, b }: { a: number, b: string } = { a: 1, b: 'b' };

// Definite assignment assertion
let a!: number;


// from file.js

/*
 * Identifiers
 */

var example;
function example() {}

var príklad;
function príklad() {}

var 例子;
function 例子() {}

var $jquery;
function _lodash() {}

var \u0075nicod\u{65};
function \u0075nicod\u{65}() {}


/*
 * Expressions (in expression statements)
 */

/*
 * Literals
 */

/* Keyword values */

var NULL = null;
var TRUE = true;
var FALSE = false;


/* Number */

var decimal1 = 0;
var decimal2 = 123.45;
var decimal3 = .66667;
var decimal4 = 10e20;
var decimal5 = 0.2e+1;
var decimal6 = .5E-20;
var hex1 = 0xDEADBEEF;
var hex2 = 0Xcafebabe;

// ES2015 binary and octal numbers
let binary1 = 0b1010;
let binary2 = 0B00001111;
let octal1 = 0o0123;
let octal2 = 0O4567;

// Legacy octal numbers
var legacy_octal1 = 01;
var legacy_octal2 = 007;

// BigInt (ES2020)
var decimal1 = 0n;
var decimal2 = 123n;
var hex1 = 0xDEADBEEFn;
var hex2 = 0Xcafebaben;
var binary1 = 0b1010n;
var binary2 = 0B00001111n;
var octal1 = 0o0123n;
var octal2 = 0O4567n;


/* String */

// Escape sequences
'\b\f\n\r\t\v\0\'\"\\'; // Single character escape
"\1\01\001";            // Octal escape (Annex B)
'\xA9';                 // Hexadecimal escape
"\u00a9";               // Unicode escape
'\u{1D306}';            // Unicode code point escape


/* Array literal */

[];
[1];
[1.0, 'two', 0x03];

// Trailing comma
[
    [1,2,3],
    [4,5,6],
];

// Spread syntax
[1, ...a, 2];


/* Object literal */

a = {};
a = { prop: 'value' };
a = { 'prop': 'value', 1: true, .2: 2 };

// Trailing comma
a = {
    prop: 'value',
    "extends": 1,
};

// Shorthand property names
a = { b, c, d };

// Getter / setter
a = {
    _hidden: null,
    get property() { return _hidden; },
    set property(value) { this._hidden = value; },

    get: 'get',
    set() { return 'set'; }
};
// Incorrectly highlighted as keyword
a = {
    get
    : 'get',
    set
    () { return 'set'; }
};

// Shorthand function notation
a = {
    method() {},
    *generator() {},

    // Async function (ES2017)
    async method() {},
    async /* comment */ method() {},
    async get() {},
    async() {},// method called "async"
    async: false, // property called "async"

    // Async generator (ES2018)
    async *generator() {}
};

// Computed property names
a = {
    ['prop']: 1,
    ['method']() {}
};

// Spread properties (ES2018)
a = {
    ...b,
    ...getObj('string')
};

// Syntax errors
a = { prop: 'val': 'val' };
a = { method() {}: 'val' };
a = { get property() {}: 'val' };
a = { *generator: 'val' };
a = { async prop: 'val' };
a = { ...b: 'val' };
a = { ...b() { return 'b'; } };


/* Regular expression literal */

/abc/;
x = /abc/gi;
function_with_regex_arg(/abc/);
[ /abc/m, /def/u ];
a = { regex: /abc/s }; // s (dotAll): ES2018
(1 === 0) ? /abc/ : /def/;
/abc/; /* Comment */
/abc/; // Comment
var matches = /abc/.exec('Alphabet ... that should contain abc, right?');

// No regex here
a = [thing / thing, thing / thing];
x = a /b/ c / d;

// Character groups with backslashes
/[ab\\]/; // a, b or backslash
/[ab\]]/; // a, b or ]
/\\[ab]/; // a or b preceded by backslash
/\[ab]/;  // Literally "[ab]"

// Control escape
/\cJ/;

// Unicode property escape (ES2018)
/\p{General_Category=Letter}/u;
/\p{Letter}/u;

// Named capture groups (ES2018)
/(?<year>\d{4})-(?<month>\d{2})-(?<day>\d{2})/u;
/(?<foobar>foo|bar)/u;
/^(?<half>.*).\k<half>$/u; // backreference

/* Template literal */

console.log(`The sum of 2 and 2 is ${2 + 2}`);

let y = 8;
let my_string = `This is a multiline
string that also contains
a template ${y + (4.1 - 2.2)}`;


/*
 * Built-in values
 */

// global object values
Infinity;
NaN;
undefined;

// global object functions
decodeURIComponent();
decodeURI();
encodeURIComponent();
encodeURI();
eval();
isFinite();
isNaN();
parseFloat();
parseInt();

// constructors (subset)
Array();
BigInt(); // ES2020
Boolean();
Date();
Error();
Function();
Map();
Object();
Promise();
RegExp();
Set();
String();
Symbol();

// objects
JSON.parse();
Math.random();

// object keywords
arguments;
globalThis; // ES2020
super;
this;

// dynamic import (ES2020)
import("module").then();
import /* comment */ ("module").then();
import // comment
("module").then();
a = await import("module");
a = await import /* comment */ ("module");
a = await import // comment
("module");

// import.meta (ES2020)
import.meta;
import . /* comment */ meta;
import // comment
.meta;
a = import.meta;
a = import . /* comment */ meta;
a = import // comment
.meta;

// new.target
new.target;
new . /* comment */ target;
new // comment
.target;

// properties (subset)
array.length;
Math.PI;
Number.NaN;
object.constructor;
Class.prototype;
Symbol.asyncIterator; // ES2018
Symbol('desc').description; // ES2019

// methods (subset)
array.keys();
date.toString();
object.valueOf();
re.test();
array.includes(); // ES2016
Object.values(); // ES2017
Object.entries(); // ES2017
string.padStart(); // ES2017
string.padEnd(); // ES2017
Object.getOwnPropertyDescriptors(); // ES2017
promise.finally(); // ES2018
Object.fromEntries(); // ES2019
string.trimStart(); // ES2019
string.trimEnd(); // ES2019
array.flat(); // ES2019
array.flatMap(); // ES2019
string.matchAll(); // ES2020
Promise.allSettled(); // ES2020
BigInt.asUintN(); // ES2020
string.replaceAll(); // ES2021


/*
 * Function expression, arrow function
 */

a = function () { return 1 };
a = function fn() {
    return;
};
a = function fn(x) {};
a = function fn(x, y) {};

// Arrow function
x => -x;
() => {};
(x, y) => x + y;
(x, y) => { return x + y; };
(x, y) => /* comment */ { return x + y; } /* comment */ ;
(x) => ({ a: x }); // return object

// Default parameters
a = function fn(x, y = 1) {};
(x, y = 1) => x + y;

// Parameter without default after default parameters
a = function fn(x = 1, y) {};
(x = 1, y) => x + y;

// Array destructuring
a = function fn([x]) {};
a = function fn([x = 5, y = 7]) {}; // default values
a = function fn([x, , y]) {}; // ignoring some returned values (elision)
a = function fn([x, ...y]) {}; // rest syntax
([x]) => x;
([x = 5, y = 7]) => x + y; // default values
([x, , y]) => x + y; // ignoring some returned values (elision)
([x, ...y]) => y; // rest syntax

// Object destructuring
a = function fn({ x }) {};
a = function fn({ a: x, b: y }) {}; // assigning to new variable names
a = function fn({ x = 5, y = 7 }) {}; // default values
a = function fn({ a: x = 5, b: y = 7 }) {}; // assigning to new variable names and default values
a = function fn({ ['a']: x, ['b']: y }) {}; // computed property names
a = function fn({ x, y, ...rest }) {}; // rest properties (ES2018)
({ x }) => x;
({ a: x, b: y }) => x + y; // assigning to new variable names
({ x = 5, y = 7 }) => x + y; // default values
({ a: x = 5, b: y = 7 }) => x + y; // assigning to new variable names and default values
({ ['a']: x, ['b']: y }) => x + y; // computed property names
({ x, y, ...rest }) => x; // rest properties (ES2018)

// Destructuring and default parameters
a = function f([x, y] = [1, 2], {c: z} = {c: 3}) {};
([x, y] = [1, 2], {c: z} = {c: x + y}) => x + y + z;

// Generator function
a = function*fn() {};
a = function * fn() {};

// Rest parameters
a = function fn(...rest) {};
a = function fn(x, y, ...rest) {};
(...rest) => rest;
(x, y, ...rest) => rest;

// Async function (ES2017)
a = async function fn() {};
a = async /* comment */ function fn() {};
a = async /* comment
*/ function fn() {}; // correctly highlighted, because async cannot be followed by a line terminator
async x => x;
async () => {};
async /* comment */ () => {};
async /* comment
*/ () => {}; // correctly highlighted, because async cannot be followed by a line terminator
async(); // incorrectly highlighted

// Async generator (ES2018)
a = async function * fn() {};

// Trailing comma (ES2017)
a = function fn(x, y,) {};
(x, y,) => x + y;

// Trailing comma after rest parameters (syntax error)
a = function fn(x, y, ...rest,) {};
(x, y, ...rest,) => rest;


/*
 * Class expression
 */

a = class Foo {
    constructor() {
    }
    method(x, y) {
        return x + y;
    }
    *generator() {}
};
a = class extends Bar {
    constructor() { this._value = null; }

    get property() { return this._value; }
    set property(x) { this._value = x; }
    async method() { return 'async'; }
    async *generator() { return 'generator'; }
    static method() { return 'static'; }

    static get property() { return this.staticval; }
    static set property(x) { this.staticval = x; }
    static async method() { return 'async'; }
    static async *generator() { return 'generator'; }

    get() { return this.val; }
    set(v) { this.val = v; }
    async() { return 'async'; }
    static() { return 'static'; }

    static get() { return this.val; }
    static set(v) { this.val = v; }
    static async() { return 'async'; }
    static static() { return 'static'; }

    static
    static
    () { return 'static'; }
};
// Incorrectly highlighted as keyword
a = class {
    get
    () { return this.val; }
    set
    (v) { this.val = v; }
    static
    () { return 'static'; }
    static get
    () { return this.val; }
    static
    set
    (v) { this.val = v; }
};
// Properties/methods called "constructor"
a = class {
    *constructor() { this._value = null; }
    get constructor() { this._value = null; }
    set constructor() { this._value = null; }
    async constructor() { this._value = null; }
    async *constructor() { this._value = null; }
};
// Incorrectly highlighted as built-in method
a = class {
    static constructor() { this._value = null; }
};


/*
 * Operators
 * use groupings to test, as there can only be one expression (in the
 * first grouping item)
 */

// Grouping
( 1 + 2 );

// Increment / decrement
( ++a );
( --a );
( a++ );
( a-- );

// Keyword unary
( await promise() ); // ES2017
( delete obj.prop );
( new Array() );
( void 1 );
( typeof 'str' );
( yield 1 );
( yield* fn() );

// Arithmetic
( 1 + 2 );
( 1 - 2 );
( 1 * 2 );
( 1 / 2 );
( 1 % 2 );
( 1 ** 2 ); // ES2016
( +1 );
( -1 );

// Keyword relational
( prop in obj );
( obj instanceof constructor );

// Comparison
( 1 == 2 );
( 1 != 2 );
( 1 === 2 );
( 1 !== 2 );
( 1 < 2 );
( 1 > 2 );
( 1 <= 2 );
( 1 >= 2 );

// Bitwise
( 1 & 2 );
( 1 | 2 );
( 1 ^ 2 );
( ~1 );
( 1 << 2 );
( 1 >> 2 );
( 1 >>> 2 );

// Logical
( 1 && 2 );
( 1 || 2 );
( !1 );

// Nullish coalescing (ES2020)
( a ?? 1 );

// Assignment
( a = 1 );
( a += 1 );
( a -= 1 );
( a *= 1 );
( a /= 1 );
( a %= 1 );
( a **= 1 ); // ES2016
( a <<= 1 );
( a >>= 1 );
( a >>>= 1 );
( a &= 1 );
( a |= 1 );
( a ^= 1 );

// Array destructuring
( [a, b] = [1, 2] );
( [a = 5, b = 7] = [1] ); // default values
( [a, , b] = f() ); // ignoring some returned values (elision)
( [a, ...b] = [1, 2, 3] ); // rest syntax

// Object destructuring
( {a, b} = { a: 1, b: 2} );
( { a: foo, b: bar } = { a: 1, b: 2 } ); // assigning to new variable names
( { a = 5, b = 7 } = { a: 1 } ); // default values
( { a: foo = 5, b: bar = 7 } = { a: 1 } ); // assigning to new variable names and default values
( { ['a']: foo, ['b']: bar } = { a: 1, b: 2 } ); // computed property names
( { a, b, ...rest } = { a: 1, b: 2, c: 3, d: 4 } ); // rest properties (ES2018)

// Comma
1, 2 ;

// Conditional / ternary
( true ? 1 : 2 );
( true ? : 2 ); // missing true value (syntax error)
obj[ true ? 1, 2 : 3 ]; // comma operator inside true expression (syntax error)


/*
 * Property accessors
 */

// Dot notation
arr.length;
( obj
    . /* comment */ prototype /* comment */
    . /* comment */ constructor /* comment */
);
const pi = Math.PI
const num = 0

// Bracket notation
arr['length'];
( obj
    /* comment */ [ /* comment */ 'prototype' /* comment */ ] /* comment */
    /* comment */ [ /* comment */ 'constructor' /* comment */ ] /* comment */
);

// Mixed
obj
    ['prototype']
    . constructor;
obj
    . prototype
    ['constructor'];


/*
 * Function call
 */

fn();
obj.fn(1);
obj['fn'](1, 2);

// Spread syntax
fn(x, y, ...args);

// Trailing comma (ES2017)
fn(x, y,);


/* Tagged template */

myTag`That ${ person } is ${ age }`;


/*
 * Optional chaining (ES2020)
 */

obj?.prototype;
obj?.['constructor'];
func?.(1, 2, ...args);

foo?.3:0; // ternary operator, not optional chaining


/*
 * Statements and declarations
 */

/* Use strict directive */

"use strict";
function () {
    'use strict';
}

// invalid directives
" use strict";
'use strict ';
"use  strict";
'use	strict';
"hello 'use strict' world";
fn("use strict");
{ 'use strict'; }


/* Block statement */

{
    hello();
    world();
}
{ hello(); world() }


/* Break statement */

break;
break label;
break       // end statement
    label;  // separate statement
{ break }


/*
 * Class declaration
 */

class Foo {
    constructor() {
    }
    method(x, y) {
        return x + y;
    }
    *generator() {}
}
class Foo extends Bar {
    constructor() { this._value = null; }

    get property() { return this._value; }
    set property(x) { this._value = x; }
    async method() { return 'async'; }
    async *generator() { return 'generator'; }
    static method() { return 'static'; }

    static get property() { return this.staticval; }
    static set property(x) { this.staticval = x; }
    static async method() { return 'async'; }
    static async *generator() { return 'generator'; }

    get() { return this.val; }
    set(v) { this.val = v; }
    async() { return 'async'; }
    static() { return 'static'; }

    static get() { return this.val; }
    static set(v) { this.val = v; }
    static async() { return 'async'; }
    static static() { return 'static'; }

    static
    static
    () { return 'static'; }
}
// Incorrectly highlighted as keyword
class Foo {
    get
    () { return this.val; }
    set
    (v) { this.val = v; }
    static
    () { return 'static'; }
    static get
    () { return this.val; }
    static
    set
    (v) { this.val = v; }
}
// Properties/methods called "constructor"
class Foo {
    *constructor() { this._value = null; }
    get constructor() { this._value = null; }
    set constructor() { this._value = null; }
    async constructor() { this._value = null; }
    async *constructor() { this._value = null; }
}
// Incorrectly highlighted as built-in method
class Foo {
    static constructor() { this._value = null; }
}


/* Continue statement */

continue;
continue label;
continue    // end statement
    label;  // separate statement
{ continue }


/* Debugger statement */

debugger;
debugger
    ;


/* Export / import statement */

export { a };
export { a, b, };
export { x as a };
export { x as a, y as b, };
export var a;
export let a, b;
export const a = 1;
export var a = 1, b = 2;
export function fn() {}
export function* fn() {}
export class Class {}

export default 1;
export default function () {}
export default function *fn() {}
export default class {}
export { a as default, b };

export * from 'module';
export * as ns from 'module';
export { a, b } from 'module';
export { x as a, y as b, } from 'module';
export { default } from 'module';

import a from "module";
import * as ns from "module";
import { a } from "module";
import { a, b } from "module";
import { x as a } from "module";
import { x as a, y as b } from "module";
import { default as a } from "module";
import a, { b } from "module";
import a, * as ns from "module";
import "module";


/* For statement */

for (i = 0; i < 10; i++) something();
for (var i = 10; i >= 0; i--) {
    something();
}
for (i = 0, j = 0; i < 10; i++, j++) something();
for (let i = 10, j = 0; i >= 0; i--, j += 1) {
    something();
}
for (prop in obj) {} // matches "in" binary operator instead
for (const prop in obj) {}
for (val of generator()) {}
for (var val of array) {}
for await (let x of asyncIterable) {} // ES2018
for /* comment */ await /* comment */ (let x of asyncIterable) {} // ES2018


/* Function declaration statement */

function fn() {
    return;
}
async function fn() {} // ES2017
async /* comment */ function fn() {} // ES2017
async /* comment
*/ function fn() {} // correctly highlighted, because async cannot be followed by a line terminator


/* If..else statement */

if (a < 0) lessThan(); else if (a > 0) greaterThan(); else equal();
if (a < 0)
    lessThan();
else if (a > 0)
    greaterThan();
else
    equal();
if (a < 0) {
    lessThan();
} else if (a > 0) {
    greaterThan();
} else {
    equal();
}


/* Label statement */

outer: for (var i = 0; i < 10; i++) {
inner /* comment */ : for (var j = 0; j < 2; j++) {}
}
loop /* comment
*/ : for (var i in obj) {} // incorrectly highlighted (though it may appear correct)


/* Return statement */

return;
return 1;
return  // end statement
    1;  // separate statement
return (
    1
);
{ return a }


/* Switch statement */

switch (foo) {
case 1:
    doIt();
    break;
case '2':
    doSomethingElse();
    break;
default:
    oops();
}


/* Throw statement */

throw e;
throw new Error();
throw   // end statement (syntax error)
    e;  // separate statement
throw (
    new Error()
);
{ throw new Error() }


/* Try...catch statement */

try {
    somethingDangerous();
}
catch (e) {
    didntWork(e);
}
catch { // ES2019
    return false;
}
finally {
    cleanup();
}


/* Variable declaration */

// Declaration only
const a;
let a, b, c;
var a
    ,
    b
    ,
    c
    ;

// With assignment
const a = 1;
let a = 1, b = [2, 3], c = 4;
var a
    =
    1
    ,
    b
    =
    [
    2
    ,
    3
    ]
    ,
    c
    =
    4
    ;

// Array destructuring
var [a, b] = [1, 2];
var [
    a
    ,
    b
    ]
    =
    [
    1
    ,
    2
    ]
    ;
var [a = 5, b = 7] = [1]; // default values
var [a, , b] = f(); // ignoring some returned values (elision)
var [a, ...b] = [1, 2, 3]; // rest syntax

// Object destructuring
var { a, b } = { a: 1, b: 2 };
var {
    a
    ,
    b
    }
    =
    {
    a
    :
    1
    ,
    b
    :
    2
    }
    ;
var { a: foo, b: bar } = { a: 1, b: 2 }; // assigning to new variable names
var { a = 5, b = 7 } = { a: 1 }; // default values
var { a: foo = 5, b: bar = 7 } = { a: 1 }; // assigning to new variable names and default values
var { ['a']: foo, ['b']: bar } = { a: 1, b: 2 }; // computed property names
var { a, b, ...rest } = { a: 1, b: 2, c: 3, d: 4 }; // rest properties (ES2018)


/* While / do...while statement */

while (true) something();
while (1) {
    something();
}
do something(); while (false);
do {
    something();
} while (0);


/* With statement */

with (Math) {
    a = PI * r * r;
    x = r * cos(PI);
    y = r * sin(PI / 2);
}


/*
 * JSDoc
 */

/* Inline tag */

/** {@link String} */
/**
 * {@link http://example.com | Ex\{am\}ple}
 */
/** {@link*/


/* Type */

/** {String} */
/**
 * {Foo.B\{a\}r}
 */
/** {number*/


/* Block tag */

// No arguments
/** @constructor */
/**
 * @deprecated since 1.1.0
 */
/** @public*/

// Generic argument
/** @default 3.14159 */
/**
 * @tutorial tutorial-1
 */
/** @variation 2*/

// Event name argument
/** @fires earthquakeEvent */
/**
 * @listens touchstart
 */
/** @event newEvent*/

// Keyword argument
/** @access protected */
/**
 * @kind module
 */
/** @access private*/

// Name/namepath argument
/** @alias foo */
/**
 * @extends bar
 */
/** @typeParam T*/

// Type and name arguments
/** @param {String} name - A \{chosen\} \@name */
/**
 * @member {Object} child
 */
/** @property {number} num*/

// Borrows block tag
/** @borrows foo as bar */
/**
 * @borrows foo as
 */
/** @borrows foo*/

// Todo block tag
/** @todo write more/less test cases */
