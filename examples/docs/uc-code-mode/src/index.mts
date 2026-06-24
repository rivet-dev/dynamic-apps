import { NodeRuntime } from "secure-exec";

function readStringField(input: unknown, field: string): string {
  if (!input || typeof input !== "object" || Array.isArray(input)) {
    throw new TypeError("binding input must be an object");
  }

  const value = (input as Record<string, unknown>)[field];
  if (typeof value !== "string") {
    throw new TypeError(`${field} must be a string`);
  }
  return value;
}

// Code Mode: instead of giving the LLM many individual bindings, you give it one
// "execute code" tool. The LLM writes JavaScript that chains binding calls,
// branches, and transforms data - then runs it in a single sandboxed pass.
//
// The heart of Code Mode is real bindings. You register them on the host with
// create({ bindings }); each becomes a named command inside the sandbox. When the
// guest invokes a binding by name with JSON input, the call round-trips back to the
// host, runs the binding's handler, and the handler's return value is delivered
// back to the guest. The guest never sees the host filesystem, network, or any
// capability beyond the named bindings you grant it.

// Register the bindings. These handlers run on the HOST, not in the sandbox.
// In a real app each handler would hit a database, an API, or a service; here we
// keep them small and deterministic so the example is easy to follow.
const rt = await NodeRuntime.create({
  bindings: {
    "get-weather": {
      description: "Look up the current temperature for a city",
      inputSchema: {
        type: "object",
        properties: { city: { type: "string" } },
        required: ["city"],
      },
      handler: (input) => {
        const city = readStringField(input, "city");
        const table: Record<string, { temp_f: number }> = {
          "San Francisco": { temp_f: 61 },
          Tokyo: { temp_f: 75 },
        };
        return table[city] ?? { temp_f: null };
      },
    },
    calculate: {
      description: "Evaluate a simple arithmetic expression",
      inputSchema: {
        type: "object",
        properties: { expression: { type: "string" } },
        required: ["expression"],
      },
      handler: (input) => {
        const expression = readStringField(input, "expression");
        return { result: Number(eval(expression)) };
      },
    },
  },
});

// Imagine this string was written by the LLM. It chains three host binding calls
// with real control flow (Promise.all, arithmetic, branching) in one execution,
// then hands a single structured result back to the host. callBinding resolves
// with the host handler's return value.
const llmGeneratedCode = `
const [sf, tokyo] = await Promise.all([
  callBinding("get-weather", { city: "San Francisco" }),
  callBinding("get-weather", { city: "Tokyo" }),
]);

const diffF = Math.abs(sf.temp_f - tokyo.temp_f);
const diffC = await callBinding("calculate", { expression: \`\${diffF} * 5 / 9\` });

console.log("chained 3 binding calls in one sandbox execution");

globalThis.__return({
  san_francisco: sf,
  tokyo: tokyo,
  difference: { fahrenheit: diffF, celsius: diffC.result },
  warmer: sf.temp_f > tokyo.temp_f ? "San Francisco" : "Tokyo",
});
`;

interface CodeModeResult {
  san_francisco: { temp_f: number };
  tokyo: { temp_f: number };
  difference: { fahrenheit: number; celsius: number };
  warmer: string;
}

try {
  // rt.run() executes the guest code and decodes whatever it passes to
  // globalThis.__return(), while still capturing stdout/stderr/exitCode.
  const result = await rt.run<CodeModeResult>(llmGeneratedCode, {
    timeout: 5000,
  });

  console.log("exitCode:", result.exitCode);
  console.log("stdout:", result.stdout.trim());
  console.log("structured result:", JSON.stringify(result.value, null, 2));
} finally {
  await rt.dispose();
}
