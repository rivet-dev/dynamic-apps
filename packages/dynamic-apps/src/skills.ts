/** Instructions and a complete starter for a buildable TypeScript HTTP app. */
export const webServerSkill = `Build a Dynamic Apps fetch handler in TypeScript. Start from this complete project and modify it for the user's request.

package.json
~~~json
{
  "name": "generated-web-app",
  "version": "0.0.0",
  "private": true,
  "type": "module",
  "main": "dist/index.js",
  "scripts": {
    "build": "tsc"
  },
  "dependencies": {
    "hono": "4.13.5"
  },
  "devDependencies": {
    "@types/node": "22.20.1",
    "typescript": "5.9.3"
  }
}
~~~

tsconfig.json
~~~json
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022", "DOM", "DOM.Iterable"],
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "outDir": "dist",
    "types": ["node"],
    "skipLibCheck": true
  },
  "include": ["src"]
}
~~~

src/index.ts
~~~ts
import { Hono } from "hono";

const app = new Hono();

app.get("/", (context) =>
  context.json({ ok: true, message: "Hello from Dynamic Apps" }),
);

export default app;
~~~

Export the fetch handler and do not open a port; Dynamic Apps owns the HTTP server. Keep the build script as "tsc" and the entrypoint as dist/index.js. The deployment runs npm run build, so invalid generated TypeScript returns compiler diagnostics that can be used to repair the files.`;

/** Instructions and a complete starter for a TypeScript app with Rivet actors. */
export const rivetActorsSkill = `Build a Dynamic Apps fetch handler with Rivet Actors. Start from this complete project and modify the actor state, actions, and HTTP routes for the user's request.

package.json
~~~json
{
  "name": "generated-rivet-actors-app",
  "version": "0.0.0",
  "private": true,
  "type": "module",
  "main": "dist/index.js",
  "scripts": {
    "build": "tsc"
  },
  "dependencies": {
    "hono": "4.13.5",
    "rivetkit": "2.3.11"
  },
  "devDependencies": {
    "@types/node": "22.20.1",
    "typescript": "5.9.3"
  }
}
~~~

tsconfig.json
~~~json
{
  "compilerOptions": {
    "target": "ES2022",
    "lib": ["ES2022", "DOM", "DOM.Iterable"],
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "strict": true,
    "outDir": "dist",
    "types": ["node"],
    "skipLibCheck": true
  },
  "include": ["src"]
}
~~~

src/index.ts
~~~ts
import { Hono } from "hono";
import { actor, event, setup } from "rivetkit";

const counter = actor({
  state: { count: 0 },
  events: { countChanged: event() },
  actions: {
    increment(context, amount: number = 1) {
      context.state.count += amount;
      context.broadcast("countChanged", context.state.count);
      return context.state.count;
    },
    getCount(context) {
      return context.state.count;
    }
  }
});

export const registry = setup({ use: { counter } });

const app = new Hono();
app.all("/api/rivet/*", (context) => registry.handler(context.req.raw));
app.get("/", (context) =>
  context.json({ ok: true, message: "Rivet Actors app is running" }),
);

export default app;
~~~

Export the fetch handler and do not call serve() or registry.start(); Dynamic Apps owns the HTTP listener. Keep the build script as "tsc" so deployment failures include TypeScript diagnostics. The example above works without reading external documentation.

Optional reference links:
- Rivet Actors: https://rivet.dev/actors/docs/
- Node.js quickstart: https://rivet.dev/actors/docs/quickstart/backend/
- Hono integration: https://rivet.dev/actors/docs/general/http-server/
- State: https://rivet.dev/actors/docs/state/
- Actions: https://rivet.dev/actors/docs/actions/
- Events: https://rivet.dev/actors/docs/events/
- SQLite: https://rivet.dev/actors/docs/sqlite/`;
