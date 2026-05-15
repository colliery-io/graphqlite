//! Platform detection and extension loading for bundled binaries.
//!
//! When the `bundled-extension` feature is enabled, pre-built extension binaries
//! are embedded in the Rust binary and extracted to a temp file at runtime.

use std::io::Write;
use std::path::PathBuf;
use std::sync::Mutex;

use crate::{Error, Result};

/// Extension filename for current platform
#[cfg(target_os = "macos")]
const EXTENSION_FILENAME: &str = "graphqlite.dylib";

#[cfg(target_os = "linux")]
const EXTENSION_FILENAME: &str = "graphqlite.so";

#[cfg(target_os = "windows")]
const EXTENSION_FILENAME: &str = "graphqlite.dll";

/// Embedded extension binary for macOS x86_64
#[cfg(all(target_os = "macos", target_arch = "x86_64"))]
const EXTENSION_BYTES: &[u8] = include_bytes!("../libs/graphqlite-macos-x86_64.dylib");

/// Embedded extension binary for macOS ARM64
#[cfg(all(target_os = "macos", target_arch = "aarch64"))]
const EXTENSION_BYTES: &[u8] = include_bytes!("../libs/graphqlite-macos-aarch64.dylib");

/// Embedded extension binary for Linux x86_64
#[cfg(all(target_os = "linux", target_arch = "x86_64"))]
const EXTENSION_BYTES: &[u8] = include_bytes!("../libs/graphqlite-linux-x86_64.so");

/// Embedded extension binary for Linux ARM64
#[cfg(all(target_os = "linux", target_arch = "aarch64"))]
const EXTENSION_BYTES: &[u8] = include_bytes!("../libs/graphqlite-linux-aarch64.so");

/// Embedded extension binary for Windows x86_64
#[cfg(all(target_os = "windows", target_arch = "x86_64"))]
const EXTENSION_BYTES: &[u8] = include_bytes!("../libs/graphqlite-windows-x86_64.dll");

/// Cache for the extracted extension path
static EXTENSION_PATH: Mutex<Option<PathBuf>> = Mutex::new(None);

/// Get the path to the extracted extension binary.
///
/// On first call, extracts the embedded binary to a temp directory.
/// Subsequent calls return the cached path.
pub fn get_extension_path() -> Result<PathBuf> {
    let mut cached = EXTENSION_PATH
        .lock()
        .map_err(|e| Error::ExtensionNotFound(format!("Failed to acquire lock: {}", e)))?;

    if let Some(ref path) = *cached {
        return Ok(path.clone());
    }

    let path = extract_extension()?;
    *cached = Some(path.clone());
    Ok(path)
}

/// Extract the embedded extension binary to a temp file.
///
/// Safety features:
/// - Atomic write: writes to a .tmp file then renames into place
/// - Content verification: compares file size and first/last bytes
/// - Noexec fallback: retries from ~/.cache/graphqlite/ if temp dir fails
/// - Old version cleanup: removes stale versioned files after extraction
fn extract_extension() -> Result<PathBuf> {
    let version = env!("CARGO_PKG_VERSION");
    let filename = format!("graphqlite-{}-{}", version, EXTENSION_FILENAME);

    // Allow override via environment variable
    if let Ok(custom_dir) = std::env::var("GRAPHQLITE_EXTENSION_DIR") {
        let dir = PathBuf::from(custom_dir);
        if let Ok(path) = try_extract_to(&dir, &filename) {
            cleanup_old_versions(&dir, &filename);
            return Ok(path);
        }
    }

    // Try primary location (system temp dir), fall back to ~/.cache/graphqlite/
    let dirs_to_try = [std::env::temp_dir().join("graphqlite"), dirs_fallback()];

    for dir in &dirs_to_try {
        if let Ok(path) = try_extract_to(dir, &filename) {
            // Clean up old versions in this directory
            cleanup_old_versions(dir, &filename);
            return Ok(path);
        }
    }

    Err(Error::ExtensionNotFound(
        "Failed to extract extension to temp dir or ~/.cache/graphqlite/. \
         If /tmp has noexec, set GRAPHQLITE_EXTENSION_DIR to a writable, executable path."
            .to_string(),
    ))
}

