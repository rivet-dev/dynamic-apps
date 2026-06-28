import type { SoftwarePackageRef } from "@agentos-software/manifest";

const packageDir = new URL("./package/", import.meta.url).pathname;

export default { packageDir } satisfies SoftwarePackageRef;
