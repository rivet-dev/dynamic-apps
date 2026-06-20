mod support;

#[allow(dead_code)]
mod s3 {
    include!("../src/plugins/s3.rs");

    mod tests {
        use super::test_support::MockS3Server;
        use super::*;

        fn test_config(server: &MockS3Server, prefix: &str) -> S3MountConfig {
            S3MountConfig {
                bucket: String::from("test-bucket"),
                prefix: Some(prefix.to_owned()),
                region: Some(String::from(DEFAULT_REGION)),
                credentials: Some(S3MountCredentials {
                    access_key_id: String::from("minioadmin"),
                    secret_access_key: String::from("minioadmin"),
                }),
                endpoint: Some(server.base_url().to_owned()),
                chunk_size: Some(8),
                inline_threshold: Some(4),
            }
        }

        #[test]
        fn s3_plugin_validates_endpoint_well_formedness_only() {
            // The endpoint is trusted mount config, not an SSRF surface, so it is
            // no longer checked against an IP denylist; only well-formedness is
            // validated. Private/metadata/loopback hosts are accepted.
            for ok in [
                "https://s3.example.com",
                "http://127.0.0.1:9000",
                "https://169.254.169.254/latest",
                "https://[2001:db8::1]",
            ] {
                validate_s3_endpoint(ok)
                    .unwrap_or_else(|error| panic!("well-formed endpoint {ok} failed: {error}"));
            }
            for bad in ["", "not a url", "ftp://example.com"] {
                let error = validate_s3_endpoint(bad)
                    .expect_err(&format!("malformed endpoint {bad:?} should fail"));
                assert_eq!(error.code(), "EINVAL", "unexpected error for {bad:?}");
            }
        }

        #[test]
        fn s3_plugin_rejects_oversized_inline_manifest_data_before_decode() {
            let error = validate_inline_manifest_data_size_with_limit("YWJjZGVm", "s3", 2, 5)
                .expect_err("oversized inline payload should fail");
            assert_eq!(error.code(), "EINVAL");
            assert!(
                error
                    .message()
                    .contains("may decode to 6 bytes, limit is 5"),
                "unexpected error: {}",
                error.message()
            );
        }

        #[test]
        fn s3_plugin_rejects_oversized_persisted_manifest_before_upload() {
            let error =
                validate_persisted_manifest_size(6, 5).expect_err("oversized manifest should fail");
            assert!(
                error
                    .to_string()
                    .contains("s3 manifest is 6 bytes, limit is 5"),
                "unexpected error: {error}"
            );
        }

        #[test]
        fn s3_plugin_rejects_oversized_persisted_file_entries_before_upload() {
            let error = validate_persisted_manifest_file_size_with_limit(6, "s3", 2, 5)
                .expect_err("oversized persisted file should fail");
            assert!(
                error
                    .to_string()
                    .contains("s3 manifest inode 2 has 6 bytes, limit is 5"),
                "unexpected error: {error}"
            );
        }

        #[test]
        fn s3_plugin_rejects_streaming_object_bodies_above_limit() {
            let runtime = Runtime::new().expect("create test runtime");
            let error = runtime
                .block_on(collect_s3_body_limited(
                    ByteStream::from(b"too large".to_vec()),
                    "streaming-object",
                    1,
                ))
                .expect_err("oversized streaming body should fail");
            assert!(
                error
                    .to_string()
                    .contains("s3 object 'streaming-object' exceeded 1 byte limit"),
                "unexpected error: {error}"
            );
        }

        #[test]
        fn s3_plugin_rejects_object_loads_above_requested_limit() {
            let server = MockS3Server::start();
            let filesystem =
                S3BackedFilesystem::from_config(test_config(&server, "limited-object"))
                    .expect("open s3 fs");
            server.put_object("test-bucket/limited-object/blob", b"too large".to_vec());

            let error = filesystem
                .store
                .load_bytes_limited("limited-object/blob", 1)
                .expect_err("oversized object load should fail");
            assert!(
                error.to_string().contains("limit is 1"),
                "unexpected error: {error}"
            );
        }

