//! agentOS package projection (moved into the sidecar from the agent-os clients).
//!
//! A package is a self-contained directory produced by `@rivet-dev/agentos-toolchain
//! pack`; the sidecar projects it read-only under `/opt/agentos/<name>/<version>` and
//! links its `bin/` commands into `/opt/agentos/bin` (which is on `$PATH`). A `current`
//! symlink gives an atomic version switch. The whole tree lives in ONE host staging dir
//! mounted at `/opt/agentos` — the VFS rejects cross-mount symlinks and confines host-dir
//! mounts with `RESOLVE_BENEATH`, so package content + `current` + the `bin/`/`man` farms
//! must share a single mount with only relative, in-tree symlinks. Because the host-dir
//! mount reflects host writes, appending to the staging dir adds commands to a running VM
//! live (the mechanism behind runtime `LinkPackage`).
//!
//! Package metadata lives in `agentos-package.json`: the package `name`, optional
//! `agent.acpEntrypoint`, and optional `provides` block come from that manifest. The
//! `version` still comes from the package's own root `package.json`, and commands are
//! derived from `bin/` (or `package.json` "bin").

use std::collections::HashMap;
use std::fs;
use std::os::unix::fs::{symlink, PermissionsExt};
use std::path::{Path, PathBuf};
use std::sync::{Mutex, OnceLock};

use crate::state::SidecarError;
use serde::Deserialize;

/// Root of the agentOS package tree inside the VM.
pub const OPT_AGENTOS_ROOT: &str = "/opt/agentos";
/// The symlink farm on `$PATH`.
pub const OPT_AGENTOS_BIN: &str = "/opt/agentos/bin";

/// A package to project, derived from `<dir>/agentos-package.json`.
#[derive(Debug, Clone)]
pub struct PackageDescriptor {
    pub name: String,
    pub dir: String,
    /// `bin/` command that speaks ACP, if this is an agent package.
    pub acp_entrypoint: Option<String>,
    pub provides: Option<PackageProvidesDescriptor>,
}

#[derive(Debug, Clone, Deserialize)]
pub struct PackageProvidesDescriptor {
    #[serde(default)]
    pub env: HashMap<String, String>,
    #[serde(default)]
    pub files: Vec<PackageProvidesFileDescriptor>,
}

#[derive(Debug, Clone, Deserialize)]
pub struct PackageProvidesFileDescriptor {
    pub source: String,
    pub target: String,
}

#[derive(Debug, Deserialize)]
struct AgentosPackageManifest {
    name: String,
    #[serde(default)]
    agent: Option<PackageAgentDescriptor>,
    #[serde(default)]
    provides: Option<PackageProvidesDescriptor>,
}

#[derive(Debug, Deserialize)]
struct PackageAgentDescriptor {
    #[serde(rename = "acpEntrypoint")]
    acp_entrypoint: String,
}

impl PackageDescriptor {
    fn from_manifest(dir: &str, manifest: AgentosPackageManifest) -> Result<Self, SidecarError> {
        if manifest.name.is_empty() {
            return Err(SidecarError::InvalidState(format!(
                "agentos-package.json in {dir} is missing a valid \"name\""
            )));
        }
        let acp_entrypoint = manifest.agent.map(|agent| agent.acp_entrypoint);
        if acp_entrypoint
            .as_ref()
            .is_some_and(|entry| entry.is_empty())
        {
            return Err(SidecarError::InvalidState(format!(
                "agentos-package.json in {dir} has an empty agent.acpEntrypoint"
            )));
        }
        Ok(Self {
            name: manifest.name,
            dir: dir.to_owned(),
            acp_entrypoint,
            provides: manifest.provides,
        })
    }
}

fn io_err(context: &str, error: std::io::Error) -> SidecarError {
    SidecarError::Io(format!("{context}: {error}"))
}

/// Read the sidecar-owned package manifest from `<dir>/agentos-package.json`.
pub fn read_package_manifest(dir: &str) -> Result<PackageDescriptor, SidecarError> {
    let path = Path::new(dir).join("agentos-package.json");
    if !path.exists() {
        return Err(SidecarError::InvalidState(format!(
            "missing required agentos-package.json in package dir {dir}"
        )));
    }
    let text = fs::read_to_string(&path).map_err(|e| io_err("read agentos-package.json", e))?;
    let manifest: AgentosPackageManifest = serde_json::from_str(&text).map_err(|e| {
        SidecarError::InvalidState(format!("invalid agentos-package.json in {dir}: {e}"))
    })?;
    PackageDescriptor::from_manifest(dir, manifest)
}

