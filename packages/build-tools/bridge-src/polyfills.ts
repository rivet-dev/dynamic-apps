import { signals } from "./builtins/os.js";
import { NativeAbortControllerGlobal, NativeAbortSignalGlobal, WebReadableStream, WebTextDecoderStream, WebTextEncoderStream, WebTransformStream, WebWritableStream, sandboxStructuredClone, undiciWebidlModule } from "./prelude.js";

// .agent/recovery/secure-exec/nodejs/src/bridge/polyfills.ts
function defineGlobal(name, value) {
  globalThis[name] = value;
}
if (typeof globalThis.global === "undefined") {
  defineGlobal("global", globalThis);
}
if (typeof globalThis.RegExp === "function" && !("__secureExecRgiEmojiCompat" in globalThis.RegExp)) {
  const NativeRegExp = globalThis.RegExp;
  const rgiEmojiPattern = "^\\p{RGI_Emoji}$";
  const rgiEmojiBaseClass = "[\\u{00A9}\\u{00AE}\\u{203C}\\u{2049}\\u{2122}\\u{2139}\\u{2194}-\\u{21AA}\\u{231A}-\\u{23FF}\\u{24C2}\\u{25AA}-\\u{27BF}\\u{2934}-\\u{2935}\\u{2B05}-\\u{2B55}\\u{3030}\\u{303D}\\u{3297}\\u{3299}\\u{1F000}-\\u{1FAFF}]";
  const rgiEmojiKeycap = "[#*0-9]\\uFE0F?\\u20E3";
  const rgiEmojiFallbackSource = "^(?:" + rgiEmojiKeycap + "|\\p{Regional_Indicator}{2}|" + rgiEmojiBaseClass + "(?:\\uFE0F|\\u200D(?:" + rgiEmojiKeycap + "|" + rgiEmojiBaseClass + ")|[\\u{1F3FB}-\\u{1F3FF}])*)$";
  try {
    new NativeRegExp(rgiEmojiPattern, "v");
  } catch (error) {
    if (String(error?.message ?? error).includes("RGI_Emoji")) {
      const CompatRegExp = function CompatRegExp2(pattern, flags) {
        const normalizedPattern = pattern instanceof NativeRegExp && flags === void 0 ? pattern.source : String(pattern);
        const normalizedFlags = flags === void 0 ? pattern instanceof NativeRegExp ? pattern.flags : "" : String(flags);
        try {
          return new NativeRegExp(pattern, flags);
        } catch (innerError) {
          if (normalizedPattern === rgiEmojiPattern && normalizedFlags === "v") {
            return new NativeRegExp(rgiEmojiFallbackSource, "u");
          }
          throw innerError;
        }
      };
      Object.setPrototypeOf(CompatRegExp, NativeRegExp);
      CompatRegExp.prototype = NativeRegExp.prototype;
      Object.defineProperty(CompatRegExp.prototype, "constructor", {
        value: CompatRegExp,
        writable: true,
        configurable: true
      });
      defineGlobal(
        "RegExp",
        Object.assign(CompatRegExp, { __secureExecRgiEmojiCompat: true })
      );
    }
  }
}
function withCode(error, code) {
  error.code = code;
  return error;
}
function createEncodingNotSupportedError(label) {
  return withCode(
    new RangeError(`The "${label}" encoding is not supported`),
    "ERR_ENCODING_NOT_SUPPORTED"
  );
}
function createEncodingInvalidDataError(encoding) {
  return withCode(
    new TypeError(`The encoded data was not valid for encoding ${encoding}`),
    "ERR_ENCODING_INVALID_ENCODED_DATA"
  );
}
function createInvalidDecodeInputError() {
  return withCode(
    new TypeError(
      'The "input" argument must be an instance of ArrayBuffer, SharedArrayBuffer, or ArrayBufferView.'
    ),
    "ERR_INVALID_ARG_TYPE"
  );
}
function trimAsciiWhitespace(value) {
  return value.replace(/^[\t\n\f\r ]+|[\t\n\f\r ]+$/g, "");
}
function normalizeEncodingLabel(label) {
  const normalized = trimAsciiWhitespace(
    label === void 0 ? "utf-8" : String(label)
  ).toLowerCase();
  switch (normalized) {
    case "utf-8":
    case "utf8":
    case "unicode-1-1-utf-8":
    case "unicode11utf8":
    case "unicode20utf8":
    case "x-unicode20utf8":
      return "utf-8";
    case "utf-16":
    case "utf-16le":
    case "ucs-2":
    case "ucs2":
    case "csunicode":
    case "iso-10646-ucs-2":
    case "unicode":
    case "unicodefeff":
      return "utf-16le";
    case "utf-16be":
    case "unicodefffe":
      return "utf-16be";
    default:
      throw createEncodingNotSupportedError(normalized);
  }
}
function toUint8Array(input) {
  if (input === void 0) {
    return new Uint8Array(0);
  }
  if (ArrayBuffer.isView(input)) {
    return new Uint8Array(input.buffer, input.byteOffset, input.byteLength);
  }
  if (input instanceof ArrayBuffer) {
    return new Uint8Array(input);
  }
  if (typeof SharedArrayBuffer !== "undefined" && input instanceof SharedArrayBuffer) {
    return new Uint8Array(input);
  }
  throw createInvalidDecodeInputError();
}
function encodeUtf8ScalarValue(codePoint, bytes) {
  if (codePoint <= 127) {
    bytes.push(codePoint);
    return;
  }
  if (codePoint <= 2047) {
    bytes.push(192 | codePoint >> 6, 128 | codePoint & 63);
    return;
  }
  if (codePoint <= 65535) {
    bytes.push(
      224 | codePoint >> 12,
      128 | codePoint >> 6 & 63,
      128 | codePoint & 63
    );
    return;
  }
  bytes.push(
    240 | codePoint >> 18,
    128 | codePoint >> 12 & 63,
    128 | codePoint >> 6 & 63,
    128 | codePoint & 63
  );
}
function encodeUtf8(input = "") {
  const value = String(input);
  const bytes = [];
  for (let index = 0; index < value.length; index += 1) {
    const codeUnit = value.charCodeAt(index);
    if (codeUnit >= 55296 && codeUnit <= 56319) {
      const nextIndex = index + 1;
      if (nextIndex < value.length) {
        const nextCodeUnit = value.charCodeAt(nextIndex);
        if (nextCodeUnit >= 56320 && nextCodeUnit <= 57343) {
          const codePoint = 65536 + (codeUnit - 55296 << 10) + (nextCodeUnit - 56320);
          encodeUtf8ScalarValue(codePoint, bytes);
          index = nextIndex;
          continue;
        }
      }
      encodeUtf8ScalarValue(65533, bytes);
      continue;
    }
    if (codeUnit >= 56320 && codeUnit <= 57343) {
      encodeUtf8ScalarValue(65533, bytes);
      continue;
    }
    encodeUtf8ScalarValue(codeUnit, bytes);
  }
  return new Uint8Array(bytes);
}
function appendCodePoint(output, codePoint) {
  if (codePoint <= 65535) {
    output.push(String.fromCharCode(codePoint));
    return;
  }
  const adjusted = codePoint - 65536;
  output.push(
    String.fromCharCode(55296 + (adjusted >> 10)),
    String.fromCharCode(56320 + (adjusted & 1023))
  );
}
function isContinuationByte(value) {
  return value >= 128 && value <= 191;
}
function decodeUtf8(bytes, fatal, stream, encoding) {
  const output = [];
  for (let index = 0; index < bytes.length; ) {
    const first = bytes[index];
    if (first <= 127) {
      output.push(String.fromCharCode(first));
      index += 1;
      continue;
    }
    let needed = 0;
    let codePoint = 0;
    if (first >= 194 && first <= 223) {
      needed = 1;
      codePoint = first & 31;
    } else if (first >= 224 && first <= 239) {
      needed = 2;
      codePoint = first & 15;
    } else if (first >= 240 && first <= 244) {
      needed = 3;
      codePoint = first & 7;
    } else {
      if (fatal) {
        throw createEncodingInvalidDataError(encoding);
      }
      output.push("\uFFFD");
      index += 1;
      continue;
    }
    if (index + needed >= bytes.length) {
      if (stream) {
        return {
          text: output.join(""),
          pending: Array.from(bytes.slice(index))
        };
      }
      if (fatal) {
        throw createEncodingInvalidDataError(encoding);
      }
      output.push("\uFFFD");
      break;
    }
    const second = bytes[index + 1];
    if (!isContinuationByte(second)) {
      if (fatal) {
        throw createEncodingInvalidDataError(encoding);
      }
      output.push("\uFFFD");
      index += 1;
      continue;
    }
    if (first === 224 && second < 160 || first === 237 && second > 159 || first === 240 && second < 144 || first === 244 && second > 143) {
      if (fatal) {
        throw createEncodingInvalidDataError(encoding);
      }
      output.push("\uFFFD");
      index += 1;
      continue;
    }
    codePoint = codePoint << 6 | second & 63;
    if (needed >= 2) {
      const third = bytes[index + 2];
      if (!isContinuationByte(third)) {
        if (fatal) {
          throw createEncodingInvalidDataError(encoding);
        }
        output.push("\uFFFD");
        index += 1;
        continue;
      }
      codePoint = codePoint << 6 | third & 63;
    }
    if (needed === 3) {
      const fourth = bytes[index + 3];
      if (!isContinuationByte(fourth)) {
        if (fatal) {
          throw createEncodingInvalidDataError(encoding);
        }
        output.push("\uFFFD");
        index += 1;
        continue;
      }
      codePoint = codePoint << 6 | fourth & 63;
    }
    if (codePoint >= 55296 && codePoint <= 57343) {
      if (fatal) {
        throw createEncodingInvalidDataError(encoding);
      }
      output.push("\uFFFD");
      index += needed + 1;
      continue;
    }
    appendCodePoint(output, codePoint);
    index += needed + 1;
  }
  return { text: output.join(""), pending: [] };
}
function decodeUtf16(bytes, encoding, fatal, stream, bomSeen) {
  const output = [];
  let endian = encoding === "utf-16be" ? "be" : "le";
  if (!bomSeen && encoding === "utf-16le" && bytes.length >= 2) {
    if (bytes[0] === 254 && bytes[1] === 255) {
      endian = "be";
    }
  }
  for (let index = 0; index < bytes.length; ) {
    if (index + 1 >= bytes.length) {
      if (stream) {
        return {
          text: output.join(""),
          pending: Array.from(bytes.slice(index))
        };
      }
      if (fatal) {
        throw createEncodingInvalidDataError(encoding);
      }
      output.push("\uFFFD");
      break;
    }
    const first = bytes[index];
    const second = bytes[index + 1];
    const codeUnit = endian === "le" ? first | second << 8 : first << 8 | second;
    index += 2;
    if (codeUnit >= 55296 && codeUnit <= 56319) {
      if (index + 1 >= bytes.length) {
        if (stream) {
          return {
            text: output.join(""),
            pending: Array.from(bytes.slice(index - 2))
          };
        }
        if (fatal) {
          throw createEncodingInvalidDataError(encoding);
        }
        output.push("\uFFFD");
        continue;
      }
      const nextFirst = bytes[index];
      const nextSecond = bytes[index + 1];
      const nextCodeUnit = endian === "le" ? nextFirst | nextSecond << 8 : nextFirst << 8 | nextSecond;
      if (nextCodeUnit >= 56320 && nextCodeUnit <= 57343) {
        const codePoint = 65536 + (codeUnit - 55296 << 10) + (nextCodeUnit - 56320);
        appendCodePoint(output, codePoint);
        index += 2;
        continue;
      }
      if (fatal) {
        throw createEncodingInvalidDataError(encoding);
      }
      output.push("\uFFFD");
      continue;
    }
    if (codeUnit >= 56320 && codeUnit <= 57343) {
      if (fatal) {
        throw createEncodingInvalidDataError(encoding);
      }
      output.push("\uFFFD");
      continue;
    }
    output.push(String.fromCharCode(codeUnit));
  }
  return { text: output.join(""), pending: [] };
}
var PatchedTextEncoder = class {
  encode(input = "") {
    return encodeUtf8(input);
  }
  encodeInto(input, destination) {
    const value = String(input);
    let read = 0;
    let written = 0;
    for (let index = 0; index < value.length; index += 1) {
      const codeUnit = value.charCodeAt(index);
      let chunk = "";
      if (codeUnit >= 55296 && codeUnit <= 56319 && index + 1 < value.length) {
        const nextCodeUnit = value.charCodeAt(index + 1);
        if (nextCodeUnit >= 56320 && nextCodeUnit <= 57343) {
          chunk = value.slice(index, index + 2);
        }
      }
      if (chunk === "") {
        chunk = value[index] ?? "";
      }
      const encoded = encodeUtf8(chunk);
      if (written + encoded.length > destination.length) {
        break;
      }
      destination.set(encoded, written);
      written += encoded.length;
      read += chunk.length;
      if (chunk.length === 2) {
        index += 1;
      }
    }
    return { read, written };
  }
  get encoding() {
    return "utf-8";
  }
  get [Symbol.toStringTag]() {
    return "TextEncoder";
  }
};
var PatchedTextDecoder = class {
  normalizedEncoding;
  fatalFlag;
  ignoreBOMFlag;
  pendingBytes = [];
  bomSeen = false;
  constructor(label, options) {
    const normalizedOptions = options == null ? {} : Object(options);
    this.normalizedEncoding = normalizeEncodingLabel(label);
    this.fatalFlag = Boolean(
      normalizedOptions.fatal
    );
    this.ignoreBOMFlag = Boolean(
      normalizedOptions.ignoreBOM
    );
  }
  get encoding() {
    return this.normalizedEncoding;
  }
  get fatal() {
    return this.fatalFlag;
  }
  get ignoreBOM() {
    return this.ignoreBOMFlag;
  }
  get [Symbol.toStringTag]() {
    return "TextDecoder";
  }
  decode(input, options) {
    const normalizedOptions = options == null ? {} : Object(options);
    const stream = Boolean(
      normalizedOptions.stream
    );
    const incoming = toUint8Array(input);
    const merged = new Uint8Array(this.pendingBytes.length + incoming.length);
    merged.set(this.pendingBytes, 0);
    merged.set(incoming, this.pendingBytes.length);
    const decoded = this.normalizedEncoding === "utf-8" ? decodeUtf8(
      merged,
      this.fatalFlag,
      stream,
      this.normalizedEncoding
    ) : decodeUtf16(
      merged,
      this.normalizedEncoding,
      this.fatalFlag,
      stream,
      this.bomSeen
    );
    this.pendingBytes = decoded.pending;
    let text = decoded.text;
    if (!this.bomSeen && text.length > 0) {
      if (!this.ignoreBOMFlag && text.charCodeAt(0) === 65279) {
        text = text.slice(1);
      }
      this.bomSeen = true;
    }
    if (!stream && this.pendingBytes.length > 0) {
      const pending = this.pendingBytes;
      this.pendingBytes = [];
      if (this.fatalFlag) {
        throw createEncodingInvalidDataError(this.normalizedEncoding);
      }
      return text + "\uFFFD".repeat(Math.ceil(pending.length / 2));
    }
    return text;
  }
};
function normalizeAddEventListenerOptions(options) {
  if (typeof options === "boolean") {
    return {
      capture: options,
      once: false,
      passive: false
    };
  }
  if (options == null) {
    return {
      capture: false,
      once: false,
      passive: false
    };
  }
  const normalized = Object(options);
  return {
    capture: Boolean(normalized.capture),
    once: Boolean(normalized.once),
    passive: Boolean(normalized.passive),
    signal: normalized.signal
  };
}
function normalizeRemoveEventListenerOptions(options) {
  if (typeof options === "boolean") {
    return options;
  }
  if (options == null) {
    return false;
  }
  return Boolean(Object(options).capture);
}
function isAbortSignalLike(value) {
  return typeof value === "object" && value !== null && "aborted" in value && typeof value.addEventListener === "function" && typeof value.removeEventListener === "function";
}
var PatchedEvent = class {
  static NONE = 0;
  static CAPTURING_PHASE = 1;
  static AT_TARGET = 2;
  static BUBBLING_PHASE = 3;
  type;
  bubbles;
  cancelable;
  composed;
  detail = null;
  defaultPrevented = false;
  target = null;
  currentTarget = null;
  eventPhase = 0;
  returnValue = true;
  cancelBubble = false;
  timeStamp = Date.now();
  isTrusted = false;
  srcElement = null;
  inPassiveListener = false;
  propagationStopped = false;
  immediatePropagationStopped = false;
  constructor(type, init) {
    if (arguments.length === 0) {
      throw new TypeError("The event type must be provided");
    }
    const normalizedInit = init == null ? {} : Object(init);
    this.type = String(type);
    this.bubbles = Boolean(normalizedInit.bubbles);
    this.cancelable = Boolean(normalizedInit.cancelable);
    this.composed = Boolean(normalizedInit.composed);
  }
  get [Symbol.toStringTag]() {
    return "Event";
  }
  preventDefault() {
    if (this.cancelable && !this.inPassiveListener) {
      this.defaultPrevented = true;
      this.returnValue = false;
    }
  }
  stopPropagation() {
    this.propagationStopped = true;
    this.cancelBubble = true;
  }
  stopImmediatePropagation() {
    this.propagationStopped = true;
    this.immediatePropagationStopped = true;
    this.cancelBubble = true;
  }
  composedPath() {
    return this.target ? [this.target] : [];
  }
  _setPassive(value) {
    this.inPassiveListener = value;
  }
  _isPropagationStopped() {
    return this.propagationStopped;
  }
  _isImmediatePropagationStopped() {
    return this.immediatePropagationStopped;
  }
};
var PatchedCustomEvent = class extends PatchedEvent {
  constructor(type, init) {
    super(type, init);
    const normalizedInit = init == null ? null : Object(init);
    this.detail = normalizedInit && "detail" in normalizedInit ? normalizedInit.detail : null;
  }
  get [Symbol.toStringTag]() {
    return "CustomEvent";
  }
};
var PatchedEventTarget = class {
  listeners = /* @__PURE__ */ new Map();
  addEventListener(type, listener, options) {
    const normalized = normalizeAddEventListenerOptions(options);
    if (normalized.signal !== void 0 && !isAbortSignalLike(normalized.signal)) {
      throw new TypeError(
        'The "signal" option must be an instance of AbortSignal.'
      );
    }
    if (listener == null) {
      return void 0;
    }
    if (typeof listener !== "function" && (typeof listener !== "object" || listener === null)) {
      return void 0;
    }
    if (normalized.signal?.aborted) {
      return void 0;
    }
    const records = this.listeners.get(type) ?? [];
    const existing = records.find(
      (record2) => record2.listener === listener && record2.capture === normalized.capture
    );
    if (existing) {
      return void 0;
    }
    const record = {
      listener,
      capture: normalized.capture,
      once: normalized.once,
      passive: normalized.passive,
      kind: typeof listener === "function" ? "function" : "object",
      signal: normalized.signal
    };
    if (normalized.signal) {
      record.abortListener = () => {
        this.removeEventListener(type, listener, normalized.capture);
      };
      normalized.signal.addEventListener("abort", record.abortListener, {
        once: true
      });
    }
    records.push(record);
    this.listeners.set(type, records);
    return void 0;
  }
  removeEventListener(type, listener, options) {
    if (listener == null) {
      return;
    }
    const capture = normalizeRemoveEventListenerOptions(options);
    const records = this.listeners.get(type);
    if (!records) {
      return;
    }
    const nextRecords = records.filter((record) => {
      const match = record.listener === listener && record.capture === capture;
      if (match && record.signal && record.abortListener) {
        record.signal.removeEventListener("abort", record.abortListener);
      }
      return !match;
    });
    if (nextRecords.length === 0) {
      this.listeners.delete(type);
      return;
    }
    this.listeners.set(type, nextRecords);
  }
  dispatchEvent(event) {
    if (typeof event !== "object" || event === null || typeof event.type !== "string") {
      throw new TypeError("Argument 1 must be an Event");
    }
    const patchedEvent = event;
    const records = (this.listeners.get(patchedEvent.type) ?? []).slice();
    patchedEvent.target = this;
    patchedEvent.currentTarget = this;
    patchedEvent.eventPhase = 2;
    for (const record of records) {
      const active = this.listeners.get(patchedEvent.type)?.includes(record);
      if (!active) {
        continue;
      }
      if (record.once) {
        this.removeEventListener(patchedEvent.type, record.listener, record.capture);
      }
      patchedEvent._setPassive(record.passive);
      if (record.kind === "function") {
        record.listener.call(this, patchedEvent);
      } else {
        const handleEvent = record.listener.handleEvent;
        if (typeof handleEvent === "function") {
          handleEvent.call(record.listener, patchedEvent);
        }
      }
      patchedEvent._setPassive(false);
      if (patchedEvent._isImmediatePropagationStopped()) {
        break;
      }
      if (patchedEvent._isPropagationStopped()) {
        break;
      }
    }
    patchedEvent.currentTarget = null;
    patchedEvent.eventPhase = 0;
    return !patchedEvent.defaultPrevented;
  }
};
var TextEncoder2 = PatchedTextEncoder;
var TextDecoder = PatchedTextDecoder;
var Event = PatchedEvent;
var CustomEvent = PatchedCustomEvent;
var EventTarget = PatchedEventTarget;
var AbortSignal = typeof NativeAbortSignalGlobal === "function" ? NativeAbortSignalGlobal : class extends EventTarget {
  constructor() {
    super();
    this.aborted = false;
    this.reason = void 0;
  }
  throwIfAborted() {
    if (this.aborted) {
      throw this.reason instanceof Error ? this.reason : new Error(String(this.reason ?? "AbortError"));
    }
  }
};
var AbortController = typeof NativeAbortControllerGlobal === "function" ? NativeAbortControllerGlobal : class {
  constructor() {
    this.signal = new AbortSignal();
  }
  abort(reason) {
    if (this.signal.aborted) {
      return;
    }
    this.signal.aborted = true;
    this.signal.reason = reason;
    this.signal.dispatchEvent(new Event("abort"));
  }
};
function ensureNamedConstructor(ctor, expectedName) {
  if (typeof ctor !== "function") {
    return;
  }
  try {
    if (ctor.name !== expectedName) {
      Object.defineProperty(ctor, "name", {
        configurable: true,
        value: expectedName
      });
    }
  } catch {
  }
}
ensureNamedConstructor(AbortSignal, "AbortSignal");
ensureNamedConstructor(AbortController, "AbortController");
try {
  const signalCtor = Object.getPrototypeOf(new AbortController().signal)?.constructor;
  ensureNamedConstructor(signalCtor, "AbortSignal");
} catch {
}
try {
  globalThis.AbortSignal = AbortSignal;
} catch {
}
try {
  globalThis.AbortController = AbortController;
} catch {
}
function createAbortSignalReason(reason) {
  if (reason !== void 0) {
    return reason;
  }
  if (typeof globalThis.DOMException === "function") {
    return new globalThis.DOMException("This operation was aborted", "AbortError");
  }
  const error = new Error("This operation was aborted");
  error.name = "AbortError";
  return error;
}
function createAbortedSignal(reason) {
  const controller = new AbortController();
  controller.abort(createAbortSignalReason(reason));
  return controller.signal;
}
function normalizeAbortSignalTimeout(delay) {
  if (typeof delay !== "number") {
    throw new TypeError(`The "delay" argument must be of type number. Received ${typeof delay}`);
  }
  if (!Number.isFinite(delay) || delay < 0) {
    throw new RangeError(`The value of "delay" is out of range. It must be >= 0. Received ${String(delay)}`);
  }
  return Math.trunc(delay);
}
if (typeof AbortSignal.abort !== "function") {
  Object.defineProperty(AbortSignal, "abort", {
    configurable: true,
    writable: true,
    value(reason = void 0) {
      return createAbortedSignal(reason);
    }
  });
}
if (typeof AbortSignal.timeout !== "function") {
  Object.defineProperty(AbortSignal, "timeout", {
    configurable: true,
    writable: true,
    value(delay) {
      const timeout = normalizeAbortSignalTimeout(delay);
      const controller = new AbortController();
      const timer = setTimeout(() => {
        controller.abort(createAbortSignalReason());
      }, timeout);
      if (typeof timer?.unref === "function") {
        timer.unref();
      }
      controller.signal.addEventListener("abort", () => {
        clearTimeout(timer);
      }, { once: true });
      return controller.signal;
    }
  });
}
if (typeof AbortSignal.any !== "function") {
  Object.defineProperty(AbortSignal, "any", {
    configurable: true,
    writable: true,
    value(signals) {
      if (!signals || typeof signals[Symbol.iterator] !== "function") {
        throw new TypeError("The \"signals\" argument must be an iterable");
      }
      const inputs = Array.from(signals);
      const controller = new AbortController();
      if (inputs.length === 0) {
        return controller.signal;
      }
      const listeners = [];
      const abortFromSignal = (signal) => {
        while (listeners.length > 0) {
          const [candidate, listener] = listeners.pop();
          candidate.removeEventListener?.("abort", listener);
        }
        controller.abort(signal.reason);
      };
      for (const signal of inputs) {
        if (!signal || typeof signal.aborted !== "boolean" || typeof signal.addEventListener !== "function") {
          throw new TypeError("The \"signals\" argument must contain AbortSignal instances");
        }
        if (signal.aborted) {
          abortFromSignal(signal);
          return controller.signal;
        }
        const onAbort = () => abortFromSignal(signal);
        listeners.push([signal, onAbort]);
        signal.addEventListener("abort", onAbort, { once: true });
      }
      return controller.signal;
    }
  });
}
var FallbackWritableStream = class {
  constructor(sink = {}) {
    this._sink = sink;
  }
  getWriter() {
    const sink = this._sink;
    return {
      write(chunk) {
        return Promise.resolve(typeof sink.write === "function" ? sink.write(chunk) : void 0);
      },
      close() {
        return Promise.resolve(typeof sink.close === "function" ? sink.close() : void 0);
      },
      releaseLock() {
      }
    };
  }
};
var FallbackReadableStream = class {
  constructor(source = {}) {
    this._queue = [];
    this._pending = [];
    this._closed = false;
    this._error = null;
    const flushPending = () => {
      while (this._pending.length > 0) {
        const waiter = this._pending.shift();
        if (this._error) {
          waiter.reject(this._error);
          continue;
        }
        if (this._queue.length > 0) {
          waiter.resolve({ value: this._queue.shift(), done: false });
          continue;
        }
        if (this._closed) {
          waiter.resolve({ value: void 0, done: true });
          continue;
        }
        this._pending.unshift(waiter);
        break;
      }
    };
    const controller = {
      enqueue: (value) => {
        if (this._closed || this._error) return;
        this._queue.push(value);
        flushPending();
      },
      close: () => {
        if (this._closed || this._error) return;
        this._closed = true;
        flushPending();
      },
      error: (error) => {
        if (this._closed || this._error) return;
        this._error = error instanceof Error ? error : new Error(String(error));
        flushPending();
      }
    };
    if (typeof source.start === "function") {
      Promise.resolve().then(() => source.start(controller)).catch((error) => controller.error(error));
    }
  }
  getReader() {
    return {
      read: () => {
        if (this._error) {
          return Promise.reject(this._error);
        }
        if (this._queue.length > 0) {
          return Promise.resolve({ value: this._queue.shift(), done: false });
        }
        if (this._closed) {
          return Promise.resolve({ value: void 0, done: true });
        }
        return new Promise((resolve, reject) => {
          this._pending.push({ resolve, reject });
        });
      },
      releaseLock() {
      }
    };
  }
};
defineGlobal("TextEncoder", TextEncoder2);
defineGlobal("TextDecoder", TextDecoder);
defineGlobal("Event", Event);
defineGlobal("CustomEvent", CustomEvent);
defineGlobal("EventTarget", EventTarget);
defineGlobal("AbortSignal", AbortSignal);
defineGlobal("AbortController", AbortController);
defineGlobal("structuredClone", sandboxStructuredClone);
if (
  globalThis.WebAssembly &&
  typeof globalThis.WebAssembly.instantiateStreaming !== "function"
) {
  globalThis.WebAssembly.instantiateStreaming = async function instantiateStreaming(source, imports) {
    const response = await source;
    if (response == null || typeof response.arrayBuffer !== "function") {
      throw new TypeError("WebAssembly.instantiateStreaming requires a Response or promise for one");
    }
    const bytes = new Uint8Array(await response.arrayBuffer());
    return globalThis.WebAssembly.instantiate(bytes, imports);
  };
}
defineGlobal("ReadableStream", typeof WebReadableStream === "function" ? WebReadableStream : FallbackReadableStream);
defineGlobal("WritableStream", typeof WebWritableStream === "function" ? WebWritableStream : FallbackWritableStream);
if (typeof WebTransformStream === "function") {
  defineGlobal("TransformStream", WebTransformStream);
}
if (typeof WebTextEncoderStream === "function") {
  defineGlobal("TextEncoderStream", WebTextEncoderStream);
}
if (typeof WebTextDecoderStream === "function") {
  defineGlobal("TextDecoderStream", WebTextDecoderStream);
}
const undiciWebidl = undiciWebidlModule?.webidl ?? undiciWebidlModule;
if (undiciWebidl?.is) {
  undiciWebidl.is.ReadableStream = (value) =>
    value != null &&
    (value instanceof globalThis.ReadableStream ||
      typeof value.getReader === "function");
  undiciWebidl.is.AbortSignal = (value) =>
    value != null &&
    (value instanceof globalThis.AbortSignal ||
      (typeof value.aborted === "boolean" &&
        typeof value.addEventListener === "function"));
}
if (undiciWebidl?.converters?.AbortSignal) {
  undiciWebidl.converters.AbortSignal = (value, ...args) => {
    if (
      value != null &&
      (value instanceof globalThis.AbortSignal ||
        (typeof value.aborted === "boolean" &&
          typeof value.addEventListener === "function"))
    ) {
      return value;
    }
    return undiciWebidl.interfaceConverter(
      undiciWebidl.is.AbortSignal,
      "AbortSignal"
    )(value, ...args);
  };
}
export { defineGlobal, withCode, createEncodingNotSupportedError, createEncodingInvalidDataError, createInvalidDecodeInputError, trimAsciiWhitespace, normalizeEncodingLabel, toUint8Array, encodeUtf8ScalarValue, encodeUtf8, appendCodePoint, isContinuationByte, decodeUtf8, decodeUtf16, PatchedTextEncoder, PatchedTextDecoder, normalizeAddEventListenerOptions, normalizeRemoveEventListenerOptions, isAbortSignalLike, PatchedEvent, PatchedCustomEvent, PatchedEventTarget, TextEncoder2, TextDecoder, Event, CustomEvent, EventTarget, AbortSignal, AbortController, ensureNamedConstructor, createAbortSignalReason, createAbortedSignal, normalizeAbortSignalTimeout, FallbackWritableStream, FallbackReadableStream, undiciWebidl };
