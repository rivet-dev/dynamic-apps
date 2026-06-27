/**
 * Astro passes `children` as a string of HTML, so we need
 * a wrapper `div` to render that content as VNodes.
 *
 * As a bonus, we can signal to React that this subtree is
 * entirely static and will never change via `shouldComponentUpdate`.
 */
declare const StaticHtml: {
    ({ value, name, hydrate, }: {
        value: string | null;
        name?: string;
        hydrate?: boolean;
    }): import("react").DOMElement<{
        name: string | undefined;
        suppressHydrationWarning: boolean;
        dangerouslySetInnerHTML: {
            __html: string;
        };
    }, Element> | null;
    shouldComponentUpdate(): boolean;
};
export default StaticHtml;
