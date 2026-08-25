FROM node:24-bookworm-slim

RUN apt-get update \
	&& apt-get install -y --no-install-recommends ca-certificates libssl3 \
	&& rm -rf /var/lib/apt/lists/* \
	&& corepack enable

WORKDIR /app

COPY package.json pnpm-lock.yaml pnpm-workspace.yaml ./
COPY packages/dynamic-apps/package.json packages/dynamic-apps/package.json
COPY packages/dynamic-apps-builder/package.json packages/dynamic-apps-builder/package.json
COPY benchmarks/dynamic-apps/package.json benchmarks/dynamic-apps/package.json
COPY examples/apps-ai-builder/package.json examples/apps-ai-builder/package.json
COPY examples/apps-hello-world/package.json examples/apps-hello-world/package.json
COPY examples/apps-multiplayer/package.json examples/apps-multiplayer/package.json
COPY examples/apps-sqlite/package.json examples/apps-sqlite/package.json
COPY examples/apps-static-website/package.json examples/apps-static-website/package.json
COPY examples/apps-workflows/package.json examples/apps-workflows/package.json
COPY tests/e2e/dynamic-apps/package.json tests/e2e/dynamic-apps/package.json
RUN pnpm install --frozen-lockfile

COPY . .
RUN pnpm build

EXPOSE 3000

CMD ["node", "--import", "tsx", "benchmarks/dynamic-apps/src/server.ts", "--host", "0.0.0.0"]