        #[test]
        fn s3_plugin_persists_files_across_reopen_and_preserves_links() {
            let server = MockS3Server::start();

            let mut filesystem = S3BackedFilesystem::from_config(test_config(&server, "persist"))
                .expect("open s3 fs");
            filesystem
                .write_file("/workspace/original.txt", b"hello world".to_vec())
                .expect("write original");
            filesystem
                .link("/workspace/original.txt", "/workspace/linked.txt")
                .expect("link file");
            filesystem
                .symlink("/workspace/original.txt", "/workspace/alias.txt")
                .expect("symlink file");
            filesystem.shutdown().expect("flush s3 fs");

            let mut reopened = S3BackedFilesystem::from_config(test_config(&server, "persist"))
                .expect("reopen s3 fs");

            assert_eq!(
                reopened
                    .read_file("/workspace/original.txt")
                    .expect("read reopened original"),
                b"hello world".to_vec()
            );
            assert_eq!(
                reopened
                    .read_file("/workspace/linked.txt")
                    .expect("read reopened hard link"),
                b"hello world".to_vec()
            );
            assert_eq!(
                reopened
                    .read_file("/workspace/alias.txt")
                    .expect("read reopened symlink"),
                b"hello world".to_vec()
            );
            assert_eq!(
                reopened
                    .stat("/workspace/original.txt")
                    .expect("stat reopened file")
                    .nlink,
                2
            );

            let chunk_keys = server
                .object_keys()
                .into_iter()
                .filter(|key| key.contains("/blocks/"))
                .collect::<Vec<_>>();
            assert!(
                chunk_keys.len() >= 2,
                "expected chunked storage to create multiple block objects"
            );
        }

        #[test]
        fn s3_plugin_cleans_up_stale_chunk_objects_after_truncate() {
            let server = MockS3Server::start();

            let mut filesystem = S3BackedFilesystem::from_config(test_config(&server, "truncate"))
                .expect("open s3 fs");
            filesystem
                .write_file("/large.txt", b"abcdefghijk".to_vec())
                .expect("write large file");
            filesystem.shutdown().expect("flush initial file");

            let before = server
                .object_keys()
                .into_iter()
                .filter(|key| key.contains("/blocks/"))
                .collect::<Vec<_>>();
            assert!(
                before.len() >= 2,
                "expected multiple blocks before truncation"
            );

            filesystem
                .truncate("/large.txt", 1)
                .expect("truncate to inline size");
            filesystem.shutdown().expect("flush truncate");

            let after = server
                .object_keys()
                .into_iter()
                .filter(|key| key.contains("/blocks/"))
                .collect::<Vec<_>>();
            assert!(
                after.is_empty(),
                "truncate should remove stale chunk objects"
            );

            let mut reopened = S3BackedFilesystem::from_config(test_config(&server, "truncate"))
                .expect("reopen truncated fs");
            assert_eq!(
                reopened
                    .read_file("/large.txt")
                    .expect("read truncated file"),
                b"a".to_vec()
            );
        }

        #[test]
        fn s3_plugin_metadata_only_flush_reuses_existing_chunks() {
            let server = MockS3Server::start();

            let mut filesystem =
                S3BackedFilesystem::from_config(test_config(&server, "chmod")).expect("open s3 fs");
            filesystem
                .write_file("/large.txt", b"abcdefghijk".to_vec())
                .expect("write large file");
            filesystem.shutdown().expect("flush initial file");
            server.clear_requests();

            for offset in 0..10 {
                filesystem
                    .chmod("/large.txt", 0o600 + offset)
                    .expect("chmod large file");
            }
            filesystem.shutdown().expect("flush chmod batch");

            let requests = server.requests();
            let chunk_uploads = requests
                .iter()
                .filter(|request| request.method == "PUT" && request.path.contains("/blocks/"))
                .count();
            assert_eq!(
                chunk_uploads, 0,
                "metadata-only flush should not re-upload file chunks"
            );
            assert!(
                requests.iter().any(|request| request.method == "PUT"
                    && request.path.contains("filesystem-manifest.json")),
                "expected metadata-only flush to update the manifest"
            );

            let mut reopened = S3BackedFilesystem::from_config(test_config(&server, "chmod"))
                .expect("reopen s3 fs");
            assert_eq!(
                reopened
                    .stat("/large.txt")
                    .expect("stat chmodded file")
                    .mode
                    & 0o777,
                0o611
            );
            assert_eq!(
                reopened
                    .read_file("/large.txt")
                    .expect("read chmodded file"),
                b"abcdefghijk".to_vec()
            );
        }

