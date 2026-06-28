# Code Mode (MCP)

Give AI agents a single code-execution tool instead of individual MCP tools. The LLM writes code that chains binding calls, executed safely in Secure Exec.

Instead of calling MCP tools one at a time, Code Mode lets the LLM write JavaScript that orchestrates everything in one go, run safely in a V8 sandbox by Secure Exec.

  [MCP Toolkit](https://mcp-toolkit.nuxt.dev/advanced/code-mode) provides a premade Code Mode library powered by Secure Exec: `experimental_codeMode: true`. We recommend trying it first. The rest of this page covers how to implement Code Mode yourself.

## Why Code Mode

- **[81% less token overhead](https://x.com/hugorcd/status/2034616192225407273)**: With 50 tools, replacing per-call tool descriptions with a single code-execution tool cuts tool description tokens by 81%
- **Fewer round-trips**: Chain multiple tool calls, conditionals, and data transformations in a single execution
- **Real control flow**: Loops, branching, and `Promise.all`, not a chain of isolated tool calls
- **One structured result**: The LLM returns a single JSON value via `globalThis.__return()`, decoded on the host as `result.value`

## How it works

1. Register your host bindings on the host with `NodeRuntime.create({ bindings })`. Each becomes a named command inside the sandbox.
2. Give the LLM one tool ("execute code") and feed its generated JavaScript to `rt.run()`.
3. The generated code invokes your bindings by name. Each call round-trips out of the sandbox, runs the binding's host `handler`, and the handler's return value comes back to the guest.
4. The guest hands a single structured result back to the host with `globalThis.__return(value)`, which `rt.run()` decodes as `result.value`.

Host bindings are the heart of Code Mode. The handlers run on the host, never in the sandbox, so the guest gets controlled, named capabilities (the kind an AI agent calls as tools) without being granted the underlying access. Registering bindings auto-grants the `binding` permission scope; pass your own `permissions.binding` policy to gate individual bindings.

## Register the host bindings

Each binding has a `description`, a JSON Schema `inputSchema`, and a `handler`. The handler receives the parsed input and returns a JSON-serializable result.

## The agent's generated code

The agent then generates code like this (call it `llmGeneratedCode`). The guest calls each binding with the `callBinding(name, input)` global, which resolves with the host handler's return value. It chains three binding calls with real control flow (`Promise.all`, arithmetic, branching) in one execution, then returns a single structured result:

## Run it and read the result

Run the LLM's code in one sandboxed pass and read back the structured result:

Three tool calls, one sandbox execution, zero extra LLM round-trips. Running it prints:

```text
exitCode: 0
stdout: chained 3 binding calls in one sandbox execution
structured result: {
  "san_francisco": {
    "temp_f": 61
  },
  "tokyo": {
    "temp_f": 75
  },
  "difference": {
    "fahrenheit": 14,
    "celsius": 7.777777777777778
  },
  "warmer": "Tokyo"
}
```

## Further reading

- [Complete Code Mode implementation guide](https://mcp-toolkit.nuxt.dev/advanced/code-mode): end-to-end Code Mode walkthrough using MCP Toolkit
- [Cloudflare Code Mode blog post](https://blog.cloudflare.com/code-mode/)
- [AI Agent Code Exec](/use-cases/ai-agent-code-exec) for simpler single-tool execution patterns