/// Read the package's `version` from its root `package.json`. A toolchain-produced
/// package (flat or `--bundle`) always has a root `package.json {name,version,bin}`.
pub fn read_package_version(dir: &str) -> Result<String, SidecarError> {
    let path = Path::new(dir).join("package.json");
    if !path.exists() {
        return Err(SidecarError::InvalidState(format!(
            "missing required package.json in {dir} \
             (produce packages with '@rivet-dev/agentos-toolchain pack')"
        )));
    }
    let text = fs::read_to_string(&path).map_err(|e| io_err("read package.json", e))?;
    let value: serde_json::Value = serde_json::from_str(&text)
        .map_err(|e| SidecarError::InvalidState(format!("invalid package.json in {dir}: {e}")))?;
    match value.get("version").and_then(|v| v.as_str()) {
        Some(version) if !version.is_empty() => Ok(version.to_owned()),
        _ => Err(SidecarError::InvalidState(format!(
            "package.json in {dir} is missing a valid \"version\""
        ))),
    }
}

/// Map each command name to its entry path RELATIVE to the package root.
///
/// A shipped package is an npm dependency, so it must not rely on `bin/` symlinks
/// (npm publish + cross-platform tooling strip/break them). Commands are therefore
/// declared in the root `package.json` "bin" map (command → real entry file). The
/// `/opt/agentos/bin/<cmd>` symlink farm lives ONLY in the sidecar's host staging
/// dir and points at that entry. WASM packages instead ship a real `bin/` of
/// `.wasm` files, so fall back to the `bin/` directory when there is no
/// `package.json` "bin".
fn command_targets(dir: &str) -> Result<Vec<(String, String)>, SidecarError> {
    let pkg_json = Path::new(dir).join("package.json");
    if pkg_json.exists() {
        if let Ok(text) = fs::read_to_string(&pkg_json) {
            if let Ok(value) = serde_json::from_str::<serde_json::Value>(&text) {
                match value.get("bin") {
                    Some(serde_json::Value::String(path)) => {
                        if let Some(name) = value.get("name").and_then(|v| v.as_str()) {
                            let unscoped = name.rsplit('/').next().unwrap_or(name).to_owned();
                            return Ok(vec![(unscoped, normalize_rel(path))]);
                        }
                    }
                    Some(serde_json::Value::Object(map)) => {
                        let mut targets: Vec<(String, String)> = map
                            .iter()
                            .filter_map(|(name, path)| {
                                path.as_str()
                                    .map(|path| (name.clone(), normalize_rel(path)))
                            })
                            .collect();
                        targets.sort_by(|a, b| a.0.cmp(&b.0));
                        return Ok(targets);
                    }
                    _ => {}
                }
            }
        }
    }

    let bin = Path::new(dir).join("bin");
    if bin.is_dir() {
        let mut targets = Vec::new();
        for entry in fs::read_dir(&bin).map_err(|e| io_err("read bin/", e))? {
            let entry = entry.map_err(|e| io_err("read bin/ entry", e))?;
            if let Some(name) = entry.file_name().to_str() {
                targets.push((name.to_owned(), format!("bin/{name}")));
            }
        }
        targets.sort_by(|a, b| a.0.cmp(&b.0));
        return Ok(targets);
    }
    Ok(Vec::new())
}

/// Strip a leading `./` so the resulting path is a clean in-package relative path.
fn normalize_rel(path: &str) -> String {
    path.strip_prefix("./").unwrap_or(path).to_owned()
}

/// Derive command names for the package (sorted). See [`command_targets`].
pub fn derive_commands(dir: &str) -> Result<Vec<String>, SidecarError> {
    Ok(command_targets(dir)?
        .into_iter()
        .map(|(name, _)| name)
        .collect())
}

/// Process-global shared-projection cache (Phase 5). Maps `<name>@<version>` to a host dir
/// holding ONE copy of that package's content; every VM's projection hardlinks from it
/// instead of re-copying. Keyed by name+version, so a version bump produces a fresh cache
/// entry (invalidation on version change).
fn projection_cache() -> &'static Mutex<HashMap<String, PathBuf>> {
    static CACHE: OnceLock<Mutex<HashMap<String, PathBuf>>> = OnceLock::new();
    CACHE.get_or_init(|| Mutex::new(HashMap::new()))
}