        #[test]
        fn s3_plugin_rejects_oversized_manifest_entries() {
            let server = MockS3Server::start();
            let manifest = PersistedFilesystemManifest {
                format: String::from(MANIFEST_FORMAT),
                path_index: BTreeMap::from([
                    (String::from("/"), 1),
                    (String::from("/huge.bin"), 2),
                ]),
                inodes: BTreeMap::from([
                    (
                        1,
                        PersistedFilesystemInode {
                            metadata: secure_exec_kernel::vfs::MemoryFileSystemSnapshotMetadata {
                                mode: 0o040755,
                                uid: 0,
                                gid: 0,
                                nlink: 1,
                                ino: 1,
                                atime_ms: 0,
                                atime_nsec: 0,
                                mtime_ms: 0,
                                mtime_nsec: 0,
                                ctime_ms: 0,
                                ctime_nsec: 0,
                                birthtime_ms: 0,
                            },
                            kind: PersistedFilesystemInodeKind::Directory,
                        },
                    ),
                    (
                        2,
                        PersistedFilesystemInode {
                            metadata: secure_exec_kernel::vfs::MemoryFileSystemSnapshotMetadata {
                                mode: 0o100644,
                                uid: 0,
                                gid: 0,
                                nlink: 1,
                                ino: 2,
                                atime_ms: 0,
                                atime_nsec: 0,
                                mtime_ms: 0,
                                mtime_nsec: 0,
                                ctime_ms: 0,
                                ctime_nsec: 0,
                                birthtime_ms: 0,
                            },
                            kind: PersistedFilesystemInodeKind::File {
                                storage: PersistedFileStorage::Chunked {
                                    size: u64::MAX,
                                    chunks: Vec::new(),
                                },
                            },
                        },
                    ),
                ]),
                next_ino: 3,
            };
            server.put_object(
                "test-bucket/oversized/filesystem-manifest.json",
                serde_json::to_vec(&manifest).expect("serialize malicious manifest"),
            );

            let error = match S3BackedFilesystem::from_config(test_config(&server, "oversized")) {
                Ok(_) => panic!("oversized manifest should be rejected"),
                Err(error) => error,
            };
            assert_eq!(error.code(), "EINVAL");
            assert!(
                error.message().contains("limit"),
                "unexpected error message: {}",
                error.message()
            );
        }

        #[test]
        fn s3_plugin_accepts_legacy_agent_os_manifest_format() {
            let server = MockS3Server::start();
            let manifest = PersistedFilesystemManifest {
                format: String::from(LEGACY_AGENT_OS_MANIFEST_FORMAT),
                path_index: BTreeMap::from([
                    (String::from("/"), 1),
                    (String::from("/legacy.txt"), 2),
                ]),
                inodes: BTreeMap::from([
                    (
                        1,
                        PersistedFilesystemInode {
                            metadata: snapshot_metadata(1, 0o040755),
                            kind: PersistedFilesystemInodeKind::Directory,
                        },
                    ),
                    (
                        2,
                        PersistedFilesystemInode {
                            metadata: snapshot_metadata(2, 0o100644),
                            kind: PersistedFilesystemInodeKind::File {
                                storage: PersistedFileStorage::Inline {
                                    data_base64: BASE64.encode(b"legacy"),
                                },
                            },
                        },
                    ),
                ]),
                next_ino: 3,
            };
            server.put_object(
                "test-bucket/legacy/filesystem-manifest.json",
                serde_json::to_vec(&manifest).expect("serialize legacy manifest"),
            );

            let mut filesystem = S3BackedFilesystem::from_config(test_config(&server, "legacy"))
                .expect("legacy s3 manifest should load");
            assert_eq!(
                filesystem
                    .read_file("/legacy.txt")
                    .expect("read legacy manifest file"),
                b"legacy".to_vec()
            );
        }

