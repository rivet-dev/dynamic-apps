use crate::bridge::MountPluginContext;
use crate::protocol::{
    JsBridgeCallRequest, JsBridgeResultResponse, OwnershipScope, SidecarRequestPayload,
    SidecarResponsePayload,
};
use crate::SidecarError;

use base64::engine::general_purpose::STANDARD as BASE64_STANDARD;
use base64::Engine;
use rusqlite::{params, Connection, OptionalExtension};
use secure_exec_kernel::mount_plugin::{
    FileSystemPluginFactory, OpenFileSystemPluginRequest, PluginError,
};
use secure_exec_kernel::mount_table::{MountedFileSystem, MountedVirtualFileSystem};
use secure_exec_kernel::vfs::{
    normalize_path, VfsError, VfsResult, VirtualDirEntry, VirtualFileSystem, VirtualStat, S_IFDIR,
    S_IFLNK, S_IFREG,
};
use serde::Deserialize;
use serde_json::{json, Value};
use std::path::Path;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::Duration;
use std::time::{SystemTime, UNIX_EPOCH};

const DEFAULT_FILE_MODE: u32 = S_IFREG | 0o644;
const DEFAULT_DIR_MODE: u32 = S_IFDIR | 0o755;
const DEFAULT_SYMLINK_MODE: u32 = S_IFLNK | 0o777;
const DIRECTORY_SIZE: u64 = 4096;
const SQLITE_VFS_CALLBACK_TIMEOUT: Duration = Duration::from_secs(30);

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
struct SqliteVfsMountConfig {
    #[serde(default)]
    backend: Option<String>,
    #[serde(default)]
    database_path: Option<String>,
    #[serde(default)]
    mount_id: Option<String>,
}

#[derive(Debug)]
pub(crate) struct SqliteVfsMountPlugin;

pub(crate) trait SqliteVfsCallbackContext {
    fn sqlite_vfs_callback_context(
        &self,
    ) -> Option<(
        crate::state::SharedSidecarRequestClient,
        OwnershipScope,
        Option<usize>,
    )>;
}

impl SqliteVfsCallbackContext for () {
    fn sqlite_vfs_callback_context(
        &self,
    ) -> Option<(
        crate::state::SharedSidecarRequestClient,
        OwnershipScope,
        Option<usize>,
    )> {
        None
    }
}

impl<B> SqliteVfsCallbackContext for MountPluginContext<B> {
    fn sqlite_vfs_callback_context(
        &self,
    ) -> Option<(
        crate::state::SharedSidecarRequestClient,
        OwnershipScope,
        Option<usize>,
    )> {
        Some((
            self.sidecar_requests.clone(),
            OwnershipScope::vm(
                self.connection_id.clone(),
                self.session_id.clone(),
                self.vm_id.clone(),
            ),
            self.max_pread_bytes,
        ))
    }
}

impl<Context> FileSystemPluginFactory<Context> for SqliteVfsMountPlugin
where
    Context: SqliteVfsCallbackContext,
{
    fn plugin_id(&self) -> &'static str {
        "sqlite_vfs"
    }

    fn open(
        &self,
        request: OpenFileSystemPluginRequest<'_, Context>,
    ) -> Result<Box<dyn MountedFileSystem + Send>, PluginError> {
        let config: SqliteVfsMountConfig = serde_json::from_value(request.config.clone())
            .map_err(|error| PluginError::invalid_input(error.to_string()))?;
        match config.backend.as_deref().unwrap_or("local") {
            "local" => {
                let database_path = config.database_path.ok_or_else(|| {
                    PluginError::invalid_input("sqlite_vfs local backend requires databasePath")
                })?;
                let filesystem = SqliteVfsFilesystem::open(database_path)
                    .map_err(|error| PluginError::invalid_input(error.message().to_owned()))?;
                Ok(Box::new(MountedVirtualFileSystem::new(filesystem)))
            }
            "callback" => {
                let (requests, ownership, max_read_bytes) = request
                    .context
                    .sqlite_vfs_callback_context()
                    .ok_or_else(|| {
                        PluginError::invalid_input(
                            "sqlite_vfs callback backend requires sidecar request context",
                        )
                    })?;
                let mount_id = config
                    .mount_id
                    .unwrap_or_else(|| request.guest_path.to_owned());
                Ok(Box::new(MountedVirtualFileSystem::new(
                    CallbackSqliteVfsFilesystem::new(requests, ownership, mount_id, max_read_bytes),
                )))
            }
            other => Err(PluginError::invalid_input(format!(
                "unsupported sqlite_vfs backend: {other}"
            ))),
        }
    }
}

#[derive(Debug, Clone)]
struct FsRow {
    path: String,
    is_directory: bool,
    content: Option<Vec<u8>>,
    mode: u32,
    uid: u32,
    gid: u32,
    size: u64,
    atime_ms: u64,
    mtime_ms: u64,
    ctime_ms: u64,
    birthtime_ms: u64,
    symlink_target: Option<String>,
    nlink: u64,
}

#[derive(Clone)]
struct CallbackSqliteVfsFilesystem {
    requests: crate::state::SharedSidecarRequestClient,
    ownership: OwnershipScope,
    mount_id: String,
    next_call_id: Arc<AtomicU64>,
    max_read_bytes: Option<usize>,
}

impl CallbackSqliteVfsFilesystem {
    fn new(
        requests: crate::state::SharedSidecarRequestClient,
        ownership: OwnershipScope,
        mount_id: String,
        max_read_bytes: Option<usize>,
    ) -> Self {
        Self {
            requests,
            ownership,
            mount_id,
            next_call_id: Arc::new(AtomicU64::new(1)),
            max_read_bytes,
        }
    }

    fn next_call_id(&self) -> String {
        format!(
            "sqlite-vfs-call-{}",
            self.next_call_id.fetch_add(1, Ordering::Relaxed)
        )
    }

