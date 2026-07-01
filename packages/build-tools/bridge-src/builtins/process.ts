import { UndiciHeaders, UndiciRequest, UndiciResponse, encodeChildProcessIpcFrame, splitChildProcessIpcFrames } from "./child-process.js";
import { BUFFER_CONSTANTS, BUFFER_MAX_LENGTH, BUFFER_MAX_STRING_LENGTH } from "./buffer-constants.js";
import { EventEmitter, once } from "./events.js";
import { ReadStream, WriteStream, _fs, _processCpuUsage, _processMemoryUsage, _processResourceUsage, _processUmask, _processVersions, decodeBridgeJson, normalizeModeArgument } from "./fs.js";
import { builtinConstantsStdlibModule, builtinPathStdlibModule } from "./module-loader.js";
import { fetch } from "./network.js";
import { getRuntimeGid, getRuntimeUid } from "./os.js";
import { URL2, installWhatwgUrlGlobals } from "./whatwg-url.js";
import { exposeCustomGlobal, exposeMutableRuntimeStateGlobal } from "../global-exposure.js";
import { CustomEvent, Event, EventTarget, TextDecoder, TextEncoder2 } from "../polyfills.js";
import { bridgeDispatchSync } from "../transport.js";
import { require_base64_js, require_buffer } from "../vendor/buffer.js";
import { __toESM } from "../vendor/esbuild-runtime.js";

