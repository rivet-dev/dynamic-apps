import { NodeRuntime } from "secure-exec";

// Code Mode: instead of giving the LLM many individual tools, you give it one
// "execute code" tool. The LLM writes JavaScript that chains tool calls,
// branches, and transforms data - then runs it in a single sandboxed pass.
//
// The heart of Code Mode is real host tools. You register them on the host with
// create({ tools }); each becomes a named command inside the sandbox. When the
// guest invokes a tool by name with JSON input, the call round-trips back to the
// host, runs the tool's handler, and the handler's return value is delivered
// back to the guest. The guest never sees the host filesystem, network, or any
// capability beyond the named tools you grant it.

// Register the host tools. These handlers run on the HOST, not in the sandbox.
// In a real app each handler would hit a database, an API, or a service; here we
// keep them small and deterministic so the example is easy to follow.
const rt = await NodeRuntime.create({
  tools: {
    "get-weather": {
      description: "Look up the current temperature for a city",
      inputSchema: {
        type: "object",
        properties: { city: { type: "string" } },
        required: ["city"],
      },
			handler: (input: unknown) => {
				const { city } = input as { city: string };
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
			handler: (input: unknown) => {
				const { expression } = input as { expression: string };
				return { result: Number(eval(expression)) };
			},
    },
  },
});

// Imagine this string was written by the LLM. It chains three host-tool calls
// with real control flow (Promise.all, arithmetic, branching) in one execution,
// then hands a single structured result back to the host.
//
// Each tool is a guest command: the guest invokes it by name with
// `--json <input>` and reads the host handler's JSON result back from stdout.
const llmGeneratedCode = `
import { execFileSync } from "node:child_process";

function callTool(name, input) {
  const out = execFileSync(name, [name, "--json", JSON.stringify(input)]);
  // The host replies with { ok, result }; unwrap the handler's return value.
  const reply = JSON.parse(out.toString());
  if (!reply.ok) throw new Error(reply.error ?? "tool failed");
  return reply.result;
}

const [sf, tokyo] = await Promise.all([
  callTool("get-weather", { city: "San Francisco" }),
  callTool("get-weather", { city: "Tokyo" }),
]);

const diffF = Math.abs(sf.temp_f - tokyo.temp_f);
const diffC = callTool("calculate", { expression: \`\${diffF} * 5 / 9\` });

console.log("chained 3 tool calls in one sandbox execution");

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