/// Copy a package's content to the shared cache once; return the cache dir.
fn cached_package_content(
    desc_dir: &str,
    name: &str,
    version: &str,
) -> Result<PathBuf, SidecarError> {
    let key = format!("{name}@{version}");
    {
        let cache = projection_cache()
            .lock()
            .expect("projection cache poisoned");
        if let Some(existing) = cache.get(&key) {
            if existing.exists() {
                return Ok(existing.clone());
            }
        }
    }
    let dir = std::env::temp_dir().join(format!(
        "agentos-pkgcache-{}-{}",
        sanitize(name),
        sanitize(version)
    ));
    let content = dir.join("content");
    if content.exists() {
        let _ = fs::remove_dir_all(&content);
    }
    copy_tree_verbatim(Path::new(desc_dir), &content)?;
    projection_cache()
        .lock()
        .expect("projection cache poisoned")
        .insert(key, content.clone());
    Ok(content)
}

fn sanitize(s: &str) -> String {
    s.chars()
        .map(|c| if c.is_ascii_alphanumeric() { c } else { '_' })
        .collect()
}

/// Recursively materialize `src` into `dst`, HARDLINKING regular files (shared inodes — no
/// data copy) and recreating symlinks/dirs. Falls back to a byte copy if hardlinking fails
/// (e.g. a cross-filesystem `EXDEV`).
fn hardlink_tree_from(src: &Path, dst: &Path) -> Result<(), SidecarError> {
    let meta = fs::symlink_metadata(src).map_err(|e| io_err("stat cache source", e))?;
    if meta.file_type().is_symlink() {
        let target = fs::read_link(src).map_err(|e| io_err("read_link", e))?;
        symlink(&target, dst).map_err(|e| io_err("symlink copy", e))?;
        return Ok(());
    }
    if meta.is_dir() {
        fs::create_dir_all(dst).map_err(|e| io_err("create_dir", e))?;
        for entry in fs::read_dir(src).map_err(|e| io_err("read_dir", e))? {
            let entry = entry.map_err(|e| io_err("read_dir entry", e))?;
            hardlink_tree_from(&entry.path(), &dst.join(entry.file_name()))?;
        }
        return Ok(());
    }
    if fs::hard_link(src, dst).is_err() {
        fs::copy(src, dst).map_err(|e| io_err("copy file", e))?;
    }
    Ok(())
}

/// Recursively copy `src` into `dst`, preserving symlinks verbatim (so relative in-package
/// links stay in-tree). Mirrors TS `cpSync({verbatimSymlinks:true})`.
fn copy_tree_verbatim(src: &Path, dst: &Path) -> Result<(), SidecarError> {
    let meta = fs::symlink_metadata(src).map_err(|e| io_err("stat source", e))?;
    if meta.file_type().is_symlink() {
        let target = fs::read_link(src).map_err(|e| io_err("read_link", e))?;
        symlink(&target, dst).map_err(|e| io_err("symlink copy", e))?;
        return Ok(());
    }
    if meta.is_dir() {
        fs::create_dir_all(dst).map_err(|e| io_err("create_dir", e))?;
        for entry in fs::read_dir(src).map_err(|e| io_err("read_dir", e))? {
            let entry = entry.map_err(|e| io_err("read_dir entry", e))?;
            copy_tree_verbatim(&entry.path(), &dst.join(entry.file_name()))?;
        }
        return Ok(());
    }
    fs::copy(src, dst).map_err(|e| io_err("copy file", e))?;
    Ok(())
}

/// Ensure the staging dir has a `bin/` so `/opt/agentos/bin` is a real (possibly empty)
/// directory on `$PATH`. Call once before any `link_package`.
pub fn init_projection(staging_root: &Path) -> Result<(), SidecarError> {
    fs::create_dir_all(staging_root.join("bin")).map_err(|e| io_err("init projection bin/", e))
}