    fn request_path(&self, operation: &str, path: &str, args: Value) -> VfsResult<Option<Value>> {
        let args = serde_json::to_string(&args).map_err(|error| {
            VfsError::io(format!(
                "failed to encode sqlite_vfs callback args for {operation} '{path}': {error}"
            ))
        })?;
        let payload = SidecarRequestPayload::JsBridgeCall(JsBridgeCallRequest {
            call_id: self.next_call_id(),
            mount_id: self.mount_id.clone(),
            operation: operation.to_owned(),
            args,
        });
        match self
            .requests
            .invoke(self.ownership.clone(), payload, SQLITE_VFS_CALLBACK_TIMEOUT)
            .map_err(|error| Self::sidecar_error_to_vfs(operation, path, error))?
        {
            SidecarResponsePayload::JsBridgeResult(JsBridgeResultResponse {
                result,
                error,
                ..
            }) => {
                if let Some(error) = error {
                    return Err(Self::callback_error_to_vfs(operation, path, &error));
                }
                result
                    .map(|result| {
                        serde_json::from_str(&result).map_err(|error| {
                            VfsError::io(format!(
                                "invalid sqlite_vfs callback result for {operation} '{path}': {error}"
                            ))
                        })
                    })
                    .transpose()
            }
            other => Err(VfsError::io(format!(
                "unexpected sqlite_vfs callback response payload: {other:?}"
            ))),
        }
    }

    fn sidecar_error_to_vfs(operation: &str, path: &str, error: SidecarError) -> VfsError {
        match error {
            SidecarError::Io(message) if message.contains("timed out") => {
                VfsError::io(format!("{operation} {path}: {message}"))
            }
            other => VfsError::io(format!("{operation} {path}: {other}")),
        }
    }

    fn callback_error_to_vfs(operation: &str, path: &str, error: &str) -> VfsError {
        let lower = error.to_ascii_lowercase();
        let code = if lower.contains("enoent")
            || lower.contains("not found")
            || lower.contains("no such file")
        {
            "ENOENT"
        } else if lower.contains("enotdir") || lower.contains("not a directory") {
            "ENOTDIR"
        } else if lower.contains("eisdir")
            || lower.contains("directory") && lower.contains("illegal")
        {
            "EISDIR"
        } else if lower.contains("enotempty") || lower.contains("not empty") {
            "ENOTEMPTY"
        } else if lower.contains("eexist") || lower.contains("already exists") {
            "EEXIST"
        } else if lower.contains("enosys") || lower.contains("not supported") {
            "ENOSYS"
        } else if lower.contains("eacces")
            || lower.contains("eperm")
            || lower.contains("permission denied")
        {
            "EACCES"
        } else {
            "EIO"
        };
        VfsError::new(code, format!("{error}, {operation} '{path}'"))
    }

    fn parse_required<T>(&self, operation: &str, path: &str, result: Option<Value>) -> VfsResult<T>
    where
        T: for<'de> Deserialize<'de>,
    {
        let value = result.ok_or_else(|| {
            VfsError::io(format!(
                "sqlite_vfs callback returned no payload for {operation} '{path}'"
            ))
        })?;
        serde_json::from_value(value).map_err(|error| {
            VfsError::io(format!(
                "invalid sqlite_vfs callback payload for {operation} '{path}': {error}"
            ))
        })
    }

    fn parse_bytes(
        &self,
        operation: &str,
        path: &str,
        result: Option<Value>,
    ) -> VfsResult<Vec<u8>> {
        match result.ok_or_else(|| {
            VfsError::io(format!(
                "sqlite_vfs callback returned no payload for {operation} '{path}'"
            ))
        })? {
            Value::String(encoded) => {
                let estimated_len = estimated_base64_decoded_len(&encoded).ok_or_else(|| {
                    VfsError::io(format!(
                        "sqlite_vfs callback base64 payload length overflows for {operation} '{path}'"
                    ))
                })?;
                check_read_length(operation, path, estimated_len, self.max_read_bytes)?;
                let decoded = BASE64_STANDARD.decode(encoded).map_err(|error| {
                    VfsError::io(format!(
                        "invalid sqlite_vfs callback base64 payload for {operation} '{path}': {error}"
                    ))
                })?;
                check_read_length(operation, path, decoded.len(), self.max_read_bytes)?;
                Ok(decoded)
            }
            other => Err(VfsError::io(format!(
                "unsupported sqlite_vfs callback byte payload for {operation} '{path}': {other:?}"
            ))),
        }
    }
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
struct CallbackVirtualStat {
    mode: u32,
    size: u64,
    blocks: u64,
    dev: u64,
    rdev: u64,
    #[serde(alias = "is_directory")]
    is_directory: bool,
    #[serde(alias = "is_symbolic_link")]
    is_symbolic_link: bool,
    #[serde(alias = "atime_ms")]
    atime_ms: u64,
    #[serde(default, alias = "atime_nsec")]
    atime_nsec: u32,
    #[serde(alias = "mtime_ms")]
    mtime_ms: u64,
    #[serde(default, alias = "mtime_nsec")]
    mtime_nsec: u32,
    #[serde(alias = "ctime_ms")]
    ctime_ms: u64,
    #[serde(default, alias = "ctime_nsec")]
    ctime_nsec: u32,
    #[serde(alias = "birthtime_ms")]
    birthtime_ms: u64,
    ino: u64,
    nlink: u64,
    uid: u32,
    gid: u32,
}

impl From<CallbackVirtualStat> for VirtualStat {
    fn from(stat: CallbackVirtualStat) -> Self {
        Self {
            mode: stat.mode,
            size: stat.size,
            blocks: stat.blocks,
            dev: stat.dev,
            rdev: stat.rdev,
            is_directory: stat.is_directory,
            is_symbolic_link: stat.is_symbolic_link,
            atime_ms: stat.atime_ms,
            atime_nsec: stat.atime_nsec,
            mtime_ms: stat.mtime_ms,
            mtime_nsec: stat.mtime_nsec,
            ctime_ms: stat.ctime_ms,
            ctime_nsec: stat.ctime_nsec,
            birthtime_ms: stat.birthtime_ms,
            ino: stat.ino,
            nlink: stat.nlink,
            uid: stat.uid,
            gid: stat.gid,
        }
    }
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
struct CallbackDirEntry {
    name: String,
    #[serde(alias = "is_directory")]
    is_directory: bool,
    #[serde(alias = "is_symbolic_link")]
    is_symbolic_link: bool,
}

impl From<CallbackDirEntry> for VirtualDirEntry {
    fn from(entry: CallbackDirEntry) -> Self {
        Self {
            name: entry.name,
            is_directory: entry.is_directory,
            is_symbolic_link: entry.is_symbolic_link,
        }
    }
}

impl VirtualFileSystem for CallbackSqliteVfsFilesystem {
    fn read_file(&mut self, path: &str) -> VfsResult<Vec<u8>> {
        let result = self.request_path("readFile", path, json!({ "path": path }))?;
        self.parse_bytes("readFile", path, result)
    }