        #[test]
        fn s3_plugin_rejects_chunk_objects_larger_than_remaining_manifest_size() {
            let server = MockS3Server::start();
            let manifest = PersistedFilesystemManifest {
                format: String::from(MANIFEST_FORMAT),
                path_index: BTreeMap::from([(String::from("/"), 1), (String::from("/one.bin"), 2)]),
                inodes: BTreeMap::from([
                    (
                        1,
                        PersistedFilesystemInode {
                            metadata: snapshot_metadata(1, 0o040755),
                            kind: PersistedFilesystemInodeKind::Directory,
                        },
                    ),
                    (
                        2,
                        PersistedFilesystemInode {
                            metadata: snapshot_metadata(2, 0o100644),
                            kind: PersistedFilesystemInodeKind::File {
                                storage: PersistedFileStorage::Chunked {
                                    size: 1,
                                    chunks: vec![PersistedChunkRef {
                                        index: 0,
                                        key: String::from("oversized-chunk/blocks/2/0"),
                                    }],
                                },
                            },
                        },
                    ),
                ]),
                next_ino: 3,
            };
            server.put_object(
                "test-bucket/oversized-chunk/filesystem-manifest.json",
                serde_json::to_vec(&manifest).expect("serialize oversized chunk manifest"),
            );
            server.put_object(
                "test-bucket/oversized-chunk/blocks/2/0",
                b"too large".to_vec(),
            );

            let error =
                match S3BackedFilesystem::from_config(test_config(&server, "oversized-chunk")) {
                    Ok(_) => panic!("oversized chunk object should be rejected"),
                    Err(error) => error,
                };
            assert_eq!(error.code(), "EIO");
            assert!(
                error.message().contains("limit is 1"),
                "unexpected error message: {}",
                error.message()
            );
        }

        #[test]
        fn s3_plugin_manifest_rejects_chunk_keys_outside_mount_prefix() {
            let server = MockS3Server::start();
            let manifest = PersistedFilesystemManifest {
                format: String::from(MANIFEST_FORMAT),
                path_index: BTreeMap::from([
                    (String::from("/"), 1),
                    (String::from("/escaped.bin"), 2),
                ]),
                inodes: BTreeMap::from([
                    (
                        1,
                        PersistedFilesystemInode {
                            metadata: snapshot_metadata(1, 0o040755),
                            kind: PersistedFilesystemInodeKind::Directory,
                        },
                    ),
                    (
                        2,
                        PersistedFilesystemInode {
                            metadata: snapshot_metadata(2, 0o100644),
                            kind: PersistedFilesystemInodeKind::File {
                                storage: PersistedFileStorage::Chunked {
                                    size: 4,
                                    chunks: vec![PersistedChunkRef {
                                        index: 0,
                                        key: String::from("outside-prefix/blocks/2/0"),
                                    }],
                                },
                            },
                        },
                    ),
                ]),
                next_ino: 3,
            };
            server.put_object(
                "test-bucket/safe-prefix/filesystem-manifest.json",
                serde_json::to_vec(&manifest).expect("serialize escaped manifest"),
            );
            server.put_object("test-bucket/outside-prefix/blocks/2/0", b"evil".to_vec());

            let error = match S3BackedFilesystem::from_config(test_config(&server, "safe-prefix")) {
                Ok(_) => panic!("escaped chunk key should be rejected"),
                Err(error) => error,
            };
            assert_eq!(error.code(), "EINVAL");
            assert!(
                error.message().contains("outside mount prefix"),
                "unexpected error message: {}",
                error.message()
            );
            assert!(
                server
                    .object_keys()
                    .contains(&String::from("test-bucket/outside-prefix/blocks/2/0")),
                "escaped chunk object should not be deleted as a stale safe-prefix chunk"
            );
        }

        #[test]
        fn s3_plugin_rejects_short_chunk_reconstruction() {
            let server = MockS3Server::start();
            let manifest = PersistedFilesystemManifest {
                format: String::from(MANIFEST_FORMAT),
                path_index: BTreeMap::from([
                    (String::from("/"), 1),
                    (String::from("/short.bin"), 2),
                ]),
                inodes: BTreeMap::from([
                    (
                        1,
                        PersistedFilesystemInode {
                            metadata: snapshot_metadata(1, 0o040755),
                            kind: PersistedFilesystemInodeKind::Directory,
                        },
                    ),
                    (
                        2,
                        PersistedFilesystemInode {
                            metadata: snapshot_metadata(2, 0o100644),
                            kind: PersistedFilesystemInodeKind::File {
                                storage: PersistedFileStorage::Chunked {
                                    size: 3,
                                    chunks: vec![PersistedChunkRef {
                                        index: 0,
                                        key: String::from("short-chunk/blocks/2/0"),
                                    }],
                                },
                            },
                        },
                    ),
                ]),
                next_ino: 3,
            };
            server.put_object(
                "test-bucket/short-chunk/filesystem-manifest.json",
                serde_json::to_vec(&manifest).expect("serialize short chunk manifest"),
            );
            server.put_object("test-bucket/short-chunk/blocks/2/0", b"no".to_vec());

            let error = match S3BackedFilesystem::from_config(test_config(&server, "short-chunk")) {
                Ok(_) => panic!("short chunk reconstruction should be rejected"),
                Err(error) => error,
            };
            assert_eq!(error.code(), "EINVAL");
            assert!(
                error.message().contains("restored 2 bytes but declared 3"),
                "unexpected error message: {}",
                error.message()
            );
        }