// .agent/recovery/secure-exec/nodejs/src/bridge/process.ts
var import_buffer2 = __toESM(require_buffer(), 1);
function readProcessConfig() {
  return {
    platform: typeof _processConfig !== "undefined" && _processConfig.platform || "linux",
    arch: typeof _processConfig !== "undefined" && _processConfig.arch || "x64",
    version: typeof _processConfig !== "undefined" && _processConfig.version || "v22.0.0",
    cwd: typeof _processConfig !== "undefined" && _processConfig.cwd || "/root",
    env: typeof _processConfig !== "undefined" && _processConfig.env || {},
    argv: typeof _processConfig !== "undefined" && _processConfig.argv || [
      "node",
      "script.js"
    ],
    execPath: typeof _processConfig !== "undefined" && _processConfig.execPath || "/usr/bin/node",
    pid: typeof _processConfig !== "undefined" && _processConfig.pid || 1,
    ppid: typeof _processConfig !== "undefined" && _processConfig.ppid || 0,
    uid: typeof _processConfig !== "undefined" && _processConfig.uid || 0,
    gid: typeof _processConfig !== "undefined" && _processConfig.gid || 0,
    stdin: typeof _processConfig !== "undefined" ? _processConfig.stdin : void 0,
    timingMitigation: typeof _processConfig !== "undefined" && _processConfig.timingMitigation || "off",
    frozenTimeMs: typeof _processConfig !== "undefined" ? _processConfig.frozenTimeMs : void 0
  };
}
var config2 = readProcessConfig();
var processClockNow = typeof performance !== "undefined" && performance && typeof performance.now === "function" ? performance.now.bind(performance) : Date.now;
function getNowMs() {
  if (config2.timingMitigation === "freeze" && typeof config2.frozenTimeMs === "number") {
    return config2.frozenTimeMs;
  }
  return processClockNow();
}
var _processStartTime = getNowMs();
var bufferPolyfillMutable = import_buffer2.Buffer;
if (typeof bufferPolyfillMutable.kMaxLength !== "number") {
  bufferPolyfillMutable.kMaxLength = BUFFER_MAX_LENGTH;
}
if (typeof bufferPolyfillMutable.kStringMaxLength !== "number") {
  bufferPolyfillMutable.kStringMaxLength = BUFFER_MAX_STRING_LENGTH;
}
if (typeof bufferPolyfillMutable.constants !== "object" || bufferPolyfillMutable.constants === null) {
  bufferPolyfillMutable.constants = {
    MAX_LENGTH: BUFFER_MAX_LENGTH,
    MAX_STRING_LENGTH: BUFFER_MAX_STRING_LENGTH
  };
}
var bufferProto = import_buffer2.Buffer.prototype;
if (typeof bufferProto.utf8Slice !== "function") {
  const encodings = ["utf8", "latin1", "ascii", "hex", "base64", "ucs2", "utf16le"];
  for (const enc of encodings) {
    if (typeof bufferProto[enc + "Slice"] !== "function") {
      bufferProto[enc + "Slice"] = function(start, end) {
        return this.toString(enc, start, end);
      };
    }
    if (typeof bufferProto[enc + "Write"] !== "function") {
      bufferProto[enc + "Write"] = function(string, offset, length) {
        return this.write(string, offset ?? 0, length ?? this.length - (offset ?? 0), enc);
      };
    }
  }
}
var bufferCtorMutable = import_buffer2.Buffer;
if (typeof bufferCtorMutable.allocUnsafe === "function" && !bufferCtorMutable.allocUnsafe._secureExecPatched) {
  const originalAllocUnsafe = bufferCtorMutable.allocUnsafe;
  bufferCtorMutable.allocUnsafe = function patchedAllocUnsafe(size) {
    try {
      return originalAllocUnsafe.call(this, size);
    } catch (error) {
      if (error instanceof RangeError && typeof size === "number" && size > BUFFER_MAX_LENGTH) {
        throw new Error("Array buffer allocation failed");
      }
      throw error;
    }
  };
  bufferCtorMutable.allocUnsafe._secureExecPatched = true;
}
var _exitCode = void 0;
var _exited = false;
var ProcessExitError = class extends Error {
  code;
  _isProcessExit;
  constructor(code) {
    super("process.exit(" + code + ")");
    this.name = "ProcessExitError";
    this.code = code;
    this._isProcessExit = true;
  }
};
exposeCustomGlobal("ProcessExitError", ProcessExitError);
var _signalNumbers = {
  SIGHUP: 1,
  SIGINT: 2,
  SIGQUIT: 3,
  SIGILL: 4,
  SIGTRAP: 5,
  SIGABRT: 6,
  SIGBUS: 7,
  SIGFPE: 8,
  SIGKILL: 9,
  SIGUSR1: 10,
  SIGSEGV: 11,
  SIGUSR2: 12,
  SIGPIPE: 13,
  SIGALRM: 14,
  SIGTERM: 15,
  SIGCHLD: 17,
  SIGCONT: 18,
  SIGSTOP: 19,
  SIGTSTP: 20,
  SIGTTIN: 21,
  SIGTTOU: 22,
  SIGURG: 23,
  SIGXCPU: 24,
  SIGXFSZ: 25,
  SIGVTALRM: 26,
  SIGPROF: 27,
  SIGWINCH: 28,
  SIGIO: 29,
  SIGPWR: 30,
  SIGSYS: 31
};
var _signalNamesByNumber = Object.fromEntries(
  Object.entries(_signalNumbers).map(([name, num]) => [num, name])
);
var _ignoredSelfSignals = /* @__PURE__ */ new Set(["SIGWINCH", "SIGCHLD", "SIGCONT", "SIGURG"]);
var _trackedProcessSignalEvents = /* @__PURE__ */ new Set(["SIGHUP", "SIGINT", "SIGTERM", "SIGWINCH", "SIGCHLD"]);
function _resolveSignal(signal) {
  if (signal === void 0 || signal === null) return 15;
  if (typeof signal === "number") return signal;
  const num = _signalNumbers[signal];
  if (num !== void 0) return num;
  throw new Error("Unknown signal: " + signal);
}
function _isTrackedProcessSignalEventName(eventName) {
  return typeof eventName === "string" && _trackedProcessSignalEvents.has(eventName);
}
var _processKillErrnoByCode = { ESRCH: 3, EPERM: 1, EINVAL: 22 };
function _createProcessKillError(error) {
  const message = String((error && error.message) || error || "");
  let code = null;
  if (error && typeof error.code === "string" && Object.prototype.hasOwnProperty.call(_processKillErrnoByCode, error.code)) {
    code = error.code;
  } else if (/\bESRCH\b/.test(message)) {
    code = "ESRCH";
  } else if (/\bEINVAL\b/.test(message)) {
    code = "EINVAL";
  } else if (/\bEPERM\b/.test(message) || /permission denied/i.test(message)) {
    code = "EPERM";
  }
  if (code === null) {
    return error instanceof Error ? error : new Error(message);
  }
  const err = new Error(`kill ${code}`);
  err.code = code;
  err.errno = -_processKillErrnoByCode[code];
  err.syscall = "kill";
  return err;
}
var _processListeners = {};
var _processOnceListeners = {};
var _processMaxListeners = 10;
var _processMaxListenersWarned = /* @__PURE__ */ new Set();
function _listenerCountForEvent(event) {
  return (_processListeners[event] || []).length + (_processOnceListeners[event] || []).length;
}
function _syncGuestProcessSignalState(eventName) {
  if (!_isTrackedProcessSignalEventName(eventName) || typeof _processSignalState === "undefined") {
    return;
  }
  const signal = _signalNumbers[eventName];
  if (typeof signal !== "number") {
    return;
  }
  const action = _listenerCountForEvent(eventName) > 0 ? "user" : "default";
  try {
    _processSignalState.applySyncPromise(void 0, [signal, action, JSON.stringify([]), 0]);
  } catch {
  }
}
function _syncAllGuestProcessSignalStates() {
  for (const eventName of _trackedProcessSignalEvents) {
    _syncGuestProcessSignalState(eventName);
  }
}
function _deliverProcessSignal(signal, action = "default") {
  const sigNum = _resolveSignal(signal);
  if (sigNum === 0) {
    return true;
  }
  const sigName = _signalNamesByNumber[sigNum] ?? `SIG${sigNum}`;
  if (action === "ignore") {
    return true;
  }
  if (_emit(sigName, sigName)) {
    return true;
  }
  if (_ignoredSelfSignals.has(sigName)) {
    return true;
  }
  return process2.exit(128 + sigNum);
}
function signalDispatch(eventType, payload) {
  if (eventType !== "signal" || payload === null || typeof payload !== "object") {
    return;
  }
  const signal = payload.signal ?? payload.number;
  const action = typeof payload.action === "string" ? payload.action : "default";
  _deliverProcessSignal(signal, action);
}
function _addListener(event, listener, once = false) {
  const target = once ? _processOnceListeners : _processListeners;
  if (!target[event]) {
    target[event] = [];
  }
  target[event].push(listener);
  if (_processMaxListeners > 0 && !_processMaxListenersWarned.has(event)) {
    const total = (_processListeners[event]?.length ?? 0) + (_processOnceListeners[event]?.length ?? 0);
    if (total > _processMaxListeners) {
      _processMaxListenersWarned.add(event);
      const warning = `MaxListenersExceededWarning: Possible EventEmitter memory leak detected. ${total} ${event} listeners added to [process]. MaxListeners is ${_processMaxListeners}. Use emitter.setMaxListeners() to increase limit`;
      if (typeof _error !== "undefined") {
        _error.applySync(void 0, [warning]);
      }
    }
  }
  _syncGuestProcessSignalState(event);
  return process2;
}
function _removeListener(event, listener) {
  if (_processListeners[event]) {
    const idx = _processListeners[event].indexOf(listener);
    if (idx !== -1) _processListeners[event].splice(idx, 1);
  }
  if (_processOnceListeners[event]) {
    const idx = _processOnceListeners[event].indexOf(listener);
    if (idx !== -1) _processOnceListeners[event].splice(idx, 1);
  }
  _syncGuestProcessSignalState(event);
  return process2;
}
function _emit(event, ...args) {
  let handled = false;
  if (_processListeners[event]) {
    for (const listener of _processListeners[event]) {
      listener.call(process2, ...args);
      handled = true;
    }
  }
  if (_processOnceListeners[event]) {
    const listeners = _processOnceListeners[event].slice();
    _processOnceListeners[event] = [];
    for (const listener of listeners) {
      listener.call(process2, ...args);
      handled = true;
    }
  }
  return handled;
}
function isProcessExitError(error) {
  return Boolean(
    error && typeof error === "object" && (error._isProcessExit === true || error.name === "ProcessExitError")
  );
}
function normalizeAsyncError(error) {
  return error instanceof Error ? error : new Error(String(error));
}
function routeAsyncCallbackError(error) {
  if (isProcessExitError(error)) {
    return { handled: false, rethrow: error };
  }
  const normalized = normalizeAsyncError(error);
  try {
    if (_emit("uncaughtException", normalized, "uncaughtException")) {
      return { handled: true, rethrow: null };
    }
  } catch (emitError) {
    return { handled: false, rethrow: emitError };
  }
  return { handled: false, rethrow: normalized };
}
function scheduleAsyncRethrow(error) {
  setTimeout2(() => {
    throw error;
  }, 0);
}
function dispatchCustomEmitterListeners(thisArg, listeners, args) {
  if (!listeners || listeners.length === 0) {
    return false;
  }
  for (const listener of listeners.slice()) {
    try {
      listener.call(thisArg, ...args);
    } catch (error) {
      const outcome = routeAsyncCallbackError(error);
      if (!outcome.handled && outcome.rethrow !== null) {
        throw outcome.rethrow;
      }
      return true;
    }
  }
  return true;
}
function _getStdinIsTTY() {
  return typeof __runtimeTtyConfig !== "undefined" && __runtimeTtyConfig.stdinIsTTY || false;
}
function _getStdoutIsTTY() {
  return typeof __runtimeTtyConfig !== "undefined" && __runtimeTtyConfig.stdoutIsTTY || false;
}
function _getStderrIsTTY() {
  return typeof __runtimeTtyConfig !== "undefined" && __runtimeTtyConfig.stderrIsTTY || false;
}
function getWriteCallback(encodingOrCallback, callback) {
  if (typeof encodingOrCallback === "function") {
    return encodingOrCallback;
  }
  if (typeof callback === "function") {
    return callback;
  }
  return void 0;
}
function emitListeners(listeners, onceListeners, event, args) {
  const persistent = listeners[event] ? listeners[event].slice() : [];
  const once = onceListeners[event] ? onceListeners[event].slice() : [];
  if (once.length > 0) {
    onceListeners[event] = [];
  }
  for (const listener of persistent) {
    listener(...args);
  }
  for (const listener of once) {
    listener(...args);
  }
  return persistent.length + once.length > 0;
}
function createStdioWriteStream(options) {
  const listeners = {};
  const onceListeners = {};
  const remove = (event, listener) => {
    if (listeners[event]) {
      const idx = listeners[event].indexOf(listener);
      if (idx !== -1) listeners[event].splice(idx, 1);
    }
    if (onceListeners[event]) {
      const idx = onceListeners[event].indexOf(listener);
      if (idx !== -1) onceListeners[event].splice(idx, 1);
    }
  };
  const stream = {
    write(data, encodingOrCallback, callback) {
      if (data instanceof Uint8Array || typeof import_buffer2.Buffer !== "undefined" && import_buffer2.Buffer.isBuffer(data)) {
        options.write(data);
      } else {
        options.write(String(data));
      }
      const done = getWriteCallback(encodingOrCallback, callback);
      if (done) {
        _queueMicrotask(() => done(null));
      }
      return true;
    },
    end(chunk, encoding, callback) {
      if (typeof chunk === "function") { callback = chunk; chunk = undefined; }
      else if (typeof encoding === "function") { callback = encoding; }
      if (chunk != null) stream.write(chunk);
      stream.writableEnded = true;
      if (typeof callback === "function") _queueMicrotask(() => callback());
      _queueMicrotask(() => emitListeners(listeners, onceListeners, "finish", []));
      return stream;
    },
    // Node Writable surface that process.stdout/stderr must expose (node-fidelity A7); these
    // streams are unbuffered host writes, so destroy/cork/uncork are no-ops that keep callers
    // (and the Claude EPIPE/buffered-destroy guards) on the standard path.
    destroyed: false,
    destroy(error) {
      if (stream.destroyed) return stream;
      stream.destroyed = true;
      if (error) _queueMicrotask(() => emitListeners(listeners, onceListeners, "error", [error]));
      _queueMicrotask(() => emitListeners(listeners, onceListeners, "close", []));
      return stream;
    },
    cork() {},
    uncork() {},
    setDefaultEncoding() { return stream; },
    on(event, listener) {
      if (!listeners[event]) listeners[event] = [];
      listeners[event].push(listener);
      return stream;
    },
    once(event, listener) {
      if (!onceListeners[event]) onceListeners[event] = [];
      onceListeners[event].push(listener);
      return stream;
    },
    off(event, listener) {
      remove(event, listener);
      return stream;
    },
    removeListener(event, listener) {
      remove(event, listener);
      return stream;
    },
    addListener(event, listener) {
      return stream.on(event, listener);
    },
    emit(event, ...args) {
      return emitListeners(listeners, onceListeners, event, args);
    },
    writable: true,
    get isTTY() {
      return options.isTTY();
    },
    get columns() {
      return typeof __runtimeTtyConfig !== "undefined" && __runtimeTtyConfig.cols || 80;
    },
    get rows() {
      return typeof __runtimeTtyConfig !== "undefined" && __runtimeTtyConfig.rows || 24;
    }
  };
  return stream;
}
var _stdout = createStdioWriteStream({
  write(data) {
    if (typeof _log !== "undefined") {
      _log.applySync(void 0, [data]);
    }
  },
  isTTY: _getStdoutIsTTY
});
var _stderr = createStdioWriteStream({
  write(data) {
    if (typeof _error !== "undefined") {
      _error.applySync(void 0, [data]);
    }
  },
  isTTY: _getStderrIsTTY
});
function formatConsoleValue(value) {
  if (typeof value === "string") {
    return value;
  }
  if (typeof value === "bigint") {
    return `${value}n`;
  }
  if (value instanceof Error) {
    return value.stack || value.message || String(value);
  }
  if (typeof value === "object" && value !== null) {
    try {
      return JSON.stringify(value);
    } catch {
    }
  }
  return String(value);
}
function formatConsoleArgs(args) {
  if (typeof builtinUtilModule !== "undefined" && typeof builtinUtilModule?.formatWithOptions === "function") {
    return builtinUtilModule.formatWithOptions({ colors: false }, ...args);
  }
  return args.map((value) => formatConsoleValue(value)).join(" ");
}
function formatConsoleLine(args) {
  return `${formatConsoleArgs(args)}\n`;
}
class Console {
  constructor(stdout = _stdout, stderr = _stderr) {
    this._stdout = stdout;
    this._stderr = stderr;
    this._counts = new Map();
    this._times = new Map();
    for (const method of [
      "assert",
      "clear",
      "count",
      "countReset",
      "debug",
      "dir",
      "dirxml",
      "error",
      "group",
      "groupCollapsed",
      "groupEnd",
      "info",
      "log",
      "table",
      "time",
      "timeEnd",
      "timeLog",
      "trace",
      "warn"
    ]) {
      this[method] = this[method].bind(this);
    }
  }
  log(...args) {
    this._stdout.write(formatConsoleLine(args));
  }
  info(...args) {
    this._stdout.write(formatConsoleLine(args));
  }
  debug(...args) {
    this._stdout.write(formatConsoleLine(args));
  }
  warn(...args) {
    this._stderr.write(formatConsoleLine(args));
  }
  error(...args) {
    this._stderr.write(formatConsoleLine(args));
  }
  dir(value) {
    this._stdout.write(formatConsoleLine([value]));
  }
  dirxml(...args) {
    this.log(...args);
  }
  trace(...args) {
    const message = formatConsoleArgs(args);
    const error = new Error(message);
    this._stderr.write(`${error.stack || message}\n`);
  }
  assert(condition, ...args) {
    if (!condition) {
      const message = args.length > 0 ? formatConsoleArgs(args) : "Assertion failed";
      this._stderr.write(`${message}\n`);
    }
  }
  clear() {
  }
  count(label = "default") {
    const next = (this._counts.get(label) ?? 0) + 1;
    this._counts.set(label, next);
    this.log(`${label}: ${next}`);
  }
  countReset(label = "default") {
    this._counts.delete(label);
  }
  group(...args) {
    if (args.length > 0) {
      this.log(...args);
    }
  }
  groupCollapsed(...args) {
    if (args.length > 0) {
      this.log(...args);
    }
  }
  groupEnd() {
  }
  table(tabularData) {
    this.log(tabularData);
  }
  time(label = "default") {
    this._times.set(label, Date.now());
  }
  timeEnd(label = "default") {
    if (!this._times.has(label)) {
      return;
    }
    const startedAt = this._times.get(label);
    this._times.delete(label);
    this.log(`${label}: ${Date.now() - startedAt}ms`);
  }
  timeLog(label = "default", ...args) {
    if (!this._times.has(label)) {
      return;
    }
    const startedAt = this._times.get(label);
    this.log(`${label}: ${Date.now() - startedAt}ms`, ...args);
  }
}
const defaultConsole = new Console();
globalThis.console = defaultConsole;
function createConsoleTask() {
  return {
    run(callback, ...args) {
      return typeof callback === "function" ? callback(...args) : void 0;
    }
  };
}
function consoleContext(stdout = _stdout, stderr = _stderr) {
  return new Console(stdout, stderr);
}
var builtinConsoleModule = {
  Console,
  assert: defaultConsole.assert.bind(defaultConsole),
  clear: defaultConsole.clear.bind(defaultConsole),
  context: consoleContext,
  count: defaultConsole.count.bind(defaultConsole),
  countReset: defaultConsole.countReset.bind(defaultConsole),
  createTask: createConsoleTask,
  debug: defaultConsole.debug.bind(defaultConsole),
  dir: defaultConsole.dir.bind(defaultConsole),
  dirxml: defaultConsole.dirxml.bind(defaultConsole),
  error: defaultConsole.error.bind(defaultConsole),
  group: defaultConsole.group.bind(defaultConsole),
  groupCollapsed: defaultConsole.groupCollapsed.bind(defaultConsole),
  groupEnd: defaultConsole.groupEnd.bind(defaultConsole),
  info: defaultConsole.info.bind(defaultConsole),
  log: defaultConsole.log.bind(defaultConsole),
  profile: void 0,
  profileEnd: void 0,
  table: defaultConsole.table.bind(defaultConsole),
  time: defaultConsole.time.bind(defaultConsole),
  timeEnd: defaultConsole.timeEnd.bind(defaultConsole),
  timeLog: defaultConsole.timeLog.bind(defaultConsole),
  timeStamp: void 0,
  trace: defaultConsole.trace.bind(defaultConsole),
  warn: defaultConsole.warn.bind(defaultConsole)
};
function v8Serialize(value) {
  const encoded = JSON.stringify(value ?? null);
  return Buffer.from(encoded, "utf8");
}
function v8Deserialize(value) {
  const buffer = Buffer.isBuffer(value) ? value : Buffer.from(value ?? []);
  return JSON.parse(buffer.toString("utf8"));
}
class V8Serializer {
  constructor() {
    this._value = null;
  }
  writeHeader() {
  }
  writeValue(value) {
    this._value = value;
  }
  releaseBuffer() {
    return v8Serialize(this._value);
  }
  transferArrayBuffer() {
  }
}
class V8Deserializer {
  constructor(buffer) {
    this._buffer = buffer;
  }
  readHeader() {
  }
  readValue() {
    return v8Deserialize(this._buffer);
  }
  transferArrayBuffer() {
  }
}
function configuredHeapLimitBytes() {
  const configured = Number(globalThis.__agentOSV8HeapLimitBytes);
  if (Number.isFinite(configured) && configured > 0) {
    return configured;
  }
  return 128 * 1024 * 1024;
}
function getHeapStatistics() {
  const heapLimit = configuredHeapLimitBytes();
  return {
    total_heap_size: Math.max(64 * 1024 * 1024, Math.floor(heapLimit / 2)),
    total_heap_size_executable: 1024 * 1024,
    total_physical_size: Math.max(64 * 1024 * 1024, Math.floor(heapLimit / 2)),
    total_available_size: Math.max(0, heapLimit - 64 * 1024 * 1024),
    used_heap_size: Math.max(0, Math.min(heapLimit, Math.floor(heapLimit * 0.4))),
    heap_size_limit: heapLimit,
    malloced_memory: 8192,
    peak_malloced_memory: 16384,
    does_zap_garbage: 0,
    number_of_native_contexts: 1,
    number_of_detached_contexts: 0,
    total_global_handles_size: 16384,
    used_global_handles_size: 8192,
    external_memory: 0
  };
}
function getHeapSpaceStatistics() {
  return [];
}
function getHeapCodeStatistics() {
  return {
    code_and_metadata_size: 0,
    bytecode_and_metadata_size: 0,
    external_script_source_size: 0,
    cpu_profiler_metadata_size: 0
  };
}
function getCppHeapStatistics() {
  return {
    committed_size_bytes: 0,
    resident_size_bytes: 0,
    used_size_bytes: 0,
    space_statistics: []
  };
}
function getHeapSnapshot() {
  return Readable.fromWeb(
    new ReadableStream({
      start(controller) {
        controller.enqueue(Buffer.from("{}"));
        controller.close();
      }
    })
  );
}
var builtinV8Module = {
  cachedDataVersionTag() {
    return 0;
  },
  DefaultDeserializer: V8Deserializer,
  DefaultSerializer: V8Serializer,
  Deserializer: V8Deserializer,
  GCProfiler: class GCProfiler {
    start() {
    }
    stop() {
      return [];
    }
  },
  Serializer: V8Serializer,
  deserialize: v8Deserialize,
  getCppHeapStatistics,
  getHeapCodeStatistics,
  getHeapSnapshot,
  getHeapSpaceStatistics,
  getHeapStatistics,
  isStringOneByteRepresentation(value) {
    return typeof value === "string" && !/[^\x00-\xff]/.test(value);
  },
  promiseHooks: {},
  queryObjects() {
    return [];
  },
  serialize: v8Serialize,
  setFlagsFromString() {
  },
  setHeapSnapshotNearHeapLimit() {
    return [];
  },
  startCpuProfile() {
    return {
      stop() {
        return {};
      }
    };
  },
  startupSnapshot: {},
  stopCoverage() {
    return [];
  },
  takeCoverage() {
    return [];
  },
  writeHeapSnapshot() {
    return "";
  }
};
var VM_CONTEXT_TAG = typeof Symbol === "function" ? Symbol.for("secure-exec.vm.context") : "__secure_exec_vm_context__";
var VM_CONTEXT_ID = typeof Symbol === "function" ? Symbol.for("secure-exec.vm.context.id") : "__secure_exec_vm_context_id__";
function createVmNotImplementedError(feature) {
  const error = new Error(`node:vm ${feature} is not implemented in the secure-exec guest runtime`);
  error.code = "ERR_NOT_IMPLEMENTED";
  return error;
}
function isVmContextCandidate(value) {
  return value !== null && (typeof value === "object" || typeof value === "function");
}
function normalizeVmOptions(options = void 0) {
  if (typeof options === "string") {
    return { filename: options };
  }
  if (!options || typeof options !== "object") {
    return {};
  }
  const normalized = {};
  if (typeof options.filename === "string") {
    normalized.filename = options.filename;
  }
  if (Number.isInteger(options.lineOffset)) {
    normalized.lineOffset = options.lineOffset;
  }
  if (Number.isInteger(options.columnOffset)) {
    normalized.columnOffset = options.columnOffset;
  }
  if (Number.isInteger(options.timeout) && options.timeout > 0) {
    normalized.timeout = options.timeout;
  }
  if (options.cachedData !== void 0) {
    normalized.cachedData = options.cachedData;
  }
  if (options.produceCachedData === true) {
    normalized.produceCachedData = true;
  }
  return normalized;
}
function mergeVmOptions(baseOptions, overrideOptions) {
  const base = normalizeVmOptions(baseOptions);
  const override = normalizeVmOptions(overrideOptions);
  return { ...base, ...override };
}
function vmCreateContext(context = {}) {
  if (!isVmContextCandidate(context)) {
    throw new TypeError('The "object" argument must be of type object.');
  }
  if (context[VM_CONTEXT_TAG] === true && Number.isInteger(context[VM_CONTEXT_ID])) {
    return context;
  }
  const contextId = _vmCreateContext(context);
  Object.defineProperty(context, VM_CONTEXT_TAG, {
    value: true,
    configurable: true,
    enumerable: false,
    writable: false
  });
  Object.defineProperty(context, VM_CONTEXT_ID, {
    value: contextId,
    configurable: false,
    enumerable: false,
    writable: false
  });
  return context;
}
function vmIsContext(context) {
  return isVmContextCandidate(context) && context[VM_CONTEXT_TAG] === true && Number.isInteger(context[VM_CONTEXT_ID]);
}
function assertVmContext(context) {
  if (!vmIsContext(context)) {
    throw new TypeError('The "contextifiedObject" argument must be a vm context.');
  }
  return context;
}
function vmRunInThisContext(code, options = void 0) {
  return _vmRunInThisContext(String(code), normalizeVmOptions(options));
}
function vmRunInContext(code, contextifiedObject, options = void 0) {
  const context = assertVmContext(contextifiedObject);
  return _vmRunInContext(context[VM_CONTEXT_ID], String(code), normalizeVmOptions(options), context);
}
function vmRunInNewContext(code, contextOrOptions = {}, maybeOptions = void 0) {
  const hasExplicitContext = isVmContextCandidate(contextOrOptions);
  const context = hasExplicitContext ? contextOrOptions : {};
  const options = hasExplicitContext ? maybeOptions : contextOrOptions;
  return vmRunInContext(code, vmCreateContext(context), options);
}
class VmScript {
  constructor(code, options = void 0) {
    this.code = String(code);
    this.options = normalizeVmOptions(options);
    this.filename = this.options.filename ?? "evalmachine.<anonymous>";
    this.lineOffset = this.options.lineOffset ?? 0;
    this.columnOffset = this.options.columnOffset ?? 0;
    this.cachedData = this.options.cachedData;
    this.cachedDataProduced = false;
    this.cachedDataRejected = false;
  }
  createCachedData() {
    return typeof Buffer === "function" ? Buffer.alloc(0) : new Uint8Array(0);
  }
  runInThisContext(options = void 0) {
    return vmRunInThisContext(this.code, mergeVmOptions(this.options, options));
  }
  runInContext(contextifiedObject, options = void 0) {
    return vmRunInContext(this.code, contextifiedObject, mergeVmOptions(this.options, options));
  }
  runInNewContext(context = {}, options = void 0) {
    return vmRunInNewContext(this.code, context, mergeVmOptions(this.options, options));
  }
}
var builtinVmModule = {
  Script: VmScript,
  compileFunction() {
    throw createVmNotImplementedError("compileFunction");
  },
  createContext: vmCreateContext,
  isContext: vmIsContext,
  measureMemory() {
    throw createVmNotImplementedError("measureMemory");
  },
  runInContext: vmRunInContext,
  runInNewContext: vmRunInNewContext,
  runInThisContext: vmRunInThisContext
};
function createWorkerThreadsNotImplementedError(feature) {
  const error = new Error(`node:worker_threads ${feature} is not available in the secure-exec guest runtime`);
  error.code = "ERR_NOT_IMPLEMENTED";
  return error;
}
class WorkerThreadPort extends EventEmitter {
  postMessage() {
  }
  start() {
  }
  close() {
    this.emit("close");
  }
  unref() {
    return this;
  }
  ref() {
    return this;
  }
}
class WorkerThreadMessageChannel {
  constructor() {
    this.port1 = new WorkerThreadPort();
    this.port2 = new WorkerThreadPort();
  }
}
class WorkerThreadWorker extends EventEmitter {
  constructor() {
    super();
    throw createWorkerThreadsNotImplementedError("Worker");
  }
}
var builtinWorkerThreadsModule = {
  BroadcastChannel: globalThis.BroadcastChannel,
  MessageChannel: globalThis.MessageChannel ?? WorkerThreadMessageChannel,
  MessagePort: globalThis.MessagePort ?? WorkerThreadPort,
  SHARE_ENV: Symbol.for("secure-exec.worker_threads.SHARE_ENV"),
  Worker: WorkerThreadWorker,
  getEnvironmentData() {
    return void 0;
  },
  isMainThread: true,
  markAsUncloneable() {
  },
  markAsUntransferable() {
  },
  moveMessagePortToContext() {
    throw createWorkerThreadsNotImplementedError("moveMessagePortToContext");
  },
  parentPort: null,
  postMessageToThread() {
    throw createWorkerThreadsNotImplementedError("postMessageToThread");
  },
  receiveMessageOnPort() {
    return void 0;
  },
  resourceLimits: {},
  setEnvironmentData() {
  },
  threadId: 0,
  workerData: null
};
var _stdinListeners = {};
var _stdinOnceListeners = {};
var _stdinLiveDecoder = new TextDecoder();
var STDIN_HANDLE_ID = "process.stdin";
var _stdinLiveBuffer = "";
var _stdinLiveStarted = false;
var _stdinLiveHandleRegistered = false;
var _stdinLiveTerminalEventsScheduled = false;
var _stdinLiveTerminalEventsEmitted = false;
exposeMutableRuntimeStateGlobal(
  "_stdinData",
  typeof _processConfig !== "undefined" && _processConfig.stdin || ""
);
exposeMutableRuntimeStateGlobal("_stdinPosition", 0);
exposeMutableRuntimeStateGlobal("_stdinEnded", false);
exposeMutableRuntimeStateGlobal("_stdinFlowMode", false);
function getStdinData() {
  return globalThis._stdinData;
}
function setStdinDataValue(v) {
  globalThis._stdinData = v;
}
function getStdinPosition() {
  return globalThis._stdinPosition;
}
function setStdinPosition(v) {
  globalThis._stdinPosition = v;
}
function getStdinEnded() {
  return globalThis._stdinEnded;
}
function setStdinEnded(v) {
  globalThis._stdinEnded = v;
}
function getStdinFlowMode() {
  return globalThis._stdinFlowMode;
}
function setStdinFlowMode(v) {
  globalThis._stdinFlowMode = v;
}
function _emitStdinData() {
  if (getStdinEnded() || !getStdinData()) return;
  if (getStdinFlowMode() && getStdinPosition() < getStdinData().length) {
    const chunk = getStdinData().slice(getStdinPosition());
    setStdinPosition(getStdinData().length);
    const dataListeners = [..._stdinListeners["data"] || [], ..._stdinOnceListeners["data"] || []];
    _stdinOnceListeners["data"] = [];
    for (const listener of dataListeners) {
      listener(chunk);
    }
    setStdinEnded(true);
    const endListeners = [..._stdinListeners["end"] || [], ..._stdinOnceListeners["end"] || []];
    _stdinOnceListeners["end"] = [];
    for (const listener of endListeners) {
      listener();
    }
    const closeListeners = [..._stdinListeners["close"] || [], ..._stdinOnceListeners["close"] || []];
    _stdinOnceListeners["close"] = [];
    for (const listener of closeListeners) {
      listener();
    }
  }
}
function emitStdinListeners(event, value) {
  const listeners = [..._stdinListeners[event] || [], ..._stdinOnceListeners[event] || []];
  _stdinOnceListeners[event] = [];
  for (const listener of listeners) {
    try {
      listener(value);
    } catch (error) {
      const outcome = routeAsyncCallbackError(error);
      if (!outcome.handled && outcome.rethrow !== null) {
        if (isProcessExitError(outcome.rethrow)) {
          scheduleAsyncRethrow(outcome.rethrow);
          return true;
        }
        throw outcome.rethrow;
      }
      return true;
    }
  }
  return listeners.length > 0;
}
function syncLiveStdinHandle(active) {
  if (active) {
    if (!_stdinLiveHandleRegistered && typeof _registerHandle === "function") {
      try {
        _registerHandle(STDIN_HANDLE_ID, "process.stdin");
        _stdinLiveHandleRegistered = true;
      } catch {
      }
    }
    return;
  }
  if (_stdinLiveHandleRegistered && typeof _unregisterHandle === "function") {
    try {
      _unregisterHandle(STDIN_HANDLE_ID);
    } catch {
    }
    _stdinLiveHandleRegistered = false;
  }
}
function flushLiveStdinBuffer() {
  if (!getStdinFlowMode() || _stdinLiveBuffer.length === 0) return;
  const chunk = _stdinLiveBuffer;
  _stdinLiveBuffer = "";
  const data = _stdin.encoding ? chunk : import_buffer2.Buffer.from(chunk);
  emitStdinListeners("data", data);
  maybeEmitLiveStdinTerminalEvents();
}
function maybeEmitLiveStdinTerminalEvents() {
  if (!getStdinEnded() || _stdinLiveTerminalEventsEmitted || _stdinLiveBuffer.length > 0) {
    return;
  }
  if (_stdinLiveTerminalEventsScheduled) {
    return;
  }
  _stdinLiveTerminalEventsScheduled = true;
  queueMicrotask(() => {
    _stdinLiveTerminalEventsScheduled = false;
    if (!getStdinEnded() || _stdinLiveTerminalEventsEmitted || _stdinLiveBuffer.length > 0) {
      return;
    }
    _stdinLiveTerminalEventsEmitted = true;
    emitStdinListeners("end");
    emitStdinListeners("close");
    syncLiveStdinHandle(false);
  });
}
function finishLiveStdin() {
  if (getStdinEnded()) return;
  setStdinEnded(true);
  flushLiveStdinBuffer();
  maybeEmitLiveStdinTerminalEvents();
}
function _getStreamStdin() {
  return typeof __runtimeStreamStdin !== "undefined" && !!__runtimeStreamStdin;
}
function ensureLiveStdinStarted() {
  if (_stdinLiveStarted) return;
  if (!_getStdinIsTTY() && !_getStreamStdin()) return;
  _stdinLiveStarted = true;
  syncLiveStdinHandle(!_stdin.paused);
  if (_getStreamStdin()) {
    return;
  }
  if (typeof _kernelStdinRead === "undefined") return;
  void (async () => {
    try {
      while (!getStdinEnded()) {
        if (typeof _kernelStdinRead === "undefined") {
          break;
        }
        const next = await _kernelStdinRead.apply(void 0, [65536, 100], {
          result: { promise: true }
        });
        if (next?.done) {
          break;
        }
        const dataBase64 = String(next?.dataBase64 ?? "");
        if (!dataBase64) {
          continue;
        }
        _stdinLiveBuffer += _stdinLiveDecoder.decode(
          import_buffer2.Buffer.from(dataBase64, "base64"),
          { stream: true }
        );
        flushLiveStdinBuffer();
      }
    } catch {
    }
    _stdinLiveBuffer += _stdinLiveDecoder.decode();
    finishLiveStdin();
  })();
}
function stdinDispatch(eventType, payload) {
  if (eventType === "stdin_end") {
    finishLiveStdin();
    return;
  }
  if (eventType !== "stdin" || getStdinEnded()) {
    return;
  }
  let chunk;
  let binary = false;
  if (payload && typeof payload === "object" && typeof payload.dataBase64 === "string") {
    const bytes = import_buffer2.Buffer.from(payload.dataBase64, "base64");
    if (bytes.length === 0) {
      return;
    }
    if (!_stdin.encoding && getStdinFlowMode()) {
      emitStdinListeners("data", bytes);
      maybeEmitLiveStdinTerminalEvents();
      return;
    }
    chunk = _stdin.encoding ? bytes.toString(_stdin.encoding) : bytes.toString("latin1");
    binary = !_stdin.encoding;
  } else {
    chunk = typeof payload === "string" ? payload : payload == null ? "" : import_buffer2.Buffer.from(payload).toString("utf8");
  }
  if (!chunk) {
    return;
  }
  _stdinLiveBuffer += chunk;
  if (binary && !_stdin.encoding && getStdinFlowMode()) {
    const buffered = _stdinLiveBuffer;
    _stdinLiveBuffer = "";
    emitStdinListeners("data", import_buffer2.Buffer.from(buffered, "latin1"));
    maybeEmitLiveStdinTerminalEvents();
    return;
  }
  flushLiveStdinBuffer();
}
var _stdin = {
  readable: true,
  paused: true,
  encoding: null,
  isRaw: false,
  read(size) {
    if (_stdinLiveBuffer.length > 0) {
      if (!size || size >= _stdinLiveBuffer.length) {
        const chunk3 = _stdinLiveBuffer;
        _stdinLiveBuffer = "";
        return chunk3;
      }
      const chunk2 = _stdinLiveBuffer.slice(0, size);
      _stdinLiveBuffer = _stdinLiveBuffer.slice(size);
      return chunk2;
    }
    if (getStdinPosition() >= getStdinData().length) return null;
    const chunk = size ? getStdinData().slice(getStdinPosition(), getStdinPosition() + size) : getStdinData().slice(getStdinPosition());
    setStdinPosition(getStdinPosition() + chunk.length);
    return chunk;
  },
  on(event, listener) {
    if (!_stdinListeners[event]) _stdinListeners[event] = [];
    _stdinListeners[event].push(listener);
    if ((_getStdinIsTTY() || _getStreamStdin()) && (event === "data" || event === "end" || event === "close")) {
      ensureLiveStdinStarted();
    }
    if (event === "data" && this.paused) {
      this.resume();
    }
    if ((event === "end" || event === "close") && (_getStdinIsTTY() || _getStreamStdin())) {
      maybeEmitLiveStdinTerminalEvents();
    }
    if (event === "end" && getStdinData() && !getStdinEnded()) {
      setStdinFlowMode(true);
      _emitStdinData();
    }
    return this;
  },
  once(event, listener) {
    if (!_stdinOnceListeners[event]) _stdinOnceListeners[event] = [];
    _stdinOnceListeners[event].push(listener);
    if ((_getStdinIsTTY() || _getStreamStdin()) && (event === "data" || event === "end" || event === "close")) {
      ensureLiveStdinStarted();
    }
    if (event === "data" && this.paused) {
      this.resume();
    }
    if ((event === "end" || event === "close") && (_getStdinIsTTY() || _getStreamStdin())) {
      maybeEmitLiveStdinTerminalEvents();
    }
    if (event === "end" && getStdinData() && !getStdinEnded()) {
      setStdinFlowMode(true);
      _emitStdinData();
    }
    return this;
  },
  off(event, listener) {
    if (_stdinListeners[event]) {
      const idx = _stdinListeners[event].indexOf(listener);
      if (idx !== -1) _stdinListeners[event].splice(idx, 1);
    }
    return this;
  },
  removeListener(event, listener) {
    return this.off(event, listener);
  },
  emit(event, ...args) {
    const listeners = [..._stdinListeners[event] || [], ..._stdinOnceListeners[event] || []];
    _stdinOnceListeners[event] = [];
    for (const listener of listeners) {
      listener(args[0]);
    }
    return listeners.length > 0;
  },
  pause() {
    this.paused = true;
    setStdinFlowMode(false);
    syncLiveStdinHandle(false);
    return this;
  },
  resume() {
    if (_getStdinIsTTY() || _getStreamStdin()) {
      ensureLiveStdinStarted();
      syncLiveStdinHandle(true);
    }
    this.paused = false;
    setStdinFlowMode(true);
    flushLiveStdinBuffer();
    _emitStdinData();
    maybeEmitLiveStdinTerminalEvents();
    return this;
  },
  setEncoding(enc) {
    this.encoding = enc;
    return this;
  },
  setRawMode(mode) {
    if (!_getStdinIsTTY()) {
      throw new Error("setRawMode is not supported when stdin is not a TTY");
    }
    if (typeof _ptySetRawMode !== "undefined") {
      _ptySetRawMode.applySync(void 0, [mode]);
    }
    this.isRaw = mode;
    return this;
  },
  get isTTY() {
    return _getStdinIsTTY();
  },
  [Symbol.asyncIterator]: function() {
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
    stream.on("end", onEnd);
    stream.on("close", onEnd);
    stream.on("error", onError);
    stream.on("data", onData);
    stream.resume();
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
};
exposeCustomGlobal("_stdinDispatch", stdinDispatch);
exposeCustomGlobal("_signalDispatch", signalDispatch);
function hrtime(prev) {
  const now = getNowMs();
  const seconds = Math.floor(now / 1e3);
  const nanoseconds = Math.floor(now % 1e3 * 1e6);
  if (prev) {
    let diffSec = seconds - prev[0];
    let diffNano = nanoseconds - prev[1];
    if (diffNano < 0) {
      diffSec -= 1;
      diffNano += 1e9;
    }
    return [diffSec, diffNano];
  }
  return [seconds, nanoseconds];
}
hrtime.bigint = function() {
  const now = getNowMs();
  return BigInt(Math.floor(now * 1e6));
};
var _cwd = config2.cwd;
var _umask = 18;
var _processVersionsCache = {
  node: config2.version.replace(/^v/, ""),
  v8: "11.3.244.8",
  uv: "1.44.2",
  zlib: "1.2.13",
  brotli: "1.0.9",
  ares: "1.19.0",
  modules: "108",
  nghttp2: "1.52.0",
  napi: "8",
  llhttp: "8.1.0",
  openssl: "3.0.8",
  cldr: "42.0",
  icu: "72.1",
  tz: "2022g",
  unicode: "15.0"
};
function defaultProcessMemoryUsage() {
  return {
    rss: 50 * 1024 * 1024,
    heapTotal: 20 * 1024 * 1024,
    heapUsed: 10 * 1024 * 1024,
    external: 1 * 1024 * 1024,
    arrayBuffers: 500 * 1024
  };
}
function readLiveProcessMemoryUsage() {
  const fallback = defaultProcessMemoryUsage();
  const usage = _processMemoryUsage.applySyncPromise(void 0, []);
  if (!usage || typeof usage !== "object") {
    return fallback;
  }
  return {
    rss: Number.isFinite(usage.rss) ? Number(usage.rss) : fallback.rss,
    heapTotal: Number.isFinite(usage.heapTotal) ? Number(usage.heapTotal) : fallback.heapTotal,
    heapUsed: Number.isFinite(usage.heapUsed) ? Number(usage.heapUsed) : fallback.heapUsed,
    external: Number.isFinite(usage.external) ? Number(usage.external) : fallback.external,
    arrayBuffers: Number.isFinite(usage.arrayBuffers) ? Number(usage.arrayBuffers) : fallback.arrayBuffers
  };
}
function readLiveProcessCpuUsage(prev) {
  const usage = _processCpuUsage.applySyncPromise(void 0, [prev ?? null]);
  if (usage && typeof usage === "object") {
    return {
      user: Number.isFinite(usage.user) ? Number(usage.user) : 1e6,
      system: Number.isFinite(usage.system) ? Number(usage.system) : 5e5
    };
  }
  const fallback = {
    user: 1e6,
    system: 5e5
  };
  if (prev && typeof prev === "object") {
    return {
      user: fallback.user - Number(prev.user || 0),
      system: fallback.system - Number(prev.system || 0)
    };
  }
  return fallback;
}
function defaultProcessResourceUsage() {
  return {
    userCPUTime: 1e6,
    systemCPUTime: 5e5,
    maxRSS: 50 * 1024,
    sharedMemorySize: 0,
    unsharedDataSize: 0,
    unsharedStackSize: 0,
    minorPageFault: 0,
    majorPageFault: 0,
    swappedOut: 0,
    fsRead: 0,
    fsWrite: 0,
    ipcSent: 0,
    ipcReceived: 0,
    signalsCount: 0,
    voluntaryContextSwitches: 0,
    involuntaryContextSwitches: 0
  };
}
function readLiveProcessResourceUsage() {
  const fallback = defaultProcessResourceUsage();
  const usage = _processResourceUsage.applySyncPromise(void 0, []);
  if (!usage || typeof usage !== "object") {
    return fallback;
  }
  return {
    userCPUTime: Number.isFinite(usage.userCPUTime) ? Number(usage.userCPUTime) : fallback.userCPUTime,
    systemCPUTime: Number.isFinite(usage.systemCPUTime) ? Number(usage.systemCPUTime) : fallback.systemCPUTime,
    maxRSS: Number.isFinite(usage.maxRSS) ? Number(usage.maxRSS) : fallback.maxRSS,
    sharedMemorySize: Number.isFinite(usage.sharedMemorySize) ? Number(usage.sharedMemorySize) : fallback.sharedMemorySize,
    unsharedDataSize: Number.isFinite(usage.unsharedDataSize) ? Number(usage.unsharedDataSize) : fallback.unsharedDataSize,
    unsharedStackSize: Number.isFinite(usage.unsharedStackSize) ? Number(usage.unsharedStackSize) : fallback.unsharedStackSize,
    minorPageFault: Number.isFinite(usage.minorPageFault) ? Number(usage.minorPageFault) : fallback.minorPageFault,
    majorPageFault: Number.isFinite(usage.majorPageFault) ? Number(usage.majorPageFault) : fallback.majorPageFault,
    swappedOut: Number.isFinite(usage.swappedOut) ? Number(usage.swappedOut) : fallback.swappedOut,
    fsRead: Number.isFinite(usage.fsRead) ? Number(usage.fsRead) : fallback.fsRead,
    fsWrite: Number.isFinite(usage.fsWrite) ? Number(usage.fsWrite) : fallback.fsWrite,
    ipcSent: Number.isFinite(usage.ipcSent) ? Number(usage.ipcSent) : fallback.ipcSent,
    ipcReceived: Number.isFinite(usage.ipcReceived) ? Number(usage.ipcReceived) : fallback.ipcReceived,
    signalsCount: Number.isFinite(usage.signalsCount) ? Number(usage.signalsCount) : fallback.signalsCount,
    voluntaryContextSwitches: Number.isFinite(usage.voluntaryContextSwitches) ? Number(usage.voluntaryContextSwitches) : fallback.voluntaryContextSwitches,
    involuntaryContextSwitches: Number.isFinite(usage.involuntaryContextSwitches) ? Number(usage.involuntaryContextSwitches) : fallback.involuntaryContextSwitches
  };
}
function readLiveProcessVersions() {
  _processVersionsCache.node = config2.version.replace(/^v/, "");
  const versions = _processVersions.applySyncPromise(void 0, []);
  if (versions && typeof versions === "object") {
    Object.assign(_processVersionsCache, versions);
    _processVersionsCache.node = config2.version.replace(/^v/, "");
  }
  return _processVersionsCache;
}
var process2 = {
  // Static properties
  platform: config2.platform,
  arch: config2.arch,
  version: config2.version,
  get versions() {
    return readLiveProcessVersions();
  },
  pid: config2.pid,
  ppid: config2.ppid,
  execPath: config2.execPath,
  execArgv: [],
  argv: config2.argv,
  argv0: config2.argv[0] || "node",
  title: "node",
  env: config2.env,
  // Config stubs
  config: {
    target_defaults: {
      cflags: [],
      default_configuration: "Release",
      defines: [],
      include_dirs: [],
      libraries: []
    },
    variables: {
      node_prefix: "/usr",
      node_shared_libuv: false
    }
  },
  release: {
    name: "node",
    sourceUrl: "https://nodejs.org/download/release/v20.0.0/node-v20.0.0.tar.gz",
    headersUrl: "https://nodejs.org/download/release/v20.0.0/node-v20.0.0-headers.tar.gz"
  },
  // Feature flags
  features: {
    inspector: false,
    debug: false,
    uv: true,
    ipv6: true,
    tls_alpn: true,
    tls_sni: true,
    tls_ocsp: true,
    tls: true
  },
  // Methods
  cwd() {
    return _cwd;
  },
  chdir(dir) {
    let statJson;
    try {
      statJson = _fs.stat.applySyncPromise(void 0, [dir]);
    } catch {
      const err = new Error(`ENOENT: no such file or directory, chdir '${dir}'`);
      err.code = "ENOENT";
      err.errno = -2;
      err.syscall = "chdir";
      err.path = dir;
      throw err;
    }
    const parsed = decodeBridgeJson(statJson);
    if (!parsed.isDirectory) {
      const err = new Error(`ENOTDIR: not a directory, chdir '${dir}'`);
      err.code = "ENOTDIR";
      err.errno = -20;
      err.syscall = "chdir";
      err.path = dir;
      throw err;
    }
    _cwd = dir;
  },
  get exitCode() {
    return _exitCode;
  },
  set exitCode(code) {
    _exitCode = code == null ? void 0 : code;
  },
  exit(code) {
    const exitCode = code !== void 0 ? code : _exitCode ?? 0;
    _exitCode = exitCode;
    _exited = true;
    try {
      _emit("exit", exitCode);
    } catch (_e) {
    }
    throw new ProcessExitError(exitCode);
  },
  abort() {
    return process2.kill(process2.pid, "SIGABRT");
  },
  nextTick(callback, ...args) {
    const asyncLocalStorageSnapshot = snapshotAsyncLocalStorageStores();
    _nextTickQueue.push({
      callback: wrapAsyncLocalStorageCallback(callback, asyncLocalStorageSnapshot),
      args
    });
    scheduleNextTickFlush();
  },
  hrtime,
  getuid() {
    return getRuntimeUid();
  },
  getgid() {
    return getRuntimeGid();
  },
  geteuid() {
    const value = globalThis.process?.euid;
    return Number.isFinite(value) ? value : getRuntimeUid();
  },
  getegid() {
    const value = globalThis.process?.egid;
    return Number.isFinite(value) ? value : getRuntimeGid();
  },
  getgroups() {
    return Array.isArray(globalThis.process?.groups) && globalThis.process.groups.length > 0 ? [...globalThis.process.groups] : [getRuntimeGid()];
  },
  setuid() {
  },
  setgid() {
  },
  seteuid() {
  },
  setegid() {
  },
  setgroups() {
  },
  umask(mask) {
    const normalizedMask = mask === void 0 ? void 0 : normalizeModeArgument(mask, "mask");
    const previousMask = Number(_processUmask.applySyncPromise(void 0, [normalizedMask ?? null]));
    if (Number.isFinite(previousMask)) {
      _umask = normalizedMask ?? previousMask;
      return previousMask;
    }
    const oldMask = _umask;
    if (normalizedMask !== void 0) {
      _umask = normalizedMask;
    }
    return oldMask;
  },
  uptime() {
    return (getNowMs() - _processStartTime) / 1e3;
  },
  memoryUsage() {
    return readLiveProcessMemoryUsage();
  },
  cpuUsage(prev) {
    return readLiveProcessCpuUsage(prev);
  },
  resourceUsage() {
    return readLiveProcessResourceUsage();
  },
  kill(pid, signal) {
    if (typeof pid !== "number" || !Number.isFinite(pid) || !Number.isInteger(pid)) {
      throw new TypeError(`The "pid" argument must be an integer. Received ${String(pid)}`);
    }
    const sigNum = _resolveSignal(signal);
    const sigName = _signalNamesByNumber[sigNum] ?? `SIG${sigNum}`;
    if (typeof _processKill !== "undefined") {
      let rawResult;
      try {
        rawResult = _processKill.applySyncPromise(void 0, [pid, sigName]);
      } catch (error) {
        throw _createProcessKillError(error);
      }
      let result = rawResult;
      if (typeof result === "string") {
        try {
          result = JSON.parse(result);
        } catch {
          result = null;
        }
      }
      if (result && typeof result === "object" && result.self === true) {
        const action = typeof result.action === "string" ? result.action : "default";
        return _deliverProcessSignal(sigNum, action);
      }
      return true;
    }
    if (pid !== process2.pid) {
      const err = new Error("Operation not permitted");
      err.code = "EPERM";
      err.errno = -1;
      err.syscall = "kill";
      throw err;
    }
    return _deliverProcessSignal(sigNum, "default");
  },
  // EventEmitter methods
  on(event, listener) {
    return _addListener(event, listener);
  },
  once(event, listener) {
    return _addListener(event, listener, true);
  },
  removeListener(event, listener) {
    return _removeListener(event, listener);
  },
  // off is an alias for removeListener (assigned below to be same reference)
  off: null,
  removeAllListeners(event) {
    if (event) {
      delete _processListeners[event];
      delete _processOnceListeners[event];
      _syncGuestProcessSignalState(event);
    } else {
      Object.keys(_processListeners).forEach((k) => delete _processListeners[k]);
      Object.keys(_processOnceListeners).forEach(
        (k) => delete _processOnceListeners[k]
      );
      _syncAllGuestProcessSignalStates();
    }
    return process2;
  },
  addListener(event, listener) {
    return _addListener(event, listener);
  },
  emit(event, ...args) {
    return _emit(event, ...args);
  },
  listeners(event) {
    return [
      ..._processListeners[event] || [],
      ..._processOnceListeners[event] || []
    ];
  },
  listenerCount(event) {
    return _listenerCountForEvent(event);
  },
  prependListener(event, listener) {
    if (!_processListeners[event]) {
      _processListeners[event] = [];
    }
    _processListeners[event].unshift(listener);
    _syncGuestProcessSignalState(event);
    return process2;
  },
  prependOnceListener(event, listener) {
    if (!_processOnceListeners[event]) {
      _processOnceListeners[event] = [];
    }
    _processOnceListeners[event].unshift(listener);
    _syncGuestProcessSignalState(event);
    return process2;
  },
  eventNames() {
    return [
      .../* @__PURE__ */ new Set([
        ...Object.keys(_processListeners),
        ...Object.keys(_processOnceListeners)
      ])
    ];
  },
  setMaxListeners(n) {
    _processMaxListeners = n;
    return process2;
  },
  getMaxListeners() {
    return _processMaxListeners;
  },
  rawListeners(event) {
    return process2.listeners(event);
  },
  // Stdio streams
  stdout: _stdout,
  stderr: _stderr,
  stdin: _stdin,
  // Process state
  connected: config2.env?.AGENTOS_NODE_IPC === "1",
  // Module info (will be set by createRequire)
  mainModule: void 0,
  // No-op methods for compatibility
  emitWarning(warning) {
    if (warning && typeof warning === "object") {
      if (typeof warning.message !== "string") {
        warning.message = String(warning.message ?? "");
      }
      if (typeof warning.name !== "string" || warning.name.length === 0) {
        warning.name = "Warning";
      }
      _emit("warning", warning);
      return;
    }
    _emit("warning", {
      message: String(warning ?? ""),
      name: "Warning"
    });
  },
  binding(_name) {
    const error = new Error("process.binding is not supported in sandbox");
    error.code = "ERR_ACCESS_DENIED";
    throw error;
  },
  _linkedBinding(_name) {
    const error = new Error("process._linkedBinding is not supported in sandbox");
    error.code = "ERR_ACCESS_DENIED";
    throw error;
  },
  dlopen() {
    throw new Error("process.dlopen is not supported");
  },
  hasUncaughtExceptionCaptureCallback() {
    return false;
  },
  setUncaughtExceptionCaptureCallback() {
  },
  send(message, sendHandleOrOptions, optionsOrCallback, maybeCallback) {
    const callback = typeof sendHandleOrOptions === "function" ? sendHandleOrOptions : typeof optionsOrCallback === "function" ? optionsOrCallback : maybeCallback;
    if (!process2.connected) {
      return false;
    }
    try {
      process2.stdout.write(encodeChildProcessIpcFrame(message));
      if (callback) {
        queueMicrotask(() => callback(null));
      }
      return true;
    } catch (error) {
      if (callback) {
        queueMicrotask(() => callback(error));
        return false;
      }
      throw error;
    }
  },
  disconnect() {
    if (!process2.connected) {
      return;
    }
    process2.connected = false;
    if (process2._agentOSIpcHandleId && typeof _unregisterHandle === "function") {
      _unregisterHandle(process2._agentOSIpcHandleId);
      process2._agentOSIpcHandleId = null;
    }
    _emit("disconnect");
  },
  // Report
  report: {
    directory: "",
    filename: "",
    compact: false,
    signal: "SIGUSR2",
    reportOnFatalError: false,
    reportOnSignal: false,
    reportOnUncaughtException: false,
    getReport() {
      return {};
    },
    writeReport() {
      return "";
    }
  },
  // Debug port
  debugPort: 9229,
  // Internal state
  _cwd: config2.cwd,
  _umask: 18
};
function installProcessIpcBridge() {
  const ipcEnabled = config2.env?.AGENTOS_NODE_IPC === "1" || globalThis.__agentOSProcessConfigEnv?.AGENTOS_NODE_IPC === "1";
  if (!ipcEnabled || process2._agentOSIpcInstalled) {
    return;
  }
  process2._agentOSIpcInstalled = true;
  process2.connected = true;
  if (!process2._agentOSIpcHandleId && typeof _registerHandle === "function") {
    process2._agentOSIpcHandleId = `process-ipc:${process2.pid}`;
    _registerHandle(process2._agentOSIpcHandleId, "child_process IPC channel");
  }
  let ipcInputBuffer = "";
  process2.stdin.on("data", (chunk) => {
    const parsed = splitChildProcessIpcFrames(ipcInputBuffer, chunk);
    ipcInputBuffer = parsed.buffer;
    for (const message of parsed.messages) {
      _emit("message", message, void 0);
    }
  });
}
function applyProcessConfig(nextConfig) {
  syncLiveStdinHandle(false);
  _stdinLiveBuffer = "";
  _stdinLiveStarted = false;
  _stdinLiveDecoder = new TextDecoder();
  _stdinLiveTerminalEventsScheduled = false;
  _stdinLiveTerminalEventsEmitted = false;
  for (const key of Object.keys(_stdinListeners)) {
    _stdinListeners[key] = [];
  }
  for (const key of Object.keys(_stdinOnceListeners)) {
    _stdinOnceListeners[key] = [];
  }
  setStdinDataValue(nextConfig.stdin ?? "");
  setStdinPosition(0);
  setStdinEnded(false);
  setStdinFlowMode(false);
  config2 = nextConfig;
  _cwd = nextConfig.cwd;
  process2.platform = nextConfig.platform;
  process2.arch = nextConfig.arch;
  process2.version = nextConfig.version;
  process2.pid = nextConfig.pid;
  process2.ppid = nextConfig.ppid;
  process2.execPath = nextConfig.execPath;
  process2.argv = nextConfig.argv;
  process2.argv0 = nextConfig.argv[0] || "node";
  process2.env = nextConfig.env;
  process2.connected = nextConfig.env?.AGENTOS_NODE_IPC === "1";
  process2._cwd = nextConfig.cwd;
  process2.stdin.paused = true;
  process2.stdin.encoding = null;
  process2.stdin.isRaw = false;
  _processVersionsCache.node = nextConfig.version.replace(/^v/, "");
}
exposeCustomGlobal("__runtimeRefreshProcessConfig", () => {
  applyProcessConfig(readProcessConfig());
});
process2.off = process2.removeListener;
exposeCustomGlobal("__runtimeInstallProcessIpcBridge", installProcessIpcBridge);
installProcessIpcBridge();
process2.memoryUsage.rss = function() {
  return readLiveProcessMemoryUsage().rss;
};
Object.defineProperty(process2, Symbol.toStringTag, {
  value: "process",
  writable: false,
  configurable: true,
  enumerable: false
});
var process_default = process2;
function ttyIsatty(fd) {
  if (fd === 0) {
    return !!process_default.stdin?.isTTY;
  }
  if (fd === 1) {
    return !!process_default.stdout?.isTTY;
  }
  if (fd === 2) {
    return !!process_default.stderr?.isTTY;
  }
  return false;
}
function TtyReadStream(fd) {
  return fd === 0 ? process_default.stdin : void 0;
}
function TtyWriteStream(fd) {
  if (fd === 1) {
    return process_default.stdout;
  }
  if (fd === 2) {
    return process_default.stderr;
  }
  return void 0;
}
var builtinTtyModule = {
  ReadStream: class ReadStream {
    constructor(fd) {
      return TtyReadStream(fd);
    }
  },
  WriteStream: class WriteStream {
    constructor(fd) {
      return TtyWriteStream(fd);
    }
  },
  isatty: ttyIsatty
};
function createPerfHooksOutOfRangeError(name, requirement, value) {
  const error = new RangeError(
    `The value of "${name}" is out of range. It must be ${requirement}. Received ${String(value)}`
  );
  error.code = "ERR_OUT_OF_RANGE";
  return error;
}
function normalizePerformanceEntry(entry) {
  return {
    name: String(entry?.name ?? ""),
    entryType: String(entry?.entryType ?? ""),
    startTime: Number(entry?.startTime ?? 0),
    duration: Number(entry?.duration ?? 0)
  };
}
function createPerformanceObserverEntryList(entries) {
  return {
    getEntries() {
      return entries.slice();
    },
    getEntriesByName(name, type = void 0) {
      const normalizedName = String(name ?? "");
      const matching = entries.filter((entry) => entry.name === normalizedName);
      if (typeof type === "string") {
        return matching.filter((entry) => entry.entryType === type);
      }
      return matching;
    },
    getEntriesByType(type) {
      const normalizedType = String(type ?? "");
      return entries.filter((entry) => entry.entryType === normalizedType);
    }
  };
}
function createPerformanceHistogram() {
  const values = [];
  return {
    percentile(percentile) {
      const normalizedPercentile = Number(percentile);
      if (!Number.isFinite(normalizedPercentile) || normalizedPercentile <= 0 || normalizedPercentile > 100) {
        throw createPerfHooksOutOfRangeError("percentile", "> 0 && <= 100", percentile);
      }
      if (values.length === 0) {
        return 0;
      }
      const sorted = values.slice().sort((left, right) => left - right);
      const index = Math.min(
        sorted.length - 1,
        Math.max(0, Math.ceil(normalizedPercentile / 100 * sorted.length) - 1)
      );
      return sorted[index];
    },
    record(value) {
      const normalizedValue = typeof value === "bigint" ? Number(value) : Number(value);
      if (!Number.isInteger(normalizedValue)) {
        throw createPerfHooksOutOfRangeError("val", "an integer", value);
      }
      if (normalizedValue < 1 || normalizedValue > Number.MAX_SAFE_INTEGER) {
        throw createPerfHooksOutOfRangeError(
          "val",
          `>= 1 && <= ${Number.MAX_SAFE_INTEGER}`,
          value
        );
      }
      values.push(normalizedValue);
    }
  };
}
var builtinPerformance = (() => {
  const marks = /* @__PURE__ */ new Map();
  const measures = [];
  const observers = /* @__PURE__ */ new Set();
  const perf = typeof performance !== "undefined" && performance && typeof performance.now === "function" ? performance : {
    now() {
      return getNowMs();
    },
    timeOrigin: Date.now() - getNowMs()
  };
  if (typeof perf.mark !== "function") {
    perf.mark = function(name) {
      const entry = {
        name: String(name ?? ""),
        entryType: "mark",
        startTime: perf.now(),
        duration: 0
      };
      const entries = marks.get(entry.name) ?? [];
      entries.push(entry);
      marks.set(entry.name, entries);
      return entry;
    };
  }
  if (typeof perf.measure !== "function") {
    perf.measure = function(name, startMarkOrOptions, endMark) {
      const normalizedName = String(name ?? "");
      let startTime = 0;
      let endTimeMs = perf.now();
      if (typeof startMarkOrOptions === "string") {
        const startEntries = marks.get(startMarkOrOptions);
        if (startEntries?.length) {
          startTime = startEntries[startEntries.length - 1].startTime;
        }
        if (typeof endMark === "string") {
          const endEntries = marks.get(endMark);
          if (endEntries?.length) {
            endTimeMs = endEntries[endEntries.length - 1].startTime;
          }
        }
      } else if (startMarkOrOptions && typeof startMarkOrOptions === "object") {
        if (typeof startMarkOrOptions.start === "number") {
          startTime = startMarkOrOptions.start;
        } else if (typeof startMarkOrOptions.startMark === "string") {
          const startEntries = marks.get(startMarkOrOptions.startMark);
          if (startEntries?.length) {
            startTime = startEntries[startEntries.length - 1].startTime;
          }
        }
        if (typeof startMarkOrOptions.end === "number") {
          endTimeMs = startMarkOrOptions.end;
        } else if (typeof startMarkOrOptions.endMark === "string") {
          const endEntries = marks.get(startMarkOrOptions.endMark);
          if (endEntries?.length) {
            endTimeMs = endEntries[endEntries.length - 1].startTime;
          }
        }
      }
      const entry = {
        name: normalizedName,
        entryType: "measure",
        startTime,
        duration: Math.max(0, endTimeMs - startTime)
      };
      measures.push(entry);
      return entry;
    };
  }
  if (typeof perf.getEntriesByName !== "function") {
    perf.getEntriesByName = function(name, type = void 0) {
      const normalizedName = String(name ?? "");
      const markEntries = marks.get(normalizedName) ?? [];
      const combined = [...markEntries, ...measures.filter((entry) => entry.name === normalizedName)];
      if (typeof type === "string") {
        return combined.filter((entry) => entry.entryType === type);
      }
      return combined;
    };
  }
  if (typeof perf.getEntries !== "function") {
    perf.getEntries = function() {
      return [...marks.values()].flatMap((entries) => [...entries]).concat(measures);
    };
  }
  if (typeof perf.getEntriesByType !== "function") {
    perf.getEntriesByType = function(type) {
      const normalizedType = String(type ?? "");
      return perf.getEntries().filter((entry) => entry.entryType === normalizedType);
    };
  }
  if (typeof perf.clearMarks !== "function") {
    perf.clearMarks = function(name = void 0) {
      if (typeof name === "undefined") {
        marks.clear();
        return;
      }
      marks.delete(String(name));
    };
  }
  if (typeof perf.clearMeasures !== "function") {
    perf.clearMeasures = function(name = void 0) {
      if (typeof name === "undefined") {
        measures.length = 0;
        return;
      }
      const normalizedName = String(name);
      for (let index = measures.length - 1; index >= 0; index -= 1) {
        if (measures[index]?.name === normalizedName) {
          measures.splice(index, 1);
        }
      }
    };
  }
  const queueObserverDelivery = (observer) => {
    if (observer._deliveryQueued) {
      return;
    }
    observer._deliveryQueued = true;
    _queueMicrotask(() => {
      observer._deliveryQueued = false;
      if (!observer._connected) {
        return;
      }
      const records = observer.takeRecords();
      observer._callback(createPerformanceObserverEntryList(records), observer);
    });
  };
  const emitEntry = (entry) => {
    const normalizedEntry = normalizePerformanceEntry(entry);
    for (const observer of observers) {
      if (!observer._entryTypes.has(normalizedEntry.entryType)) {
        continue;
      }
      observer._records.push(normalizedEntry);
      queueObserverDelivery(observer);
    }
  };
  const originalMark = perf.mark.bind(perf);
  perf.mark = function(...args) {
    const entry = originalMark(...args);
    emitEntry(entry);
    return entry;
  };
  const originalMeasure = perf.measure.bind(perf);
  perf.measure = function(...args) {
    const entry = originalMeasure(...args);
    emitEntry(entry);
    return entry;
  };
  perf.__agentOSObservers = observers;
  return perf;
})();
async function collectReadableChunks(input) {
  const readable = getNodeReadableAsyncIterable(input);
  if (readable) {
    const chunks = [];
    for await (const chunk of readable) {
      chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk ?? []));
    }
    return chunks;
  }
  if (input && typeof input[Symbol.asyncIterator] === "function") {
    const chunks = [];
    for await (const chunk of input) {
      chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk ?? []));
    }
    return chunks;
  }
  if (input && typeof input.getReader === "function") {
    const reader = input.getReader();
    const chunks = [];
    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        chunks.push(Buffer.from(value ?? []));
      }
    } finally {
      reader.releaseLock?.();
    }
    return chunks;
  }
  throw new TypeError("expected an async iterable or WHATWG ReadableStream");
}
function createBuiltinBlob(buffer, type = "") {
  return {
    size: buffer.byteLength,
    type,
    async arrayBuffer() {
      return buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength);
    },
    stream() {
      return new ReadableStream({
        start(controller) {
          controller.enqueue(buffer);
          controller.close();
        }
      });
    },
    async text() {
      return buffer.toString("utf8");
    }
  };
}
var builtinStreamConsumersModule = {
  async arrayBuffer(stream) {
    const chunks = await collectReadableChunks(stream);
    const buffer = Buffer.concat(chunks);
    return buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength);
  },
  async blob(stream) {
    return createBuiltinBlob(await builtinStreamConsumersModule.buffer(stream));
  },
  async buffer(stream) {
    return Buffer.concat(await collectReadableChunks(stream));
  },
  async json(stream) {
    return JSON.parse(await builtinStreamConsumersModule.text(stream));
  },
  async text(stream) {
    return (await builtinStreamConsumersModule.buffer(stream)).toString("utf8");
  }
};
function getNodeReadableAsyncIterable(stream) {
  if (
    !stream ||
    typeof stream.on !== "function" ||
    (typeof stream.read !== "function" &&
      typeof stream.pipe !== "function" &&
      typeof stream.resume !== "function")
  ) {
    return null;
  }
  return {
    async *[Symbol.asyncIterator]() {
      const queuedChunks = [];
      const pendingResolves = [];
      let done = false;
      let error = null;
      const cleanup = [];
      const removeListener =
        typeof stream.off === "function"
          ? stream.off.bind(stream)
          : typeof stream.removeListener === "function"
            ? stream.removeListener.bind(stream)
            : null;
      const flush = () => {
        while (pendingResolves.length > 0) {
          if (error) {
            pendingResolves.shift()?.(Promise.reject(error));
            continue;
          }
          if (queuedChunks.length > 0) {
            pendingResolves.shift()?.(
              Promise.resolve({ done: false, value: queuedChunks.shift() })
            );
            continue;
          }
          if (done) {
            pendingResolves.shift()?.(Promise.resolve({ done: true, value: void 0 }));
            continue;
          }
          break;
        }
      };
      const add = (eventName, handler) => {
        stream.on(eventName, handler);
        cleanup.push(() => removeListener?.(eventName, handler));
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
      add("data", onData);
      add("end", onEnd);
      add("close", onEnd);
      add("error", onError);
      stream.resume?.();
      try {
        while (true) {
          if (error) {
            throw error;
          }
          if (queuedChunks.length > 0) {
            yield queuedChunks.shift();
            continue;
          }
          if (done) {
            return;
          }
          const result = await new Promise((resolve) => {
            pendingResolves.push(resolve);
          });
          if (result.done) {
            return;
          }
          yield result.value;
        }
      } finally {
        while (cleanup.length > 0) {
          cleanup.pop()?.();
        }
      }
    }
  };
}
var builtinStreamPromisesModule = {
  finished(stream) {
    return new Promise((resolve, reject) => {
      if (!stream || typeof stream !== "object") {
        reject(new TypeError("finished() expects a stream"));
        return;
      }
      const cleanup = [];
      const add = (eventName, handler) => {
        stream?.once?.(eventName, handler);
        cleanup.push(() => stream?.off?.(eventName, handler));
      };
      const settle = (callback) => (value) => {
        while (cleanup.length > 0) {
          cleanup.pop()?.();
        }
        callback(value);
      };
      add("finish", settle(resolve));
      add("end", settle(resolve));
      add("close", settle(resolve));
      add("error", settle(reject));
    });
  },
  async pipeline(source, destination) {
    const readable =
      getNodeReadableAsyncIterable(source) ??
      (source && typeof source[Symbol.asyncIterator] === "function"
        ? source
        : source && typeof source.getReader === "function"
          ? {
              async *[Symbol.asyncIterator]() {
                const reader = source.getReader();
                try {
                  while (true) {
                    const { value, done } = await reader.read();
                    if (done) break;
                    yield Buffer.from(value ?? []);
                  }
                } finally {
                  reader.releaseLock?.();
                }
              }
            }
          : null);
    if (readable == null) {
      throw new TypeError("pipeline source must be async iterable or a WHATWG ReadableStream");
    }
    if (!destination || typeof destination.write !== "function") {
      throw new TypeError("pipeline destination must provide write()");
    }
    for await (const chunk of readable) {
      await new Promise((resolve, reject) => {
        try {
          destination.write(chunk, (error) => error ? reject(error) : resolve());
        } catch (error) {
          reject(error);
        }
      });
    }
    const completion = builtinStreamPromisesModule.finished(destination);
    if (typeof destination.end === "function") {
      await new Promise((resolve, reject) => {
        try {
          destination.end((error) => error ? reject(error) : resolve());
        } catch (error) {
          reject(error);
        }
      });
    }
    await completion;
    return destination;
  }
};
var builtinTimersPromisesModule = {
  scheduler: {
    wait(delay = 0, options = void 0) {
      return builtinTimersPromisesModule.setTimeout(delay, void 0, options);
    },
    yield() {
      return builtinTimersPromisesModule.setImmediate();
    }
  },
  setImmediate(value = void 0, options = void 0) {
    if (options?.signal?.aborted) {
      return Promise.reject(options.signal.reason ?? new Error("The operation was aborted"));
    }
    return new Promise((resolve, reject) => {
      const onAbort = () => reject(options.signal.reason ?? new Error("The operation was aborted"));
      options?.signal?.addEventListener?.("abort", onAbort, { once: true });
      globalThis.setImmediate?.(() => {
        options?.signal?.removeEventListener?.("abort", onAbort);
        resolve(value);
      }) ?? globalThis.setTimeout?.(() => {
        options?.signal?.removeEventListener?.("abort", onAbort);
        resolve(value);
      }, 0);
    });
  },
  setInterval(delay = 1, value = void 0, options = void 0) {
    let active = true;
    const signal = options?.signal;
    if (signal?.aborted) {
      active = false;
    }
    return {
      [Symbol.asyncIterator]() {
        return this;
      },
      async next() {
        if (!active) {
          return { done: true, value: void 0 };
        }
        try {
          const intervalValue = await builtinTimersPromisesModule.setTimeout(delay, value, { signal });
          return active ? { done: false, value: intervalValue } : { done: true, value: void 0 };
        } catch (error) {
          active = false;
          throw error;
        }
      },
      async return() {
        active = false;
        return { done: true, value: void 0 };
      }
    };
  },
  setTimeout(delay = 1, value = void 0, options = void 0) {
    if (options?.signal?.aborted) {
      return Promise.reject(options.signal.reason ?? new Error("The operation was aborted"));
    }
    return new Promise((resolve, reject) => {
      const timer = globalThis.setTimeout?.(() => {
        options?.signal?.removeEventListener?.("abort", onAbort);
        resolve(value);
      }, delay ?? 0);
      const onAbort = () => {
        if (typeof globalThis.clearTimeout === "function") {
          globalThis.clearTimeout(timer);
        }
        reject(options.signal.reason ?? new Error("The operation was aborted"));
      };
      options?.signal?.addEventListener?.("abort", onAbort, { once: true });
    });
  }
};
var builtinPerfHooksModule = {
  PerformanceObserver: class {
    constructor(callback) {
      if (typeof callback !== "function") {
        throw new TypeError("PerformanceObserver callback must be a function");
      }
      this._callback = callback;
      this._connected = false;
      this._deliveryQueued = false;
      this._entryTypes = /* @__PURE__ */ new Set();
      this._records = [];
    }
    static get supportedEntryTypes() {
      return ["mark", "measure"];
    }
    observe(options = {}) {
      const entryTypes = Array.isArray(options?.entryTypes) ? options.entryTypes.map((entryType) => String(entryType)) : typeof options?.type === "string" ? [String(options.type)] : [];
      if (entryTypes.length === 0) {
        throw new TypeError("PerformanceObserver.observe() requires an entryTypes array or type string");
      }
      this.disconnect();
      this._entryTypes = new Set(entryTypes);
      this._connected = true;
      builtinPerformance.__agentOSObservers.add(this);
      if (options?.buffered) {
        for (const entryType of this._entryTypes) {
          for (const entry of builtinPerformance.getEntriesByType(entryType)) {
            this._records.push(normalizePerformanceEntry(entry));
          }
        }
        if (this._records.length > 0) {
          this._deliveryQueued = true;
          _queueMicrotask(() => {
            this._deliveryQueued = false;
            if (!this._connected) {
              return;
            }
            const records = this.takeRecords();
            this._callback(createPerformanceObserverEntryList(records), this);
          });
        }
      }
    }
    disconnect() {
      this._connected = false;
      builtinPerformance.__agentOSObservers.delete(this);
    }
    takeRecords() {
      const records = this._records.slice();
      this._records.length = 0;
      return records;
    }
  },
  constants: {},
  createHistogram() {
    return createPerformanceHistogram();
  },
  performance: builtinPerformance
};
function createAccessDeniedBuiltinError(request) {
  const normalized = String(request).replace(/^node:/, "");
  const error = new Error(`node:${normalized} is not available in the secure-exec guest runtime`);
  error.code = "ERR_ACCESS_DENIED";
  return error;
}
class DiagnosticsChannel {
  constructor(name = "") {
    this.name = String(name);
    this._subscribers = /* @__PURE__ */ new Set();
  }
  get hasSubscribers() {
    return this._subscribers.size > 0;
  }
  publish(message) {
    for (const subscriber of Array.from(this._subscribers)) {
      subscriber(message, this.name);
    }
  }
  subscribe(subscriber) {
    if (typeof subscriber === "function") {
      this._subscribers.add(subscriber);
    }
  }
  unsubscribe(subscriber) {
    return this._subscribers.delete(subscriber);
  }
  runStores(context, callback, thisArg, ...args) {
    if (typeof callback !== "function") {
      return callback;
    }
    return callback.apply(thisArg, args);
  }
}
var diagnosticsChannelCache = /* @__PURE__ */ new Map();
function getDiagnosticsChannel(name = "") {
  const channelName = String(name);
  let existing = diagnosticsChannelCache.get(channelName);
  if (!existing) {
    existing = new DiagnosticsChannel(channelName);
    diagnosticsChannelCache.set(channelName, existing);
  }
  return existing;
}
function createDiagnosticsTracingChannel(name = "") {
  const channelName = String(name);
  const tracing = {
    start: getDiagnosticsChannel(`tracing:${channelName}:start`),
    end: getDiagnosticsChannel(`tracing:${channelName}:end`),
    asyncStart: getDiagnosticsChannel(`tracing:${channelName}:asyncStart`),
    asyncEnd: getDiagnosticsChannel(`tracing:${channelName}:asyncEnd`),
    error: getDiagnosticsChannel(`tracing:${channelName}:error`),
    subscribe() {
    },
    unsubscribe() {
      return true;
    },
    traceSync(fn, context, thisArg, ...args) {
      if (typeof fn !== "function") {
        return fn;
      }
      return fn.apply(thisArg, args);
    },
    tracePromise(fn, context, thisArg, ...args) {
      if (typeof fn !== "function") {
        return Promise.resolve(fn);
      }
      return Promise.resolve(fn.apply(thisArg, args));
    },
    traceCallback(fn, position, context, thisArg, ...args) {
      if (typeof fn !== "function") {
        return fn;
      }
      return fn.apply(thisArg, args);
    }
  };
  Object.defineProperty(tracing, "hasSubscribers", {
    get() {
      return tracing.start.hasSubscribers || tracing.end.hasSubscribers || tracing.asyncStart.hasSubscribers || tracing.asyncEnd.hasSubscribers || tracing.error.hasSubscribers;
    },
    enumerable: false,
    configurable: true
  });
  return tracing;
}
var builtinDiagnosticsChannelModule = {
  Channel: DiagnosticsChannel,
  channel: getDiagnosticsChannel,
  hasSubscribers(name = "") {
    return getDiagnosticsChannel(name).hasSubscribers;
  },
  subscribe(name = "", subscriber) {
    return getDiagnosticsChannel(name).subscribe(subscriber);
  },
  tracingChannel: createDiagnosticsTracingChannel,
  unsubscribe(name = "", subscriber) {
    return getDiagnosticsChannel(name).unsubscribe(subscriber);
  }
};
var asyncLocalStorageInstances = /* @__PURE__ */ new Set();
function snapshotAsyncLocalStorageStores() {
  return Array.from(asyncLocalStorageInstances, (storage) => ({
    storage,
    hasStore: storage._hasStore === true,
    store: storage._store
  }));
}
function applyAsyncLocalStorageSnapshot(snapshot) {
  for (const entry of snapshot) {
    entry.storage._hasStore = entry.hasStore;
    entry.storage._store = entry.store;
  }
}
function runWithAsyncLocalStorageSnapshot(snapshot, callback, thisArg, args) {
  if (typeof callback !== "function") {
    return callback;
  }
  const previous = snapshotAsyncLocalStorageStores();
  applyAsyncLocalStorageSnapshot(snapshot);
  try {
    return callback.apply(thisArg, args);
  } finally {
    applyAsyncLocalStorageSnapshot(previous);
  }
}
function wrapAsyncLocalStorageCallback(callback, snapshot) {
  if (typeof callback !== "function") {
    return callback;
  }
  return function(...args) {
    try {
      return runWithAsyncLocalStorageSnapshot(snapshot, callback, this, args);
    } catch (error) {
      if (isProcessExitError(error)) {
        throw error;
      }
      throw error;
    }
  };
}
var builtinAsyncHooksModule = {
  AsyncLocalStorage: class {
    constructor() {
      this._hasStore = false;
      this._store = void 0;
      asyncLocalStorageInstances.add(this);
    }
    disable() {
      this._hasStore = false;
      this._store = void 0;
    }
    enterWith(store) {
      this._hasStore = true;
      this._store = store;
    }
    exit(callback, ...args) {
      const previous = {
        hasStore: this._hasStore,
        store: this._store
      };
      this._hasStore = false;
      this._store = void 0;
      let restoreOnFinally = true;
      try {
        const result = callback(...args);
        if (result && typeof result.then === "function") {
          restoreOnFinally = false;
          return Promise.resolve(result).finally(() => {
            this._hasStore = previous.hasStore;
            this._store = previous.store;
          });
        }
        return result;
      } finally {
        if (restoreOnFinally) {
          this._hasStore = previous.hasStore;
          this._store = previous.store;
        }
      }
    }
    getStore() {
      return this._hasStore ? this._store : void 0;
    }
    run(store, callback, ...args) {
      const previous = {
        hasStore: this._hasStore,
        store: this._store
      };
      this._hasStore = true;
      this._store = store;
      let restoreOnFinally = true;
      try {
        const result = callback(...args);
        if (result && typeof result.then === "function") {
          restoreOnFinally = false;
          return Promise.resolve(result).finally(() => {
            this._hasStore = previous.hasStore;
            this._store = previous.store;
          });
        }
        return result;
      } finally {
        if (restoreOnFinally) {
          this._hasStore = previous.hasStore;
          this._store = previous.store;
        }
      }
    }
  },
  AsyncResource: class {
    constructor(type = "SecureExecAsyncResource") {
      this.type = type;
      this._asyncLocalStorageSnapshot = snapshotAsyncLocalStorageStores();
    }
    emitBefore() {
    }
    emitAfter() {
    }
    emitDestroy() {
    }
    asyncId() {
      return 0;
    }
    triggerAsyncId() {
      return 0;
    }
    runInAsyncScope(callback, thisArg, ...args) {
      return runWithAsyncLocalStorageSnapshot(
        this._asyncLocalStorageSnapshot,
        callback,
        thisArg,
        args
      );
    }
  },
  createHook() {
    return {
      enable() {
        return this;
      },
      disable() {
        return this;
      }
    };
  },
  executionAsyncId() {
    return 0;
  },
  triggerAsyncId() {
    return 0;
  }
};
if (!Promise.prototype.__agentOSAsyncLocalStoragePatched) {
  const nativePromiseThen = Promise.prototype.then;
  Promise.prototype.then = function(onFulfilled, onRejected) {
    const snapshot = snapshotAsyncLocalStorageStores();
    const wrappedRejected = typeof onRejected === "function" ? (error) => {
      if (isProcessExitError(error)) {
        throw error;
      }
      return onRejected(error);
    } : onRejected;
    return nativePromiseThen.call(
      this,
      wrapAsyncLocalStorageCallback(onFulfilled, snapshot),
      wrapAsyncLocalStorageCallback(wrappedRejected, snapshot)
    );
  };
  Object.defineProperty(Promise.prototype, "__agentOSAsyncLocalStoragePatched", {
    value: true,
    configurable: true
  });
}
var TIMER_DISPATCH = {
  create: "kernelTimerCreate",
  arm: "kernelTimerArm",
  clear: "kernelTimerClear"
};
exposeCustomGlobal("_asyncHooksModule", builtinAsyncHooksModule);
var _queueMicrotask = typeof queueMicrotask === "function" ? queueMicrotask : function(fn) {
  Promise.resolve().then(fn);
};
function normalizeTimerDelay(delay) {
  const numericDelay = Number(delay ?? 0);
  if (!Number.isFinite(numericDelay) || numericDelay <= 0) {
    return 0;
  }
  return Math.floor(numericDelay);
}
function getTimerId(timer) {
  if (timer && typeof timer === "object" && timer._id !== void 0) {
    return timer._id;
  }
  if (typeof timer === "number") {
    return timer;
  }
  return void 0;
}
function createKernelTimer(delayMs, repeat) {
  try {
    return bridgeDispatchSync(TIMER_DISPATCH.create, delayMs, repeat);
  } catch (error) {
    if (error instanceof Error && error.message.includes("EAGAIN")) {
      throw new Error(
        "ERR_RESOURCE_BUDGET_EXCEEDED: maximum number of timers exceeded"
      );
    }
    throw error;
  }
}
function armKernelTimer(timerId) {
  bridgeDispatchSync(TIMER_DISPATCH.arm, timerId);
}
var TimerHandle = class {
  _id;
  _destroyed;
  _refed;
  constructor(id) {
    this._id = id;
    this._destroyed = false;
    this._refed = true;
  }
  ref() {
    this._refed = true;
    return this;
  }
  unref() {
    this._refed = false;
    return this;
  }
  hasRef() {
    return this._refed;
  }
  refresh() {
    if (!this._destroyed && _timerEntries.has(this._id)) {
      armKernelTimer(this._id);
    }
    return this;
  }
  [Symbol.toPrimitive]() {
    return this._id;
  }
};
var _timerEntries = /* @__PURE__ */ new Map();
var _timerDrainResolvers = [];
function getRefedTimerCount() {
  if (typeof _exited !== "undefined" && _exited) {
    return 0;
  }
  let count = 0;
  for (const entry of _timerEntries.values()) {
    if (entry.handle?.hasRef?.() !== false) {
      count += 1;
    }
  }
  return count;
}
function checkTimerDrain() {
  if (getRefedTimerCount() === 0 && _timerDrainResolvers.length > 0) {
    const resolvers = _timerDrainResolvers;
    _timerDrainResolvers = [];
    resolvers.forEach((r) => r());
  }
}
function _getPendingTimerCount() {
  return getRefedTimerCount();
}
function _waitForTimerDrain() {
  if (getRefedTimerCount() === 0) return Promise.resolve();
  return new Promise((resolve) => {
    _timerDrainResolvers.push(resolve);
    checkTimerDrain();
  });
}
var _nextTickQueue = [];
var _nextTickScheduled = false;
function flushNextTickQueue() {
  _nextTickScheduled = false;
  while (_nextTickQueue.length > 0) {
    const entry = _nextTickQueue.shift();
    if (!entry) {
      break;
    }
    try {
      entry.callback(...entry.args);
    } catch (error) {
      const outcome = routeAsyncCallbackError(error);
      if (!outcome.handled && outcome.rethrow !== null) {
        _nextTickQueue.length = 0;
        scheduleAsyncRethrow(outcome.rethrow);
      }
      return;
    }
  }
}
function scheduleNextTickFlush() {
  if (_nextTickScheduled) {
    return;
  }
  _nextTickScheduled = true;
  const asyncLocalStorageSnapshot = snapshotAsyncLocalStorageStores();
  _queueMicrotask(() =>
    runWithAsyncLocalStorageSnapshot(
      asyncLocalStorageSnapshot,
      flushNextTickQueue,
      globalThis,
      []
    )
  );
}
function timerDispatch(_eventType, payload) {
  const timerId = typeof payload === "number" ? payload : Number(payload?.timerId);
  if (!Number.isFinite(timerId)) return;
  const entry = _timerEntries.get(timerId);
  if (!entry) return;
  if (!entry.repeat) {
    entry.handle._destroyed = true;
    _timerEntries.delete(timerId);
  }
  try {
    entry.callback(...entry.args);
  } catch (error) {
    const outcome = routeAsyncCallbackError(error);
    if (!outcome.handled && outcome.rethrow !== null) {
      throw outcome.rethrow;
    }
    return;
  }
  if (typeof _exited !== "undefined" && _exited) {
    checkTimerDrain();
    return;
  }
  if (entry.repeat && _timerEntries.has(timerId)) {
    armKernelTimer(timerId);
  }
  checkTimerDrain();
}
function setTimeout2(callback, delay, ...args) {
  const actualDelay = Math.max(1, normalizeTimerDelay(delay));
  const id = createKernelTimer(actualDelay, false);
  const handle = new TimerHandle(id);
  const asyncLocalStorageSnapshot = snapshotAsyncLocalStorageStores();
  _timerEntries.set(id, {
    handle,
    callback: wrapAsyncLocalStorageCallback(callback, asyncLocalStorageSnapshot),
    args,
    repeat: false
  });
  armKernelTimer(id);
  return handle;
}
function clearTimeout2(timer) {
  const id = getTimerId(timer);
  if (id === void 0) return;
  const entry = _timerEntries.get(id);
  if (entry) {
    entry.handle._destroyed = true;
    _timerEntries.delete(id);
  }
  bridgeDispatchSync(TIMER_DISPATCH.clear, id);
  checkTimerDrain();
}
function setInterval(callback, delay, ...args) {
  const actualDelay = Math.max(1, normalizeTimerDelay(delay));
  const id = createKernelTimer(actualDelay, true);
  const handle = new TimerHandle(id);
  const asyncLocalStorageSnapshot = snapshotAsyncLocalStorageStores();
  _timerEntries.set(id, {
    handle,
    callback: wrapAsyncLocalStorageCallback(callback, asyncLocalStorageSnapshot),
    args,
    repeat: true
  });
  armKernelTimer(id);
  return handle;
}
function clearInterval(timer) {
  clearTimeout2(timer);
}
exposeCustomGlobal("_timerDispatch", timerDispatch);
exposeCustomGlobal("_getPendingTimerCount", _getPendingTimerCount);
exposeCustomGlobal("_waitForTimerDrain", _waitForTimerDrain);
function setImmediate(callback, ...args) {
  const id = createKernelTimer(0, false);
  const handle = new TimerHandle(id);
  const asyncLocalStorageSnapshot = snapshotAsyncLocalStorageStores();
  _timerEntries.set(id, {
    handle,
    callback: wrapAsyncLocalStorageCallback(callback, asyncLocalStorageSnapshot),
    args,
    repeat: false
  });
  armKernelTimer(id);
  return handle;
}
function clearImmediate(id) {
  clearTimeout2(id);
}
var Buffer3 = import_buffer2.Buffer;
function createUnsupportedCryptoApiError(subject) {
  const error = new Error(`${subject} is not supported in sandbox`);
  error.code = "ERR_NOT_IMPLEMENTED";
  return error;
}
function throwUnsupportedCryptoApi(api) {
  throw createUnsupportedCryptoApiError(`crypto.${api}`);
}
var kCryptoKeyToken = /* @__PURE__ */ Symbol("secureExecCryptoKey");
var kCryptoToken = /* @__PURE__ */ Symbol("secureExecCrypto");
var kSubtleToken = /* @__PURE__ */ Symbol("secureExecSubtle");
var ERR_INVALID_THIS2 = "ERR_INVALID_THIS";
var ERR_ILLEGAL_CONSTRUCTOR = "ERR_ILLEGAL_CONSTRUCTOR";
function createNodeTypeError2(message, code) {
  const error = new TypeError(message);
  error.code = code;
  return error;
}
class SandboxDOMException extends Error {
  constructor(message = "", name = "Error") {
    super(message);
    this.name = String(name);
    this.code = 0;
  }
}
function createDomLikeError(name, code, message) {
  const error = new Error(message);
  error.name = name;
  error.code = code;
  return error;
}
function assertCryptoReceiver(receiver) {
  if (!(receiver instanceof SandboxCrypto) || receiver._token !== kCryptoToken) {
    throw createNodeTypeError2('Value of "this" must be of type Crypto', ERR_INVALID_THIS2);
  }
}
function assertSubtleReceiver(receiver) {
  if (!(receiver instanceof SandboxSubtleCrypto) || receiver._token !== kSubtleToken) {
    throw createNodeTypeError2('Value of "this" must be of type SubtleCrypto', ERR_INVALID_THIS2);
  }
}
function isIntegerTypedArray(value) {
  if (!ArrayBuffer.isView(value) || value instanceof DataView) {
    return false;
  }
  return value instanceof Int8Array || value instanceof Int16Array || value instanceof Int32Array || value instanceof Uint8Array || value instanceof Uint16Array || value instanceof Uint32Array || value instanceof Uint8ClampedArray || value instanceof BigInt64Array || value instanceof BigUint64Array || import_buffer2.Buffer.isBuffer(value);
}
function toBase64(data) {
  if (typeof data === "string") {
    return import_buffer2.Buffer.from(data).toString("base64");
  }
  if (data instanceof ArrayBuffer) {
    return import_buffer2.Buffer.from(new Uint8Array(data)).toString("base64");
  }
  if (ArrayBuffer.isView(data)) {
    return import_buffer2.Buffer.from(
      new Uint8Array(data.buffer, data.byteOffset, data.byteLength)
    ).toString("base64");
  }
  return import_buffer2.Buffer.from(data).toString("base64");
}
function toArrayBuffer(data) {
  const buf = import_buffer2.Buffer.from(data, "base64");
  return buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength);
}
function normalizeAlgorithm(algorithm) {
  if (typeof algorithm === "string") {
    return { name: algorithm };
  }
  return algorithm ?? {};
}
function normalizeBridgeAlgorithm(algorithm) {
  const normalized = { ...normalizeAlgorithm(algorithm) };
  const hash = normalized.hash;
  const publicExponent = normalized.publicExponent;
  const iv = normalized.iv;
  const additionalData = normalized.additionalData;
  const salt = normalized.salt;
  const info = normalized.info;
  const context = normalized.context;
  const label = normalized.label;
  const publicKey = normalized.public;
  if (hash) {
    normalized.hash = normalizeAlgorithm(hash);
  }
  if (publicExponent && ArrayBuffer.isView(publicExponent)) {
    normalized.publicExponent = import_buffer2.Buffer.from(
      new Uint8Array(
        publicExponent.buffer,
        publicExponent.byteOffset,
        publicExponent.byteLength
      )
    ).toString("base64");
  }
  if (iv) {
    normalized.iv = toBase64(iv);
  }
  if (additionalData) {
    normalized.additionalData = toBase64(additionalData);
  }
  if (salt) {
    normalized.salt = toBase64(salt);
  }
  if (info) {
    normalized.info = toBase64(info);
  }
  if (context) {
    normalized.context = toBase64(context);
  }
  if (label) {
    normalized.label = toBase64(label);
  }
  if (publicKey && typeof publicKey === "object" && "_keyData" in publicKey) {
    normalized.public = publicKey._keyData;
  }
  return normalized;
}
var SandboxCryptoKey = class {
  type;
  extractable;
  algorithm;
  usages;
  _keyData;
  _pem;
  _jwk;
  _raw;
  _sourceKeyObjectData;
  [kCryptoKeyToken];
  constructor(keyData, token) {
    if (token !== kCryptoKeyToken || !keyData) {
      throw createNodeTypeError2("Illegal constructor", ERR_ILLEGAL_CONSTRUCTOR);
    }
    this.type = keyData.type;
    this.extractable = keyData.extractable;
    this.algorithm = keyData.algorithm;
    this.usages = keyData.usages;
    this._keyData = keyData;
    this._pem = keyData._pem;
    this._jwk = keyData._jwk;
    this._raw = keyData._raw;
    this._sourceKeyObjectData = keyData._sourceKeyObjectData;
    this[kCryptoKeyToken] = true;
  }
};
Object.defineProperty(SandboxCryptoKey.prototype, Symbol.toStringTag, {
  value: "CryptoKey",
  configurable: true
});
Object.defineProperty(SandboxCryptoKey, Symbol.hasInstance, {
  value(candidate) {
    return Boolean(
      candidate && typeof candidate === "object" && (candidate[kCryptoKeyToken] === true || "_keyData" in candidate && candidate[Symbol.toStringTag] === "CryptoKey")
    );
  },
  configurable: true
});
function createCryptoKey(keyData) {
  const globalCryptoKey = globalThis.CryptoKey;
  if (typeof globalCryptoKey === "function" && globalCryptoKey.prototype && globalCryptoKey.prototype !== SandboxCryptoKey.prototype) {
    const key = Object.create(globalCryptoKey.prototype);
    key.type = keyData.type;
    key.extractable = keyData.extractable;
    key.algorithm = keyData.algorithm;
    key.usages = keyData.usages;
    key._keyData = keyData;
    key._pem = keyData._pem;
    key._jwk = keyData._jwk;
    key._raw = keyData._raw;
    key._sourceKeyObjectData = keyData._sourceKeyObjectData;
    return key;
  }
  return new SandboxCryptoKey(keyData, kCryptoKeyToken);
}
function subtleCall(request) {
  if (typeof _cryptoSubtle === "undefined") {
    throw new Error("crypto.subtle is not supported in sandbox");
  }
  return _cryptoSubtle.applySync(void 0, [JSON.stringify(request)]);
}
var SandboxSubtleCrypto = class {
  _token;
  constructor(token) {
    if (token !== kSubtleToken) {
      throw createNodeTypeError2("Illegal constructor", ERR_ILLEGAL_CONSTRUCTOR);
    }
    this._token = token;
  }
  digest(algorithm, data) {
    assertSubtleReceiver(this);
    return Promise.resolve().then(() => {
      const result = JSON.parse(
        subtleCall({
          op: "digest",
          algorithm: normalizeAlgorithm(algorithm).name,
          data: toBase64(data)
        })
      );
      return toArrayBuffer(result.data);
    });
  }
  generateKey(algorithm, extractable, keyUsages) {
    assertSubtleReceiver(this);
    return Promise.resolve().then(() => {
      const result = JSON.parse(
        subtleCall({
          op: "generateKey",
          algorithm: normalizeBridgeAlgorithm(algorithm),
          extractable,
          usages: Array.from(keyUsages)
        })
      );
      if ("publicKey" in result && "privateKey" in result) {
        return {
          publicKey: createCryptoKey(result.publicKey),
          privateKey: createCryptoKey(result.privateKey)
        };
      }
      return createCryptoKey(result.key);
    });
  }
  importKey(format, keyData, algorithm, extractable, keyUsages) {
    assertSubtleReceiver(this);
    return Promise.resolve().then(() => {
      const result = JSON.parse(
        subtleCall({
          op: "importKey",
          format,
          keyData: format === "jwk" ? keyData : toBase64(keyData),
          algorithm: normalizeBridgeAlgorithm(algorithm),
          extractable,
          usages: Array.from(keyUsages)
        })
      );
      return createCryptoKey(result.key);
    });
  }
  exportKey(format, key) {
    assertSubtleReceiver(this);
    return Promise.resolve().then(() => {
      const result = JSON.parse(
        subtleCall({
          op: "exportKey",
          format,
          key: key._keyData
        })
      );
      if (format === "jwk") {
        return result.jwk;
      }
      return toArrayBuffer(result.data ?? "");
    });
  }
  encrypt(algorithm, key, data) {
    assertSubtleReceiver(this);
    return Promise.resolve().then(() => {
      const result = JSON.parse(
        subtleCall({
          op: "encrypt",
          algorithm: normalizeBridgeAlgorithm(algorithm),
          key: key._keyData,
          data: toBase64(data)
        })
      );
      return toArrayBuffer(result.data);
    });
  }
  decrypt(algorithm, key, data) {
    assertSubtleReceiver(this);
    return Promise.resolve().then(() => {
      const result = JSON.parse(
        subtleCall({
          op: "decrypt",
          algorithm: normalizeBridgeAlgorithm(algorithm),
          key: key._keyData,
          data: toBase64(data)
        })
      );
      return toArrayBuffer(result.data);
    });
  }
  sign(algorithm, key, data) {
    assertSubtleReceiver(this);
    return Promise.resolve().then(() => {
      const result = JSON.parse(
        subtleCall({
          op: "sign",
          algorithm: normalizeBridgeAlgorithm(algorithm),
          key: key._keyData,
          data: toBase64(data)
        })
      );
      return toArrayBuffer(result.data);
    });
  }
  verify(algorithm, key, signature, data) {
    assertSubtleReceiver(this);
    return Promise.resolve().then(() => {
      const result = JSON.parse(
        subtleCall({
          op: "verify",
          algorithm: normalizeBridgeAlgorithm(algorithm),
          key: key._keyData,
          signature: toBase64(signature),
          data: toBase64(data)
        })
      );
      return result.result;
    });
  }
  deriveBits(algorithm, baseKey, length) {
    assertSubtleReceiver(this);
    return Promise.resolve().then(() => {
      const result = JSON.parse(
        subtleCall({
          op: "deriveBits",
          algorithm: normalizeBridgeAlgorithm(algorithm),
          baseKey: baseKey._keyData,
          length
        })
      );
      return toArrayBuffer(result.data);
    });
  }
  deriveKey(algorithm, baseKey, derivedKeyAlgorithm, extractable, keyUsages) {
    assertSubtleReceiver(this);
    return Promise.resolve().then(() => {
      const result = JSON.parse(
        subtleCall({
          op: "deriveKey",
          algorithm: normalizeBridgeAlgorithm(algorithm),
          baseKey: baseKey._keyData,
          derivedKeyAlgorithm: normalizeBridgeAlgorithm(derivedKeyAlgorithm),
          extractable,
          usages: Array.from(keyUsages)
        })
      );
      return createCryptoKey(result.key);
    });
  }
  wrapKey(format, key, wrappingKey, wrapAlgorithm) {
    assertSubtleReceiver(this);
    return Promise.resolve().then(() => {
      const result = JSON.parse(
        subtleCall({
          op: "wrapKey",
          format,
          key: key._keyData,
          wrappingKey: wrappingKey._keyData,
          wrapAlgorithm: normalizeBridgeAlgorithm(wrapAlgorithm)
        })
      );
      return toArrayBuffer(result.data);
    });
  }
  unwrapKey(format, wrappedKey, unwrappingKey, unwrapAlgorithm, unwrappedKeyAlgorithm, extractable, keyUsages) {
    assertSubtleReceiver(this);
    return Promise.resolve().then(() => {
      const result = JSON.parse(
        subtleCall({
          op: "unwrapKey",
          format,
          wrappedKey: toBase64(wrappedKey),
          unwrappingKey: unwrappingKey._keyData,
          unwrapAlgorithm: normalizeBridgeAlgorithm(unwrapAlgorithm),
          unwrappedKeyAlgorithm: normalizeBridgeAlgorithm(unwrappedKeyAlgorithm),
          extractable,
          usages: Array.from(keyUsages)
        })
      );
      return createCryptoKey(result.key);
    });
  }
};
var subtleCrypto = new SandboxSubtleCrypto(kSubtleToken);
var SandboxCrypto = class {
  _token;
  constructor(token) {
    if (token !== kCryptoToken) {
      throw createNodeTypeError2("Illegal constructor", ERR_ILLEGAL_CONSTRUCTOR);
    }
    this._token = token;
  }
  get subtle() {
    assertCryptoReceiver(this);
    return subtleCrypto;
  }
  getRandomValues(array) {
    assertCryptoReceiver(this);
    if (!isIntegerTypedArray(array)) {
      throw createDomLikeError(
        "TypeMismatchError",
        17,
        "The data argument must be an integer-type TypedArray"
      );
    }
    if (typeof _cryptoRandomFill === "undefined") {
      throwUnsupportedCryptoApi("getRandomValues");
    }
    if (array.byteLength > 65536) {
      throw createDomLikeError(
        "QuotaExceededError",
        22,
        `The ArrayBufferView's byte length (${array.byteLength}) exceeds the number of bytes of entropy available via this API (65536)`
      );
    }
    const bytes = new Uint8Array(array.buffer, array.byteOffset, array.byteLength);
    try {
      const base64 = _cryptoRandomFill.applySync(void 0, [bytes.byteLength]);
      const hostBytes = import_buffer2.Buffer.from(base64, "base64");
      if (hostBytes.byteLength !== bytes.byteLength) {
        throw new Error("invalid host entropy size");
      }
      bytes.set(hostBytes);
      return array;
    } catch {
      throwUnsupportedCryptoApi("getRandomValues");
    }
  }
  randomUUID() {
    assertCryptoReceiver(this);
    if (typeof _cryptoRandomUUID === "undefined") {
      throwUnsupportedCryptoApi("randomUUID");
    }
    try {
      const uuid = _cryptoRandomUUID.applySync(void 0, []);
      if (typeof uuid !== "string") {
        throw new Error("invalid host uuid");
      }
      return uuid;
    } catch {
      throwUnsupportedCryptoApi("randomUUID");
    }
  }
};
var cryptoPolyfillInstance = new SandboxCrypto(kCryptoToken);
var cryptoPolyfill = cryptoPolyfillInstance;
function createBuiltinHash(algorithm) {
  const chunks = [];
  return {
    update(data, encoding) {
      const buffer = typeof data === "string" ? import_buffer2.Buffer.from(data, encoding || "utf8") : import_buffer2.Buffer.from(data);
      chunks.push(buffer);
      return this;
    },
    digest(encoding) {
      if (typeof _cryptoHashDigest === "undefined") {
        throwUnsupportedCryptoApi("createHash");
      }
      const input = chunks.length === 1 ? chunks[0] : import_buffer2.Buffer.concat(chunks);
      const resultBase64 = _cryptoHashDigest.applySync(void 0, [
        String(algorithm),
        input.toString("base64")
      ]);
      const result = import_buffer2.Buffer.from(String(resultBase64 || ""), "base64");
      return encoding ? result.toString(encoding) : result;
    }
  };
}
function toSymmetricKeyBuffer(key) {
  if (isBuiltinKeyObject(key)) {
    if (key.type !== "secret") {
      throw new TypeError("Symmetric crypto operations require a secret KeyObject");
    }
    return import_buffer2.Buffer.from(String(key._serialized.raw || ""), "base64");
  }
  if (key && typeof key === "object" && key[Symbol.toStringTag] === "CryptoKey" && typeof key._raw === "string") {
    return import_buffer2.Buffer.from(String(key._raw || ""), "base64");
  }
  return typeof key === "string" ? import_buffer2.Buffer.from(key) : toCryptoBuffer(key);
}
function createBuiltinHmac(algorithm, key) {
  const chunks = [];
  const keyBuffer = toSymmetricKeyBuffer(key);
  return {
    update(data, encoding) {
      const buffer = typeof data === "string" ? import_buffer2.Buffer.from(data, encoding || "utf8") : import_buffer2.Buffer.from(data);
      chunks.push(buffer);
      return this;
    },
    digest(encoding) {
      if (typeof _cryptoHmacDigest === "undefined") {
        throwUnsupportedCryptoApi("createHmac");
      }
      const input = chunks.length === 1 ? chunks[0] : import_buffer2.Buffer.concat(chunks);
      const resultBase64 = _cryptoHmacDigest.applySync(void 0, [
        String(algorithm),
        keyBuffer.toString("base64"),
        input.toString("base64")
      ]);
      const result = import_buffer2.Buffer.from(String(resultBase64 || ""), "base64");
      return encoding ? result.toString(encoding) : result;
    }
  };
}
var kBuiltinCryptoKeyObjectToken = /* @__PURE__ */ Symbol("secureExecBuiltinKeyObject");
function isBufferLikeValue(value) {
  return import_buffer2.Buffer.isBuffer(value) || value instanceof ArrayBuffer || ArrayBuffer.isView(value);
}
function toCryptoBuffer(value, encoding = void 0) {
  if (import_buffer2.Buffer.isBuffer(value)) {
    return import_buffer2.Buffer.from(value);
  }
  if (typeof value === "string") {
    return import_buffer2.Buffer.from(value, encoding || "utf8");
  }
  if (value instanceof ArrayBuffer) {
    return import_buffer2.Buffer.from(new Uint8Array(value));
  }
  if (ArrayBuffer.isView(value)) {
    return import_buffer2.Buffer.from(new Uint8Array(value.buffer, value.byteOffset, value.byteLength));
  }
  return import_buffer2.Buffer.from(value ?? []);
}
function encodeCryptoResult(buffer, encoding = void 0) {
  return encoding ? buffer.toString(encoding) : buffer;
}
function isBuiltinKeyObject(value) {
  return Boolean(value && typeof value === "object" && (value[kBuiltinCryptoKeyObjectToken] === true || value[Symbol.toStringTag] === "KeyObject" && "_serialized" in value));
}
function serializeBridgeValue(value) {
  if (isBuiltinKeyObject(value)) {
    return value._serialized;
  }
  if (value && typeof value === "object" && value[Symbol.toStringTag] === "CryptoKey" && "_keyData" in value) {
    return value._keyData;
  }
  if (typeof value === "bigint") {
    return {
      __type: "bigint",
      value: value.toString()
    };
  }
  if (isBufferLikeValue(value)) {
    return {
      __type: "buffer",
      value: toCryptoBuffer(value).toString("base64")
    };
  }
  if (Array.isArray(value)) {
    return value.map((entry) => serializeBridgeValue(entry));
  }
  if (value && typeof value === "object") {
    const normalized = {};
    for (const [key, entry] of Object.entries(value)) {
      if (entry !== void 0) {
        normalized[key] = serializeBridgeValue(entry);
      }
    }
    return normalized;
  }
  return value;
}
function deserializeBridgeValue(value) {
  if (Array.isArray(value)) {
    return value.map((entry) => deserializeBridgeValue(entry));
  }
  if (!value || typeof value !== "object") {
    return value;
  }
  if (value.__type === "buffer") {
    return import_buffer2.Buffer.from(String(value.value || ""), "base64");
  }
  if (value.__type === "bigint") {
    return BigInt(String(value.value || "0"));
  }
  if (value.__type === "keyObject") {
    return createBuiltinKeyObject(value.value);
  }
  const normalized = {};
  for (const [key, entry] of Object.entries(value)) {
    normalized[key] = deserializeBridgeValue(entry);
  }
  return normalized;
}
function normalizeDirectCryptoKeyInput(key) {
  if (isBuiltinKeyObject(key)) {
    return key._serialized;
  }
  if (key && typeof key === "object" && key[Symbol.toStringTag] === "CryptoKey" && "_keyData" in key) {
    return key._keyData;
  }
  if (key && typeof key === "object" && !Array.isArray(key) && "key" in key) {
    const { key: keySource, ...rest } = key;
    const normalizedSource = normalizeDirectCryptoKeyInput(keySource);
    if (normalizedSource && typeof normalizedSource === "object" && !Array.isArray(normalizedSource) && ("type" in normalizedSource && ("pem" in normalizedSource || "raw" in normalizedSource))) {
      return {
        ...normalizedSource,
        ...Object.fromEntries(Object.entries(rest).map(([name, value]) => [name, serializeBridgeValue(value)]))
      };
    }
    return {
      ...Object.fromEntries(Object.entries(rest).map(([name, value]) => [name, serializeBridgeValue(value)])),
      key: serializeBridgeValue(keySource)
    };
  }
  return serializeBridgeValue(key);
}
function serializeCryptoKeyInput(key) {
  return JSON.stringify(normalizeDirectCryptoKeyInput(key));
}
function serializeOptionalCryptoOptions(options) {
  return JSON.stringify({
    hasOptions: options !== void 0,
    options: options === void 0 ? null : serializeBridgeValue(options)
  });
}
function normalizeCryptoAlgorithmName(algorithm) {
  if (algorithm == null) {
    return null;
  }
  if (typeof algorithm === "string") {
    return algorithm;
  }
  if (typeof algorithm === "object" && algorithm && typeof algorithm.name === "string") {
    return algorithm.name;
  }
  return String(algorithm);
}
function callCryptoSync(bridge, api, args) {
  if (typeof bridge === "undefined") {
    throwUnsupportedCryptoApi(api);
  }
  return bridge.applySync(void 0, args);
}
function decodeGeneratedCryptoValue(value) {
  if (value && typeof value === "object" && value.kind === "buffer") {
    return import_buffer2.Buffer.from(String(value.value || ""), "base64");
  }
  if (value && typeof value === "object" && value.kind === "string") {
    return String(value.value || "");
  }
  return createBuiltinKeyObject(value);
}
class BuiltinKeyObject {
  type;
  asymmetricKeyType;
  asymmetricKeyDetails;
  symmetricKeySize;
  _serialized;
  [kBuiltinCryptoKeyObjectToken];
  constructor(serialized, token) {
    if (token !== kBuiltinCryptoKeyObjectToken || !serialized || typeof serialized !== "object") {
      throw createNodeTypeError2("Illegal constructor", ERR_ILLEGAL_CONSTRUCTOR);
    }
    this.type = serialized.type;
    this.asymmetricKeyType = serialized.asymmetricKeyType;
    this.asymmetricKeyDetails = serialized.asymmetricKeyDetails;
    this.symmetricKeySize = serialized.raw ? import_buffer2.Buffer.from(String(serialized.raw), "base64").length : void 0;
    this._serialized = serialized;
    this[kBuiltinCryptoKeyObjectToken] = true;
  }
  export(options = void 0) {
    if (this.type === "secret") {
      if (options && options.format === "jwk" && this._serialized.jwk) {
        return { ...this._serialized.jwk };
      }
      return import_buffer2.Buffer.from(String(this._serialized.raw || ""), "base64");
    }
    if (options == null || typeof options !== "object") {
      const error = new TypeError('The "options" argument must be of type object. Received undefined');
      error.code = "ERR_INVALID_ARG_TYPE";
      throw error;
    }
    if (options.format === "jwk" && this._serialized.jwk) {
      return { ...this._serialized.jwk };
    }
    if (options.format && options.format !== "pem") {
      throw createUnsupportedCryptoApiError(`crypto.KeyObject.export(${options.format})`);
    }
    return String(this._serialized.pem || "");
  }
  equals(other) {
    return isBuiltinKeyObject(other) && JSON.stringify(this._serialized) === JSON.stringify(other._serialized);
  }
}
Object.defineProperty(BuiltinKeyObject.prototype, Symbol.toStringTag, {
  value: "KeyObject",
  configurable: true
});
Object.defineProperty(BuiltinKeyObject, Symbol.hasInstance, {
  value(candidate) {
    return isBuiltinKeyObject(candidate);
  },
  configurable: true
});
function createBuiltinKeyObject(serialized) {
  if (isBuiltinKeyObject(serialized)) {
    return serialized;
  }
  return new BuiltinKeyObject(serialized, kBuiltinCryptoKeyObjectToken);
}
function normalizeCipherOptions(options) {
  if (!options || typeof options !== "object") {
    return {};
  }
  const normalized = {};
  if ("aad" in options && options.aad != null) {
    normalized.aad = toCryptoBuffer(options.aad).toString("base64");
  }
  if ("authTag" in options && options.authTag != null) {
    normalized.authTag = toCryptoBuffer(options.authTag).toString("base64");
  }
  if ("authTagLength" in options && options.authTagLength != null) {
    normalized.authTagLength = Number(options.authTagLength);
  }
  if ("autoPadding" in options) {
    normalized.autoPadding = Boolean(options.autoPadding);
  }
  return normalized;
}
class BuiltinCipherTransform {
  _mode;
  _algorithm;
  _key;
  _iv;
  _options;
  _sessionId;
  _authTag;
  constructor(mode, algorithm, key, iv, options = void 0) {
    this._mode = mode;
    this._algorithm = String(algorithm);
    this._key = toSymmetricKeyBuffer(key);
    this._iv = iv == null ? null : toCryptoBuffer(iv);
    this._options = normalizeCipherOptions(options);
    this._sessionId = null;
    this._authTag = null;
  }
  _ensureSession() {
    if (this._sessionId != null) {
      return this._sessionId;
    }
    this._sessionId = Number(callCryptoSync(_cryptoCipherivCreate, this._mode === "cipher" ? "createCipheriv" : "createDecipheriv", [
      this._mode,
      this._algorithm,
      this._key.toString("base64"),
      this._iv ? this._iv.toString("base64") : null,
      Object.keys(this._options).length > 0 ? JSON.stringify(this._options) : null
    ]));
    return this._sessionId;
  }
  _assertMutable(methodName) {
    if (this._sessionId != null) {
      throw createUnsupportedCryptoApiError(`crypto.${methodName} after update()`);
    }
  }
  update(data, inputEncoding = void 0, outputEncoding = void 0) {
    const chunk = toCryptoBuffer(data, inputEncoding);
    const resultBase64 = callCryptoSync(_cryptoCipherivUpdate, this._mode === "cipher" ? "createCipheriv" : "createDecipheriv", [
      this._ensureSession(),
      chunk.toString("base64")
    ]);
    return encodeCryptoResult(import_buffer2.Buffer.from(String(resultBase64 || ""), "base64"), outputEncoding);
  }
  final(outputEncoding = void 0) {
    const response = JSON.parse(String(callCryptoSync(_cryptoCipherivFinal, this._mode === "cipher" ? "createCipheriv" : "createDecipheriv", [this._ensureSession()]) || "{}"));
    if (response.authTag) {
      this._authTag = import_buffer2.Buffer.from(String(response.authTag), "base64");
    }
    return encodeCryptoResult(import_buffer2.Buffer.from(String(response.data || ""), "base64"), outputEncoding);
  }
  setAAD(buffer, options = void 0) {
    this._assertMutable(this._mode === "cipher" ? "createCipheriv" : "createDecipheriv");
    this._options.aad = toCryptoBuffer(buffer).toString("base64");
    if (options && typeof options === "object" && options.authTagLength != null) {
      this._options.authTagLength = Number(options.authTagLength);
    }
    return this;
  }
  setAuthTag(buffer) {
    this._assertMutable("createDecipheriv");
    this._authTag = toCryptoBuffer(buffer);
    this._options.authTag = this._authTag.toString("base64");
    return this;
  }
  getAuthTag() {
    if (!this._authTag) {
      throw new Error("Invalid state for operation getAuthTag");
    }
    return import_buffer2.Buffer.from(this._authTag);
  }
  setAutoPadding(autoPadding = true) {
    this._assertMutable(this._mode === "cipher" ? "createCipheriv" : "createDecipheriv");
    this._options.autoPadding = Boolean(autoPadding);
    return this;
  }
}
class BuiltinSignContext {
  _algorithm;
  _chunks;
  constructor(algorithm) {
    this._algorithm = algorithm;
    this._chunks = [];
  }
  update(data, encoding = void 0) {
    this._chunks.push(toCryptoBuffer(data, encoding));
    return this;
  }
  write(data, encoding = void 0) {
    this.update(data, encoding);
    return true;
  }
  end(data = void 0, encoding = void 0) {
    if (data !== void 0) {
      this.update(data, encoding);
    }
    return this;
  }
  _inputBuffer() {
    if (this._chunks.length === 0) {
      return import_buffer2.Buffer.alloc(0);
    }
    return this._chunks.length === 1 ? this._chunks[0] : import_buffer2.Buffer.concat(this._chunks);
  }
  sign(key, outputEncoding = void 0) {
    const resultBase64 = callCryptoSync(_cryptoSign, "sign", [
      normalizeCryptoAlgorithmName(this._algorithm),
      this._inputBuffer().toString("base64"),
      serializeCryptoKeyInput(key)
    ]);
    return encodeCryptoResult(import_buffer2.Buffer.from(String(resultBase64 || ""), "base64"), outputEncoding);
  }
}
class BuiltinVerifyContext extends BuiltinSignContext {
  verify(key, signature, signatureEncoding = void 0) {
    const signatureBuffer = toCryptoBuffer(signature, signatureEncoding);
    return Boolean(callCryptoSync(_cryptoVerify, "verify", [
      normalizeCryptoAlgorithmName(this._algorithm),
      this._inputBuffer().toString("base64"),
      serializeCryptoKeyInput(key),
      signatureBuffer.toString("base64")
    ]));
  }
}
function normalizeDiffieHellmanArgs(sizeOrKey, keyEncoding = void 0, generator = void 0, generatorEncoding = void 0) {
  const args = [];
  if (typeof sizeOrKey === "string") {
    args.push(toCryptoBuffer(sizeOrKey, typeof keyEncoding === "string" ? keyEncoding : void 0));
    if (generator !== void 0) {
      args.push(typeof generator === "string" ? toCryptoBuffer(generator, typeof generatorEncoding === "string" ? generatorEncoding : void 0) : generator);
    } else if (typeof keyEncoding === "number" || isBufferLikeValue(keyEncoding)) {
      args.push(keyEncoding);
    }
    return args;
  }
  args.push(sizeOrKey);
  if (generator !== void 0) {
    args.push(generator);
  } else if (typeof keyEncoding === "number" || isBufferLikeValue(keyEncoding)) {
    args.push(keyEncoding);
  }
  return args;
}
const diffieHellmanSessionFinalizer = typeof FinalizationRegistry === "function" ? new FinalizationRegistry((sessionId) => {
  try {
    callCryptoSync(_cryptoDiffieHellmanSessionDestroy, "createDiffieHellman", [sessionId]);
  } catch {
  }
}) : null;
class BuiltinDiffieHellmanSession {
  _sessionId;
  constructor(request) {
    this._sessionId = Number(callCryptoSync(_cryptoDiffieHellmanSessionCreate, "createDiffieHellman", [JSON.stringify({
      type: request.type,
      name: request.name,
      args: (request.args || []).map((entry) => serializeBridgeValue(entry))
    })]));
    diffieHellmanSessionFinalizer?.register(this, this._sessionId, this);
  }
  _destroySession() {
    if (this._sessionId == null) {
      return;
    }
    const sessionId = this._sessionId;
    this._sessionId = null;
    diffieHellmanSessionFinalizer?.unregister(this);
    callCryptoSync(_cryptoDiffieHellmanSessionDestroy, "createDiffieHellman", [sessionId]);
  }
  dispose() {
    this._destroySession();
  }
  [Symbol.dispose || Symbol.for("Symbol.dispose")]() {
    this._destroySession();
  }
  _call(method, args = []) {
    if (this._sessionId == null) {
      throw new Error("Diffie-Hellman session has been destroyed");
    }
    const response = JSON.parse(String(callCryptoSync(_cryptoDiffieHellmanSessionCall, "createDiffieHellman", [
      this._sessionId,
      JSON.stringify({
        method,
        args: args.map((entry) => serializeBridgeValue(entry))
      })
    ]) || "{}"));
    return response.hasResult ? deserializeBridgeValue(response.result) : void 0;
  }
  get verifyError() {
    const value = this._call("verifyError");
    return value == null ? 0 : Number(value);
  }
  generateKeys(encoding = void 0) {
    return encodeCryptoResult(toCryptoBuffer(this._call("generateKeys")), encoding);
  }
  computeSecret(otherPublicKey, inputEncoding = void 0, outputEncoding = void 0) {
    const result = this._call("computeSecret", [toCryptoBuffer(otherPublicKey, inputEncoding)]);
    return encodeCryptoResult(toCryptoBuffer(result), outputEncoding);
  }
  getPrime(encoding = void 0) {
    return encodeCryptoResult(toCryptoBuffer(this._call("getPrime")), encoding);
  }
  getGenerator(encoding = void 0) {
    return encodeCryptoResult(toCryptoBuffer(this._call("getGenerator")), encoding);
  }
  getPublicKey(encoding = void 0) {
    return encodeCryptoResult(toCryptoBuffer(this._call("getPublicKey")), encoding);
  }
  getPrivateKey(encoding = void 0) {
    return encodeCryptoResult(toCryptoBuffer(this._call("getPrivateKey")), encoding);
  }
  setPrivateKey(privateKey, encoding = void 0) {
    this._call("setPrivateKey", [toCryptoBuffer(privateKey, encoding)]);
  }
  setPublicKey(publicKey, encoding = void 0) {
    this._call("setPublicKey", [toCryptoBuffer(publicKey, encoding)]);
  }
}
var builtinCryptoModule = {
  KeyObject: BuiltinKeyObject,
  DiffieHellman: BuiltinDiffieHellmanSession,
  ECDH: BuiltinDiffieHellmanSession,
  randomFillSync(buffer, offset = 0, size = void 0) {
    const target = buffer instanceof ArrayBuffer ? new Uint8Array(buffer) : buffer;
    if (!ArrayBuffer.isView(target)) {
      throw new TypeError(
        'The "buffer" argument must be an instance of ArrayBuffer, Buffer, TypedArray, or DataView'
      );
    }
    const start = Number(offset) || 0;
    if (!Number.isInteger(start) || start < 0 || start > target.byteLength) {
      throw new RangeError('The value of "offset" is out of range');
    }
    const length = size === void 0 ? target.byteLength - start : Number(size);
    if (!Number.isInteger(length) || length < 0 || start + length > target.byteLength) {
      throw new RangeError('The value of "size" is out of range');
    }
    const view = new Uint8Array(target.buffer, target.byteOffset + start, length);
    cryptoPolyfill.getRandomValues(view);
    return buffer;
  },
  randomFill(buffer, offset, size, callback) {
    let start = offset;
    let length = size;
    let done = callback;
    if (typeof start === "function") {
      done = start;
      start = 0;
      length = void 0;
    } else if (typeof length === "function") {
      done = length;
      length = void 0;
    }
    if (typeof done !== "function") {
      throw new TypeError('The "callback" argument must be of type function');
    }
    try {
      const result = builtinCryptoModule.randomFillSync(buffer, start, length);
      queueMicrotask(() => done(null, result));
    } catch (error) {
      queueMicrotask(() => done(error));
    }
  },
  createHash(algorithm) {
    return createBuiltinHash(algorithm);
  },
  createHmac(algorithm, key) {
    return createBuiltinHmac(algorithm, key);
  },
  createCipheriv(algorithm, key, iv, options = void 0) {
    return new BuiltinCipherTransform("cipher", algorithm, key, iv, options);
  },
  createDecipheriv(algorithm, key, iv, options = void 0) {
    return new BuiltinCipherTransform("decipher", algorithm, key, iv, options);
  },
  createSign(algorithm) {
    return new BuiltinSignContext(algorithm);
  },
  createVerify(algorithm) {
    return new BuiltinVerifyContext(algorithm);
  },
  sign(algorithm, data, key) {
    const signer = new BuiltinSignContext(algorithm);
    signer.update(data);
    return signer.sign(key);
  },
  verify(algorithm, data, key, signature) {
    const verifier = new BuiltinVerifyContext(algorithm);
    verifier.update(data);
    return verifier.verify(key, signature);
  },
  createPrivateKey(key) {
    const payload = callCryptoSync(_cryptoCreateKeyObject, "createPrivateKey", [
      "createPrivateKey",
      serializeCryptoKeyInput(key)
    ]);
    return createBuiltinKeyObject(JSON.parse(String(payload || "{}")));
  },
  createPublicKey(key) {
    const payload = callCryptoSync(_cryptoCreateKeyObject, "createPublicKey", [
      "createPublicKey",
      serializeCryptoKeyInput(key)
    ]);
    return createBuiltinKeyObject(JSON.parse(String(payload || "{}")));
  },
  createSecretKey(key) {
    return createBuiltinKeyObject({
      type: "secret",
      raw: toCryptoBuffer(key).toString("base64")
    });
  },
  publicEncrypt(key, buffer) {
    const payload = callCryptoSync(_cryptoAsymmetricOp, "publicEncrypt", [
      "publicEncrypt",
      serializeCryptoKeyInput(key),
      toCryptoBuffer(buffer).toString("base64")
    ]);
    return import_buffer2.Buffer.from(String(payload || ""), "base64");
  },
  publicDecrypt(key, buffer) {
    const payload = callCryptoSync(_cryptoAsymmetricOp, "publicDecrypt", [
      "publicDecrypt",
      serializeCryptoKeyInput(key),
      toCryptoBuffer(buffer).toString("base64")
    ]);
    return import_buffer2.Buffer.from(String(payload || ""), "base64");
  },
  privateEncrypt(key, buffer) {
    const payload = callCryptoSync(_cryptoAsymmetricOp, "privateEncrypt", [
      "privateEncrypt",
      serializeCryptoKeyInput(key),
      toCryptoBuffer(buffer).toString("base64")
    ]);
    return import_buffer2.Buffer.from(String(payload || ""), "base64");
  },
  privateDecrypt(key, buffer) {
    const payload = callCryptoSync(_cryptoAsymmetricOp, "privateDecrypt", [
      "privateDecrypt",
      serializeCryptoKeyInput(key),
      toCryptoBuffer(buffer).toString("base64")
    ]);
    return import_buffer2.Buffer.from(String(payload || ""), "base64");
  },
  pbkdf2Sync(password, salt, iterations, keylen, digest) {
    const payload = callCryptoSync(_cryptoPbkdf2, "pbkdf2Sync", [
      toCryptoBuffer(password).toString("base64"),
      toCryptoBuffer(salt).toString("base64"),
      Number(iterations),
      Number(keylen),
      String(digest)
    ]);
    return import_buffer2.Buffer.from(String(payload || ""), "base64");
  },
  pbkdf2(password, salt, iterations, keylen, digest, callback) {
    let algorithm = digest;
    let done = callback;
    if (typeof algorithm === "function") {
      done = algorithm;
      algorithm = "sha1";
    }
    if (typeof done !== "function") {
      throw new TypeError('The "callback" argument must be of type function');
    }
    queueMicrotask(() => {
      try {
        done(null, builtinCryptoModule.pbkdf2Sync(password, salt, iterations, keylen, algorithm));
      } catch (error) {
        done(error);
      }
    });
  },
  scryptSync(password, salt, keylen, options = void 0) {
    const payload = callCryptoSync(_cryptoScrypt, "scryptSync", [
      toCryptoBuffer(password).toString("base64"),
      toCryptoBuffer(salt).toString("base64"),
      Number(keylen),
      JSON.stringify(serializeBridgeValue(options || {}))
    ]);
    return import_buffer2.Buffer.from(String(payload || ""), "base64");
  },
  scrypt(password, salt, keylen, options, callback) {
    let normalizedOptions = options;
    let done = callback;
    if (typeof normalizedOptions === "function") {
      done = normalizedOptions;
      normalizedOptions = void 0;
    }
    if (typeof done !== "function") {
      throw new TypeError('The "callback" argument must be of type function');
    }
    queueMicrotask(() => {
      try {
        done(null, builtinCryptoModule.scryptSync(password, salt, keylen, normalizedOptions));
      } catch (error) {
        done(error);
      }
    });
  },
  generateKeyPairSync(type, options = void 0) {
    const payload = JSON.parse(String(callCryptoSync(_cryptoGenerateKeyPairSync, "generateKeyPairSync", [
      String(type),
      serializeOptionalCryptoOptions(options)
    ]) || "{}"));
    return {
      publicKey: decodeGeneratedCryptoValue(payload.publicKey),
      privateKey: decodeGeneratedCryptoValue(payload.privateKey)
    };
  },
  generateKeyPair(type, options, callback) {
    let normalizedOptions = options;
    let done = callback;
    if (typeof normalizedOptions === "function") {
      done = normalizedOptions;
      normalizedOptions = void 0;
    }
    if (typeof done !== "function") {
      throw new TypeError('The "callback" argument must be of type function');
    }
    queueMicrotask(() => {
      try {
        const result = builtinCryptoModule.generateKeyPairSync(type, normalizedOptions);
        done(null, result.publicKey, result.privateKey);
      } catch (error) {
        done(error);
      }
    });
  },
  generateKeySync(type, options = void 0) {
    const payload = JSON.parse(String(callCryptoSync(_cryptoGenerateKeySync, "generateKeySync", [
      String(type),
      serializeOptionalCryptoOptions(options)
    ]) || "{}"));
    return createBuiltinKeyObject(payload);
  },
  generatePrimeSync(size, options = void 0) {
    const payload = JSON.parse(String(callCryptoSync(_cryptoGeneratePrimeSync, "generatePrimeSync", [
      Number(size),
      serializeOptionalCryptoOptions(options)
    ]) || "null"));
    return deserializeBridgeValue(payload);
  },
  generatePrime(size, options, callback) {
    let normalizedOptions = options;
    let done = callback;
    if (typeof normalizedOptions === "function") {
      done = normalizedOptions;
      normalizedOptions = void 0;
    }
    if (typeof done !== "function") {
      throw new TypeError('The "callback" argument must be of type function');
    }
    queueMicrotask(() => {
      try {
        done(null, builtinCryptoModule.generatePrimeSync(size, normalizedOptions));
      } catch (error) {
        done(error);
      }
    });
  },
  diffieHellman(options) {
    const payload = JSON.parse(String(callCryptoSync(_cryptoDiffieHellman, "diffieHellman", [
      JSON.stringify(serializeBridgeValue(options))
    ]) || "null"));
    return toCryptoBuffer(deserializeBridgeValue(payload));
  },
  getDiffieHellman(name) {
    return new BuiltinDiffieHellmanSession({ type: "group", name: String(name) });
  },
  createDiffieHellman(sizeOrKey, keyEncoding = void 0, generator = void 0, generatorEncoding = void 0) {
    return new BuiltinDiffieHellmanSession({
      type: "dh",
      args: normalizeDiffieHellmanArgs(sizeOrKey, keyEncoding, generator, generatorEncoding)
    });
  },
  createECDH(curve) {
    return new BuiltinDiffieHellmanSession({ type: "ecdh", name: String(curve) });
  },
  getFips() {
    return 0;
  },
  getHashes() {
    return ["md5", "sha1", "sha224", "sha256", "sha384", "sha512"];
  },
  getCiphers() {
    return [
      "aes-128-cbc",
      "aes-128-ctr",
      "aes-128-gcm",
      "aes-192-cbc",
      "aes-192-ctr",
      "aes-192-gcm",
      "aes-256-cbc",
      "aes-256-ctr",
      "aes-256-gcm",
      "aes128",
      "aes192",
      "aes256"
    ];
  },
  getCurves() {
    return ["prime256v1", "secp256k1", "secp384r1", "secp521r1"];
  },
  getRandomValues(array) {
    return cryptoPolyfill.getRandomValues(array);
  },
  randomBytes(size) {
    const length = Math.max(0, Number(size) || 0);
    const bytes = new Uint8Array(length);
    cryptoPolyfill.getRandomValues(bytes);
    return import_buffer2.Buffer.from(bytes);
  },
  randomUUID() {
    return cryptoPolyfill.randomUUID();
  },
  get constants() {
    return builtinConstantsStdlibModule;
  },
  subtle: subtleCrypto,
  webcrypto: cryptoPolyfill
};
function padDateTimeField(value, length = 2) {
  return String(Math.trunc(value)).padStart(length, "0");
}
function coerceIntlDate(value) {
  const date = value instanceof Date ? value : new Date(value ?? Date.now());
  if (Number.isNaN(date.getTime())) {
    throw new RangeError("Invalid time value");
  }
  return date;
}
function formatSafeDateTimeValue(value, options = {}) {
  const date = coerceIntlDate(value);
  const normalizedOptions = options && typeof options === "object" ? options : {};
  const year = padDateTimeField(date.getUTCFullYear(), 4);
  const month = padDateTimeField(date.getUTCMonth() + 1);
  const day = padDateTimeField(date.getUTCDate());
  const hour = padDateTimeField(date.getUTCHours());
  const minute = padDateTimeField(date.getUTCMinutes());
  const second = padDateTimeField(date.getUTCSeconds());
  const datePart = `${year}-${month}-${day}`;
  const timePart = `${hour}:${minute}:${second}`;
  const wantsDate = normalizedOptions.dateStyle || normalizedOptions.year || normalizedOptions.month || normalizedOptions.day || !normalizedOptions.timeStyle && !normalizedOptions.hour && !normalizedOptions.minute && !normalizedOptions.second;
  const wantsTime = normalizedOptions.timeStyle || normalizedOptions.hour || normalizedOptions.minute || normalizedOptions.second;
  if (wantsDate && wantsTime) {
    return `${datePart}, ${timePart}`;
  }
  if (wantsTime) {
    return timePart;
  }
  return datePart;
}
class SafeDateTimeFormat {
  constructor(locales = "en-US", options = {}) {
    this.locales = locales;
    this.options = options && typeof options === "object" ? { ...options } : {};
    this.format = this.format.bind(this);
  }
  format(value = Date.now()) {
    return formatSafeDateTimeValue(value, this.options);
  }
  formatToParts(value = Date.now()) {
    return [{ type: "literal", value: this.format(value) }];
  }
  formatRange(start, end) {
    return `${this.format(start)} – ${this.format(end)}`;
  }
  formatRangeToParts(start, end) {
    return [{ type: "literal", value: this.formatRange(start, end), source: "shared" }];
  }
  resolvedOptions() {
    const locale = Array.isArray(this.locales) ? this.locales.find((entry) => typeof entry === "string") || "en-US" : typeof this.locales === "string" ? this.locales : "en-US";
    return {
      locale,
      calendar: "gregory",
      numberingSystem: "latn",
      timeZone: "UTC",
      ...this.options
    };
  }
  static supportedLocalesOf(locales) {
    if (Array.isArray(locales)) {
      return locales.filter((entry) => typeof entry === "string");
    }
    return typeof locales === "string" ? [locales] : [];
  }
}
function normalizeFractionDigitOption(value, fallback) {
  const number = Number(value);
  if (!Number.isFinite(number)) return fallback;
  return Math.min(20, Math.max(0, Math.trunc(number)));
}
function applySafeNumberGrouping(value) {
  const [integer, fraction] = value.split(".");
  const sign = integer.startsWith("-") ? "-" : "";
  const digits = sign ? integer.slice(1) : integer;
  const grouped = digits.replace(/\B(?=(\d{3})+(?!\d))/g, ",");
  return fraction === void 0 ? `${sign}${grouped}` : `${sign}${grouped}.${fraction}`;
}
class SafeNumberFormat {
  constructor(locales = "en-US", options = {}) {
    this.locales = locales;
    this.options = options && typeof options === "object" ? { ...options } : {};
    this.format = this.format.bind(this);
  }
  format(value) {
    const number = Number(value);
    if (Number.isNaN(number)) return "NaN";
    if (number === Infinity) return "∞";
    if (number === -Infinity) return "-∞";
    const minimumFractionDigits = normalizeFractionDigitOption(this.options.minimumFractionDigits, 0);
    const maximumFractionDigits = Math.max(
      minimumFractionDigits,
      normalizeFractionDigitOption(this.options.maximumFractionDigits, Math.max(minimumFractionDigits, 3))
    );
    let formatted = number.toFixed(maximumFractionDigits);
    if (maximumFractionDigits > minimumFractionDigits) {
      formatted = formatted.replace(/(\.\d*?)0+$/, "$1").replace(/\.$/, "");
      const fractionLength = formatted.includes(".") ? formatted.length - formatted.indexOf(".") - 1 : 0;
      if (fractionLength < minimumFractionDigits) {
        formatted += `${fractionLength === 0 ? "." : ""}${"0".repeat(minimumFractionDigits - fractionLength)}`;
      }
    }
    if (this.options.useGrouping === false) return formatted;
    return applySafeNumberGrouping(formatted);
  }
  formatToParts(value) {
    return [{ type: "literal", value: this.format(value) }];
  }
  resolvedOptions() {
    const locale = Array.isArray(this.locales) ? this.locales.find((entry) => typeof entry === "string") || "en-US" : typeof this.locales === "string" ? this.locales : "en-US";
    return {
      locale,
      numberingSystem: "latn",
      style: "decimal",
      minimumFractionDigits: normalizeFractionDigitOption(this.options.minimumFractionDigits, 0),
      maximumFractionDigits: normalizeFractionDigitOption(this.options.maximumFractionDigits, 3),
      useGrouping: this.options.useGrouping !== false,
      ...this.options
    };
  }
  static supportedLocalesOf(locales) {
    if (Array.isArray(locales)) {
      return locales.filter((entry) => typeof entry === "string");
    }
    return typeof locales === "string" ? [locales] : [];
  }
}
function installSafeIntlFormatters(target) {
  const existingIntl = target.Intl && typeof target.Intl === "object" ? target.Intl : {};
  existingIntl.DateTimeFormat = SafeDateTimeFormat;
  existingIntl.NumberFormat = SafeNumberFormat;
  target.Intl = existingIntl;
  Date.prototype.toLocaleString = function(locales, options) {
    return new target.Intl.DateTimeFormat(locales, options).format(this);
  };
  Date.prototype.toLocaleDateString = function(locales, options) {
    return new target.Intl.DateTimeFormat(locales, { ...(options || {}), hour: void 0, minute: void 0, second: void 0 }).format(this);
  };
  Date.prototype.toLocaleTimeString = function(locales, options) {
    return new target.Intl.DateTimeFormat(locales, {
      hour: "2-digit",
      minute: "2-digit",
      second: "2-digit",
      ...(options || {})
    }).format(this);
  };
  Number.prototype.toLocaleString = function(locales, options) {
    return new target.Intl.NumberFormat(locales, options).format(this.valueOf());
  };
}
function encodeFilePathSegment(value) {
  return encodeURIComponent(String(value)).replace(/%2F/g, "/");
}
function pathToFileURL2(filePath) {
  const normalized = builtinPathStdlibModule.posix.resolve(String(filePath || "/"));
  const pathname = encodeFilePathSegment(normalized);
  return new URL2(`file://${pathname.startsWith("/") ? pathname : `/${pathname}`}`);
}
function fileURLToPath2(input) {
  const href = input instanceof URL2 ? input.href : String(input ?? "");
  if (!href.startsWith("file:")) {
    throw new TypeError("The URL must be of scheme file");
  }
  let pathname = href.slice("file:".length);
  if (pathname.startsWith("//")) {
    const authorityMatch = /^\/\/[^/]*(.*)$/.exec(pathname);
    pathname = authorityMatch?.[1] || "/";
  }
  pathname = pathname.split(/[?#]/, 1)[0] || "/";
  pathname = decodeURIComponent(pathname);
  if (!pathname.startsWith("/")) {
    pathname = `/${pathname}`;
  }
  return pathname;
}
function installBuiltinUtilFormatWithOptions(builtinUtilModule) {
  if (!builtinUtilModule || typeof builtinUtilModule.formatWithOptions === "function") {
    return builtinUtilModule;
  }
  builtinUtilModule.formatWithOptions = function formatWithOptions(inspectOptions, format, ...args) {
    const inspectValue = (value) => {
      if (typeof builtinUtilModule.inspect === "function") {
        return builtinUtilModule.inspect(value, inspectOptions);
      }
      try {
        return JSON.stringify(value);
      } catch {
        return String(value);
      }
    };
    const formatValue = (value) => typeof value === "string" ? value : inspectValue(value);
    if (typeof format !== "string") {
      return [format, ...args].map(formatValue).join(" ");
    }
    let index = 0;
    const formatted = format.replace(/%[sdifjoO%]/g, (token) => {
      if (token === "%%") {
        return "%";
      }
      if (index >= args.length) {
        return token;
      }
      const value = args[index++];
      switch (token) {
        case "%s":
          return String(value);
        case "%d":
          return Number(value).toString();
        case "%i":
          return Number.parseInt(value, 10).toString();
        case "%f":
          return Number.parseFloat(value).toString();
        case "%j":
          try {
            return JSON.stringify(value);
          } catch {
            return "[Circular]";
          }
        case "%o":
        case "%O":
          return inspectValue(value);
        default:
          return token;
      }
    });
    if (index >= args.length) {
      return formatted;
    }
    return [formatted, ...args.slice(index).map(formatValue)].join(" ");
  };
  return builtinUtilModule;
}
function setupGlobals() {
  const g = globalThis;
  g.process = process2;
  g.setTimeout = setTimeout2;
  g.clearTimeout = clearTimeout2;
  g.setInterval = setInterval;
  g.clearInterval = clearInterval;
  g.setImmediate = setImmediate;
  g.clearImmediate = clearImmediate;
  const nativeQueueMicrotask = typeof g.queueMicrotask === "function" ? g.queueMicrotask.bind(g) : _queueMicrotask;
  g.queueMicrotask = (callback) => {
    const asyncLocalStorageSnapshot = snapshotAsyncLocalStorageStores();
    return nativeQueueMicrotask(() =>
      runWithAsyncLocalStorageSnapshot(
        asyncLocalStorageSnapshot,
        callback,
        g,
        []
      )
    );
  };
  installWhatwgUrlGlobals(g);
  g.TextEncoder = TextEncoder2;
  g.TextDecoder = TextDecoder;
  g.Event = Event;
  g.CustomEvent = CustomEvent;
  g.EventTarget = EventTarget;
  if (typeof g.Buffer === "undefined") {
    g.Buffer = Buffer3;
  }
  const globalBuffer = g.Buffer;
  if (typeof globalBuffer.kMaxLength !== "number") {
    globalBuffer.kMaxLength = BUFFER_MAX_LENGTH;
  }
  if (typeof globalBuffer.kStringMaxLength !== "number") {
    globalBuffer.kStringMaxLength = BUFFER_MAX_STRING_LENGTH;
  }
  if (typeof globalBuffer.constants !== "object" || globalBuffer.constants === null) {
    globalBuffer.constants = BUFFER_CONSTANTS;
  }
  const builtinUtilModule = globalThis.__secureExecBuiltinUtilModule;
  if (builtinUtilModule?.types) {
    builtinUtilModule.types.isProxy = () => false;
  }
  installBuiltinUtilFormatWithOptions(builtinUtilModule);
  if (typeof g.atob === "undefined" || typeof g.btoa === "undefined") {
    const base64 = require_base64_js();
    if (typeof g.atob === "undefined") {
      g.atob = (value) => {
        const bytes = base64.toByteArray(String(value));
        let decoded = "";
        for (const byte of bytes) {
          decoded += String.fromCharCode(byte);
        }
        return decoded;
      };
    }
    if (typeof g.btoa === "undefined") {
      g.btoa = (value) => {
        const input = String(value);
        const bytes = new Uint8Array(input.length);
        for (let index = 0; index < input.length; index += 1) {
          const code = input.charCodeAt(index);
          if (code > 255) {
            throw new TypeError("Invalid character");
          }
          bytes[index] = code;
        }
        return base64.fromByteArray(bytes);
      };
    }
  }
  if (typeof g.Crypto === "undefined") {
    g.Crypto = SandboxCrypto;
  }
  if (typeof g.SubtleCrypto === "undefined") {
    g.SubtleCrypto = SandboxSubtleCrypto;
  }
  if (typeof g.CryptoKey === "undefined") {
    g.CryptoKey = SandboxCryptoKey;
  }
  if (typeof g.DOMException === "undefined") {
    g.DOMException = SandboxDOMException;
  }
  if (typeof g.crypto === "undefined") {
    g.crypto = builtinCryptoModule;
  } else {
    const cryptoObj = g.crypto;
    for (const [name, value] of Object.entries(builtinCryptoModule)) {
      if (typeof cryptoObj[name] === "undefined") {
        cryptoObj[name] = value;
      }
    }
  }
  g.fetch = fetch;
  g.Headers = UndiciHeaders;
  g.Request = UndiciRequest;
  g.Response = UndiciResponse;
  installSafeIntlFormatters(g);
}
export { import_buffer2, readProcessConfig, config2, processClockNow, getNowMs, _processStartTime, BUFFER_MAX_LENGTH, BUFFER_MAX_STRING_LENGTH, BUFFER_CONSTANTS, bufferPolyfillMutable, bufferProto, bufferCtorMutable, _exitCode, _exited, ProcessExitError, _signalNumbers, _signalNamesByNumber, _ignoredSelfSignals, _trackedProcessSignalEvents, _resolveSignal, _isTrackedProcessSignalEventName, _processKillErrnoByCode, _createProcessKillError, _processListeners, _processOnceListeners, _processMaxListeners, _processMaxListenersWarned, _listenerCountForEvent, _syncGuestProcessSignalState, _syncAllGuestProcessSignalStates, _deliverProcessSignal, signalDispatch, _addListener, _removeListener, _emit, isProcessExitError, normalizeAsyncError, routeAsyncCallbackError, scheduleAsyncRethrow, dispatchCustomEmitterListeners, _getStdinIsTTY, _getStdoutIsTTY, _getStderrIsTTY, getWriteCallback, emitListeners, createStdioWriteStream, _stdout, _stderr, formatConsoleValue, formatConsoleArgs, formatConsoleLine, Console, defaultConsole, createConsoleTask, consoleContext, builtinConsoleModule, v8Serialize, v8Deserialize, V8Serializer, V8Deserializer, configuredHeapLimitBytes, getHeapStatistics, getHeapSpaceStatistics, getHeapCodeStatistics, getCppHeapStatistics, getHeapSnapshot, builtinV8Module, VM_CONTEXT_TAG, VM_CONTEXT_ID, createVmNotImplementedError, isVmContextCandidate, normalizeVmOptions, mergeVmOptions, vmCreateContext, vmIsContext, assertVmContext, vmRunInThisContext, vmRunInContext, vmRunInNewContext, VmScript, builtinVmModule, createWorkerThreadsNotImplementedError, WorkerThreadPort, WorkerThreadMessageChannel, WorkerThreadWorker, builtinWorkerThreadsModule, _stdinListeners, _stdinOnceListeners, _stdinLiveDecoder, STDIN_HANDLE_ID, _stdinLiveBuffer, _stdinLiveStarted, _stdinLiveHandleRegistered, _stdinLiveTerminalEventsScheduled, _stdinLiveTerminalEventsEmitted, getStdinData, setStdinDataValue, getStdinPosition, setStdinPosition, getStdinEnded, setStdinEnded, getStdinFlowMode, setStdinFlowMode, _emitStdinData, emitStdinListeners, syncLiveStdinHandle, flushLiveStdinBuffer, maybeEmitLiveStdinTerminalEvents, finishLiveStdin, _getStreamStdin, ensureLiveStdinStarted, stdinDispatch, _stdin, hrtime, _cwd, _umask, _processVersionsCache, defaultProcessMemoryUsage, readLiveProcessMemoryUsage, readLiveProcessCpuUsage, defaultProcessResourceUsage, readLiveProcessResourceUsage, readLiveProcessVersions, process2, installProcessIpcBridge, applyProcessConfig, process_default, ttyIsatty, TtyReadStream, TtyWriteStream, builtinTtyModule, createPerfHooksOutOfRangeError, normalizePerformanceEntry, createPerformanceObserverEntryList, createPerformanceHistogram, builtinPerformance, collectReadableChunks, createBuiltinBlob, builtinStreamConsumersModule, getNodeReadableAsyncIterable, builtinStreamPromisesModule, builtinTimersPromisesModule, builtinPerfHooksModule, createAccessDeniedBuiltinError, DiagnosticsChannel, diagnosticsChannelCache, getDiagnosticsChannel, createDiagnosticsTracingChannel, builtinDiagnosticsChannelModule, asyncLocalStorageInstances, snapshotAsyncLocalStorageStores, applyAsyncLocalStorageSnapshot, runWithAsyncLocalStorageSnapshot, wrapAsyncLocalStorageCallback, builtinAsyncHooksModule, TIMER_DISPATCH, _queueMicrotask, normalizeTimerDelay, getTimerId, createKernelTimer, armKernelTimer, TimerHandle, _timerEntries, _timerDrainResolvers, getRefedTimerCount, checkTimerDrain, _getPendingTimerCount, _waitForTimerDrain, _nextTickQueue, _nextTickScheduled, flushNextTickQueue, scheduleNextTickFlush, timerDispatch, setTimeout2, clearTimeout2, setInterval, clearInterval, setImmediate, clearImmediate, Buffer3, createUnsupportedCryptoApiError, throwUnsupportedCryptoApi, kCryptoKeyToken, kCryptoToken, kSubtleToken, ERR_INVALID_THIS2, ERR_ILLEGAL_CONSTRUCTOR, createNodeTypeError2, SandboxDOMException, createDomLikeError, assertCryptoReceiver, assertSubtleReceiver, isIntegerTypedArray, toBase64, toArrayBuffer, normalizeAlgorithm, normalizeBridgeAlgorithm, SandboxCryptoKey, createCryptoKey, subtleCall, SandboxSubtleCrypto, subtleCrypto, SandboxCrypto, cryptoPolyfillInstance, cryptoPolyfill, createBuiltinHash, toSymmetricKeyBuffer, createBuiltinHmac, kBuiltinCryptoKeyObjectToken, isBufferLikeValue, toCryptoBuffer, encodeCryptoResult, isBuiltinKeyObject, serializeBridgeValue, deserializeBridgeValue, normalizeDirectCryptoKeyInput, serializeCryptoKeyInput, serializeOptionalCryptoOptions, normalizeCryptoAlgorithmName, callCryptoSync, decodeGeneratedCryptoValue, BuiltinKeyObject, createBuiltinKeyObject, normalizeCipherOptions, BuiltinCipherTransform, BuiltinSignContext, BuiltinVerifyContext, normalizeDiffieHellmanArgs, diffieHellmanSessionFinalizer, BuiltinDiffieHellmanSession, builtinCryptoModule, padDateTimeField, coerceIntlDate, formatSafeDateTimeValue, SafeDateTimeFormat, normalizeFractionDigitOption, applySafeNumberGrouping, SafeNumberFormat, installSafeIntlFormatters, encodeFilePathSegment, pathToFileURL2, fileURLToPath2, installBuiltinUtilFormatWithOptions, setupGlobals };