    fn read_dir(&mut self, path: &str) -> VfsResult<Vec<String>> {
        self.parse_required(
            "readDir",
            path,
            self.request_path("readDir", path, json!({ "path": path }))?,
        )
    }

    fn read_dir_with_types(&mut self, path: &str) -> VfsResult<Vec<VirtualDirEntry>> {
        let entries: Vec<CallbackDirEntry> = self.parse_required(
            "readDirWithTypes",
            path,
            self.request_path("readDirWithTypes", path, json!({ "path": path }))?,
        )?;
        Ok(entries.into_iter().map(Into::into).collect())
    }

    fn write_file(&mut self, path: &str, content: impl Into<Vec<u8>>) -> VfsResult<()> {
        let content = BASE64_STANDARD.encode(content.into());
        self.request_path(
            "writeFile",
            path,
            json!({
                "path": path,
                "content": content,
            }),
        )?;
        Ok(())
    }

    fn write_file_with_mode(
        &mut self,
        path: &str,
        content: impl Into<Vec<u8>>,
        mode: Option<u32>,
    ) -> VfsResult<()> {
        let content = BASE64_STANDARD.encode(content.into());
        self.request_path(
            "writeFile",
            path,
            json!({
                "path": path,
                "content": content,
                "mode": mode,
            }),
        )?;
        Ok(())
    }

    fn create_file_exclusive(&mut self, path: &str, content: impl Into<Vec<u8>>) -> VfsResult<()> {
        let content = BASE64_STANDARD.encode(content.into());
        self.request_path(
            "createFileExclusive",
            path,
            json!({
                "path": path,
                "content": content,
            }),
        )?;
        Ok(())
    }

    fn create_file_exclusive_with_mode(
        &mut self,
        path: &str,
        content: impl Into<Vec<u8>>,
        mode: Option<u32>,
    ) -> VfsResult<()> {
        let content = BASE64_STANDARD.encode(content.into());
        self.request_path(
            "createFileExclusive",
            path,
            json!({
                "path": path,
                "content": content,
                "mode": mode,
            }),
        )?;
        Ok(())
    }

    fn create_dir(&mut self, path: &str) -> VfsResult<()> {
        self.request_path("createDir", path, json!({ "path": path }))?;
        Ok(())
    }

    fn create_dir_with_mode(&mut self, path: &str, mode: Option<u32>) -> VfsResult<()> {
        self.request_path(
            "createDir",
            path,
            json!({
                "path": path,
                "mode": mode,
            }),
        )?;
        Ok(())
    }

    fn mkdir(&mut self, path: &str, recursive: bool) -> VfsResult<()> {
        self.request_path(
            "mkdir",
            path,
            json!({
                "path": path,
                "recursive": recursive,
            }),
        )?;
        Ok(())
    }

    fn mkdir_with_mode(&mut self, path: &str, recursive: bool, mode: Option<u32>) -> VfsResult<()> {
        self.request_path(
            "mkdir",
            path,
            json!({
                "path": path,
                "recursive": recursive,
                "mode": mode,
            }),
        )?;
        Ok(())
    }

    fn exists(&self, path: &str) -> bool {
        let Ok(args) = serde_json::to_string(&json!({ "path": path })) else {
            return false;
        };
        self.requests
            .invoke(
                self.ownership.clone(),
                SidecarRequestPayload::JsBridgeCall(JsBridgeCallRequest {
                    call_id: self.next_call_id(),
                    mount_id: self.mount_id.clone(),
                    operation: String::from("exists"),
                    args,
                }),
                SQLITE_VFS_CALLBACK_TIMEOUT,
            )
            .ok()
            .and_then(|payload| match payload {
                SidecarResponsePayload::JsBridgeResult(JsBridgeResultResponse {
                    result,
                    error,
                    ..
                }) if error.is_none() => result,
                _ => None,
            })
            .and_then(|value| serde_json::from_str::<Value>(&value).ok())
            .and_then(|value| value.as_bool())
            .unwrap_or(false)
    }

    fn stat(&mut self, path: &str) -> VfsResult<VirtualStat> {
        let stat: CallbackVirtualStat = self.parse_required(
            "stat",
            path,
            self.request_path("stat", path, json!({ "path": path }))?,
        )?;
        Ok(stat.into())
    }

    fn remove_file(&mut self, path: &str) -> VfsResult<()> {
        self.request_path("removeFile", path, json!({ "path": path }))?;
        Ok(())
    }

    fn remove_dir(&mut self, path: &str) -> VfsResult<()> {
        self.request_path("removeDir", path, json!({ "path": path }))?;
        Ok(())
    }

