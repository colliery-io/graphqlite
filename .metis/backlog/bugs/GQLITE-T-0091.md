---
id: rust-bindings-should-build-c
level: task
title: "Rust bindings should build C extension from source via build.rs"
short_code: "GQLITE-T-0091"
created_at: 2026-01-08T14:46:05.846330+00:00
updated_at: 2026-01-08T17:13:38.199570+00:00
parent: 
blocked_by: []
archived: false

tags:
  - "#task"
  - "#bug"
  - "#phase/completed"


exit_criteria_met: false
strategy_id: NULL
initiative_id: NULL
---

# Rust bindings should build C extension from source via build.rs

**Target Release**: 0.2.1 (blocker for GPU initiative)

## Objective

Enable `cargo add graphqlite` to work out of the box without requiring users to manually build and install the C extension. The Rust bindings should compile the C extension from vendored source during `cargo build`.

## Backlog Item Details

### Type
- [x] Bug - Production issue that needs fixing

### Priority
- [x] P1 - High (important for user experience)

### Impact Assessment
- **Affected Users**: All Rust users (100%)
- **Reproduction Steps**: 
  1. `cargo new test-project && cd test-project`
  2. `cargo add graphqlite`
  3. `cargo build`
- **Expected vs Actual**: 
  - **Expected**: Build succeeds, library ready to use
  - **Actual**: Linker error - cannot find graphqlite library. User must manually build C extension and set `LIBRARY_PATH` or equivalent.

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

## Acceptance Criteria

- [ ] `cargo add graphqlite && cargo build` succeeds on clean machine (no pre-installed graphqlite)
- [ ] C source files vendored in `bindings/rust/` or referenced via path
- [ ] `build.rs` compiles C extension using `cc` crate
- [ ] Parser (Flex/Bison generated files) either pre-generated or generated at build time
- [ ] Works on macOS, Linux, and Windows
- [ ] Existing API unchanged - purely build system fix
- [ ] CI validates fresh-machine build scenario

## Implementation Notes

### Technical Approach

**1. Vendor C source** (or reference via relative path):
```
bindings/rust/
├── Cargo.toml
├── build.rs          # NEW
├── src/
│   └── lib.rs
└── vendor/           # Symlink or copy of C sources
    ├── parser/
    ├── backend/
    └── ...
```

**2. Create build.rs** using `cc` crate:
```rust
// build.rs
fn main() {
    println!("cargo:rerun-if-changed=vendor/");
    
    cc::Build::new()
        .files(glob::glob("vendor/**/*.c").unwrap().filter_map(Result::ok))
        .include("vendor/include")
        .include("vendor/sqlite")
        .compile("graphqlite");
}
```

**3. Handle Flex/Bison outputs**:
- Option A: Pre-generate `lexer.c` and `parser.c`, commit to repo
- Option B: Use `lalrpop` or similar Rust-native parser (larger change)
- **Recommended**: Option A - pre-generate and commit

**4. Cargo.toml build dependencies**:
```toml
[build-dependencies]
cc = "1.0"
glob = "0.3"
```

### Dependencies

- Flex/Bison generated files must be committed (or build machine needs flex/bison)
- SQLite amalgamation already vendored

### Risk Considerations

- **Cross-compilation**: `cc` crate handles most cases, but may need testing
- **Windows MSVC vs MinGW**: Current Makefile targets MinGW; may need MSVC support
- **Build time increase**: Compiling C from source adds ~10-30s to first build (acceptable)

### Reference Implementation

See how `rusqlite` handles this:
- Vendors SQLite amalgamation
- Uses `cc` crate in build.rs
- Provides feature flags for bundled vs system SQLite

## Status Updates

*To be added during implementation*