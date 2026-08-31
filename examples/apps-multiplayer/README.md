# Dynamic Apps: Multiplayer

The deployed app defines one keyed room actor. Clients share player positions
and receive realtime events whenever a player joins or moves.

Run the example with Node.js 22 or newer:

```sh
pnpm --dir examples/apps-multiplayer start
# In another terminal:
pnpm --dir examples/apps-multiplayer client
```

The app's ordinary HTTP handler is available at
`http://localhost:3000/apps/multiplayer-room/`.
