# Build the Astro site. Railway (and most platforms) use the repository root as
# the Docker build context, so paths here are written relative to the repo root
# (the site lives in website/). The root .dockerignore narrows the context to
# website/ plus the @secure-exec/core SDK source (needed by the typedoc API-gen
# step), so the multi-GB monorepo is never uploaded. The shared docs theme
# (@rivet-dev/docs-theme) is fetched from GitHub during install.
FROM node:22-alpine AS build
WORKDIR /app
RUN apk add --no-cache git && npm install -g pnpm@9

# Clone + build the shared docs theme at a pinned sha + generate the vendored
# @rivet-gg/icons dist. The icons generate needs a FontAwesome Pro token (set it
# as a Railway build variable; passed via --build-arg locally). It is only used
# in this discarded build stage — the final caddy image never contains it.
ARG DOCS_THEME_SHA=450c498555135098c6a927adfdf13458be9be22a
ARG FONTAWESOME_PACKAGE_TOKEN
# The ARG is already in this RUN's environment, so the icons generate reads
# FONTAWESOME_PACKAGE_TOKEN directly — we don't echo it into the build log.
RUN git clone https://github.com/rivet-dev/docs-theme.git /theme \
 && git -C /theme checkout "$DOCS_THEME_SHA" \
 && pnpm -C /theme install \
 && pnpm -C /theme/packages/theme/vendor/icons run gen:manifest \
 && pnpm -C /theme/packages/theme/vendor/icons run gen:vendor \
 && pnpm -C /theme/packages/theme build

# Copy the site source + the SDK source the typedoc step reads from ../packages
# + the examples the <CodeSnippet> embeds read from /examples.
COPY website/ ./
COPY packages/core /packages/core
COPY tsconfig.base.json /tsconfig.base.json
COPY examples /examples

# Point the theme deps at the clone (committed deps are local for dev) and drop
# any injected meta, then install + build.
RUN node -e "const fs=require('fs'),p=require('./package.json'),d=p.dependencies;d['@rivet-dev/docs-theme']='file:/theme/packages/theme';d['@rivet-gg/components']='file:/theme/packages/theme/vendor/components';d['@rivet-gg/icons']='file:/theme/packages/theme/vendor/icons';delete p.dependenciesMeta;fs.writeFileSync('./package.json',JSON.stringify(p,null,2))" \
 && pnpm install \
 && pnpm build

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