        #[test]
        fn s3_plugin_rejects_non_contiguous_chunk_indexes_before_loading_chunks() {
            let server = MockS3Server::start();
            let manifest = PersistedFilesystemManifest {
                format: String::from(MANIFEST_FORMAT),
                path_index: BTreeMap::from([
                    (String::from("/"), 1),
                    (String::from("/gapped.bin"), 2),
                ]),
                inodes: BTreeMap::from([
                    (
                        1,
                        PersistedFilesystemInode {
                            metadata: snapshot_metadata(1, 0o040755),
                            kind: PersistedFilesystemInodeKind::Directory,
                        },
                    ),
                    (
                        2,
                        PersistedFilesystemInode {
                            metadata: snapshot_metadata(2, 0o100644),
                            kind: PersistedFilesystemInodeKind::File {
                                storage: PersistedFileStorage::Chunked {
                                    size: 2,
                                    chunks: vec![
                                        PersistedChunkRef {
                                            index: 0,
                                            key: String::from("gapped-chunk/blocks/2/0"),
                                        },
                                        PersistedChunkRef {
                                            index: 2,
                                            key: String::from("gapped-chunk/blocks/2/2"),
                                        },
                                    ],
                                },
                            },
                        },
                    ),
                ]),
                next_ino: 3,
            };
            server.put_object(
                "test-bucket/gapped-chunk/filesystem-manifest.json",
                serde_json::to_vec(&manifest).expect("serialize gapped chunk manifest"),
            );

            let error = match S3BackedFilesystem::from_config(test_config(&server, "gapped-chunk"))
            {
                Ok(_) => panic!("gapped chunk manifest should be rejected"),
                Err(error) => error,
            };
            assert_eq!(error.code(), "EINVAL");
            assert!(
                error.message().contains("chunk indexes must be contiguous"),
                "unexpected error message: {}",
                error.message()
            );
            assert!(
                !server
                    .requests()
                    .iter()
                    .any(|request| request.path.contains("/blocks/")),
                "chunk objects should not be loaded after index validation fails"
            );
        }

        fn snapshot_metadata(
            ino: u64,
            mode: u32,
        ) -> secure_exec_kernel::vfs::MemoryFileSystemSnapshotMetadata {
            secure_exec_kernel::vfs::MemoryFileSystemSnapshotMetadata {
                mode,
                uid: 0,
                gid: 0,
                nlink: 1,
                ino,
                atime_ms: 0,
                atime_nsec: 0,
                mtime_ms: 0,
                mtime_nsec: 0,
                ctime_ms: 0,
                ctime_nsec: 0,
                birthtime_ms: 0,
            }
        }
    }
}

use secure_exec_bridge::StructuredEventRecord;
use secure_exec_sidecar::wire::{
    BootstrapRootFilesystemRequest, ConfigureVmRequest, DisposeReason, DisposeVmRequest,
    GuestFilesystemCallRequest, GuestFilesystemOperation, GuestRuntimeKind, MountDescriptor,
    MountPluginDescriptor, RequestPayload, ResponsePayload, RootFilesystemEntry,
    RootFilesystemEntryEncoding, RootFilesystemEntryKind,
};
use std::collections::HashMap;
use support::{
    authenticate_wire, create_vm_wire, open_session_wire, temp_dir, wire_request, wire_vm,
};

fn structured_events(
    sidecar: &secure_exec_sidecar::NativeSidecar<support::RecordingBridge>,
) -> Vec<StructuredEventRecord> {
    sidecar
        .with_bridge_mut(|bridge| bridge.structured_events.clone())
        .expect("inspect structured events")
}

