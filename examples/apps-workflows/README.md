# Dynamic Apps: Workflows

The deployed app defines an `order` actor with
[`@rivet-dev/workflows`](https://rivet.dev/workflows/docs/). Its durable
workflow starts when the actor is created, charges and ships the order, sleeps
while it is in transit, and resumes to mark it delivered. The client deploys the
app as `order-workflow` and polls the order's status until it is delivered.

Run the example with Node.js 22 or newer:

```sh
pnpm --dir examples/apps-workflows start
# In another terminal:
pnpm --dir examples/apps-workflows client
```

The app's ordinary HTTP handler is available at
`http://localhost:3000/apps/order-workflow/`.
