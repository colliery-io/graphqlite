//! Build script for graphqlite Rust bindings.
//!
//! When the `bundled-extension` feature is enabled, pre-built extension binaries
//! are embedded directly in the Rust binary via include_bytes!() in platform.rs.
//! No C compilation is needed at build time.

fn main() {
    // Tell cargo to re-run if the bundled libraries change
    #[cfg(feature = "bundled-extension")]
    {
        println!("cargo:rerun-if-changed=libs/");
    }
}
