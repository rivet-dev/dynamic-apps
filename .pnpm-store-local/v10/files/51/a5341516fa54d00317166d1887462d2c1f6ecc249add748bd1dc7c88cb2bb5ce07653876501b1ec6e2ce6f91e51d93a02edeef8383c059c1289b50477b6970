/**
 * @sealed
 */
export declare class ConfigError extends Error {
    name: string;
}
export type Config = {
    readonly generator: "bare" | "dts" | "js" | "ts";
    readonly legacy: boolean;
    readonly lib: boolean;
    /**
     * Output filename.
     * An empty string means inline output.
     *
     * If `generator` is unspecified, then the output filename extension is used
     * to determinate the generator.
     */
    readonly out: string | number | null;
    readonly pedantic: boolean;
    /**
     * Input filename.
     */
    readonly schema: string | number | null;
    readonly useClass: boolean;
    readonly useGenericArray: boolean;
    readonly useIntEnum: boolean;
    readonly useIntTag: boolean;
    readonly useMutable: boolean;
    readonly usePrimitiveFlatUnion: boolean;
    readonly useSafeInt: boolean;
    readonly useStructFlatUnion: boolean;
    readonly useUndefined: boolean;
};
/**
 * Complete the configuration by setting missing fields to their default values.
 *
 * @throws {@link ConfigError} if the code generator cannot be determinate or
 * the format of the schema is not a supported.
 */
export declare function Config({ generator, legacy, lib, out, pedantic, schema, useClass, useGenericArray, useIntEnum, useIntTag, useMutable, usePrimitiveFlatUnion, useSafeInt, useStructFlatUnion, useUndefined, }: Partial<Config>): Config;
