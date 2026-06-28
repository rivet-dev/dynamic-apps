//! Unit tests for the sidecar package projection (moved here from the agent-os
//! Rust client's `package_projection_test.rs`). These assert the on-disk
//! `/opt/agentos` layout, the shared-inode content cache, and version-keyed cache
//! invalidation directly against the projection module — no VM required.

use std::fs;
use std::os::unix::fs::MetadataExt;
use std::path::{Path, PathBuf};

use secure_exec_sidecar::package_projection::{
    derive_commands, init_projection, link_package, read_package_manifest, read_package_version,
    PackageDescriptor,
};

fn unique_dir(tag: &str) -> PathBuf {
    let nonce = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_nanos();
    let dir = std::env::temp_dir().join(format!("agentos-projtest-{tag}-{nonce}"));
    fs::create_dir_all(&dir).unwrap();
    dir
}

/// Write a minimal toolchain-style package dir: root `package.json {name,version}`
/// plus a `bin/` with the given executable command files.
fn write_package(root: &Path, name: &str, version: &str, commands: &[&str]) {
    fs::create_dir_all(root.join("bin")).unwrap();
    fs::write(
        root.join("package.json"),
        format!("{{\"name\":\"{name}\",\"version\":\"{version}\"}}"),
    )
    .unwrap();
    fs::write(
        root.join("agentos-package.json"),
        format!("{{\"name\":\"{name}\"}}"),
    )
    .unwrap();
    for cmd in commands {
        fs::write(
            root.join("bin").join(cmd),
            format!("#!/usr/bin/env node\n// {cmd}\n"),
        )
        .unwrap();
    }
}

fn manifest_descriptor(root: &Path) -> PackageDescriptor {
    read_package_manifest(root.to_str().unwrap()).unwrap()
}

#[test]
fn reads_version_from_package_json_and_errors_when_missing() {
    let pkg = unique_dir("ver");
    write_package(&pkg, "vt", "3.1.4", &["vt"]);
    assert_eq!(
        read_package_version(pkg.to_str().unwrap()).unwrap(),
        "3.1.4"
    );

    let empty = unique_dir("ver-missing");
    assert!(read_package_version(empty.to_str().unwrap()).is_err());
}

#[test]
fn reads_name_agent_and_provides_from_agentos_package_json() {
    let pkg = unique_dir("manifest");
    write_package(&pkg, "package-json-name", "1.0.0", &["agent-cmd"]);
    fs::create_dir_all(pkg.join("share/config")).unwrap();
    fs::write(
        pkg.join("agentos-package.json"),
        r#"{
          "name": "manifest-name",
          "agent": { "acpEntrypoint": "agent-cmd" },
          "provides": {
            "env": { "FROM_MANIFEST": "yes" },
            "files": [{ "source": "share/config", "target": "/etc/manifest" }]
          }
        }"#,
    )
    .unwrap();

    let descriptor = read_package_manifest(pkg.to_str().unwrap()).unwrap();
    assert_eq!(descriptor.name, "manifest-name");
    assert_eq!(descriptor.acp_entrypoint.as_deref(), Some("agent-cmd"));
    let provides = descriptor.provides.as_ref().expect("provides");
    assert_eq!(
        provides.env.get("FROM_MANIFEST").map(String::as_str),
        Some("yes")
    );
    assert_eq!(provides.files[0].target, "/etc/manifest");
}

#[test]
fn missing_agentos_package_json_is_fatal() {
    let pkg = unique_dir("manifest-missing");
    fs::write(
        pkg.join("package.json"),
        r#"{"name":"missing-manifest","version":"1.0.0"}"#,
    )
    .unwrap();
    let err = read_package_manifest(pkg.to_str().unwrap()).unwrap_err();
    assert!(
        err.to_string()
            .contains("missing required agentos-package.json"),
        "{err}"
    );
    assert!(err.to_string().contains(pkg.to_str().unwrap()), "{err}");
}

#[test]
fn derives_commands_from_bin_dir() {
    let pkg = unique_dir("cmds");
    write_package(&pkg, "tool", "1.0.0", &["foo", "bar"]);
    let mut commands = derive_commands(pkg.to_str().unwrap()).unwrap();
    commands.sort();
    assert_eq!(commands, vec!["bar".to_string(), "foo".to_string()]);
}

#[test]
fn projects_bin_current_and_version_dir() {
    let pkg = unique_dir("layout-src");
    write_package(&pkg, "demo", "2.0.0", &["demo"]);

    let staging = unique_dir("layout-staging");
    init_projection(&staging).unwrap();
    let commands = link_package(&manifest_descriptor(&pkg), &staging).unwrap();
    assert_eq!(commands, vec!["demo".to_string()]);

    // bin/<cmd> is a relative in-tree symlink through current.
    let bin_link = staging.join("bin").join("demo");
    assert_eq!(
        fs::read_link(&bin_link).unwrap(),
        Path::new("../demo/current/bin/demo"),
    );
    // <name>/current -> <version>
    assert_eq!(
        fs::read_link(staging.join("demo").join("current")).unwrap(),
        Path::new("2.0.0"),
    );
    // package content lives under <name>/<version>/
    assert!(staging
        .join("demo")
        .join("2.0.0")
        .join("bin")
        .join("demo")
        .exists());
}

