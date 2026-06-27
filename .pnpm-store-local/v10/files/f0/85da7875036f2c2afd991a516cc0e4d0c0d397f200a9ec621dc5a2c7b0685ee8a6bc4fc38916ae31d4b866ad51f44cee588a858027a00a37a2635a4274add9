import react, {} from "@vitejs/plugin-react";
import {
  getReactMajorVersion,
  isSupportedReactVersion,
  versionsConfig
} from "./version.js";
const FAST_REFRESH_PREAMBLE = react.preambleCode;
function getRenderer(reactConfig) {
  return {
    name: "@astrojs/react",
    clientEntrypoint: reactConfig.client,
    serverEntrypoint: reactConfig.server
  };
}
function optionsPlugin({
  experimentalReactChildren = false,
  experimentalDisableStreaming = false
}) {
  const virtualModule = "astro:react:opts";
  const virtualModuleId = "\0" + virtualModule;
  return {
    name: "@astrojs/react:opts",
    resolveId(id) {
      if (id === virtualModule) {
        return virtualModuleId;
      }
    },
    load(id) {
      if (id === virtualModuleId) {
        return {
          code: `export default {
						experimentalReactChildren: ${JSON.stringify(experimentalReactChildren)},
						experimentalDisableStreaming: ${JSON.stringify(experimentalDisableStreaming)}
					}`
        };
      }
    }
  };
}
function getViteConfiguration({
  include,
  exclude,
  babel,
  experimentalReactChildren,
  experimentalDisableStreaming
} = {}, reactConfig) {
  return {
    optimizeDeps: {
      include: [reactConfig.client],
      exclude: [reactConfig.server]
    },
    plugins: [
      react({ include, exclude, babel }),
      optionsPlugin({
        experimentalReactChildren: !!experimentalReactChildren,
        experimentalDisableStreaming: !!experimentalDisableStreaming
      })
    ],
    ssr: {
      noExternal: [
        // These are all needed to get mui to work.
        "@mui/material",
        "@mui/base",
        "@babel/runtime",
        "use-immer",
        "@material-tailwind/react"
      ]
    }
  };
}
function index_default({
  include,
  exclude,
  babel,
  experimentalReactChildren,
  experimentalDisableStreaming
} = {}) {
  const majorVersion = getReactMajorVersion();
  if (!isSupportedReactVersion(majorVersion)) {
    throw new Error(`Unsupported React version: ${majorVersion}.`);
  }
  const versionConfig = versionsConfig[majorVersion];
  return {
    name: "@astrojs/react",
    hooks: {
      "astro:config:setup": ({ command, addRenderer, updateConfig, injectScript }) => {
        addRenderer(getRenderer(versionConfig));
        updateConfig({
          vite: getViteConfiguration(
            { include, exclude, babel, experimentalReactChildren, experimentalDisableStreaming },
            versionConfig
          )
        });
        if (command === "dev") {
          const preamble = FAST_REFRESH_PREAMBLE.replace(`__BASE__`, "/");
          injectScript("before-hydration", preamble);
        }
      },
      "astro:config:done": ({ logger, config }) => {
        const knownJsxRenderers = ["@astrojs/react", "@astrojs/preact", "@astrojs/solid-js"];
        const enabledKnownJsxRenderers = config.integrations.filter(
          (renderer) => knownJsxRenderers.includes(renderer.name)
        );
        if (enabledKnownJsxRenderers.length > 1 && !include && !exclude) {
          logger.warn(
            "More than one JSX renderer is enabled. This will lead to unexpected behavior unless you set the `include` or `exclude` option. See https://docs.astro.build/en/guides/integrations-guide/react/#combining-multiple-jsx-frameworks for more information."
          );
        }
      }
    }
  };
}
function getContainerRenderer() {
  const majorVersion = getReactMajorVersion();
  if (!isSupportedReactVersion(majorVersion)) {
    throw new Error(`Unsupported React version: ${majorVersion}.`);
  }
  return getRenderer(versionsConfig[majorVersion]);
}
export {
  index_default as default,
  getContainerRenderer
};