#[test]
fn dispose_vm_surfaces_s3_flush_failures_as_structured_events() {
    let server = s3::test_support::MockS3Server::start();
    let mut sidecar = support::new_sidecar("s3-dispose-shutdown-failure");
    let cwd = temp_dir("s3-dispose-shutdown-failure-cwd");

    let connection_id = authenticate_wire(&mut sidecar, "conn-1");
    let session_id = open_session_wire(&mut sidecar, 2, &connection_id);
    let (vm_id, _) = create_vm_wire(
        &mut sidecar,
        3,
        &connection_id,
        &session_id,
        GuestRuntimeKind::JavaScript,
        &cwd,
    );

    sidecar
        .dispatch_wire_blocking(wire_request(
            4,
            wire_vm(&connection_id, &session_id, &vm_id),
            RequestPayload::BootstrapRootFilesystemRequest(BootstrapRootFilesystemRequest {
                entries: vec![RootFilesystemEntry {
                    path: String::from("/data"),
                    kind: RootFilesystemEntryKind::Directory,
                    mode: None,
                    uid: None,
                    gid: None,
                    content: None,
                    encoding: None,
                    target: None,
                    executable: false,
                }],
            }),
        ))
        .expect("bootstrap s3 mountpoint");

    sidecar
        .dispatch_wire_blocking(wire_request(
            5,
            wire_vm(&connection_id, &session_id, &vm_id),
            RequestPayload::ConfigureVmRequest(ConfigureVmRequest {
                mounts: vec![MountDescriptor {
                    guest_path: String::from("/data"),
                    read_only: false,
                    plugin: MountPluginDescriptor {
                        id: String::from("s3"),
                        config: serde_json::to_string(&serde_json::json!({
                            "bucket": "test-bucket",
                            "prefix": "dispose-failure",
                            "region": "us-east-1",
                            "endpoint": server.base_url(),
                            "credentials": {
                                "accessKeyId": "minioadmin",
                                "secretAccessKey": "minioadmin",
                            },
                            "chunkSize": 8,
                            "inlineThreshold": 4,
                        }))
                        .expect("serialize s3 mount config"),
                    },
                }],
                software: Vec::new(),
                permissions: None,
                module_access_cwd: None,
                instructions: Vec::new(),
                projected_modules: Vec::new(),
                command_permissions: HashMap::new(),
                loopback_exempt_ports: Vec::new(),
            }),
        ))
        .expect("configure s3 mount");

    let write = sidecar
        .dispatch_wire_blocking(wire_request(
            6,
            wire_vm(&connection_id, &session_id, &vm_id),
            RequestPayload::GuestFilesystemCallRequest(GuestFilesystemCallRequest {
                operation: GuestFilesystemOperation::WriteFile,
                path: String::from("/data/pending.txt"),
                destination_path: None,
                target: None,
                content: Some(String::from("pending s3 flush")),
                encoding: Some(RootFilesystemEntryEncoding::Utf8),
                recursive: false,
                mode: None,
                uid: None,
                gid: None,
                atime_ms: None,
                mtime_ms: None,
                len: None,
                offset: None,
            }),
        ))
        .expect("write pending s3 file");
    match write.response.payload {
        ResponsePayload::GuestFilesystemResultResponse(_) => {}
        other => panic!("unexpected write response: {other:?}"),
    }

    drop(server);

    let dispose = sidecar
        .dispatch_wire_blocking(wire_request(
            7,
            wire_vm(&connection_id, &session_id, &vm_id),
            RequestPayload::DisposeVmRequest(DisposeVmRequest {
                reason: DisposeReason::Requested,
            }),
        ))
        .expect("dispose vm after s3 shutdown failure");
    match dispose.response.payload {
        ResponsePayload::VmDisposedResponse(response) => assert_eq!(response.vm_id, vm_id),
        other => panic!("unexpected dispose response: {other:?}"),
    }

    let event = structured_events(&sidecar)
        .into_iter()
        .rfind(|event| event.name == "filesystem.mount.shutdown_failed")
        .expect("expected structured shutdown failure event");
    assert_eq!(event.vm_id, vm_id);
    assert_eq!(event.fields["guest_path"], "/data");
    assert_eq!(event.fields["plugin_id"], "s3");
    assert_eq!(event.fields["read_only"], "false");
    assert_eq!(event.fields["phase"], "dispose_vm");
    assert_eq!(event.fields["error_code"], "EIO");
    assert!(
        event.fields["error"].contains("write s3 object"),
        "unexpected shutdown error: {}",
        event.fields["error"]
    );
    assert!(
        event.fields["error"].contains("dispose-failure/"),
        "unexpected shutdown error: {}",
        event.fields["error"]
    );
    event.fields["timestamp"]
        .parse::<u128>()
        .expect("structured event timestamp should be numeric");
}
