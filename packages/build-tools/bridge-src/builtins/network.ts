import { UndiciClient, getSecureExecUndiciDispatcher, undiciFetch, undiciRequest } from "./child-process.js";
import { _fdGetPath, _fs, createBridgeSyncFacade, decodeBridgeJson } from "./fs.js";
import { dispatchCustomEmitterListeners, setImmediate } from "./process.js";
import { exposeCustomGlobal } from "../global-exposure.js";
import { undiciHeadersModule, undiciRequestModule, undiciResponseModule } from "../prelude.js";
import { __export } from "../vendor/esbuild-runtime.js";

// .agent/recovery/secure-exec/nodejs/src/bridge/network.ts
var network_exports = {};
__export(network_exports, {
  ClientRequest: () => ClientRequest,
  Headers: () => Headers,
  IncomingMessage: () => IncomingMessage,
  Request: () => Request,
  Response: () => Response,
  default: () => network_default,
  dns: () => dns,
  fetch: () => fetch,
  http: () => http,
  http2: () => http2,
  https: () => https
});
var MAX_HTTP_BODY_BYTES = 50 * 1024 * 1024;
var MAX_HTTP_REQUEST_HEADER_BYTES = 64 * 1024;
var MAX_HTTP_REQUEST_HEADERS = 2e3;
var _fetchHandleCounter = 0;
var UndiciHeaders = undiciHeadersModule?.Headers ?? undiciHeadersModule?.default ?? undiciHeadersModule;
var UndiciRequest = undiciRequestModule?.Request ?? undiciRequestModule?.default ?? undiciRequestModule;
var UndiciResponse = undiciResponseModule?.Response ?? undiciResponseModule?.default ?? undiciResponseModule;
function serializeFetchHeaders(headers) {
  if (!headers) {
    return {};
  }
  if (headers instanceof Headers) {
    return Object.fromEntries(headers.entries());
  }
  if (typeof UndiciHeaders === "function" && headers instanceof UndiciHeaders) {
    return Object.fromEntries(headers.entries());
  }
  if (isFlatHeaderList(headers)) {
    const normalized = {};
    for (let index = 0; index < headers.length; index += 2) {
      const key = headers[index];
      const value = headers[index + 1];
      if (key !== void 0 && value !== void 0) {
        normalized[key] = value;
      }
    }
    return normalized;
  }
  if (typeof headers.entries === "function") {
    return Object.fromEntries(headers.entries());
  }
  if (typeof headers[Symbol.iterator] === "function") {
    return Object.fromEntries(headers);
  }
  return Object.fromEntries(new Headers(headers).entries());
}
function createFetchHeaders(headers) {
  return new Headers(serializeFetchHeaders(headers));
}
function normalizeFetchRequestInit(options = {}) {
  const normalized = { ...options };
  // Some bundled Node SDKs pass node-fetch style `agent` options into fetch().
  // Undici doesn't accept that field, and the default global dispatcher already
  // routes through the secure-exec virtual network stack.
  if (Object.prototype.hasOwnProperty.call(normalized, "agent")) {
    delete normalized.agent;
  }
  if (Object.prototype.hasOwnProperty.call(normalized, "headers")) {
    normalized.headers = serializeFetchHeaders(normalized.headers);
  }
  if (
    normalized.body != null &&
    normalized.duplex == null &&
    String(normalized.method ?? "GET").toUpperCase() !== "GET" &&
    String(normalized.method ?? "GET").toUpperCase() !== "HEAD"
  ) {
    normalized.duplex = "half";
  }
  return normalized;
}
function ensureFetchAcceptEncoding(options) {
  const headers = serializeFetchHeaders(options?.headers);
  const hasAcceptEncoding = Object.keys(headers).some(
    (key) => key.toLowerCase() === "accept-encoding"
  );
  if (!hasAcceptEncoding) {
    headers["accept-encoding"] = "gzip, deflate";
  }
  return { ...(options || {}), headers };
}
async function fetch(input, options = {}) {
  if (typeof undiciFetch !== "function") {
    throw new Error("fetch requires undici to be configured");
  }
  let resolvedInput = input;
  let normalizedOptions = options;
  if (input instanceof Request) {
    resolvedInput = input.url;
    normalizedOptions = {
      method: input.method,
      headers: serializeFetchHeaders(input.headers),
      body: input.body,
      ...options
    };
  }
  normalizedOptions = normalizeFetchRequestInit(normalizedOptions);
  normalizedOptions = ensureFetchAcceptEncoding(normalizedOptions);
  const requestLabel = typeof resolvedInput === "string" ? resolvedInput : resolvedInput?.url ? String(resolvedInput.url) : String(resolvedInput);
  const handleId = typeof _registerHandle === "function" ? `fetch:${++_fetchHandleCounter}` : null;
  if (handleId) {
    _registerHandle?.(handleId, `fetch ${requestLabel}`);
  }
  const fetchDispatcher = normalizedOptions.dispatcher == null && typeof getSecureExecUndiciDispatcher === "function" ? getSecureExecUndiciDispatcher() : null;
  try {
    return await undiciFetch(
      resolvedInput,
      fetchDispatcher ? { ...normalizedOptions, dispatcher: fetchDispatcher } : normalizedOptions
    );
  } finally {
    if (handleId) {
      _unregisterHandle?.(handleId);
    }
  }
}
var Headers = class _Headers {
  _headers = {};
  constructor(init) {
    if (init && init !== null) {
      if (init instanceof _Headers) {
        this._headers = { ...init._headers };
      } else if (Array.isArray(init)) {
        init.forEach(([key, value]) => {
          this._headers[key.toLowerCase()] = value;
        });
      } else if (typeof init === "object") {
        Object.entries(init).forEach(([key, value]) => {
          this._headers[key.toLowerCase()] = value;
        });
      }
    }
  }
  get(name) {
    return this._headers[name.toLowerCase()] || null;
  }
  set(name, value) {
    this._headers[name.toLowerCase()] = value;
  }
  has(name) {
    return name.toLowerCase() in this._headers;
  }
  delete(name) {
    delete this._headers[name.toLowerCase()];
  }
  entries() {
    return Object.entries(this._headers)[Symbol.iterator]();
  }
  [Symbol.iterator]() {
    return this.entries();
  }
  keys() {
    return Object.keys(this._headers)[Symbol.iterator]();
  }
  values() {
    return Object.values(this._headers)[Symbol.iterator]();
  }
  append(name, value) {
    const key = name.toLowerCase();
    if (key in this._headers) {
      this._headers[key] = this._headers[key] + ", " + value;
    } else {
      this._headers[key] = value;
    }
  }
  forEach(callback) {
    Object.entries(this._headers).forEach(([k, v]) => callback(v, k, this));
  }
};
var Request = class _Request {
  url;
  method;
  headers;
  body;
  mode;
  credentials;
  cache;
  redirect;
  referrer;
  integrity;
  constructor(input, init = {}) {
    this.url = typeof input === "string" ? input : input.url;
    this.method = init.method || (typeof input !== "string" ? input.method : void 0) || "GET";
    this.headers = createFetchHeaders(
      init.headers || (typeof input !== "string" ? input.headers : void 0)
    );
    this.body = init.body || null;
    this.mode = init.mode || "cors";
    this.credentials = init.credentials || "same-origin";
    this.cache = init.cache || "default";
    this.redirect = init.redirect || "follow";
    this.referrer = init.referrer || "about:client";
    this.integrity = init.integrity || "";
  }
  clone() {
    return new _Request(this.url, this);
  }
};
var Response = class _Response {
  _body;
  status;
  statusText;
  headers;
  ok;
  type;
  url;
  redirected;
  constructor(body, init = {}) {
    this._body = body || null;
    this.status = init.status || 200;
    this.statusText = init.statusText || "OK";
    this.headers = new Headers(init.headers);
    this.ok = this.status >= 200 && this.status < 300;
    this.type = "default";
    this.url = "";
    this.redirected = false;
  }
  async text() {
    return String(this._body || "");
  }
  async json() {
    return JSON.parse(this._body || "{}");
  }
  get body() {
    const bodyStr = this._body;
    if (bodyStr === null) return null;
    return {
      getReader() {
        let consumed = false;
        return {
          async read() {
            if (consumed) return { done: true };
            consumed = true;
            const encoder = new TextEncoder();
            return { done: false, value: encoder.encode(bodyStr) };
          }
        };
      }
    };
  }
  clone() {
    return new _Response(this._body, { status: this.status, statusText: this.statusText });
  }
  static error() {
    return new _Response(null, { status: 0, statusText: "" });
  }
  static redirect(url, status = 302) {
    return new _Response(null, { status, headers: { Location: url } });
  }
};
function normalizeDnsLookupInvocation(hostname, options, callback) {
  let normalizedOptions = {};
  let done = callback;
  if (typeof options === "function") {
    done = options;
  } else if (typeof options === "number") {
    normalizedOptions = { family: options };
  } else if (options == null) {
    normalizedOptions = {};
  } else if (typeof options === "object") {
    normalizedOptions = { ...options };
  } else {
    throw new TypeError("dns.lookup options must be a number, object, or callback");
  }
  const family = normalizedOptions.family === 4 || normalizedOptions.family === 6 ? normalizedOptions.family : void 0;
  return {
    callback: done,
    options: {
      hostname: String(hostname),
      family,
      all: normalizedOptions.all === true
    }
  };
}
function createUnsupportedDnsError(subject) {
  const error = new Error(`${subject} is not supported by the secure-exec dns polyfill`);
  error.code = "ERR_NOT_IMPLEMENTED";
  return error;
}
function normalizeDnsResolveInvocation(methodName, hostname, rrtype, callback) {
  let type = rrtype;
  let done = callback;
  if (typeof rrtype === "function") {
    done = rrtype;
    type = void 0;
  }
  const normalizedType = String(type ?? "A").toUpperCase();
  if (![
    "A",
    "AAAA",
    "MX",
    "TXT",
    "SRV",
    "CNAME",
    "PTR",
    "NS",
    "SOA",
    "NAPTR",
    "CAA",
    "ANY"
  ].includes(normalizedType)) {
    throw createUnsupportedDnsError(`${methodName}(${normalizedType})`);
  }
  return {
    callback: done,
    options: {
      hostname: String(hostname),
      rrtype: normalizedType
    }
  };
}
function parseDnsLookupRecords(resultJson) {
  let parsed = resultJson;
  if (typeof parsed === "string") {
    parsed = JSON.parse(parsed);
  }
  if (parsed && typeof parsed === "object" && Array.isArray(parsed.records)) {
    parsed = parsed.records;
  } else if (parsed && typeof parsed === "object" && typeof parsed.address === "string") {
    parsed = [parsed];
  }
  if (!Array.isArray(parsed)) {
    return [];
  }
  return parsed.filter((record) => record && typeof record.address === "string").map((record) => ({
    address: record.address,
    family: record.family === 6 ? 6 : 4
  }));
}
function parseDnsResolveRecords(resultJson) {
  let parsed = resultJson;
  if (typeof parsed === "string") {
    parsed = JSON.parse(parsed);
  }
  return parsed;
}
function createInvalidDnsServersError(subject) {
  const error = new TypeError(`${subject} expects an array of non-empty server strings`);
  error.code = "ERR_INVALID_ARG_TYPE";
  return error;
}
function normalizeDnsServers(subject, servers) {
  if (!Array.isArray(servers)) {
    throw createInvalidDnsServersError(subject);
  }
  return servers.map((server) => {
    if (typeof server !== "string" || server.length === 0) {
      throw createInvalidDnsServersError(subject);
    }
    return server;
  });
}
function lookupDnsRecords(hostname, options, callback) {
  const invocation = normalizeDnsLookupInvocation(hostname, options, callback);
  return _networkDnsLookupRaw.apply(
    void 0,
    [invocation.options],
    { result: { promise: true } }
  ).then((resultJson) => {
    const records = parseDnsLookupRecords(resultJson);
    if (typeof invocation.callback === "function") {
      if (invocation.options.all) {
        invocation.callback(null, records);
      } else {
        const first = records[0] ?? {
          address: null,
          family: invocation.options.family ?? 0
        };
        invocation.callback(null, first.address, first.family);
      }
    }
    return invocation.options.all ? records : records[0] ?? {
      address: "",
      family: invocation.options.family ?? 0
    };
  });
}
function resolveDnsRecords(methodName, hostname, rrtype, callback) {
  const invocation = normalizeDnsResolveInvocation(methodName, hostname, rrtype, callback);
  return _networkDnsResolveRaw.apply(
    void 0,
    [invocation.options],
    { result: { promise: true } }
  ).then((resultJson) => {
    const records = parseDnsResolveRecords(resultJson);
    if (typeof invocation.callback === "function") {
      queueMicrotask(() => invocation.callback(null, records));
    }
    return records;
  }).catch((err) => {
    if (typeof invocation.callback === "function") {
      queueMicrotask(() => invocation.callback(err));
    }
    throw err;
  });
}
// Resolver instances keep guest-owned server lists for API compatibility.
// Queries still use the VM-wide kernel resolver until the sync RPC grows
// per-request nameserver overrides.
class SecureExecResolver {
  constructor() {
    this._servers = [];
  }
  cancel() {
  }
  getServers() {
    return this._servers.slice();
  }
  lookup(hostname, options, callback) {
    return lookupDnsRecords(hostname, options, callback);
  }
  resolve(hostname, rrtype, callback) {
    return resolveDnsRecords("dns.resolve", hostname, rrtype, callback);
  }
  resolve4(hostname, callback) {
    return resolveDnsRecords("dns.resolve4", hostname, "A", callback);
  }
  resolve6(hostname, callback) {
    return resolveDnsRecords("dns.resolve6", hostname, "AAAA", callback);
  }
  resolveAny(hostname, callback) {
    return resolveDnsRecords("dns.resolveAny", hostname, "ANY", callback);
  }
  resolveMx(hostname, callback) {
    return resolveDnsRecords("dns.resolveMx", hostname, "MX", callback);
  }
  resolveTxt(hostname, callback) {
    return resolveDnsRecords("dns.resolveTxt", hostname, "TXT", callback);
  }
  resolveSrv(hostname, callback) {
    return resolveDnsRecords("dns.resolveSrv", hostname, "SRV", callback);
  }
  resolveCname(hostname, callback) {
    return resolveDnsRecords("dns.resolveCname", hostname, "CNAME", callback);
  }
  resolvePtr(hostname, callback) {
    return resolveDnsRecords("dns.resolvePtr", hostname, "PTR", callback);
  }
  resolveNs(hostname, callback) {
    return resolveDnsRecords("dns.resolveNs", hostname, "NS", callback);
  }
  resolveSoa(hostname, callback) {
    return resolveDnsRecords("dns.resolveSoa", hostname, "SOA", callback);
  }
  resolveNaptr(hostname, callback) {
    return resolveDnsRecords("dns.resolveNaptr", hostname, "NAPTR", callback);
  }
  resolveCaa(hostname, callback) {
    return resolveDnsRecords("dns.resolveCaa", hostname, "CAA", callback);
  }
  setServers(servers) {
    this._servers = normalizeDnsServers("dns.Resolver.setServers", servers);
  }
}
class SecureExecPromisesResolver {
  constructor() {
    this._servers = [];
  }
  cancel() {
  }
  getServers() {
    return this._servers.slice();
  }
  lookup(hostname, options) {
    return lookupDnsRecords(hostname, options);
  }
  resolve(hostname, rrtype) {
    return resolveDnsRecords("dns.resolve", hostname, rrtype);
  }
  resolve4(hostname) {
    return resolveDnsRecords("dns.resolve4", hostname, "A");
  }
  resolve6(hostname) {
    return resolveDnsRecords("dns.resolve6", hostname, "AAAA");
  }
  resolveAny(hostname) {
    return resolveDnsRecords("dns.resolveAny", hostname, "ANY");
  }
  resolveMx(hostname) {
    return resolveDnsRecords("dns.resolveMx", hostname, "MX");
  }
  resolveTxt(hostname) {
    return resolveDnsRecords("dns.resolveTxt", hostname, "TXT");
  }
  resolveSrv(hostname) {
    return resolveDnsRecords("dns.resolveSrv", hostname, "SRV");
  }
  resolveCname(hostname) {
    return resolveDnsRecords("dns.resolveCname", hostname, "CNAME");
  }
  resolvePtr(hostname) {
    return resolveDnsRecords("dns.resolvePtr", hostname, "PTR");
  }
  resolveNs(hostname) {
    return resolveDnsRecords("dns.resolveNs", hostname, "NS");
  }
  resolveSoa(hostname) {
    return resolveDnsRecords("dns.resolveSoa", hostname, "SOA");
  }
  resolveNaptr(hostname) {
    return resolveDnsRecords("dns.resolveNaptr", hostname, "NAPTR");
  }
  resolveCaa(hostname) {
    return resolveDnsRecords("dns.resolveCaa", hostname, "CAA");
  }
  setServers(servers) {
    this._servers = normalizeDnsServers("dns.promises.Resolver.setServers", servers);
  }
}
var dns = {
  lookup(hostname, options, callback) {
    lookupDnsRecords(hostname, options, callback).catch((err) => {
      const done = typeof options === "function" ? options : callback;
      done?.(err);
    });
  },
  resolve(hostname, rrtype, callback) {
    resolveDnsRecords("dns.resolve", hostname, rrtype, callback).catch(() => {
    });
  },
  resolve4(hostname, callback) {
    resolveDnsRecords("dns.resolve4", hostname, "A", callback).catch(() => {
    });
  },
  resolve6(hostname, callback) {
    resolveDnsRecords("dns.resolve6", hostname, "AAAA", callback).catch(() => {
    });
  },
  resolveAny(hostname, callback) {
    resolveDnsRecords("dns.resolveAny", hostname, "ANY", callback).catch(() => {
    });
  },
  resolveMx(hostname, callback) {
    resolveDnsRecords("dns.resolveMx", hostname, "MX", callback).catch(() => {
    });
  },
  resolveTxt(hostname, callback) {
    resolveDnsRecords("dns.resolveTxt", hostname, "TXT", callback).catch(() => {
    });
  },
  resolveSrv(hostname, callback) {
    resolveDnsRecords("dns.resolveSrv", hostname, "SRV", callback).catch(() => {
    });
  },
  resolveCname(hostname, callback) {
    resolveDnsRecords("dns.resolveCname", hostname, "CNAME", callback).catch(() => {
    });
  },
  resolvePtr(hostname, callback) {
    resolveDnsRecords("dns.resolvePtr", hostname, "PTR", callback).catch(() => {
    });
  },
  resolveNs(hostname, callback) {
    resolveDnsRecords("dns.resolveNs", hostname, "NS", callback).catch(() => {
    });
  },
  resolveSoa(hostname, callback) {
    resolveDnsRecords("dns.resolveSoa", hostname, "SOA", callback).catch(() => {
    });
  },
  resolveNaptr(hostname, callback) {
    resolveDnsRecords("dns.resolveNaptr", hostname, "NAPTR", callback).catch(() => {
    });
  },
  resolveCaa(hostname, callback) {
    resolveDnsRecords("dns.resolveCaa", hostname, "CAA", callback).catch(() => {
    });
  },
  promises: {
    Resolver: SecureExecPromisesResolver,
    lookup(hostname, _options) {
      return lookupDnsRecords(hostname, _options);
    },
    resolve(hostname, rrtype) {
      return resolveDnsRecords("dns.resolve", hostname, rrtype || "A");
    },
    resolve4(hostname) {
      return resolveDnsRecords("dns.resolve4", hostname, "A");
    },
    resolve6(hostname) {
      return resolveDnsRecords("dns.resolve6", hostname, "AAAA");
    },
    resolveAny(hostname) {
      return resolveDnsRecords("dns.resolveAny", hostname, "ANY");
    },
    resolveMx(hostname) {
      return resolveDnsRecords("dns.resolveMx", hostname, "MX");
    },
    resolveTxt(hostname) {
      return resolveDnsRecords("dns.resolveTxt", hostname, "TXT");
    },
    resolveSrv(hostname) {
      return resolveDnsRecords("dns.resolveSrv", hostname, "SRV");
    },
    resolveCname(hostname) {
      return resolveDnsRecords("dns.resolveCname", hostname, "CNAME");
    },
    resolvePtr(hostname) {
      return resolveDnsRecords("dns.resolvePtr", hostname, "PTR");
    },
    resolveNs(hostname) {
      return resolveDnsRecords("dns.resolveNs", hostname, "NS");
    },
    resolveSoa(hostname) {
      return resolveDnsRecords("dns.resolveSoa", hostname, "SOA");
    },
    resolveNaptr(hostname) {
      return resolveDnsRecords("dns.resolveNaptr", hostname, "NAPTR");
    },
    resolveCaa(hostname) {
      return resolveDnsRecords("dns.resolveCaa", hostname, "CAA");
    }
  },
  Resolver: SecureExecResolver,
  getServers() {
    return [];
  },
  lookupService() {
    throw createUnsupportedDnsError("dns.lookupService");
  },
  reverse() {
    throw createUnsupportedDnsError("dns.reverse");
  },
  setServers() {
    throw createUnsupportedDnsError("dns.setServers");
  }
};
function createConnResetError(message = "socket hang up") {
  const error = new Error(message);
  error.code = "ECONNRESET";
  return error;
}
function createAbortError2() {
  const error = new Error("The operation was aborted");
  error.name = "AbortError";
  error.code = "ABORT_ERR";
  return error;
}
var IncomingMessage = class {
  headers;
  rawHeaders;
  trailers;
  rawTrailers;
  httpVersion;
  httpVersionMajor;
  httpVersionMinor;
  method;
  url;
  statusCode;
  statusMessage;
  _body;
  _isBinary;
  _listeners;
  complete;
  aborted;
  socket;
  _bodyConsumed;
  _ended;
  _flowing;
  readable;
  readableEnded;
  readableFlowing;
  destroyed;
  _encoding;
  _closeEmitted;
  constructor(response) {
    const normalizedHeaders = {};
    if (Array.isArray(response?.headers)) {
      response.headers.forEach(([key, value]) => {
        appendNormalizedHeader(normalizedHeaders, key.toLowerCase(), value);
      });
    } else if (response?.headers) {
      Object.entries(response.headers).forEach(([key, value]) => {
        normalizedHeaders[key] = Array.isArray(value) ? [...value] : value;
      });
    }
    this.rawHeaders = Array.isArray(response?.rawHeaders) ? [...response.rawHeaders] : [];
    if (this.rawHeaders.length > 0) {
      this.headers = {};
      for (let index = 0; index < this.rawHeaders.length; index += 2) {
        const key = this.rawHeaders[index];
        const value = this.rawHeaders[index + 1];
        if (key !== void 0 && value !== void 0) {
          appendNormalizedHeader(this.headers, key.toLowerCase(), value);
        }
      }
    } else {
      this.headers = normalizedHeaders;
    }
    if (this.rawHeaders.length === 0 && this.headers && typeof this.headers === "object") {
      Object.entries(this.headers).forEach(([k, v]) => {
        if (Array.isArray(v)) {
          v.forEach((entry) => {
            this.rawHeaders.push(k, entry);
          });
          return;
        }
        this.rawHeaders.push(k, v);
      });
    }
    if (response?.trailers && typeof response.trailers === "object") {
      this.trailers = response.trailers;
      this.rawTrailers = [];
      Object.entries(response.trailers).forEach(([k, v]) => {
        this.rawTrailers.push(k, v);
      });
    } else {
      this.trailers = {};
      this.rawTrailers = [];
    }
    this.httpVersion = "1.1";
    this.httpVersionMajor = 1;
    this.httpVersionMinor = 1;
    this.method = null;
    this.url = response?.url || "";
    this.statusCode = response?.status;
    this.statusMessage = response?.statusText;
    const bodyEncodingHeader = this.headers["x-body-encoding"];
    const bodyEncoding = response?.bodyEncoding || (Array.isArray(bodyEncodingHeader) ? bodyEncodingHeader[0] : bodyEncodingHeader);
    if (bodyEncoding === "base64" && response?.body && typeof Buffer !== "undefined") {
      this._body = Buffer.from(response.body, "base64").toString("binary");
      this._isBinary = true;
    } else {
      this._body = response?.body || "";
      this._isBinary = false;
    }
    this._listeners = {};
    this.complete = false;
    this.aborted = false;
    this.socket = null;
    this._bodyConsumed = false;
    this._ended = false;
    this._flowing = false;
    this.readable = true;
    this.readableEnded = false;
    this.readableFlowing = null;
    this.destroyed = false;
    this._closeEmitted = false;
  }
  on(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(listener);
    if (event === "data" && !this._bodyConsumed) {
      this._flowing = true;
      this.readableFlowing = true;
      Promise.resolve().then(() => {
        if (!this._bodyConsumed) {
          this._bodyConsumed = true;
          if (this._body && this._body.length > 0) {
            let buf;
            if (typeof Buffer !== "undefined") {
              buf = this._isBinary ? Buffer.from(this._body, "binary") : Buffer.from(this._body);
            } else {
              buf = this._body;
            }
            this.emit("data", buf);
          }
          Promise.resolve().then(() => {
            if (!this._ended) {
              this._ended = true;
              this.complete = true;
              this.readable = false;
              this.readableEnded = true;
              this.emit("end");
            }
          });
        }
      });
    }
    if (event === "end" && this._bodyConsumed && !this._ended) {
      Promise.resolve().then(() => {
        if (!this._ended) {
          this._ended = true;
          this.complete = true;
          this.readable = false;
          this.readableEnded = true;
          listener();
        }
      });
    }
    return this;
  }
  once(event, listener) {
    const wrapper = (...args) => {
      this.off(event, wrapper);
      listener(...args);
    };
    wrapper._originalListener = listener;
    return this.on(event, wrapper);
  }
  off(event, listener) {
    if (this._listeners[event]) {
      const idx = this._listeners[event].findIndex(
        (fn) => fn === listener || fn._originalListener === listener
      );
      if (idx !== -1) this._listeners[event].splice(idx, 1);
    }
    return this;
  }
  removeListener(event, listener) {
    return this.off(event, listener);
  }
  removeAllListeners(event) {
    if (event) {
      delete this._listeners[event];
    } else {
      this._listeners = {};
    }
    return this;
  }
  emit(event, ...args) {
    return dispatchCustomEmitterListeners(this, this._listeners[event], args);
  }
  setEncoding(encoding) {
    this._encoding = encoding;
    return this;
  }
  read(_size) {
    if (this._bodyConsumed) return null;
    this._bodyConsumed = true;
    let buf;
    if (typeof Buffer !== "undefined") {
      buf = this._isBinary ? Buffer.from(this._body, "binary") : Buffer.from(this._body);
    } else {
      buf = this._body;
    }
    Promise.resolve().then(() => {
      if (!this._ended) {
        this._ended = true;
        this.complete = true;
        this.readable = false;
        this.readableEnded = true;
        this.emit("end");
      }
    });
    return buf;
  }
  pipe(dest) {
    let buf;
    if (typeof Buffer !== "undefined") {
      buf = this._isBinary ? Buffer.from(this._body || "", "binary") : Buffer.from(this._body || "");
    } else {
      buf = this._body || "";
    }
    if (typeof dest.write === "function" && (typeof buf === "string" ? buf.length : buf.length) > 0) {
      dest.write(buf);
    }
    if (typeof dest.end === "function") {
      Promise.resolve().then(() => dest.end());
    }
    this._bodyConsumed = true;
    this._ended = true;
    this.complete = true;
    this.readable = false;
    this.readableEnded = true;
    return dest;
  }
  pause() {
    this._flowing = false;
    this.readableFlowing = false;
    return this;
  }
  resume() {
    this._flowing = true;
    this.readableFlowing = true;
    if (!this._bodyConsumed) {
      Promise.resolve().then(() => {
        if (!this._bodyConsumed) {
          this._bodyConsumed = true;
          if (this._body) {
            let buf;
            if (typeof Buffer !== "undefined") {
              buf = this._isBinary ? Buffer.from(this._body, "binary") : Buffer.from(this._body);
            } else {
              buf = this._body;
            }
            this.emit("data", buf);
          }
          Promise.resolve().then(() => {
            if (!this._ended) {
              this._ended = true;
              this.complete = true;
              this.readable = false;
              this.readableEnded = true;
              this.emit("end");
            }
          });
        }
      });
    }
    return this;
  }
  unpipe(_dest) {
    return this;
  }
  destroy(err) {
    this.destroyed = true;
    this.readable = false;
    if (err) this.emit("error", err);
    this._emitClose();
    return this;
  }
  _abort(err = createConnResetError("aborted")) {
    if (this.aborted) {
      return;
    }
    this.aborted = true;
    this.complete = false;
    this.destroyed = true;
    this.readable = false;
    this.readableEnded = true;
    this.emit("aborted");
    if (err) {
      this.emit("error", err);
    }
    this._emitClose();
  }
  _emitClose() {
    if (this._closeEmitted) {
      return;
    }
    this._closeEmitted = true;
    this.emit("close");
  }
  [Symbol.asyncIterator]() {
    const self = this;
    let dataEmitted = false;
    let ended = false;
    return {
      async next() {
        if (ended || self._ended) {
          return { done: true, value: void 0 };
        }
        if (!dataEmitted && !self._bodyConsumed) {
          dataEmitted = true;
          self._bodyConsumed = true;
          let buf;
          if (typeof Buffer !== "undefined") {
            buf = self._isBinary ? Buffer.from(self._body || "", "binary") : Buffer.from(self._body || "");
          } else {
            buf = self._body || "";
          }
          return { done: false, value: buf };
        }
        ended = true;
        self._ended = true;
        self.complete = true;
        self.readable = false;
        self.readableEnded = true;
        return { done: true, value: void 0 };
      },
      return() {
        ended = true;
        return Promise.resolve({ done: true, value: void 0 });
      },
      throw(err) {
        ended = true;
        self.emit("error", err);
        return Promise.resolve({ done: true, value: void 0 });
      }
    };
  }
};
var ClientRequest = class {
  _options;
  _callback;
  _listeners = {};
  _headers = {};
  _rawHeaderNames = /* @__PURE__ */ new Map();
  _body = "";
  _bodyBytes = 0;
  _ended = false;
  _agent;
  _hostKey;
  _socketEndListener = null;
  _socketCloseListener = null;
  _loopbackAbort;
  _response = null;
  _closeEmitted = false;
  _abortEmitted = false;
  _signalAbortHandler;
  _signalPollTimer = null;
  _skipExecute = false;
  _destroyError;
  _errorEmitted = false;
  socket;
  finished = false;
  aborted = false;
  destroyed = false;
  path;
  method;
  reusedSocket = false;
  timeoutCb;
  constructor(options, callback) {
    const normalizedMethod = validateRequestMethod(options.method);
    this._options = {
      ...options,
      method: normalizedMethod,
      path: validateRequestPath(options.path)
    };
    this._callback = callback;
    this._validateTimeoutOption();
    this._setOutgoingHeaders(options.headers);
    if (!this._headers.host) {
      this._setHeaderValue("Host", buildHostHeader(this._options));
    }
    this.path = String(this._options.path || "/");
    this.method = String(this._options.method || "GET").toUpperCase();
    const agentOpt = this._options.agent;
    if (agentOpt === false) {
      this._agent = null;
    } else if (agentOpt instanceof Agent) {
      this._agent = agentOpt;
    } else if (this._options._agentOSDefaultAgent instanceof Agent) {
      this._agent = this._options._agentOSDefaultAgent;
    } else {
      this._agent = null;
    }
    this._hostKey = this._agent ? this._agent._getHostKey(this._options) : "";
    this._bindAbortSignal();
    if (typeof this._options.timeout === "number") {
      this.setTimeout(this._options.timeout);
    }
    Promise.resolve().then(() => this._execute());
  }
  _assignSocket(socket, reusedSocket) {
    this.socket = socket;
    this.reusedSocket = reusedSocket;
    const trackedSocket = socket;
    if (!trackedSocket._agentPermanentListenersInstalled) {
      trackedSocket._agentPermanentListenersInstalled = true;
      socket.on("error", () => {
      });
      socket.on("end", () => {
      });
    }
    this._socketEndListener = () => {
    };
    socket.on("end", this._socketEndListener);
    this._socketCloseListener = () => {
      this.destroyed = true;
      this._clearTimeout();
      this._emitClose();
    };
    socket.on("close", this._socketCloseListener);
    this._applyTimeoutToSocket(socket);
    this._emit("socket", socket);
    if (this.destroyed) {
      if (this._destroyError && !this._errorEmitted) {
        this._errorEmitted = true;
        queueMicrotask(() => {
          this._emit("error", this._destroyError);
        });
      }
      socket.destroy();
      return;
    }
    void this._dispatchWithSocket(socket);
  }
  _handleSocketError(err) {
    this._emit("error", err);
  }
  _finalizeSocket(socket, keepSocketAlive) {
    if (this._socketEndListener) {
      socket.off?.("end", this._socketEndListener);
      socket.removeListener?.("end", this._socketEndListener);
      this._socketEndListener = null;
    }
    if (this._socketCloseListener) {
      socket.off?.("close", this._socketCloseListener);
      socket.removeListener?.("close", this._socketCloseListener);
      this._socketCloseListener = null;
    }
    if (this._agent) {
      this._agent._releaseSocket(this._hostKey, socket, this._options, keepSocketAlive);
    } else if (!socket.destroyed) {
      socket.destroy();
    }
  }
  async _dispatchWithSocket(socket) {
    try {
      const normalizedHeaders = normalizeRequestHeaders(this._options.headers);
      const requestMethod = String(this._options.method || "GET").toUpperCase();
      const bridgeBackedSocket = socket instanceof NetSocket || (typeof socket?._socketId === "string" && socket._socketId.length > 0) || (typeof socket?._socketId === "number" && socket._socketId > 0);
      // Bridge-backed sockets already speak kernel-routed byte streams, so route
      // HTTP requests through the raw serializer instead of undici's dispatcher.
      if (bridgeBackedSocket || socket?._loopbackServer || isRawSocketRequest(requestMethod, normalizedHeaders) || this._options.socketPath || this._agent?.keepAlive === true) {
        await this._dispatchRawSocketRequest(socket, requestMethod, normalizedHeaders);
      } else {
        await this._dispatchUndiciRequest(socket, requestMethod);
      }
    } catch (err) {
      this._clearTimeout();
      this._emit("error", err);
      this._finalizeSocket(socket, false);
    }
  }
  async _dispatchUndiciRequest(socket, requestMethod) {
    await waitForSocketReadyForProtocol(socket, this._options.protocol || "http:");
    const dispatcher = getUndiciClientForSocket(socket, this._options);
    const bodyBuffer = this._body ? Buffer.from(this._body) : Buffer.alloc(0);
    const headerPairs = buildRawHttpHeaderPairs(this._headers, this._rawHeaderNames);
    if (bodyBuffer.length > 0 && !this._headers["content-length"] && !this._headers["transfer-encoding"]) {
      headerPairs.push(["Content-Length", String(bodyBuffer.length)]);
    }
    const response = await new Promise((resolve, reject) => {
      try {
        undiciRequest.call(dispatcher, {
          path: this._options.path || "/",
          method: requestMethod,
          headers: flattenHeaderPairs(headerPairs),
          body: bodyBuffer.length > 0 ? bodyBuffer : null,
          signal: this._options.signal,
          responseHeaders: "raw"
        }, (err, result) => {
          if (err) {
            reject(err);
            return;
          }
          resolve(result);
        });
      } catch (error) {
        reject(error);
      }
    });
    const responseBody = await readUndiciReadableBody(response?.body);
    await new Promise((resolve) => {
      queueMicrotask(resolve);
    });
    this.finished = true;
    this._clearTimeout();
    const res = new IncomingMessage({
      status: response?.statusCode,
      statusText: response?.statusText,
      headers: Array.isArray(response?.headers) ? response.headers : [],
      rawHeaders: Array.isArray(response?.headers) ? response.headers : [],
      trailers: response?.trailers && typeof response.trailers === "object" ? response.trailers : {},
      body: responseBody.length > 0 ? responseBody.toString("base64") : "",
      bodyEncoding: "base64",
      url: this._buildUrl()
    });
    this._response = res;
    res.socket = socket;
    res.once("end", () => {
      process.nextTick(() => {
        this._finalizeSocket(socket, this._agent?.keepAlive === true && !this.aborted);
      });
    });
    if (this._callback) {
      this._callback(res);
    }
    this._emit("response", res);
    if (!this._callback && this._listenerCount("response") === 0) {
      queueMicrotask(() => {
        res.resume();
      });
    }
  }
  async _dispatchRawSocketRequest(socket, requestMethod, normalizedHeaders) {
    const protocol = this._options.protocol || "http:";
    await waitForSocketReadyForProtocol(socket, protocol);
    const bodyBuffer = this._body ? Buffer.from(this._body) : Buffer.alloc(0);
    const headerPairs = buildRawHttpHeaderPairs(this._headers, this._rawHeaderNames);
    if (bodyBuffer.length > 0 && !normalizedHeaders["content-length"] && !normalizedHeaders["transfer-encoding"]) {
      headerPairs.push(["Content-Length", String(bodyBuffer.length)]);
    }
    const requestBuffer = serializeRawHttpRequest(
      requestMethod,
      this._options.path || "/",
      headerPairs,
      bodyBuffer
    );
    const timeoutMs = typeof this._options.timeout === "number" && this._options.timeout > 0 ? this._options.timeout : 3e4;
    const responsePromise = waitForRawHttpResponse(socket, requestMethod, timeoutMs);
    socket.write(requestBuffer);
    const response = await responsePromise;
    this.finished = true;
    this._clearTimeout();
    if (response.status === 101) {
      const res2 = new IncomingMessage({
        status: response.status,
        statusText: response.statusText,
        headers: response.headers,
        rawHeaders: response.rawHeaders,
        body: "",
        bodyEncoding: "base64",
        url: this._buildUrl()
      });
      this._response = res2;
      res2.socket = socket;
      const head = response.head ?? Buffer.alloc(0);
      if (this._listenerCount("upgrade") === 0) {
        socket.destroy();
        return;
      }
      this._emit("upgrade", res2, socket, head);
      return;
    }
    if (requestMethod === "CONNECT") {
      const res2 = new IncomingMessage({
        status: response.status,
        statusText: response.statusText,
        headers: response.headers,
        rawHeaders: response.rawHeaders,
        body: "",
        bodyEncoding: "base64",
        url: this._buildUrl()
      });
      this._response = res2;
      res2.socket = socket;
      const head = response.head ?? Buffer.alloc(0);
      this._emit("connect", res2, socket, head);
      return;
    }
    const res = new IncomingMessage({
      status: response.status,
      statusText: response.statusText,
      headers: response.headers,
      rawHeaders: response.rawHeaders,
      body: response.body && response.body.length > 0 ? response.body.toString("base64") : "",
      bodyEncoding: "base64",
      url: this._buildUrl()
    });
    this._response = res;
    res.socket = socket;
    res.once("end", () => {
      process.nextTick(() => {
        this._finalizeSocket(socket, this._agent?.keepAlive === true && !this.aborted);
      });
    });
    if (this._callback) {
      this._callback(res);
    }
    this._emit("response", res);
    if (!this._callback && this._listenerCount("response") === 0) {
      queueMicrotask(() => {
        res.resume();
      });
    }
  }
  _execute() {
    if (this._skipExecute) {
      return;
    }
    if (this._agent) {
      this._agent.addRequest(this, this._options);
      return;
    }
    const finish = (socket) => {
      if (!socket) {
        this._handleSocketError(new Error("Failed to create socket"));
        this._emitClose();
        return;
      }
      this._assignSocket(socket, false);
    };
    const createConnection = this._options.createConnection;
    if (typeof createConnection === "function") {
      const maybeSocket = createConnection(this._options, (_err, socket) => {
        finish(socket);
      });
      finish(maybeSocket);
      return;
    }
    finish(createHttpRequestSocket(this._options));
  }
  _buildUrl() {
    const opts = this._options;
    const protocol = opts.protocol || (opts.port === 443 ? "https:" : "http:");
    const host = opts.hostname || opts.host || "localhost";
    const port = opts.port ? ":" + opts.port : "";
    const path = opts.path || "/";
    return protocol + "//" + host + port + path;
  }
  on(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(listener);
    return this;
  }
  addListener(event, listener) {
    return this.on(event, listener);
  }
  once(event, listener) {
    const wrapper = (...args) => {
      this.off(event, wrapper);
      listener(...args);
    };
    wrapper.listener = listener;
    return this.on(event, wrapper);
  }
  off(event, listener) {
    if (this._listeners[event]) {
      const idx = this._listeners[event].findIndex(
        (registered) => registered === listener || registered.listener === listener
      );
      if (idx !== -1) this._listeners[event].splice(idx, 1);
    }
    return this;
  }
  removeListener(event, listener) {
    return this.off(event, listener);
  }
  getHeader(name) {
    if (typeof name !== "string") {
      throw createTypeErrorWithCode(
        `The "name" argument must be of type string. Received ${formatReceivedType(name)}`,
        "ERR_INVALID_ARG_TYPE"
      );
    }
    return this._headers[name.toLowerCase()];
  }
  getHeaders() {
    const headers = /* @__PURE__ */ Object.create(null);
    for (const [key, value] of Object.entries(this._headers)) {
      headers[key] = Array.isArray(value) ? [...value] : value;
    }
    return headers;
  }
  getHeaderNames() {
    return Object.keys(this._headers);
  }
  getRawHeaderNames() {
    return Object.keys(this._headers).map((key) => this._rawHeaderNames.get(key) || key);
  }
  hasHeader(name) {
    if (typeof name !== "string") {
      throw createTypeErrorWithCode(
        `The "name" argument must be of type string. Received ${formatReceivedType(name)}`,
        "ERR_INVALID_ARG_TYPE"
      );
    }
    return Object.prototype.hasOwnProperty.call(this._headers, name.toLowerCase());
  }
  removeHeader(name) {
    if (typeof name !== "string") {
      throw createTypeErrorWithCode(
        `The "name" argument must be of type string. Received ${formatReceivedType(name)}`,
        "ERR_INVALID_ARG_TYPE"
      );
    }
    const lowerName = name.toLowerCase();
    delete this._headers[lowerName];
    this._rawHeaderNames.delete(lowerName);
    this._options.headers = { ...this._headers };
  }
  _emit(event, ...args) {
    dispatchCustomEmitterListeners(this, this._listeners[event], args);
  }
  _listenerCount(event) {
    return this._listeners[event]?.length || 0;
  }
  _setOutgoingHeaders(headers) {
    this._headers = {};
    this._rawHeaderNames = /* @__PURE__ */ new Map();
    if (!headers) {
      this._options.headers = {};
      return;
    }
    if (Array.isArray(headers)) {
      for (let index = 0; index < headers.length; index += 2) {
        const key = headers[index];
        const value = headers[index + 1];
        if (key !== void 0 && value !== void 0) {
          this._setHeaderValue(String(key), value);
        }
      }
      return;
    }
    Object.entries(headers).forEach(([key, value]) => {
      if (value !== void 0) {
        this._setHeaderValue(key, value);
      }
    });
  }
  _setHeaderValue(name, value) {
    const actualName = validateHeaderName(name).toLowerCase();
    validateHeaderValue(actualName, value);
    this._headers[actualName] = Array.isArray(value) ? value.map((entry) => String(entry)) : String(value);
    if (!this._rawHeaderNames.has(actualName)) {
      this._rawHeaderNames.set(actualName, name);
    }
    this._options.headers = { ...this._headers };
  }
  write(data) {
    const addedBytes = typeof Buffer !== "undefined" ? Buffer.byteLength(data) : data.length;
    if (this._bodyBytes + addedBytes > MAX_HTTP_BODY_BYTES) {
      throw new Error("ERR_HTTP_BODY_TOO_LARGE: request body exceeds " + MAX_HTTP_BODY_BYTES + " byte limit");
    }
    this._body += data;
    this._bodyBytes += addedBytes;
    return true;
  }
  end(data) {
    if (data) this.write(data);
    this._ended = true;
    return this;
  }
  abort() {
    if (this.aborted) {
      return;
    }
    this.aborted = true;
    if (!this._abortEmitted) {
      this._abortEmitted = true;
      queueMicrotask(() => {
        this._emit("abort");
      });
    }
    this._loopbackAbort?.();
    this.destroy();
  }
  destroy(err) {
    if (this.destroyed) {
      return this;
    }
    this.destroyed = true;
    this._clearTimeout();
    this._unbindAbortSignal();
    this._loopbackAbort?.();
    this._loopbackAbort = void 0;
    if (!this.socket && err && err.code === "ABORT_ERR") {
      this._skipExecute = true;
    }
    const responseStarted = this._response != null;
    const destroyError = err ?? (!this.aborted && !responseStarted ? createConnResetError() : void 0);
    this._destroyError = destroyError;
    if (this._response && !this._response.complete && !this._response.aborted) {
      this._response._abort(destroyError ?? createConnResetError("aborted"));
    }
    if (this.socket && !this.socket.destroyed) {
      if (destroyError && !this._errorEmitted) {
        this._errorEmitted = true;
        queueMicrotask(() => {
          this._emit("error", destroyError);
        });
      }
      this.socket.destroy(destroyError);
    } else {
      if (destroyError) {
        this._errorEmitted = true;
        queueMicrotask(() => {
          this._emit("error", destroyError);
        });
      }
      queueMicrotask(() => {
        this._emitClose();
      });
    }
    return this;
  }
  setTimeout(timeout, callback) {
    if (callback) {
      this.once("timeout", callback);
    }
    this.timeoutCb = () => {
      this._emit("timeout");
    };
    this._clearTimeout();
    if (timeout === 0) {
      return this;
    }
    if (!Number.isFinite(timeout) || timeout < 0) {
      throw new TypeError(`The "timeout" argument must be of type number. Received ${String(timeout)}`);
    }
    this._options.timeout = timeout;
    if (this.socket) {
      this._applyTimeoutToSocket(this.socket);
    }
    return this;
  }
  setNoDelay() {
    return this;
  }
  setSocketKeepAlive() {
    return this;
  }
  flushHeaders() {
  }
  _emitClose() {
    if (this._closeEmitted) {
      return;
    }
    this._closeEmitted = true;
    this._emit("close");
  }
  _applyTimeoutToSocket(socket) {
    const timeout = this._options.timeout;
    if (typeof timeout !== "number" || timeout === 0) {
      return;
    }
    if (!this.timeoutCb) {
      this.timeoutCb = () => {
        this._emit("timeout");
      };
    }
    socket.off?.("timeout", this.timeoutCb);
    socket.removeListener?.("timeout", this.timeoutCb);
    socket.setTimeout?.(timeout, this.timeoutCb);
  }
  _validateTimeoutOption() {
    const timeout = this._options.timeout;
    if (timeout === void 0) {
      return;
    }
    if (typeof timeout !== "number") {
      const received = timeout === null ? "null" : typeof timeout === "string" ? `type string ('${timeout}')` : `type ${typeof timeout} (${JSON.stringify(timeout)})`;
      const error = new TypeError(`The "timeout" argument must be of type number. Received ${received}`);
      error.code = "ERR_INVALID_ARG_TYPE";
      throw error;
    }
  }
  _bindAbortSignal() {
    const signal = this._options.signal;
    if (!signal) {
      return;
    }
    this._signalAbortHandler = () => {
      this.destroy(createAbortError2());
    };
    if (signal.aborted) {
      this.destroyed = true;
      this._skipExecute = true;
      queueMicrotask(() => {
        this._emit("error", createAbortError2());
        this._emitClose();
      });
      return;
    }
    if (typeof signal.addEventListener === "function") {
      signal.addEventListener("abort", this._signalAbortHandler, { once: true });
      return;
    }
    const signalWithOnAbort = signal;
    signalWithOnAbort.__secureExecPrevOnAbort__ = signalWithOnAbort.onabort ?? null;
    signalWithOnAbort.onabort = ((event) => {
      signalWithOnAbort.__secureExecPrevOnAbort__?.call(signal, event);
      this._signalAbortHandler?.();
    });
    this._startAbortSignalPoll(signal);
  }
  _unbindAbortSignal() {
    const signal = this._options.signal;
    if (!signal || !this._signalAbortHandler) {
      return;
    }
    if (this._signalPollTimer) {
      clearTimeout(this._signalPollTimer);
      this._signalPollTimer = null;
    }
    if (typeof signal.removeEventListener === "function") {
      signal.removeEventListener("abort", this._signalAbortHandler);
      this._signalAbortHandler = void 0;
      return;
    }
    const signalWithOnAbort = signal;
    if (signalWithOnAbort.onabort === this._signalAbortHandler) {
      signalWithOnAbort.onabort = signalWithOnAbort.__secureExecPrevOnAbort__ ?? null;
    } else if (signalWithOnAbort.__secureExecPrevOnAbort__ !== void 0) {
      signalWithOnAbort.onabort = signalWithOnAbort.__secureExecPrevOnAbort__ ?? null;
    }
    delete signalWithOnAbort.__secureExecPrevOnAbort__;
    this._signalAbortHandler = void 0;
  }
  _startAbortSignalPoll(signal) {
    const poll = () => {
      if (this.destroyed) {
        this._signalPollTimer = null;
        return;
      }
      if (signal.aborted) {
        this._signalPollTimer = null;
        this._signalAbortHandler?.();
        return;
      }
      this._signalPollTimer = setTimeout(poll, 5);
    };
    if (!this._signalPollTimer) {
      this._signalPollTimer = setTimeout(poll, 5);
    }
  }
  _clearTimeout() {
    if (this.socket && this.timeoutCb) {
      this.socket.off?.("timeout", this.timeoutCb);
      this.socket.removeListener?.("timeout", this.timeoutCb);
    }
    if (this.socket?.setTimeout) {
      this.socket.setTimeout(0);
    }
  }
};
function createUnsupportedHttpSocketWriteError(surface) {
  return createErrorWithCode(
    `${surface}.write() is not implemented by the secure-exec http compatibility layer`,
    "ERR_NOT_IMPLEMENTED"
  );
}
var FakeSocket = class {
  remoteAddress;
  remotePort;
  localAddress = "127.0.0.1";
  localPort = 0;
  connecting = false;
  destroyed = false;
  writable = true;
  readable = true;
  timeout = 0;
  _listeners = {};
  _closed = false;
  _closeScheduled = false;
  _timeoutTimer = null;
  _freeTimer = null;
  constructor(options) {
    this.remoteAddress = options?.host || "127.0.0.1";
    this.remotePort = options?.port || 80;
  }
  setTimeout(ms, cb) {
    this.timeout = ms;
    if (cb) {
      this.on("timeout", cb);
    }
    if (this._timeoutTimer) {
      clearTimeout(this._timeoutTimer);
      this._timeoutTimer = null;
    }
    if (ms > 0) {
      this._timeoutTimer = setTimeout(() => {
        this.emit("timeout");
      }, ms);
    }
    return this;
  }
  setNoDelay(_noDelay) {
    return this;
  }
  setKeepAlive(_enable, _delay) {
    return this;
  }
  on(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(listener);
    return this;
  }
  once(event, listener) {
    const wrapper = (...args) => {
      this.off(event, wrapper);
      listener.call(this, ...args);
    };
    return this.on(event, wrapper);
  }
  off(event, listener) {
    if (this._listeners[event]) {
      const idx = this._listeners[event].indexOf(listener);
      if (idx !== -1) this._listeners[event].splice(idx, 1);
    }
    return this;
  }
  removeListener(event, listener) {
    return this.off(event, listener);
  }
  removeAllListeners(event) {
    if (event) {
      delete this._listeners[event];
    } else {
      this._listeners = {};
    }
    return this;
  }
  emit(event, ...args) {
    const handlers = this._listeners[event];
    return dispatchCustomEmitterListeners(this, handlers, args);
  }
  listenerCount(event) {
    return this._listeners[event]?.length || 0;
  }
  listeners(event) {
    return [...this._listeners[event] || []];
  }
  write(_data, _encodingOrCallback, _callback) {
    throw createUnsupportedHttpSocketWriteError("http.ClientRequest.socket");
  }
  end() {
    if (this.destroyed || this._closed) return this;
    this.writable = false;
    queueMicrotask(() => {
      if (this.destroyed || this._closed) return;
      this.readable = false;
      this.emit("end");
      this.destroy();
    });
    return this;
  }
  destroy() {
    if (this.destroyed || this._closed) return this;
    this.destroyed = true;
    this._closed = true;
    this.writable = false;
    this.readable = false;
    if (this._timeoutTimer) {
      clearTimeout(this._timeoutTimer);
      this._timeoutTimer = null;
    }
    if (!this._closeScheduled) {
      this._closeScheduled = true;
      queueMicrotask(() => {
        this._closeScheduled = false;
        this.emit("close");
      });
    }
    return this;
  }
};
var DirectTunnelSocket = class {
  remoteAddress;
  remotePort;
  localAddress = "127.0.0.1";
  localPort = 0;
  connecting = false;
  destroyed = false;
  writable = true;
  readable = true;
  readyState = "open";
  bytesWritten = 0;
  _listeners = {};
  _encoding;
  _peer = null;
  _readableState = { endEmitted: false, ended: false };
  _writableState = { finished: false, errorEmitted: false };
  constructor(options) {
    this.remoteAddress = options?.host || "127.0.0.1";
    this.remotePort = options?.port || 80;
  }
  _attachPeer(peer) {
    this._peer = peer;
  }
  setTimeout(_ms, _cb) {
    return this;
  }
  setNoDelay(_noDelay) {
    return this;
  }
  setKeepAlive(_enable, _delay) {
    return this;
  }
  setEncoding(encoding) {
    this._encoding = encoding;
    return this;
  }
  ref() {
    return this;
  }
  unref() {
    return this;
  }
  cork() {
  }
  uncork() {
  }
  pause() {
    return this;
  }
  resume() {
    return this;
  }
  address() {
    return { address: this.localAddress, family: "IPv4", port: this.localPort };
  }
  on(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(listener);
    return this;
  }
  once(event, listener) {
    const wrapper = (...args) => {
      this.off(event, wrapper);
      listener.call(this, ...args);
    };
    return this.on(event, wrapper);
  }
  off(event, listener) {
    const listeners = this._listeners[event];
    if (!listeners) return this;
    const index = listeners.indexOf(listener);
    if (index !== -1) listeners.splice(index, 1);
    return this;
  }
  removeListener(event, listener) {
    return this.off(event, listener);
  }
  removeAllListeners(event) {
    if (event) {
      delete this._listeners[event];
    } else {
      this._listeners = {};
    }
    return this;
  }
  emit(event, ...args) {
    const listeners = this._listeners[event];
    return dispatchCustomEmitterListeners(this, listeners, args);
  }
  listenerCount(event) {
    return this._listeners[event]?.length || 0;
  }
  write(data, encodingOrCb, cb) {
    if (this.destroyed || !this._peer) return false;
    const callback = typeof encodingOrCb === "function" ? encodingOrCb : cb;
    const buffer = normalizeSocketChunk(data);
    this.bytesWritten += buffer.length;
    queueMicrotask(() => {
      this._peer?._pushData(buffer);
    });
    callback?.();
    return true;
  }
  end(data) {
    if (data !== void 0) {
      this.write(data);
    }
    this.writable = false;
    this._writableState.finished = true;
    queueMicrotask(() => {
      this._peer?._pushEnd();
    });
    this.emit("finish");
    return this;
  }
  destroy(err) {
    if (this.destroyed) return this;
    this.destroyed = true;
    this.readable = false;
    this.writable = false;
    this._readableState.endEmitted = true;
    this._readableState.ended = true;
    this._writableState.finished = true;
    if (err) {
      this.emit("error", err);
    }
    queueMicrotask(() => {
      this._peer?._pushEnd();
    });
    this.emit("close", false);
    return this;
  }
  _pushData(buffer) {
    if (!this.readable || this.destroyed) {
      return;
    }
    this.emit("data", this._encoding ? buffer.toString(this._encoding) : buffer);
  }
  _pushEnd() {
    if (this.destroyed) {
      return;
    }
    this.readable = false;
    this.writable = false;
    this._readableState.endEmitted = true;
    this._readableState.ended = true;
    this._writableState.finished = true;
    this.emit("end");
    this.emit("close", false);
  }
};
function normalizeSocketChunk(data) {
  if (typeof Buffer !== "undefined" && Buffer.isBuffer(data)) {
    return data;
  }
  if (data instanceof Uint8Array) {
    return Buffer.from(data);
  }
  return Buffer.from(String(data));
}
var Agent = class _Agent {
  static defaultMaxSockets = Infinity;
  options;
  maxSockets;
  maxTotalSockets;
  maxFreeSockets;
  keepAlive;
  keepAliveMsecs;
  timeout;
  requests;
  sockets;
  freeSockets;
  totalSocketCount;
  _listeners = {};
  constructor(options) {
    this.options = { ...options };
    this._validateSocketCountOption("maxSockets", options?.maxSockets);
    this._validateSocketCountOption("maxFreeSockets", options?.maxFreeSockets);
    this._validateSocketCountOption("maxTotalSockets", options?.maxTotalSockets);
    this.keepAlive = options?.keepAlive ?? false;
    this.keepAliveMsecs = options?.keepAliveMsecs ?? 1e3;
    this.maxSockets = options?.maxSockets ?? _Agent.defaultMaxSockets;
    this.maxTotalSockets = options?.maxTotalSockets ?? Infinity;
    this.maxFreeSockets = options?.maxFreeSockets ?? 256;
    this.timeout = options?.timeout ?? -1;
    this.requests = {};
    this.sockets = {};
    this.freeSockets = {};
    this.totalSocketCount = 0;
  }
  _validateSocketCountOption(name, value) {
    if (value === void 0) return;
    if (typeof value !== "number") {
      const received = typeof value === "string" ? `type string ('${value}')` : `type ${typeof value} (${JSON.stringify(value)})`;
      const err = new TypeError(
        `The "${name}" argument must be of type number. Received ${received}`
      );
      err.code = "ERR_INVALID_ARG_TYPE";
      throw err;
    }
    if (Number.isNaN(value) || value <= 0) {
      const err = new RangeError(
        `The value of "${name}" is out of range. It must be > 0. Received ${String(value)}`
      );
      err.code = "ERR_OUT_OF_RANGE";
      throw err;
    }
  }
  getName(options) {
    const host = options?.hostname || options?.host || "localhost";
    const port = options?.port ?? "";
    const localAddress = options?.localAddress ?? "";
    let suffix = "";
    if (options?.socketPath) {
      suffix = `:${options.socketPath}`;
    } else if (options?.family === 4 || options?.family === 6) {
      suffix = `:${options.family}`;
    }
    return `${host}:${port}:${localAddress}${suffix}`;
  }
  _getHostKey(options) {
    return this.getName(options);
  }
  on(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(listener);
    return this;
  }
  once(event, listener) {
    const wrapper = (...args) => {
      this.off(event, wrapper);
      listener(...args);
    };
    return this.on(event, wrapper);
  }
  off(event, listener) {
    const listeners = this._listeners[event];
    if (!listeners) return this;
    const index = listeners.indexOf(listener);
    if (index !== -1) listeners.splice(index, 1);
    return this;
  }
  removeListener(event, listener) {
    return this.off(event, listener);
  }
  emit(event, ...args) {
    const listeners = this._listeners[event];
    return dispatchCustomEmitterListeners(this, listeners, args);
  }
  createConnection(options, cb) {
    const createConnection = typeof options.createConnection === "function" ? options.createConnection : typeof this.options.createConnection === "function" ? this.options.createConnection : null;
    if (createConnection) {
      return createConnection(
        options,
        cb ?? (() => void 0)
      );
    }
    return createHttpRequestSocket(options, cb);
  }
  addRequest(request, options) {
    const name = this.getName(options);
    const freeSocket = this._takeFreeSocket(name);
    if (freeSocket) {
      this._activateSocket(name, freeSocket);
      request._assignSocket(freeSocket, true);
      return;
    }
    if (this._canCreateSocket(name)) {
      this._createSocketForRequest(name, request, options);
      return;
    }
    if (!this.requests[name]) {
      this.requests[name] = [];
    }
    this.requests[name].push({ request, options });
  }
  _releaseSocket(name, socket, options, keepSocketAlive) {
    const removedActive = this._removeSocket(this.sockets, name, socket);
    if (keepSocketAlive && !socket.destroyed) {
      const freeList = this.freeSockets[name] ?? (this.freeSockets[name] = []);
      if (freeList.length < this.maxFreeSockets) {
        if (socket._freeTimer) {
          clearTimeout(socket._freeTimer);
          socket._freeTimer = null;
        }
        freeList.push(socket);
        if (this.timeout > 0) {
          socket._freeTimer = setTimeout(() => {
            socket._freeTimer = null;
            socket.destroy();
          }, this.timeout);
        }
        socket.emit("free");
        this.emit("free", socket, options);
      } else {
        if (removedActive) {
          this.totalSocketCount = Math.max(0, this.totalSocketCount - 1);
        }
        socket.destroy();
      }
    } else if (!socket.destroyed) {
      if (removedActive) {
        this.totalSocketCount = Math.max(0, this.totalSocketCount - 1);
      }
      socket.destroy();
    }
    Promise.resolve().then(() => this._processPendingRequests());
  }
  _removeSocketCompletely(name, socket) {
    if (socket._freeTimer) {
      clearTimeout(socket._freeTimer);
      socket._freeTimer = null;
    }
    const removed = this._removeSocket(this.sockets, name, socket) || this._removeSocket(this.freeSockets, name, socket);
    if (removed) {
      this.totalSocketCount = Math.max(0, this.totalSocketCount - 1);
      Promise.resolve().then(() => this._processPendingRequests());
    }
  }
  _canCreateSocket(name) {
    const activeCount = this.sockets[name]?.length ?? 0;
    if (activeCount >= this.maxSockets) {
      return false;
    }
    if (this.totalSocketCount < this.maxTotalSockets) {
      return true;
    }
    this._evictFreeSocket(name);
    return this.totalSocketCount < this.maxTotalSockets;
  }
  _takeFreeSocket(name) {
    const freeList = this.freeSockets[name];
    while (freeList && freeList.length > 0) {
      const socket = freeList.shift();
      if (!socket.destroyed) {
        if (socket._freeTimer) {
          clearTimeout(socket._freeTimer);
          socket._freeTimer = null;
        }
        if (freeList.length === 0) delete this.freeSockets[name];
        return socket;
      }
      this.totalSocketCount = Math.max(0, this.totalSocketCount - 1);
    }
    if (freeList && freeList.length === 0) {
      delete this.freeSockets[name];
    }
    return null;
  }
  _activateSocket(name, socket) {
    const activeList = this.sockets[name] ?? (this.sockets[name] = []);
    activeList.push(socket);
  }
  _createSocketForRequest(name, request, options) {
    let settled = false;
    const finish = (err, socket) => {
      if (settled) return;
      settled = true;
      if (err || !socket) {
        request._handleSocketError(err ?? new Error("Failed to create socket"));
        this._processPendingRequests();
        return;
      }
      if (request.destroyed) {
        this.totalSocketCount += 1;
        this._activateSocket(name, socket);
        socket.once("close", () => {
          this._removeSocketCompletely(name, socket);
        });
        request._assignSocket(socket, false);
        return;
      }
      this.totalSocketCount += 1;
      this._activateSocket(name, socket);
      socket.once("close", () => {
        this._removeSocketCompletely(name, socket);
      });
      request._assignSocket(socket, false);
    };
    const connectionOptions = {
      ...options,
      keepAlive: this.keepAlive,
      keepAliveInitialDelay: this.keepAliveMsecs
    };
    try {
      const maybeSocket = this.createConnection(connectionOptions, (err, socket) => {
        finish(err, socket);
      });
      if (maybeSocket) {
        finish(null, maybeSocket);
      }
    } catch (err) {
      finish(err instanceof Error ? err : new Error(String(err)));
    }
  }
  _processPendingRequests() {
    for (const name of Object.keys(this.requests)) {
      const queue = this.requests[name];
      while (queue && queue.length > 0) {
        const freeSocket = this._takeFreeSocket(name);
        if (freeSocket) {
          const entry2 = queue.shift();
          if (entry2.request.destroyed) {
            this._activateSocket(name, freeSocket);
            this._releaseSocket(name, freeSocket, entry2.options, true);
            continue;
          }
          this._activateSocket(name, freeSocket);
          entry2.request._assignSocket(freeSocket, true);
          continue;
        }
        if (!this._canCreateSocket(name)) {
          break;
        }
        const entry = queue.shift();
        if (entry.request.destroyed) {
          continue;
        }
        this._createSocketForRequest(name, entry.request, entry.options);
      }
      if (!queue || queue.length === 0) {
        delete this.requests[name];
      }
    }
  }
  _removeSocket(sockets, name, socket) {
    const list = sockets[name];
    if (!list) return false;
    const index = list.indexOf(socket);
    if (index === -1) return false;
    list.splice(index, 1);
    if (list.length === 0) delete sockets[name];
    return true;
  }
  _evictFreeSocket(preferredName) {
    const keys = Object.keys(this.freeSockets);
    const orderedKeys = keys.includes(preferredName) ? [...keys.filter((key) => key !== preferredName), preferredName] : keys;
    for (const key of orderedKeys) {
      const socket = this.freeSockets[key]?.[0];
      if (!socket) continue;
      socket.destroy();
      return;
    }
  }
  destroy() {
    for (const socket of Object.values(this.sockets).flat()) {
      socket.destroy();
    }
    for (const socket of Object.values(this.freeSockets).flat()) {
      socket.destroy();
    }
    this.requests = {};
    this.sockets = {};
    this.freeSockets = {};
    this.totalSocketCount = 0;
  }
};
function debugBridgeNetwork(...args) {
  if (process.env.SECURE_EXEC_DEBUG_HTTP_BRIDGE === "1") {
    console.error("[secure-exec bridge network]", ...args);
  }
}
var nextServerId = 1;
var serverInstances = /* @__PURE__ */ new Map();
var HTTP_METHODS = [
  "ACL",
  "BIND",
  "CHECKOUT",
  "CONNECT",
  "COPY",
  "DELETE",
  "GET",
  "HEAD",
  "LINK",
  "LOCK",
  "M-SEARCH",
  "MERGE",
  "MKACTIVITY",
  "MKCALENDAR",
  "MKCOL",
  "MOVE",
  "NOTIFY",
  "OPTIONS",
  "PATCH",
  "POST",
  "PROPFIND",
  "PROPPATCH",
  "PURGE",
  "PUT",
  "QUERY",
  "REBIND",
  "REPORT",
  "SEARCH",
  "SOURCE",
  "SUBSCRIBE",
  "TRACE",
  "UNBIND",
  "UNLINK",
  "UNLOCK",
  "UNSUBSCRIBE"
];
var INVALID_REQUEST_PATH_REGEXP = /[^\u0021-\u00ff]/;
var HTTP_TOKEN_EXTRA_CHARS = /* @__PURE__ */ new Set(["!", "#", "$", "%", "&", "'", "*", "+", "-", ".", "^", "_", "`", "|", "~"]);
function createTypeErrorWithCode(message, code) {
  const error = new TypeError(message);
  error.code = code;
  return error;
}
function createErrorWithCode(message, code) {
  const error = new Error(message);
  error.code = code;
  return error;
}
function formatReceivedType(value) {
  if (value === null) {
    return "null";
  }
  if (Array.isArray(value)) {
    return "an instance of Array";
  }
  const valueType = typeof value;
  if (valueType === "function") {
    const name = typeof value.name === "string" && value.name.length > 0 ? value.name : "anonymous";
    return `function ${name}`;
  }
  if (valueType === "object") {
    const ctorName = value && typeof value === "object" && typeof value.constructor?.name === "string" ? value.constructor.name : "Object";
    return `an instance of ${ctorName}`;
  }
  if (valueType === "string") {
    return `type string ('${String(value)}')`;
  }
  if (valueType === "symbol") {
    return `type symbol (${String(value)})`;
  }
  return `type ${valueType} (${String(value)})`;
}
function createInvalidArgTypeError2(argumentName, expectedType, value) {
  return createTypeErrorWithCode(
    `The "${argumentName}" property must be of type ${expectedType}. Received ${formatReceivedType(value)}`,
    "ERR_INVALID_ARG_TYPE"
  );
}
function checkIsHttpToken(value) {
  if (value.length === 0) {
    return false;
  }
  for (let index = 0; index < value.length; index += 1) {
    const char = value[index];
    const code = value.charCodeAt(index);
    const isAlphaNum = code >= 48 && code <= 57 || code >= 65 && code <= 90 || code >= 97 && code <= 122;
    if (!isAlphaNum && !HTTP_TOKEN_EXTRA_CHARS.has(char)) {
      return false;
    }
  }
  return true;
}
function checkInvalidHeaderChar(value) {
  for (let index = 0; index < value.length; index += 1) {
    const code = value.charCodeAt(index);
    if (code === 9) {
      continue;
    }
    if (code < 32 || code === 127 || code > 255) {
      return true;
    }
  }
  return false;
}
function validateHeaderName(name, label = "Header name") {
  const actualName = String(name);
  if (!checkIsHttpToken(actualName)) {
    throw createTypeErrorWithCode(
      `${label} must be a valid HTTP token [${JSON.stringify(actualName)}]`,
      "ERR_INVALID_HTTP_TOKEN"
    );
  }
  return actualName;
}
function validateHeaderValue(name, value) {
  if (value === void 0) {
    throw createTypeErrorWithCode(
      `Invalid value "undefined" for header "${name}"`,
      "ERR_HTTP_INVALID_HEADER_VALUE"
    );
  }
  if (Array.isArray(value)) {
    for (const entry of value) {
      validateHeaderValue(name, entry);
    }
    return;
  }
  if (checkInvalidHeaderChar(String(value))) {
    throw createTypeErrorWithCode(
      `Invalid character in header content [${JSON.stringify(name)}]`,
      "ERR_INVALID_CHAR"
    );
  }
}
function serializeHeaderValue(value) {
  if (Array.isArray(value)) {
    return value.map((entry) => String(entry));
  }
  return String(value);
}
function joinHeaderValue(value) {
  return Array.isArray(value) ? value.join(", ") : value;
}
function cloneStoredHeaderValue(value) {
  return Array.isArray(value) ? [...value] : value;
}
function appendNormalizedHeader(target, key, value) {
  if (key === "set-cookie") {
    const existing2 = target[key];
    if (existing2 === void 0) {
      target[key] = [value];
    } else if (Array.isArray(existing2)) {
      existing2.push(value);
    } else {
      target[key] = [existing2, value];
    }
    return;
  }
  const existing = target[key];
  target[key] = existing === void 0 ? value : `${joinHeaderValue(existing)}, ${value}`;
}
function validateRequestMethod(method) {
  if (method == null || method === "") {
    return void 0;
  }
  if (typeof method !== "string") {
    throw createInvalidArgTypeError2("options.method", "string", method);
  }
  return validateHeaderName(method, "Method");
}
function validateRequestPath(path) {
  const resolvedPath = path == null || path === "" ? "/" : String(path);
  if (INVALID_REQUEST_PATH_REGEXP.test(resolvedPath)) {
    throw createTypeErrorWithCode(
      "Request path contains unescaped characters",
      "ERR_UNESCAPED_CHARACTERS"
    );
  }
  return resolvedPath;
}
function buildHostHeader(options) {
  const host = String(options.hostname || options.host || "localhost");
  const defaultPort = options.protocol === "https:" || Number(options.port) === 443 ? 443 : 80;
  const port = options.port != null ? Number(options.port) : defaultPort;
  return port === defaultPort ? host : `${host}:${port}`;
}
function isFlatHeaderList(headers) {
  return Array.isArray(headers) && (headers.length === 0 || typeof headers[0] === "string");
}
function normalizeRequestHeaders(headers) {
  if (!headers) return {};
  if (Array.isArray(headers)) {
    const normalized2 = {};
    for (let i = 0; i < headers.length; i += 2) {
      const key = headers[i];
      const value = headers[i + 1];
      if (key !== void 0 && value !== void 0) {
        const normalizedKey = validateHeaderName(key).toLowerCase();
        validateHeaderValue(normalizedKey, value);
        appendNormalizedHeader(normalized2, normalizedKey, String(value));
      }
    }
    return normalized2;
  }
  const normalized = {};
  Object.entries(headers).forEach(([key, value]) => {
    if (value === void 0) return;
    const normalizedKey = validateHeaderName(key).toLowerCase();
    validateHeaderValue(normalizedKey, value);
    if (Array.isArray(value)) {
      value.forEach((entry) => appendNormalizedHeader(normalized, normalizedKey, String(entry)));
      return;
    }
    appendNormalizedHeader(normalized, normalizedKey, String(value));
  });
  return normalized;
}
function hasUpgradeRequestHeaders(headers) {
  const connectionHeader = joinHeaderValue(headers.connection || "").toLowerCase();
  return connectionHeader.includes("upgrade") && Boolean(headers.upgrade);
}
function isRawSocketRequest(method, headers) {
  if (String(method || "GET").toUpperCase() === "CONNECT") {
    return true;
  }
  return hasUpgradeRequestHeaders(headers);
}
function socketReadyEventNameForProtocol(protocol) {
  return protocol === "https:" ? "secureConnect" : "connect";
}
function isSocketReadyForProtocol(socket, protocol) {
  if (!socket || socket.destroyed === true) {
    return false;
  }
  if (protocol === "https:") {
    return socket.encrypted === true && socket._tlsUpgrading !== true;
  }
  if (socket._connected === true || socket._loopbackServer) {
    return true;
  }
  if (typeof socket._socketId === "number") {
    return false;
  }
  return socket.connecting === false;
}
function waitForSocketReadyForProtocol(socket, protocol) {
  if (isSocketReadyForProtocol(socket, protocol)) {
    return Promise.resolve();
  }
  return new Promise((resolve, reject) => {
    const readyEvent = socketReadyEventNameForProtocol(protocol);
    const onReady = () => {
      cleanup();
      resolve();
    };
    const onError = (error) => {
      cleanup();
      reject(error instanceof Error ? error : new Error(String(error)));
    };
    const onClose = () => {
      cleanup();
      reject(createConnResetError("socket closed before request was ready"));
    };
    const cleanup = () => {
      socket.off?.(readyEvent, onReady);
      socket.removeListener?.(readyEvent, onReady);
      socket.off?.("error", onError);
      socket.removeListener?.("error", onError);
      socket.off?.("close", onClose);
      socket.removeListener?.("close", onClose);
    };
    socket.once(readyEvent, onReady);
    socket.once("error", onError);
    socket.once("close", onClose);
  });
}
function buildUndiciOrigin(options) {
  const protocol = options?.protocol === "https:" ? "https:" : "http:";
  const hostname = String(options?.hostname || options?.host || "localhost");
  const defaultPort = protocol === "https:" ? 443 : 80;
  const port = Number(options?.port) || defaultPort;
  const originUrl = new URL(`${protocol}//${hostname}`);
  if (port !== defaultPort) {
    originUrl.port = String(port);
  }
  return originUrl.origin;
}
function getUndiciClientForSocket(socket, options) {
  if (typeof UndiciClient !== "function" || typeof undiciRequest !== "function") {
    throw new Error("Undici request transport is not available");
  }
  const origin = buildUndiciOrigin(options);
  if (socket._agentOSUndiciClient && socket._agentOSUndiciOrigin === origin && socket._agentOSUndiciClient.destroyed !== true) {
    return socket._agentOSUndiciClient;
  }
  const client = new UndiciClient(origin, {
    pipelining: 1,
    connect(_connectOptions, callback) {
      callback(null, socket);
      return socket;
    }
  });
  const clearClient = () => {
    if (socket._agentOSUndiciClient === client) {
      socket._agentOSUndiciClient = null;
      socket._agentOSUndiciOrigin = null;
    }
  };
  socket.once?.("close", clearClient);
  socket._agentOSUndiciClient = client;
  socket._agentOSUndiciOrigin = origin;
  return client;
}
function createHttpRequestSocket(options, callback) {
  const protocol = options?.protocol === "https:" ? "https:" : "http:";
  const host = String(options?.hostname || options?.host || "localhost");
  const port = Number(options?.port) || (protocol === "https:" ? 443 : 80);
  const socket = protocol === "https:" ? tlsConnect({
    host,
    port,
    servername: options?.servername || host,
    rejectUnauthorized: options?.rejectUnauthorized,
    socket: options?.socket
  }) : netConnect({
    host,
    port,
    path: options?.socketPath,
    keepAlive: options?.keepAlive,
    keepAliveInitialDelay: options?.keepAliveInitialDelay
  });
  if (callback) {
    const readyEvent = socketReadyEventNameForProtocol(protocol);
    const onReady = () => {
      cleanup();
      callback(null, socket);
    };
    const onError = (error) => {
      cleanup();
      callback(error instanceof Error ? error : new Error(String(error)));
    };
    const cleanup = () => {
      socket.off?.(readyEvent, onReady);
      socket.removeListener?.(readyEvent, onReady);
      socket.off?.("error", onError);
      socket.removeListener?.("error", onError);
    };
    socket.once(readyEvent, onReady);
    socket.once("error", onError);
  }
  return socket;
}
function flattenHeaderPairs(headerPairs) {
  const flattened = [];
  for (const [name, value] of headerPairs) {
    flattened.push(name, value);
  }
  return flattened;
}
function buildRawHttpHeaderPairs(headers, rawHeaderNames) {
  const pairs = [];
  Object.entries(headers).forEach(([key, value]) => {
    const rawName = rawHeaderNames.get(key) || key;
    if (Array.isArray(value)) {
      value.forEach((entry) => {
        pairs.push([rawName, String(entry)]);
      });
      return;
    }
    pairs.push([rawName, String(value)]);
  });
  return pairs;
}
function serializeRawHttpRequest(method, path, headerPairs, bodyBuffer) {
  const lines = [`${method} ${path} HTTP/1.1`];
  headerPairs.forEach(([name, value]) => {
    lines.push(`${name}: ${value}`);
  });
  lines.push("", "");
  const headerBuffer = Buffer.from(lines.join("\r\n"), "latin1");
  if (!bodyBuffer || bodyBuffer.length === 0) {
    return headerBuffer;
  }
  return Buffer.concat([headerBuffer, bodyBuffer]);
}
async function readUndiciReadableBody(body) {
  if (!body) {
    return Buffer.alloc(0);
  }
  const chunks = [];
  for await (const chunk of body) {
    if (typeof Buffer !== "undefined" && Buffer.isBuffer(chunk)) {
      chunks.push(chunk);
    } else if (chunk instanceof Uint8Array) {
      chunks.push(Buffer.from(chunk));
    } else {
      chunks.push(Buffer.from(String(chunk)));
    }
  }
  if (chunks.length === 0) {
    return Buffer.alloc(0);
  }
  return chunks.length === 1 ? chunks[0] : Buffer.concat(chunks);
}
function parseRawHttpResponse(buffer) {
  const headerEnd = buffer.indexOf("\r\n\r\n");
  if (headerEnd === -1) {
    return null;
  }
  const headText = buffer.subarray(0, headerEnd).toString("latin1");
  const lines = headText.split("\r\n");
  const statusLine = lines.shift() || "";
  const statusMatch = /^HTTP\/(\d)\.(\d)\s+(\d{3})(?:\s+(.*))?$/.exec(statusLine);
  if (!statusMatch) {
    throw new Error(`Invalid HTTP response status line: ${statusLine}`);
  }
  const headers = {};
  const rawHeaders = [];
  let previousHeaderName = null;
  for (const line of lines) {
    if (!line) {
      continue;
    }
    if ((line.startsWith(" ") || line.startsWith("\t")) && rawHeaders.length >= 2 && previousHeaderName) {
      const continuation = line.trim();
      rawHeaders[rawHeaders.length - 1] += ` ${continuation}`;
      headers[previousHeaderName] = joinHeaderValue(headers[previousHeaderName]) + ` ${continuation}`;
      continue;
    }
    const separatorIndex = line.indexOf(":");
    if (separatorIndex === -1) {
      throw new Error(`Invalid HTTP response header line: ${line}`);
    }
    const rawName = line.slice(0, separatorIndex);
    const rawValue = line.slice(separatorIndex + 1).trim();
    previousHeaderName = rawName.toLowerCase();
    rawHeaders.push(rawName, rawValue);
    appendNormalizedHeader(headers, previousHeaderName, rawValue);
  }
  return {
    status: Number(statusMatch[3]),
    statusText: statusMatch[4] || "",
    headers,
    rawHeaders,
    head: buffer.subarray(headerEnd + 4)
  };
}
function waitForRawHttpResponseHead(socket, timeoutMs) {
  return new Promise((resolve, reject) => {
    let buffer = Buffer.alloc(0);
    let settled = false;
    const finish = (error, value) => {
      if (settled) {
        return;
      }
      settled = true;
      cleanup();
      if (error) {
        reject(error);
        return;
      }
      resolve(value);
    };
    const cleanup = () => {
      clearTimeout(timer);
      socket.off?.("data", onData);
      socket.removeListener?.("data", onData);
      socket.off?.("error", onError);
      socket.removeListener?.("error", onError);
      socket.off?.("end", onEnd);
      socket.removeListener?.("end", onEnd);
      socket.off?.("close", onClose);
      socket.removeListener?.("close", onClose);
    };
    const onData = (chunk) => {
      const payload = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
      buffer = Buffer.concat([buffer, payload]);
      try {
        const parsed = parseRawHttpResponse(buffer);
        if (parsed) {
          finish(null, parsed);
        }
      } catch (error) {
        finish(error instanceof Error ? error : new Error(String(error)));
      }
    };
    const onError = (error) => {
      finish(error instanceof Error ? error : new Error(String(error)));
    };
    const onEnd = () => {
      finish(createConnResetError("socket ended before receiving HTTP response head"));
    };
    const onClose = () => {
      finish(createConnResetError("socket closed before receiving HTTP response head"));
    };
    const timer = setTimeout(() => {
      finish(new Error(`Timed out waiting for HTTP response head after ${timeoutMs}ms`));
    }, timeoutMs);
    socket.on("data", onData);
    socket.once("error", onError);
    socket.once("end", onEnd);
    socket.once("close", onClose);
  });
}
function waitForRawHttpResponse(socket, requestMethod, timeoutMs) {
  return new Promise((resolve, reject) => {
    let header = null;
    let bodyBuffer = Buffer.alloc(0);
    let expectedContentLength = null;
    let expectsChunkedBody = false;
    let expectsCloseDelimitedBody = false;
    let settled = false;
    const finish = (error, value) => {
      if (settled) {
        return;
      }
      settled = true;
      cleanup();
      if (error) {
        reject(error);
        return;
      }
      resolve(value);
    };
    const cleanup = () => {
      clearTimeout(timer);
      socket.off?.("data", onData);
      socket.removeListener?.("data", onData);
      socket.off?.("error", onError);
      socket.removeListener?.("error", onError);
      socket.off?.("end", onEnd);
      socket.removeListener?.("end", onEnd);
      socket.off?.("close", onClose);
      socket.removeListener?.("close", onClose);
    };
    const maybeFinishWithBody = () => {
      if (!header) {
        return false;
      }
      if (!hasResponseBody(header.status, requestMethod)) {
        finish(null, {
          ...header,
          body: Buffer.alloc(0)
        });
        return true;
      }
      if (expectsChunkedBody) {
        const parsedChunked = parseChunkedBody(bodyBuffer);
        if (parsedChunked === null) {
          finish(new Error("Invalid chunked HTTP response body"));
          return true;
        }
        if (!parsedChunked.complete) {
          return false;
        }
        finish(null, {
          ...header,
          body: parsedChunked.body
        });
        return true;
      }
      if (expectedContentLength !== null) {
        if (bodyBuffer.length < expectedContentLength) {
          return false;
        }
        finish(null, {
          ...header,
          body: bodyBuffer.subarray(0, expectedContentLength)
        });
        return true;
      }
      return false;
    };
    const configureBodyHandling = () => {
      if (!header || !hasResponseBody(header.status, requestMethod)) {
        return;
      }
      const transferEncoding = header.headers["transfer-encoding"];
      const contentLength = header.headers["content-length"];
      if (transferEncoding !== void 0) {
        const tokens = splitTransferEncodingTokens(joinHeaderValue(transferEncoding));
        const chunkedCount = tokens.filter((entry) => entry === "chunked").length;
        const hasChunked = chunkedCount > 0;
        const chunkedIsFinal = hasChunked && tokens[tokens.length - 1] === "chunked";
        if (!hasChunked || chunkedCount !== 1 || !chunkedIsFinal || contentLength !== void 0) {
          throw new Error("Unsupported transfer-encoding in HTTP response");
        }
        expectsChunkedBody = true;
        return;
      }
      if (contentLength !== void 0) {
        const parsedContentLength = parseContentLengthHeader(contentLength);
        if (parsedContentLength === null) {
          throw new Error("Invalid content-length in HTTP response");
        }
        expectedContentLength = parsedContentLength;
        return;
      }
      expectsCloseDelimitedBody = true;
    };
    const onData = (chunk) => {
      const payload = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
      if (!header) {
        bodyBuffer = Buffer.concat([bodyBuffer, payload]);
        try {
          const parsed = parseRawHttpResponse(bodyBuffer);
          if (!parsed) {
            return;
          }
          header = parsed;
          bodyBuffer = Buffer.from(parsed.head);
          configureBodyHandling();
          maybeFinishWithBody();
        } catch (error) {
          finish(error instanceof Error ? error : new Error(String(error)));
        }
        return;
      }
      bodyBuffer = Buffer.concat([bodyBuffer, payload]);
      try {
        maybeFinishWithBody();
      } catch (error) {
        finish(error instanceof Error ? error : new Error(String(error)));
      }
    };
    const onError = (error) => {
      finish(error instanceof Error ? error : new Error(String(error)));
    };
    const onEnd = () => {
      if (!header) {
        finish(createConnResetError("socket ended before receiving HTTP response head"));
        return;
      }
      if (expectsCloseDelimitedBody) {
        finish(null, {
          ...header,
          body: bodyBuffer
        });
        return;
      }
      if (maybeFinishWithBody()) {
        return;
      }
      finish(createConnResetError("socket ended before receiving complete HTTP response body"));
    };
    const onClose = () => {
      if (!header) {
        finish(createConnResetError("socket closed before receiving HTTP response head"));
        return;
      }
      if (expectsCloseDelimitedBody) {
        finish(null, {
          ...header,
          body: bodyBuffer
        });
        return;
      }
      if (maybeFinishWithBody()) {
        return;
      }
      finish(createConnResetError("socket closed before receiving complete HTTP response body"));
    };
    const timer = setTimeout(() => {
      finish(new Error(`Timed out waiting for HTTP response after ${timeoutMs}ms`));
    }, timeoutMs);
    socket.on("data", onData);
    socket.once("error", onError);
    socket.once("end", onEnd);
    socket.once("close", onClose);
  });
}
function hasResponseBody(statusCode, method) {
  if (method === "HEAD") {
    return false;
  }
  if (statusCode >= 100 && statusCode < 200 || statusCode === 204 || statusCode === 304) {
    return false;
  }
  return true;
}
function splitTransferEncodingTokens(value) {
  return value.split(",").map((entry) => entry.trim().toLowerCase()).filter((entry) => entry.length > 0);
}
function parseContentLengthHeader(value) {
  if (value === void 0) {
    return 0;
  }
  const entries = Array.isArray(value) ? value : [value];
  let parsed = null;
  for (const entry of entries) {
    if (!/^\d+$/.test(entry)) {
      return null;
    }
    const nextValue = Number(entry);
    if (!Number.isSafeInteger(nextValue) || nextValue < 0) {
      return null;
    }
    if (parsed !== null && parsed !== nextValue) {
      return null;
    }
    parsed = nextValue;
  }
  return parsed ?? 0;
}
function parseChunkedBody(bodyBuffer, maxBodyBytes = MAX_HTTP_BODY_BYTES) {
  let offset = 0;
  let totalBodyBytes = 0;
  const chunks = [];
  while (true) {
    const lineEnd = bodyBuffer.indexOf("\r\n", offset);
    if (lineEnd === -1) {
      return { complete: false };
    }
    const sizeLine = bodyBuffer.subarray(offset, lineEnd).toString("latin1");
    if (sizeLine.length === 0 || /[\r\n]/.test(sizeLine)) {
      return null;
    }
    const [sizePart, extensionPart] = sizeLine.split(";", 2);
    if (!/^[0-9A-Fa-f]+$/.test(sizePart)) {
      return null;
    }
    if (extensionPart !== void 0 && /[\r\n]/.test(extensionPart)) {
      return null;
    }
    const chunkSize = Number.parseInt(sizePart, 16);
    if (!Number.isSafeInteger(chunkSize) || chunkSize < 0) {
      return null;
    }
    if (totalBodyBytes + chunkSize > maxBodyBytes) {
      return null;
    }
    const chunkStart = lineEnd + 2;
    const chunkEnd = chunkStart + chunkSize;
    const chunkTerminatorEnd = chunkEnd + 2;
    if (chunkTerminatorEnd > bodyBuffer.length) {
      return { complete: false };
    }
    if (bodyBuffer[chunkEnd] !== 13 || bodyBuffer[chunkEnd + 1] !== 10) {
      return null;
    }
    if (chunkSize > 0) {
      totalBodyBytes += chunkSize;
      chunks.push(bodyBuffer.subarray(chunkStart, chunkEnd));
      offset = chunkTerminatorEnd;
      continue;
    }
    const trailersEnd = bodyBuffer.indexOf("\r\n\r\n", chunkStart);
    if (trailersEnd === -1) {
      return { complete: false };
    }
    const trailerBlock = bodyBuffer.subarray(chunkStart, trailersEnd).toString("latin1");
    if (trailerBlock.length > 0) {
      for (const trailerLine of trailerBlock.split("\r\n")) {
        if (trailerLine.length === 0) {
          continue;
        }
        if (trailerLine.startsWith(" ") || trailerLine.startsWith("	")) {
          return null;
        }
        if (trailerLine.indexOf(":") === -1) {
          return null;
        }
      }
    }
    return {
      complete: true,
      bytesConsumed: trailersEnd + 4,
      body: chunks.length > 0 ? Buffer.concat(chunks) : Buffer.alloc(0)
    };
  }
}
function parseLoopbackRequestBuffer(buffer, server) {
  let requestStart = 0;
  while (requestStart + 1 < buffer.length && buffer[requestStart] === 13 && buffer[requestStart + 1] === 10) {
    requestStart += 2;
  }
  const headerEnd = buffer.indexOf("\r\n\r\n", requestStart);
  if (headerEnd === -1) {
    if (buffer.length - requestStart > MAX_HTTP_REQUEST_HEADER_BYTES) {
      return {
        kind: "bad-request",
        closeConnection: true
      };
    }
    return { kind: "incomplete" };
  }
  if (headerEnd - requestStart > MAX_HTTP_REQUEST_HEADER_BYTES) {
    return {
      kind: "bad-request",
      closeConnection: true
    };
  }
  const headerBlock = buffer.subarray(requestStart, headerEnd).toString("latin1");
  const [requestLine, ...headerLines] = headerBlock.split("\r\n");
  if (headerLines.length > MAX_HTTP_REQUEST_HEADERS) {
    return {
      kind: "bad-request",
      closeConnection: true
    };
  }
  const requestMatch = /^([A-Z]+)\s+(\S+)\s+HTTP\/(1)\.(0|1)$/.exec(requestLine);
  if (!requestMatch) {
    return {
      kind: "bad-request",
      closeConnection: true
    };
  }
  const headers = {};
  const rawHeaders = [];
  let previousHeaderName = null;
  try {
    for (const headerLine of headerLines) {
      if (headerLine.length === 0) {
        continue;
      }
      if (headerLine.startsWith(" ") || headerLine.startsWith("	")) {
        return {
          kind: "bad-request",
          closeConnection: true
        };
      }
      const separatorIndex = headerLine.indexOf(":");
      if (separatorIndex === -1) {
        return {
          kind: "bad-request",
          closeConnection: true
        };
      }
      const rawName = headerLine.slice(0, separatorIndex).trim();
      const rawValue = headerLine.slice(separatorIndex + 1).trim();
      const normalizedName = validateHeaderName(rawName).toLowerCase();
      validateHeaderValue(normalizedName, rawValue);
      appendNormalizedHeader(headers, normalizedName, rawValue);
      rawHeaders.push(rawName, rawValue);
      previousHeaderName = normalizedName;
    }
  } catch {
    return {
      kind: "bad-request",
      closeConnection: true
    };
  }
  const requestMethod = requestMatch[1];
  const requestUrl = requestMatch[2];
  const httpMinorVersion = Number(requestMatch[4]);
  const requestCloseHeader = joinHeaderValue(headers.connection || "").toLowerCase();
  let closeConnection = httpMinorVersion === 0 ? !requestCloseHeader.includes("keep-alive") : requestCloseHeader.includes("close");
  if (hasUpgradeRequestHeaders(headers) && server.listenerCount("upgrade") > 0) {
    return {
      kind: "request",
      bytesConsumed: buffer.length,
      closeConnection: false,
      request: {
        method: requestMethod,
        url: requestUrl,
        headers,
        rawHeaders,
        bodyBase64: headerEnd + 4 < buffer.length ? buffer.subarray(headerEnd + 4).toString("base64") : void 0
      },
      upgradeHead: headerEnd + 4 < buffer.length ? buffer.subarray(headerEnd + 4) : Buffer.alloc(0)
    };
  }
  const transferEncoding = headers["transfer-encoding"];
  const contentLength = headers["content-length"];
  let requestBody = Buffer.alloc(0);
  let bytesConsumed = headerEnd + 4;
  if (transferEncoding !== void 0) {
    const tokens = splitTransferEncodingTokens(joinHeaderValue(transferEncoding));
    const chunkedCount = tokens.filter((entry) => entry === "chunked").length;
    const hasChunked = chunkedCount > 0;
    const chunkedIsFinal = hasChunked && tokens[tokens.length - 1] === "chunked";
    if (!hasChunked || chunkedCount !== 1 || !chunkedIsFinal || contentLength !== void 0) {
      return {
        kind: "bad-request",
        closeConnection: true
      };
    }
    const parsedChunked = parseChunkedBody(buffer.subarray(headerEnd + 4));
    if (parsedChunked === null) {
      return {
        kind: "bad-request",
        closeConnection: true
      };
    }
    if (!parsedChunked.complete) {
      return { kind: "incomplete" };
    }
    requestBody = parsedChunked.body;
    bytesConsumed = headerEnd + 4 + parsedChunked.bytesConsumed;
  } else if (contentLength !== void 0) {
    const parsedContentLength = parseContentLengthHeader(contentLength);
    if (parsedContentLength === null || parsedContentLength > MAX_HTTP_BODY_BYTES) {
      return {
        kind: "bad-request",
        closeConnection: true
      };
    }
    const bodyEnd = headerEnd + 4 + parsedContentLength;
    if (bodyEnd > buffer.length) {
      return { kind: "incomplete" };
    }
    requestBody = buffer.subarray(headerEnd + 4, bodyEnd);
    bytesConsumed = bodyEnd;
  }
  return {
    kind: "request",
    bytesConsumed,
    closeConnection,
    request: {
      method: requestMethod,
      url: requestUrl,
      headers,
      rawHeaders,
      bodyBase64: requestBody.length > 0 ? requestBody.toString("base64") : void 0
    }
  };
}
function serializeRawHeaderPairs(rawHeaders, fallbackHeaders) {
  const headers = {};
  const rawNameMap = /* @__PURE__ */ new Map();
  const order = [];
  if (Array.isArray(rawHeaders) && rawHeaders.length > 0) {
    for (let index = 0; index < rawHeaders.length; index += 2) {
      const rawName = rawHeaders[index];
      const value = rawHeaders[index + 1];
      if (rawName === void 0 || value === void 0) {
        continue;
      }
      const normalizedName = rawName.toLowerCase();
      appendNormalizedHeader(headers, normalizedName, value);
      if (!rawNameMap.has(normalizedName)) {
        rawNameMap.set(normalizedName, rawName);
        order.push(normalizedName);
      }
    }
    return { headers, rawNameMap, order };
  }
  if (Array.isArray(fallbackHeaders)) {
    for (const [name, value] of fallbackHeaders) {
      const normalizedName = name.toLowerCase();
      appendNormalizedHeader(headers, normalizedName, value);
      if (!rawNameMap.has(normalizedName)) {
        rawNameMap.set(normalizedName, name);
        order.push(normalizedName);
      }
    }
  }
  return { headers, rawNameMap, order };
}
function finalizeRawHeaderPairs(headers, rawNameMap, order) {
  const entries = [];
  const seen = /* @__PURE__ */ new Set();
  for (const key of order) {
    const value = headers[key];
    if (value === void 0) {
      continue;
    }
    const rawName = rawNameMap.get(key) || key;
    const serialized = Array.isArray(value) ? key === "set-cookie" ? value : [value.join(", ")] : [value];
    for (const entry of serialized) {
      entries.push([rawName, entry]);
    }
    seen.add(key);
  }
  for (const [key, value] of Object.entries(headers)) {
    if (seen.has(key)) {
      continue;
    }
    const rawName = rawNameMap.get(key) || key;
    const serialized = Array.isArray(value) ? key === "set-cookie" ? value : [value.join(", ")] : [value];
    for (const entry of serialized) {
      entries.push([rawName, entry]);
    }
  }
  return entries;
}
function createBadRequestResponseBuffer() {
  return Buffer.from("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n", "latin1");
}
function serializeLoopbackResponse(response, request, requestWantsClose) {
  const statusCode = response.status || 200;
  const statusText = HTTP_STATUS_TEXT[statusCode] || "OK";
  const {
    headers,
    rawNameMap,
    order
  } = serializeRawHeaderPairs(response.rawHeaders, response.headers);
  const trailerInfo = serializeRawHeaderPairs(response.rawTrailers, response.trailers);
  const bodyBuffer = response.body == null ? Buffer.alloc(0) : response.bodyEncoding === "base64" ? Buffer.from(response.body, "base64") : Buffer.from(response.body, "utf8");
  const bodyAllowed = hasResponseBody(statusCode, request.method);
  const transferEncodingTokens = headers["transfer-encoding"] ? splitTransferEncodingTokens(joinHeaderValue(headers["transfer-encoding"])) : [];
  let isChunked = transferEncodingTokens.includes("chunked");
  const hasExplicitContentLength = headers["content-length"] !== void 0;
  let closeConnection = requestWantsClose || response.connectionEnded === true || response.connectionReset === true;
  if (!bodyAllowed) {
    if (isChunked) {
      closeConnection = true;
    }
    delete headers["content-length"];
  } else if (!isChunked && !hasExplicitContentLength) {
    if (response.streamed === true) {
      headers["transfer-encoding"] = "chunked";
      rawNameMap.set("transfer-encoding", "Transfer-Encoding");
      order.push("transfer-encoding");
      isChunked = true;
    } else {
      headers["content-length"] = String(bodyBuffer.length);
      rawNameMap.set("content-length", "Content-Length");
      order.push("content-length");
    }
  }
  if (closeConnection) {
    headers.connection = "close";
    if (!rawNameMap.has("connection")) {
      rawNameMap.set("connection", "Connection");
      order.push("connection");
    }
  } else if (headers.connection === void 0 && request.headers.connection !== void 0) {
    headers.connection = "keep-alive";
    rawNameMap.set("connection", "Connection");
    order.push("connection");
  }
  const serializedChunks = [];
  for (const informational of response.informational ?? []) {
    const infoHeaders = finalizeRawHeaderPairs(
      serializeRawHeaderPairs(informational.rawHeaders, informational.headers).headers,
      serializeRawHeaderPairs(informational.rawHeaders, informational.headers).rawNameMap,
      serializeRawHeaderPairs(informational.rawHeaders, informational.headers).order
    );
    const headerLines2 = infoHeaders.map(([name, value]) => `${name}: ${value}\r
`).join("");
    serializedChunks.push(
      Buffer.from(
        `HTTP/1.1 ${informational.status} ${informational.statusText || HTTP_STATUS_TEXT[informational.status] || ""}\r
${headerLines2}\r
`,
        "latin1"
      )
    );
  }
  const finalHeaders = finalizeRawHeaderPairs(headers, rawNameMap, order);
  const headerLines = finalHeaders.map(([name, value]) => `${name}: ${value}\r
`).join("");
  serializedChunks.push(
    Buffer.from(`HTTP/1.1 ${statusCode} ${statusText}\r
${headerLines}\r
`, "latin1")
  );
  if (bodyAllowed) {
    if (isChunked) {
      if (bodyBuffer.length > 0) {
        serializedChunks.push(Buffer.from(bodyBuffer.length.toString(16) + "\r\n", "latin1"));
        serializedChunks.push(bodyBuffer);
        serializedChunks.push(Buffer.from("\r\n", "latin1"));
      }
      serializedChunks.push(Buffer.from("0\r\n", "latin1"));
      if (Object.keys(trailerInfo.headers).length > 0) {
        const trailerPairs = finalizeRawHeaderPairs(
          trailerInfo.headers,
          trailerInfo.rawNameMap,
          trailerInfo.order
        );
        for (const [name, value] of trailerPairs) {
          serializedChunks.push(Buffer.from(`${name}: ${value}\r
`, "latin1"));
        }
      }
      serializedChunks.push(Buffer.from("\r\n", "latin1"));
    } else if (bodyBuffer.length > 0) {
      serializedChunks.push(bodyBuffer);
    }
  }
  return {
    payload: serializedChunks.length === 1 ? serializedChunks[0] : Buffer.concat(serializedChunks),
    closeConnection
  };
}
var HTTP_STATUS_TEXT = {
  100: "Continue",
  101: "Switching Protocols",
  102: "Processing",
  103: "Early Hints",
  200: "OK",
  201: "Created",
  204: "No Content",
  301: "Moved Permanently",
  302: "Found",
  304: "Not Modified",
  400: "Bad Request",
  401: "Unauthorized",
  403: "Forbidden",
  404: "Not Found",
  500: "Internal Server Error"
};
function isLoopbackRequestHost(hostname) {
  const bare = hostname.startsWith("[") && hostname.endsWith("]") ? hostname.slice(1, -1) : hostname;
  return bare === "localhost" || bare === "127.0.0.1" || bare === "::1";
}
var ServerIncomingMessage = class {
  headers;
  rawHeaders;
  method;
  url;
  socket;
  connection;
  rawBody;
  destroyed = false;
  errored;
  readable = true;
  httpVersion = "1.1";
  httpVersionMajor = 1;
  httpVersionMinor = 1;
  complete = true;
  aborted = false;
  // Readable stream state stub for frameworks that inspect internal state
  _readableState = { flowing: null, length: 0, ended: false, objectMode: false };
  _listeners = {};
  constructor(request) {
    this.headers = request.headers || {};
    this.rawHeaders = request.rawHeaders || [];
    if (!Array.isArray(this.rawHeaders) || this.rawHeaders.length % 2 !== 0) {
      this.rawHeaders = [];
    }
    this.method = request.method || "GET";
    this.url = request.url || "/";
    const fakeSocket = {
      encrypted: false,
      remoteAddress: "127.0.0.1",
      remotePort: 0,
      writable: true,
      on() {
        return fakeSocket;
      },
      once() {
        return fakeSocket;
      },
      removeListener() {
        return fakeSocket;
      },
      destroy() {
      },
      end() {
      }
    };
    this.socket = fakeSocket;
    this.connection = fakeSocket;
    const rawHost = this.headers.host;
    if (typeof rawHost === "string" && rawHost.includes(",")) {
      this.headers.host = rawHost.split(",")[0].trim();
    }
    if (!this.headers.host) {
      this.headers.host = "127.0.0.1";
    }
    if (this.rawHeaders.length === 0) {
      Object.entries(this.headers).forEach(([key, value]) => {
        if (Array.isArray(value)) {
          value.forEach((entry) => {
            this.rawHeaders.push(key, entry);
          });
          return;
        }
        this.rawHeaders.push(key, value);
      });
    }
    if (request.bodyBase64 && typeof Buffer !== "undefined") {
      this.rawBody = Buffer.from(request.bodyBase64, "base64");
    }
  }
  on(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(listener);
    return this;
  }
  once(event, listener) {
    const wrapped = (...args) => {
      this.off(event, wrapped);
      listener.call(this, ...args);
    };
    return this.on(event, wrapped);
  }
  off(event, listener) {
    const listeners = this._listeners[event];
    if (!listeners) return this;
    const index = listeners.indexOf(listener);
    if (index !== -1) listeners.splice(index, 1);
    return this;
  }
  removeListener(event, listener) {
    return this.off(event, listener);
  }
  emit(event, ...args) {
    const listeners = this._listeners[event];
    return dispatchCustomEmitterListeners(this, listeners, args);
  }
  // Readable stream stubs for framework compatibility
  unpipe() {
    return this;
  }
  pause() {
    return this;
  }
  resume() {
    return this;
  }
  read() {
    return null;
  }
  pipe(dest) {
    return dest;
  }
  isPaused() {
    return false;
  }
  setEncoding() {
    return this;
  }
  destroy(err) {
    this.destroyed = true;
    this.errored = err;
    if (err) {
      this.emit("error", err);
    }
    this.emit("close");
    return this;
  }
  _abort() {
    if (this.aborted) {
      return;
    }
    this.aborted = true;
    const error = createConnResetError("aborted");
    this.emit("aborted");
    this.emit("error", error);
    this.emit("close");
  }
};
var ServerResponseBridge = class {
  statusCode = 200;
  statusMessage = "OK";
  headersSent = false;
  writable = true;
  writableFinished = false;
  outputSize = 0;
  _headers = /* @__PURE__ */ new Map();
  _trailers = /* @__PURE__ */ new Map();
  _chunks = [];
  _chunksBytes = 0;
  _streamed = false;
  _listeners = {};
  _closedPromise;
  _resolveClosed = null;
  _connectionEnded = false;
  _connectionReset = false;
  _rawHeaderNames = /* @__PURE__ */ new Map();
  _rawTrailerNames = /* @__PURE__ */ new Map();
  _informational = [];
  _pendingRawInfoBuffer = "";
  _streamSocket = null;
  _streamRequest = null;
  _streamedDirectly = false;
  _streamCloseConnection = false;
  constructor() {
    this._closedPromise = new Promise((resolve) => {
      this._resolveClosed = resolve;
    });
  }
  on(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(listener);
    return this;
  }
  once(event, listener) {
    const wrapped = (...args) => {
      this.off(event, wrapped);
      listener.call(this, ...args);
    };
    return this.on(event, wrapped);
  }
  off(event, listener) {
    const listeners = this._listeners[event];
    if (!listeners) return this;
    const index = listeners.indexOf(listener);
    if (index !== -1) listeners.splice(index, 1);
    return this;
  }
  removeListener(event, listener) {
    return this.off(event, listener);
  }
  emit(event, ...args) {
    const listeners = this._listeners[event];
    if (!listeners || listeners.length === 0) return false;
    listeners.slice().forEach((fn) => fn.call(this, ...args));
    return true;
  }
  _emit(event, ...args) {
    this.emit(event, ...args);
  }
  writeHead(statusCode, headers) {
    if (statusCode >= 100 && statusCode < 200 && statusCode !== 101) {
      const informationalHeaders = /* @__PURE__ */ new Map();
      const informationalRawHeaderNames = /* @__PURE__ */ new Map();
      if (headers) {
        if (isFlatHeaderList(headers)) {
          for (let index = 0; index < headers.length; index += 2) {
            const key = headers[index];
            const value = headers[index + 1];
            if (key === void 0 || value === void 0) {
              continue;
            }
            const actualName = validateHeaderName(key).toLowerCase();
            validateHeaderValue(actualName, value);
            informationalHeaders.set(actualName, String(value));
            if (!informationalRawHeaderNames.has(actualName)) {
              informationalRawHeaderNames.set(actualName, key);
            }
          }
        } else if (Array.isArray(headers)) {
          headers.forEach(([key, value]) => {
            const actualName = validateHeaderName(key).toLowerCase();
            validateHeaderValue(actualName, value);
            informationalHeaders.set(actualName, String(value));
            if (!informationalRawHeaderNames.has(actualName)) {
              informationalRawHeaderNames.set(actualName, key);
            }
          });
        } else {
          Object.entries(headers).forEach(([key, value]) => {
            const actualName = validateHeaderName(key).toLowerCase();
            validateHeaderValue(actualName, value);
            informationalHeaders.set(actualName, String(value));
            if (!informationalRawHeaderNames.has(actualName)) {
              informationalRawHeaderNames.set(actualName, key);
            }
          });
        }
      }
      const normalizedHeaders = Array.from(informationalHeaders.entries()).flatMap(([key, value]) => {
        const serialized = serializeHeaderValue(value);
        return Array.isArray(serialized) ? serialized.map((entry) => [key, entry]) : [[key, serialized]];
      });
      const rawHeaders = Array.from(informationalHeaders.entries()).flatMap(([key, value]) => {
        const rawName = informationalRawHeaderNames.get(key) || key;
        const serialized = serializeHeaderValue(value);
        return Array.isArray(serialized) ? serialized.flatMap((entry) => [rawName, entry]) : [rawName, serialized];
      });
      this._informational.push({
        status: statusCode,
        statusText: HTTP_STATUS_TEXT[statusCode],
        headers: normalizedHeaders,
        rawHeaders
      });
      return this;
    }
    this.statusCode = statusCode;
    if (headers) {
      if (isFlatHeaderList(headers)) {
        for (let index = 0; index < headers.length; index += 2) {
          const key = headers[index];
          const value = headers[index + 1];
          if (key !== void 0 && value !== void 0) {
            this.setHeader(key, value);
          }
        }
      } else if (Array.isArray(headers)) {
        headers.forEach(([key, value]) => this.setHeader(key, value));
      } else {
        Object.entries(headers).forEach(
          ([key, value]) => this.setHeader(key, value)
        );
      }
    }
    this.headersSent = true;
    this.outputSize += 64;
    return this;
  }
  setHeader(name, value) {
    if (this.headersSent) {
      throw createErrorWithCode(
        "Cannot set headers after they are sent to the client",
        "ERR_HTTP_HEADERS_SENT"
      );
    }
    const lower = validateHeaderName(name).toLowerCase();
    validateHeaderValue(lower, value);
    const storedValue = Array.isArray(value) ? Array.from(value) : value;
    this._headers.set(lower, storedValue);
    if (!this._rawHeaderNames.has(lower)) {
      this._rawHeaderNames.set(lower, name);
    }
    return this;
  }
  setHeaders(headers) {
    if (this.headersSent) {
      throw createErrorWithCode(
        "Cannot set headers after they are sent to the client",
        "ERR_HTTP_HEADERS_SENT"
      );
    }
    if (!(headers instanceof Headers) && !(headers instanceof Map)) {
      throw createTypeErrorWithCode(
        `The "headers" argument must be an instance of Headers or Map. Received ${formatReceivedType(headers)}`,
        "ERR_INVALID_ARG_TYPE"
      );
    }
    if (headers instanceof Headers) {
      const pending = /* @__PURE__ */ Object.create(null);
      headers.forEach((value, key) => {
        appendNormalizedHeader(pending, key.toLowerCase(), value);
      });
      Object.entries(pending).forEach(([key, value]) => {
        this.setHeader(key, value);
      });
      return this;
    }
    headers.forEach((value, key) => {
      this.setHeader(key, value);
    });
    return this;
  }
  getHeader(name) {
    if (typeof name !== "string") {
      throw createTypeErrorWithCode(
        `The "name" argument must be of type string. Received ${formatReceivedType(name)}`,
        "ERR_INVALID_ARG_TYPE"
      );
    }
    const value = this._headers.get(name.toLowerCase());
    return value === void 0 ? void 0 : cloneStoredHeaderValue(value);
  }
  hasHeader(name) {
    if (typeof name !== "string") {
      throw createTypeErrorWithCode(
        `The "name" argument must be of type string. Received ${formatReceivedType(name)}`,
        "ERR_INVALID_ARG_TYPE"
      );
    }
    return this._headers.has(name.toLowerCase());
  }
  removeHeader(name) {
    if (typeof name !== "string") {
      throw createTypeErrorWithCode(
        `The "name" argument must be of type string. Received ${formatReceivedType(name)}`,
        "ERR_INVALID_ARG_TYPE"
      );
    }
    const lower = name.toLowerCase();
    this._headers.delete(lower);
    this._rawHeaderNames.delete(lower);
  }
  _appendChunk(chunk, encoding, streamed) {
    if (chunk == null) return true;
    const buf = typeof chunk === "string" ? Buffer.from(chunk, typeof encoding === "string" ? encoding : void 0) : chunk;
    if (this._chunksBytes + buf.byteLength > MAX_HTTP_BODY_BYTES) {
      throw new Error("ERR_HTTP_BODY_TOO_LARGE: response body exceeds " + MAX_HTTP_BODY_BYTES + " byte limit");
    }
    this._chunks.push(buf);
    this._chunksBytes += buf.byteLength;
    this._streamed ||= streamed;
    this.headersSent = true;
    this.outputSize += buf.byteLength;
    return true;
  }
  write(chunk, encodingOrCallback, callback) {
    this._appendChunk(chunk, typeof encodingOrCallback === "string" ? encodingOrCallback : void 0, true);
    const writeCallback = typeof encodingOrCallback === "function" ? encodingOrCallback : callback;
    if (typeof writeCallback === "function") {
      queueMicrotask(writeCallback);
    }
    return true;
  }
  end(chunkOrCallback, encodingOrCallback, callback) {
    let chunk;
    let endCallback;
    if (typeof chunkOrCallback === "function") {
      endCallback = chunkOrCallback;
    } else {
      chunk = chunkOrCallback;
      endCallback = typeof encodingOrCallback === "function" ? encodingOrCallback : callback;
    }
    // Streaming fast path for socket-backed servers: a single `res.end(body)`
    // with no prior `res.write()` flushes headers then streams the body to the
    // connection socket in bounded slices. This avoids materializing the whole
    // body (plus its serialize/transmit copies) at once — a multi-MB response
    // otherwise trips the guest isolate heap-limit OOM guard before the host
    // can apply its own response-size limit. Per-slice `socket.destroyed`
    // checks also make the host's mid-stream rejection close graceful instead
    // of crashing the guest.
    if (
      this._streamSocket &&
      this._chunks.length === 0 &&
      !this.writableFinished &&
      !this._streamedDirectly
    ) {
      const encoding =
        typeof encodingOrCallback === "string" ? encodingOrCallback : void 0;
      this._streamEndBody(chunk, encoding);
      if (typeof endCallback === "function") {
        queueMicrotask(endCallback);
      }
      return this;
    }
    if (chunk != null) {
      if (typeof chunk === "string" && typeof encodingOrCallback === "string") {
        this._appendChunk(chunk, encodingOrCallback, false);
      } else {
        this._appendChunk(chunk, void 0, false);
      }
    }
    this._finalize();
    if (typeof endCallback === "function") {
      queueMicrotask(endCallback);
    }
    return this;
  }
  _streamEndBody(body, encoding) {
    const isString = typeof body === "string";
    const SLICE_BYTES = 256 * 1024;
    // Compute the body byte length in bounded slices. `Buffer.byteLength` on a
    // whole multi-MB string allocates enough that, with the isolate already
    // near its heap cap, it trips the OOM guard before a single byte is sent.
    let byteLength = 0;
    if (body != null) {
      if (isString) {
        for (let offset = 0; offset < body.length; offset += SLICE_BYTES) {
          byteLength += Buffer.byteLength(
            body.slice(offset, offset + SLICE_BYTES),
            encoding,
          );
        }
      } else {
        byteLength = body.length;
      }
    }
    if (!this._headers.has("content-length") && !this._headers.has("transfer-encoding")) {
      this._headers.set("content-length", String(byteLength));
      this._rawHeaderNames.set("content-length", "Content-Length");
    }
    this.headersSent = true;
    // Serialize headers only (no buffered chunks => empty body in the payload),
    // then stream the real body separately in bounded slices.
    const headerResponse = this.serialize();
    const built = serializeLoopbackResponse(headerResponse, this._streamRequest, true);
    this._streamCloseConnection = built.closeConnection;
    this._streamedDirectly = true;
    this.outputSize += byteLength;
    if (!this._streamSocket.destroyed && built.payload.length > 0) {
      this._streamSocket.write(built.payload);
    }
    if (body != null && byteLength > 0) {
      if (isString) {
        for (let offset = 0; offset < body.length; offset += SLICE_BYTES) {
          if (this._streamSocket.destroyed) break;
          this._streamSocket.write(
            Buffer.from(body.slice(offset, offset + SLICE_BYTES), encoding),
          );
        }
      } else {
        for (let offset = 0; offset < body.length; offset += SLICE_BYTES) {
          if (this._streamSocket.destroyed) break;
          this._streamSocket.write(body.subarray(offset, offset + SLICE_BYTES));
        }
      }
    }
    this._finalize();
  }
  getHeaderNames() {
    return Array.from(this._headers.keys());
  }
  getRawHeaderNames() {
    return Array.from(this._headers.keys()).map((key) => this._rawHeaderNames.get(key) || key);
  }
  getHeaders() {
    const result = /* @__PURE__ */ Object.create(null);
    for (const [key, value] of this._headers) {
      result[key] = cloneStoredHeaderValue(value);
    }
    return result;
  }
  // Writable stream state stub for frameworks that inspect internal state
  _writableState = { length: 0, ended: false, finished: false, objectMode: false, corked: 0 };
  // Fake socket for frameworks that access res.socket/res.connection
  socket = {
    writable: true,
    writableCorked: 0,
    writableHighWaterMark: 16 * 1024,
    on: () => this.socket,
    once: () => this.socket,
    removeListener: () => this.socket,
    destroy: () => {
      this._connectionReset = true;
      this._finalize();
    },
    end: () => {
      this._connectionEnded = true;
    },
    cork: () => {
      this._writableState.corked += 1;
      this.socket.writableCorked = this._writableState.corked;
    },
    uncork: () => {
      this._writableState.corked = Math.max(0, this._writableState.corked - 1);
      this.socket.writableCorked = this._writableState.corked;
    },
    write: (chunk, encodingOrCallback, callback) => {
      return this.write(chunk, encodingOrCallback, callback);
    }
  };
  connection = this.socket;
  // Node.js http.ServerResponse socket/stream compatibility stubs
  assignSocket() {
  }
  detachSocket() {
  }
  writeContinue() {
    this.writeHead(100);
  }
  writeProcessing() {
    this.writeHead(102);
  }
  addTrailers(headers) {
    if (Array.isArray(headers)) {
      for (let index = 0; index < headers.length; index += 2) {
        const key = headers[index];
        const value = headers[index + 1];
        if (key === void 0 || value === void 0) {
          continue;
        }
        const actualName = validateHeaderName(key).toLowerCase();
        validateHeaderValue(actualName, value);
        this._trailers.set(actualName, String(value));
        if (!this._rawTrailerNames.has(actualName)) {
          this._rawTrailerNames.set(actualName, key);
        }
      }
      return;
    }
    Object.entries(headers).forEach(([key, value]) => {
      const actualName = validateHeaderName(key).toLowerCase();
      validateHeaderValue(actualName, value);
      this._trailers.set(actualName, String(value));
      if (!this._rawTrailerNames.has(actualName)) {
        this._rawTrailerNames.set(actualName, key);
      }
    });
  }
  cork() {
    this.socket.cork();
  }
  uncork() {
    this.socket.uncork();
  }
  setTimeout(_msecs) {
    return this;
  }
  get writableCorked() {
    return Number(this.socket.writableCorked || 0);
  }
  flushHeaders() {
    this.headersSent = true;
  }
  destroy(err) {
    this._connectionReset = true;
    if (err) {
      this._emit("error", err);
    }
    this._finalize();
  }
  async waitForClose() {
    await this._closedPromise;
  }
  serialize() {
    const bodyBuffer = this._chunks.length > 0 ? Buffer.concat(this._chunks) : Buffer.alloc(0);
    const serializedHeaders = Array.from(this._headers.entries()).flatMap(([key, value]) => {
      const serialized = serializeHeaderValue(value);
      if (Array.isArray(serialized)) {
        if (key === "set-cookie") {
          return serialized.map((entry) => [key, entry]);
        }
        return [[key, serialized.join(", ")]];
      }
      return [[key, serialized]];
    });
    const rawHeaders = Array.from(this._headers.entries()).flatMap(([key, value]) => {
      const rawName = this._rawHeaderNames.get(key) || key;
      const serialized = serializeHeaderValue(value);
      if (Array.isArray(serialized)) {
        if (key === "set-cookie") {
          return serialized.flatMap((entry) => [rawName, entry]);
        }
        return [rawName, serialized.join(", ")];
      }
      return [rawName, serialized];
    });
    const serializedTrailers = Array.from(this._trailers.entries()).flatMap(([key, value]) => {
      const serialized = serializeHeaderValue(value);
      return Array.isArray(serialized) ? serialized.map((entry) => [key, entry]) : [[key, serialized]];
    });
    const rawTrailers = Array.from(this._trailers.entries()).flatMap(([key, value]) => {
      const rawName = this._rawTrailerNames.get(key) || key;
      const serialized = serializeHeaderValue(value);
      return Array.isArray(serialized) ? serialized.flatMap((entry) => [rawName, entry]) : [rawName, serialized];
    });
    return {
      status: this.statusCode,
      headers: serializedHeaders,
      rawHeaders,
      informational: this._informational.length > 0 ? [...this._informational] : void 0,
      body: bodyBuffer.toString("base64"),
      bodyEncoding: "base64",
      trailers: serializedTrailers.length > 0 ? serializedTrailers : void 0,
      rawTrailers: rawTrailers.length > 0 ? rawTrailers : void 0,
      connectionEnded: this._connectionEnded,
      connectionReset: this._connectionReset,
      streamed: this._streamed
    };
  }
  _writeRaw(chunk, callback) {
    this._pendingRawInfoBuffer += String(chunk);
    this._flushPendingRawInformational();
    if (typeof callback === "function") {
      queueMicrotask(callback);
    }
    return true;
  }
  _finalize() {
    if (this.writableFinished) {
      return;
    }
    this.writableFinished = true;
    this.writable = false;
    this._writableState.ended = true;
    this._writableState.finished = true;
    this._emit("finish");
    this._emit("close");
    this._resolveClosed?.();
    this._resolveClosed = null;
  }
  _flushPendingRawInformational() {
    let separatorIndex = this._pendingRawInfoBuffer.indexOf("\r\n\r\n");
    while (separatorIndex !== -1) {
      const rawFrame = this._pendingRawInfoBuffer.slice(0, separatorIndex);
      this._pendingRawInfoBuffer = this._pendingRawInfoBuffer.slice(separatorIndex + 4);
      const [statusLine, ...headerLines] = rawFrame.split("\r\n");
      const statusMatch = /^HTTP\/1\.[01]\s+(\d{3})(?:\s+(.*))?$/.exec(statusLine);
      if (!statusMatch) {
        separatorIndex = this._pendingRawInfoBuffer.indexOf("\r\n\r\n");
        continue;
      }
      const status = Number(statusMatch[1]);
      if (status >= 100 && status < 200 && status !== 101) {
        const headers = [];
        const rawHeaders = [];
        for (const headerLine of headerLines) {
          const separator = headerLine.indexOf(":");
          if (separator === -1) {
            continue;
          }
          const key = headerLine.slice(0, separator).trim();
          const value = headerLine.slice(separator + 1).trim();
          headers.push([key.toLowerCase(), value]);
          rawHeaders.push(key, value);
        }
        this._informational.push({
          status,
          statusText: statusMatch[2] || HTTP_STATUS_TEXT[status] || void 0,
          headers,
          rawHeaders
        });
      }
      separatorIndex = this._pendingRawInfoBuffer.indexOf("\r\n\r\n");
    }
  }
};
var Server = class {
  listening = false;
  _listeners = {};
  _serverId;
  _netServer = null;
  _listenPromise = null;
  _address = null;
  _handleId = null;
  _hostCloseWaitStarted = false;
  _activeRequestDispatches = 0;
  _closePending = false;
  _closeRunning = false;
  _closeCallbacks = [];
  /** @internal Request listener stored on the instance (replaces serverRequestListeners Map). */
  _requestListener;
  constructor(requestListener) {
    this._serverId = nextServerId++;
    this._requestListener = requestListener ?? (() => void 0);
    serverInstances.set(this._serverId, this);
  }
  /** @internal Bridge-visible server ID for loopback self-dispatch. */
  get _bridgeServerId() {
    return this._serverId;
  }
  /** @internal Emit an event — used by upgrade dispatch to fire 'upgrade' events. */
  _emit(event, ...args) {
    const listeners = this._listeners[event];
    if (!listeners || listeners.length === 0) return;
    listeners.slice().forEach((listener) => listener.call(this, ...args));
  }
  _finishStart(resultJson) {
    const result = JSON.parse(resultJson);
    this._address = result.address;
    this.listening = true;
    this._handleId = `http-server:${this._serverId}`;
    debugBridgeNetwork("server listening", this._serverId, this._address);
    if (typeof _registerHandle === "function") {
      _registerHandle(this._handleId, "http server");
    }
    this._startHostCloseWait();
  }
  _completeClose() {
    this.listening = false;
    this._address = null;
    serverInstances.delete(this._serverId);
    if (this._handleId && typeof _unregisterHandle === "function") {
      _unregisterHandle(this._handleId);
    }
    this._handleId = null;
  }
  _beginRequestDispatch() {
    this._activeRequestDispatches += 1;
  }
  _endRequestDispatch() {
    this._activeRequestDispatches = Math.max(0, this._activeRequestDispatches - 1);
    if (this._closePending && this._activeRequestDispatches === 0) {
      this._closePending = false;
      queueMicrotask(() => {
        this._startClose();
      });
    }
  }
  _startHostCloseWait() {
    this._hostCloseWaitStarted = true;
  }
  async _start(port, hostname) {
    if (typeof NetServer === "undefined") {
      throw new Error(
        "http.createServer requires kernel-backed network bridge support"
      );
    }
    debugBridgeNetwork("server listen start", this._serverId, port, hostname);
    const netServer = new NetServer({ allowHalfOpen: true });
    this._netServer = netServer;
    netServer.on("connection", (socket) => {
      this._emit("connection", socket);
      attachHttpServerSocket(this, socket);
    });
    netServer.on("error", (error) => {
      this._emit("error", error);
    });
    await new Promise((resolve, reject) => {
      let settled = false;
      const cleanup = () => {
        netServer.removeListener?.("listening", onListening);
        netServer.removeListener?.("error", onError);
      };
      const onListening = () => {
        if (settled) return;
        settled = true;
        cleanup();
        resolve();
      };
      const onError = (error) => {
        if (settled) return;
        settled = true;
        cleanup();
        reject(error instanceof Error ? error : new Error(String(error)));
      };
      netServer.once("listening", onListening);
      netServer.once("error", onError);
      netServer.listen(port ?? 0, hostname);
    });
    this._address = netServer.address();
    this.listening = true;
    this._startHostCloseWait();
    debugBridgeNetwork("server listening", this._serverId, this._address);
  }
  listen(portOrCb, hostOrCb, cb) {
    const port = typeof portOrCb === "number" ? portOrCb : void 0;
    const hostname = typeof hostOrCb === "string" ? hostOrCb : void 0;
    const callback = typeof cb === "function" ? cb : typeof hostOrCb === "function" ? hostOrCb : typeof portOrCb === "function" ? portOrCb : void 0;
    if (!this._listenPromise) {
      this._listenPromise = this._start(port, hostname).then(() => {
        this._emit("listening");
        callback?.call(this);
      }).catch((error) => {
        this._emit("error", error);
      });
    }
    return this;
  }
  close(cb) {
    debugBridgeNetwork("server close requested", this._serverId, this.listening);
    if (cb) {
      this._closeCallbacks.push(cb);
    }
    if (this._activeRequestDispatches > 0) {
      this._closePending = true;
      return this;
    }
    queueMicrotask(() => {
      this._startClose();
    });
    return this;
  }
  _startClose() {
    if (this._closeRunning) {
      return;
    }
    this._closeRunning = true;
    const run = async () => {
      try {
        if (this._listenPromise) {
          await this._listenPromise;
        }
        const netServer = this._netServer;
        if (this.listening && netServer) {
          debugBridgeNetwork("server close net server", this._serverId);
          await new Promise((resolve, reject) => {
            netServer.close((error) => {
              if (error) {
                reject(error);
              } else {
                resolve();
              }
            });
          });
        }
        this._netServer = null;
        this._completeClose();
        debugBridgeNetwork("server close complete", this._serverId);
        const callbacks = this._closeCallbacks.splice(0);
        callbacks.forEach((callback) => callback());
        this._emit("close");
      } catch (err) {
        const error = err instanceof Error ? err : new Error(String(err));
        debugBridgeNetwork("server close error", this._serverId, error.message);
        const callbacks = this._closeCallbacks.splice(0);
        callbacks.forEach((callback) => callback(error));
        this._emit("error", error);
      } finally {
        this._closeRunning = false;
      }
    };
    void run();
  }
  address() {
    return this._address;
  }
  on(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(listener);
    return this;
  }
  once(event, listener) {
    const wrapped = (...args) => {
      this.off(event, wrapped);
      listener.call(this, ...args);
    };
    return this.on(event, wrapped);
  }
  off(event, listener) {
    const listeners = this._listeners[event];
    if (!listeners) return this;
    const index = listeners.indexOf(listener);
    if (index !== -1) listeners.splice(index, 1);
    return this;
  }
  removeListener(event, listener) {
    return this.off(event, listener);
  }
  removeAllListeners(event) {
    if (event) {
      delete this._listeners[event];
    } else {
      this._listeners = {};
    }
    return this;
  }
  listenerCount(event) {
    return this._listeners[event]?.length || 0;
  }
  // Node.js Server timeout properties (no-op in sandbox)
  keepAliveTimeout = 5e3;
  requestTimeout = 3e5;
  headersTimeout = 6e4;
  timeout = 0;
  maxRequestsPerSocket = 0;
  setTimeout(_msecs, _callback) {
    if (typeof _msecs === "number") this.timeout = _msecs;
    return this;
  }
  ref() {
    return this;
  }
  unref() {
    return this;
  }
};
function ServerCallable(requestListener) {
  return new Server(requestListener);
}
ServerCallable.prototype = Server.prototype;
async function dispatchServerRequest(serverId, requestJson) {
  const server = serverInstances.get(serverId);
  if (!server) {
    throw new Error(`Unknown HTTP server: ${serverId}`);
  }
  const listener = server._requestListener;
  server._beginRequestDispatch();
  const request = JSON.parse(requestJson);
  const incoming = new ServerIncomingMessage(request);
  const outgoing = new ServerResponseBridge();
  incoming.socket = outgoing.socket;
  incoming.connection = outgoing.socket;
  const pendingImmediates = [];
  const pendingTimers = [];
  const trackedTimers = /* @__PURE__ */ new Map();
  let consumedTimerCount = 0;
  let consumedImmediateCount = 0;
  try {
    try {
      const originalSetImmediate = globalThis.setImmediate;
      const originalSetTimeout = globalThis.setTimeout;
      const originalClearTimeout = globalThis.clearTimeout;
      if (typeof originalSetImmediate === "function") {
        globalThis.setImmediate = ((callback, ...args) => {
          const pending = new Promise((resolve) => {
            queueMicrotask(() => {
              try {
                callback(...args);
              } finally {
                resolve();
              }
            });
          });
          pendingImmediates.push(pending);
          return 0;
        });
      }
      if (typeof originalSetTimeout === "function") {
        globalThis.setTimeout = ((callback, delay, ...args) => {
          if (typeof callback !== "function") {
            return originalSetTimeout(callback, delay, ...args);
          }
          const normalizedDelay = typeof delay === "number" && Number.isFinite(delay) ? Math.max(0, delay) : 0;
          if (normalizedDelay > 1e3) {
            return originalSetTimeout(callback, normalizedDelay, ...args);
          }
          let resolvePending;
          const pending = new Promise((resolve) => {
            resolvePending = resolve;
          });
          let handle;
          handle = originalSetTimeout(() => {
            trackedTimers.delete(handle);
            try {
              callback(...args);
            } finally {
              resolvePending();
            }
          }, normalizedDelay);
          trackedTimers.set(handle, resolvePending);
          pendingTimers.push(pending);
          return handle;
        });
      }
      if (typeof originalClearTimeout === "function") {
        globalThis.clearTimeout = ((handle) => {
          if (handle != null) {
            const resolvePending = trackedTimers.get(handle);
            if (resolvePending) {
              trackedTimers.delete(handle);
              resolvePending();
            }
          }
          return originalClearTimeout(handle);
        });
      }
      try {
        const listenerResult = listener(incoming, outgoing);
        if (incoming.rawBody && incoming.rawBody.length > 0) {
          incoming.emit("data", incoming.rawBody);
        }
        incoming.emit("end");
        await Promise.resolve(listenerResult);
        while (consumedTimerCount < pendingTimers.length || consumedImmediateCount < pendingImmediates.length) {
          const pending = [
            ...pendingTimers.slice(consumedTimerCount),
            ...pendingImmediates.slice(consumedImmediateCount)
          ];
          consumedTimerCount = pendingTimers.length;
          consumedImmediateCount = pendingImmediates.length;
          await Promise.allSettled(pending);
        }
      } finally {
        if (typeof originalSetImmediate === "function") {
          globalThis.setImmediate = originalSetImmediate;
        }
        if (typeof originalSetTimeout === "function") {
          globalThis.setTimeout = originalSetTimeout;
        }
        if (typeof originalClearTimeout === "function") {
          globalThis.clearTimeout = originalClearTimeout;
        }
      }
    } catch (err) {
      outgoing.statusCode = 500;
      try {
        outgoing.end(err instanceof Error ? `Error: ${err.message}` : "Error");
      } catch {
        if (!outgoing.writableFinished) outgoing.end();
      }
    }
    if (!outgoing.writableFinished) {
      outgoing.end();
    }
    await outgoing.waitForClose();
    await Promise.allSettled([...pendingTimers, ...pendingImmediates]);
    return JSON.stringify(outgoing.serialize());
  } finally {
    server._endRequestDispatch();
  }
}
async function dispatchHttp2CompatibilityRequest(serverId, requestId) {
  const pending = pendingHttp2CompatRequests.get(requestId);
  if (!pending || pending.serverId !== serverId || typeof _networkHttp2ServerRespondRaw === "undefined") {
    return;
  }
  pendingHttp2CompatRequests.delete(requestId);
  const server = http2Servers.get(serverId);
  if (!server) {
    _networkHttp2ServerRespondRaw.applySync(void 0, [
      serverId,
      requestId,
      JSON.stringify({
        status: 500,
        headers: [["content-type", "text/plain"]],
        body: "Unknown HTTP/2 server",
        bodyEncoding: "utf8"
      })
    ]);
    return;
  }
  const request = JSON.parse(pending.requestJson);
  const incoming = new ServerIncomingMessage(request);
  const outgoing = new ServerResponseBridge();
  incoming.socket = outgoing.socket;
  incoming.connection = outgoing.socket;
  try {
    server.emit("request", incoming, outgoing);
    if (incoming.rawBody && incoming.rawBody.length > 0) {
      incoming.emit("data", incoming.rawBody);
    }
    incoming.emit("end");
    if (!outgoing.writableFinished) {
      outgoing.end();
    }
    await outgoing.waitForClose();
    _networkHttp2ServerRespondRaw.applySync(void 0, [
      serverId,
      requestId,
      JSON.stringify(outgoing.serialize())
    ]);
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    _networkHttp2ServerRespondRaw.applySync(void 0, [
      serverId,
      requestId,
      JSON.stringify({
        status: 500,
        headers: [["content-type", "text/plain"]],
        body: `Error: ${message}`,
        bodyEncoding: "utf8"
      })
    ]);
  }
}
async function dispatchLoopbackServerRequest(serverOrId, requestInput) {
  const server = typeof serverOrId === "number" ? serverInstances.get(serverOrId) : serverOrId;
  if (!server) {
    throw new Error(
      `Unknown HTTP server: ${typeof serverOrId === "number" ? serverOrId : "<detached>"}`
    );
  }
  const request = typeof requestInput === "string" ? JSON.parse(requestInput) : requestInput;
  const incoming = new ServerIncomingMessage(request);
  const outgoing = new ServerResponseBridge();
  incoming.socket = outgoing.socket;
  incoming.connection = outgoing.socket;
  const pendingImmediates = [];
  const pendingTimers = [];
  const trackedTimers = /* @__PURE__ */ new Map();
  let consumedTimerCount = 0;
  let consumedImmediateCount = 0;
  server._beginRequestDispatch();
  try {
    try {
      const originalSetImmediate = globalThis.setImmediate;
      const originalSetTimeout = globalThis.setTimeout;
      const originalClearTimeout = globalThis.clearTimeout;
      if (typeof originalSetImmediate === "function") {
        globalThis.setImmediate = ((callback, ...args) => {
          const pending = new Promise((resolve) => {
            queueMicrotask(() => {
              try {
                callback(...args);
              } finally {
                resolve();
              }
            });
          });
          pendingImmediates.push(pending);
          return 0;
        });
      }
      if (typeof originalSetTimeout === "function") {
        globalThis.setTimeout = ((callback, delay, ...args) => {
          if (typeof callback !== "function") {
            return originalSetTimeout(callback, delay, ...args);
          }
          const normalizedDelay = typeof delay === "number" && Number.isFinite(delay) ? Math.max(0, delay) : 0;
          if (normalizedDelay > 1e3) {
            return originalSetTimeout(callback, normalizedDelay, ...args);
          }
          let resolvePending;
          const pending = new Promise((resolve) => {
            resolvePending = resolve;
          });
          let handle;
          handle = originalSetTimeout(() => {
            trackedTimers.delete(handle);
            try {
              callback(...args);
            } finally {
              resolvePending();
            }
          }, normalizedDelay);
          trackedTimers.set(handle, resolvePending);
          pendingTimers.push(pending);
          return handle;
        });
      }
      if (typeof originalClearTimeout === "function") {
        globalThis.clearTimeout = ((handle) => {
          if (handle != null) {
            const resolvePending = trackedTimers.get(handle);
            if (resolvePending) {
              trackedTimers.delete(handle);
              resolvePending();
            }
          }
          return originalClearTimeout(handle);
        });
      }
      try {
        const listenerResult = server._requestListener(incoming, outgoing);
        if (incoming.rawBody && incoming.rawBody.length > 0) {
          incoming.emit("data", incoming.rawBody);
        }
        incoming.emit("end");
        await Promise.resolve(listenerResult);
        while (consumedTimerCount < pendingTimers.length || consumedImmediateCount < pendingImmediates.length) {
          const pending = [
            ...pendingTimers.slice(consumedTimerCount),
            ...pendingImmediates.slice(consumedImmediateCount)
          ];
          consumedTimerCount = pendingTimers.length;
          consumedImmediateCount = pendingImmediates.length;
          await Promise.allSettled(pending);
        }
      } finally {
        if (typeof originalSetImmediate === "function") {
          globalThis.setImmediate = originalSetImmediate;
        }
        if (typeof originalSetTimeout === "function") {
          globalThis.setTimeout = originalSetTimeout;
        }
        if (typeof originalClearTimeout === "function") {
          globalThis.clearTimeout = originalClearTimeout;
        }
      }
    } catch (err) {
      outgoing.statusCode = 500;
      try {
        outgoing.end(err instanceof Error ? `Error: ${err.message}` : "Error");
      } catch {
        if (!outgoing.writableFinished) outgoing.end();
      }
    }
    if (!outgoing.writableFinished) {
      outgoing.end();
    }
    await outgoing.waitForClose();
    await Promise.allSettled([...pendingTimers, ...pendingImmediates]);
    let aborted = false;
    return {
      responseJson: JSON.stringify(outgoing.serialize()),
      abortRequest: () => {
        if (aborted) {
          return;
        }
        aborted = true;
        incoming._abort();
      }
    };
  } finally {
    server._endRequestDispatch();
  }
}
async function dispatchSocketBackedServerRequest(server, requestInput, streamSocket) {
  const request = typeof requestInput === "string" ? JSON.parse(requestInput) : requestInput;
  const incoming = new ServerIncomingMessage(request);
  const outgoing = new ServerResponseBridge();
  incoming.socket = outgoing.socket;
  incoming.connection = outgoing.socket;
  // Enable the streaming fast path so a single large `res.end(body)` is written
  // to the connection socket in slices instead of buffered + serialized whole.
  if (streamSocket) {
    outgoing._streamSocket = streamSocket;
    outgoing._streamRequest = request;
  }
  server._beginRequestDispatch();
  try {
    try {
      const listenerResult = server._requestListener(incoming, outgoing);
      if (incoming.rawBody && incoming.rawBody.length > 0) {
        incoming.emit("data", incoming.rawBody);
      }
      incoming.emit("end");
      await Promise.resolve(listenerResult);
    } catch (err) {
      outgoing.statusCode = 500;
      try {
        outgoing.end(err instanceof Error ? `Error: ${err.message}` : "Error");
      } catch {
        if (!outgoing.writableFinished) outgoing.end();
      }
    }
    if (!outgoing.writableFinished) {
      outgoing.end();
    }
    await outgoing.waitForClose();
    let aborted = false;
    const abortRequest = () => {
      if (aborted) {
        return;
      }
      aborted = true;
      incoming._abort();
    };
    if (outgoing._streamedDirectly) {
      // Response already written straight to the socket; nothing left to serialize.
      return {
        streamedDirectly: true,
        closeConnection: outgoing._streamCloseConnection,
        abortRequest
      };
    }
    return {
      responseJson: JSON.stringify(outgoing.serialize()),
      abortRequest
    };
  } finally {
    server._endRequestDispatch();
  }
}
function attachHttpServerSocket(server, socket) {
  let buffer = Buffer.alloc(0);
  let dispatchRunning = false;
  let dispatchPending = false;
  let ended = false;
  let detached = false;
  const cleanup = () => {
    if (detached) {
      return;
    }
    detached = true;
    socket.off?.("data", onData);
    socket.removeListener?.("data", onData);
    socket.off?.("end", onEnd);
    socket.removeListener?.("end", onEnd);
    socket.off?.("close", onClose);
    socket.removeListener?.("close", onClose);
    socket.off?.("error", onError);
    socket.removeListener?.("error", onError);
  };
  const scheduleDispatch = () => {
    if (dispatchRunning) {
      dispatchPending = true;
      return;
    }
    dispatchRunning = true;
    void processRequests().finally(() => {
      dispatchRunning = false;
      if (dispatchPending && !detached) {
        dispatchPending = false;
        scheduleDispatch();
      } else {
        dispatchPending = false;
      }
    });
  };
  const finishSocket = () => {
    cleanup();
    if (!socket.destroyed && !socket._writableEnded) {
      socket.end();
    }
  };
  const onData = (chunk) => {
    const payload = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
    buffer = buffer.length === 0 ? payload : Buffer.concat([buffer, payload]);
    scheduleDispatch();
  };
  const onEnd = () => {
    ended = true;
    if (buffer.length === 0) {
      cleanup();
      return;
    }
    scheduleDispatch();
  };
  const onClose = () => {
    cleanup();
  };
  const onError = () => {
    cleanup();
  };
  async function processRequests() {
    let closeAfterDrain = false;
    while (!detached && !socket.destroyed) {
      const parsed = parseLoopbackRequestBuffer(buffer, server);
      if (parsed.kind === "incomplete") {
        if (ended && buffer.length > 0) {
          socket.write(createBadRequestResponseBuffer());
          finishSocket();
        }
        return;
      }
      if (parsed.kind === "bad-request") {
        socket.write(createBadRequestResponseBuffer());
        finishSocket();
        buffer = Buffer.alloc(0);
        return;
      }
      buffer = buffer.subarray(parsed.bytesConsumed);
      if (parsed.upgradeHead) {
        cleanup();
        const incoming = new ServerIncomingMessage(parsed.request);
        incoming.socket = socket;
        incoming.connection = socket;
        server._emit("upgrade", incoming, socket, parsed.upgradeHead);
        return;
      }
      const result = await dispatchSocketBackedServerRequest(
        server,
        parsed.request,
        socket,
      );
      if (detached || socket.destroyed) {
        return;
      }
      // Keep-alive for socket-backed HTTP servers is intentionally deferred:
      // pipelined bytes already in `buffer` drain, then this connection closes.
      // Revisit when the bridge owns full Node-compatible request lifecycle
      // timers and per-socket request limits.
      let mustClose;
      if (result.streamedDirectly) {
        // Response was already streamed straight to the socket by res.end().
        mustClose = result.closeConnection;
      } else {
        const response = JSON.parse(result.responseJson);
        const serialized = serializeLoopbackResponse(response, parsed.request, true);
        if (!closeAfterDrain && serialized.payload.length > 0) {
          socket.write(serialized.payload);
        }
        mustClose = serialized.closeConnection;
      }
      if (mustClose) {
        closeAfterDrain = true;
        if (buffer.length === 0) {
          finishSocket();
          return;
        }
      }
    }
  }
  socket.on("data", onData);
  socket.once("end", onEnd);
  socket.once("close", onClose);
  socket.once("error", onError);
}
function dispatchSocketRequest(event, serverId, requestJson, headBase64, socketId) {
  const server = serverInstances.get(serverId);
  if (!server) {
    throw new Error(`Unknown HTTP server for ${event}: ${serverId}`);
  }
  const request = JSON.parse(requestJson);
  const incoming = new ServerIncomingMessage(request);
  const head = typeof Buffer !== "undefined" ? Buffer.from(headBase64, "base64") : new Uint8Array(0);
  const hostHeader = incoming.headers["host"];
  const socket = new UpgradeSocket(socketId, {
    host: (Array.isArray(hostHeader) ? hostHeader[0] : hostHeader)?.split(":")[0] || "127.0.0.1"
  });
  upgradeSocketInstances.set(socketId, socket);
  server._emit(event, incoming, socket, head);
}
var upgradeSocketInstances = /* @__PURE__ */ new Map();
var UpgradeSocket = class {
  remoteAddress;
  remotePort;
  localAddress = "127.0.0.1";
  localPort = 0;
  connecting = false;
  destroyed = false;
  writable = true;
  readable = true;
  readyState = "open";
  bytesWritten = 0;
  _listeners = {};
  _socketId;
  // Readable stream state stub for ws compatibility (socketOnClose checks _readableState.endEmitted)
  _readableState = { endEmitted: false, ended: false };
  _writableState = { finished: false, errorEmitted: false };
  constructor(socketId, options) {
    this._socketId = socketId;
    this.remoteAddress = options?.host || "127.0.0.1";
    this.remotePort = options?.port || 80;
  }
  setTimeout(_ms, _cb) {
    return this;
  }
  setNoDelay(_noDelay) {
    return this;
  }
  setKeepAlive(_enable, _delay) {
    return this;
  }
  ref() {
    return this;
  }
  unref() {
    return this;
  }
  cork() {
  }
  uncork() {
  }
  pause() {
    return this;
  }
  resume() {
    return this;
  }
  address() {
    return { address: this.localAddress, family: "IPv4", port: this.localPort };
  }
  on(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(listener);
    return this;
  }
  addListener(event, listener) {
    return this.on(event, listener);
  }
  once(event, listener) {
    const wrapper = (...args) => {
      this.off(event, wrapper);
      listener(...args);
    };
    return this.on(event, wrapper);
  }
  off(event, listener) {
    if (this._listeners[event]) {
      const idx = this._listeners[event].indexOf(listener);
      if (idx !== -1) this._listeners[event].splice(idx, 1);
    }
    return this;
  }
  removeListener(event, listener) {
    return this.off(event, listener);
  }
  removeAllListeners(event) {
    if (event) {
      delete this._listeners[event];
    } else {
      this._listeners = {};
    }
    return this;
  }
  emit(event, ...args) {
    const handlers = this._listeners[event];
    return dispatchCustomEmitterListeners(this, handlers, args);
  }
  listenerCount(event) {
    return this._listeners[event]?.length || 0;
  }
  write(data, encodingOrCb, cb) {
    if (this.destroyed) return false;
    const callback = typeof encodingOrCb === "function" ? encodingOrCb : cb;
    if (typeof _upgradeSocketWriteRaw !== "undefined") {
      let base64;
      if (typeof Buffer !== "undefined" && Buffer.isBuffer(data)) {
        base64 = data.toString("base64");
      } else if (typeof data === "string") {
        base64 = typeof Buffer !== "undefined" ? Buffer.from(data).toString("base64") : btoa(data);
      } else if (data instanceof Uint8Array) {
        base64 = typeof Buffer !== "undefined" ? Buffer.from(data).toString("base64") : btoa(String.fromCharCode(...data));
      } else {
        base64 = typeof Buffer !== "undefined" ? Buffer.from(String(data)).toString("base64") : btoa(String(data));
      }
      this.bytesWritten += base64.length;
      _upgradeSocketWriteRaw.applySync(void 0, [this._socketId, base64]);
    }
    if (callback) callback();
    return true;
  }
  end(data) {
    if (data) this.write(data);
    if (typeof _upgradeSocketEndRaw !== "undefined" && !this.destroyed) {
      _upgradeSocketEndRaw.applySync(void 0, [this._socketId]);
    }
    this.writable = false;
    this.emit("finish");
    return this;
  }
  destroy(err) {
    if (this.destroyed) return this;
    this.destroyed = true;
    this.writable = false;
    this.readable = false;
    this._readableState.endEmitted = true;
    this._readableState.ended = true;
    this._writableState.finished = true;
    if (typeof _upgradeSocketDestroyRaw !== "undefined") {
      _upgradeSocketDestroyRaw.applySync(void 0, [this._socketId]);
    }
    upgradeSocketInstances.delete(this._socketId);
    if (err) this.emit("error", err);
    this.emit("close", false);
    return this;
  }
  // Push data received from the host into this socket
  _pushData(data) {
    this.emit("data", data);
  }
  // Signal end-of-stream from the host
  _pushEnd() {
    this.readable = false;
    this._readableState.endEmitted = true;
    this._readableState.ended = true;
    this._writableState.finished = true;
    this.emit("end");
    this.emit("close", false);
    upgradeSocketInstances.delete(this._socketId);
  }
};
function dispatchUpgradeRequest(serverId, requestJson, headBase64, socketId) {
  dispatchSocketRequest("upgrade", serverId, requestJson, headBase64, socketId);
}
function dispatchConnectRequest(serverId, requestJson, headBase64, socketId) {
  dispatchSocketRequest("connect", serverId, requestJson, headBase64, socketId);
}
function onUpgradeSocketData(socketId, dataBase64) {
  const socket = upgradeSocketInstances.get(socketId);
  if (socket) {
    const data = typeof Buffer !== "undefined" ? Buffer.from(dataBase64, "base64") : new Uint8Array(0);
    socket._pushData(data);
  }
}
function onUpgradeSocketEnd(socketId) {
  const socket = upgradeSocketInstances.get(socketId);
  if (socket) {
    socket._pushEnd();
  }
}
function ServerResponseCallable() {
  this.statusCode = 200;
  this.statusMessage = "OK";
  this.headersSent = false;
  this.writable = true;
  this.writableFinished = false;
  this.outputSize = 0;
  this._headers = /* @__PURE__ */ new Map();
  this._trailers = /* @__PURE__ */ new Map();
  this._rawHeaderNames = /* @__PURE__ */ new Map();
  this._rawTrailerNames = /* @__PURE__ */ new Map();
  this._informational = [];
  this._pendingRawInfoBuffer = "";
  this._chunks = [];
  this._chunksBytes = 0;
  this._listeners = {};
  this._closedPromise = new Promise((resolve) => {
    this._resolveClosed = resolve;
  });
  this._connectionEnded = false;
  this._connectionReset = false;
  this._writableState = { length: 0, ended: false, finished: false, objectMode: false, corked: 0 };
  const fakeSocket = {
    writable: true,
    writableCorked: 0,
    writableHighWaterMark: 16 * 1024,
    on() {
      return fakeSocket;
    },
    once() {
      return fakeSocket;
    },
    removeListener() {
      return fakeSocket;
    },
    destroy() {
    },
    end() {
    },
    cork() {
    },
    uncork() {
    },
    write: (chunk, encodingOrCallback, callback) => {
      return this.write(chunk, encodingOrCallback, callback);
    }
  };
  this.socket = fakeSocket;
  this.connection = fakeSocket;
}
ServerResponseCallable.prototype = Object.create(ServerResponseBridge.prototype, {
  constructor: { value: ServerResponseCallable, writable: true, configurable: true }
});
function createHttpModule(protocol) {
  const defaultProtocol = protocol === "https" ? "https:" : "http:";
  const moduleAgent = new Agent({
    keepAlive: false,
    createConnection(options, cb) {
      return createHttpRequestSocket({ ...options, protocol: defaultProtocol }, cb);
    }
  });
  function ensureProtocol(opts) {
    if (!opts.protocol) return { ...opts, protocol: defaultProtocol };
    return opts;
  }
  function withModuleDefaultAgent(opts) {
    if (opts.agent !== void 0) {
      return opts;
    }
    return {
      ...opts,
      _agentOSDefaultAgent: moduleAgent
    };
  }
  return {
    request(options, optionsOrCallback, maybeCallback) {
      let opts;
      const callback = typeof optionsOrCallback === "function" ? optionsOrCallback : maybeCallback;
      if (typeof options === "string") {
        const url = new URL(options);
        opts = {
          protocol: url.protocol,
          hostname: url.hostname,
          port: url.port,
          path: url.pathname + url.search,
          ...typeof optionsOrCallback === "object" && optionsOrCallback ? optionsOrCallback : {}
        };
      } else if (options instanceof URL) {
        opts = {
          protocol: options.protocol,
          hostname: options.hostname,
          port: options.port,
          path: options.pathname + options.search,
          ...typeof optionsOrCallback === "object" && optionsOrCallback ? optionsOrCallback : {}
        };
      } else {
        opts = {
          ...options,
          ...typeof optionsOrCallback === "object" && optionsOrCallback ? optionsOrCallback : {}
        };
      }
      return new ClientRequest(withModuleDefaultAgent(ensureProtocol(opts)), callback);
    },
    get(options, optionsOrCallback, maybeCallback) {
      let opts;
      const callback = typeof optionsOrCallback === "function" ? optionsOrCallback : maybeCallback;
      if (typeof options === "string") {
        const url = new URL(options);
        opts = {
          protocol: url.protocol,
          hostname: url.hostname,
          port: url.port,
          path: url.pathname + url.search,
          method: "GET",
          ...typeof optionsOrCallback === "object" && optionsOrCallback ? optionsOrCallback : {}
        };
      } else if (options instanceof URL) {
        opts = {
          protocol: options.protocol,
          hostname: options.hostname,
          port: options.port,
          path: options.pathname + options.search,
          method: "GET",
          ...typeof optionsOrCallback === "object" && optionsOrCallback ? optionsOrCallback : {}
        };
      } else {
        opts = {
          ...options,
          ...typeof optionsOrCallback === "object" && optionsOrCallback ? optionsOrCallback : {},
          method: "GET"
        };
      }
      const req = new ClientRequest(withModuleDefaultAgent(ensureProtocol(opts)), callback);
      req.end();
      return req;
    },
    createServer(_optionsOrListener, maybeListener) {
      const listener = typeof _optionsOrListener === "function" ? _optionsOrListener : maybeListener;
      return new Server(listener);
    },
    Agent,
    globalAgent: moduleAgent,
    Server: ServerCallable,
    ServerResponse: ServerResponseCallable,
    IncomingMessage,
    ClientRequest,
    validateHeaderName,
    validateHeaderValue,
    _checkIsHttpToken: checkIsHttpToken,
    _checkInvalidHeaderChar: checkInvalidHeaderChar,
    maxHeaderSize: 65535,
    METHODS: [...HTTP_METHODS],
    STATUS_CODES: HTTP_STATUS_TEXT
  };
}
var http = createHttpModule("http");
var https = createHttpModule("https");
var HTTP2_K_SOCKET = /* @__PURE__ */ Symbol.for("secure-exec.http2.kSocket");
var HTTP2_OPTIONS = /* @__PURE__ */ Symbol("options");
var http2Servers = /* @__PURE__ */ new Map();
var http2Sessions = /* @__PURE__ */ new Map();
var http2Streams = /* @__PURE__ */ new Map();
var pendingHttp2ClientStreamEvents = /* @__PURE__ */ new Map();
var scheduledHttp2ClientStreamFlushes = /* @__PURE__ */ new Set();
var queuedHttp2DispatchEvents = [];
var pendingHttp2CompatRequests = /* @__PURE__ */ new Map();
var scheduledHttp2DispatchDrain = false;
var nextHttp2ServerId = 1;
var Http2EventEmitter = class {
  _listeners = {};
  _onceListeners = {};
  on(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(listener);
    return this;
  }
  addListener(event, listener) {
    return this.on(event, listener);
  }
  once(event, listener) {
    if (!this._onceListeners[event]) this._onceListeners[event] = [];
    this._onceListeners[event].push(listener);
    return this;
  }
  removeListener(event, listener) {
    const remove = (target) => {
      if (!target) return;
      const index = target.indexOf(listener);
      if (index !== -1) target.splice(index, 1);
    };
    remove(this._listeners[event]);
    remove(this._onceListeners[event]);
    return this;
  }
  off(event, listener) {
    return this.removeListener(event, listener);
  }
  listenerCount(event) {
    return (this._listeners[event]?.length ?? 0) + (this._onceListeners[event]?.length ?? 0);
  }
  setMaxListeners(_value) {
    return this;
  }
  emit(event, ...args) {
    let handled = false;
    const listeners = this._listeners[event];
    if (listeners) {
      for (const listener of [...listeners]) {
        listener.call(this, ...args);
        handled = true;
      }
    }
    const onceListeners = this._onceListeners[event];
    if (onceListeners) {
      this._onceListeners[event] = [];
      for (const listener of [...onceListeners]) {
        listener.call(this, ...args);
        handled = true;
      }
    }
    return handled;
  }
};
var Http2SocketProxy = class extends Http2EventEmitter {
  allowHalfOpen = false;
  encrypted = false;
  localAddress = "127.0.0.1";
  localPort = 0;
  localFamily = "IPv4";
  remoteAddress = "127.0.0.1";
  remotePort = 0;
  remoteFamily = "IPv4";
  servername;
  alpnProtocol = false;
  readable = true;
  writable = true;
  destroyed = false;
  _bridgeReadPollTimer = null;
  _loopbackServer = null;
  _onDestroy;
  _destroyCallbackInvoked = false;
  constructor(state, onDestroy) {
    super();
    this._onDestroy = onDestroy;
    this._applyState(state);
  }
  _applyState(state) {
    if (!state) return;
    this.allowHalfOpen = state.allowHalfOpen === true;
    this.encrypted = state.encrypted === true;
    this.localAddress = state.localAddress ?? this.localAddress;
    this.localPort = state.localPort ?? this.localPort;
    this.localFamily = state.localFamily ?? this.localFamily;
    this.remoteAddress = state.remoteAddress ?? this.remoteAddress;
    this.remotePort = state.remotePort ?? this.remotePort;
    this.remoteFamily = state.remoteFamily ?? this.remoteFamily;
    this.servername = state.servername;
    this.alpnProtocol = state.alpnProtocol ?? this.alpnProtocol;
  }
  _clearTimeoutTimer() {
  }
  _emitNet(event, error) {
    if (event === "error" && error) {
      this.emit("error", error);
      return;
    }
    if (event === "close") {
      if (!this._destroyCallbackInvoked) {
        this._destroyCallbackInvoked = true;
        queueMicrotask(() => {
          this._onDestroy?.();
        });
      }
      this.emit("close");
    }
  }
  end() {
    this.destroyed = true;
    this.readable = false;
    this.writable = false;
    this.emit("close");
    return this;
  }
  destroy() {
    if (this.destroyed) {
      return this;
    }
    this.destroyed = true;
    this.readable = false;
    this.writable = false;
    this._emitNet("close");
    return this;
  }
};
function createHttp2ArgTypeError(argumentName, expected, value) {
  return createTypeErrorWithCode(
    `The "${argumentName}" argument must be of type ${expected}. Received ${formatReceivedType(value)}`,
    "ERR_INVALID_ARG_TYPE"
  );
}
function createHttp2Error(code, message) {
  return createErrorWithCode(message, code);
}
function createHttp2SettingRangeError(setting, value) {
  const error = new RangeError(
    `Invalid value for setting "${setting}": ${String(value)}`
  );
  error.code = "ERR_HTTP2_INVALID_SETTING_VALUE";
  return error;
}
function createHttp2SettingTypeError(setting, value) {
  const error = new TypeError(
    `Invalid value for setting "${setting}": ${String(value)}`
  );
  error.code = "ERR_HTTP2_INVALID_SETTING_VALUE";
  return error;
}
var HTTP2_INTERNAL_BINDING_CONSTANTS = {
  NGHTTP2_NO_ERROR: 0,
  NGHTTP2_PROTOCOL_ERROR: 1,
  NGHTTP2_INTERNAL_ERROR: 2,
  NGHTTP2_FLOW_CONTROL_ERROR: 3,
  NGHTTP2_SETTINGS_TIMEOUT: 4,
  NGHTTP2_STREAM_CLOSED: 5,
  NGHTTP2_FRAME_SIZE_ERROR: 6,
  NGHTTP2_REFUSED_STREAM: 7,
  NGHTTP2_CANCEL: 8,
  NGHTTP2_COMPRESSION_ERROR: 9,
  NGHTTP2_CONNECT_ERROR: 10,
  NGHTTP2_ENHANCE_YOUR_CALM: 11,
  NGHTTP2_INADEQUATE_SECURITY: 12,
  NGHTTP2_HTTP_1_1_REQUIRED: 13,
  NGHTTP2_NV_FLAG_NONE: 0,
  NGHTTP2_NV_FLAG_NO_INDEX: 1,
  NGHTTP2_ERR_DEFERRED: -508,
  NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE: -509,
  NGHTTP2_ERR_STREAM_CLOSED: -510,
  NGHTTP2_ERR_INVALID_ARGUMENT: -501,
  NGHTTP2_ERR_FRAME_SIZE_ERROR: -522,
  NGHTTP2_ERR_NOMEM: -901,
  NGHTTP2_FLAG_NONE: 0,
  NGHTTP2_FLAG_END_STREAM: 1,
  NGHTTP2_FLAG_END_HEADERS: 4,
  NGHTTP2_FLAG_ACK: 1,
  NGHTTP2_FLAG_PADDED: 8,
  NGHTTP2_FLAG_PRIORITY: 32,
  NGHTTP2_DEFAULT_WEIGHT: 16,
  NGHTTP2_SETTINGS_HEADER_TABLE_SIZE: 1,
  NGHTTP2_SETTINGS_ENABLE_PUSH: 2,
  NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS: 3,
  NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE: 4,
  NGHTTP2_SETTINGS_MAX_FRAME_SIZE: 5,
  NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE: 6,
  NGHTTP2_SETTINGS_ENABLE_CONNECT_PROTOCOL: 8
};
var HTTP2_NGHTTP2_ERROR_MESSAGES = {
  [HTTP2_INTERNAL_BINDING_CONSTANTS.NGHTTP2_ERR_DEFERRED]: "Data deferred",
  [HTTP2_INTERNAL_BINDING_CONSTANTS.NGHTTP2_ERR_STREAM_ID_NOT_AVAILABLE]: "Stream ID is not available",
  [HTTP2_INTERNAL_BINDING_CONSTANTS.NGHTTP2_ERR_STREAM_CLOSED]: "Stream was already closed or invalid",
  [HTTP2_INTERNAL_BINDING_CONSTANTS.NGHTTP2_ERR_INVALID_ARGUMENT]: "Invalid argument",
  [HTTP2_INTERNAL_BINDING_CONSTANTS.NGHTTP2_ERR_FRAME_SIZE_ERROR]: "Frame size error",
  [HTTP2_INTERNAL_BINDING_CONSTANTS.NGHTTP2_ERR_NOMEM]: "Out of memory"
};
var NghttpError = class extends Error {
  code = "ERR_HTTP2_ERROR";
  constructor(message) {
    super(message);
    this.name = "Error";
  }
};
function nghttp2ErrorString(code) {
  return HTTP2_NGHTTP2_ERROR_MESSAGES[code] ?? `HTTP/2 error (${String(code)})`;
}
function createHttp2InvalidArgValueError(property, value) {
  return createTypeErrorWithCode(
    `The property 'options.${property}' is invalid. Received ${formatHttp2InvalidValue(value)}`,
    "ERR_INVALID_ARG_VALUE"
  );
}
function formatHttp2InvalidValue(value) {
  if (typeof value === "function") {
    return `[Function${value.name ? `: ${value.name}` : ": function"}]`;
  }
  if (typeof value === "symbol") {
    return value.toString();
  }
  if (Array.isArray(value)) {
    return "[]";
  }
  if (value === null) {
    return "null";
  }
  if (typeof value === "object") {
    return "{}";
  }
  return String(value);
}
function createHttp2PayloadForbiddenError(statusCode) {
  return createHttp2Error(
    "ERR_HTTP2_PAYLOAD_FORBIDDEN",
    `Responses with ${String(statusCode)} status must not have a payload`
  );
}
var S_IFMT = 61440;
var S_IFDIR = 16384;
var S_IFREG = 32768;
var S_IFIFO = 4096;
var S_IFSOCK = 49152;
var S_IFLNK = 40960;
function createHttp2BridgeStat(stat) {
  const atimeMs = stat.atimeMs ?? 0;
  const mtimeMs = stat.mtimeMs ?? atimeMs;
  const ctimeMs = stat.ctimeMs ?? mtimeMs;
  const birthtimeMs = stat.birthtimeMs ?? ctimeMs;
  const fileType = stat.mode & S_IFMT;
  return {
    size: stat.size,
    mode: stat.mode,
    atimeMs,
    mtimeMs,
    ctimeMs,
    birthtimeMs,
    atime: new Date(atimeMs),
    mtime: new Date(mtimeMs),
    ctime: new Date(ctimeMs),
    birthtime: new Date(birthtimeMs),
    isFile: () => fileType === S_IFREG,
    isDirectory: () => fileType === S_IFDIR,
    isFIFO: () => fileType === S_IFIFO,
    isSocket: () => fileType === S_IFSOCK,
    isSymbolicLink: () => fileType === S_IFLNK
  };
}
function normalizeHttp2FileResponseOptions(options) {
  const normalized = options ?? {};
  const offset = normalized.offset;
  if (offset !== void 0 && (typeof offset !== "number" || !Number.isFinite(offset))) {
    throw createHttp2InvalidArgValueError("offset", offset);
  }
  const length = normalized.length;
  if (length !== void 0 && (typeof length !== "number" || !Number.isFinite(length))) {
    throw createHttp2InvalidArgValueError("length", length);
  }
  const statCheck = normalized.statCheck;
  if (statCheck !== void 0 && typeof statCheck !== "function") {
    throw createHttp2InvalidArgValueError("statCheck", statCheck);
  }
  const onError = normalized.onError;
  return {
    offset: offset === void 0 ? 0 : Math.max(0, Math.trunc(offset)),
    length: typeof length === "number" ? Math.trunc(length) : void 0,
    statCheck: typeof statCheck === "function" ? statCheck : void 0,
    onError: typeof onError === "function" ? onError : void 0
  };
}
function sliceHttp2FileBody(body, offset, length) {
  const safeOffset = Math.max(0, Math.min(offset, body.length));
  if (length === void 0 || length < 0) {
    return body.subarray(safeOffset);
  }
  return body.subarray(safeOffset, Math.min(body.length, safeOffset + length));
}
var Http2Stream = class {
  constructor(_streamId) {
    this._streamId = _streamId;
  }
  _streamId;
  respond(headers) {
    if (typeof _networkHttp2StreamRespondRaw === "undefined") {
      throw new Error("http2 server stream respond bridge is not available");
    }
    _networkHttp2StreamRespondRaw.applySync(void 0, [
      this._streamId,
      serializeHttp2Headers(headers)
    ]);
    return 0;
  }
};
var DEFAULT_HTTP2_SETTINGS = {
  headerTableSize: 4096,
  enablePush: true,
  initialWindowSize: 65535,
  maxFrameSize: 16384,
  maxConcurrentStreams: 4294967295,
  maxHeaderListSize: 65535,
  maxHeaderSize: 65535,
  enableConnectProtocol: false
};
var DEFAULT_HTTP2_SESSION_STATE = {
  effectiveLocalWindowSize: 65535,
  localWindowSize: 65535,
  remoteWindowSize: 65535,
  nextStreamID: 1,
  outboundQueueSize: 1,
  deflateDynamicTableSize: 0,
  inflateDynamicTableSize: 0
};
function cloneHttp2Settings(settings) {
  const cloned = {};
  for (const [key, value] of Object.entries(settings ?? {})) {
    if (key === "customSettings" && value && typeof value === "object") {
      const customSettings = {};
      for (const [customKey, customValue] of Object.entries(value)) {
        customSettings[Number(customKey)] = Number(customValue);
      }
      cloned.customSettings = customSettings;
      continue;
    }
    cloned[key] = value;
  }
  return cloned;
}
function cloneHttp2SessionRuntimeState(state) {
  return {
    ...DEFAULT_HTTP2_SESSION_STATE,
    ...state ?? {}
  };
}
function parseHttp2SessionRuntimeState(state) {
  if (!state || typeof state !== "object") {
    return void 0;
  }
  const record = state;
  const parsed = {};
  const numericKeys = [
    "effectiveLocalWindowSize",
    "localWindowSize",
    "remoteWindowSize",
    "nextStreamID",
    "outboundQueueSize",
    "deflateDynamicTableSize",
    "inflateDynamicTableSize"
  ];
  for (const key of numericKeys) {
    if (typeof record[key] === "number") {
      parsed[key] = record[key];
    }
  }
  return parsed;
}
function validateHttp2Settings(settings, argumentName = "settings") {
  if (!settings || typeof settings !== "object" || Array.isArray(settings)) {
    throw createHttp2ArgTypeError(argumentName, "object", settings);
  }
  const record = settings;
  const normalized = {};
  const numberRanges = {
    headerTableSize: [0, 4294967295],
    initialWindowSize: [0, 4294967295],
    maxFrameSize: [16384, 16777215],
    maxConcurrentStreams: [0, 4294967295],
    maxHeaderListSize: [0, 4294967295],
    maxHeaderSize: [0, 4294967295]
  };
  for (const [key, value] of Object.entries(record)) {
    if (value === void 0) {
      continue;
    }
    if (key === "enablePush" || key === "enableConnectProtocol") {
      if (typeof value !== "boolean") {
        throw createHttp2SettingTypeError(key, value);
      }
      normalized[key] = value;
      continue;
    }
    if (key === "customSettings") {
      if (!value || typeof value !== "object" || Array.isArray(value)) {
        throw createHttp2SettingRangeError(key, value);
      }
      const customSettings = {};
      for (const [customKey, customValue] of Object.entries(value)) {
        const numericKey = Number(customKey);
        if (!Number.isInteger(numericKey) || numericKey < 0 || numericKey > 65535) {
          throw createHttp2SettingRangeError(key, value);
        }
        if (typeof customValue !== "number" || !Number.isInteger(customValue) || customValue < 0 || customValue > 4294967295) {
          throw createHttp2SettingRangeError(key, value);
        }
        customSettings[numericKey] = customValue;
      }
      normalized.customSettings = customSettings;
      continue;
    }
    if (key in numberRanges) {
      const [min, max] = numberRanges[key];
      if (typeof value !== "number" || !Number.isInteger(value) || value < min || value > max) {
        throw createHttp2SettingRangeError(key, value);
      }
      normalized[key] = value;
      continue;
    }
    normalized[key] = value;
  }
  return normalized;
}
function serializeHttp2Headers(headers) {
  return JSON.stringify(headers ?? {});
}
function parseHttp2Headers(headersJson) {
  if (!headersJson) {
    return {};
  }
  try {
    const parsed = JSON.parse(headersJson);
    return parsed && typeof parsed === "object" ? parsed : {};
  } catch {
    return {};
  }
}
function parseHttp2SessionState(data) {
  if (!data) {
    return null;
  }
  try {
    const parsed = JSON.parse(data);
    return parsed && typeof parsed === "object" ? parsed : null;
  } catch {
    return null;
  }
}
function parseHttp2SocketState(data) {
  if (!data) {
    return null;
  }
  try {
    const parsed = JSON.parse(data);
    return parsed && typeof parsed === "object" ? parsed : null;
  } catch {
    return null;
  }
}
function parseHttp2ErrorPayload(data) {
  if (!data) {
    return new Error("Unknown HTTP/2 bridge error");
  }
  try {
    const parsed = JSON.parse(data);
    const error = new Error(parsed.message ?? "Unknown HTTP/2 bridge error");
    if (parsed.name) error.name = parsed.name;
    if (parsed.code) error.code = parsed.code;
    return error;
  } catch {
    return new Error(data);
  }
}
function normalizeHttp2Headers(headers) {
  const normalized = {};
  if (!headers || typeof headers !== "object") {
    return normalized;
  }
  for (const [key, value] of Object.entries(headers)) {
    normalized[String(key)] = value;
  }
  return normalized;
}
function validateHttp2RequestOptions(options) {
  if (!options) {
    return;
  }
  const validators = {
    endStream: "boolean",
    weight: "number",
    parent: "number",
    exclusive: "boolean",
    silent: "boolean"
  };
  for (const [key, expectedType] of Object.entries(validators)) {
    if (!(key in options) || options[key] === void 0) {
      continue;
    }
    const value = options[key];
    if (expectedType === "boolean" && typeof value !== "boolean") {
      throw createHttp2ArgTypeError(key, "boolean", value);
    }
    if (expectedType === "number" && typeof value !== "number") {
      throw createHttp2ArgTypeError(key, "number", value);
    }
  }
}
function validateHttp2ConnectOptions(options) {
  if (!options || !options.settings || typeof options.settings !== "object") {
    return;
  }
  const settings = options.settings;
  if ("maxFrameSize" in settings) {
    const value = settings.maxFrameSize;
    if (typeof value !== "number" || !Number.isInteger(value) || value < 16384 || value > 16777215) {
      throw createHttp2SettingRangeError("maxFrameSize", value);
    }
  }
}
function applyHttp2SessionState(session, state) {
  if (!state) {
    return;
  }
  session.encrypted = state.encrypted === true;
  session.alpnProtocol = state.alpnProtocol ?? (session.encrypted ? "h2" : "h2c");
  session.originSet = Array.isArray(state.originSet) && state.originSet.length > 0 ? [...state.originSet] : session.encrypted ? [] : void 0;
  if (state.localSettings && typeof state.localSettings === "object") {
    session.localSettings = cloneHttp2Settings(state.localSettings);
  }
  if (state.remoteSettings && typeof state.remoteSettings === "object") {
    session.remoteSettings = cloneHttp2Settings(state.remoteSettings);
  }
  if (state.state && typeof state.state === "object") {
    session._applyRuntimeState(parseHttp2SessionRuntimeState(state.state));
  }
  session.socket._applyState(state.socket);
}
function normalizeHttp2Authority(authority, options) {
  if (authority instanceof URL) {
    return authority;
  }
  if (typeof authority === "string") {
    return new URL(authority);
  }
  if (authority && typeof authority === "object") {
    const record = authority;
    const protocol = typeof (options?.protocol ?? record.protocol) === "string" ? String(options?.protocol ?? record.protocol) : "http:";
    const hostname = typeof (options?.host ?? record.host ?? options?.hostname ?? record.hostname) === "string" ? String(options?.host ?? record.host ?? options?.hostname ?? record.hostname) : "localhost";
    const portValue = options?.port ?? record.port;
    const port = portValue === void 0 ? "" : String(portValue);
    return new URL(`${protocol}//${hostname}${port ? `:${port}` : ""}`);
  }
  return new URL("http://localhost");
}
function normalizeHttp2ConnectArgs(authorityOrOptions, optionsOrListener, maybeListener) {
  const listener = typeof optionsOrListener === "function" ? optionsOrListener : typeof maybeListener === "function" ? maybeListener : void 0;
  const options = typeof optionsOrListener === "function" ? {} : optionsOrListener ?? {};
  return {
    authority: normalizeHttp2Authority(authorityOrOptions, options),
    options,
    listener
  };
}
function resolveHttp2SocketId(socket) {
  if (!socket || typeof socket !== "object") {
    return void 0;
  }
  const value = socket._socketId;
  return typeof value === "number" && Number.isFinite(value) ? value : void 0;
}
var ClientHttp2Stream = class extends Http2EventEmitter {
  _streamId;
  _encoding;
  _utf8Remainder;
  _isPushStream;
  _session;
  _receivedResponse = false;
  _needsDrain = false;
  _pendingWritableBytes = 0;
  _drainScheduled = false;
  _writableHighWaterMark = 16 * 1024;
  rstCode = 0;
  readable = true;
  writable = true;
  writableEnded = false;
  writableFinished = false;
  destroyed = false;
  _writableState = { ended: false, finished: false, objectMode: false, corked: 0, length: 0 };
  constructor(streamId, session, isPushStream = false) {
    super();
    this._streamId = streamId;
    this._session = session;
    this._isPushStream = isPushStream;
    if (!isPushStream) {
      queueMicrotask(() => {
        this.emit("ready");
      });
    }
  }
  setEncoding(encoding) {
    this._encoding = encoding;
    this._utf8Remainder = this._encoding === "utf8" || this._encoding === "utf-8" ? Buffer.alloc(0) : void 0;
    return this;
  }
  close() {
    this.end();
    return this;
  }
  destroy(error) {
    if (this.destroyed) {
      return this;
    }
    this.destroyed = true;
    if (error) {
      this.emit("error", error);
    }
    this.end();
    return this;
  }
  _scheduleDrain() {
    if (!this._needsDrain || this._drainScheduled) {
      return;
    }
    this._drainScheduled = true;
    queueMicrotask(() => {
      this._drainScheduled = false;
      if (!this._needsDrain) {
        return;
      }
      this._needsDrain = false;
      this._pendingWritableBytes = 0;
      this.emit("drain");
    });
  }
  write(data, encodingOrCallback, callback) {
    if (typeof _networkHttp2StreamWriteRaw === "undefined") {
      throw new Error("http2 session stream write bridge is not available");
    }
    const buffer = Buffer.isBuffer(data) ? data : typeof data === "string" ? Buffer.from(data, typeof encodingOrCallback === "string" ? encodingOrCallback : "utf8") : Buffer.from(data);
    const wrote = _networkHttp2StreamWriteRaw.applySync(void 0, [this._streamId, buffer.toString("base64")]);
    this._pendingWritableBytes += buffer.byteLength;
    const shouldBackpressure = wrote === false || this._pendingWritableBytes >= this._writableHighWaterMark;
    if (shouldBackpressure) {
      this._needsDrain = true;
    }
    const cb = typeof encodingOrCallback === "function" ? encodingOrCallback : callback;
    cb?.();
    return !shouldBackpressure;
  }
  end(data) {
    if (typeof _networkHttp2StreamEndRaw === "undefined") {
      throw new Error("http2 session stream end bridge is not available");
    }
    let encoded = null;
    if (data !== void 0) {
      const buffer = Buffer.isBuffer(data) ? data : typeof data === "string" ? Buffer.from(data) : Buffer.from(data);
      encoded = buffer.toString("base64");
    }
    _networkHttp2StreamEndRaw.applySync(void 0, [this._streamId, encoded]);
    this.writableEnded = true;
    this._writableState.ended = true;
    queueMicrotask(() => {
      this.writable = false;
      this.writableFinished = true;
      this._writableState.finished = true;
      this.emit("finish");
    });
    return this;
  }
  resume() {
    return this;
  }
  _emitPush(headers, flags) {
    if (process.env.SECURE_EXEC_DEBUG_HTTP2_BRIDGE === "1") {
      console.error("[secure-exec http2 isolate] push", this._streamId);
    }
    this.emit("push", headers, flags ?? 0);
  }
  _hasReceivedResponse() {
    return this._receivedResponse;
  }
  _belongsTo(session) {
    return this._session === session;
  }
  _emitResponseHeaders(headers) {
    this._receivedResponse = true;
    if (process.env.SECURE_EXEC_DEBUG_HTTP2_BRIDGE === "1") {
      console.error("[secure-exec http2 isolate] response headers", this._streamId, this._isPushStream);
    }
    if (!this._isPushStream) {
      this.emit("response", headers);
    }
  }
  _emitDataChunk(dataBase64) {
    if (!dataBase64) {
      return;
    }
    const chunkBuffer = Buffer.from(dataBase64, "base64");
    if (this._utf8Remainder !== void 0) {
      const buffer = this._utf8Remainder.length > 0 ? Buffer.concat([this._utf8Remainder, chunkBuffer]) : chunkBuffer;
      const completeLength = getCompleteUtf8PrefixLength(buffer);
      const chunk = buffer.subarray(0, completeLength).toString("utf8");
      this._utf8Remainder = completeLength < buffer.length ? buffer.subarray(completeLength) : Buffer.alloc(0);
      if (chunk.length > 0) {
        this.emit("data", chunk);
      }
    } else if (this._encoding) {
      this.emit("data", chunkBuffer.toString(this._encoding));
    } else {
      this.emit("data", chunkBuffer);
    }
    this._scheduleDrain();
  }
  _emitEnd() {
    if (this._utf8Remainder && this._utf8Remainder.length > 0) {
      const trailing = this._utf8Remainder.toString("utf8");
      this._utf8Remainder = Buffer.alloc(0);
      if (trailing.length > 0) {
        this.emit("data", trailing);
      }
    }
    this.readable = false;
    this.emit("end");
    this._scheduleDrain();
  }
  _emitClose(rstCode) {
    if (typeof rstCode === "number") {
      this.rstCode = rstCode;
    }
    this.destroyed = true;
    this.readable = false;
    this.writable = false;
    this._scheduleDrain();
    this.emit("close");
  }
};
function getCompleteUtf8PrefixLength(buffer) {
  if (buffer.length === 0) {
    return 0;
  }
  let continuationCount = 0;
  for (let index = buffer.length - 1; index >= 0 && continuationCount < 3; index -= 1) {
    if ((buffer[index] & 192) !== 128) {
      const trailingBytes = buffer.length - index;
      const lead = buffer[index];
      const expectedBytes = (lead & 128) === 0 ? 1 : (lead & 224) === 192 ? 2 : (lead & 240) === 224 ? 3 : (lead & 248) === 240 ? 4 : 1;
      return trailingBytes < expectedBytes ? index : buffer.length;
    }
    continuationCount += 1;
  }
  return continuationCount > 0 ? buffer.length - continuationCount : buffer.length;
}
var ServerHttp2Stream = class _ServerHttp2Stream extends Http2EventEmitter {
  _streamId;
  _binding;
  _responded = false;
  _endQueued = false;
  _pendingSyntheticErrorSuppressions = 0;
  _requestHeaders;
  _isPushStream;
  session;
  rstCode = 0;
  readable = true;
  writable = true;
  destroyed = false;
  _readableState;
  _writableState;
  constructor(streamId, session, requestHeaders, isPushStream = false) {
    super();
    this._streamId = streamId;
    this._binding = new Http2Stream(streamId);
    this.session = session;
    this._requestHeaders = requestHeaders;
    this._isPushStream = isPushStream;
    this._readableState = {
      flowing: null,
      ended: false,
      highWaterMark: 16 * 1024
    };
    this._writableState = {
      ended: requestHeaders?.[":method"] === "HEAD"
    };
  }
  _closeWithCode(code) {
    this.rstCode = code;
    _networkHttp2StreamCloseRaw?.applySync(void 0, [this._streamId, code]);
  }
  _markSyntheticClose() {
    this.destroyed = true;
    this.readable = false;
    this.writable = false;
  }
  _shouldSuppressHostError() {
    if (this._pendingSyntheticErrorSuppressions <= 0) {
      return false;
    }
    this._pendingSyntheticErrorSuppressions -= 1;
    return true;
  }
  _emitNghttp2Error(errorCode) {
    const error = new NghttpError(nghttp2ErrorString(errorCode));
    this._pendingSyntheticErrorSuppressions += 1;
    this._markSyntheticClose();
    this.emit("error", error);
    this._closeWithCode(HTTP2_INTERNAL_BINDING_CONSTANTS.NGHTTP2_INTERNAL_ERROR);
  }
  _emitInternalStreamError() {
    const error = createHttp2Error(
      "ERR_HTTP2_STREAM_ERROR",
      "Stream closed with error code NGHTTP2_INTERNAL_ERROR"
    );
    this._pendingSyntheticErrorSuppressions += 1;
    this._markSyntheticClose();
    this.emit("error", error);
    this._closeWithCode(HTTP2_INTERNAL_BINDING_CONSTANTS.NGHTTP2_INTERNAL_ERROR);
  }
  _submitResponse(headers) {
    this._responded = true;
    const ngError = this._binding.respond(headers);
    if (typeof ngError === "number" && ngError !== 0) {
      this._emitNghttp2Error(ngError);
      return false;
    }
    return true;
  }
  respond(headers) {
    if (this.destroyed) {
      throw createHttp2Error("ERR_HTTP2_INVALID_STREAM", "The stream has been destroyed");
    }
    if (this._responded) {
      throw createHttp2Error("ERR_HTTP2_HEADERS_SENT", "Response has already been initiated.");
    }
    this._submitResponse(headers);
  }
  pushStream(headers, optionsOrCallback, maybeCallback) {
    if (this._isPushStream) {
      throw createHttp2Error(
        "ERR_HTTP2_NESTED_PUSH",
        "A push stream cannot initiate another push stream."
      );
    }
    const callback = typeof optionsOrCallback === "function" ? optionsOrCallback : maybeCallback;
    if (typeof callback !== "function") {
      throw createHttp2ArgTypeError("callback", "function", callback);
    }
    if (typeof _networkHttp2StreamPushStreamRaw === "undefined") {
      throw new Error("http2 server stream push bridge is not available");
    }
    const options = optionsOrCallback && typeof optionsOrCallback === "object" && !Array.isArray(optionsOrCallback) ? optionsOrCallback : {};
    const resultJson = _networkHttp2StreamPushStreamRaw.applySync(
      void 0,
      [
        this._streamId,
        serializeHttp2Headers(normalizeHttp2Headers(headers)),
        JSON.stringify(options ?? {})
      ]
    );
    const result = JSON.parse(resultJson);
    if (result.error) {
      callback(parseHttp2ErrorPayload(result.error));
      return;
    }
    const pushStream = new _ServerHttp2Stream(
      Number(result.streamId),
      this.session,
      parseHttp2Headers(result.headers),
      true
    );
    http2Streams.set(Number(result.streamId), pushStream);
    callback(null, pushStream, parseHttp2Headers(result.headers));
  }
  write(data) {
    if (this._writableState.ended) {
      queueMicrotask(() => {
        this.emit("error", createHttp2Error("ERR_STREAM_WRITE_AFTER_END", "write after end"));
      });
      return false;
    }
    if (typeof _networkHttp2StreamWriteRaw === "undefined") {
      throw new Error("http2 server stream write bridge is not available");
    }
    const buffer = Buffer.isBuffer(data) ? data : typeof data === "string" ? Buffer.from(data) : Buffer.from(data);
    return _networkHttp2StreamWriteRaw.applySync(void 0, [this._streamId, buffer.toString("base64")]);
  }
  end(data) {
    if (!this._responded) {
      if (!this._submitResponse({ ":status": 200 })) {
        return;
      }
    }
    if (this._endQueued) {
      return;
    }
    if (typeof _networkHttp2StreamEndRaw === "undefined") {
      throw new Error("http2 server stream end bridge is not available");
    }
    this._writableState.ended = true;
    let encoded = null;
    if (data !== void 0) {
      const buffer = Buffer.isBuffer(data) ? data : typeof data === "string" ? Buffer.from(data) : Buffer.from(data);
      encoded = buffer.toString("base64");
    }
    this._endQueued = true;
    queueMicrotask(() => {
      if (!this._endQueued || this.destroyed) {
        return;
      }
      this._endQueued = false;
      _networkHttp2StreamEndRaw.applySync(void 0, [this._streamId, encoded]);
    });
  }
  pause() {
    this._readableState.flowing = false;
    _networkHttp2StreamPauseRaw?.applySync(void 0, [this._streamId]);
    return this;
  }
  resume() {
    this._readableState.flowing = true;
    _networkHttp2StreamResumeRaw?.applySync(void 0, [this._streamId]);
    return this;
  }
  respondWithFile(path, headers, options) {
    if (this.destroyed) {
      throw createHttp2Error("ERR_HTTP2_INVALID_STREAM", "The stream has been destroyed");
    }
    if (this._responded) {
      throw createHttp2Error("ERR_HTTP2_HEADERS_SENT", "Response has already been initiated.");
    }
    const normalizedOptions = normalizeHttp2FileResponseOptions(options);
    const responseHeaders = { ...headers ?? {} };
    const statusCode = responseHeaders[":status"];
    if (statusCode === 204 || statusCode === 205 || statusCode === 304) {
      throw createHttp2PayloadForbiddenError(Number(statusCode));
    }
    try {
      const statJson = _fs.stat.applySyncPromise(void 0, [path]);
      const bodyBase64 = _fs.readFileBinary.applySyncPromise(void 0, [path]);
      const stat = createHttp2BridgeStat(decodeBridgeJson(statJson));
      const callbackOptions = {
        offset: normalizedOptions.offset,
        length: normalizedOptions.length ?? Math.max(0, stat.size - normalizedOptions.offset)
      };
      normalizedOptions.statCheck?.(stat, responseHeaders, callbackOptions);
      const body = Buffer.from(bodyBase64, "base64");
      const slicedBody = sliceHttp2FileBody(
        body,
        normalizedOptions.offset,
        normalizedOptions.length
      );
      if (responseHeaders["content-length"] === void 0) {
        responseHeaders["content-length"] = slicedBody.byteLength;
      }
      if (!this._submitResponse({
        ":status": 200,
        ...responseHeaders
      })) {
        return;
      }
      this.end(slicedBody);
      return;
    } catch {
    }
    if (typeof _networkHttp2StreamRespondWithFileRaw === "undefined") {
      throw new Error("http2 server stream respondWithFile bridge is not available");
    }
    this._responded = true;
    _networkHttp2StreamRespondWithFileRaw.applySync(
      void 0,
      [
        this._streamId,
        path,
        JSON.stringify(headers ?? {}),
        JSON.stringify(options ?? {})
      ]
    );
  }
  respondWithFD(fdOrHandle, headers, options) {
    const fd = typeof fdOrHandle === "number" ? fdOrHandle : typeof fdOrHandle?.fd === "number" ? fdOrHandle.fd : NaN;
    const path = Number.isFinite(fd) ? _fdGetPath.applySync(void 0, [fd]) : null;
    if (!path) {
      this._emitInternalStreamError();
      return;
    }
    this.respondWithFile(path, headers, options);
  }
  destroy(error) {
    if (this.destroyed) {
      return this;
    }
    this.destroyed = true;
    if (error) {
      this.emit("error", error);
    }
    this._closeWithCode(HTTP2_INTERNAL_BINDING_CONSTANTS.NGHTTP2_CANCEL);
    return this;
  }
  _emitData(dataBase64) {
    if (!dataBase64) {
      return;
    }
    this.emit("data", Buffer.from(dataBase64, "base64"));
  }
  _emitEnd() {
    this._readableState.ended = true;
    this.emit("end");
  }
  _emitDrain() {
    this.emit("drain");
  }
  _emitClose(rstCode) {
    if (typeof rstCode === "number") {
      this.rstCode = rstCode;
    }
    this.destroyed = true;
    this.emit("close");
  }
};
var Http2ServerRequest = class extends Http2EventEmitter {
  headers;
  method;
  url;
  connection;
  socket;
  stream;
  destroyed = false;
  readable = true;
  _readableState = { flowing: null, length: 0, ended: false, objectMode: false };
  constructor(headers, socket, stream) {
    super();
    this.headers = headers;
    this.method = typeof headers[":method"] === "string" ? String(headers[":method"]) : "GET";
    this.url = typeof headers[":path"] === "string" ? String(headers[":path"]) : "/";
    this.connection = socket;
    this.socket = socket;
    this.stream = stream;
  }
  on(event, listener) {
    super.on(event, listener);
    if (event === "data" && this._readableState.flowing !== false) {
      this.resume();
    }
    return this;
  }
  once(event, listener) {
    super.once(event, listener);
    if (event === "data" && this._readableState.flowing !== false) {
      this.resume();
    }
    return this;
  }
  resume() {
    this._readableState.flowing = true;
    this.stream.resume();
    return this;
  }
  pause() {
    this._readableState.flowing = false;
    this.stream.pause();
    return this;
  }
  pipe(dest) {
    this.on("data", (chunk) => {
      const wrote = dest.write(chunk);
      if (wrote === false && typeof dest.once === "function") {
        this.pause();
        dest.once("drain", () => this.resume());
      }
    });
    this.on("end", () => dest.end());
    this.resume();
    return dest;
  }
  unpipe() {
    return this;
  }
  read() {
    return null;
  }
  isPaused() {
    return this._readableState.flowing === false;
  }
  setEncoding() {
    return this;
  }
  _emitData(chunk) {
    this._readableState.length += chunk.byteLength;
    this.emit("data", chunk);
  }
  _emitEnd() {
    this._readableState.ended = true;
    this.emit("end");
    this.emit("close");
  }
  _emitError(error) {
    this.emit("error", error);
  }
  destroy(err) {
    this.destroyed = true;
    if (err) {
      this.emit("error", err);
    }
    this.emit("close");
    return this;
  }
};
var Http2ServerResponse = class extends Http2EventEmitter {
  _stream;
  _headers = {};
  _statusCode = 200;
  headersSent = false;
  writable = true;
  writableEnded = false;
  writableFinished = false;
  socket;
  connection;
  stream;
  _writableState = { ended: false, finished: false, objectMode: false, corked: 0, length: 0 };
  constructor(stream) {
    super();
    this._stream = stream;
    this.stream = stream;
    this.socket = stream.session.socket;
    this.connection = this.socket;
  }
  writeHead(statusCode, headers) {
    this._statusCode = statusCode;
    this._headers = {
      ...this._headers,
      ...headers ?? {},
      ":status": statusCode
    };
    this._stream.respond(this._headers);
    this.headersSent = true;
    return this;
  }
  setHeader(name, value) {
    this._headers[name] = value;
    return this;
  }
  getHeader(name) {
    return this._headers[name];
  }
  hasHeader(name) {
    return Object.prototype.hasOwnProperty.call(this._headers, name);
  }
  removeHeader(name) {
    delete this._headers[name];
  }
  write(data, encodingOrCallback, callback) {
    if (!(":status" in this._headers)) {
      this._headers[":status"] = this._statusCode;
      this._stream.respond(this._headers);
      this.headersSent = true;
    }
    const wrote = this._stream.write(
      typeof data === "string" && typeof encodingOrCallback === "string" ? Buffer.from(data, encodingOrCallback) : data
    );
    const cb = typeof encodingOrCallback === "function" ? encodingOrCallback : callback;
    cb?.();
    return wrote;
  }
  end(data) {
    if (!(":status" in this._headers)) {
      this._headers[":status"] = this._statusCode;
      this._stream.respond(this._headers);
      this.headersSent = true;
    }
    this.writableEnded = true;
    this._writableState.ended = true;
    this._stream.end(data);
    queueMicrotask(() => {
      this.writable = false;
      this.writableFinished = true;
      this._writableState.finished = true;
      this.emit("finish");
      this.emit("close");
    });
    return this;
  }
  destroy(err) {
    if (err) {
      this.emit("error", err);
    }
    this.writable = false;
    this.writableEnded = true;
    this.writableFinished = true;
    this.emit("close");
    return this;
  }
};
var Http2Session = class extends Http2EventEmitter {
  encrypted = false;
  alpnProtocol = false;
  originSet;
  localSettings = cloneHttp2Settings(DEFAULT_HTTP2_SETTINGS);
  remoteSettings = cloneHttp2Settings(DEFAULT_HTTP2_SETTINGS);
  pendingSettingsAck = false;
  socket;
  state = cloneHttp2SessionRuntimeState(DEFAULT_HTTP2_SESSION_STATE);
  _sessionId;
  _waitStarted = false;
  _pendingSettingsAckCount = 0;
  _awaitingInitialSettingsAck = false;
  _settingsCallbacks = [];
  constructor(sessionId, socketState) {
    super();
    this._sessionId = sessionId;
    this.socket = new Http2SocketProxy(socketState, () => {
      setTimeout(() => {
        this.destroy();
      }, 0);
    });
    this[HTTP2_K_SOCKET] = this.socket;
  }
  _retain() {
    if (this._waitStarted || typeof _networkHttp2SessionWaitRaw === "undefined") {
      return;
    }
    this._waitStarted = true;
    void _networkHttp2SessionWaitRaw.apply(void 0, [this._sessionId], {
      result: { promise: true }
    }).catch((error) => {
      this.emit("error", error instanceof Error ? error : new Error(String(error)));
    });
  }
  _release() {
    this._waitStarted = false;
  }
  _beginInitialSettingsAck() {
    this._awaitingInitialSettingsAck = true;
    this._pendingSettingsAckCount += 1;
    this.pendingSettingsAck = true;
  }
  _applyLocalSettings(settings) {
    this.localSettings = cloneHttp2Settings(settings);
    if (this._awaitingInitialSettingsAck) {
      this._awaitingInitialSettingsAck = false;
      this._pendingSettingsAckCount = Math.max(0, this._pendingSettingsAckCount - 1);
      this.pendingSettingsAck = this._pendingSettingsAckCount > 0;
    }
    this.emit("localSettings", this.localSettings);
  }
  _applyRemoteSettings(settings) {
    this.remoteSettings = cloneHttp2Settings(settings);
    this.emit("remoteSettings", this.remoteSettings);
  }
  _applyRuntimeState(state) {
    this.state = cloneHttp2SessionRuntimeState(state);
  }
  _ackSettings() {
    this._pendingSettingsAckCount = Math.max(0, this._pendingSettingsAckCount - 1);
    this.pendingSettingsAck = this._pendingSettingsAckCount > 0;
    const callback = this._settingsCallbacks.shift();
    callback?.();
  }
  request(headers, options) {
    if (typeof _networkHttp2SessionRequestRaw === "undefined") {
      throw new Error("http2 session request bridge is not available");
    }
    validateHttp2RequestOptions(options);
    const streamId = _networkHttp2SessionRequestRaw.applySync(
      void 0,
      [
        this._sessionId,
        serializeHttp2Headers(normalizeHttp2Headers(headers)),
        JSON.stringify(options ?? {})
      ]
    );
    const stream = new ClientHttp2Stream(streamId, this);
    http2Streams.set(streamId, stream);
    return stream;
  }
  settings(settings, callback) {
    if (callback !== void 0 && typeof callback !== "function") {
      throw createHttp2ArgTypeError("callback", "function", callback);
    }
    if (typeof _networkHttp2SessionSettingsRaw === "undefined") {
      throw new Error("http2 session settings bridge is not available");
    }
    const normalized = validateHttp2Settings(settings);
    _networkHttp2SessionSettingsRaw.applySync(
      void 0,
      [this._sessionId, JSON.stringify(normalized)]
    );
    this._pendingSettingsAckCount += 1;
    this.pendingSettingsAck = true;
    if (callback) {
      this._settingsCallbacks.push(callback);
    }
  }
  setLocalWindowSize(windowSize) {
    if (typeof windowSize !== "number" || Number.isNaN(windowSize)) {
      throw createHttp2ArgTypeError("windowSize", "number", windowSize);
    }
    if (!Number.isInteger(windowSize) || windowSize < 0 || windowSize > 2147483647) {
      const error = new RangeError(
        `The value of "windowSize" is out of range. It must be >= 0 && <= 2147483647. Received ${windowSize}`
      );
      error.code = "ERR_OUT_OF_RANGE";
      throw error;
    }
    if (typeof _networkHttp2SessionSetLocalWindowSizeRaw === "undefined") {
      throw new Error("http2 session setLocalWindowSize bridge is not available");
    }
    const result = _networkHttp2SessionSetLocalWindowSizeRaw.applySync(
      void 0,
      [this._sessionId, windowSize]
    );
    this._applyRuntimeState(parseHttp2SessionState(result)?.state);
  }
  goaway(code = 0, lastStreamID = 0, opaqueData) {
    const payload = opaqueData === void 0 ? null : Buffer.isBuffer(opaqueData) ? opaqueData.toString("base64") : typeof opaqueData === "string" ? Buffer.from(opaqueData).toString("base64") : Buffer.from(opaqueData).toString("base64");
    _networkHttp2SessionGoawayRaw?.applySync(void 0, [this._sessionId, code, lastStreamID, payload]);
  }
  close() {
    const pendingStreams = Array.from(http2Streams.entries()).filter(
      ([, stream]) => typeof stream._belongsTo === "function" && stream._belongsTo(this) && !stream._hasReceivedResponse()
    );
    if (pendingStreams.length > 0) {
      const error = createHttp2Error(
        "ERR_HTTP2_GOAWAY_SESSION",
        "The HTTP/2 session is closing before the stream could be established."
      );
      queueMicrotask(() => {
        for (const [streamId, stream] of pendingStreams) {
          if (http2Streams.get(streamId) !== stream) {
            continue;
          }
          stream.emit("error", error);
          stream.emit("close");
          http2Streams.delete(streamId);
        }
      });
      if (typeof _networkHttp2SessionDestroyRaw !== "undefined") {
        _networkHttp2SessionDestroyRaw.applySync(void 0, [this._sessionId]);
        return;
      }
    }
    _networkHttp2SessionCloseRaw?.applySync(void 0, [this._sessionId]);
    setTimeout(() => {
      if (!http2Sessions.has(this._sessionId)) {
        return;
      }
      this._release();
      this.emit("close");
      http2Sessions.delete(this._sessionId);
      _unregisterHandle?.(`http2:session:${this._sessionId}`);
    }, 50);
  }
  destroy() {
    if (typeof _networkHttp2SessionDestroyRaw !== "undefined") {
      _networkHttp2SessionDestroyRaw.applySync(void 0, [this._sessionId]);
      return;
    }
    this.close();
  }
};
var Http2Server = class extends Http2EventEmitter {
  allowHalfOpen;
  allowHTTP1;
  encrypted;
  _serverId;
  listening = false;
  _address = null;
  _options;
  _timeoutMs = 0;
  _waitStarted = false;
  constructor(options, listener, encrypted) {
    super();
    this.allowHalfOpen = options?.allowHalfOpen === true;
    this.allowHTTP1 = options?.allowHTTP1 === true;
    this.encrypted = encrypted;
    const initialSettings = options?.settings && typeof options.settings === "object" && !Array.isArray(options.settings) ? cloneHttp2Settings(options.settings) : {};
    this._options = {
      ...options ?? {},
      settings: initialSettings
    };
    this._serverId = nextHttp2ServerId++;
    this[HTTP2_OPTIONS] = {
      settings: cloneHttp2Settings(initialSettings),
      unknownProtocolTimeout: 1e4,
      ...encrypted ? { ALPNProtocols: ["h2"] } : {}
    };
    if (listener) {
      this.on("request", listener);
    }
    http2Servers.set(this._serverId, this);
  }
  address() {
    return this._address;
  }
  _retain() {
    if (this._waitStarted || typeof _networkHttp2ServerWaitRaw === "undefined") {
      return;
    }
    this._waitStarted = true;
    void _networkHttp2ServerWaitRaw.apply(void 0, [this._serverId], {
      result: { promise: true }
    }).catch((error) => {
      this.emit("error", error instanceof Error ? error : new Error(String(error)));
    });
  }
  _release() {
    this._waitStarted = false;
  }
  setTimeout(timeout, callback) {
    this._timeoutMs = normalizeSocketTimeout(timeout);
    if (callback) {
      this.on("timeout", callback);
    }
    return this;
  }
  updateSettings(settings) {
    const normalized = validateHttp2Settings(settings);
    const mergedSettings = {
      ...cloneHttp2Settings(this._options.settings),
      ...cloneHttp2Settings(normalized)
    };
    this._options = {
      ...this._options,
      settings: mergedSettings
    };
    const optionsState = this[HTTP2_OPTIONS];
    optionsState.settings = cloneHttp2Settings(mergedSettings);
    return this;
  }
  listen(portOrOptions, hostOrCallback, backlogOrCallback, callback) {
    if (typeof _networkHttp2ServerListenRaw === "undefined") {
      throw new Error(`http2.${this.encrypted ? "createSecureServer" : "createServer"} is not supported in sandbox`);
    }
    const options = normalizeListenArgs(portOrOptions, hostOrCallback, backlogOrCallback, callback);
    if (options.callback) {
      this.once("listening", options.callback);
    }
    const payload = {
      serverId: this._serverId,
      secure: this.encrypted,
      port: options.port,
      host: options.host,
      backlog: options.backlog,
      allowHalfOpen: this.allowHalfOpen,
      allowHTTP1: this._options.allowHTTP1 === true,
      timeout: this._timeoutMs,
      settings: this._options.settings,
      remoteCustomSettings: this._options.remoteCustomSettings,
      tls: this.encrypted ? buildSerializedTlsOptions(
        {
          ...this._options,
          ...portOrOptions && typeof portOrOptions === "object" ? portOrOptions : {}
        },
        { isServer: true }
      ) : void 0
    };
    const result = JSON.parse(
      _networkHttp2ServerListenRaw.applySyncPromise(void 0, [JSON.stringify(payload)])
    );
    this._address = result.address ?? null;
    this.listening = true;
    this._retain();
    _registerHandle?.(`http2:server:${this._serverId}`, "http2 server");
    this.emit("listening");
    return this;
  }
  close(callback) {
    if (callback) {
      this.once("close", callback);
    }
    if (!this.listening) {
      this._release();
      queueMicrotask(() => this.emit("close"));
      return this;
    }
    void _networkHttp2ServerCloseRaw?.apply(void 0, [this._serverId], {
      result: { promise: true }
    });
    setTimeout(() => {
      if (!this.listening) {
        return;
      }
      this.listening = false;
      this._release();
      this.emit("close");
      http2Servers.delete(this._serverId);
      _unregisterHandle?.(`http2:server:${this._serverId}`);
    }, 50);
    return this;
  }
};
function createHttp2Server(secure, optionsOrListener, maybeListener) {
  const listener = typeof optionsOrListener === "function" ? optionsOrListener : maybeListener;
  const options = optionsOrListener && typeof optionsOrListener === "object" && !Array.isArray(optionsOrListener) ? optionsOrListener : void 0;
  return new Http2Server(options, listener, secure);
}
function connectHttp2(authorityOrOptions, optionsOrListener, maybeListener) {
  if (typeof _networkHttp2SessionConnectRaw === "undefined") {
    throw new Error("http2.connect is not supported in sandbox");
  }
  const { authority, options, listener } = normalizeHttp2ConnectArgs(
    authorityOrOptions,
    optionsOrListener,
    maybeListener
  );
  if (authority.protocol !== "http:" && authority.protocol !== "https:") {
    throw createHttp2Error(
      "ERR_HTTP2_UNSUPPORTED_PROTOCOL",
      `protocol "${authority.protocol}" is unsupported.`
    );
  }
  validateHttp2ConnectOptions(options);
  const socketId = options.createConnection ? resolveHttp2SocketId(options.createConnection()) : void 0;
  const rawPort = options.port ?? authority.port;
  const port = rawPort === "" || rawPort === void 0 || rawPort === null ? void 0 : Number(rawPort);
  const response = JSON.parse(
    _networkHttp2SessionConnectRaw.applySyncPromise(
      void 0,
      [
        JSON.stringify({
          authority: authority.toString(),
          protocol: authority.protocol,
          host: options.host ?? options.hostname ?? authority.hostname,
          port,
          localAddress: options.localAddress,
          family: options.family,
          socketId,
          settings: options.settings,
          remoteCustomSettings: options.remoteCustomSettings,
          tls: authority.protocol === "https:" ? buildSerializedTlsOptions(options, { servername: typeof options.servername === "string" ? options.servername : authority.hostname }) : void 0
        })
      ]
    )
  );
  const initialState = parseHttp2SessionState(response.state);
  const session = new Http2Session(
    response.sessionId,
    initialState?.socket ?? void 0
  );
  applyHttp2SessionState(session, initialState);
  session._beginInitialSettingsAck();
  session._retain();
  if (listener) {
    session.once("connect", () => listener(session));
  }
  http2Sessions.set(response.sessionId, session);
  _registerHandle?.(`http2:session:${response.sessionId}`, "http2 session");
  if (authority.protocol === "https:") {
    session.socket.once("secureConnect", () => {
    });
  }
  return session;
}
function getOrCreateHttp2Session(sessionId, state) {
  let session = http2Sessions.get(sessionId);
  if (!session) {
    session = new Http2Session(sessionId, state?.socket ?? void 0);
    http2Sessions.set(sessionId, session);
  }
  applyHttp2SessionState(session, state);
  return session;
}
function queuePendingHttp2ClientStreamEvent(streamId, event) {
  const pending = pendingHttp2ClientStreamEvents.get(streamId) ?? [];
  pending.push(event);
  pendingHttp2ClientStreamEvents.set(streamId, pending);
}
function schedulePendingHttp2ClientStreamEventsFlush(streamId) {
  if (scheduledHttp2ClientStreamFlushes.has(streamId)) {
    return;
  }
  scheduledHttp2ClientStreamFlushes.add(streamId);
  const flush = () => {
    scheduledHttp2ClientStreamFlushes.delete(streamId);
    flushPendingHttp2ClientStreamEvents(streamId);
  };
  const scheduleImmediate = globalThis.setImmediate;
  if (typeof scheduleImmediate === "function") {
    scheduleImmediate(flush);
    return;
  }
  setTimeout(flush, 0);
}
function flushPendingHttp2ClientStreamEvents(streamId) {
  const stream = http2Streams.get(streamId);
  if (!stream || typeof stream._emitResponseHeaders !== "function") {
    return;
  }
  const pending = pendingHttp2ClientStreamEvents.get(streamId);
  if (!pending || pending.length === 0) {
    return;
  }
  pendingHttp2ClientStreamEvents.delete(streamId);
  for (const event of pending) {
    if (event.kind === "push") {
      stream._emitPush(parseHttp2Headers(event.data), event.extraNumber);
      continue;
    }
    if (event.kind === "responseHeaders") {
      stream._emitResponseHeaders(parseHttp2Headers(event.data));
      continue;
    }
    if (event.kind === "data") {
      stream._emitDataChunk(event.data);
      continue;
    }
    if (event.kind === "end") {
      stream._emitEnd();
      continue;
    }
    if (event.kind === "error") {
      stream.emit("error", parseHttp2ErrorPayload(event.data));
      continue;
    }
    if (typeof stream._emitClose === "function") {
      stream._emitClose(event.extraNumber);
    } else {
      stream.emit("close");
    }
    http2Streams.delete(streamId);
  }
}
function http2Dispatch(kind, id, data, extra, extraNumber, extraHeaders, flags) {
  if (kind === "sessionConnect") {
    const session = http2Sessions.get(id);
    if (!session) return;
    const state = parseHttp2SessionState(data);
    applyHttp2SessionState(session, state);
    if (session.encrypted) {
      session.socket.emit("secureConnect");
    }
    session.emit("connect");
    return;
  }
  if (kind === "sessionClose") {
    const session = http2Sessions.get(id);
    if (!session) return;
    session._release();
    session.emit("close");
    http2Sessions.delete(id);
    _unregisterHandle?.(`http2:session:${id}`);
    return;
  }
  if (kind === "sessionError") {
    const session = http2Sessions.get(id);
    if (!session) return;
    session.emit("error", parseHttp2ErrorPayload(data));
    return;
  }
  if (kind === "sessionLocalSettings") {
    const session = http2Sessions.get(id);
    if (!session) return;
    session._applyLocalSettings(parseHttp2Headers(data));
    return;
  }
  if (kind === "sessionRemoteSettings") {
    const session = http2Sessions.get(id);
    if (!session) return;
    session._applyRemoteSettings(parseHttp2Headers(data));
    return;
  }
  if (kind === "sessionSettingsAck") {
    const session = http2Sessions.get(id);
    if (!session) return;
    session._ackSettings();
    return;
  }
  if (kind === "sessionGoaway") {
    const session = http2Sessions.get(id);
    if (!session) return;
    session.emit(
      "goaway",
      Number(extraNumber ?? 0),
      Number(flags ?? 0),
      data ? Buffer.from(data, "base64") : Buffer.alloc(0)
    );
    return;
  }
  if (kind === "clientPushStream") {
    const session = http2Sessions.get(id);
    if (!session) return;
    const streamId = Number(data);
    const stream = new ClientHttp2Stream(streamId, session, true);
    http2Streams.set(streamId, stream);
    session.emit("stream", stream, parseHttp2Headers(extraHeaders), Number(flags ?? 0));
    schedulePendingHttp2ClientStreamEventsFlush(streamId);
    return;
  }
  if (kind === "clientPushHeaders") {
    queuePendingHttp2ClientStreamEvent(id, {
      kind: "push",
      data,
      extraNumber: Number(extraNumber ?? 0)
    });
    schedulePendingHttp2ClientStreamEventsFlush(id);
    return;
  }
  if (kind === "clientResponseHeaders") {
    queuePendingHttp2ClientStreamEvent(id, {
      kind: "responseHeaders",
      data
    });
    schedulePendingHttp2ClientStreamEventsFlush(id);
    return;
  }
  if (kind === "clientData") {
    queuePendingHttp2ClientStreamEvent(id, {
      kind: "data",
      data
    });
    schedulePendingHttp2ClientStreamEventsFlush(id);
    return;
  }
  if (kind === "clientEnd") {
    queuePendingHttp2ClientStreamEvent(id, {
      kind: "end"
    });
    schedulePendingHttp2ClientStreamEventsFlush(id);
    return;
  }
  if (kind === "clientClose") {
    queuePendingHttp2ClientStreamEvent(id, {
      kind: "close",
      extraNumber: Number(extraNumber ?? 0)
    });
    schedulePendingHttp2ClientStreamEventsFlush(id);
    return;
  }
  if (kind === "clientError") {
    queuePendingHttp2ClientStreamEvent(id, {
      kind: "error",
      data
    });
    schedulePendingHttp2ClientStreamEventsFlush(id);
    return;
  }
  if (kind === "serverStream") {
    const server = http2Servers.get(id);
    if (!server) return;
    const sessionState = parseHttp2SessionState(extra);
    const sessionId = Number(extraNumber);
    const session = getOrCreateHttp2Session(sessionId, sessionState);
    const streamId = Number(data);
    const headers = parseHttp2Headers(extraHeaders);
    const numericFlags = Number(flags ?? 0);
    const stream = new ServerHttp2Stream(streamId, session, headers);
    http2Streams.set(streamId, stream);
    server.emit("stream", stream, headers, numericFlags);
    if (server.listenerCount("request") > 0) {
      const request = new Http2ServerRequest(headers, session.socket, stream);
      const response = new Http2ServerResponse(stream);
      stream.on("data", (chunk) => {
        request._emitData(chunk);
      });
      stream.on("end", () => {
        request._emitEnd();
      });
      stream.on("error", (error) => {
        request._emitError(error);
      });
      stream.on("drain", () => {
        response.emit("drain");
      });
      server.emit("request", request, response);
    }
    return;
  }
  if (kind === "serverStreamData") {
    const stream = http2Streams.get(id);
    if (!stream || typeof stream._emitData !== "function") return;
    stream._emitData(data);
    return;
  }
  if (kind === "serverStreamEnd") {
    const stream = http2Streams.get(id);
    if (!stream || typeof stream._emitEnd !== "function") return;
    stream._emitEnd();
    return;
  }
  if (kind === "serverStreamDrain") {
    const stream = http2Streams.get(id);
    if (!stream || typeof stream._emitDrain !== "function") return;
    stream._emitDrain();
    return;
  }
  if (kind === "serverStreamError") {
    const stream = http2Streams.get(id);
    if (!stream) return;
    if (typeof stream._shouldSuppressHostError === "function" && stream._shouldSuppressHostError()) {
      return;
    }
    stream.emit("error", parseHttp2ErrorPayload(data));
    return;
  }
  if (kind === "serverStreamClose") {
    const stream = http2Streams.get(id);
    if (!stream || typeof stream._emitClose !== "function") return;
    stream._emitClose(Number(extraNumber ?? 0));
    http2Streams.delete(id);
    return;
  }
  if (kind === "serverSession") {
    const server = http2Servers.get(id);
    if (!server) return;
    const sessionId = Number(extraNumber);
    const session = getOrCreateHttp2Session(sessionId, parseHttp2SessionState(data));
    server.emit("session", session);
    return;
  }
  if (kind === "serverTimeout") {
    http2Servers.get(id)?.emit("timeout");
    return;
  }
  if (kind === "serverConnection") {
    http2Servers.get(id)?.emit("connection", new Http2SocketProxy(parseHttp2SocketState(data) ?? void 0));
    return;
  }
  if (kind === "serverSecureConnection") {
    http2Servers.get(id)?.emit("secureConnection", new Http2SocketProxy(parseHttp2SocketState(data) ?? void 0));
    return;
  }
  if (kind === "serverClose") {
    const server = http2Servers.get(id);
    if (!server) return;
    server.listening = false;
    server._release();
    server.emit("close");
    http2Servers.delete(id);
    _unregisterHandle?.(`http2:server:${id}`);
    return;
  }
  if (kind === "serverCompatRequest") {
    pendingHttp2CompatRequests.set(Number(extraNumber), {
      serverId: id,
      requestJson: data ?? "{}"
    });
    void dispatchHttp2CompatibilityRequest(id, Number(extraNumber));
  }
}
function scheduleQueuedHttp2DispatchDrain() {
  if (scheduledHttp2DispatchDrain) {
    return;
  }
  scheduledHttp2DispatchDrain = true;
  const drain = () => {
    scheduledHttp2DispatchDrain = false;
    while (queuedHttp2DispatchEvents.length > 0) {
      const event = queuedHttp2DispatchEvents.shift();
      if (!event) {
        continue;
      }
      http2Dispatch(
        event.kind,
        event.id,
        event.data,
        event.extra,
        event.extraNumber,
        event.extraHeaders,
        event.flags
      );
    }
  };
  queueMicrotask(drain);
}
function onHttp2Dispatch(_eventType, payload) {
  if (!payload || typeof payload !== "object") {
    return;
  }
  const event = payload;
  if (typeof event.kind !== "string" || typeof event.id !== "number") {
    return;
  }
  if (process.env.SECURE_EXEC_DEBUG_HTTP2_BRIDGE === "1") {
    console.error("[secure-exec http2 isolate dispatch]", event.kind, event.id);
  }
  const kind = event.kind;
  const id = event.id;
  const data = typeof event.data === "string" ? event.data : void 0;
  const extra = typeof event.extra === "string" ? event.extra : void 0;
  const normalizedExtraNumber = typeof event.extraNumber === "string" || typeof event.extraNumber === "number" ? event.extraNumber : void 0;
  const extraHeaders = typeof event.extraHeaders === "string" ? event.extraHeaders : void 0;
  const flags = typeof event.flags === "string" || typeof event.flags === "number" ? event.flags : void 0;
  queuedHttp2DispatchEvents.push({
    kind,
    id,
    data,
    extra,
    extraNumber: normalizedExtraNumber,
    extraHeaders,
    flags
  });
  scheduleQueuedHttp2DispatchDrain();
}
var http2 = {
  Http2ServerRequest,
  Http2ServerResponse,
  Http2Stream,
  NghttpError,
  nghttp2ErrorString,
  constants: {
    HTTP2_HEADER_METHOD: ":method",
    HTTP2_HEADER_PATH: ":path",
    HTTP2_HEADER_SCHEME: ":scheme",
    HTTP2_HEADER_AUTHORITY: ":authority",
    HTTP2_HEADER_STATUS: ":status",
    HTTP2_HEADER_CONTENT_TYPE: "content-type",
    HTTP2_HEADER_CONTENT_LENGTH: "content-length",
    HTTP2_HEADER_LAST_MODIFIED: "last-modified",
    HTTP2_HEADER_ACCEPT: "accept",
    HTTP2_HEADER_ACCEPT_ENCODING: "accept-encoding",
    HTTP2_METHOD_GET: "GET",
    HTTP2_METHOD_POST: "POST",
    HTTP2_METHOD_PUT: "PUT",
    HTTP2_METHOD_DELETE: "DELETE",
    ...HTTP2_INTERNAL_BINDING_CONSTANTS,
    DEFAULT_SETTINGS_MAX_HEADER_LIST_SIZE: 65535
  },
  getDefaultSettings() {
    return cloneHttp2Settings(DEFAULT_HTTP2_SETTINGS);
  },
  connect: connectHttp2,
  createServer: createHttp2Server.bind(void 0, false),
  createSecureServer: createHttp2Server.bind(void 0, true)
};
exposeCustomGlobal("_httpModule", http);
exposeCustomGlobal("_httpsModule", https);
exposeCustomGlobal("_http2Module", http2);
exposeCustomGlobal("_dnsModule", dns);
function onHttpServerRequest(eventType, payload) {
  debugBridgeNetwork("http stream event", eventType, payload);
  if (eventType !== "http_request") {
    return;
  }
  if (!payload || payload.serverId === void 0 || payload.requestId === void 0 || typeof payload.request !== "string") {
    return;
  }
  if (typeof _networkHttpServerRespondRaw === "undefined") {
    debugBridgeNetwork("http stream missing respond bridge");
    return;
  }
  void dispatchServerRequest(payload.serverId, payload.request).then((responseJson) => {
    debugBridgeNetwork("http stream response", payload.serverId, payload.requestId);
    _networkHttpServerRespondRaw.applySync(void 0, [
      payload.serverId,
      payload.requestId,
      responseJson
    ]);
  }).catch((err) => {
    const message = err instanceof Error ? err.message : String(err);
    debugBridgeNetwork("http stream error", payload.serverId, payload.requestId, message);
    _networkHttpServerRespondRaw.applySync(void 0, [
      payload.serverId,
      payload.requestId,
      JSON.stringify({
        status: 500,
        headers: [["content-type", "text/plain"]],
        body: `Error: ${message}`,
        bodyEncoding: "utf8"
      })
    ]);
  });
}
exposeCustomGlobal("_httpServerDispatch", onHttpServerRequest);
exposeCustomGlobal("_httpServerUpgradeDispatch", dispatchUpgradeRequest);
exposeCustomGlobal("_httpServerConnectDispatch", dispatchConnectRequest);
exposeCustomGlobal("_http2Dispatch", onHttp2Dispatch);
exposeCustomGlobal("_upgradeSocketData", onUpgradeSocketData);
exposeCustomGlobal("_upgradeSocketEnd", onUpgradeSocketEnd);
exposeCustomGlobal("fetch", fetch);
exposeCustomGlobal("Headers", UndiciHeaders);
exposeCustomGlobal("Request", UndiciRequest);
exposeCustomGlobal("Response", UndiciResponse);
var Blob = globalThis.Blob;
if (typeof Blob === "undefined") {
  Blob = class BlobStub {
  };
  exposeCustomGlobal("Blob", Blob);
}
var File = globalThis.File;
if (typeof File === "undefined") {
  File = class FileStub extends Blob {
    name;
    lastModified;
    webkitRelativePath;
    constructor(parts = [], name = "", options = {}) {
      super(parts, options);
      this.name = String(name);
      this.lastModified = typeof options.lastModified === "number" ? options.lastModified : Date.now();
      this.webkitRelativePath = "";
    }
  };
  exposeCustomGlobal("File", File);
}
if (typeof globalThis.FormData === "undefined") {
  class FormDataStub {
    _entries = [];
    append(name, value) {
      this._entries.push([name, value]);
    }
    get(name) {
      const entry = this._entries.find(([k]) => k === name);
      return entry ? entry[1] : null;
    }
    getAll(name) {
      return this._entries.filter(([k]) => k === name).map(([, v]) => v);
    }
    has(name) {
      return this._entries.some(([k]) => k === name);
    }
    delete(name) {
      this._entries = this._entries.filter(([k]) => k !== name);
    }
    entries() {
      return this._entries[Symbol.iterator]();
    }
    [Symbol.iterator]() {
      return this.entries();
    }
  }
  exposeCustomGlobal("FormData", FormDataStub);
}
var NET_SOCKET_REGISTRY_PREFIX = "__secureExecNetSocket:";
var NET_SERVER_HANDLE_PREFIX = "net-server:";
var registeredNetSockets = /* @__PURE__ */ new Map();
var registeredNetServersByPort = /* @__PURE__ */ new Map();
function getRegisteredNetSocket(socketId) {
  return globalThis[`${NET_SOCKET_REGISTRY_PREFIX}${socketId}`];
}
function registerNetSocket(socketId, socket) {
  globalThis[`${NET_SOCKET_REGISTRY_PREFIX}${socketId}`] = socket;
  registeredNetSockets.set(socketId, socket);
}
function unregisterNetSocket(socketId) {
  delete globalThis[`${NET_SOCKET_REGISTRY_PREFIX}${socketId}`];
  registeredNetSockets.delete(socketId);
}
function registerNetServer(server) {
  const port = server?._address?.port;
  if (typeof port === "number") {
    registeredNetServersByPort.set(port, server);
  }
}
function unregisterNetServer(server) {
  const port = server?._address?.port;
  if (typeof port === "number" && registeredNetServersByPort.get(port) === server) {
    registeredNetServersByPort.delete(port);
  }
}
function wakeSocketBridgeReads(socket) {
  countNetBridgeMetric("readWakeAttempts");
  if (!socket || socket.destroyed || socket._socketId === 0 || socket._loopbackServer || socket._loopbackHttpTarget) {
    countNetBridgeMetric("readWakeInvalidTargets");
    return;
  }
  if (socket._bridgeReadLoopRunning) {
    countNetBridgeMetric("readWakeAlreadyRunning");
  }
  if (!socket._bridgeReadPollTimer) {
    countNetBridgeMetric("readWakeNoTimer");
    countNetBridgeMetric(socket._bridgeReadPumpStarted ? "readWakeNoTimerAfterFirstPump" : "readWakeNoTimerBeforeFirstPump");
    if (socket._connected) {
      countNetBridgeMetric("readWakeNoTimerConnected");
    }
    if (socket.connecting) {
      countNetBridgeMetric("readWakeNoTimerConnecting");
    }
    countNetBridgeMetric(socket._refed ? "readWakeNoTimerRefed" : "readWakeNoTimerUnrefed");
    const hasDataListener = (socket._listeners?.data?.length ?? 0) > 0;
    const hasReadableListener = (socket._listeners?.readable?.length ?? 0) > 0;
    if (hasDataListener) {
      countNetBridgeMetric("readWakeNoTimerHasDataListener");
    }
    if (hasReadableListener) {
      countNetBridgeMetric("readWakeNoTimerHasReadableListener");
    }
    if (socket._bridgeWriteFlushScheduled) {
      countNetBridgeMetric("readWakeNoTimerPendingWriteFlush");
      countNetBridgeMetric("readWakeNoTimerPendingWriteBytes", socket._pendingBridgeWriteBytes);
    }
    if (!socket._bridgeReadPumpStarted && socket._firstReadNoTimerWakeAtUs === 0 && isNetBridgeMetricsEnabled()) {
      socket._firstReadNoTimerWakeAtUs = netBridgeNowUs();
    }
    if (
      isNetBridgeMetricsEnabled() &&
      !socket._bridgeReadPumpStarted &&
      socket._connected &&
      socket._refed &&
      (hasDataListener || hasReadableListener)
    ) {
      countNetBridgeMetric("readFirstPumpScheduleCandidates");
      if (socket._bridgeReadFirstPumpBenchmarkScheduled) {
        countNetBridgeMetric("readFirstPumpScheduleAlreadyScheduled");
      } else {
        countNetBridgeMetric("readFirstPumpScheduleQueued");
        socket._bridgeReadFirstPumpBenchmarkScheduled = true;
        const queuedAtUs = netBridgeNowUs();
        queueMicrotask(() => {
          countNetBridgeMetric("readFirstPumpScheduleRuns");
          const runAtUs = netBridgeNowUs();
          const queuedToRunUs = Math.max(0, runAtUs - queuedAtUs);
          countNetBridgeMetric("readFirstPumpScheduleQueuedToRunUs", queuedToRunUs);
          maxNetBridgeMetric("readFirstPumpScheduleQueuedToRunMaxUs", queuedToRunUs);
          socket._bridgeReadFirstPumpBenchmarkScheduled = false;
          if (socket.destroyed) {
            countNetBridgeMetric("readFirstPumpScheduleSkipDestroyed");
            return;
          }
          if (socket._tlsUpgrading) {
            countNetBridgeMetric("readFirstPumpScheduleSkipTlsUpgrading");
            return;
          }
          if (socket._bridgeReadPumpStarted) {
            countNetBridgeMetric("readFirstPumpScheduleSkipPumpStarted");
            return;
          }
          if (socket._bridgeReadLoopRunning) {
            countNetBridgeMetric("readFirstPumpScheduleSkipLoopRunning");
            return;
          }
          if (socket._socketId === 0) {
            countNetBridgeMetric("readFirstPumpScheduleSkipSocketClosed");
            return;
          }
          countNetBridgeMetric("readFirstPumpSchedulePumpCalls");
          socket._nextReadPumpOrigin = "eventWake";
          socket._readFirstPumpScheduleActive = true;
          socket._readFirstPumpScheduleQueuedAtUs = queuedAtUs;
          void socket._pumpBridgeReads();
        });
      }
    }
    return;
  }
	    clearTimeout(socket._bridgeReadPollTimer);
	    socket._bridgeReadPollTimer = null;
	    countNetBridgeMetric("readEventWakeups");
	    if (isNetBridgeMetricsEnabled()) {
	      socket._readWakeQueuedAtUs = netBridgeNowUs();
	    }
	    queueMicrotask(() => {
	      if (!socket.destroyed) {
	        socket._nextReadPumpOrigin = "eventWake";
	        void socket._pumpBridgeReads();
    }
  });
}
function wakePeerBridgeReads(socket) {
  countNetBridgeMetric("peerWakeScans");
  if (!socket || socket._socketId === 0 || socket.remotePort === void 0 || socket.localPort === void 0) {
    countNetBridgeMetric("peerWakeInvalidTargets");
    return;
  }
  for (const peer of registeredNetSockets.values()) {
    if (peer === socket || peer.destroyed) {
      continue;
    }
    if (peer.localPort === socket.remotePort && peer.remotePort === socket.localPort) {
      countNetBridgeMetric("peerWakeFound");
      wakeSocketBridgeReads(peer);
      return;
    }
  }
  countNetBridgeMetric("peerWakeMiss");
}
function wakeNetServerAccept(server) {
  countNetBridgeMetric("acceptWakeAttempts");
  if (!server || !server.listening || server._serverId === 0 || !server._acceptPollTimer) {
    if (!server || !server.listening || server._serverId === 0) {
      countNetBridgeMetric("acceptWakeInvalidTargets");
    } else {
      countNetBridgeMetric("acceptWakeNoTimer");
      countNetBridgeMetric(server._acceptPumpStarted ? "acceptWakeNoTimerAfterFirstPump" : "acceptWakeNoTimerBeforeFirstPump");
      if (server._acceptLoopRunning) {
        countNetBridgeMetric("acceptWakeNoTimerLoopRunning");
      }
      if (server._acceptLoopActive) {
        countNetBridgeMetric("acceptWakeNoTimerLoopActive");
      }
      countNetBridgeMetric(server._refed ? "acceptWakeNoTimerRefed" : "acceptWakeNoTimerUnrefed");
      const connectionCount = server._connections?.size ?? 0;
      countNetBridgeMetric("acceptWakeNoTimerConnections", connectionCount);
      maxNetBridgeMetric("acceptWakeNoTimerConnectionsMax", connectionCount);
      if (!server._acceptPumpStarted && server._firstAcceptNoTimerWakeAtUs === 0 && isNetBridgeMetricsEnabled()) {
        server._firstAcceptNoTimerWakeAtUs = netBridgeNowUs();
      }
    }
    return;
  }
  if (server._acceptLoopRunning) {
    countNetBridgeMetric("acceptWakeAlreadyRunning");
  }
	    clearTimeout(server._acceptPollTimer);
	    server._acceptPollTimer = null;
	    countNetBridgeMetric("acceptEventWakeups");
	    if (isNetBridgeMetricsEnabled()) {
	      server._acceptWakeQueuedAtUs = netBridgeNowUs();
	    }
	    queueMicrotask(() => {
    if (server.listening && server._serverId !== 0) {
      server._nextAcceptPumpOrigin = "eventWake";
      void server._pumpAccepts();
    }
  });
}
function wakeNetServerAcceptForSocket(socket) {
  countNetBridgeMetric("acceptWakeSocketScans");
  const port = socket?.remotePort;
  if (typeof port !== "number") {
    countNetBridgeMetric("acceptWakeSocketInvalidTargets");
    return;
  }
  const server = registeredNetServersByPort.get(port);
  if (server) {
    countNetBridgeMetric("acceptWakeSocketFound");
  } else {
    countNetBridgeMetric("acceptWakeSocketMiss");
  }
  wakeNetServerAccept(server);
}
function isTruthySocketOption(value) {
  return value === void 0 ? true : Boolean(value);
}
function normalizeKeepAliveDelay(initialDelay) {
  if (typeof initialDelay !== "number" || !Number.isFinite(initialDelay)) {
    return 0;
  }
  return Math.max(0, Math.floor(initialDelay / 1e3));
}
function createTimeoutArgTypeError(argumentName, value) {
  return createTypeErrorWithCode(
    `The "${argumentName}" argument must be of type number. Received ${formatReceivedType(value)}`,
    "ERR_INVALID_ARG_TYPE"
  );
}
function createFunctionArgTypeError(argumentName, value) {
  return createTypeErrorWithCode(
    `The "${argumentName}" argument must be of type function. Received ${formatReceivedType(value)}`,
    "ERR_INVALID_ARG_TYPE"
  );
}
function createTimeoutRangeError(value) {
  const error = new RangeError(
    `The value of "timeout" is out of range. It must be a non-negative finite number. Received ${String(value)}`
  );
  error.code = "ERR_OUT_OF_RANGE";
  return error;
}
function createListenArgValueError(message) {
  return createTypeErrorWithCode(message, "ERR_INVALID_ARG_VALUE");
}
function createSocketBadPortError(value) {
  const error = new RangeError(
    `options.port should be >= 0 and < 65536. Received ${formatReceivedType(value)}.`
  );
  error.code = "ERR_SOCKET_BAD_PORT";
  return error;
}
function isValidTcpPort(value) {
  return Number.isInteger(value) && value >= 0 && value < 65536;
}
function isDecimalIntegerString(value) {
  return /^[0-9]+$/.test(value);
}
function normalizeListenPortValue(value) {
  if (value === void 0 || value === null) {
    return 0;
  }
  if (typeof value === "string" && value.length > 0) {
    const parsed = Number(value);
    if (isValidTcpPort(parsed)) {
      return parsed;
    }
    throw createSocketBadPortError(value);
  }
  if (typeof value === "number") {
    if (isValidTcpPort(value)) {
      return value;
    }
    throw createSocketBadPortError(value);
  }
  throw createListenArgValueError(
    `The argument 'options' is invalid. Received ${String(value)}`
  );
}
function normalizeListenArgs(portOrOptions, hostOrCallback, backlogOrCallback, callback) {
  const defaultOptions = {
    port: 0,
    host: "127.0.0.1",
    backlog: 511,
    readableAll: false,
    writableAll: false
  };
  if (typeof portOrOptions === "function") {
    return {
      ...defaultOptions,
      callback: portOrOptions
    };
  }
  if (portOrOptions !== null && typeof portOrOptions === "object") {
    const options = portOrOptions;
    const hasPort = Object.prototype.hasOwnProperty.call(options, "port");
    const hasPath = Object.prototype.hasOwnProperty.call(options, "path");
    if (!hasPort && !hasPath) {
      throw createListenArgValueError(
        `The argument 'options' must have the property "port" or "path". Received ${String(portOrOptions)}`
      );
    }
    if (hasPort && hasPath) {
      throw createListenArgValueError(
        `The argument 'options' is invalid. Received ${String(portOrOptions)}`
      );
    }
    if (hasPort && options.port !== void 0 && options.port !== null && typeof options.port !== "number" && typeof options.port !== "string") {
      throw createListenArgValueError(
        `The argument 'options' is invalid. Received ${String(portOrOptions)}`
      );
    }
    if (hasPath) {
      if (typeof options.path !== "string" || options.path.length === 0) {
        throw createListenArgValueError(
          `The argument 'options' is invalid. Received ${String(portOrOptions)}`
        );
      }
      return {
        path: options.path,
        backlog: typeof options.backlog === "number" && Number.isFinite(options.backlog) ? options.backlog : defaultOptions.backlog,
        readableAll: options.readableAll === true,
        writableAll: options.writableAll === true,
        callback: typeof hostOrCallback === "function" ? hostOrCallback : typeof backlogOrCallback === "function" ? backlogOrCallback : callback
      };
    }
    return {
      port: normalizeListenPortValue(options.port),
      host: typeof options.host === "string" && options.host.length > 0 ? options.host : defaultOptions.host,
      backlog: typeof options.backlog === "number" && Number.isFinite(options.backlog) ? options.backlog : defaultOptions.backlog,
      readableAll: false,
      writableAll: false,
      callback: typeof hostOrCallback === "function" ? hostOrCallback : typeof backlogOrCallback === "function" ? backlogOrCallback : callback
    };
  }
  if (portOrOptions !== void 0 && portOrOptions !== null && typeof portOrOptions !== "number" && typeof portOrOptions !== "string") {
    throw createListenArgValueError(
      `The argument 'options' is invalid. Received ${String(portOrOptions)}`
    );
  }
  if (typeof portOrOptions === "string" && portOrOptions.length > 0 && !isDecimalIntegerString(portOrOptions)) {
    return {
      path: portOrOptions,
      backlog: defaultOptions.backlog,
      readableAll: false,
      writableAll: false,
      callback: typeof hostOrCallback === "function" ? hostOrCallback : typeof backlogOrCallback === "function" ? backlogOrCallback : callback
    };
  }
  return {
    port: normalizeListenPortValue(portOrOptions),
    host: typeof hostOrCallback === "string" ? hostOrCallback : defaultOptions.host,
    backlog: typeof backlogOrCallback === "number" ? backlogOrCallback : defaultOptions.backlog,
    readableAll: false,
    writableAll: false,
    callback: typeof hostOrCallback === "function" ? hostOrCallback : typeof backlogOrCallback === "function" ? backlogOrCallback : callback
  };
}
function normalizeConnectArgs(portOrOptions, hostOrCallback, callback) {
  if (portOrOptions !== null && typeof portOrOptions === "object") {
    const normalizedPort = typeof portOrOptions.port === "string" ? Number(portOrOptions.port) : portOrOptions.port;
    return {
      host: typeof portOrOptions.host === "string" && portOrOptions.host.length > 0 ? portOrOptions.host : void 0,
      port: normalizedPort,
      path: typeof portOrOptions.path === "string" && portOrOptions.path.length > 0 ? portOrOptions.path : void 0,
      keepAlive: portOrOptions.keepAlive,
      keepAliveInitialDelay: portOrOptions.keepAliveInitialDelay,
      callback: typeof hostOrCallback === "function" ? hostOrCallback : callback
    };
  }
  if (typeof portOrOptions === "string" && !isDecimalIntegerString(portOrOptions)) {
    return {
      path: portOrOptions,
      callback: typeof hostOrCallback === "function" ? hostOrCallback : callback
    };
  }
  return {
    port: typeof portOrOptions === "number" ? portOrOptions : Number(portOrOptions),
    host: typeof hostOrCallback === "string" ? hostOrCallback : "127.0.0.1",
    callback: typeof hostOrCallback === "function" ? hostOrCallback : callback
  };
}
function isValidIPv4Segment(segment) {
  if (!/^[0-9]{1,3}$/.test(segment)) {
    return false;
  }
  if (segment.length > 1 && segment.startsWith("0")) {
    return false;
  }
  const value = Number(segment);
  return Number.isInteger(value) && value >= 0 && value <= 255;
}
function isIPv4String(input) {
  const segments = input.split(".");
  return segments.length === 4 && segments.every((segment) => isValidIPv4Segment(segment));
}
function isValidIPv6Zone(zone) {
  return zone.length > 0 && /^[0-9A-Za-z_.-]+$/.test(zone);
}
function countIPv6Parts(part) {
  if (part.length === 0) {
    return 0;
  }
  const segments = part.split(":");
  let count = 0;
  for (const segment of segments) {
    if (segment.length === 0) {
      return null;
    }
    if (segment.includes(".")) {
      if (segment !== segments[segments.length - 1] || !isIPv4String(segment)) {
        return null;
      }
      count += 2;
      continue;
    }
    if (!/^[0-9A-Fa-f]{1,4}$/.test(segment)) {
      return null;
    }
    count += 1;
  }
  return count;
}
function isIPv6String(input) {
  if (input.length === 0) {
    return false;
  }
  let address = input;
  const zoneIndex = address.indexOf("%");
  if (zoneIndex !== -1) {
    if (address.indexOf("%", zoneIndex + 1) !== -1) {
      return false;
    }
    const zone = address.slice(zoneIndex + 1);
    if (!isValidIPv6Zone(zone)) {
      return false;
    }
    address = address.slice(0, zoneIndex);
  }
  const doubleColonIndex = address.indexOf("::");
  if (doubleColonIndex !== -1) {
    if (address.indexOf("::", doubleColonIndex + 2) !== -1) {
      return false;
    }
    const [left, right] = address.split("::");
    if (left.includes(".")) {
      return false;
    }
    const leftCount = countIPv6Parts(left);
    const rightCount = countIPv6Parts(right);
    if (leftCount === null || rightCount === null) {
      return false;
    }
    return leftCount + rightCount < 8;
  }
  const count = countIPv6Parts(address);
  return count === 8;
}
function coerceIpInput(input) {
  if (input === null || input === void 0) {
    return "";
  }
  return String(input);
}
function classifyIpAddress(input) {
  const value = coerceIpInput(input);
  if (isIPv4String(value)) {
    return 4;
  }
  if (isIPv6String(value)) {
    return 6;
  }
  return 0;
}
function normalizeIpFamilyLabel(address, family) {
  if (family === "ipv4" || family === 4) {
    return "ipv4";
  }
  if (family === "ipv6" || family === 6) {
    return "ipv6";
  }
  const detected = classifyIpAddress(address);
  if (detected === 4) {
    return "ipv4";
  }
  if (detected === 6) {
    return "ipv6";
  }
  throw new TypeError(`Invalid IP address: ${address}`);
}
function ipv4ToBigInt(address) {
  return address.split(".").reduce((value, segment) => (value << 8n) + BigInt(Number(segment)), 0n);
}
function expandIpv6Address(address) {
  let normalized = String(address);
  const zoneIndex = normalized.indexOf("%");
  if (zoneIndex !== -1) {
    normalized = normalized.slice(0, zoneIndex);
  }
  if (normalized.includes(".")) {
    const lastColonIndex = normalized.lastIndexOf(":");
    const ipv4Part = normalized.slice(lastColonIndex + 1);
    const ipv4Value = ipv4ToBigInt(ipv4Part);
    const high = Number((ipv4Value >> 16n) & 65535n).toString(16);
    const low = Number(ipv4Value & 65535n).toString(16);
    normalized = `${normalized.slice(0, lastColonIndex)}:${high}:${low}`;
  }
  const hasDoubleColon = normalized.includes("::");
  const [leftRaw, rightRaw] = hasDoubleColon ? normalized.split("::") : [normalized, ""];
  const left = leftRaw.length > 0 ? leftRaw.split(":") : [];
  const right = rightRaw.length > 0 ? rightRaw.split(":") : [];
  const fill = hasDoubleColon ? Math.max(0, 8 - (left.length + right.length)) : 0;
  const parts = [...left, ...new Array(fill).fill("0"), ...right];
  if (parts.length !== 8) {
    throw new TypeError(`Invalid IPv6 address: ${address}`);
  }
  return parts.map((part) => part.length === 0 ? "0" : part);
}
function ipv6ToBigInt(address) {
  return expandIpv6Address(address).reduce((value, part) => (value << 16n) + BigInt(parseInt(part, 16)), 0n);
}
function ipAddressToBigInt(address, family) {
  return family === "ipv4" ? ipv4ToBigInt(address) : ipv6ToBigInt(address);
}
function formatBlockListRule(rule) {
  if (rule.type === "address") {
    return `Address: ${rule.family === "ipv4" ? "IPv4" : "IPv6"} ${rule.address}`;
  }
  if (rule.type === "range") {
    return `Range: ${rule.family === "ipv4" ? "IPv4" : "IPv6"} ${rule.start}-${rule.end}`;
  }
  return `Subnet: ${rule.family === "ipv4" ? "IPv4" : "IPv6"} ${rule.network}/${rule.prefix}`;
}
var BlockList = class {
  _rules = [];
  addAddress(address, family) {
    const normalizedFamily = normalizeIpFamilyLabel(address, family);
    this._rules.push({ type: "address", family: normalizedFamily, address: String(address) });
    return this;
  }
  addRange(start, end, family) {
    const normalizedFamily = normalizeIpFamilyLabel(start, family);
    if (normalizeIpFamilyLabel(end, normalizedFamily) !== normalizedFamily) {
      throw new TypeError("BlockList range family mismatch");
    }
    this._rules.push({
      type: "range",
      family: normalizedFamily,
      start: String(start),
      end: String(end)
    });
    return this;
  }
  addSubnet(network, prefix, family) {
    const normalizedFamily = normalizeIpFamilyLabel(network, family);
    const numericPrefix = Number(prefix);
    const maxPrefix = normalizedFamily === "ipv4" ? 32 : 128;
    if (!Number.isInteger(numericPrefix) || numericPrefix < 0 || numericPrefix > maxPrefix) {
      throw new RangeError(`Invalid subnet prefix: ${prefix}`);
    }
    this._rules.push({
      type: "subnet",
      family: normalizedFamily,
      network: String(network),
      prefix: numericPrefix
    });
    return this;
  }
  check(address, family) {
    const normalizedFamily = normalizeIpFamilyLabel(address, family);
    const value = ipAddressToBigInt(String(address), normalizedFamily);
    for (const rule of this._rules) {
      if (rule.family !== normalizedFamily) {
        continue;
      }
      if (rule.type === "address" && value === ipAddressToBigInt(rule.address, normalizedFamily)) {
        return true;
      }
      if (rule.type === "range") {
        const start = ipAddressToBigInt(rule.start, normalizedFamily);
        const end = ipAddressToBigInt(rule.end, normalizedFamily);
        if (value >= start && value <= end) {
          return true;
        }
      }
      if (rule.type === "subnet") {
        const bits = normalizedFamily === "ipv4" ? 32n : 128n;
        const prefixBits = BigInt(rule.prefix);
        const shift = bits - prefixBits;
        const mask = prefixBits === 0n ? 0n : ((1n << bits) - 1n) ^ ((1n << shift) - 1n);
        const network = ipAddressToBigInt(rule.network, normalizedFamily);
        if ((value & mask) === (network & mask)) {
          return true;
        }
      }
    }
    return false;
  }
  toJSON() {
    return this._rules.map((rule) => ({ ...rule }));
  }
  fromJSON(value) {
    if (!Array.isArray(value)) {
      throw new TypeError("BlockList JSON must be an array");
    }
    this._rules = value.map((rule) => ({ ...rule }));
    return this;
  }
  get rules() {
    return this._rules.map((rule) => formatBlockListRule(rule));
  }
};
var defaultAutoSelectFamily = true;
var defaultAutoSelectFamilyAttemptTimeout = 250;
var SocketAddress = class _SocketAddress {
  constructor(options = {}) {
    const address = String(options.address ?? "");
    const family = normalizeIpFamilyLabel(address, options.family);
    const port = Number(options.port ?? 0);
    const flowlabel = Number(options.flowlabel ?? 0);
    if (!Number.isInteger(port) || port < 0 || port > 65535) {
      throw new RangeError(`Invalid port: ${options.port}`);
    }
    if (!Number.isInteger(flowlabel) || flowlabel < 0) {
      throw new RangeError(`Invalid flowlabel: ${options.flowlabel}`);
    }
    this.address = address;
    this.port = port;
    this.family = family;
    this.flowlabel = flowlabel;
  }
  toJSON() {
    return {
      address: this.address,
      port: this.port,
      family: this.family,
      flowlabel: this.flowlabel
    };
  }
  static isSocketAddress(value) {
    return value instanceof _SocketAddress;
  }
  static parse(value) {
    const input = String(value);
    if (input.startsWith("[")) {
      const closingIndex = input.indexOf("]");
      if (closingIndex === -1) {
        return void 0;
      }
      const address = input.slice(1, closingIndex);
      const port = input[closingIndex + 1] === ":" ? Number(input.slice(closingIndex + 2)) : 0;
      return new _SocketAddress({ address, family: "ipv6", port });
    }
    const lastColonIndex = input.lastIndexOf(":");
    if (lastColonIndex !== -1 && input.indexOf(":") === lastColonIndex) {
      const address = input.slice(0, lastColonIndex);
      const port = Number(input.slice(lastColonIndex + 1));
      if (classifyIpAddress(address) !== 0 && Number.isInteger(port)) {
        return new _SocketAddress({ address, port });
      }
    }
    if (classifyIpAddress(input) !== 0) {
      return new _SocketAddress({ address: input });
    }
    return void 0;
  }
};
function normalizeSocketTimeout(timeout) {
  if (typeof timeout !== "number") {
    throw createTimeoutArgTypeError("timeout", timeout);
  }
  if (!Number.isFinite(timeout) || timeout < 0) {
    throw createTimeoutRangeError(timeout);
  }
  return timeout;
}
function parseNetSocketInfo(data) {
  if (!data) {
    return null;
  }
  try {
    const parsed = JSON.parse(data);
    return parsed && typeof parsed === "object" ? parsed : null;
  } catch {
    return null;
  }
}
function normalizeNetSocketHandle(handle) {
  if (!handle) {
    throw new Error("net.connect bridge returned an empty socket handle");
  }
  if (typeof handle === "string") {
    return {
      socketId: handle
    };
  }
  if (typeof handle === "object" && (typeof handle.socketId === "string" || typeof handle.socketId === "number")) {
    return handle;
  }
  if (typeof handle === "object" && handle.loopbackHttpTarget) {
    return handle;
  }
  throw new Error("net.connect bridge returned an invalid socket handle");
}
function serializeTlsValue(value) {
  if (value === void 0 || value === null) {
    return void 0;
  }
  if (Array.isArray(value)) {
    const entries = value.map((entry) => serializeTlsValue(entry)).flatMap((entry) => Array.isArray(entry) ? entry : entry ? [entry] : []);
    return entries.length > 0 ? entries : void 0;
  }
  if (typeof value === "string") {
    return { kind: "string", data: value };
  }
  if (Buffer.isBuffer(value) || value instanceof Uint8Array) {
    return { kind: "buffer", data: Buffer.from(value).toString("base64") };
  }
  return void 0;
}
function isTlsSecureContextWrapper(value) {
  return !!value && typeof value === "object" && "__secureExecTlsContext" in value;
}
function buildSerializedTlsOptions(options, extra) {
  const contextOptions = isTlsSecureContextWrapper(options?.secureContext) ? options.secureContext.__secureExecTlsContext : void 0;
  const serialized = {
    ...contextOptions ?? {},
    ...extra
  };
  const key = serializeTlsValue(options?.key);
  const cert = serializeTlsValue(options?.cert);
  const ca = serializeTlsValue(options?.ca);
  if (key !== void 0) serialized.key = key;
  if (cert !== void 0) serialized.cert = cert;
  if (ca !== void 0) serialized.ca = ca;
  if (typeof options?.passphrase === "string") serialized.passphrase = options.passphrase;
  if (typeof options?.ciphers === "string") serialized.ciphers = options.ciphers;
  if (Buffer.isBuffer(options?.session) || options?.session instanceof Uint8Array) {
    serialized.session = Buffer.from(options.session).toString("base64");
  }
  if (Array.isArray(options?.ALPNProtocols)) {
    const protocols = options.ALPNProtocols.filter((value) => typeof value === "string");
    if (protocols.length > 0) {
      serialized.ALPNProtocols = protocols;
    }
  }
  if (typeof options?.minVersion === "string") serialized.minVersion = options.minVersion;
  if (typeof options?.maxVersion === "string") serialized.maxVersion = options.maxVersion;
  if (typeof options?.servername === "string") serialized.servername = options.servername;
  if (typeof options?.rejectUnauthorized === "boolean") {
    serialized.rejectUnauthorized = options.rejectUnauthorized;
  }
  if (typeof options?.requestCert === "boolean") {
    serialized.requestCert = options.requestCert;
  }
  return serialized;
}
function parseTlsState(payload) {
  if (!payload) {
    return null;
  }
  try {
    return JSON.parse(payload);
  } catch {
    return null;
  }
}
function parseTlsClientHello(payload) {
  if (!payload) {
    return null;
  }
  try {
    return JSON.parse(payload);
  } catch {
    return null;
  }
}
function createBridgedTlsError(payload) {
  if (!payload) {
    return new Error("socket error");
  }
  try {
    const parsed = JSON.parse(payload);
    const error = new Error(parsed.message);
    if (parsed.name) error.name = parsed.name;
    if (parsed.code) {
      error.code = parsed.code;
    }
    if (parsed.stack) error.stack = parsed.stack;
    return error;
  } catch {
    return new Error(payload);
  }
}
function deserializeTlsBridgeValue(value, refs = /* @__PURE__ */ new Map()) {
  if (value === null || typeof value === "boolean" || typeof value === "number" || typeof value === "string") {
    return value;
  }
  if (value.type === "undefined") {
    return void 0;
  }
  if (value.type === "buffer") {
    return Buffer.from(value.data, "base64");
  }
  if (value.type === "array") {
    return value.value.map((entry) => deserializeTlsBridgeValue(entry, refs));
  }
  if (value.type === "ref") {
    return refs.get(value.id);
  }
  const target = {};
  refs.set(value.id, target);
  for (const [key, entry] of Object.entries(value.value)) {
    target[key] = deserializeTlsBridgeValue(entry, refs);
  }
  return target;
}
function queryTlsSocket(socketId, query, detailed) {
  if (typeof _netSocketTlsQueryRaw === "undefined") {
    return void 0;
  }
  const payload = _netSocketTlsQueryRaw.applySync(
    void 0,
    detailed === void 0 ? [socketId, query] : [socketId, query, detailed]
  );
  return deserializeTlsBridgeValue(JSON.parse(payload));
}
function finalizeTlsUpgrade(socket, eventName = "secureConnect") {
  socket._tlsUpgrading = false;
  socket.encrypted = true;
  socket.authorized = socket.authorizationError == null;
  if (typeof socket._socketId === "string" && socket._socketId.length > 0) {
    const protocol = queryTlsSocket(socket._socketId, "getProtocol");
    if (typeof protocol === "string" || protocol === null) {
      socket._tlsProtocol = protocol;
    }
    const cipher = queryTlsSocket(socket._socketId, "getCipher");
    if (cipher !== void 0) {
      socket._tlsCipher = cipher;
    }
    const reused = queryTlsSocket(socket._socketId, "isSessionReused");
    if (typeof reused === "boolean") {
      socket._tlsSessionReused = reused;
    }
  }
  socket._touchTimeout();
  socket._emitNet(eventName);
  if (eventName !== "secure") {
    socket._emitNet("secure");
  }
  if (!socket.destroyed && !socket._bridgeReadLoopRunning) {
    socket._nextReadPumpOrigin = "tls";
    void socket._pumpBridgeReads();
  }
}
function createConnectedSocketHandle(socketId) {
  return {
    socketId,
    setNoDelay(enable) {
      _netSocketSetNoDelayRaw?.applySync(void 0, [socketId, enable !== false]);
      return this;
    },
    setKeepAlive(enable, initialDelay) {
      _netSocketSetKeepAliveRaw?.applySync(void 0, [
        socketId,
        enable !== false,
        normalizeKeepAliveDelay(initialDelay)
      ]);
      return this;
    },
    ref() {
      return this;
    },
    unref() {
      return this;
    }
  };
}
function createAcceptedClientHandle(socketId, info) {
  return {
    socketId,
    info,
    setNoDelay(enable) {
      _netSocketSetNoDelayRaw?.applySync(void 0, [socketId, enable !== false]);
      return this;
    },
    setKeepAlive(enable, initialDelay) {
      _netSocketSetKeepAliveRaw?.applySync(void 0, [
        socketId,
        enable !== false,
        normalizeKeepAliveDelay(initialDelay)
      ]);
      return this;
    },
    ref() {
      return this;
    },
    unref() {
      return this;
    }
  };
}
var NET_BRIDGE_TIMEOUT_SENTINEL = "__secure_exec_net_timeout__";
var NET_BRIDGE_POLL_DELAY_MS = 10;
var netBridgePollDelayOverrideMs = null;
function isNetBridgeTraceEnabled() {
  const env = typeof process !== "undefined" ? process.env : globalThis.__agentOSProcessConfigEnv;
  return env?.AGENTOS_NET_BRIDGE_TRACE === "1";
}
function normalizeNetBridgePollDelayMs(value) {
  const numeric = Number(value);
  if (!Number.isFinite(numeric) || numeric < 0) {
    return NET_BRIDGE_POLL_DELAY_MS;
  }
  return Math.trunc(numeric);
}
function netBridgePollDelayMs() {
  return netBridgePollDelayOverrideMs ?? NET_BRIDGE_POLL_DELAY_MS;
}
function isNetRetainOwnedWriteBufferEnabled() {
  const processEnv = typeof process !== "undefined" ? process.env : undefined;
  const configEnv = globalThis.__agentOSProcessConfigEnv;
  return processEnv?.AGENTOS_NET_RETAIN_OWNED_WRITE_BUFFER !== "0" && configEnv?.AGENTOS_NET_RETAIN_OWNED_WRITE_BUFFER !== "0";
}
function createNetBridgeMetrics() {
  return {
    userWriteCalls: 0,
    userWriteBytes: 0,
    queuedWriteChunks: 0,
    queuedWriteBytes: 0,
    queuedWriteCopiedChunks: 0,
    queuedWriteCopiedBytes: 0,
    queuedWriteRetainedChunks: 0,
    queuedWriteRetainedBytes: 0,
    flushCalls: 0,
    flushChunks: 0,
    flushBytes: 0,
    writeBufferedBytesMax: 0,
    writeBufferedChunksMax: 0,
    writeBase64EncodeCalls: 0,
    writeBase64EncodeBytes: 0,
    writeBase64EncodeUs: 0,
    writeRawCalls: 0,
    writeRawBytes: 0,
    writeRawElapsedUs: 0,
    readRawCalls: 0,
    readRawElapsedUs: 0,
    readPumpRuns: 0,
    readTimeoutSentinels: 0,
    readPollTimersScheduled: 0,
    readPollTimerFires: 0,
    readPollTimerFireLagUs: 0,
    readPollTimerFireLagMaxUs: 0,
    readDataEvents: 0,
    readBytes: 0,
    readBase64DecodeCalls: 0,
    readBase64DecodeBytes: 0,
    readBase64DecodeChars: 0,
    readBase64DecodeUs: 0,
    readPayloadMaterializeCalls: 0,
    readPayloadMaterializeBytes: 0,
    readPayloadMaterializeUs: 0,
    readEndEvents: 0,
    readMacrotaskYields: 0,
    readMacrotaskYieldElapsedUs: 0,
    readMacrotaskYieldMaxUs: 0,
    queueReadablePayloads: 0,
    queueReadablePayloadElapsedUs: 0,
    queueReadablePayloadMaxUs: 0,
    queueReadableBytes: 0,
    queueReadableBytesMax: 0,
    queueReadableImmediateReadCalls: 0,
    queueReadableImmediateReadUs: 0,
    queueReadableImmediateReadMaxUs: 0,
    socketReadableEmits: 0,
    socketReadableEmitUs: 0,
    socketReadableEmitMaxUs: 0,
    socketDataEmits: 0,
    socketDataEmitUs: 0,
    socketDataEmitMaxUs: 0,
    readPostDeliveryProbeCalls: 0,
    readPostDeliveryProbeTimeoutSentinels: 0,
    readPostDeliveryProbeDataEvents: 0,
    readPostDeliveryNextRawCalls: 0,
    readPostDeliveryNextRawTimeoutSentinels: 0,
    readPostDeliveryNextRawDataEvents: 0,
    readPostDeliveryToProbeStartUs: 0,
    readPostDeliveryProbeElapsedUs: 0,
    readPostDeliveryProbeMaxUs: 0,
    readPostDeliveryPendingWriteFlushes: 0,
    readPostDeliveryPendingWriteBytes: 0,
    userWriteDuringDataEmitCalls: 0,
    dataEmitStartToUserWriteUs: 0,
    dataEmitEndToUserWriteUs: 0,
    writeQueuedToFlushStartUs: 0,
    writeQueuedToFlushStartMaxUs: 0,
    writeFlushQueuedToRawUs: 0,
    writeFlushQueuedToRawMaxUs: 0,
    readWakeQueuedToPumpStartUs: 0,
    readWakeQueuedToPumpStartMaxUs: 0,
    acceptWakeQueuedToPumpStartUs: 0,
    acceptWakeQueuedToPumpStartMaxUs: 0,
    socketEndEmits: 0,
    socketCloseEmits: 0,
    socketConnectEmits: 0,
    acceptRawCalls: 0,
    acceptRawElapsedUs: 0,
    acceptPumpRuns: 0,
    acceptLoopAlreadyRunning: 0,
    acceptTimeoutSentinels: 0,
    acceptPollTimersScheduled: 0,
    acceptPollTimerFires: 0,
    acceptPollTimerFireLagUs: 0,
    acceptPollTimerFireLagMaxUs: 0,
    acceptConnections: 0,
    acceptJsonParseUs: 0,
    acceptOnConnectionUs: 0,
    connectionEmits: 0,
    readEventWakeups: 0,
    readWakeAttempts: 0,
    readWakeInvalidTargets: 0,
    readWakeAlreadyRunning: 0,
    readWakeNoTimer: 0,
    readWakeNoTimerBeforeFirstPump: 0,
    readWakeNoTimerAfterFirstPump: 0,
    readWakeNoTimerConnected: 0,
    readWakeNoTimerConnecting: 0,
    readWakeNoTimerRefed: 0,
    readWakeNoTimerUnrefed: 0,
    readWakeNoTimerHasDataListener: 0,
    readWakeNoTimerHasReadableListener: 0,
    readWakeNoTimerPendingWriteFlush: 0,
    readWakeNoTimerPendingWriteBytes: 0,
    readFirstPumpAfterNoTimerWakeCalls: 0,
    readFirstPumpAfterNoTimerWakeUs: 0,
    readFirstPumpAfterNoTimerWakeMaxUs: 0,
    readFirstPumpOriginConnectWait: 0,
    readFirstPumpOriginAcceptedHandle: 0,
    readFirstPumpOriginEventWake: 0,
    readFirstPumpOriginTimer: 0,
    readFirstPumpOriginRef: 0,
    readFirstPumpOriginTls: 0,
    readFirstPumpOriginUnknown: 0,
    readFirstPumpResultData: 0,
    readFirstPumpResultEnd: 0,
    readFirstPumpResultTimeout: 0,
    readFirstPumpScheduleCandidates: 0,
    readFirstPumpScheduleQueued: 0,
    readFirstPumpScheduleAlreadyScheduled: 0,
    readFirstPumpScheduleRuns: 0,
    readFirstPumpSchedulePumpCalls: 0,
    readFirstPumpScheduleSkipDestroyed: 0,
    readFirstPumpScheduleSkipTlsUpgrading: 0,
    readFirstPumpScheduleSkipPumpStarted: 0,
    readFirstPumpScheduleSkipLoopRunning: 0,
    readFirstPumpScheduleSkipSocketClosed: 0,
    readFirstPumpScheduleQueuedToRunUs: 0,
    readFirstPumpScheduleQueuedToRunMaxUs: 0,
    readFirstPumpScheduleQueuedToPumpStartUs: 0,
    readFirstPumpScheduleQueuedToPumpStartMaxUs: 0,
    readFirstPumpScheduleResultData: 0,
    readFirstPumpScheduleResultTimeout: 0,
    readFirstPumpScheduleResultEnd: 0,
    peerWakeScans: 0,
    peerWakeInvalidTargets: 0,
    peerWakeFound: 0,
    peerWakeMiss: 0,
    acceptEventWakeups: 0,
    acceptWakeAttempts: 0,
    acceptWakeInvalidTargets: 0,
    acceptWakeNoTimer: 0,
    acceptWakeNoTimerBeforeFirstPump: 0,
    acceptWakeNoTimerAfterFirstPump: 0,
    acceptWakeNoTimerLoopRunning: 0,
    acceptWakeNoTimerLoopActive: 0,
    acceptWakeNoTimerRefed: 0,
    acceptWakeNoTimerUnrefed: 0,
    acceptWakeNoTimerConnections: 0,
    acceptWakeNoTimerConnectionsMax: 0,
    acceptFirstPumpAfterNoTimerWakeCalls: 0,
    acceptFirstPumpAfterNoTimerWakeUs: 0,
    acceptFirstPumpAfterNoTimerWakeMaxUs: 0,
    acceptFirstPumpOriginListen: 0,
    acceptFirstPumpOriginEventWake: 0,
    acceptFirstPumpOriginTimer: 0,
    acceptFirstPumpOriginRef: 0,
    acceptFirstPumpOriginUnknown: 0,
    acceptFirstPumpResultConnection: 0,
    acceptFirstPumpResultTimeout: 0,
    acceptFirstPumpResultEmpty: 0,
    acceptWakeAlreadyRunning: 0,
    acceptWakeSocketScans: 0,
    acceptWakeSocketInvalidTargets: 0,
    acceptWakeSocketFound: 0,
    acceptWakeSocketMiss: 0
  };
}
var netBridgeTraceForced = false;
var netBridgeMetrics = createNetBridgeMetrics();
function isNetBridgeMetricsEnabled() {
  return netBridgeTraceForced || isNetBridgeTraceEnabled();
}
function netBridgeNowUs() {
  if (typeof performance !== "undefined" && typeof performance.now === "function") {
    return Math.round(performance.now() * 1000);
  }
  return Date.now() * 1000;
}
function countNetBridgeMetric(name, amount = 1) {
  if (!isNetBridgeMetricsEnabled()) return;
  netBridgeMetrics[name] = (netBridgeMetrics[name] ?? 0) + amount;
}
function maxNetBridgeMetric(name, value) {
  if (!isNetBridgeMetricsEnabled()) return;
  const numeric = Number(value);
  if (!Number.isFinite(numeric)) return;
  netBridgeMetrics[name] = Math.max(netBridgeMetrics[name] ?? 0, numeric);
}
function countReadFirstPumpOrigin(origin) {
  switch (origin) {
    case "connectWait":
      countNetBridgeMetric("readFirstPumpOriginConnectWait");
      break;
    case "acceptedHandle":
      countNetBridgeMetric("readFirstPumpOriginAcceptedHandle");
      break;
    case "eventWake":
      countNetBridgeMetric("readFirstPumpOriginEventWake");
      break;
    case "timer":
      countNetBridgeMetric("readFirstPumpOriginTimer");
      break;
    case "ref":
      countNetBridgeMetric("readFirstPumpOriginRef");
      break;
    case "tls":
      countNetBridgeMetric("readFirstPumpOriginTls");
      break;
    default:
      countNetBridgeMetric("readFirstPumpOriginUnknown");
      break;
  }
}
function countAcceptFirstPumpOrigin(origin) {
  switch (origin) {
    case "listen":
      countNetBridgeMetric("acceptFirstPumpOriginListen");
      break;
    case "eventWake":
      countNetBridgeMetric("acceptFirstPumpOriginEventWake");
      break;
    case "timer":
      countNetBridgeMetric("acceptFirstPumpOriginTimer");
      break;
    case "ref":
      countNetBridgeMetric("acceptFirstPumpOriginRef");
      break;
    default:
      countNetBridgeMetric("acceptFirstPumpOriginUnknown");
      break;
  }
}
exposeCustomGlobal("__agentOSNetBridgeMetrics", {
  get enabled() {
    return isNetBridgeMetricsEnabled();
  },
  enable() {
    netBridgeTraceForced = true;
  },
  disable() {
    netBridgeTraceForced = false;
  },
  setPollDelayMs(value) {
    netBridgePollDelayOverrideMs = normalizeNetBridgePollDelayMs(value);
  },
  resetPollDelayMs() {
    netBridgePollDelayOverrideMs = null;
  },
  pollDelayMs() {
    return netBridgePollDelayMs();
  },
  reset() {
    netBridgeMetrics = createNetBridgeMetrics();
    if (typeof _benchNetTcpMetricsResetRaw !== "undefined") {
      _benchNetTcpMetricsResetRaw.applySync(void 0, []);
    }
  },
  snapshot() {
    let sidecarNetTrace = void 0;
    if (typeof _benchNetTcpMetricsSnapshotRaw !== "undefined") {
      sidecarNetTrace = _benchNetTcpMetricsSnapshotRaw.applySync(void 0, []);
    }
    return {
      ...netBridgeMetrics,
      ...(sidecarNetTrace ? { sidecarNetTrace } : {})
    };
  }
});
function yieldBridgeMacrotask() {
  return new Promise((resolve) => {
    if (typeof setImmediate === "function") {
      setImmediate(resolve);
    } else {
      setTimeout(resolve, 0);
    }
  });
}
function netSocketDispatch(socketId, event, data) {
  if (socketId === 0 && event.startsWith("http2:")) {
    debugBridgeNetwork("http2 dispatch via netSocket", event);
    try {
      const payload = data ? JSON.parse(data) : {};
      http2Dispatch(
        event.slice("http2:".length),
        Number(payload.id ?? 0),
        payload.data,
        payload.extra,
        payload.extraNumber,
        payload.extraHeaders,
        payload.flags
      );
    } catch {
    }
    return;
  }
  const socket = getRegisteredNetSocket(socketId);
  if (!socket) return;
  switch (event) {
    case "connect": {
      socket._applySocketInfo(parseNetSocketInfo(data));
      socket._connected = true;
      socket.connecting = false;
      socket._touchTimeout();
      socket._emitNet("connect");
      socket._emitNet("ready");
      break;
    }
    case "secureConnect":
    case "secure": {
      const state = parseTlsState(data);
      if (state) {
        socket.authorized = state.authorized === true;
        socket.authorizationError = state.authorizationError;
        socket.alpnProtocol = state.alpnProtocol ?? false;
        socket.servername = state.servername ?? socket.servername;
        socket._tlsProtocol = state.protocol ?? null;
        socket._tlsSessionReused = state.sessionReused === true;
        socket._tlsCipher = state.cipher ?? null;
      }
      finalizeTlsUpgrade(socket, event);
      break;
    }
    case "data": {
      const buf = typeof Buffer !== "undefined" ? Buffer.from(data, "base64") : new Uint8Array(0);
      socket._touchTimeout();
      socket._emitNet("data", buf);
      break;
    }
    case "end":
      socket._handleRemoteReadableEnd();
      break;
    case "session": {
      const session = typeof Buffer !== "undefined" ? Buffer.from(data ?? "", "base64") : new Uint8Array(0);
      socket._tlsSession = Buffer.from(session);
      socket._emitNet("session", session);
      break;
    }
    case "error":
      if (data) {
        try {
          const parsed = JSON.parse(data);
          socket.authorized = parsed.authorized === true;
          socket.authorizationError = parsed.authorizationError;
        } catch {
        }
      }
      socket._emitNet("error", createBridgedTlsError(data));
      break;
    case "close":
      socket._emitSocketClose(false);
      break;
  }
}
exposeCustomGlobal("_netSocketDispatch", netSocketDispatch);
const NET_BRIDGE_MAX_RAW_WRITE_BYTES = 256 * 1024;
var NetSocket = class _NetSocket {
  _listeners = {};
  _onceListeners = {};
  _socketId = 0;
  _loopbackServer = null;
  _loopbackBuffer = Buffer.alloc(0);
  _loopbackDispatchRunning = false;
  _loopbackDispatchPending = false;
  _loopbackReadableEnded = false;
  _loopbackUpgradeSocket = null;
  _loopbackEventQueue = Promise.resolve();
  _encoding;
  _noDelayState = false;
  _keepAliveState = false;
  _keepAliveDelaySeconds = 0;
  _refed = true;
  _bridgeReadLoopRunning = false;
  _bridgeReadPollTimer = null;
  _bridgeReadPumpStarted = false;
  _bridgeReadFirstPumpBenchmarkScheduled = false;
  _readFirstPumpScheduleActive = false;
  _readFirstPumpScheduleQueuedAtUs = 0;
  _nextReadPumpOrigin = null;
  _firstReadNoTimerWakeAtUs = 0;
  _timeoutMs = 0;
  _timeoutTimer = null;
  _pendingBridgeWriteChunks = null;
	    _pendingBridgeWriteCallbacks = null;
	    _pendingBridgeWriteBytes = 0;
	    _bridgeWriteFlushScheduled = false;
	    _bridgeWriteFlushQueuedAtUs = 0;
	    _lastReadDeliveryEndUs = 0;
	    _currentDataEmitStartUs = 0;
	    _lastDataEmitEndUs = 0;
	    _readWakeQueuedAtUs = 0;
	    _tlsUpgrading = false;
  _remoteEnded = false;
  _writableEnded = false;
  _closeEmitted = false;
  _connected = false;
  connecting = false;
  destroyed = false;
  writable = true;
  readable = true;
  readyState = "open";
  readableLength = 0;
  writableLength = 0;
  remoteAddress;
  remotePort;
  remoteFamily;
  localAddress = "0.0.0.0";
  localPort = 0;
  localFamily = "IPv4";
  localPath;
  remotePath;
  bytesRead = 0;
  bytesWritten = 0;
  bufferSize = 0;
  pending = true;
  allowHalfOpen = false;
  encrypted = false;
  authorized = false;
  authorizationError;
  servername;
  alpnProtocol = false;
  writableHighWaterMark = 16 * 1024;
  server;
  _tlsCipher = null;
  _tlsProtocol = null;
  _tlsSession = null;
  _tlsSessionReused = false;
  // Readable stream state stub for library compatibility
  _readableState = { endEmitted: false, ended: false };
  _readQueue = [];
  _handle = null;
  constructor(options) {
    if (options?.allowHalfOpen) this.allowHalfOpen = true;
    if (options?.handle) this._handle = options.handle;
  }
  connect(portOrOptions, hostOrCallback, callback) {
    if (typeof _netSocketConnectRaw === "undefined") {
      throw new Error("net.Socket is not supported in sandbox (bridge not available)");
    }
    const {
      host = "127.0.0.1",
      port = 0,
      path,
      keepAlive,
      keepAliveInitialDelay,
      callback: cb
    } = normalizeConnectArgs(portOrOptions, hostOrCallback, callback);
    if (cb) this.once("connect", cb);
    this.connecting = true;
    this.remoteAddress = path ?? host;
    this.remotePort = path ? void 0 : port;
    this.remotePath = path;
    this.pending = false;
    let handle;
    try {
      handle = normalizeNetSocketHandle(_netSocketConnectRaw.applySync(
        void 0,
        [path ? { path } : { host, port }]
      ));
    } catch (error) {
      this.connecting = false;
      this.pending = false;
      queueMicrotask(() => {
        if (!this.destroyed) {
          this.destroy(error);
        }
      });
      return this;
    }
    if (handle.loopbackHttpTarget) {
      this._loopbackHttpTarget = handle.loopbackHttpTarget;
      this._applySocketInfo(handle);
      this._connected = true;
      this.connecting = false;
      queueMicrotask(() => {
        this._touchTimeout();
        this._emitNet("connect");
        this._emitNet("ready");
      });
      return this;
    }
    debugBridgeNetwork("socket connect", handle.socketId, host, port, path ?? null);
    this._socketId = handle.socketId;
    this._handle = createConnectedSocketHandle(this._socketId);
    this._applySocketInfo(handle);
    registerNetSocket(this._socketId, this);
    wakeNetServerAcceptForSocket(this);
    void this._waitForConnect();
    if (keepAlive) {
      this.once("connect", () => {
        this.setKeepAlive(true, keepAliveInitialDelay);
      });
    }
    return this;
  }
  write(data, encodingOrCallback, callback) {
    let buf;
    let canRetainWriteBuffer = false;
    if (Buffer.isBuffer(data)) {
      buf = data;
    } else if (typeof data === "string") {
      const enc = typeof encodingOrCallback === "string" ? encodingOrCallback : "utf-8";
      buf = Buffer.from(data, enc);
      canRetainWriteBuffer = isNetRetainOwnedWriteBufferEnabled();
    } else {
      buf = Buffer.from(data);
    }
    if (this._loopbackServer || this._loopbackHttpTarget) {
      debugBridgeNetwork("socket write loopback", this._socketId, buf.length);
      this.bytesWritten += buf.length;
      if (this._loopbackUpgradeSocket) {
        this._touchTimeout();
        this._loopbackUpgradeSocket._pushData(buf);
        const cb2 = typeof encodingOrCallback === "function" ? encodingOrCallback : callback;
        if (cb2) cb2();
        return true;
      }
      this._loopbackBuffer = Buffer.concat([this._loopbackBuffer, buf]);
      this._touchTimeout();
      this._dispatchLoopbackHttpRequest();
      const cb2 = typeof encodingOrCallback === "function" ? encodingOrCallback : callback;
      if (cb2) cb2();
      return true;
    }
    if (typeof _netSocketWriteRaw === "undefined") return false;
	      if (this.destroyed || !this._socketId) return false;
	      countNetBridgeMetric("userWriteCalls");
	      countNetBridgeMetric("userWriteBytes", buf.length);
	      if (isNetBridgeMetricsEnabled()) {
	        const nowUs = netBridgeNowUs();
	        if (this._currentDataEmitStartUs > 0) {
	          countNetBridgeMetric("userWriteDuringDataEmitCalls");
	          countNetBridgeMetric("dataEmitStartToUserWriteUs", nowUs - this._currentDataEmitStartUs);
	        } else if (this._lastDataEmitEndUs > 0) {
	          countNetBridgeMetric("dataEmitEndToUserWriteUs", nowUs - this._lastDataEmitEndUs);
	        }
	      }
	      this.bytesWritten += buf.length;
    const cb = typeof encodingOrCallback === "function" ? encodingOrCallback : callback;
    this._queueBridgeWrite(buf, cb, canRetainWriteBuffer);
    return true;
  }
  end(dataOrCallback, encodingOrCallback, callback) {
    if (typeof dataOrCallback === "function") {
      this.once("finish", dataOrCallback);
    } else if (dataOrCallback != null) {
      this.write(dataOrCallback, encodingOrCallback, callback);
    }
    if (this._writableEnded || this.destroyed) {
      return this;
    }
    this._writableEnded = true;
    this.writable = false;
    queueMicrotask(() => {
      if (!this.destroyed) {
        this._emitNet("finish");
        if (this._remoteEnded) {
          this._emitSocketClose(false);
        }
      }
    });
    if (this._loopbackServer || this._loopbackHttpTarget) {
      if (this._loopbackUpgradeSocket) {
        queueMicrotask(() => {
          this._loopbackUpgradeSocket?._pushEnd();
        });
      } else if (!this._loopbackReadableEnded) {
        queueMicrotask(() => {
          this._closeLoopbackReadable();
        });
      }
      return this;
    }
    if (typeof _netSocketEndRaw !== "undefined" && this._socketId && !this.destroyed) {
      this._flushBridgeWrites();
      debugBridgeNetwork("socket end", this._socketId);
      _netSocketEndRaw.applySync(void 0, [this._socketId]);
      this._touchTimeout();
    }
    return this;
  }
  destroy(error) {
    if (this.destroyed) return this;
    debugBridgeNetwork("socket destroy", this._socketId, error?.message ?? null);
    this.destroyed = true;
    this._writableEnded = true;
    this.writable = false;
    this.readable = false;
    this._readableState.endEmitted = true;
    this._readableState.ended = true;
    this._clearTimeoutTimer();
    if (this._bridgeReadPollTimer) {
      clearTimeout(this._bridgeReadPollTimer);
      this._bridgeReadPollTimer = null;
    }
    if (this._loopbackServer || this._loopbackHttpTarget) {
      this._loopbackUpgradeSocket?.destroy(error);
      this._loopbackUpgradeSocket = null;
      this._loopbackServer = null;
      this._loopbackHttpTarget = null;
      if (error) {
        this._emitNet("error", error);
      }
      this._emitSocketClose(Boolean(error));
      return this;
    }
    if (typeof _netSocketDestroyRaw !== "undefined" && this._socketId) {
      _netSocketDestroyRaw.applySync(void 0, [this._socketId]);
    }
    if (error) {
      this._emitNet("error", error);
    }
    this._emitSocketClose(Boolean(error));
    return this;
  }
  _queueBridgeWrite(buf, callback, retainInput = false) {
    if (!this._pendingBridgeWriteChunks) {
      this._pendingBridgeWriteChunks = [];
      this._pendingBridgeWriteCallbacks = [];
    }
    const chunk = retainInput ? buf : Buffer.from(buf);
    this._pendingBridgeWriteChunks.push(chunk);
    this._pendingBridgeWriteBytes += chunk.length;
    countNetBridgeMetric("queuedWriteChunks");
    countNetBridgeMetric("queuedWriteBytes", chunk.length);
    if (retainInput) {
      countNetBridgeMetric("queuedWriteRetainedChunks");
      countNetBridgeMetric("queuedWriteRetainedBytes", chunk.length);
    } else {
      countNetBridgeMetric("queuedWriteCopiedChunks");
      countNetBridgeMetric("queuedWriteCopiedBytes", chunk.length);
    }
    maxNetBridgeMetric("writeBufferedBytesMax", this._pendingBridgeWriteBytes);
    maxNetBridgeMetric("writeBufferedChunksMax", this._pendingBridgeWriteChunks.length);
    if (callback) {
      this._pendingBridgeWriteCallbacks.push(callback);
    }
	      if (!this._bridgeWriteFlushScheduled) {
	        this._bridgeWriteFlushScheduled = true;
	        if (isNetBridgeMetricsEnabled()) {
	          this._bridgeWriteFlushQueuedAtUs = netBridgeNowUs();
	        }
	        queueMicrotask(() => {
	          this._flushBridgeWrites();
	        });
    }
  }
  _flushBridgeWrites() {
	      const chunks = this._pendingBridgeWriteChunks;
	      if (!chunks || chunks.length === 0) {
	        this._bridgeWriteFlushScheduled = false;
	        this._bridgeWriteFlushQueuedAtUs = 0;
	        return;
	      }
    const callbacks = this._pendingBridgeWriteCallbacks ?? [];
    const totalBytes = this._pendingBridgeWriteBytes;
    this._pendingBridgeWriteChunks = null;
    this._pendingBridgeWriteCallbacks = null;
    this._pendingBridgeWriteBytes = 0;
    this._bridgeWriteFlushScheduled = false;
    if (this.destroyed || !this._socketId || typeof _netSocketWriteRaw === "undefined") {
      return;
    }
	      const traceMetrics = isNetBridgeMetricsEnabled();
	      if (traceMetrics && this._bridgeWriteFlushQueuedAtUs > 0) {
	        const queuedToFlushUs = Math.max(0, netBridgeNowUs() - this._bridgeWriteFlushQueuedAtUs);
	        countNetBridgeMetric("writeQueuedToFlushStartUs", queuedToFlushUs);
	        maxNetBridgeMetric("writeQueuedToFlushStartMaxUs", queuedToFlushUs);
	        countNetBridgeMetric("writeFlushQueuedToRawUs", queuedToFlushUs);
	        maxNetBridgeMetric("writeFlushQueuedToRawMaxUs", queuedToFlushUs);
	        this._bridgeWriteFlushQueuedAtUs = 0;
	      }
	      debugBridgeNetwork("socket write", this._socketId, totalBytes, chunks.length);
    countNetBridgeMetric("flushCalls");
    countNetBridgeMetric("flushChunks", chunks.length);
    countNetBridgeMetric("flushBytes", totalBytes);
    const writeStartUs = traceMetrics ? netBridgeNowUs() : 0;
    let pending = [];
    let pendingBytes = 0;
    const flushPending = () => {
      if (pendingBytes === 0) return;
      const payload = pending.length === 1 ? pending[0] : Buffer.concat(pending, pendingBytes);
      countNetBridgeMetric("writeRawCalls");
      countNetBridgeMetric("writeRawBytes", payload.length);
      _netSocketWriteRaw.applySync(void 0, [this._socketId, payload]);
      pending = [];
      pendingBytes = 0;
    };
    for (const chunk of chunks) {
      for (let offset = 0; offset < chunk.length; offset += NET_BRIDGE_MAX_RAW_WRITE_BYTES) {
        const piece = chunk.subarray(offset, offset + NET_BRIDGE_MAX_RAW_WRITE_BYTES);
        if (pendingBytes > 0 && pendingBytes + piece.length > NET_BRIDGE_MAX_RAW_WRITE_BYTES) {
          flushPending();
        }
        pending.push(piece);
        pendingBytes += piece.length;
        if (pendingBytes >= NET_BRIDGE_MAX_RAW_WRITE_BYTES) {
          flushPending();
        }
      }
    }
    flushPending();
    if (traceMetrics) {
      countNetBridgeMetric("writeRawElapsedUs", netBridgeNowUs() - writeStartUs);
    }
    wakePeerBridgeReads(this);
    this._touchTimeout();
    for (const callback of callbacks) {
      callback();
    }
  }
  _emitSocketClose(hadError = false) {
    if (this._closeEmitted) {
      return;
    }
    this._closeEmitted = true;
    this._connected = false;
    this.connecting = false;
    this.pending = false;
    this.readable = false;
    this.writable = false;
    this._clearTimeoutTimer();
    if (this._socketId) {
      unregisterNetSocket(this._socketId);
    }
    this._emitNet("close", hadError);
  }
  _handleRemoteReadableEnd() {
    if (this.destroyed || this._remoteEnded) {
      return;
    }
    debugBridgeNetwork("socket remote end", this._socketId);
    this._remoteEnded = true;
    this.readable = false;
    this._readableState.endEmitted = true;
    this._readableState.ended = true;
    queueMicrotask(() => {
      if (this.destroyed) {
        return;
      }
      this._emitNet("end");
      if (this.destroyed) {
        return;
      }
      if (!this.allowHalfOpen && !this._writableEnded) {
        this.end();
        return;
      }
      if (this._writableEnded) {
        this._emitSocketClose(false);
      }
    });
  }
  _applySocketInfo(info) {
    if (!info) {
      return;
    }
    this.localAddress = info.localAddress;
    this.localPort = info.localPort;
    this.localFamily = info.localFamily;
    this.localPath = info.localPath;
    this.remoteAddress = info.remoteAddress ?? this.remoteAddress;
    this.remotePort = info.remotePort ?? this.remotePort;
    this.remoteFamily = info.remoteFamily ?? this.remoteFamily;
    this.remotePath = info.remotePath ?? this.remotePath;
  }
  _applyAcceptedKeepAlive(initialDelay) {
    this._keepAliveState = true;
    this._keepAliveDelaySeconds = normalizeKeepAliveDelay(initialDelay);
  }
  static fromAcceptedHandle(handle, options) {
    const socket = new _NetSocket({ allowHalfOpen: options?.allowHalfOpen });
    socket._socketId = handle.socketId;
    socket._handle = createConnectedSocketHandle(handle.socketId);
    socket._applySocketInfo(handle.info);
    socket._connected = true;
    socket.connecting = false;
    socket.pending = false;
    registerNetSocket(handle.socketId, socket);
    queueMicrotask(() => {
      if (!socket.destroyed && !socket._tlsUpgrading) {
        socket._nextReadPumpOrigin = "acceptedHandle";
        void socket._pumpBridgeReads();
      }
    });
    return socket;
  }
  setKeepAlive(enable, initialDelay) {
    const nextEnable = isTruthySocketOption(enable);
    const nextDelaySeconds = normalizeKeepAliveDelay(initialDelay);
    if (nextEnable === this._keepAliveState && (!nextEnable || nextDelaySeconds === this._keepAliveDelaySeconds)) {
      return this;
    }
    this._keepAliveState = nextEnable;
    this._keepAliveDelaySeconds = nextEnable ? nextDelaySeconds : 0;
    debugBridgeNetwork("socket setKeepAlive", this._socketId, nextEnable, nextDelaySeconds);
    this._handle?.setKeepAlive?.(nextEnable, nextDelaySeconds);
    return this;
  }
  setNoDelay(noDelay) {
    const nextState = isTruthySocketOption(noDelay);
    if (nextState === this._noDelayState) {
      return this;
    }
    this._noDelayState = nextState;
    debugBridgeNetwork("socket setNoDelay", this._socketId, nextState);
    this._handle?.setNoDelay?.(nextState);
    return this;
  }
  setTimeout(timeout, callback) {
    const nextTimeout = normalizeSocketTimeout(timeout);
    if (callback !== void 0 && typeof callback !== "function") {
      throw createFunctionArgTypeError("callback", callback);
    }
    if (callback) {
      this.once("timeout", callback);
    }
    this._timeoutMs = nextTimeout;
    if (nextTimeout === 0) {
      this._clearTimeoutTimer();
      return this;
    }
    this._touchTimeout();
    return this;
  }
  ref() {
    this._refed = true;
    this._handle?.ref?.();
    if (this._timeoutTimer && typeof this._timeoutTimer.ref === "function") {
      this._timeoutTimer.ref();
    }
    if (!this.destroyed && this._connected && !this._loopbackServer && !this._loopbackHttpTarget && !this._bridgeReadLoopRunning) {
      this._nextReadPumpOrigin = "ref";
      void this._pumpBridgeReads();
    }
    return this;
  }
  unref() {
    this._refed = false;
    this._handle?.unref?.();
    if (this._timeoutTimer && typeof this._timeoutTimer.unref === "function") {
      this._timeoutTimer.unref();
    }
    if (this._bridgeReadPollTimer) {
      clearTimeout(this._bridgeReadPollTimer);
      this._bridgeReadPollTimer = null;
    }
    return this;
  }
  pause() {
    return this;
  }
  resume() {
    return this;
  }
  read(size) {
    if (this._readQueue.length === 0) {
      return null;
    }
    if (size == null || size <= 0) {
      const chunk = this._readQueue.shift() ?? null;
      if (chunk) {
        this.readableLength = Math.max(0, this.readableLength - chunk.length);
      }
      return chunk;
    }
    const head = this._readQueue[0];
    if (!head) {
      return null;
    }
    if (head.length <= size) {
      this._readQueue.shift();
      this.readableLength = Math.max(0, this.readableLength - head.length);
      return head;
    }
    const chunk = head.subarray(0, size);
    this._readQueue[0] = head.subarray(size);
    this.readableLength = Math.max(0, this.readableLength - chunk.length);
    return chunk;
  }
  unshift(chunk) {
    const payload = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
    if (payload.length === 0) {
      return this;
    }
    this._readQueue.unshift(payload);
    this.readableLength += payload.length;
    return this;
  }
  cork() {
  }
  uncork() {
  }
  address() {
    return { port: this.localPort, family: this.localFamily, address: this.localAddress };
  }
  getCipher() {
    return queryTlsSocket(this._socketId, "getCipher") ?? this._tlsCipher;
  }
  getSession() {
    const session = queryTlsSocket(this._socketId, "getSession");
    if (Buffer.isBuffer(session)) {
      this._tlsSession = Buffer.from(session);
      return Buffer.from(session);
    }
    return this._tlsSession ? Buffer.from(this._tlsSession) : null;
  }
  isSessionReused() {
    const reused = queryTlsSocket(this._socketId, "isSessionReused");
    return typeof reused === "boolean" ? reused : this._tlsSessionReused;
  }
  getPeerCertificate(detailed) {
    const cert = queryTlsSocket(this._socketId, "getPeerCertificate", detailed === true);
    return cert && typeof cert === "object" ? cert : {};
  }
  getCertificate() {
    const cert = queryTlsSocket(this._socketId, "getCertificate");
    return cert && typeof cert === "object" ? cert : {};
  }
  getProtocol() {
    const protocol = queryTlsSocket(this._socketId, "getProtocol");
    return typeof protocol === "string" ? protocol : this._tlsProtocol;
  }
  setEncoding(encoding) {
    this._encoding = encoding;
    return this;
  }
  pipe(destination) {
    return destination;
  }
  on(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(listener);
    return this;
  }
  addListener(event, listener) {
    return this.on(event, listener);
  }
  once(event, listener) {
    if (!this._onceListeners[event]) this._onceListeners[event] = [];
    this._onceListeners[event].push(listener);
    return this;
  }
  removeListener(event, listener) {
    const listeners = this._listeners[event];
    if (listeners) {
      const idx = listeners.indexOf(listener);
      if (idx >= 0) listeners.splice(idx, 1);
    }
    const onceListeners = this._onceListeners[event];
    if (onceListeners) {
      const idx = onceListeners.indexOf(listener);
      if (idx >= 0) onceListeners.splice(idx, 1);
    }
    return this;
  }
  off(event, listener) {
    return this.removeListener(event, listener);
  }
  removeAllListeners(event) {
    if (event) {
      delete this._listeners[event];
      delete this._onceListeners[event];
    } else {
      this._listeners = {};
      this._onceListeners = {};
    }
    return this;
  }
  listeners(event) {
    return [...this._listeners[event] ?? [], ...this._onceListeners[event] ?? []];
  }
  listenerCount(event) {
    return (this._listeners[event]?.length ?? 0) + (this._onceListeners[event]?.length ?? 0);
  }
  setMaxListeners(_n) {
    return this;
  }
  getMaxListeners() {
    return 10;
  }
  prependListener(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].unshift(listener);
    return this;
  }
  prependOnceListener(event, listener) {
    if (!this._onceListeners[event]) this._onceListeners[event] = [];
    this._onceListeners[event].unshift(listener);
    return this;
  }
  eventNames() {
    return [.../* @__PURE__ */ new Set([...Object.keys(this._listeners), ...Object.keys(this._onceListeners)])];
  }
  rawListeners(event) {
    return this.listeners(event);
  }
	    emit(event, ...args) {
	      return this._emitNet(event, ...args);
	    }
	    _emitNet(event, ...args) {
	      const traceEmit = isNetBridgeMetricsEnabled() && (event === "readable" || event === "data");
	      const emitStartUs = traceEmit ? netBridgeNowUs() : 0;
	      if (traceEmit && event === "data") {
	        this._currentDataEmitStartUs = emitStartUs;
	      }
	      if (event === "readable") {
	        countNetBridgeMetric("socketReadableEmits");
	      } else if (event === "data") {
      countNetBridgeMetric("socketDataEmits");
    } else if (event === "end") {
      countNetBridgeMetric("socketEndEmits");
    } else if (event === "close") {
      countNetBridgeMetric("socketCloseEmits");
    } else if (event === "connect") {
      countNetBridgeMetric("socketConnectEmits");
    }
	      if (event === "data" && this._encoding && args[0] && Buffer.isBuffer(args[0])) {
	        args[0] = args[0].toString(this._encoding);
	      }
	      let handled = false;
	      try {
	        const listeners = this._listeners[event];
	        if (listeners) {
	          for (const fn of [...listeners]) {
	            fn.call(this, ...args);
	            handled = true;
	          }
	        }
	        const onceListeners = this._onceListeners[event];
	        if (onceListeners) {
	          const fns = [...onceListeners];
	          this._onceListeners[event] = [];
	          for (const fn of fns) {
	            fn.call(this, ...args);
	            handled = true;
	          }
	        }
	      } finally {
	        if (traceEmit) {
	          const elapsedUs = netBridgeNowUs() - emitStartUs;
	          if (event === "readable") {
	            countNetBridgeMetric("socketReadableEmitUs", elapsedUs);
	            maxNetBridgeMetric("socketReadableEmitMaxUs", elapsedUs);
	          } else if (event === "data") {
	            countNetBridgeMetric("socketDataEmitUs", elapsedUs);
	            maxNetBridgeMetric("socketDataEmitMaxUs", elapsedUs);
	            this._lastDataEmitEndUs = netBridgeNowUs();
	            this._currentDataEmitStartUs = 0;
	          }
	        }
	      }
	      return handled;
	    }
	    _queueReadablePayload(payload) {
	      if (!payload || payload.length === 0) {
	        return;
	      }
	      const traceMetrics = isNetBridgeMetricsEnabled();
	      const queueStartUs = traceMetrics ? netBridgeNowUs() : 0;
	      try {
	        this._readQueue.push(payload);
	        this.readableLength += payload.length;
	        countNetBridgeMetric("queueReadablePayloads");
	        countNetBridgeMetric("queueReadableBytes", payload.length);
	        maxNetBridgeMetric("queueReadableBytesMax", this.readableLength);
	        this._emitNet("readable");
	        if (this.listenerCount("data") > 0) {
	          const readStartUs = traceMetrics ? netBridgeNowUs() : 0;
	          const chunk = this.read();
	          if (traceMetrics) {
	            const readElapsedUs = netBridgeNowUs() - readStartUs;
	            countNetBridgeMetric("queueReadableImmediateReadCalls");
	            countNetBridgeMetric("queueReadableImmediateReadUs", readElapsedUs);
	            maxNetBridgeMetric("queueReadableImmediateReadMaxUs", readElapsedUs);
	          }
	          if (chunk !== null) {
	            this._emitNet("data", chunk);
	          }
	        }
	      } finally {
	        if (traceMetrics) {
	          const queueElapsedUs = netBridgeNowUs() - queueStartUs;
	          countNetBridgeMetric("queueReadablePayloadElapsedUs", queueElapsedUs);
	          maxNetBridgeMetric("queueReadablePayloadMaxUs", queueElapsedUs);
	        }
	      }
	    }
  async _waitForConnect() {
    if (typeof _netSocketWaitConnectRaw === "undefined" || this._socketId === 0) {
      return;
    }
    try {
      const infoJson = await _netSocketWaitConnectRaw.apply(
        void 0,
        [this._socketId],
        { result: { promise: true } }
      );
      if (this.destroyed) {
        return;
      }
      this._applySocketInfo(parseNetSocketInfo(infoJson));
      this._connected = true;
      this.connecting = false;
      debugBridgeNetwork("socket connected", this._socketId, this.localAddress, this.localPort, this.remoteAddress, this.remotePort);
      this._touchTimeout();
      debugBridgeNetwork("socket emit connect", this._socketId, this.listenerCount("connect"));
      this._emitNet("connect");
      debugBridgeNetwork("socket emit ready", this._socketId, this.listenerCount("ready"));
      this._emitNet("ready");
      if (!this._tlsUpgrading) {
        this._nextReadPumpOrigin = "connectWait";
        await this._pumpBridgeReads();
      }
    } catch (error) {
      if (this.destroyed) {
        return;
      }
      const err = error instanceof Error ? error : new Error(String(error));
      debugBridgeNetwork("socket connect error", this._socketId, err.message, err.stack ?? null);
      this._emitNet("error", err);
      this.destroy();
    }
  }
	    async _pumpBridgeReads() {
	      if (this._bridgeReadLoopRunning || typeof _netSocketReadRaw === "undefined" || this._socketId === 0) {
	        return;
	      }
	      countNetBridgeMetric("readPumpRuns");
	      const firstPumpRun = !this._bridgeReadPumpStarted;
	      const scheduleActive = this._readFirstPumpScheduleActive === true;
	      if (firstPumpRun) {
	        countReadFirstPumpOrigin(this._nextReadPumpOrigin);
	        if (this._firstReadNoTimerWakeAtUs > 0 && isNetBridgeMetricsEnabled()) {
	          const elapsedUs = Math.max(0, netBridgeNowUs() - this._firstReadNoTimerWakeAtUs);
	          countNetBridgeMetric("readFirstPumpAfterNoTimerWakeCalls");
	          countNetBridgeMetric("readFirstPumpAfterNoTimerWakeUs", elapsedUs);
	          maxNetBridgeMetric("readFirstPumpAfterNoTimerWakeMaxUs", elapsedUs);
	        }
	        if (scheduleActive && this._readFirstPumpScheduleQueuedAtUs > 0 && isNetBridgeMetricsEnabled()) {
	          const queuedToPumpStartUs = Math.max(0, netBridgeNowUs() - this._readFirstPumpScheduleQueuedAtUs);
	          countNetBridgeMetric("readFirstPumpScheduleQueuedToPumpStartUs", queuedToPumpStartUs);
	          maxNetBridgeMetric("readFirstPumpScheduleQueuedToPumpStartMaxUs", queuedToPumpStartUs);
	        }
	      }
	      this._readFirstPumpScheduleActive = false;
	      this._readFirstPumpScheduleQueuedAtUs = 0;
	      this._nextReadPumpOrigin = null;
	      this._bridgeReadPumpStarted = true;
	      let firstPumpResultRecorded = false;
	      let scheduleResultRecorded = false;
	      if (isNetBridgeMetricsEnabled() && this._readWakeQueuedAtUs > 0) {
	        const queuedToPumpUs = Math.max(0, netBridgeNowUs() - this._readWakeQueuedAtUs);
	        countNetBridgeMetric("readWakeQueuedToPumpStartUs", queuedToPumpUs);
	        maxNetBridgeMetric("readWakeQueuedToPumpStartMaxUs", queuedToPumpUs);
	        this._readWakeQueuedAtUs = 0;
	      }
	      this._bridgeReadLoopRunning = true;
	      try {
	        while (!this.destroyed) {
	          countNetBridgeMetric("readRawCalls");
	          const traceMetrics = isNetBridgeMetricsEnabled();
	          const postDeliveryProbeStartUs = traceMetrics && this._lastReadDeliveryEndUs > 0 ? netBridgeNowUs() : 0;
	          if (postDeliveryProbeStartUs > 0) {
	            countNetBridgeMetric("readPostDeliveryProbeCalls");
	            countNetBridgeMetric("readPostDeliveryNextRawCalls");
	            countNetBridgeMetric("readPostDeliveryToProbeStartUs", postDeliveryProbeStartUs - this._lastReadDeliveryEndUs);
	            if (this._bridgeWriteFlushScheduled) {
	              countNetBridgeMetric("readPostDeliveryPendingWriteFlushes");
	              countNetBridgeMetric("readPostDeliveryPendingWriteBytes", this._pendingBridgeWriteBytes);
	            }
	            this._lastReadDeliveryEndUs = 0;
	          }
	          const readStartUs = traceMetrics ? netBridgeNowUs() : 0;
	          const chunk = _netSocketReadRaw.applySync(void 0, [this._socketId]);
	          if (traceMetrics) {
	            const readElapsedUs = netBridgeNowUs() - readStartUs;
	            countNetBridgeMetric("readRawElapsedUs", readElapsedUs);
	            if (postDeliveryProbeStartUs > 0) {
	              countNetBridgeMetric("readPostDeliveryProbeElapsedUs", readElapsedUs);
	              maxNetBridgeMetric("readPostDeliveryProbeMaxUs", readElapsedUs);
	            }
	          }
        if (this.destroyed) {
          return;
        }
	          if (chunk === NET_BRIDGE_TIMEOUT_SENTINEL) {
	            if (firstPumpRun && !firstPumpResultRecorded) {
	              firstPumpResultRecorded = true;
	              countNetBridgeMetric("readFirstPumpResultTimeout");
	            }
	            if (scheduleActive && !scheduleResultRecorded) {
	              scheduleResultRecorded = true;
	              countNetBridgeMetric("readFirstPumpScheduleResultTimeout");
	            }
	            countNetBridgeMetric("readTimeoutSentinels");
	            if (postDeliveryProbeStartUs > 0) {
	              countNetBridgeMetric("readPostDeliveryProbeTimeoutSentinels");
	              countNetBridgeMetric("readPostDeliveryNextRawTimeoutSentinels");
	            }
          if (!this._refed) {
            return;
	            }
	            countNetBridgeMetric("readPollTimersScheduled");
	            const pollDelayMs = netBridgePollDelayMs();
	            const scheduledAtUs = isNetBridgeMetricsEnabled() ? netBridgeNowUs() : 0;
	            this._bridgeReadPollTimer = setTimeout(() => {
	              if (isNetBridgeMetricsEnabled()) {
	                const lagUs = Math.max(0, netBridgeNowUs() - scheduledAtUs - pollDelayMs * 1000);
	                countNetBridgeMetric("readPollTimerFires");
	                countNetBridgeMetric("readPollTimerFireLagUs", lagUs);
	                maxNetBridgeMetric("readPollTimerFireLagMaxUs", lagUs);
	              }
	              this._bridgeReadPollTimer = null;
	              this._nextReadPumpOrigin = "timer";
	              void this._pumpBridgeReads();
	            }, pollDelayMs);
          return;
        }
        if (chunk === null) {
          if (firstPumpRun && !firstPumpResultRecorded) {
            firstPumpResultRecorded = true;
            countNetBridgeMetric("readFirstPumpResultEnd");
          }
          if (scheduleActive && !scheduleResultRecorded) {
            scheduleResultRecorded = true;
            countNetBridgeMetric("readFirstPumpScheduleResultEnd");
          }
          countNetBridgeMetric("readEndEvents");
          this._handleRemoteReadableEnd();
          return;
	          }
	          if (postDeliveryProbeStartUs > 0) {
	            countNetBridgeMetric("readPostDeliveryProbeDataEvents");
	            countNetBridgeMetric("readPostDeliveryNextRawDataEvents");
	          }
	          if (firstPumpRun && !firstPumpResultRecorded) {
	            firstPumpResultRecorded = true;
	            countNetBridgeMetric("readFirstPumpResultData");
	          }
	          if (scheduleActive && !scheduleResultRecorded) {
	            scheduleResultRecorded = true;
	            countNetBridgeMetric("readFirstPumpScheduleResultData");
	          }
	          let payload;
        if (typeof chunk === "string") {
          const decodeStartUs = traceMetrics ? netBridgeNowUs() : 0;
          payload = Buffer.from(chunk, "base64");
          if (traceMetrics) {
            countNetBridgeMetric("readBase64DecodeCalls");
            countNetBridgeMetric("readBase64DecodeBytes", payload.length);
            countNetBridgeMetric("readBase64DecodeChars", chunk.length);
            countNetBridgeMetric("readBase64DecodeUs", netBridgeNowUs() - decodeStartUs);
          }
        } else {
          const materializeStartUs = traceMetrics ? netBridgeNowUs() : 0;
          payload = Buffer.from(chunk);
          if (traceMetrics) {
            countNetBridgeMetric("readPayloadMaterializeCalls");
            countNetBridgeMetric("readPayloadMaterializeBytes", payload.length);
            countNetBridgeMetric("readPayloadMaterializeUs", netBridgeNowUs() - materializeStartUs);
          }
        }
        debugBridgeNetwork("socket data", this._socketId, payload.length);
        countNetBridgeMetric("readDataEvents");
        countNetBridgeMetric("readBytes", payload.length);
        this.bytesRead += payload.length;
        this._touchTimeout();
        // Yield to a macrotask before delivering each payload so that socket
        // bytes surface across distinct event-loop turns, exactly as they do
        // on real Node where each readable arrives in its own I/O callback.
        // _netSocketReadRaw is synchronous, so without this the loop drains an
        // entire HTTP response and emits "readable"/"data" in one synchronous
        // burst. That collapses the turn boundaries undici's keep-alive socket
        // recycling depends on: its setImmediate(client[kResume]) never runs
        // before the caller's microtask dispatches the next request, so the
        // pool keeps every Client at kNeedDrain and allocates a fresh
        // Client+socket per request — leaking EventEmitter listeners
	          // (MaxListenersExceededWarning) and unbounded memory until the VM dies.
	          countNetBridgeMetric("readMacrotaskYields");
	          const yieldStartUs = traceMetrics ? netBridgeNowUs() : 0;
	          await yieldBridgeMacrotask();
	          if (traceMetrics) {
	            const yieldElapsedUs = netBridgeNowUs() - yieldStartUs;
	            countNetBridgeMetric("readMacrotaskYieldElapsedUs", yieldElapsedUs);
	            maxNetBridgeMetric("readMacrotaskYieldMaxUs", yieldElapsedUs);
	          }
	          if (this.destroyed) {
	            return;
	          }
	          this._queueReadablePayload(payload);
	          if (traceMetrics) {
	            this._lastReadDeliveryEndUs = netBridgeNowUs();
	          }
	        }
    } finally {
      this._bridgeReadLoopRunning = false;
    }
  }
  _dispatchLoopbackHttpRequest() {
    if ((!this._loopbackServer && !this._loopbackHttpTarget) || this.destroyed) {
      return;
    }
    if (this._loopbackDispatchRunning) {
      this._loopbackDispatchPending = true;
      return;
    }
    this._loopbackDispatchRunning = true;
    void this._processLoopbackHttpRequests().finally(() => {
      this._loopbackDispatchRunning = false;
      if (this._loopbackDispatchPending && this._loopbackBuffer.length > 0) {
        this._loopbackDispatchPending = false;
        this._dispatchLoopbackHttpRequest();
      } else {
        this._loopbackDispatchPending = false;
      }
    });
  }
  async _processLoopbackHttpRequests() {
    let closeAfterDrain = false;
    while ((this._loopbackServer || this._loopbackHttpTarget) && !this.destroyed) {
      const parserServer = this._loopbackServer ?? { listenerCount: () => 0 };
      const parsed = parseLoopbackRequestBuffer(this._loopbackBuffer, parserServer);
      if (parsed.kind === "incomplete") {
        if (closeAfterDrain) {
          this._closeLoopbackReadable();
        }
        return;
      }
      if (parsed.kind === "bad-request") {
        this._pushLoopbackData(createBadRequestResponseBuffer());
        if (parsed.closeConnection) {
          this._closeLoopbackReadable();
        }
        this._loopbackBuffer = Buffer.alloc(0);
        return;
      }
      this._loopbackBuffer = this._loopbackBuffer.subarray(parsed.bytesConsumed);
      if (parsed.upgradeHead) {
        this._dispatchLoopbackUpgrade(parsed.request, parsed.upgradeHead);
        return;
      }
      let responseJson;
      if (this._loopbackHttpTarget) {
        if (typeof _networkHttpServerRequestRaw === "undefined") {
          throw new Error("HTTP loopback bridge is not available");
        }
        responseJson = _networkHttpServerRequestRaw.applySync(void 0, [{
          ...this._loopbackHttpTarget,
          request: JSON.stringify(parsed.request)
        }]);
      } else {
        ({
          responseJson
        } = await dispatchLoopbackServerRequest(this._loopbackServer, parsed.request));
      }
      const response = JSON.parse(responseJson);
      const serialized = serializeLoopbackResponse(response, parsed.request, parsed.closeConnection);
      if (!closeAfterDrain && serialized.payload.length > 0) {
        this._pushLoopbackData(serialized.payload);
      }
      if (serialized.closeConnection) {
        closeAfterDrain = true;
        if (this._loopbackBuffer.length === 0) {
          this._closeLoopbackReadable();
          return;
        }
      }
    }
  }
  _pushLoopbackData(data) {
    if (data.length === 0 || this._loopbackReadableEnded) {
      return;
    }
    const payload = Buffer.from(data);
    this._queueLoopbackEvent(() => {
      if (this.destroyed) {
        return;
      }
      this.bytesRead += payload.length;
      this._touchTimeout();
      this._queueReadablePayload(payload);
    });
  }
  _closeLoopbackReadable() {
    if (this._loopbackReadableEnded) {
      return;
    }
    this._loopbackReadableEnded = true;
    this.readable = false;
    this.writable = false;
    this._readableState.endEmitted = true;
    this._readableState.ended = true;
    this._clearTimeoutTimer();
    this._queueLoopbackEvent(() => {
      this._emitNet("end");
      this._emitNet("close");
    });
  }
  _queueLoopbackEvent(callback) {
    this._loopbackEventQueue = this._loopbackEventQueue.then(
      () => new Promise((resolve) => {
        queueMicrotask(() => {
          try {
            callback();
          } finally {
            resolve();
          }
        });
      })
    );
  }
  _dispatchLoopbackUpgrade(request, head) {
    if (!this._loopbackServer) {
      return;
    }
    try {
      const socket = new DirectTunnelSocket({
        host: this.remoteAddress,
        port: this.remotePort
      });
      socket._attachPeer({
        _pushData: (data) => this._pushLoopbackData(data),
        _pushEnd: () => this._closeLoopbackReadable()
      });
      this._loopbackUpgradeSocket = socket;
      this._loopbackServer._emit(
        "upgrade",
        new ServerIncomingMessage(request),
        socket,
        head
      );
    } catch (error) {
      const rethrow = error instanceof Error ? error : new Error(String(error));
      let handled = false;
      let exitCodeFromHandler = null;
      if (typeof process !== "undefined" && typeof process.emit === "function") {
        const processEmitter = process;
        try {
          handled = processEmitter.emit("uncaughtException", rethrow, "uncaughtException");
        } catch (emitError) {
          if (emitError && typeof emitError === "object" && emitError.name === "ProcessExitError") {
            handled = true;
            const exitCode = Number(emitError.code);
            exitCodeFromHandler = Number.isFinite(exitCode) ? exitCode : 0;
          } else {
            throw emitError;
          }
        }
      }
      if (handled) {
        if (exitCodeFromHandler !== null) {
          process.exitCode = exitCodeFromHandler;
        }
        this._loopbackServer?.close();
        this.destroy();
        return;
      }
      throw rethrow;
    }
  }
  // Upgrade this socket to TLS
  _upgradeTls(options) {
    if (typeof _netSocketUpgradeTlsRaw === "undefined") {
      throw new Error("tls.connect is not supported in sandbox (bridge not available)");
    }
    this._tlsUpgrading = true;
    if (this._loopbackServer && (typeof this._socketId !== "string" || this._socketId.length === 0)) {
      queueMicrotask(() => {
        if (!this.destroyed) {
          finalizeTlsUpgrade(this);
        }
      });
      return;
    }
    _netSocketUpgradeTlsRaw.applySync(void 0, [this._socketId, JSON.stringify(options ?? {})]);
    queueMicrotask(() => {
      if (!this.destroyed) {
        finalizeTlsUpgrade(this);
      }
    });
  }
  _touchTimeout() {
    if (this._timeoutMs === 0 || this.destroyed) {
      return;
    }
    this._clearTimeoutTimer();
    this._timeoutTimer = setTimeout(() => {
      this._timeoutTimer = null;
      if (this.destroyed) {
        return;
      }
      this._emitNet("timeout");
    }, this._timeoutMs);
    if (!this._refed && typeof this._timeoutTimer.unref === "function") {
      this._timeoutTimer.unref();
    }
  }
  _clearTimeoutTimer() {
    if (this._timeoutTimer) {
      clearTimeout(this._timeoutTimer);
      this._timeoutTimer = null;
    }
  }
};
function netConnect(portOrOptions, hostOrCallback, callback) {
  const socket = new NetSocket();
  socket.connect(portOrOptions, hostOrCallback, callback);
  return socket;
}
var NetServer = class {
  _listeners = {};
  _onceListeners = {};
  _serverId = 0;
  _address = null;
	    _acceptLoopActive = false;
	    _acceptLoopRunning = false;
	    _acceptPollTimer = null;
	    _acceptPumpStarted = false;
	    _nextAcceptPumpOrigin = null;
	    _firstAcceptNoTimerWakeAtUs = 0;
	    _acceptWakeQueuedAtUs = 0;
	    _handleRefId = null;
  _connections = /* @__PURE__ */ new Set();
  _refed = true;
  listening = false;
  keepAlive = false;
  keepAliveInitialDelay = 0;
  allowHalfOpen = false;
  maxConnections;
  _handle;
  constructor(optionsOrListener, maybeListener) {
    if (typeof optionsOrListener === "function") {
      this.on("connection", optionsOrListener);
    } else {
      this.allowHalfOpen = optionsOrListener?.allowHalfOpen === true;
      this.keepAlive = optionsOrListener?.keepAlive === true;
      this.keepAliveInitialDelay = optionsOrListener?.keepAliveInitialDelay ?? 0;
      if (maybeListener) {
        this.on("connection", maybeListener);
      }
    }
    this._handle = {
      onconnection: (err, clientHandle) => {
        if (err) {
          this._emit("error", err);
          return;
        }
        if (!clientHandle) {
          return;
        }
        if (typeof this.maxConnections === "number" && this.maxConnections >= 0 && this._connections.size >= this.maxConnections) {
          this._emit("drop", {
            localAddress: clientHandle.info.localAddress,
            localPort: clientHandle.info.localPort,
            localFamily: clientHandle.info.localFamily,
            remoteAddress: clientHandle.info.remoteAddress,
            remotePort: clientHandle.info.remotePort,
            remoteFamily: clientHandle.info.remoteFamily
          });
          _netSocketDestroyRaw?.applySync(void 0, [clientHandle.socketId]);
          return;
        }
        if (this.keepAlive) {
          clientHandle.setKeepAlive?.(true, this.keepAliveInitialDelay);
        }
        const socket = NetSocket.fromAcceptedHandle(clientHandle, {
          allowHalfOpen: this.allowHalfOpen
        });
        socket.server = this;
        this._connections.add(socket);
        socket.once("close", () => {
          this._connections.delete(socket);
        });
        if (this.keepAlive) {
          socket._applyAcceptedKeepAlive(this.keepAliveInitialDelay);
        }
        countNetBridgeMetric("connectionEmits");
        this._emit("connection", socket);
      }
    };
  }
  listen(portOrOptions, hostOrCallback, backlogOrCallback, callback) {
    if (typeof _netServerListenRaw === "undefined" || typeof _netServerAcceptRaw === "undefined") {
      throw new Error("net.createServer is not supported in sandbox");
    }
    const { port, host, path, backlog, readableAll, writableAll, callback: cb } = normalizeListenArgs(
      portOrOptions,
      hostOrCallback,
      backlogOrCallback,
      callback
    );
    if (cb) {
      this.once("listening", cb);
    }
    try {
      const resultValue = _netServerListenRaw.applySyncPromise(
        void 0,
        [{ port, host, path, backlog, readableAll, writableAll }]
      );
      const result = typeof resultValue === "string" ? JSON.parse(resultValue) : resultValue;
      const address = result.address ?? result;
      this._serverId = result.serverId;
      this._address = address.localPath ? address.localPath : {
        address: address.localAddress,
        family: address.localFamily ?? address.family,
        port: address.localPort
      };
      this.listening = true;
      registerNetServer(this);
      this._syncHandleRef();
      this._acceptLoopActive = true;
      queueMicrotask(() => {
        if (!this.listening || this._serverId === 0) {
          return;
        }
        this._emit("listening");
        this._nextAcceptPumpOrigin = "listen";
        void this._pumpAccepts();
      });
    } catch (error) {
      queueMicrotask(() => {
        this._emit("error", error);
      });
    }
    return this;
  }
  close(callback) {
    if (callback) {
      this.once("close", callback);
    }
    if (!this.listening || typeof _netServerCloseRaw === "undefined") {
      queueMicrotask(() => {
        this._emit("close");
      });
      return this;
    }
    this.listening = false;
    this._acceptLoopActive = false;
    unregisterNetServer(this);
    if (this._acceptPollTimer) {
      clearTimeout(this._acceptPollTimer);
      this._acceptPollTimer = null;
    }
    this._syncHandleRef();
    const serverId = this._serverId;
    this._serverId = 0;
    void (async () => {
      try {
        await _netServerCloseRaw.apply(void 0, [serverId], {
          result: { promise: true }
        });
      } finally {
        this._address = null;
        this._emit("close");
      }
    })();
    return this;
  }
  address() {
    return this._address;
  }
  getConnections(callback) {
    if (typeof callback !== "function") {
      throw createFunctionArgTypeError("callback", callback);
    }
    queueMicrotask(() => {
      callback(null, this._connections.size);
    });
    return this;
  }
  ref() {
    this._refed = true;
    this._syncHandleRef();
    if (this.listening && this._acceptLoopActive && !this._acceptLoopRunning) {
      this._nextAcceptPumpOrigin = "ref";
      void this._pumpAccepts();
    }
    return this;
  }
  unref() {
    this._refed = false;
    if (this._acceptPollTimer) {
      clearTimeout(this._acceptPollTimer);
      this._acceptPollTimer = null;
    }
    this._syncHandleRef();
    return this;
  }
  on(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(listener);
    return this;
  }
  once(event, listener) {
    if (!this._onceListeners[event]) this._onceListeners[event] = [];
    this._onceListeners[event].push(listener);
    return this;
  }
  emit(event, ...args) {
    return this._emit(event, ...args);
  }
  _emit(event, ...args) {
    let handled = false;
    const listeners = this._listeners[event];
    if (listeners) {
      for (const fn of [...listeners]) {
        fn.call(this, ...args);
        handled = true;
      }
    }
    const onceListeners = this._onceListeners[event];
    if (onceListeners) {
      this._onceListeners[event] = [];
      for (const fn of [...onceListeners]) {
        fn.call(this, ...args);
        handled = true;
      }
    }
    return handled;
  }
  _syncHandleRef() {
    if (!this.listening || this._serverId === 0 || !this._refed) {
      if (this._handleRefId && typeof _unregisterHandle === "function") {
        _unregisterHandle(this._handleRefId);
      }
      this._handleRefId = null;
      return;
    }
    const nextHandleId = `${NET_SERVER_HANDLE_PREFIX}${this._serverId}`;
    if (this._handleRefId === nextHandleId) {
      return;
    }
    if (this._handleRefId && typeof _unregisterHandle === "function") {
      _unregisterHandle(this._handleRefId);
    }
    this._handleRefId = nextHandleId;
    if (typeof _registerHandle === "function") {
      _registerHandle(this._handleRefId, "net server");
    }
  }
	    async _pumpAccepts() {
	      if (typeof _netServerAcceptRaw === "undefined" || this._acceptLoopRunning) {
	        if (this._acceptLoopRunning) {
	          countNetBridgeMetric("acceptLoopAlreadyRunning");
	        }
	        return;
	      }
	      countNetBridgeMetric("acceptPumpRuns");
	      const firstPumpRun = !this._acceptPumpStarted;
	      if (firstPumpRun) {
	        countAcceptFirstPumpOrigin(this._nextAcceptPumpOrigin);
	        if (this._firstAcceptNoTimerWakeAtUs > 0 && isNetBridgeMetricsEnabled()) {
	          const elapsedUs = Math.max(0, netBridgeNowUs() - this._firstAcceptNoTimerWakeAtUs);
	          countNetBridgeMetric("acceptFirstPumpAfterNoTimerWakeCalls");
	          countNetBridgeMetric("acceptFirstPumpAfterNoTimerWakeUs", elapsedUs);
	          maxNetBridgeMetric("acceptFirstPumpAfterNoTimerWakeMaxUs", elapsedUs);
	        }
	      }
	      this._nextAcceptPumpOrigin = null;
	      this._acceptPumpStarted = true;
	      let firstPumpResultRecorded = false;
	      if (isNetBridgeMetricsEnabled() && this._acceptWakeQueuedAtUs > 0) {
	        const queuedToPumpUs = Math.max(0, netBridgeNowUs() - this._acceptWakeQueuedAtUs);
	        countNetBridgeMetric("acceptWakeQueuedToPumpStartUs", queuedToPumpUs);
	        maxNetBridgeMetric("acceptWakeQueuedToPumpStartMaxUs", queuedToPumpUs);
	        this._acceptWakeQueuedAtUs = 0;
	      }
	      this._acceptLoopRunning = true;
	      try {
      while (this._acceptLoopActive && this._serverId !== 0) {
        countNetBridgeMetric("acceptRawCalls");
        const traceMetrics = isNetBridgeMetricsEnabled();
        const acceptStartUs = traceMetrics ? netBridgeNowUs() : 0;
        const payload = _netServerAcceptRaw.applySync(void 0, [this._serverId]);
        if (traceMetrics) {
          countNetBridgeMetric("acceptRawElapsedUs", netBridgeNowUs() - acceptStartUs);
        }
        if (payload === NET_BRIDGE_TIMEOUT_SENTINEL) {
          if (firstPumpRun && !firstPumpResultRecorded) {
            firstPumpResultRecorded = true;
            countNetBridgeMetric("acceptFirstPumpResultTimeout");
          }
          countNetBridgeMetric("acceptTimeoutSentinels");
          if (!this._refed) {
            return;
	            }
	            countNetBridgeMetric("acceptPollTimersScheduled");
	            const pollDelayMs = netBridgePollDelayMs();
	            const scheduledAtUs = isNetBridgeMetricsEnabled() ? netBridgeNowUs() : 0;
	            this._acceptPollTimer = setTimeout(() => {
	              if (isNetBridgeMetricsEnabled()) {
	                const lagUs = Math.max(0, netBridgeNowUs() - scheduledAtUs - pollDelayMs * 1000);
	                countNetBridgeMetric("acceptPollTimerFires");
	                countNetBridgeMetric("acceptPollTimerFireLagUs", lagUs);
	                maxNetBridgeMetric("acceptPollTimerFireLagMaxUs", lagUs);
	              }
	              this._acceptPollTimer = null;
	              this._nextAcceptPumpOrigin = "timer";
	              void this._pumpAccepts();
	            }, pollDelayMs);
          return;
        }
        if (!payload) {
          if (firstPumpRun && !firstPumpResultRecorded) {
            firstPumpResultRecorded = true;
            countNetBridgeMetric("acceptFirstPumpResultEmpty");
          }
          return;
        }
        try {
          const parseStartUs = traceMetrics ? netBridgeNowUs() : 0;
          const accepted = JSON.parse(payload);
          if (traceMetrics) {
            countNetBridgeMetric("acceptJsonParseUs", netBridgeNowUs() - parseStartUs);
          }
          countNetBridgeMetric("acceptConnections");
          if (firstPumpRun && !firstPumpResultRecorded) {
            firstPumpResultRecorded = true;
            countNetBridgeMetric("acceptFirstPumpResultConnection");
          }
          const clientHandle = createAcceptedClientHandle(accepted.socketId, accepted.info);
          const onConnectionStartUs = traceMetrics ? netBridgeNowUs() : 0;
          this._handle.onconnection(null, clientHandle);
          if (traceMetrics) {
            countNetBridgeMetric("acceptOnConnectionUs", netBridgeNowUs() - onConnectionStartUs);
          }
        } catch (error) {
          this._emit("error", error);
        }
      }
    } finally {
      this._acceptLoopRunning = false;
    }
  }
};
function NetServerCallable(optionsOrListener, maybeListener) {
  return new NetServer(optionsOrListener, maybeListener);
}
var netModule = {
  BlockList,
  Socket: NetSocket,
  SocketAddress,
  Server: NetServerCallable,
  Stream: NetSocket,
  connect: netConnect,
  createConnection: netConnect,
  createServer(optionsOrListener, maybeListener) {
    return new NetServer(optionsOrListener, maybeListener);
  },
  getDefaultAutoSelectFamily() {
    return defaultAutoSelectFamily;
  },
  getDefaultAutoSelectFamilyAttemptTimeout() {
    return defaultAutoSelectFamilyAttemptTimeout;
  },
  isIP(input) {
    return classifyIpAddress(input);
  },
  isIPv4(input) {
    return classifyIpAddress(input) === 4;
  },
  isIPv6(input) {
    return classifyIpAddress(input) === 6;
  },
  setDefaultAutoSelectFamily(value) {
    defaultAutoSelectFamily = value !== false;
  },
  setDefaultAutoSelectFamilyAttemptTimeout(value) {
    const numeric = Number(value);
    if (!Number.isFinite(numeric) || numeric < 0) {
      throw new RangeError(`Invalid auto-select family attempt timeout: ${value}`);
    }
    defaultAutoSelectFamilyAttemptTimeout = Math.trunc(numeric);
  }
};
function createSecureContextWrapper(options) {
  return {
    __secureExecTlsContext: buildSerializedTlsOptions(options),
    context: {}
  };
}
function adoptRawTlsSocket(rawSocket, options) {
  if (!(rawSocket instanceof NetSocket)) {
    throw new TypeError("tls.TLSSocket requires a net.Socket instance");
  }
  const normalizedOptions = options && typeof options === "object" ? { ...options } : {};
  Object.setPrototypeOf(rawSocket, TLSSocket.prototype);
  const upgradeOptions = buildSerializedTlsOptions(
    normalizedOptions,
    {
      isServer: normalizedOptions.isServer === true,
      servername: normalizedOptions.servername ?? rawSocket.servername ?? rawSocket.remoteAddress ?? "127.0.0.1"
    }
  );
  if (!upgradeOptions.isServer) {
    rawSocket.servername = upgradeOptions.servername;
  }
  if (rawSocket._connected) {
    rawSocket._upgradeTls(upgradeOptions);
  } else {
    rawSocket.once("connect", () => {
      rawSocket._upgradeTls(upgradeOptions);
    });
  }
  return rawSocket;
}
class TLSSocket extends NetSocket {
  constructor(socketOrOptions, options) {
    if (socketOrOptions instanceof NetSocket) {
      super({ allowHalfOpen: socketOrOptions.allowHalfOpen === true });
      return adoptRawTlsSocket(socketOrOptions, options);
    }
    super(
      socketOrOptions && typeof socketOrOptions === "object" ? socketOrOptions : options
    );
  }
}
function tlsConnect(...args) {
  let socket;
  let options;
  const values = [...args];
  const cb = typeof values[values.length - 1] === "function" ? values.pop() : void 0;
  if (values[0] != null && typeof values[0] === "object") {
    options = { ...values[0] };
    if (options.socket) {
      socket = options.socket;
    } else {
      socket = new NetSocket();
      socket.connect({ host: options.host ?? "127.0.0.1", port: options.port });
    }
  } else {
    const positional = {};
    if (values.length > 0) {
      positional.port = values.shift();
    }
    if (typeof values[0] === "string") {
      positional.host = values.shift();
    }
    const providedOptions = values[0] != null && typeof values[0] === "object" ? { ...values[0] } : {};
    options = { ...providedOptions, ...positional };
    socket = new NetSocket();
    socket.connect({
      host: options.host ?? "127.0.0.1",
      port: options.port
    });
  }
  if (cb) socket.once("secureConnect", cb);
  const upgradeOptions = buildSerializedTlsOptions(
    options,
    {
      isServer: false,
      servername: options.servername ?? options.host ?? "127.0.0.1"
    }
  );
  socket.servername = upgradeOptions.servername;
  if (socket._connected) {
    socket._upgradeTls(upgradeOptions);
  } else {
    socket.once("connect", () => {
      socket._upgradeTls(upgradeOptions);
    });
  }
  return socket;
}
function matchesServername(pattern, servername) {
  if (!pattern.startsWith("*.")) {
    return pattern === servername;
  }
  const suffix = pattern.slice(1);
  if (!servername.endsWith(suffix)) {
    return false;
  }
  const prefix = servername.slice(0, -suffix.length);
  return prefix.length > 0 && !prefix.includes(".");
}
var TLSServer = class {
  _listeners = {};
  _onceListeners = {};
  _server;
  _tlsOptions;
  _sniCallback;
  _alpnCallback;
  _contexts = [];
  constructor(optionsOrListener, maybeListener) {
    const options = typeof optionsOrListener === "function" || optionsOrListener === void 0 ? void 0 : optionsOrListener;
    const listener = typeof optionsOrListener === "function" ? optionsOrListener : maybeListener;
    if (options?.ALPNCallback && options?.ALPNProtocols) {
      const error = new Error(
        "The ALPNCallback and ALPNProtocols TLS options are mutually exclusive"
      );
      error.code = "ERR_TLS_ALPN_CALLBACK_WITH_PROTOCOLS";
      throw error;
    }
    this._tlsOptions = buildSerializedTlsOptions(
      options,
      { isServer: true }
    );
    this._sniCallback = options?.SNICallback;
    this._alpnCallback = options?.ALPNCallback;
    this._server = new NetServer(
      options ? {
        allowHalfOpen: options.allowHalfOpen,
        keepAlive: options.keepAlive,
        keepAliveInitialDelay: options.keepAliveInitialDelay
      } : void 0,
      ((socket) => {
        const tlsSocket = socket;
        tlsSocket.server = this;
        void this._handleSecureSocket(tlsSocket);
      })
    );
    if (listener) {
      this.on("secureConnection", listener);
    }
    this._server.on("listening", (...args) => this._emit("listening", ...args));
    this._server.on("close", (...args) => this._emit("close", ...args));
    this._server.on("error", (...args) => this._emit("error", ...args));
    this._server.on("drop", (...args) => this._emit("drop", ...args));
  }
  listen(portOrOptions, hostOrCallback, backlogOrCallback, callback) {
    this._server.listen(portOrOptions, hostOrCallback, backlogOrCallback, callback);
    return this;
  }
  close(callback) {
    if (callback) {
      this.once("close", callback);
    }
    this._server.close();
    return this;
  }
  address() {
    return this._server.address();
  }
  getConnections(callback) {
    this._server.getConnections(callback);
    return this;
  }
  ref() {
    this._server.ref();
    return this;
  }
  unref() {
    this._server.unref();
    return this;
  }
  addContext(servername, context) {
    const wrapper = isTlsSecureContextWrapper(context) ? context : createSecureContextWrapper(
      context && typeof context === "object" ? context : void 0
    );
    this._contexts.push({ servername, context: wrapper });
    return this;
  }
  on(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(listener);
    return this;
  }
  once(event, listener) {
    if (!this._onceListeners[event]) this._onceListeners[event] = [];
    this._onceListeners[event].push(listener);
    return this;
  }
  emit(event, ...args) {
    return this._emit(event, ...args);
  }
  _emit(event, ...args) {
    let handled = false;
    const listeners = this._listeners[event];
    if (listeners) {
      for (const fn of [...listeners]) {
        fn.call(this, ...args);
        handled = true;
      }
    }
    const onceListeners = this._onceListeners[event];
    if (onceListeners) {
      this._onceListeners[event] = [];
      for (const fn of [...onceListeners]) {
        fn.call(this, ...args);
        handled = true;
      }
    }
    return handled;
  }
  async _handleSecureSocket(socket) {
    const clientHello = this._getClientHello(socket);
    const requestedServername = clientHello?.servername;
    if (requestedServername) {
      socket.servername = requestedServername;
    }
    try {
      const upgradeOptions = await this._resolveTlsOptions(
        requestedServername,
        clientHello?.ALPNProtocols ?? []
      );
      if (!upgradeOptions) {
        this._emitTlsClientError(socket, "Invalid SNI context");
        return;
      }
      socket._upgradeTls(upgradeOptions);
      socket.once("secure", () => {
        this._emit("secureConnection", socket);
        this._emit("connection", socket);
      });
      socket.on("error", (error) => {
        this._emit("tlsClientError", error, socket);
      });
    } catch (error) {
      const err = error instanceof Error ? error : new Error(String(error));
      this._emitTlsClientError(socket, err.message, err);
      if (err.uncaught) {
        process.emit?.("uncaughtException", err, "uncaughtException");
      }
    }
  }
  _getClientHello(socket) {
    if (typeof _netSocketGetTlsClientHelloRaw === "undefined") {
      return null;
    }
    const socketId = socket._socketId;
    if (typeof socketId !== "number" || socketId === 0) {
      return null;
    }
    return parseTlsClientHello(
      _netSocketGetTlsClientHelloRaw.applySync(void 0, [socketId])
    );
  }
  async _resolveTlsOptions(servername, clientProtocols) {
    let selectedContext = null;
    let invalidContext = false;
    if (servername && this._sniCallback) {
      selectedContext = await new Promise((resolve, reject) => {
        this._sniCallback?.(servername, (error, context) => {
          if (error) {
            reject(error);
            return;
          }
          if (context == null) {
            resolve(null);
            return;
          }
          if (isTlsSecureContextWrapper(context)) {
            resolve(context);
            return;
          }
          if (context && typeof context === "object" && Object.keys(context).length > 0) {
            resolve(createSecureContextWrapper(context));
            return;
          }
          invalidContext = true;
          resolve(null);
        });
      });
      if (invalidContext) {
        return null;
      }
    } else if (servername) {
      selectedContext = this._findContext(servername);
    }
    const resolvedOptions = {
      ...this._tlsOptions,
      ...selectedContext?.__secureExecTlsContext ?? {},
      isServer: true
    };
    if (this._alpnCallback) {
      const selectedProtocol = this._alpnCallback({
        servername,
        protocols: clientProtocols
      });
      if (selectedProtocol === void 0) {
        const error = new Error("ALPN callback rejected the client protocol list");
        error.code = "ERR_SSL_TLSV1_ALERT_NO_APPLICATION_PROTOCOL";
        throw error;
      }
      if (!clientProtocols.includes(selectedProtocol)) {
        const error = new Error(
          "The ALPNCallback callback returned an invalid protocol"
        );
        error.code = "ERR_TLS_ALPN_CALLBACK_INVALID_RESULT";
        error.uncaught = true;
        throw error;
      }
      resolvedOptions.ALPNProtocols = [selectedProtocol];
    }
    return resolvedOptions;
  }
  _findContext(servername) {
    for (let index = this._contexts.length - 1; index >= 0; index -= 1) {
      const entry = this._contexts[index];
      if (matchesServername(entry.servername, servername)) {
        return entry.context;
      }
    }
    return null;
  }
  _emitTlsClientError(socket, message, existingError) {
    const error = existingError ?? new Error(message);
    socket.servername ??= this._getClientHello(socket)?.servername;
    this._emit("tlsClientError", error, socket);
    socket.destroy();
  }
};
function TLSServerCallable(optionsOrListener, maybeListener) {
  return new TLSServer(optionsOrListener, maybeListener);
}
var tlsModule = {
  connect: tlsConnect,
  TLSSocket,
  Server: TLSServerCallable,
  createServer(optionsOrListener, maybeListener) {
    return new TLSServer(optionsOrListener, maybeListener);
  },
  createSecureContext(options) {
    return createSecureContextWrapper(options);
  },
  getCiphers() {
    if (typeof _tlsGetCiphersRaw === "undefined") {
      throw new Error("tls.getCiphers is not supported in sandbox");
    }
    try {
      return JSON.parse(_tlsGetCiphersRaw.applySync(void 0, []));
    } catch {
      return [];
    }
  },
  DEFAULT_MIN_VERSION: "TLSv1.2",
  DEFAULT_MAX_VERSION: "TLSv1.3"
};
var DGRAM_HANDLE_PREFIX = "dgram-socket:";
function createBadDgramSocketTypeError() {
  return createTypeErrorWithCode(
    "Bad socket type specified. Valid types are: udp4, udp6",
    "ERR_SOCKET_BAD_TYPE"
  );
}
function createDgramAlreadyBoundError() {
  const error = new Error("Socket is already bound");
  error.code = "ERR_SOCKET_ALREADY_BOUND";
  return error;
}
function createDgramAddressError() {
  return new Error("getsockname EBADF");
}
function createDgramArgTypeError(argumentName, expectedType, value) {
  return createTypeErrorWithCode(
    `The "${argumentName}" argument must be of type ${expectedType}. Received ${formatReceivedType(value)}`,
    "ERR_INVALID_ARG_TYPE"
  );
}
function createDgramMissingArgError(argumentName) {
  return createTypeErrorWithCode(
    `The "${argumentName}" argument must be specified`,
    "ERR_MISSING_ARGS"
  );
}
function createDgramNotRunningError() {
  return createErrorWithCode("Not running", "ERR_SOCKET_DGRAM_NOT_RUNNING");
}
function getDgramErrno(code) {
  switch (code) {
    case "EBADF":
      return -9;
    case "EINVAL":
      return -22;
    case "EADDRNOTAVAIL":
      return -99;
    case "ENOPROTOOPT":
      return -92;
  }
}
function createDgramSyscallError(syscall, code) {
  const error = new Error(`${syscall} ${code}`);
  error.code = code;
  error.errno = getDgramErrno(code);
  error.syscall = syscall;
  return error;
}
function createDgramTtlArgTypeError(value) {
  return createTypeErrorWithCode(
    `The "ttl" argument must be of type number. Received ${formatReceivedType(value)}`,
    "ERR_INVALID_ARG_TYPE"
  );
}
function createDgramBufferSizeTypeError() {
  return createTypeErrorWithCode(
    "Buffer size must be a positive integer",
    "ERR_SOCKET_BAD_BUFFER_SIZE"
  );
}
function createDgramBufferSizeSystemError(which, code) {
  const syscall = `uv_${which}_buffer_size`;
  const info = {
    errno: code === "EBADF" ? -9 : -22,
    code,
    message: code === "EBADF" ? "bad file descriptor" : "invalid argument",
    syscall
  };
  const error = new Error(
    `Could not get or set buffer size: ${syscall} returned ${code} (${info.message})`
  );
  error.name = "SystemError [ERR_SOCKET_BUFFER_SIZE]";
  error.code = "ERR_SOCKET_BUFFER_SIZE";
  error.info = info;
  let errno2 = info.errno;
  let syscallValue = syscall;
  Object.defineProperty(error, "errno", {
    enumerable: true,
    configurable: true,
    get() {
      return errno2;
    },
    set(value) {
      errno2 = value;
    }
  });
  Object.defineProperty(error, "syscall", {
    enumerable: true,
    configurable: true,
    get() {
      return syscallValue;
    },
    set(value) {
      syscallValue = value;
    }
  });
  return error;
}
function getPlatformDgramBufferSize(size) {
  if (size <= 0) {
    return size;
  }
  return process.platform === "linux" ? size * 2 : size;
}
function normalizeDgramTtlValue(value, syscall) {
  if (typeof value !== "number") {
    throw createDgramTtlArgTypeError(value);
  }
  if (!Number.isInteger(value) || value <= 0 || value >= 256) {
    throw createDgramSyscallError(syscall, "EINVAL");
  }
  return value;
}
function isIPv4MulticastAddress(address) {
  if (!isIPv4String(address)) {
    return false;
  }
  const first = Number(address.split(".")[0]);
  return first >= 224 && first <= 239;
}
function isIPv4UnicastAddress(address) {
  return isIPv4String(address) && !isIPv4MulticastAddress(address) && address !== "255.255.255.255";
}
function isIPv6MulticastAddress(address) {
  const zoneIndex = address.indexOf("%");
  const normalized = zoneIndex === -1 ? address : address.slice(0, zoneIndex);
  return isIPv6String(address) && normalized.toLowerCase().startsWith("ff");
}
function validateDgramMulticastAddress(type, syscall, address) {
  if (typeof address !== "string") {
    throw createDgramArgTypeError(
      syscall === "addSourceSpecificMembership" || syscall === "dropSourceSpecificMembership" ? "groupAddress" : "multicastAddress",
      "string",
      address
    );
  }
  const isValid = type === "udp6" ? isIPv6MulticastAddress(address) : isIPv4MulticastAddress(address);
  if (!isValid) {
    throw createDgramSyscallError(syscall, "EINVAL");
  }
  return address;
}
function validateDgramSourceAddress(type, syscall, address) {
  if (typeof address !== "string") {
    throw createDgramArgTypeError("sourceAddress", "string", address);
  }
  const isValid = type === "udp6" ? isIPv6String(address) && !isIPv6MulticastAddress(address) : isIPv4UnicastAddress(address);
  if (!isValid) {
    throw createDgramSyscallError(syscall, "EINVAL");
  }
  return address;
}
function normalizeDgramSocketType(value) {
  if (value === "udp4" || value === "udp6") {
    return value;
  }
  throw createBadDgramSocketTypeError();
}
function normalizeDgramSocketOptions(options) {
  if (typeof options === "string") {
    return { type: normalizeDgramSocketType(options) };
  }
  if (!options || typeof options !== "object" || Array.isArray(options)) {
    throw createBadDgramSocketTypeError();
  }
  const typedOptions = options;
  const result = {
    type: normalizeDgramSocketType(typedOptions.type)
  };
  if (typedOptions.recvBufferSize !== void 0) {
    if (typeof typedOptions.recvBufferSize !== "number") {
      throw createInvalidArgTypeError2(
        "options.recvBufferSize",
        "number",
        typedOptions.recvBufferSize
      );
    }
    result.recvBufferSize = typedOptions.recvBufferSize;
  }
  if (typedOptions.sendBufferSize !== void 0) {
    if (typeof typedOptions.sendBufferSize !== "number") {
      throw createInvalidArgTypeError2(
        "options.sendBufferSize",
        "number",
        typedOptions.sendBufferSize
      );
    }
    result.sendBufferSize = typedOptions.sendBufferSize;
  }
  return result;
}
function normalizeDgramAddressValue(address, type, defaultAddress) {
  if (address === void 0 || address === null || address === "") {
    return defaultAddress;
  }
  if (typeof address !== "string") {
    throw createDgramArgTypeError("address", "string", address);
  }
  if (address === "localhost") {
    return type === "udp6" ? "::1" : "127.0.0.1";
  }
  return address;
}
function normalizeDgramPortValue(port) {
  if (typeof port !== "number") {
    throw createDgramArgTypeError("port", "number", port);
  }
  if (!isValidTcpPort(port)) {
    throw createSocketBadPortError(port);
  }
  return port;
}
function createDgramMessageBuffer(value) {
  if (typeof value === "string") {
    return Buffer.from(value);
  }
  if (Buffer.isBuffer(value)) {
    return Buffer.from(value);
  }
  if (ArrayBuffer.isView(value)) {
    return Buffer.from(value.buffer, value.byteOffset, value.byteLength);
  }
  throw createDgramArgTypeError("msg", "string or Buffer or Uint8Array or DataView", value);
}
function createDgramMessageListBuffer(value) {
  if (Array.isArray(value)) {
    return Buffer.concat(value.map((entry) => createDgramMessageBuffer(entry)));
  }
  return createDgramMessageBuffer(value);
}
function normalizeDgramBridgeResult(value) {
  if (typeof value !== "string") {
    return value;
  }
  try {
    return JSON.parse(value);
  } catch {
    return value;
  }
}
function decodeDgramBridgeBytes(value) {
  if (Buffer.isBuffer(value)) {
    return Buffer.from(value);
  }
  if (value instanceof Uint8Array) {
    return Buffer.from(value);
  }
  if (typeof value === "string") {
    return Buffer.from(value, "base64");
  }
  if (value && typeof value === "object") {
    if (value.__type === "Buffer" && typeof value.data === "string") {
      return Buffer.from(value.data, "base64");
    }
    if (value.__agentOSType === "bytes" && typeof value.base64 === "string") {
      return Buffer.from(value.base64, "base64");
    }
  }
  return Buffer.alloc(0);
}
function normalizeDgramBindArgs(args, type) {
  let port;
  let address;
  let callback;
  if (typeof args[0] === "function") {
    callback = args[0];
  } else if (args[0] && typeof args[0] === "object" && !Array.isArray(args[0])) {
    const options = args[0];
    port = options.port;
    address = options.address;
    callback = args[1];
  } else {
    port = args[0];
    if (typeof args[1] === "function") {
      callback = args[1];
    } else {
      address = args[1];
      callback = args[2];
    }
  }
  if (callback !== void 0 && typeof callback !== "function") {
    throw createFunctionArgTypeError("callback", callback);
  }
  return {
    port: port === void 0 ? 0 : normalizeDgramPortValue(port),
    address: normalizeDgramAddressValue(
      address,
      type,
      type === "udp6" ? "::" : "0.0.0.0"
    ),
    callback
  };
}
function normalizeDgramSendArgs(args, type) {
  if (args.length === 0) {
    throw createDgramArgTypeError("msg", "string or Buffer or Uint8Array or DataView", void 0);
  }
  const message = args[0];
  const hasOffsetLength = typeof args[1] === "number" && typeof args[2] === "number" && args.length >= 4;
  if (hasOffsetLength) {
    const source = createDgramMessageBuffer(message);
    const offset = args[1];
    const length = args[2];
    const callback2 = typeof args[4] === "function" ? args[4] : args[5];
    if (callback2 !== void 0 && typeof callback2 !== "function") {
      throw createFunctionArgTypeError("callback", callback2);
    }
    return {
      data: Buffer.from(source.subarray(offset, offset + length)),
      port: normalizeDgramPortValue(args[3]),
      address: normalizeDgramAddressValue(
        typeof args[4] === "function" ? void 0 : args[4],
        type,
        type === "udp6" ? "::1" : "127.0.0.1"
      ),
      callback: callback2
    };
  }
  const callback = typeof args[2] === "function" ? args[2] : args[3];
  if (callback !== void 0 && typeof callback !== "function") {
    throw createFunctionArgTypeError("callback", callback);
  }
  return {
    data: createDgramMessageListBuffer(message),
    port: normalizeDgramPortValue(args[1]),
    address: normalizeDgramAddressValue(
      typeof args[2] === "function" ? void 0 : args[2],
      type,
      type === "udp6" ? "::1" : "127.0.0.1"
    ),
    callback
  };
}
var DgramSocket = class {
  _type;
  _socketId;
  _listeners = {};
  _onceListeners = {};
  _bindPromise = null;
  _receiveLoopRunning = false;
  _receivePollTimer = null;
  _refed = true;
  _closed = false;
  _bound = false;
  _handleRefId = null;
  _recvBufferSize;
  _sendBufferSize;
  _memberships = /* @__PURE__ */ new Set();
  _multicastInterface;
  _broadcast = false;
  _multicastLoopback = 1;
  _multicastTtl = 1;
  _ttl = 64;
  constructor(optionsOrType, listener) {
    if (typeof _dgramSocketCreateRaw === "undefined") {
      throw new Error("dgram.createSocket is not supported in sandbox");
    }
    const options = normalizeDgramSocketOptions(optionsOrType);
    this._type = options.type;
    const result = normalizeDgramBridgeResult(
      _dgramSocketCreateRaw.applySync(void 0, [{ type: this._type }])
    );
    this._socketId = String(result?.socketId ?? result);
    if (listener) {
      this.on("message", listener);
    }
    if (options.recvBufferSize !== void 0) {
      this._setBufferSize("recv", options.recvBufferSize, false);
    }
    if (options.sendBufferSize !== void 0) {
      this._setBufferSize("send", options.sendBufferSize, false);
    }
  }
  bind(...args) {
    const { port, address, callback } = normalizeDgramBindArgs(args, this._type);
    void this._bindInternal(port, address, callback);
    return this;
  }
  send(...args) {
    const { data, port, address, callback } = normalizeDgramSendArgs(args, this._type);
    void this._sendInternal(data, port, address, callback);
  }
  sendto(...args) {
    this.send(...args);
  }
  address() {
    if (typeof _dgramSocketAddressRaw === "undefined") {
      throw createDgramAddressError();
    }
    try {
      return normalizeDgramBridgeResult(
        _dgramSocketAddressRaw.applySync(void 0, [this._socketId])
      );
    } catch {
      throw createDgramAddressError();
    }
  }
  close(callback) {
    if (callback !== void 0 && typeof callback !== "function") {
      throw createFunctionArgTypeError("callback", callback);
    }
    if (callback) {
      this.once("close", callback);
    }
    if (this._closed) {
      return this;
    }
    this._closed = true;
    this._bound = false;
    this._clearReceivePollTimer();
    this._syncHandleRef();
    if (typeof _dgramSocketCloseRaw === "undefined") {
      queueMicrotask(() => {
        this._emit("close");
      });
      return this;
    }
    try {
      _dgramSocketCloseRaw.applySyncPromise(void 0, [this._socketId]);
    } finally {
      queueMicrotask(() => {
        this._emit("close");
      });
    }
    return this;
  }
  ref() {
    this._refed = true;
    this._syncHandleRef();
    if (this._receivePollTimer && typeof this._receivePollTimer.ref === "function") {
      this._receivePollTimer.ref();
    }
    if (this._bound && !this._closed && !this._receiveLoopRunning) {
      void this._pumpMessages();
    }
    return this;
  }
  unref() {
    this._refed = false;
    this._syncHandleRef();
    if (this._receivePollTimer && typeof this._receivePollTimer.unref === "function") {
      this._receivePollTimer.unref();
    }
    return this;
  }
  setRecvBufferSize(size) {
    this._setBufferSize("recv", size);
  }
  setSendBufferSize(size) {
    this._setBufferSize("send", size);
  }
  getRecvBufferSize() {
    return this._getBufferSize("recv");
  }
  getSendBufferSize() {
    return this._getBufferSize("send");
  }
  setBroadcast(flag) {
    this._ensureBoundForSocketOption("setBroadcast");
    this._broadcast = Boolean(flag);
  }
  setTTL(ttl) {
    this._ensureBoundForSocketOption("setTTL");
    this._ttl = normalizeDgramTtlValue(ttl, "setTTL");
    return this._ttl;
  }
  setMulticastTTL(ttl) {
    this._ensureBoundForSocketOption("setMulticastTTL");
    this._multicastTtl = normalizeDgramTtlValue(ttl, "setMulticastTTL");
    return this._multicastTtl;
  }
  setMulticastLoopback(flag) {
    this._ensureBoundForSocketOption("setMulticastLoopback");
    this._multicastLoopback = Number(flag);
    return this._multicastLoopback;
  }
  addMembership(multicastAddress, multicastInterface) {
    if (multicastAddress === void 0) {
      throw createDgramMissingArgError("multicastAddress");
    }
    if (this._closed) {
      throw createDgramNotRunningError();
    }
    const groupAddress = validateDgramMulticastAddress(
      this._type,
      "addMembership",
      multicastAddress
    );
    if (multicastInterface !== void 0 && typeof multicastInterface !== "string") {
      throw createDgramArgTypeError("multicastInterface", "string", multicastInterface);
    }
    this._memberships.add(`${groupAddress}|${multicastInterface ?? ""}`);
  }
  dropMembership(multicastAddress, multicastInterface) {
    if (multicastAddress === void 0) {
      throw createDgramMissingArgError("multicastAddress");
    }
    if (this._closed) {
      throw createDgramNotRunningError();
    }
    const groupAddress = validateDgramMulticastAddress(
      this._type,
      "dropMembership",
      multicastAddress
    );
    if (multicastInterface !== void 0 && typeof multicastInterface !== "string") {
      throw createDgramArgTypeError("multicastInterface", "string", multicastInterface);
    }
    const membershipKey = `${groupAddress}|${multicastInterface ?? ""}`;
    if (!this._memberships.has(membershipKey)) {
      throw createDgramSyscallError("dropMembership", "EADDRNOTAVAIL");
    }
    this._memberships.delete(membershipKey);
  }
  addSourceSpecificMembership(sourceAddress, groupAddress, multicastInterface) {
    if (this._closed) {
      throw createDgramNotRunningError();
    }
    if (typeof sourceAddress !== "string") {
      throw createDgramArgTypeError("sourceAddress", "string", sourceAddress);
    }
    if (typeof groupAddress !== "string") {
      throw createDgramArgTypeError("groupAddress", "string", groupAddress);
    }
    const validatedSource = validateDgramSourceAddress(
      this._type,
      "addSourceSpecificMembership",
      sourceAddress
    );
    const validatedGroup = validateDgramMulticastAddress(
      this._type,
      "addSourceSpecificMembership",
      groupAddress
    );
    if (multicastInterface !== void 0 && typeof multicastInterface !== "string") {
      throw createDgramArgTypeError("multicastInterface", "string", multicastInterface);
    }
    this._memberships.add(`${validatedSource}>${validatedGroup}|${multicastInterface ?? ""}`);
  }
  dropSourceSpecificMembership(sourceAddress, groupAddress, multicastInterface) {
    if (this._closed) {
      throw createDgramNotRunningError();
    }
    if (typeof sourceAddress !== "string") {
      throw createDgramArgTypeError("sourceAddress", "string", sourceAddress);
    }
    if (typeof groupAddress !== "string") {
      throw createDgramArgTypeError("groupAddress", "string", groupAddress);
    }
    const validatedSource = validateDgramSourceAddress(
      this._type,
      "dropSourceSpecificMembership",
      sourceAddress
    );
    const validatedGroup = validateDgramMulticastAddress(
      this._type,
      "dropSourceSpecificMembership",
      groupAddress
    );
    if (multicastInterface !== void 0 && typeof multicastInterface !== "string") {
      throw createDgramArgTypeError("multicastInterface", "string", multicastInterface);
    }
    const membershipKey = `${validatedSource}>${validatedGroup}|${multicastInterface ?? ""}`;
    if (!this._memberships.has(membershipKey)) {
      throw createDgramSyscallError("dropSourceSpecificMembership", "EADDRNOTAVAIL");
    }
    this._memberships.delete(membershipKey);
  }
  setMulticastInterface(interfaceAddress) {
    if (typeof interfaceAddress !== "string") {
      throw createDgramArgTypeError("interfaceAddress", "string", interfaceAddress);
    }
    if (this._closed) {
      throw createDgramNotRunningError();
    }
    this._ensureBoundForSocketOption("setMulticastInterface");
    if (this._type === "udp4") {
      if (interfaceAddress === "0.0.0.0") {
        this._multicastInterface = interfaceAddress;
        return;
      }
      if (!isIPv4String(interfaceAddress)) {
        throw createDgramSyscallError("setMulticastInterface", "ENOPROTOOPT");
      }
      if (!isIPv4UnicastAddress(interfaceAddress)) {
        throw createDgramSyscallError("setMulticastInterface", "EADDRNOTAVAIL");
      }
      this._multicastInterface = interfaceAddress;
      return;
    }
    if (interfaceAddress === "" || interfaceAddress === "undefined" || !isIPv6String(interfaceAddress)) {
      throw createDgramSyscallError("setMulticastInterface", "EINVAL");
    }
    this._multicastInterface = interfaceAddress;
  }
  on(event, listener) {
    if (!this._listeners[event]) this._listeners[event] = [];
    this._listeners[event].push(listener);
    return this;
  }
  addListener(event, listener) {
    return this.on(event, listener);
  }
  once(event, listener) {
    if (!this._onceListeners[event]) this._onceListeners[event] = [];
    this._onceListeners[event].push(listener);
    return this;
  }
  removeListener(event, listener) {
    const listeners = this._listeners[event];
    if (listeners) {
      const index = listeners.indexOf(listener);
      if (index >= 0) listeners.splice(index, 1);
    }
    const onceListeners = this._onceListeners[event];
    if (onceListeners) {
      const index = onceListeners.indexOf(listener);
      if (index >= 0) onceListeners.splice(index, 1);
    }
    return this;
  }
  off(event, listener) {
    return this.removeListener(event, listener);
  }
  emit(event, ...args) {
    return this._emit(event, ...args);
  }
  async _bindInternal(port, address, callback) {
    if (this._closed) {
      return;
    }
    if (this._bound || this._bindPromise) {
      throw createDgramAlreadyBoundError();
    }
    if (typeof _dgramSocketBindRaw === "undefined") {
      throw new Error("dgram.bind is not supported in sandbox");
    }
    this._bindPromise = (async () => {
      try {
        normalizeDgramBridgeResult(_dgramSocketBindRaw.applySyncPromise(void 0, [
          this._socketId,
          { port, address }
        ]));
        this._bound = true;
        this._applyInitialBufferSizes();
        this._syncHandleRef();
        queueMicrotask(() => {
          if (this._closed) {
            return;
          }
          this._emit("listening");
          callback?.call(this);
          void this._pumpMessages();
        });
      } catch (error) {
        queueMicrotask(() => {
          this._emit("error", error);
        });
        throw error;
      } finally {
        this._bindPromise = null;
      }
    })();
    return this._bindPromise;
  }
  async _ensureBound() {
    if (this._bound) {
      return;
    }
    if (this._bindPromise) {
      await this._bindPromise;
      return;
    }
    await this._bindInternal(0, this._type === "udp6" ? "::" : "0.0.0.0");
  }
  async _sendInternal(data, port, address, callback) {
    try {
      await this._ensureBound();
      if (this._closed || typeof _dgramSocketSendRaw === "undefined") {
        return;
      }
      const result = normalizeDgramBridgeResult(_dgramSocketSendRaw.applySyncPromise(void 0, [
        this._socketId,
        data,
        { port, address }
      ]));
      if (callback) {
        queueMicrotask(() => {
          callback(null, typeof result?.bytes === "number" ? result.bytes : data.length);
        });
      }
    } catch (error) {
      if (callback) {
        queueMicrotask(() => {
          callback(error);
        });
        return;
      }
      queueMicrotask(() => {
        this._emit("error", error);
      });
    }
  }
  async _pumpMessages() {
    if (this._receiveLoopRunning || this._closed || !this._bound) {
      return;
    }
    if (typeof _dgramSocketRecvRaw === "undefined") {
      return;
    }
    this._receiveLoopRunning = true;
    try {
      while (!this._closed && this._bound) {
        const payload = normalizeDgramBridgeResult(
          _dgramSocketRecvRaw.applySync(void 0, [this._socketId, NET_BRIDGE_POLL_DELAY_MS])
        );
        if (payload === NET_BRIDGE_TIMEOUT_SENTINEL || !payload) {
          this._receivePollTimer = setTimeout(() => {
            this._receivePollTimer = null;
            void this._pumpMessages();
          }, NET_BRIDGE_POLL_DELAY_MS);
          if (!this._refed && typeof this._receivePollTimer.unref === "function") {
            this._receivePollTimer.unref();
          }
          return;
        }
        if (payload.type === "message") {
          const message = decodeDgramBridgeBytes(payload.data);
          this._emit("message", message, {
            address: payload.remoteAddress,
            family: payload.remoteFamily ?? socketFamilyForAddress(payload.remoteAddress),
            port: payload.remotePort,
            size: message.length
          });
          continue;
        }
        if (payload.type === "error") {
          const error = new Error(
            typeof payload.message === "string" ? payload.message : "secure-exec dgram socket error"
          );
          if (typeof payload.code === "string" && payload.code.length > 0) {
            error.code = payload.code;
          }
          this._emit("error", error);
        }
      }
    } catch (error) {
      this._emit("error", error);
    } finally {
      this._receiveLoopRunning = false;
    }
  }
  _clearReceivePollTimer() {
    if (this._receivePollTimer) {
      clearTimeout(this._receivePollTimer);
      this._receivePollTimer = null;
    }
  }
  _ensureBoundForSocketOption(syscall) {
    if (!this._bound || this._closed) {
      throw createDgramSyscallError(syscall, "EBADF");
    }
  }
  _setBufferSize(which, size, requireRunning = true) {
    if (!Number.isInteger(size) || size <= 0 || !Number.isFinite(size)) {
      throw createDgramBufferSizeTypeError();
    }
    if (size > 2147483647) {
      throw createDgramBufferSizeSystemError(which, "EINVAL");
    }
    if (requireRunning && (!this._bound || this._closed)) {
      throw createDgramBufferSizeSystemError(which, "EBADF");
    }
    if (typeof _dgramSocketSetBufferSizeRaw !== "undefined" && this._bound && !this._closed) {
      _dgramSocketSetBufferSizeRaw.applySync(void 0, [this._socketId, which, size]);
    }
    if (which === "recv") {
      this._recvBufferSize = size;
      return;
    }
    this._sendBufferSize = size;
  }
  _getBufferSize(which) {
    if (!this._bound || this._closed) {
      throw createDgramBufferSizeSystemError(which, "EBADF");
    }
    const fallback = which === "recv" ? this._recvBufferSize ?? 0 : this._sendBufferSize ?? 0;
    if (typeof _dgramSocketGetBufferSizeRaw === "undefined") {
      return getPlatformDgramBufferSize(fallback);
    }
    const rawSize = _dgramSocketGetBufferSizeRaw.applySync(void 0, [this._socketId, which]);
    return getPlatformDgramBufferSize(rawSize > 0 ? rawSize : fallback);
  }
  _applyInitialBufferSizes() {
    if (this._recvBufferSize !== void 0) {
      this._setBufferSize("recv", this._recvBufferSize);
    }
    if (this._sendBufferSize !== void 0) {
      this._setBufferSize("send", this._sendBufferSize);
    }
  }
  _syncHandleRef() {
    if (!this._bound || this._closed || !this._refed) {
      if (this._handleRefId && typeof _unregisterHandle === "function") {
        _unregisterHandle(this._handleRefId);
      }
      this._handleRefId = null;
      return;
    }
    const nextHandleId = `${DGRAM_HANDLE_PREFIX}${this._socketId}`;
    if (this._handleRefId === nextHandleId) {
      return;
    }
    if (this._handleRefId && typeof _unregisterHandle === "function") {
      _unregisterHandle(this._handleRefId);
    }
    this._handleRefId = nextHandleId;
    if (typeof _registerHandle === "function") {
      _registerHandle(this._handleRefId, "dgram socket");
    }
  }
  _emit(event, ...args) {
    let handled = false;
    const listeners = this._listeners[event];
    if (listeners) {
      for (const listener of [...listeners]) {
        listener(...args);
        handled = true;
      }
    }
    const onceListeners = this._onceListeners[event];
    if (onceListeners) {
      this._onceListeners[event] = [];
      for (const listener of [...onceListeners]) {
        listener(...args);
        handled = true;
      }
    }
    return handled;
  }
};
var dgramModule = {
  Socket: DgramSocket,
  createSocket(optionsOrType, callback) {
    return new DgramSocket(optionsOrType, callback);
  }
};
function isSqlitePlainObject(value) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return false;
  }
  if (Buffer.isBuffer(value) || value instanceof Uint8Array) {
    return false;
  }
  const prototype = Object.getPrototypeOf(value);
  return prototype === Object.prototype || prototype === null;
}
function encodeSqliteValue(value) {
  if (value === null || value === void 0 || typeof value === "boolean" || typeof value === "number" || typeof value === "string") {
    return value ?? null;
  }
  if (typeof value === "bigint") {
    return {
      __agentosSqliteType: "bigint",
      value: value.toString()
    };
  }
  if (Buffer.isBuffer(value) || value instanceof Uint8Array) {
    return {
      __agentosSqliteType: "uint8array",
      value: Buffer.from(value).toString("base64")
    };
  }
  if (Array.isArray(value)) {
    return value.map((entry) => encodeSqliteValue(entry));
  }
  if (value && typeof value === "object") {
    return Object.fromEntries(
      Object.entries(value).map(([key, entry]) => [key, encodeSqliteValue(entry)])
    );
  }
  return null;
}
function decodeSqliteValue(value) {
  if (value === null || value === void 0 || typeof value === "boolean" || typeof value === "number" || typeof value === "string") {
    return value ?? null;
  }
  if (Array.isArray(value)) {
    return value.map((entry) => decodeSqliteValue(entry));
  }
  if (value && typeof value === "object") {
    if (value.__agentosSqliteType === "bigint" && typeof value.value === "string") {
      return BigInt(value.value);
    }
    if (value.__agentosSqliteType === "uint8array" && typeof value.value === "string") {
      return Buffer.from(value.value, "base64");
    }
    return Object.fromEntries(
      Object.entries(value).map(([key, entry]) => [key, decodeSqliteValue(entry)])
    );
  }
  return value;
}
function normalizeSqliteParams(params) {
  if (!Array.isArray(params) || params.length === 0) {
    return null;
  }
  if (params.length === 1 && isSqlitePlainObject(params[0])) {
    return encodeSqliteValue(params[0]);
  }
  return params.map((entry) => encodeSqliteValue(entry));
}
function sqliteBridgeCall(bridgeFn, args, label) {
  if (typeof bridgeFn === "function") {
    return decodeSqliteValue(bridgeFn(...args));
  }
  if (!bridgeFn) {
    throw new Error(`sqlite bridge is not available for ${label}`);
  }
  if (typeof bridgeFn.applySync === "function") {
    return decodeSqliteValue(bridgeFn.applySync(void 0, args));
  }
  if (typeof bridgeFn.applySyncPromise === "function") {
    return decodeSqliteValue(bridgeFn.applySyncPromise(void 0, args));
  }
  throw new Error(`sqlite bridge is not available for ${label}`);
}
var _sqliteConstants = createBridgeSyncFacade("_sqliteConstantsRaw");
var _sqliteDatabaseOpen = createBridgeSyncFacade("_sqliteDatabaseOpenRaw");
var _sqliteDatabaseClose = createBridgeSyncFacade("_sqliteDatabaseCloseRaw");
var _sqliteDatabaseExec = createBridgeSyncFacade("_sqliteDatabaseExecRaw");
var _sqliteDatabaseQuery = createBridgeSyncFacade("_sqliteDatabaseQueryRaw");
var _sqliteDatabasePrepare = createBridgeSyncFacade("_sqliteDatabasePrepareRaw");
var _sqliteDatabaseLocation = createBridgeSyncFacade("_sqliteDatabaseLocationRaw");
var _sqliteDatabaseCheckpoint = createBridgeSyncFacade("_sqliteDatabaseCheckpointRaw");
var _sqliteStatementRun = createBridgeSyncFacade("_sqliteStatementRunRaw");
var _sqliteStatementGet = createBridgeSyncFacade("_sqliteStatementGetRaw");
var _sqliteStatementAll = createBridgeSyncFacade("_sqliteStatementAllRaw");
var _sqliteStatementColumns = createBridgeSyncFacade("_sqliteStatementColumnsRaw");
var _sqliteStatementSetReturnArrays = createBridgeSyncFacade("_sqliteStatementSetReturnArraysRaw");
var _sqliteStatementSetReadBigInts = createBridgeSyncFacade("_sqliteStatementSetReadBigIntsRaw");
var _sqliteStatementSetAllowBareNamedParameters = createBridgeSyncFacade("_sqliteStatementSetAllowBareNamedParametersRaw");
var _sqliteStatementSetAllowUnknownNamedParameters = createBridgeSyncFacade("_sqliteStatementSetAllowUnknownNamedParametersRaw");
var _sqliteStatementFinalize = createBridgeSyncFacade("_sqliteStatementFinalizeRaw");
var StatementSync = class {
  constructor(database, statementId) {
    this._database = database;
    this._statementId = statementId;
    this._finalized = false;
  }
  _assertOpen() {
    this._database._assertOpen();
    if (this._finalized) {
      throw new Error("SQLite statement is already finalized");
    }
  }
  run(...params) {
    this._assertOpen();
    return sqliteBridgeCall(
      _sqliteStatementRun,
      [this._statementId, normalizeSqliteParams(params)],
      "statement.run"
    );
  }
  get(...params) {
    this._assertOpen();
    return sqliteBridgeCall(
      _sqliteStatementGet,
      [this._statementId, normalizeSqliteParams(params)],
      "statement.get"
    );
  }
  all(...params) {
    this._assertOpen();
    return sqliteBridgeCall(
      _sqliteStatementAll,
      [this._statementId, normalizeSqliteParams(params)],
      "statement.all"
    );
  }
  iterate(...params) {
    const rows = this.all(...params);
    return rows[Symbol.iterator]();
  }
  columns() {
    this._assertOpen();
    return sqliteBridgeCall(
      _sqliteStatementColumns,
      [this._statementId],
      "statement.columns"
    );
  }
  setReturnArrays(enabled) {
    this._assertOpen();
    sqliteBridgeCall(
      _sqliteStatementSetReturnArrays,
      [this._statementId, Boolean(enabled)],
      "statement.setReturnArrays"
    );
  }
  setReadBigInts(enabled) {
    this._assertOpen();
    sqliteBridgeCall(
      _sqliteStatementSetReadBigInts,
      [this._statementId, Boolean(enabled)],
      "statement.setReadBigInts"
    );
  }
  setAllowBareNamedParameters(enabled) {
    this._assertOpen();
    sqliteBridgeCall(
      _sqliteStatementSetAllowBareNamedParameters,
      [this._statementId, Boolean(enabled)],
      "statement.setAllowBareNamedParameters"
    );
  }
  setAllowUnknownNamedParameters(enabled) {
    this._assertOpen();
    sqliteBridgeCall(
      _sqliteStatementSetAllowUnknownNamedParameters,
      [this._statementId, Boolean(enabled)],
      "statement.setAllowUnknownNamedParameters"
    );
  }
  finalize() {
    if (this._finalized) {
      return null;
    }
    this._database._assertOpen();
    sqliteBridgeCall(
      _sqliteStatementFinalize,
      [this._statementId],
      "statement.finalize"
    );
    this._finalized = true;
    return null;
  }
};
var DatabaseSync = class {
  constructor(location = ":memory:", options = void 0) {
    this._closed = false;
    this._databaseId = sqliteBridgeCall(
      _sqliteDatabaseOpen,
      [typeof location === "string" ? location : ":memory:", options ?? null],
      "database.open"
    );
  }
  _assertOpen() {
    if (this._closed) {
      throw new Error("SQLite database is already closed");
    }
  }
  close() {
    if (this._closed) {
      return null;
    }
    sqliteBridgeCall(
      _sqliteDatabaseClose,
      [this._databaseId],
      "database.close"
    );
    this._closed = true;
    return null;
  }
  exec(sql) {
    this._assertOpen();
    return sqliteBridgeCall(
      _sqliteDatabaseExec,
      [this._databaseId, String(sql ?? "")],
      "database.exec"
    );
  }
  query(sql, params = null, options = null) {
    this._assertOpen();
    const normalized = params === null ? null : normalizeSqliteParams(Array.isArray(params) ? params : [params]);
    return sqliteBridgeCall(
      _sqliteDatabaseQuery,
      [this._databaseId, String(sql ?? ""), normalized, options ?? null],
      "database.query"
    );
  }
  prepare(sql) {
    this._assertOpen();
    const statementId = sqliteBridgeCall(
      _sqliteDatabasePrepare,
      [this._databaseId, String(sql ?? "")],
      "database.prepare"
    );
    return new StatementSync(this, statementId);
  }
  location() {
    this._assertOpen();
    return sqliteBridgeCall(
      _sqliteDatabaseLocation,
      [this._databaseId],
      "database.location"
    );
  }
  checkpoint() {
    this._assertOpen();
    return sqliteBridgeCall(
      _sqliteDatabaseCheckpoint,
      [this._databaseId],
      "database.checkpoint"
    );
  }
};
DatabaseSync.prototype[Symbol.dispose] = DatabaseSync.prototype.close;
StatementSync.prototype[Symbol.dispose] = StatementSync.prototype.finalize;
var sqliteConstants;
function getSqliteConstants() {
  if (sqliteConstants === void 0) {
    sqliteConstants = Object.freeze(
      sqliteBridgeCall(_sqliteConstants, [], "constants") ?? {}
    );
  }
  return sqliteConstants;
}
var sqliteModule = {
  DatabaseSync,
  StatementSync,
  get constants() {
    return getSqliteConstants();
  }
};
exposeCustomGlobal("_netModule", netModule);
exposeCustomGlobal("_tlsModule", tlsModule);
exposeCustomGlobal("_dgramModule", dgramModule);
exposeCustomGlobal("_sqliteModule", sqliteModule);
var network_default = {
  fetch,
  Headers,
  Request,
  Response,
  dns,
  http,
  https,
  http2,
  IncomingMessage,
  ClientRequest,
  net: netModule,
  tls: tlsModule,
  dgram: dgramModule
};
export { network_exports, MAX_HTTP_BODY_BYTES, MAX_HTTP_REQUEST_HEADER_BYTES, MAX_HTTP_REQUEST_HEADERS, _fetchHandleCounter, serializeFetchHeaders, createFetchHeaders, normalizeFetchRequestInit, ensureFetchAcceptEncoding, fetch, Headers, Request, Response, normalizeDnsLookupInvocation, createUnsupportedDnsError, normalizeDnsResolveInvocation, parseDnsLookupRecords, parseDnsResolveRecords, createInvalidDnsServersError, normalizeDnsServers, lookupDnsRecords, resolveDnsRecords, SecureExecResolver, SecureExecPromisesResolver, dns, createConnResetError, createAbortError2, IncomingMessage, ClientRequest, createUnsupportedHttpSocketWriteError, FakeSocket, DirectTunnelSocket, normalizeSocketChunk, Agent, debugBridgeNetwork, nextServerId, serverInstances, HTTP_METHODS, INVALID_REQUEST_PATH_REGEXP, HTTP_TOKEN_EXTRA_CHARS, createTypeErrorWithCode, createErrorWithCode, formatReceivedType, createInvalidArgTypeError2, checkIsHttpToken, checkInvalidHeaderChar, validateHeaderName, validateHeaderValue, serializeHeaderValue, joinHeaderValue, cloneStoredHeaderValue, appendNormalizedHeader, validateRequestMethod, validateRequestPath, buildHostHeader, isFlatHeaderList, normalizeRequestHeaders, hasUpgradeRequestHeaders, isRawSocketRequest, socketReadyEventNameForProtocol, isSocketReadyForProtocol, waitForSocketReadyForProtocol, buildUndiciOrigin, getUndiciClientForSocket, createHttpRequestSocket, flattenHeaderPairs, buildRawHttpHeaderPairs, serializeRawHttpRequest, readUndiciReadableBody, parseRawHttpResponse, waitForRawHttpResponseHead, waitForRawHttpResponse, hasResponseBody, splitTransferEncodingTokens, parseContentLengthHeader, parseChunkedBody, parseLoopbackRequestBuffer, serializeRawHeaderPairs, finalizeRawHeaderPairs, createBadRequestResponseBuffer, serializeLoopbackResponse, HTTP_STATUS_TEXT, isLoopbackRequestHost, ServerIncomingMessage, ServerResponseBridge, Server, ServerCallable, dispatchServerRequest, dispatchHttp2CompatibilityRequest, dispatchLoopbackServerRequest, dispatchSocketBackedServerRequest, attachHttpServerSocket, dispatchSocketRequest, upgradeSocketInstances, UpgradeSocket, dispatchUpgradeRequest, dispatchConnectRequest, onUpgradeSocketData, onUpgradeSocketEnd, ServerResponseCallable, createHttpModule, http, https, HTTP2_K_SOCKET, HTTP2_OPTIONS, http2Servers, http2Sessions, http2Streams, pendingHttp2ClientStreamEvents, scheduledHttp2ClientStreamFlushes, queuedHttp2DispatchEvents, pendingHttp2CompatRequests, scheduledHttp2DispatchDrain, nextHttp2ServerId, Http2EventEmitter, Http2SocketProxy, createHttp2ArgTypeError, createHttp2Error, createHttp2SettingRangeError, createHttp2SettingTypeError, HTTP2_INTERNAL_BINDING_CONSTANTS, HTTP2_NGHTTP2_ERROR_MESSAGES, NghttpError, nghttp2ErrorString, createHttp2InvalidArgValueError, formatHttp2InvalidValue, createHttp2PayloadForbiddenError, S_IFMT, S_IFDIR, S_IFREG, S_IFIFO, S_IFSOCK, S_IFLNK, createHttp2BridgeStat, normalizeHttp2FileResponseOptions, sliceHttp2FileBody, Http2Stream, DEFAULT_HTTP2_SETTINGS, DEFAULT_HTTP2_SESSION_STATE, cloneHttp2Settings, cloneHttp2SessionRuntimeState, parseHttp2SessionRuntimeState, validateHttp2Settings, serializeHttp2Headers, parseHttp2Headers, parseHttp2SessionState, parseHttp2SocketState, parseHttp2ErrorPayload, normalizeHttp2Headers, validateHttp2RequestOptions, validateHttp2ConnectOptions, applyHttp2SessionState, normalizeHttp2Authority, normalizeHttp2ConnectArgs, resolveHttp2SocketId, ClientHttp2Stream, getCompleteUtf8PrefixLength, ServerHttp2Stream, Http2ServerRequest, Http2ServerResponse, Http2Session, Http2Server, createHttp2Server, connectHttp2, getOrCreateHttp2Session, queuePendingHttp2ClientStreamEvent, schedulePendingHttp2ClientStreamEventsFlush, flushPendingHttp2ClientStreamEvents, http2Dispatch, scheduleQueuedHttp2DispatchDrain, onHttp2Dispatch, http2, onHttpServerRequest, Blob, File, NET_SOCKET_REGISTRY_PREFIX, NET_SERVER_HANDLE_PREFIX, registeredNetSockets, registeredNetServersByPort, getRegisteredNetSocket, registerNetSocket, unregisterNetSocket, registerNetServer, unregisterNetServer, wakeSocketBridgeReads, wakePeerBridgeReads, wakeNetServerAccept, wakeNetServerAcceptForSocket, isTruthySocketOption, normalizeKeepAliveDelay, createTimeoutArgTypeError, createFunctionArgTypeError, createTimeoutRangeError, createListenArgValueError, createSocketBadPortError, isValidTcpPort, isDecimalIntegerString, normalizeListenPortValue, normalizeListenArgs, normalizeConnectArgs, isValidIPv4Segment, isIPv4String, isValidIPv6Zone, countIPv6Parts, isIPv6String, coerceIpInput, classifyIpAddress, normalizeIpFamilyLabel, ipv4ToBigInt, expandIpv6Address, ipv6ToBigInt, ipAddressToBigInt, formatBlockListRule, BlockList, defaultAutoSelectFamily, defaultAutoSelectFamilyAttemptTimeout, SocketAddress, normalizeSocketTimeout, parseNetSocketInfo, normalizeNetSocketHandle, serializeTlsValue, isTlsSecureContextWrapper, buildSerializedTlsOptions, parseTlsState, parseTlsClientHello, createBridgedTlsError, deserializeTlsBridgeValue, queryTlsSocket, finalizeTlsUpgrade, createConnectedSocketHandle, createAcceptedClientHandle, NET_BRIDGE_TIMEOUT_SENTINEL, NET_BRIDGE_POLL_DELAY_MS, netBridgePollDelayOverrideMs, isNetBridgeTraceEnabled, normalizeNetBridgePollDelayMs, netBridgePollDelayMs, isNetRetainOwnedWriteBufferEnabled, createNetBridgeMetrics, netBridgeTraceForced, netBridgeMetrics, isNetBridgeMetricsEnabled, netBridgeNowUs, countNetBridgeMetric, maxNetBridgeMetric, countReadFirstPumpOrigin, countAcceptFirstPumpOrigin, yieldBridgeMacrotask, netSocketDispatch, NET_BRIDGE_MAX_RAW_WRITE_BYTES, NetSocket, netConnect, NetServer, NetServerCallable, netModule, createSecureContextWrapper, adoptRawTlsSocket, TLSSocket, tlsConnect, matchesServername, TLSServer, TLSServerCallable, tlsModule, DGRAM_HANDLE_PREFIX, createBadDgramSocketTypeError, createDgramAlreadyBoundError, createDgramAddressError, createDgramArgTypeError, createDgramMissingArgError, createDgramNotRunningError, getDgramErrno, createDgramSyscallError, createDgramTtlArgTypeError, createDgramBufferSizeTypeError, createDgramBufferSizeSystemError, getPlatformDgramBufferSize, normalizeDgramTtlValue, isIPv4MulticastAddress, isIPv4UnicastAddress, isIPv6MulticastAddress, validateDgramMulticastAddress, validateDgramSourceAddress, normalizeDgramSocketType, normalizeDgramSocketOptions, normalizeDgramAddressValue, normalizeDgramPortValue, createDgramMessageBuffer, createDgramMessageListBuffer, normalizeDgramBridgeResult, decodeDgramBridgeBytes, normalizeDgramBindArgs, normalizeDgramSendArgs, DgramSocket, dgramModule, isSqlitePlainObject, encodeSqliteValue, decodeSqliteValue, normalizeSqliteParams, sqliteBridgeCall, _sqliteConstants, _sqliteDatabaseOpen, _sqliteDatabaseClose, _sqliteDatabaseExec, _sqliteDatabaseQuery, _sqliteDatabasePrepare, _sqliteDatabaseLocation, _sqliteDatabaseCheckpoint, _sqliteStatementRun, _sqliteStatementGet, _sqliteStatementAll, _sqliteStatementColumns, _sqliteStatementSetReturnArrays, _sqliteStatementSetReadBigInts, _sqliteStatementSetAllowBareNamedParameters, _sqliteStatementSetAllowUnknownNamedParameters, _sqliteStatementFinalize, StatementSync, DatabaseSync, sqliteConstants, getSqliteConstants, sqliteModule, network_default };
