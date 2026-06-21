# Build the Astro site. Railway (and most platforms) use the repository root as
# the Docker build context, so paths here are written relative to the repo root
# (the site lives in website/). The root .dockerignore narrows the context to
# website/ plus the @secure-exec/core SDK source (needed by the typedoc API-gen
# step), so the multi-GB monorepo is never uploaded. The shared docs theme
# (@rivet-dev/docs-theme) is fetched from GitHub during install.
FROM node:22-alpine AS build
WORKDIR /app
# git is required for pnpm to resolve the github: docs-theme dependency.
RUN apk add --no-cache git && npm install -g pnpm@9

# Install deps first (cache layer) — from the website package, not the monorepo root.
COPY website/package.json ./
RUN pnpm install

# Copy the site source.
COPY website/ ./

# The docs:gen:ts (typedoc) step in the build reads the SDK source from
# ../packages, which lives outside website/. Bring just that package's source
# into the image at the path typedoc.json points at (../packages/core), plus the
# root tsconfig that package's tsconfig extends.
COPY packages/core /packages/core
COPY tsconfig.base.json /tsconfig.base.json

RUN pnpm build

# Serve the static output. This is a static multi-page site (Astro/Starlight
# emits <path>/index.html), NOT an SPA — so resolve each page's index.html and
# serve the real 404.html on a miss (no SPA fallback to the landing page).
FROM caddy:alpine
COPY --from=build /app/dist /srv
RUN cat > /etc/caddy/Caddyfile <<'EOF'
:80 {
	root * /srv
	encode gzip
	try_files {path} {path}/index.html {path}.html
	file_server
	handle_errors {
		rewrite * /404.html
		file_server
	}
}
EOF
EXPOSE 80
