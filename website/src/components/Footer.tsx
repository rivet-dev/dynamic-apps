"use client";

// Basic Secure Exec footer (dark chrome). Replaces the previously rivet-branded
// footer — uses Secure Exec's own logo, tagline, and links.
const links = [
  { name: "Documentation", href: "/docs" },
  { name: "Changelog", href: "https://github.com/rivet-dev/secure-exec/releases" },
  { name: "GitHub", href: "https://github.com/rivet-dev/secure-exec" },
  { name: "Discord", href: "https://rivet.dev/discord" },
];

export function Footer() {
  return (
    <footer className="border-t border-white/10 bg-[#09090b]">
      <div className="mx-auto flex max-w-6xl flex-col gap-8 px-6 py-12 sm:flex-row sm:items-center sm:justify-between">
        <div className="space-y-3">
          <a href="/" className="inline-block">
            <img
              src="/secure-exec-logo-long.svg"
              alt="Secure Exec"
              className="h-6 w-auto opacity-90 transition-opacity hover:opacity-100"
            />
          </a>
          <p className="text-sm text-zinc-500">
            Secure Node.js execution — no containers, no VMs.
          </p>
        </div>

        <nav className="flex flex-wrap gap-x-6 gap-y-2">
          {links.map((item) => {
            const external = item.href.startsWith("http");
            return (
              <a
                key={item.name}
                href={item.href}
                target={external ? "_blank" : undefined}
                rel={external ? "noopener noreferrer" : undefined}
                className="text-sm text-zinc-400 transition-colors hover:text-white"
              >
                {item.name}
              </a>
            );
          })}
        </nav>
      </div>

      <div className="border-t border-white/10">
        <div className="mx-auto max-w-6xl px-6 py-6">
          <p className="text-center text-xs text-zinc-600 sm:text-left">
            &copy; {new Date().getFullYear()} Rivet Gaming, Inc. Secure Exec is Apache 2.0 licensed.
          </p>
        </div>
      </div>
    </footer>
  );
}
