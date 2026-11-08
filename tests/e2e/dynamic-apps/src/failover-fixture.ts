export const FAILOVER_APP_ID = "dynamic-apps-failover-e2e";

export function failoverFixtureFiles(): Record<string, string> {
	return {
		"package.json": JSON.stringify({
			name: "dynamic-apps-failover-e2e-fixture",
			version: "1.0.0",
			private: true,
			type: "module",
			main: "index.js",
			dependencies: { hono: "4.13.5", rivetkit: "3.0.2" },
		}),
		"index.js": `
import { Hono } from "hono";
import { actor, setup } from "rivetkit";

const counter = actor({
  state: { count: 0 },
  actions: {
    async add(c, amount = 1) {
      c.state.count += amount;
      // The test kills this host right after the call, so persist before replying.
      await c.saveState({ immediate: true });
      return c.state.count;
    },
    inspect(c) {
      return c.state.count;
    },
  },
});

export const registry = setup({ use: { counter } });
const app = new Hono();
app.all("*", () => Response.json({ ok: true }));
export default app;
`,
	};
}
