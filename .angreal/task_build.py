"""Build commands for GraphQLite.

Orchestrates compilation by delegating to Make for the actual build work.
Make handles dependency tracking and compilation; angreal provides the dev UX.
"""

import angreal
from utils import run_make

build = angreal.command_group(name="build", about="Build GraphQLite components")


@build()
@angreal.command(
    name="extension",
    about="Build the SQLite extension",
    tool=angreal.ToolDescription(
        """
Build the GraphQLite SQLite extension (.dylib on macOS, .so on Linux, .dll on Windows).

## When to use
- Before running tests that use the extension
- Before deploying or distributing the extension
- After making changes to C source files

## Examples
```
angreal build extension           # Debug build
angreal build extension --release # Optimized build
```

## Output
Creates `build/graphqlite.dylib` (or platform equivalent)
""",
        risk_level="safe"
    )
)
@angreal.argument(
    name="release",
    long="release",
    short="r",
    is_flag=True,
    takes_value=False,
    help="Build optimized release version (default: debug)"
)
@angreal.argument(
    name="verbose",
    long="verbose",
    short="v",
    is_flag=True,
    takes_value=False,
    help="Show build commands"
)
def build_extension(release: bool = False, verbose: bool = False) -> int:
    """Build the SQLite extension."""
    mode = "release" if release else "debug"
    print(f"Building extension ({mode})...")

    result = run_make("extension", release=release, verbose=verbose)

    if result == 0:
        print("Extension built successfully!")
    else:
        print("Build failed!")

    return result


@build()
@angreal.command(
    name="app",
    about="Build the main gqlite application",
    tool=angreal.ToolDescription(
        """
Build the main GraphQLite interactive application (gqlite).

## When to use
- When you need the standalone CLI tool
- For interactive Cypher query testing

## Examples
```
angreal build app           # Debug build
angreal build app --release # Optimized build
```

## Output
Creates `build/gqlite` executable
""",
        risk_level="safe"
    )
)
@angreal.argument(
    name="release",
    long="release",
    short="r",
    is_flag=True,
    takes_value=False,
    help="Build optimized release version"
)
@angreal.argument(
    name="verbose",
    long="verbose",
    short="v",
    is_flag=True,
    takes_value=False,
    help="Show build commands"
)
def build_app(release: bool = False, verbose: bool = False) -> int:
    """Build the main application."""
    mode = "release" if release else "debug"
    print(f"Building gqlite application ({mode})...")

    result = run_make("graphqlite", release=release, verbose=verbose)

    if result == 0:
        print("Application built successfully!")
    else:
        print("Build failed!")

    return result


@build()
@angreal.command(
    name="all",
    about="Build all components",
    tool=angreal.ToolDescription(
        """
Build all GraphQLite components (extension and application).

## When to use
- Full project build
- CI/CD pipelines
- After fresh clone or major changes

## Examples
```
angreal build all
angreal build all --release
```
""",
        risk_level="safe"
    )
)
@angreal.argument(
    name="release",
    long="release",
    short="r",
    is_flag=True,
    takes_value=False,
    help="Build optimized release versions"
)
@angreal.argument(
    name="verbose",
    long="verbose",
    short="v",
    is_flag=True,
    takes_value=False,
    help="Show build commands"
)
def build_all(release: bool = False, verbose: bool = False) -> int:
    """Build all components."""
    mode = "release" if release else "debug"
    print(f"Building all components ({mode})...")

    result = run_make("extension", release=release, verbose=verbose)
    if result != 0:
        print("Extension build failed!")
        return result

    result = run_make("graphqlite", release=release, verbose=verbose)
    if result != 0:
        print("Application build failed!")
        return result

    print("All components built successfully!")
    return 0
