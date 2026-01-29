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
    let mut cached = EXTENSION_PATH.lock().map_err(|e| {
        Error::ExtensionNotFound(format!("Failed to acquire lock: {}", e))
    })?;

    if let Some(ref path) = *cached {
        return Ok(path.clone());
    }

    let path = extract_extension()?;
    *cached = Some(path.clone());
    Ok(path)
}

/// Extract the embedded extension binary to a temp file.
fn extract_extension() -> Result<PathBuf> {
    // Create a directory in the system temp dir for graphqlite
    let temp_dir = std::env::temp_dir().join("graphqlite");
    std::fs::create_dir_all(&temp_dir).map_err(|e| {
        Error::ExtensionNotFound(format!("Failed to create temp directory: {}", e))
    })?;

    // Use a versioned filename to handle upgrades
    let version = env!("CARGO_PKG_VERSION");
    let filename = format!("graphqlite-{}-{}", version, EXTENSION_FILENAME);
    let extension_path = temp_dir.join(&filename);

    // Only extract if not already present (or different size)
    let needs_extract = match std::fs::metadata(&extension_path) {
        Ok(meta) => meta.len() != EXTENSION_BYTES.len() as u64,
        Err(_) => true,
    };

    if needs_extract {
        let mut file = std::fs::File::create(&extension_path).map_err(|e| {
            Error::ExtensionNotFound(format!("Failed to create extension file: {}", e))
        })?;

        file.write_all(EXTENSION_BYTES).map_err(|e| {
            Error::ExtensionNotFound(format!("Failed to write extension file: {}", e))
        })?;

        // On Unix, make the file executable
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            let mut perms = std::fs::metadata(&extension_path)
                .map_err(|e| Error::ExtensionNotFound(format!("Failed to get permissions: {}", e)))?
                .permissions();
            perms.set_mode(0o755);
            std::fs::set_permissions(&extension_path, perms)
                .map_err(|e| Error::ExtensionNotFound(format!("Failed to set permissions: {}", e)))?;
        }
    }

    Ok(extension_path)
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