#[test]
fn reprojecting_same_package_is_idempotent() {
    // Two meta-packages can both pull in the same sub-package (e.g. build-essential AND
    // common both include coreutils). Projecting the same name@version twice into one
    // staging dir must be a no-op, NOT an EEXIST "symlink copy" conflict.
    let pkg = unique_dir("idem-src");
    write_package(&pkg, "coreutils", "9.5.0", &["ls", "cat"]);
    let desc = manifest_descriptor(&pkg);
    let staging = unique_dir("idem-staging");
    init_projection(&staging).unwrap();

    let first = link_package(&desc, &staging).unwrap();
    let second = link_package(&desc, &staging).unwrap(); // must not error
    assert_eq!(first, second);
    assert!(staging.join("bin").join("ls").exists());
    assert!(staging
        .join("coreutils")
        .join("9.5.0")
        .join("bin")
        .join("ls")
        .exists());
}

#[test]
fn projects_from_package_json_bin_without_bin_symlinks() {
    // An npm-shippable package: commands declared in package.json "bin" pointing at
    // real entry files; NO bin/ symlink dir in the package.
    let pkg = unique_dir("pjbin-src");
    fs::create_dir_all(pkg.join("dist")).unwrap();
    fs::write(
        pkg.join("package.json"),
        "{\"name\":\"@scope/tool\",\"version\":\"4.5.6\",\"bin\":{\"mytool\":\"./dist/cli.js\"}}",
    )
    .unwrap();
    fs::write(
        pkg.join("agentos-package.json"),
        r#"{"name":"tool","agent":{"acpEntrypoint":"mytool"}}"#,
    )
    .unwrap();
    fs::write(pkg.join("dist/cli.js"), "#!/usr/bin/env node\n").unwrap();

    let staging = unique_dir("pjbin-staging");
    init_projection(&staging).unwrap();
    let commands = link_package(&manifest_descriptor(&pkg), &staging).unwrap();
    assert_eq!(commands, vec!["mytool".to_string()]);
    // /opt/agentos/bin/mytool -> ../<name>/current/dist/cli.js (the real entry).
    assert_eq!(
        fs::read_link(staging.join("bin").join("mytool")).unwrap(),
        Path::new("../tool/current/dist/cli.js"),
    );
}

#[test]
fn hardlinks_content_so_two_projections_share_inodes() {
    let pkg = unique_dir("share-src");
    write_package(&pkg, "shared", "9.9.9", &["shared"]);
    let desc = manifest_descriptor(&pkg);

    let staging_a = unique_dir("share-a");
    let staging_b = unique_dir("share-b");
    init_projection(&staging_a).unwrap();
    init_projection(&staging_b).unwrap();
    link_package(&desc, &staging_a).unwrap();
    link_package(&desc, &staging_b).unwrap();

    let a = fs::metadata(staging_a.join("shared/9.9.9/bin/shared")).unwrap();
    let b = fs::metadata(staging_b.join("shared/9.9.9/bin/shared")).unwrap();
    assert_eq!(
        a.ino(),
        b.ino(),
        "cross-projection content should share an inode"
    );
    assert!(a.nlink() >= 2, "hardlinked content should have nlink >= 2");
}

#[test]
fn invalidates_cache_on_version_change() {
    let pkg_v1 = unique_dir("inval-v1");
    write_package(&pkg_v1, "p", "1.0.0", &["p"]);
    let pkg_v2 = unique_dir("inval-v2");
    write_package(&pkg_v2, "p", "2.0.0", &["p"]);

    let staging_a = unique_dir("inval-a");
    let staging_b = unique_dir("inval-b");
    init_projection(&staging_a).unwrap();
    init_projection(&staging_b).unwrap();
    link_package(&manifest_descriptor(&pkg_v1), &staging_a).unwrap();
    link_package(&manifest_descriptor(&pkg_v2), &staging_b).unwrap();

    let a = fs::metadata(staging_a.join("p/1.0.0/bin/p")).unwrap();
    let b = fs::metadata(staging_b.join("p/2.0.0/bin/p")).unwrap();
    assert_ne!(
        a.ino(),
        b.ino(),
        "a version bump must produce a fresh cache entry"
    );
}

#[test]
fn rejects_duplicate_command_across_packages() {
    let pkg_a = unique_dir("dup-a");
    write_package(&pkg_a, "a", "1.0.0", &["clash"]);
    let pkg_b = unique_dir("dup-b");
    write_package(&pkg_b, "b", "1.0.0", &["clash"]);

    let staging = unique_dir("dup-staging");
    init_projection(&staging).unwrap();
    link_package(&manifest_descriptor(&pkg_a), &staging).unwrap();
    let err = link_package(&manifest_descriptor(&pkg_b), &staging);
    assert!(err.is_err(), "duplicate command must be rejected");
}

#[test]
fn rejects_unknown_acp_entrypoint() {
    let pkg = unique_dir("acp-src");
    write_package(&pkg, "agentpkg", "1.0.0", &["real-cmd"]);
    fs::write(
        pkg.join("agentos-package.json"),
        r#"{"name":"agentpkg","agent":{"acpEntrypoint":"does-not-exist"}}"#,
    )
    .unwrap();

    let staging = unique_dir("acp-staging");
    init_projection(&staging).unwrap();
    let err = link_package(&manifest_descriptor(&pkg), &staging);
    assert!(
        err.is_err(),
        "acpEntrypoint not in commands must be rejected"
    );
}