/// Add one package to the `/opt/agentos` staging dir. Returns the command names it linked.
/// Idempotent per command name (errors on a duplicate).
pub fn link_package(
    desc: &PackageDescriptor,
    staging_root: &Path,
) -> Result<Vec<String>, SidecarError> {
    let name = desc.name.clone();
    let version = read_package_version(&desc.dir)?;
    let targets = command_targets(&desc.dir)?;
    let commands: Vec<String> = targets.iter().map(|(name, _)| name.clone()).collect();
    if let Some(acp) = &desc.acp_entrypoint {
        if !commands.contains(acp) {
            return Err(SidecarError::InvalidState(format!(
                "agent acpEntrypoint {acp:?} is not one of {name}'s commands"
            )));
        }
    }

    let bin_dir = staging_root.join("bin");
    fs::create_dir_all(&bin_dir).map_err(|e| io_err("create bin/", e))?;
    let name_dir = staging_root.join(&name);
    let version_dir = name_dir.join(&version);
    // Two meta-packages can both pull in the same sub-package (e.g. `build-essential` and
    // `common` both include `coreutils`). Projecting an already-projected `<name>/<version>`
    // is an idempotent no-op, not a conflict — its content + `bin/`/`man` links are already in
    // the staging dir. (A *different* package re-providing a command still errors at the
    // bin-link step below, which is the real duplicate-command case.)
    if version_dir.exists() {
        return Ok(commands);
    }
    // Hardlink content from a process-global cache (Phase 5: shared cross-VM projection) so
    // a package is copied to disk ONCE and shared (same inodes) across every VM's read-only
    // projection. Falls back to a copy across filesystems.
    let cached = cached_package_content(&desc.dir, &name, &version)?;
    hardlink_tree_from(&cached, &version_dir)?;

    // Toolchain-packed command files (npm `bin` scripts AND WASM `bin/*.wasm`) ship as
    // plain `0644` data inside the npm tarball — npm never preserves an execute bit. The
    // kernel's `$PATH` walk and exec(2) both require the execute bits (`0o111`), so a
    // `0644` command would resolve to ENOENT (bare name skipped as non-executable) or
    // EACCES (absolute path). Mark every projected command entry executable so the
    // `/opt/agentos/bin` symlink farm points at runnable files. The entries are hardlinks
    // into the shared cache, so this is idempotent across VMs (and a no-op on re-projection
    // because `version_dir.exists()` short-circuits above).
    for (_, entry) in &targets {
        let entry_path = version_dir.join(entry);
        if let Ok(meta) = fs::metadata(&entry_path) {
            let mut perms = meta.permissions();
            let mode = perms.mode();
            perms.set_mode(mode | 0o111);
            fs::set_permissions(&entry_path, perms)
                .map_err(|e| io_err("chmod +x command entry", e))?;
        }
    }

    // <name>/current -> <version>
    let current = name_dir.join("current");
    let _ = fs::remove_file(&current);
    symlink(&version, &current).map_err(|e| io_err("current symlink", e))?;

    // bin/<cmd> -> ../<name>/current/<entry> (the entry from package.json "bin", or
    // bin/<cmd> for WASM packages). The symlink farm exists only in the staging dir.
    for (cmd, entry) in &targets {
        let dest = bin_dir.join(cmd);
        if dest.exists() {
            return Err(SidecarError::InvalidState(format!(
                "command {cmd:?} is already provided by another package"
            )));
        }
        symlink(format!("../{name}/current/{entry}"), &dest)
            .map_err(|e| io_err("bin symlink", e))?;
    }

    // share/man/<section>/* -> ../../../<name>/current/share/man/<section>/*
    let man = version_dir.join("share").join("man");
    if man.is_dir() {
        for section in fs::read_dir(&man).map_err(|e| io_err("read man/", e))? {
            let section = section.map_err(|e| io_err("man section", e))?;
            if !section.path().is_dir() {
                continue;
            }
            let sec_name = section.file_name();
            let farm = staging_root.join("share").join("man").join(&sec_name);
            fs::create_dir_all(&farm).map_err(|e| io_err("man farm dir", e))?;
            for page in fs::read_dir(section.path()).map_err(|e| io_err("man pages", e))? {
                let page = page.map_err(|e| io_err("man page", e))?;
                let page_name = page.file_name();
                let target = format!(
                    "../../../{name}/current/share/man/{}/{}",
                    sec_name.to_string_lossy(),
                    page_name.to_string_lossy()
                );
                symlink(target, farm.join(&page_name)).map_err(|e| io_err("man symlink", e))?;
            }
        }
    }

    Ok(commands)
}