    fn rename(&mut self, old_path: &str, new_path: &str) -> VfsResult<()> {
        self.request_path(
            "rename",
            old_path,
            json!({
                "oldPath": old_path,
                "newPath": new_path,
            }),
        )?;
        Ok(())
    }

    fn realpath(&self, path: &str) -> VfsResult<String> {
        let result = self.request_path("realpath", path, json!({ "path": path }))?;
        self.parse_required("realpath", path, result)
    }

    fn symlink(&mut self, target: &str, link_path: &str) -> VfsResult<()> {
        self.request_path(
            "symlink",
            link_path,
            json!({
                "target": target,
                "path": link_path,
            }),
        )?;
        Ok(())
    }

    fn read_link(&self, path: &str) -> VfsResult<String> {
        let result = self.request_path("readLink", path, json!({ "path": path }))?;
        self.parse_required("readLink", path, result)
    }

    fn lstat(&self, path: &str) -> VfsResult<VirtualStat> {
        let stat: CallbackVirtualStat = self.parse_required(
            "lstat",
            path,
            self.request_path("lstat", path, json!({ "path": path }))?,
        )?;
        Ok(stat.into())
    }

    fn link(&mut self, old_path: &str, new_path: &str) -> VfsResult<()> {
        self.request_path(
            "link",
            old_path,
            json!({
                "oldPath": old_path,
                "newPath": new_path,
            }),
        )?;
        Ok(())
    }

    fn chmod(&mut self, path: &str, mode: u32) -> VfsResult<()> {
        self.request_path(
            "chmod",
            path,
            json!({
                "path": path,
                "mode": mode,
            }),
        )?;
        Ok(())
    }

    fn chown(&mut self, path: &str, uid: u32, gid: u32) -> VfsResult<()> {
        self.request_path(
            "chown",
            path,
            json!({
                "path": path,
                "uid": uid,
                "gid": gid,
            }),
        )?;
        Ok(())
    }

    fn utimes(&mut self, path: &str, atime_ms: u64, mtime_ms: u64) -> VfsResult<()> {
        self.request_path(
            "utimes",
            path,
            json!({
                "path": path,
                "atimeMs": atime_ms,
                "mtimeMs": mtime_ms,
            }),
        )?;
        Ok(())
    }

    fn truncate(&mut self, path: &str, length: u64) -> VfsResult<()> {
        self.request_path(
            "truncate",
            path,
            json!({
                "path": path,
                "length": length,
            }),
        )?;
        Ok(())
    }

    fn pread(&mut self, path: &str, offset: u64, length: usize) -> VfsResult<Vec<u8>> {
        let result = self.request_path(
            "pread",
            path,
            json!({
                "path": path,
                "offset": offset,
                "length": length,
            }),
        )?;
        self.parse_bytes("pread", path, result)
    }
}

pub(crate) struct SqliteVfsFilesystem {
    conn: Connection,
}

impl SqliteVfsFilesystem {
    pub(crate) fn open(path: impl AsRef<Path>) -> VfsResult<Self> {
        let conn = Connection::open(path).map_err(sqlite_error)?;
        let filesystem = Self { conn };
        filesystem.ensure_schema()?;
        filesystem.ensure_root()?;
        Ok(filesystem)
    }

    #[cfg(test)]
    fn memory() -> VfsResult<Self> {
        let conn = Connection::open_in_memory().map_err(sqlite_error)?;
        let filesystem = Self { conn };
        filesystem.ensure_schema()?;
        filesystem.ensure_root()?;
        Ok(filesystem)
    }

    fn ensure_schema(&self) -> VfsResult<()> {
        self.conn
            .execute_batch(
                "\
CREATE TABLE IF NOT EXISTS agent_os_fs_entries (
    path TEXT PRIMARY KEY,
    is_directory INTEGER NOT NULL DEFAULT 0,
    content BLOB,
    mode INTEGER NOT NULL DEFAULT 33188,
    uid INTEGER NOT NULL DEFAULT 0,
    gid INTEGER NOT NULL DEFAULT 0,
    size INTEGER NOT NULL DEFAULT 0,
    atime_ms INTEGER NOT NULL,
    mtime_ms INTEGER NOT NULL,
    ctime_ms INTEGER NOT NULL,
    birthtime_ms INTEGER NOT NULL,
    symlink_target TEXT,
    nlink INTEGER NOT NULL DEFAULT 1
);
CREATE INDEX IF NOT EXISTS idx_fs_entries_parent
    ON agent_os_fs_entries(path);
",
            )
            .map_err(sqlite_error)
    }

    fn ensure_root(&self) -> VfsResult<()> {
        let now = now_ms();
        self.conn
            .execute(
                "\
INSERT OR IGNORE INTO agent_os_fs_entries
    (path, is_directory, content, mode, uid, gid, size, atime_ms, mtime_ms, ctime_ms, birthtime_ms, symlink_target, nlink)
VALUES
    ('/', 1, NULL, ?1, 0, 0, 0, ?2, ?2, ?2, ?2, NULL, 2)",
                params![DEFAULT_DIR_MODE, now],
            )
            .map_err(sqlite_error)?;
        Ok(())
    }

