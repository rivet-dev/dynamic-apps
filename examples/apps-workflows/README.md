# Dynamic Apps: Workflows

The deployed app defines a keyed job actor whose durable workflow starts when
the actor is created, sleeps, and resumes before marking the job complete.

Run the example with Node.js 22 or newer:

```sh
pnpm --dir examples/apps-workflows start
# In another terminal:
pnpm --dir examples/apps-workflows client
```

The app's ordinary HTTP handler is available at
`http://localhost:3000/apps/durable-workflow/`.
