export type Lexer = {
    /**
     * Content to lex.
     */
    readonly content: string;
    /**
     * 0-based offset of `token` in `content`.
     */
    offset: number;
    /**
     * Comments preceding `token`.
     */
    comment: string;
    /**
     * Current token.
     * An empty string means that there is no more tokens to process.
     */
    token: string;
};
/**
 * Create a new lexer of `content` and advance to the first token if any.
 */
export declare function create(content: string): Lexer;
/**
 * Next token.
 * Reset `lex.comment`, update `lex.token` and `lex.offset`.
 */
export declare function nextToken(lex: Lexer): void;