    fn row(&self, path: &str) -> VfsResult<Option<FsRow>> {
        self.conn
            .query_row(
                "\
SELECT path, is_directory, content, mode, uid, gid, size, atime_ms, mtime_ms, ctime_ms, birthtime_ms, symlink_target, nlink
FROM agent_os_fs_entries WHERE path = ?1",
                params![path],
                |row| {
                    Ok(FsRow {
                        path: row.get(0)?,
                        is_directory: row.get::<_, i64>(1)? != 0,
                        content: row.get(2)?,
                        mode: row.get::<_, i64>(3)? as u32,
                        uid: row.get::<_, i64>(4)? as u32,
                        gid: row.get::<_, i64>(5)? as u32,
                        size: row.get::<_, i64>(6)? as u64,
                        atime_ms: row.get::<_, i64>(7)? as u64,
                        mtime_ms: row.get::<_, i64>(8)? as u64,
                        ctime_ms: row.get::<_, i64>(9)? as u64,
                        birthtime_ms: row.get::<_, i64>(10)? as u64,
                        symlink_target: row.get(11)?,
                        nlink: row.get::<_, i64>(12)? as u64,
                    })
                },
            )
            .optional()
            .map_err(sqlite_error)
    }

    fn row_or_enoent(&self, op: &'static str, path: &str) -> VfsResult<FsRow> {
        self.row(path)?.ok_or_else(|| {
            VfsError::new(
                "ENOENT",
                format!("no such file or directory, {op} '{path}'"),
            )
        })
    }

    fn ensure_parent_dir(&self, path: &str) -> VfsResult<()> {
        let parent = parent_path(path);
        if parent == path {
            return Ok(());
        }
        let row = self.row_or_enoent("open", &parent)?;
        if !row.is_directory {
            return Err(VfsError::new(
                "ENOTDIR",
                format!("not a directory, open '{parent}'"),
            ));
        }
        Ok(())
    }

    fn direct_children(&self, path: &str) -> VfsResult<Vec<FsRow>> {
        let prefix = if path == "/" {
            "/".to_owned()
        } else {
            format!("{path}/")
        };
        let pattern = format!("{prefix}%");
        let mut stmt = self
            .conn
            .prepare(
                "\
SELECT path, is_directory, content, mode, uid, gid, size, atime_ms, mtime_ms, ctime_ms, birthtime_ms, symlink_target, nlink
FROM agent_os_fs_entries WHERE path LIKE ?1 AND path != ?2 ORDER BY path",
            )
            .map_err(sqlite_error)?;
        let rows = stmt
            .query_map(params![pattern, path], |row| {
                Ok(FsRow {
                    path: row.get(0)?,
                    is_directory: row.get::<_, i64>(1)? != 0,
                    content: row.get(2)?,
                    mode: row.get::<_, i64>(3)? as u32,
                    uid: row.get::<_, i64>(4)? as u32,
                    gid: row.get::<_, i64>(5)? as u32,
                    size: row.get::<_, i64>(6)? as u64,
                    atime_ms: row.get::<_, i64>(7)? as u64,
                    mtime_ms: row.get::<_, i64>(8)? as u64,
                    ctime_ms: row.get::<_, i64>(9)? as u64,
                    birthtime_ms: row.get::<_, i64>(10)? as u64,
                    symlink_target: row.get(11)?,
                    nlink: row.get::<_, i64>(12)? as u64,
                })
            })
            .map_err(sqlite_error)?;

        let mut out = Vec::new();
        for row in rows {
            let row = row.map_err(sqlite_error)?;
            let relative = row.path.strip_prefix(&prefix).unwrap_or(row.path.as_str());
            if !relative.is_empty() && !relative.contains('/') {
                out.push(row);
            }
        }
        Ok(out)
    }

    fn insert_entry(
        &self,
        path: &str,
        is_directory: bool,
        content: Option<&[u8]>,
        mode: u32,
        symlink_target: Option<&str>,
    ) -> VfsResult<()> {
        let now = now_ms();
        let size = if is_directory {
            0
        } else if let Some(target) = symlink_target {
            target.len() as u64
        } else {
            content.map(|content| content.len() as u64).unwrap_or(0)
        };
        self.conn
            .execute(
                "\
INSERT INTO agent_os_fs_entries
    (path, is_directory, content, mode, uid, gid, size, atime_ms, mtime_ms, ctime_ms, birthtime_ms, symlink_target, nlink)
VALUES
    (?1, ?2, ?3, ?4, 0, 0, ?5, ?6, ?6, ?6, ?6, ?7, ?8)",
                params![
                    path,
                    if is_directory { 1 } else { 0 },
                    content,
                    mode,
                    size,
                    now,
                    symlink_target,
                    if is_directory { 2 } else { 1 },
                ],
            )
            .map_err(|error| {
                if matches!(error, rusqlite::Error::SqliteFailure(ref inner, _)
                    if inner.extended_code == rusqlite::ffi::SQLITE_CONSTRAINT_PRIMARYKEY)
                {
                    VfsError::new("EEXIST", format!("file already exists, open '{path}'"))
                } else {
                    sqlite_error(error)
                }
            })?;
        Ok(())
    }

    fn overwrite_file(&self, path: &str, content: &[u8], mode: Option<u32>) -> VfsResult<()> {
        let existing = self.row(path)?;
        match existing {
            Some(row) if row.is_directory => {
                return Err(VfsError::new(
                    "EISDIR",
                    format!("illegal operation on a directory, open '{path}'"),
                ));
            }
            Some(row) => {
                let now = now_ms();
                let mode = mode.unwrap_or(row.mode);
                self.conn
                    .execute(
                        "\
UPDATE agent_os_fs_entries
SET content = ?2, mode = ?3, size = ?4, mtime_ms = ?5, ctime_ms = ?5, symlink_target = NULL, is_directory = 0
WHERE path = ?1",
                        params![path, content, mode, content.len() as u64, now],
                    )
                    .map_err(sqlite_error)?;
            }
            None => {
                self.ensure_parent_dir(path)?;
                self.insert_entry(
                    path,
                    false,
                    Some(content),
                    mode.unwrap_or(DEFAULT_FILE_MODE),
                    None,
                )?;
            }
        }
        Ok(())
    }

