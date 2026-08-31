# Dynamic Apps: SQLite

The deployed app defines a RivetKit actor backed by SQLite. The client deploys
the app, connects to its namespace, and writes and reads real database rows.

Run the example with Node.js 22 or newer:

```sh
pnpm --dir examples/apps-sqlite start
# In another terminal:
pnpm --dir examples/apps-sqlite client
```

The app's ordinary HTTP handler is available at
`http://localhost:3000/apps/sqlite-notes/`.