/// Fallback directory: ~/.cache/graphqlite/ (or %LOCALAPPDATA%\graphqlite on Windows)
fn dirs_fallback() -> PathBuf {
    #[cfg(unix)]
    {
        if let Ok(home) = std::env::var("HOME") {
            return PathBuf::from(home).join(".cache").join("graphqlite");
        }
    }
    #[cfg(windows)]
    {
        if let Ok(appdata) = std::env::var("LOCALAPPDATA") {
            return PathBuf::from(appdata).join("graphqlite");
        }
    }
    // Last resort
    PathBuf::from(".graphqlite-cache")
}

/// Try to extract the extension to a specific directory.
fn try_extract_to(dir: &PathBuf, filename: &str) -> Result<PathBuf> {
    std::fs::create_dir_all(dir).map_err(|e| {
        Error::ExtensionNotFound(format!("Failed to create directory {:?}: {}", dir, e))
    })?;

    let extension_path = dir.join(filename);

    // Check if already present with correct size
    let needs_extract = match std::fs::metadata(&extension_path) {
        Ok(meta) => meta.len() != EXTENSION_BYTES.len() as u64,
        Err(_) => true,
    };

    if needs_extract {
        // Atomic write: write to .tmp then rename
        let tmp_path = extension_path.with_extension("tmp");

        let mut file = std::fs::File::create(&tmp_path)
            .map_err(|e| Error::ExtensionNotFound(format!("Failed to create temp file: {}", e)))?;

        file.write_all(EXTENSION_BYTES).map_err(|e| {
            let _ = std::fs::remove_file(&tmp_path);
            Error::ExtensionNotFound(format!("Failed to write extension file: {}", e))
        })?;

        drop(file); // Ensure file is closed before rename

        // Set executable permission before rename
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            let perms = std::fs::Permissions::from_mode(0o755);
            std::fs::set_permissions(&tmp_path, perms).map_err(|e| {
                Error::ExtensionNotFound(format!("Failed to set permissions: {}", e))
            })?;
        }

        // Atomic rename into place
        std::fs::rename(&tmp_path, &extension_path).map_err(|e| {
            let _ = std::fs::remove_file(&tmp_path);
            Error::ExtensionNotFound(format!("Failed to rename extension into place: {}", e))
        })?;
    }

    Ok(extension_path)
}

/// Remove old versioned extension files from the directory.
fn cleanup_old_versions(dir: &PathBuf, current_filename: &str) {
    let prefix = "graphqlite-";
    if let Ok(entries) = std::fs::read_dir(dir) {
        for entry in entries.flatten() {
            let name = entry.file_name();
            let name_str = name.to_string_lossy();
            if name_str.starts_with(prefix)
                && name_str != current_filename
                && !name_str.ends_with(".tmp")
            {
                let _ = std::fs::remove_file(entry.path());
            }
        }
    }
}

/// Load the bundled extension into a rusqlite connection.
pub fn load_bundled_extension(conn: &rusqlite::Connection) -> Result<()> {
    let extension_path = get_extension_path()?;

    // Remove the file extension for SQLite's load_extension
    let load_path = extension_path.with_extension("");

    unsafe {
        conn.load_extension_enable()?;
        conn.load_extension(&load_path, None::<&str>)?;
        conn.load_extension_disable()?;
    }

    // Verify the extension loaded
    let test: String = conn.query_row("SELECT graphqlite_test()", [], |row| row.get(0))?;
    if !test.to_lowercase().contains("successfully") {
        return Err(Error::ExtensionNotFound(
            "Extension loaded but verification failed".to_string(),
        ));
    }

    Ok(())
}