    fn remove_entry(&self, op: &'static str, path: &str, expect_dir: bool) -> VfsResult<()> {
        if path == "/" {
            return Err(VfsError::new(
                "EBUSY",
                format!("cannot remove root directory, {op} '{path}'"),
            ));
        }
        let row = self.row_or_enoent(op, path)?;
        if expect_dir && !row.is_directory {
            return Err(VfsError::new(
                "ENOTDIR",
                format!("not a directory, {op} '{path}'"),
            ));
        }
        if !expect_dir && row.is_directory {
            return Err(VfsError::new(
                "EISDIR",
                format!("illegal operation on a directory, {op} '{path}'"),
            ));
        }
        if expect_dir && !self.direct_children(path)?.is_empty() {
            return Err(VfsError::new(
                "ENOTEMPTY",
                format!("directory not empty, rmdir '{path}'"),
            ));
        }
        self.conn
            .execute(
                "DELETE FROM agent_os_fs_entries WHERE path = ?1",
                params![path],
            )
            .map_err(sqlite_error)?;
        Ok(())
    }

    fn stat_row(row: FsRow) -> VirtualStat {
        VirtualStat {
            mode: row.mode,
            size: if row.is_directory {
                DIRECTORY_SIZE
            } else {
                row.size
            },
            blocks: row.size.div_ceil(512),
            dev: 1,
            rdev: 0,
            is_directory: row.is_directory,
            is_symbolic_link: row.symlink_target.is_some(),
            atime_ms: row.atime_ms,
            atime_nsec: 0,
            mtime_ms: row.mtime_ms,
            mtime_nsec: 0,
            ctime_ms: row.ctime_ms,
            ctime_nsec: 0,
            birthtime_ms: row.birthtime_ms,
            ino: stable_ino(&row.path),
            nlink: row.nlink,
            uid: row.uid,
            gid: row.gid,
        }
    }
}

impl VirtualFileSystem for SqliteVfsFilesystem {
    fn read_file(&mut self, path: &str) -> VfsResult<Vec<u8>> {
        let path = norm_path(path);
        let row = self.row_or_enoent("open", &path)?;
        if row.is_directory {
            return Err(VfsError::new(
                "EISDIR",
                format!("illegal operation on a directory, open '{path}'"),
            ));
        }
        Ok(row.content.unwrap_or_default())
    }

    fn read_dir(&mut self, path: &str) -> VfsResult<Vec<String>> {
        Ok(self
            .read_dir_with_types(path)?
            .into_iter()
            .map(|entry| entry.name)
            .collect())
    }

    fn read_dir_with_types(&mut self, path: &str) -> VfsResult<Vec<VirtualDirEntry>> {
        let path = norm_path(path);
        let row = self.row_or_enoent("scandir", &path)?;
        if !row.is_directory {
            return Err(VfsError::new(
                "ENOTDIR",
                format!("not a directory, scandir '{path}'"),
            ));
        }
        Ok(self
            .direct_children(&path)?
            .into_iter()
            .map(|child| VirtualDirEntry {
                name: child
                    .path
                    .rsplit('/')
                    .next()
                    .unwrap_or(child.path.as_str())
                    .to_owned(),
                is_directory: child.is_directory,
                is_symbolic_link: child.symlink_target.is_some(),
            })
            .collect())
    }

    fn write_file(&mut self, path: &str, content: impl Into<Vec<u8>>) -> VfsResult<()> {
        self.write_file_with_mode(path, content, None)
    }

    fn write_file_with_mode(
        &mut self,
        path: &str,
        content: impl Into<Vec<u8>>,
        mode: Option<u32>,
    ) -> VfsResult<()> {
        let path = norm_path(path);
        self.overwrite_file(&path, &content.into(), mode)
    }

    fn create_file_exclusive(&mut self, path: &str, content: impl Into<Vec<u8>>) -> VfsResult<()> {
        self.create_file_exclusive_with_mode(path, content, None)
    }

    fn create_file_exclusive_with_mode(
        &mut self,
        path: &str,
        content: impl Into<Vec<u8>>,
        mode: Option<u32>,
    ) -> VfsResult<()> {
        let path = norm_path(path);
        if self.exists(&path) {
            return Err(VfsError::new(
                "EEXIST",
                format!("file already exists, open '{path}'"),
            ));
        }
        self.ensure_parent_dir(&path)?;
        self.insert_entry(
            &path,
            false,
            Some(&content.into()),
            mode.unwrap_or(DEFAULT_FILE_MODE),
            None,
        )
    }

    fn create_dir(&mut self, path: &str) -> VfsResult<()> {
        self.create_dir_with_mode(path, None)
    }

    fn create_dir_with_mode(&mut self, path: &str, mode: Option<u32>) -> VfsResult<()> {
        let path = norm_path(path);
        if self.exists(&path) {
            return Err(VfsError::new(
                "EEXIST",
                format!("file already exists, mkdir '{path}'"),
            ));
        }
        self.ensure_parent_dir(&path)?;
        self.insert_entry(&path, true, None, mode.unwrap_or(DEFAULT_DIR_MODE), None)
    }

    fn mkdir(&mut self, path: &str, recursive: bool) -> VfsResult<()> {
        self.mkdir_with_mode(path, recursive, None)
    }

    fn mkdir_with_mode(&mut self, path: &str, recursive: bool, mode: Option<u32>) -> VfsResult<()> {
        let path = norm_path(path);
        if !recursive {
            return self.create_dir_with_mode(&path, mode);
        }
        let mut current = String::from("/");
        for segment in path
            .trim_start_matches('/')
            .split('/')
            .filter(|s| !s.is_empty())
        {
            current = if current == "/" {
                format!("/{segment}")
            } else {
                format!("{current}/{segment}")
            };
            match self.row(&current)? {
                Some(row) if row.is_directory => {}
                Some(_) => {
                    return Err(VfsError::new(
                        "ENOTDIR",
                        format!("not a directory, mkdir '{current}'"),
                    ));
                }
                None => {
                    self.insert_entry(&current, true, None, mode.unwrap_or(DEFAULT_DIR_MODE), None)?
                }
            }
        }
        Ok(())
    }

