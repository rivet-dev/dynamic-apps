import { BUFFER_CONSTANTS, BUFFER_MAX_LENGTH, BUFFER_MAX_STRING_LENGTH } from "./buffer-constants.js";
import { eventsModule } from "./events.js";
import { builtinAsyncHooksModule, builtinConsoleModule, builtinCryptoModule, builtinDiagnosticsChannelModule, builtinPerfHooksModule, builtinStreamConsumersModule, builtinStreamPromisesModule, builtinTimersPromisesModule, builtinTtyModule, builtinV8Module, builtinVmModule, builtinWorkerThreadsModule, createAccessDeniedBuiltinError, fileURLToPath2, installBuiltinUtilFormatWithOptions, pathToFileURL2, process_default } from "./process.js";
import { URL2, URLSearchParams } from "./whatwg-url.js";
import { exposeCustomGlobal, exposeMutableRuntimeStateGlobal } from "../global-exposure.js";
import { TextDecoder, defineGlobal } from "../polyfills.js";
import { bufferStdlibModuleNs, constantsStdlibModuleNs, eventsStdlibModuleNs, pathStdlibModuleNs, punycodeStdlibModuleNs, querystringStdlibModuleNs, streamStdlibModuleNs, stringDecoderStdlibModuleNs, urlStdlibModuleNs } from "../prelude.js";

