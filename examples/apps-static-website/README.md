# Dynamic Apps: Static Website

A directory with `index.html` and no `package.json` is served directly. CSS,
JavaScript, SVG, and other assets are included in the immutable release. A
package with a build script is treated as a built static site when it emits
`dist/index.html`.

Run the example with Node.js 22 or newer:

```sh
pnpm --dir examples/apps-static-website start
```

Open `http://localhost:3000/apps/static-website/`.