    fn exists(&self, path: &str) -> bool {
        self.row(&norm_path(path)).ok().flatten().is_some()
    }

    fn stat(&mut self, path: &str) -> VfsResult<VirtualStat> {
        let path = norm_path(path);
        let row = self.row_or_enoent("stat", &path)?;
        Ok(Self::stat_row(row))
    }

    fn remove_file(&mut self, path: &str) -> VfsResult<()> {
        self.remove_entry("unlink", &norm_path(path), false)
    }

    fn remove_dir(&mut self, path: &str) -> VfsResult<()> {
        self.remove_entry("rmdir", &norm_path(path), true)
    }

    fn rename(&mut self, old_path: &str, new_path: &str) -> VfsResult<()> {
        let old_path = norm_path(old_path);
        let new_path = norm_path(new_path);
        if old_path == "/" {
            return Err(VfsError::new("EBUSY", "cannot rename root directory"));
        }
        let old = self.row_or_enoent("rename", &old_path)?;
        self.ensure_parent_dir(&new_path)?;
        if let Some(existing) = self.row(&new_path)? {
            if existing.is_directory && !self.direct_children(&new_path)?.is_empty() {
                return Err(VfsError::new(
                    "ENOTEMPTY",
                    format!("directory not empty, rename '{new_path}'"),
                ));
            }
            self.conn
                .execute(
                    "DELETE FROM agent_os_fs_entries WHERE path = ?1",
                    params![new_path],
                )
                .map_err(sqlite_error)?;
        }

        let old_prefix = format!("{old_path}/");
        let new_prefix = format!("{new_path}/");
        self.conn
            .execute(
                "UPDATE agent_os_fs_entries SET path = ?2 WHERE path = ?1",
                params![old_path, new_path],
            )
            .map_err(sqlite_error)?;
        if old.is_directory {
            let pattern = format!("{old_prefix}%");
            let children: Vec<String> = {
                let mut stmt = self
                    .conn
                    .prepare(
                        "SELECT path FROM agent_os_fs_entries WHERE path LIKE ?1 ORDER BY path",
                    )
                    .map_err(sqlite_error)?;
                let rows = stmt
                    .query_map(params![pattern], |row| row.get::<_, String>(0))
                    .map_err(sqlite_error)?;
                rows.collect::<Result<Vec<_>, _>>().map_err(sqlite_error)?
            };
            for child in children {
                let renamed = child.replacen(&old_prefix, &new_prefix, 1);
                self.conn
                    .execute(
                        "UPDATE agent_os_fs_entries SET path = ?2 WHERE path = ?1",
                        params![child, renamed],
                    )
                    .map_err(sqlite_error)?;
            }
        }
        Ok(())
    }

    fn realpath(&self, path: &str) -> VfsResult<String> {
        let path = norm_path(path);
        self.row_or_enoent("realpath", &path)?;
        Ok(path)
    }

    fn symlink(&mut self, target: &str, link_path: &str) -> VfsResult<()> {
        let link_path = norm_path(link_path);
        if self.exists(&link_path) {
            return Err(VfsError::new(
                "EEXIST",
                format!("file already exists, symlink '{link_path}'"),
            ));
        }
        self.ensure_parent_dir(&link_path)?;
        self.insert_entry(&link_path, false, None, DEFAULT_SYMLINK_MODE, Some(target))
    }

    fn read_link(&self, path: &str) -> VfsResult<String> {
        let path = norm_path(path);
        let row = self.row_or_enoent("readlink", &path)?;
        row.symlink_target
            .ok_or_else(|| VfsError::new("EINVAL", format!("invalid argument, readlink '{path}'")))
    }

    fn lstat(&self, path: &str) -> VfsResult<VirtualStat> {
        let path = norm_path(path);
        let row = self.row_or_enoent("lstat", &path)?;
        Ok(Self::stat_row(row))
    }

    fn link(&mut self, _old_path: &str, _new_path: &str) -> VfsResult<()> {
        Err(VfsError::unsupported(
            "sqlite_vfs hard links are not supported",
        ))
    }

    fn chmod(&mut self, path: &str, mode: u32) -> VfsResult<()> {
        let path = norm_path(path);
        self.row_or_enoent("chmod", &path)?;
        self.conn
            .execute(
                "UPDATE agent_os_fs_entries SET mode = ?2, ctime_ms = ?3 WHERE path = ?1",
                params![path, mode, now_ms()],
            )
            .map_err(sqlite_error)?;
        Ok(())
    }

    fn chown(&mut self, path: &str, uid: u32, gid: u32) -> VfsResult<()> {
        let path = norm_path(path);
        self.row_or_enoent("chown", &path)?;
        self.conn
            .execute(
                "UPDATE agent_os_fs_entries SET uid = ?2, gid = ?3, ctime_ms = ?4 WHERE path = ?1",
                params![path, uid, gid, now_ms()],
            )
            .map_err(sqlite_error)?;
        Ok(())
    }

    fn utimes(&mut self, path: &str, atime_ms: u64, mtime_ms: u64) -> VfsResult<()> {
        let path = norm_path(path);
        self.row_or_enoent("utimes", &path)?;
        self.conn
            .execute(
                "UPDATE agent_os_fs_entries SET atime_ms = ?2, mtime_ms = ?3, ctime_ms = ?4 WHERE path = ?1",
                params![path, atime_ms, mtime_ms, now_ms()],
            )
            .map_err(sqlite_error)?;
        Ok(())
    }