// .agent/recovery/secure-exec/nodejs/src/bridge/module.ts
var initialModuleCache = {};
var initialPendingModules = {};
var initialCurrentModule = {
  id: "/<entry>.js",
  filename: "/<entry>.js",
  dirname: "/",
  exports: {},
  loaded: false
};
exposeMutableRuntimeStateGlobal("_moduleCache", initialModuleCache);
exposeMutableRuntimeStateGlobal("_pendingModules", initialPendingModules);
exposeMutableRuntimeStateGlobal("_currentModule", initialCurrentModule);
function _pathDirname(p) {
  const lastSlash = p.lastIndexOf("/");
  if (lastSlash === -1) return ".";
  if (lastSlash === 0) return "/";
  return p.slice(0, lastSlash);
}
function _parseFileUrl(url) {
  if (url.startsWith("file://")) {
    let path = url.slice(7);
    if (path.startsWith("/")) {
      return path;
    }
    return "/" + path;
  }
  return url;
}
function computeRequireResolvePaths(dirname, request) {
  if (Module.isBuiltin(request)) {
    return null;
  }
  if (request.startsWith("./") || request.startsWith("../") || request.startsWith("/")) {
    return [dirname];
  }
  return Module._nodeModulePaths(dirname);
}
function createRequireResolve(dirname) {
  const resolve = function(request, _options) {
    const resolved = _resolveModule.applySyncPromise(void 0, [
      request,
      dirname,
      "require"
    ]);
    if (resolved === null) {
      const err = new Error("Cannot find module '" + request + "'");
      err.code = "MODULE_NOT_FOUND";
      throw err;
    }
    return resolved;
  };
  resolve.paths = function(request) {
    return computeRequireResolvePaths(dirname, request);
  };
  return resolve;
}
const defaultRequireExtensions = {
  ".js": function(_module, _filename) {
  },
  ".json": function(_module, _filename) {
  },
  ".node": function(_module, _filename) {
    throw new Error(".node extensions are not supported in sandbox");
  }
};
function attachRequireMetadata(requireFn) {
  requireFn.cache = _moduleCache;
  requireFn.main = globalThis.process?.mainModule;
  requireFn.extensions = defaultRequireExtensions;
  return requireFn;
}
function createRequireEsmError(filename) {
  const error = new Error(`require() of ES Module ${filename} is not supported.`);
  error.code = "ERR_REQUIRE_ESM";
  return error;
}
function createModuleFormatBridgeMissingError(filename) {
  const error = new Error(
    `secure-exec module format bridge is not registered; cannot require ${filename}.`
  );
  error.code = "ERR_AGENTOS_MODULE_FORMAT_BRIDGE_MISSING";
  return error;
}
function assertCommonjsLoadable(filename) {
  if (
    typeof _moduleFormat === "undefined" ||
    typeof _moduleFormat.applySyncPromise !== "function"
  ) {
    throw createModuleFormatBridgeMissingError(filename);
  }
  const format = _moduleFormat.applySyncPromise(void 0, [filename]);
  if (format === "module") {
    throw createRequireEsmError(filename);
  }
}
function createRequire(filename) {
  if (typeof filename !== "string" && !(filename instanceof URL)) {
    throw new TypeError("filename must be a string or URL");
  }
  const filepath = _parseFileUrl(String(filename));
  const dirname = _pathDirname(filepath);
  const resolve = createRequireResolve(dirname);
  const rejectRestrictedBuiltin = function(_request) {
  };
  const requireFn = function(request) {
    rejectRestrictedBuiltin(request);
    return _requireFrom(request, dirname);
  };
  requireFn.resolve = resolve;
  return attachRequireMetadata(requireFn);
}
// Expose createRequire under a namespaced global so an agent-SDK snapshot bundle
// (which runs as a raw IIFE Script, not a wrapped CJS module, when evaluated into
// the V8 startup snapshot) can bind a `require` for its node-builtin imports. This
// grants no new capability — guest CJS modules already receive `require` via the
// module wrapper, and resolution still flows through _requireFrom with the same
// permission checks — and the namespaced name avoids changing `typeof require` for
// guest code that branches on CJS-vs-ESM.
defineGlobal("__secureExecGuestCreateRequire", createRequire);
var Module = class _Module {
  id;
  path;
  exports;
  filename;
  loaded;
  children;
  paths;
  parent;
  isPreloading;
  constructor(id, parent) {
    this.id = id;
    this.path = _pathDirname(id);
    this.exports = {};
    this.filename = id;
    this.loaded = false;
    this.children = [];
    this.paths = [];
    this.parent = parent;
    this.isPreloading = false;
    let current = this.path;
    while (current !== "/") {
      this.paths.push(current + "/node_modules");
      current = _pathDirname(current);
    }
    this.paths.push("/node_modules");
  }
  require(request) {
    return _requireFrom(request, this.path);
  }
  _compile(content, filename) {
    const contentWithSourceUrl = String(content) + "\n//# sourceURL=" + String(filename);
    const wrapper = new Function(
      "exports",
      "require",
      "module",
      "__filename",
      "__dirname",
      contentWithSourceUrl
    );
    const moduleRequire = (request) => {
      rejectRestrictedBuiltinRequest(request);
      return _requireFrom(request, this.path);
    };
    moduleRequire.resolve = createRequireResolve(this.path);
    attachRequireMetadata(moduleRequire);
    const previousModule = globalThis._currentModule;
    globalThis._currentModule = this;
    try {
      wrapper(this.exports, moduleRequire, this, filename, this.path);
      this.loaded = true;
      return this.exports;
    } finally {
      globalThis._currentModule = previousModule;
    }
  }
  static _extensions = {
    ...defaultRequireExtensions,
    ".js": function(module, filename) {
      assertCommonjsLoadable(filename);
      const content = typeof _loadFile !== "undefined" ? _loadFile.applySyncPromise(void 0, [
        filename
      ]) : _requireFrom("fs", "/").readFileSync(filename, "utf8");
      module._compile(content, filename);
    },
    ".json": function(module, filename) {
      const content = typeof _loadFile !== "undefined" ? _loadFile.applySyncPromise(void 0, [
        filename
      ]) : _requireFrom("fs", "/").readFileSync(filename, "utf8");
      module.exports = JSON.parse(content);
    }
  };
  static _cache = typeof _moduleCache !== "undefined" ? _moduleCache : {};
  static _resolveFilename(request, parent, _isMain, _options) {
    const parentDir = parent && parent.path ? parent.path : "/";
    const resolved = _resolveModule.applySyncPromise(void 0, [
      request,
      parentDir,
      "require"
    ]);
    if (resolved === null) {
      const err = new Error("Cannot find module '" + request + "'");
      err.code = "MODULE_NOT_FOUND";
      throw err;
    }
    return resolved;
  }
  static wrap(content) {
    return "(function (exports, require, module, __filename, __dirname) { " + content + "\n});";
  }
  static builtinModules = [
    "assert",
    "async_hooks",
    "buffer",
    "child_process",
    "console",
    "cluster",
    "constants",
    "crypto",
    "dgram",
    "diagnostics_channel",
    "domain",
    "dns",
    "dns/promises",
    "events",
    "fs",
    "fs/promises",
    "http",
    "http2",
    "https",
    "inspector",
    "module",
    "net",
    "os",
    "path",
    "path/posix",
    "path/win32",
    "perf_hooks",
    "process",
    "punycode",
    "querystring",
    "readline",
    "repl",
    "sqlite",
    "stream",
    "stream/consumers",
    "stream/promises",
    "stream/web",
    "string_decoder",
    "sys",
    "timers",
    "timers/promises",
    "trace_events",
    "tls",
    "tty",
    "url",
    "util",
    "util/types",
    "v8",
    "wasi",
    "worker_threads",
    "zlib",
    "vm"
  ];
  static isBuiltin(moduleName) {
    const name = moduleName.replace(/^node:/, "");
    return _Module.builtinModules.includes(name);
  }
  static createRequire = createRequire;
  static syncBuiltinESMExports() {
  }
  static findSourceMap(_path) {
    return void 0;
  }
  static _nodeModulePaths(from) {
    const paths = [];
    let current = from;
    while (current !== "/") {
      paths.push(current + "/node_modules");
      current = _pathDirname(current);
      if (current === ".") break;
    }
    paths.push("/node_modules");
    return paths;
  }
  static _load(request, parent, _isMain) {
    const parentDir = parent && parent.path ? parent.path : "/";
    return _requireFrom(request, parentDir);
  }
  static runMain() {
  }
};
var SourceMap = class {
  constructor(_payload) {
    throw new Error("SourceMap is not implemented in sandbox");
  }
  get payload() {
    throw new Error("SourceMap is not implemented in sandbox");
  }
  set payload(_value) {
    throw new Error("SourceMap is not implemented in sandbox");
  }
  findEntry(_line, _column) {
    throw new Error("SourceMap is not implemented in sandbox");
  }
};
var moduleModule = Object.assign(Module, {
  Module,
  createRequire,
  // Module._extensions (deprecated alias)
  _extensions: Module._extensions,
  // Module._cache reference
  _cache: Module._cache,
  // Built-in module list
  builtinModules: Module.builtinModules,
  // isBuiltin check
  isBuiltin: Module.isBuiltin,
  // Module._resolveFilename (internal but sometimes used)
  _resolveFilename: Module._resolveFilename,
  // wrap function
  wrap: Module.wrap,
  // syncBuiltinESMExports (stub for ESM interop)
  syncBuiltinESMExports: Module.syncBuiltinESMExports,
  // findSourceMap (stub)
  findSourceMap: Module.findSourceMap,
  // SourceMap class (stub)
  SourceMap
});
exposeCustomGlobal("_moduleModule", moduleModule);
var builtinTimersModule = {
  clearImmediate: globalThis.clearImmediate ?? function() {
  },
  clearInterval: globalThis.clearInterval ?? function() {
  },
  clearTimeout: globalThis.clearTimeout ?? function() {
  },
  setImmediate: globalThis.setImmediate ?? function(callback, ...args) {
    return globalThis.setTimeout?.(() => callback(...args), 0);
  },
  setInterval: globalThis.setInterval ?? function(callback, delay, ...args) {
    return globalThis.setTimeout?.(() => callback(...args), delay ?? 0);
  },
  setTimeout: globalThis.setTimeout ?? function() {
    return void 0;
  }
};
function unwrapStdlibModule(moduleNamespace) {
  if (moduleNamespace && typeof moduleNamespace === "object" && moduleNamespace.default != null) {
    return moduleNamespace.default;
  }
  return moduleNamespace;
}
function cloneStdlibModule(moduleNamespace) {
  const resolved = unwrapStdlibModule(moduleNamespace);
  if (resolved == null) {
    return resolved;
  }
  if (typeof resolved === "function") {
    return resolved;
  }
  if (typeof resolved === "object") {
    return { ...resolved };
  }
  return resolved;
}
function defineMissingModuleProperty(target, key, value) {
  if (target != null && typeof target[key] === "undefined") {
    target[key] = value;
  }
}
function trimNonRootTrailingSlash(pathValue) {
  return typeof pathValue === "string" && pathValue.length > 1 && pathValue.endsWith("/") ? pathValue.slice(0, -1) : pathValue;
}
var builtinBufferStdlibModule = cloneStdlibModule(bufferStdlibModuleNs);
defineMissingModuleProperty(builtinBufferStdlibModule, "constants", BUFFER_CONSTANTS);
defineMissingModuleProperty(builtinBufferStdlibModule, "kMaxLength", BUFFER_MAX_LENGTH);
defineMissingModuleProperty(
  builtinBufferStdlibModule,
  "kStringMaxLength",
  BUFFER_MAX_STRING_LENGTH
);
defineMissingModuleProperty(builtinBufferStdlibModule, "Blob", globalThis.Blob);
defineMissingModuleProperty(builtinBufferStdlibModule, "File", globalThis.File);
var builtinConstantsStdlibModule = cloneStdlibModule(constantsStdlibModuleNs);
var builtinEventsStdlibModule = cloneStdlibModule(eventsStdlibModuleNs);
var builtinEventsConstructor = null;
var builtinEventsStdlibModuleInitialized = false;
function ensureBuiltinEventsStdlibModule() {
  if (builtinEventsStdlibModuleInitialized) {
    return builtinEventsStdlibModule;
  }
  builtinEventsStdlibModuleInitialized = true;
  builtinEventsConstructor =
    typeof builtinEventsStdlibModule === "function"
      ? builtinEventsStdlibModule
      : builtinEventsStdlibModule?.EventEmitter;
  if (typeof builtinEventsConstructor === "function") {
    Object.assign(
      builtinEventsConstructor.prototype,
      eventsModule.EventEmitter.prototype
    );
    Object.assign(eventsModule.EventEmitter, builtinEventsStdlibModule, eventsModule);
    builtinEventsStdlibModule = eventsModule.EventEmitter;
    builtinEventsStdlibModule.EventEmitter = builtinEventsStdlibModule;
  } else {
    builtinEventsStdlibModule = {
      ...builtinEventsStdlibModule,
      ...eventsModule
    };
  }
  return builtinEventsStdlibModule;
}
var builtinPathStdlibModule = cloneStdlibModule(pathStdlibModuleNs);
if (!builtinPathStdlibModule?.posix) {
  builtinPathStdlibModule.posix = cloneStdlibModule(
    pathStdlibModuleNs?.posix ?? pathStdlibModuleNs?.default?.posix
  ) ?? builtinPathStdlibModule;
}
if (!builtinPathStdlibModule?.win32) {
  builtinPathStdlibModule.win32 = cloneStdlibModule(
    pathStdlibModuleNs?.win32 ?? pathStdlibModuleNs?.default?.win32
  ) ?? builtinPathStdlibModule;
}
if (builtinPathStdlibModule?.normalize) {
  const builtinPathNormalize = builtinPathStdlibModule.normalize.bind(builtinPathStdlibModule);
  builtinPathStdlibModule.normalize = function(pathValue) {
    return trimNonRootTrailingSlash(builtinPathNormalize(pathValue));
  };
}
var builtinPunycodeStdlibModule = cloneStdlibModule(punycodeStdlibModuleNs);
var builtinQuerystringStdlibModule = cloneStdlibModule(querystringStdlibModuleNs);
var builtinStreamStdlibModule = cloneStdlibModule(streamStdlibModuleNs);
if (typeof builtinStreamStdlibModule?.Stream === "function") {
  Object.assign(builtinStreamStdlibModule.Stream, builtinStreamStdlibModule);
  builtinStreamStdlibModule = builtinStreamStdlibModule.Stream;
  builtinStreamStdlibModule.Stream = builtinStreamStdlibModule;
  const isBuiltinStreamInstance = (value) => {
    if (!value || (typeof value !== "object" && typeof value !== "function")) {
      return false;
    }
    return (
      (typeof builtinStreamStdlibModule.Readable === "function" &&
        value instanceof builtinStreamStdlibModule.Readable) ||
      (typeof builtinStreamStdlibModule.Writable === "function" &&
        value instanceof builtinStreamStdlibModule.Writable) ||
      (typeof builtinStreamStdlibModule.Duplex === "function" &&
        value instanceof builtinStreamStdlibModule.Duplex) ||
      (typeof builtinStreamStdlibModule.Transform === "function" &&
        value instanceof builtinStreamStdlibModule.Transform) ||
      (typeof builtinStreamStdlibModule.PassThrough === "function" &&
        value instanceof builtinStreamStdlibModule.PassThrough)
    );
  };
  Object.defineProperty(builtinStreamStdlibModule, Symbol.hasInstance, {
    configurable: true,
    value: isBuiltinStreamInstance
  });
}
function defineReadableAsyncIterator(target) {
  if (!target || typeof target[Symbol.asyncIterator] === "function") {
    return;
  }
  Object.defineProperty(target, Symbol.asyncIterator, {
    configurable: true,
    value: function() {
      const stream = this;
      const queuedChunks = [];
      const pendingResolves = [];
      let done = false;
      let error = null;
      const flush = () => {
        while (pendingResolves.length > 0) {
          if (error) {
            pendingResolves.shift()(Promise.reject(error));
            continue;
          }
          if (queuedChunks.length > 0) {
            pendingResolves.shift()(Promise.resolve({ done: false, value: queuedChunks.shift() }));
            continue;
          }
          if (done) {
            pendingResolves.shift()(Promise.resolve({ done: true, value: void 0 }));
            continue;
          }
          break;
        }
      };
      const onData = (chunk) => {
        queuedChunks.push(chunk);
        flush();
      };
      const onEnd = () => {
        done = true;
        flush();
      };
      const onError = (reason) => {
        error = reason;
        done = true;
        flush();
      };
      stream.on?.("data", onData);
      stream.on?.("end", onEnd);
      stream.on?.("close", onEnd);
      stream.on?.("error", onError);
      stream.resume?.();
      return {
        next() {
          if (error) {
            return Promise.reject(error);
          }
          if (queuedChunks.length > 0) {
            return Promise.resolve({ done: false, value: queuedChunks.shift() });
          }
          if (done) {
            return Promise.resolve({ done: true, value: void 0 });
          }
          return new Promise((resolve) => {
            pendingResolves.push(resolve);
          });
        },
        return() {
          done = true;
          stream.off?.("data", onData);
          stream.off?.("end", onEnd);
          stream.off?.("close", onEnd);
          stream.off?.("error", onError);
          flush();
          return Promise.resolve({ done: true, value: void 0 });
        },
        [Symbol.asyncIterator]() {
          return this;
        }
      };
    }
  });
}
defineReadableAsyncIterator(builtinStreamStdlibModule?.Readable?.prototype);
defineReadableAsyncIterator(builtinStreamStdlibModule?.PassThrough?.prototype);
defineReadableAsyncIterator(builtinStreamStdlibModule?.Transform?.prototype);
defineReadableAsyncIterator(builtinStreamStdlibModule?.Duplex?.prototype);
defineMissingModuleProperty(builtinStreamStdlibModule, "isReadable", (stream) => {
  return Boolean(stream) && stream.readable !== false && stream.destroyed !== true;
});
defineMissingModuleProperty(builtinStreamStdlibModule, "isErrored", (stream) => {
  return stream?.errored != null;
});
defineMissingModuleProperty(builtinStreamStdlibModule, "isDisturbed", (stream) => {
  return Boolean(stream?.locked || stream?.disturbed === true || stream?.readableDidRead === true);
});
var builtinStringDecoderStdlibModule = cloneStdlibModule(stringDecoderStdlibModuleNs);
var builtinUrlStdlibModule = cloneStdlibModule(urlStdlibModuleNs);
var builtinUrlStdlibModuleInitialized = false;
function ensureBuiltinUrlStdlibModule() {
  if (builtinUrlStdlibModuleInitialized) {
    return builtinUrlStdlibModule;
  }
  builtinUrlStdlibModuleInitialized = true;
  builtinUrlStdlibModule.URL = URL2;
  builtinUrlStdlibModule.URLSearchParams = URLSearchParams;
  builtinUrlStdlibModule.fileURLToPath = fileURLToPath2;
  builtinUrlStdlibModule.pathToFileURL = pathToFileURL2;
  if (builtinUrlStdlibModule?.default && typeof builtinUrlStdlibModule.default === "object") {
    builtinUrlStdlibModule.default.URL = URL2;
    builtinUrlStdlibModule.default.URLSearchParams = URLSearchParams;
    builtinUrlStdlibModule.default.fileURLToPath = fileURLToPath2;
    builtinUrlStdlibModule.default.pathToFileURL = pathToFileURL2;
  }
  return builtinUrlStdlibModule;
}
function normalizeBuiltinRequest(request) {
  return String(request).replace(/^node:/, "");
}
let __jsRuntimeBuiltinAllowlist = null;
function rejectRestrictedBuiltinRequest(request) {
  const normalized = normalizeBuiltinRequest(request);
  // jsRuntime builtin allow-list gate. When the per-execution shim installed an
  // allow-list (non-node platforms => empty => deny all; node + explicit list),
  // deny any builtin whose root name is not permitted. Absent => unrestricted.
  const allow = __jsRuntimeBuiltinAllowlist;
  if (Array.isArray(allow)) {
    const root = String(normalized == null ? request : normalized)
      .replace(/^node:/, "")
      .split("/")[0];
    if (!allow.includes(root)) {
      throw createAccessDeniedBuiltinError(request);
    }
  }
  return normalized;
}
exposeCustomGlobal("__agentOSInitJsRuntime", function (allowlist) {
  __jsRuntimeBuiltinAllowlist = Array.isArray(allowlist)
    ? allowlist.map((name) => String(name).replace(/^node:/, "").split("/")[0])
    : null;
});
function loadBuiltinModule(request) {
  const normalized = rejectRestrictedBuiltinRequest(request);
  switch (normalized) {
    case "assert":
      return globalThis.__secureExecBuiltinAssertModule;
    case "async_hooks":
      return builtinAsyncHooksModule;
    case "buffer":
      defineMissingModuleProperty(builtinBufferStdlibModule, "Blob", globalThis.Blob);
      defineMissingModuleProperty(builtinBufferStdlibModule, "File", globalThis.File);
      return builtinBufferStdlibModule;
    case "cluster":
      throw createAccessDeniedBuiltinError(request);
    case "crypto":
      return builtinCryptoModule;
    case "diagnostics_channel":
      return builtinDiagnosticsChannelModule;
    case "domain":
      throw createAccessDeniedBuiltinError(request);
    case "http":
      return _httpModule;
    case "http2":
      return _http2Module;
    case "events":
      return ensureBuiltinEventsStdlibModule();
    case "fs":
      return _fsModule;
    case "fs/promises":
      return _fsModule.promises;
    case "os":
      return _osModule;
    case "path":
      return builtinPathStdlibModule;
    case "path/posix":
      return builtinPathStdlibModule.posix;
    case "path/win32":
      return builtinPathStdlibModule.win32;
    case "perf_hooks":
      return builtinPerfHooksModule;
    case "process":
      return process_default;
    case "punycode":
      return builtinPunycodeStdlibModule;
    case "querystring":
      return builtinQuerystringStdlibModule;
    case "readline":
      return {
        createInterface(options = {}) {
          const input = options.input ?? null;
          const output = options.output ?? null;
          const listeners = new Map();
          let closed = false;
          let ended = false;
          let lineBuffer = "";
          const queuedLines = [];
          let pendingLineResolve = null;
          const pendingQuestionResolves = [];
          const textDecoder = new TextDecoder();
          const emit = (event, ...args) => {
            const current = listeners.get(event) ?? [];
            for (const listener of [...current]) {
              listener(...args);
            }
          };
          const enqueueLine = (line) => {
            if (pendingQuestionResolves.length > 0) {
              const resolve = pendingQuestionResolves.shift();
              resolve(line);
              return;
            }
            if (pendingLineResolve) {
              const resolve = pendingLineResolve;
              pendingLineResolve = null;
              resolve({ done: false, value: line });
              return;
            }
            queuedLines.push(line);
          };
          const emitLine = (line) => {
            emit("line", line);
            enqueueLine(line);
          };
          const flushBufferedLines = () => {
            let newlineIndex = lineBuffer.indexOf("\n");
            while (newlineIndex !== -1) {
              let line = lineBuffer.slice(0, newlineIndex);
              if (line.endsWith("\r")) {
                line = line.slice(0, -1);
              }
              lineBuffer = lineBuffer.slice(newlineIndex + 1);
              emitLine(line);
              newlineIndex = lineBuffer.indexOf("\n");
            }
          };
          const detachInput = () => {
            if (!input || typeof input.off !== "function") {
              return;
            }
            input.off("data", onData);
            input.off("end", onEnd);
          };
          const onData = (chunk) => {
            if (closed) {
              return;
            }
            if (typeof chunk === "string") {
              lineBuffer += chunk;
            } else if (chunk instanceof Uint8Array) {
              lineBuffer += textDecoder.decode(chunk, { stream: true });
            } else if (chunk != null) {
              lineBuffer += String(chunk);
            }
            flushBufferedLines();
          };
          const onEnd = () => {
            if (ended) {
              return;
            }
            ended = true;
            const trailing = textDecoder.decode();
            if (trailing) {
              lineBuffer += trailing;
            }
            flushBufferedLines();
            if (lineBuffer.length > 0) {
              emitLine(lineBuffer);
              lineBuffer = "";
            }
            api.close();
          };
          if (input && typeof input.on === "function") {
            input.on("data", onData);
            input.on("end", onEnd);
            if (typeof input.resume === "function") {
              input.resume();
            }
          }
          const iterator = {
            next() {
              if (queuedLines.length > 0) {
                return Promise.resolve({
                  done: false,
                  value: queuedLines.shift(),
                });
              }
              if (closed || ended) {
                return Promise.resolve({ done: true, value: void 0 });
              }
              return new Promise((resolve) => {
                pendingLineResolve = resolve;
              });
            },
            return() {
              api.close();
              return Promise.resolve({ done: true, value: void 0 });
            },
            [Symbol.asyncIterator]() {
              return this;
            },
          };
          const api = {
            addListener(event, listener) {
              return this.on(event, listener);
            },
            on(event, listener) {
              const current = listeners.get(event) ?? [];
              current.push(listener);
              listeners.set(event, current);
              return this;
            },
            once(event, listener) {
              const wrapped = (...args) => {
                this.off(event, wrapped);
                listener(...args);
              };
              return this.on(event, wrapped);
            },
            off(event, listener) {
              const current = listeners.get(event) ?? [];
              listeners.set(
                event,
                current.filter((candidate) => candidate !== listener)
              );
              return this;
            },
            removeListener(event, listener) {
              return this.off(event, listener);
            },
            close() {
              if (closed) {
                return;
              }
              closed = true;
              detachInput();
              while (pendingQuestionResolves.length > 0) {
                const resolve = pendingQuestionResolves.shift();
                resolve("");
              }
              if (pendingLineResolve) {
                const resolve = pendingLineResolve;
                pendingLineResolve = null;
                resolve({ done: true, value: void 0 });
              }
              emit("close");
            },
            question(prompt, callback) {
              if (output && typeof output.write === "function" && prompt) {
                output.write(String(prompt));
              }
              const readAnswer = () => {
                if (queuedLines.length > 0) {
                  return Promise.resolve(queuedLines.shift());
                }
                if (closed || ended) {
                  return Promise.resolve("");
                }
                return new Promise((resolve) => {
                  pendingQuestionResolves.push(resolve);
                });
              };
              if (typeof callback === "function") {
                void readAnswer().then((answer) => {
                  callback(answer);
                });
                return;
              }
              return readAnswer();
            },
            [Symbol.asyncIterator]() {
              return iterator;
            },
          };
          return api;
        }
      };
    case "repl":
      throw createAccessDeniedBuiltinError(request);
    case "stream":
      return builtinStreamStdlibModule;
    case "stream/consumers":
      return builtinStreamConsumersModule;
    case "stream/promises":
      return builtinStreamPromisesModule;
    case "string_decoder":
      return builtinStringDecoderStdlibModule;
    case "stream/web":
      return {
        ReadableStream: globalThis.ReadableStream,
        WritableStream: globalThis.WritableStream,
        TransformStream: globalThis.TransformStream,
        TextEncoderStream: globalThis.TextEncoderStream,
        TextDecoderStream: globalThis.TextDecoderStream,
        CompressionStream: globalThis.CompressionStream,
        DecompressionStream: globalThis.DecompressionStream
      };
    case "timers":
      return builtinTimersModule;
    case "timers/promises":
      return builtinTimersPromisesModule;
    case "trace_events":
      throw createAccessDeniedBuiltinError(request);
    case "url":
      return ensureBuiltinUrlStdlibModule();
    case "sys":
      return installBuiltinUtilFormatWithOptions(globalThis.__secureExecBuiltinUtilModule);
    case "util":
      return installBuiltinUtilFormatWithOptions(globalThis.__secureExecBuiltinUtilModule);
    case "util/types":
      return installBuiltinUtilFormatWithOptions(globalThis.__secureExecBuiltinUtilModule).types;
    case "child_process":
      return _childProcessModule;
    case "console":
      return builtinConsoleModule;
    case "constants":
      return builtinConstantsStdlibModule;
    case "dns":
      return _dnsModule;
    case "dns/promises":
      return _dnsModule.promises;
    case "net":
      return _netModule;
    case "tls":
      return _tlsModule;
    case "tty":
      return builtinTtyModule;
    case "dgram":
      return _dgramModule;
    case "sqlite":
      return _sqliteModule;
    case "https":
      return _httpsModule;
    case "inspector":
      throw createAccessDeniedBuiltinError(request);
    case "module":
      return _moduleModule;
    case "wasi":
      throw createAccessDeniedBuiltinError(request);
    case "zlib":
      return globalThis.__secureExecBuiltinZlibModule;
    case "v8":
      return builtinV8Module;
    case "vm":
      return builtinVmModule;
    case "worker_threads":
      return builtinWorkerThreadsModule;
    default: {
      const error = new Error(`Cannot find module '${request}'`);
      error.code = "MODULE_NOT_FOUND";
      throw error;
    }
  }
}
function requireFrom(request, parentDir) {
  const parentPath = typeof parentDir === "string" ? parentDir : "/";
  if (Module.isBuiltin(request)) {
    try {
      return loadBuiltinModule(request);
    } catch (error) {
      if (error?.code !== "MODULE_NOT_FOUND") {
        throw error;
      }
    }
  }
  const resolved = _resolveModule.applySyncPromise(void 0, [
    request,
    parentPath,
    "require"
  ]);
  if (resolved === null) {
    const error = new Error(`Cannot find module '${request}'`);
    error.code = "MODULE_NOT_FOUND";
    throw error;
  }
  if (Object.prototype.hasOwnProperty.call(_moduleCache, resolved)) {
    return _moduleCache[resolved].exports;
  }
  assertCommonjsLoadable(resolved);
  const module = new Module(resolved, { path: parentPath });
  _moduleCache[resolved] = module;
  try {
    const extension = resolved.endsWith(".json") ? ".json" : resolved.endsWith(".node") ? ".node" : ".js";
    const loader = Module._extensions[extension] ?? Module._extensions[".js"];
    loader(module, resolved);
    module.loaded = true;
    return module.exports;
  } catch (error) {
    delete _moduleCache[resolved];
    throw error;
  }
}
exposeCustomGlobal("_requireFrom", requireFrom);
var module_default = moduleModule;
export { initialModuleCache, initialPendingModules, initialCurrentModule, _pathDirname, _parseFileUrl, computeRequireResolvePaths, createRequireResolve, defaultRequireExtensions, attachRequireMetadata, createRequireEsmError, createModuleFormatBridgeMissingError, assertCommonjsLoadable, createRequire, Module, SourceMap, moduleModule, builtinTimersModule, unwrapStdlibModule, cloneStdlibModule, defineMissingModuleProperty, trimNonRootTrailingSlash, builtinBufferStdlibModule, builtinConstantsStdlibModule, builtinEventsStdlibModule, builtinEventsConstructor, builtinEventsStdlibModuleInitialized, ensureBuiltinEventsStdlibModule, builtinPathStdlibModule, builtinPunycodeStdlibModule, builtinQuerystringStdlibModule, builtinStreamStdlibModule, defineReadableAsyncIterator, builtinStringDecoderStdlibModule, builtinUrlStdlibModule, builtinUrlStdlibModuleInitialized, ensureBuiltinUrlStdlibModule, normalizeBuiltinRequest, __jsRuntimeBuiltinAllowlist, rejectRestrictedBuiltinRequest, loadBuiltinModule, requireFrom, module_default };