    fn truncate(&mut self, path: &str, length: u64) -> VfsResult<()> {
        let path = norm_path(path);
        let row = self.row_or_enoent("truncate", &path)?;
        if row.is_directory {
            return Err(VfsError::new(
                "EISDIR",
                format!("illegal operation on a directory, truncate '{path}'"),
            ));
        }
        let mut content = row.content.unwrap_or_default();
        content.resize(
            usize::try_from(length).map_err(|_| {
                VfsError::new("ENOMEM", format!("truncate length too large: {length}"))
            })?,
            0,
        );
        self.overwrite_file(&path, &content, Some(row.mode))
    }

    fn pread(&mut self, path: &str, offset: u64, length: usize) -> VfsResult<Vec<u8>> {
        let content = self.read_file(path)?;
        let start = usize::try_from(offset)
            .map_err(|_| VfsError::new("EINVAL", format!("pread offset too large: {offset}")))?;
        if start >= content.len() {
            return Ok(Vec::new());
        }
        let end = start.saturating_add(length).min(content.len());
        Ok(content[start..end].to_vec())
    }
}

fn norm_path(path: &str) -> String {
    let normalized = normalize_path(path);
    if normalized.len() > 1 {
        normalized.trim_end_matches('/').to_owned()
    } else {
        normalized
    }
}

fn parent_path(path: &str) -> String {
    if path == "/" {
        return "/".to_owned();
    }
    let trimmed = path.trim_end_matches('/');
    match trimmed.rsplit_once('/') {
        Some(("", _)) => "/".to_owned(),
        Some((parent, _)) if !parent.is_empty() => parent.to_owned(),
        _ => "/".to_owned(),
    }
}

fn now_ms() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_millis() as u64)
        .unwrap_or(0)
}

fn sqlite_error(error: rusqlite::Error) -> VfsError {
    VfsError::new("EIO", format!("sqlite_vfs database error: {error}"))
}

fn estimated_base64_decoded_len(encoded: &str) -> Option<usize> {
    let padding = encoded
        .as_bytes()
        .iter()
        .rev()
        .take_while(|byte| **byte == b'=')
        .count()
        .min(2);
    encoded
        .len()
        .checked_add(3)
        .map(|length| (length / 4).saturating_mul(3).saturating_sub(padding))
}

fn check_read_length(
    operation: &str,
    path: &str,
    length: usize,
    max_bytes: Option<usize>,
) -> VfsResult<()> {
    if let Some(limit) = max_bytes {
        if length > limit {
            return Err(VfsError::new(
                "EINVAL",
                format!(
                    "sqlite_vfs callback payload length {length} exceeds configured read limit {limit}, {operation} '{path}'"
                ),
            ));
        }
    }
    Ok(())
}

fn stable_ino(path: &str) -> u64 {
    let mut hash = 1469598103934665603_u64;
    for byte in path.as_bytes() {
        hash ^= u64::from(*byte);
        hash = hash.wrapping_mul(1099511628211);
    }
    hash
}

#[cfg(test)]
mod tests {
    use super::*;
    use secure_exec_kernel::vfs::VirtualFileSystem;

    #[test]
    fn sqlite_vfs_round_trips_files_dirs_symlinks_and_metadata() {
        let mut fs = SqliteVfsFilesystem::memory().expect("memory fs");

        fs.mkdir("/tmp/nested", true).expect("mkdir -p");
        fs.write_file_with_mode(
            "/tmp/nested/a.txt",
            b"hello".to_vec(),
            Some(S_IFREG | 0o600),
        )
        .expect("write file");
        assert_eq!(fs.read_file("/tmp/nested/a.txt").expect("read"), b"hello");
        assert_eq!(fs.pread("/tmp/nested/a.txt", 1, 3).expect("pread"), b"ell");
        let stat = fs.stat("/tmp/nested/a.txt").expect("stat");
        assert_eq!(stat.mode, S_IFREG | 0o600);
        assert_eq!(stat.size, 5);

        fs.symlink("/tmp/nested/a.txt", "/tmp/nested/link")
            .expect("symlink");
        assert_eq!(
            fs.read_link("/tmp/nested/link").expect("readlink"),
            "/tmp/nested/a.txt"
        );
        assert!(
            fs.lstat("/tmp/nested/link")
                .expect("lstat")
                .is_symbolic_link
        );

        let entries = fs.read_dir_with_types("/tmp/nested").expect("readdir");
        assert!(entries.iter().any(|entry| entry.name == "a.txt"));
        assert!(entries
            .iter()
            .any(|entry| entry.name == "link" && entry.is_symbolic_link));

        fs.rename("/tmp/nested/a.txt", "/tmp/nested/b.txt")
            .expect("rename");
        assert!(!fs.exists("/tmp/nested/a.txt"));
        assert_eq!(fs.read_file("/tmp/nested/b.txt").expect("read b"), b"hello");

        fs.truncate("/tmp/nested/b.txt", 2).expect("truncate");
        assert_eq!(
            fs.read_file("/tmp/nested/b.txt").expect("read truncated"),
            b"he"
        );
    }

    #[test]
    fn sqlite_vfs_reports_expected_errno_shapes() {
        let mut fs = SqliteVfsFilesystem::memory().expect("memory fs");

        assert_eq!(
            fs.read_file("/missing").expect_err("missing").code(),
            "ENOENT"
        );
        fs.mkdir("/dir", false).expect("mkdir");
        assert_eq!(fs.read_file("/dir").expect_err("read dir").code(), "EISDIR");
        assert_eq!(
            fs.write_file("/dir/child/file.txt", b"x".to_vec())
                .expect_err("missing parent")
                .code(),
            "ENOENT"
        );
        fs.write_file("/dir/file.txt", b"x".to_vec())
            .expect("write child");
        assert_eq!(
            fs.remove_dir("/dir").expect_err("nonempty").code(),
            "ENOTEMPTY"
        );
        assert_eq!(
            fs.create_dir("/dir/file.txt").expect_err("exists").code(),
            "EEXIST"
        );
    }
}
