"""Platform detection and extension path resolution."""

import os
import platform
from pathlib import Path
from typing import Optional


def get_extension_name() -> str:
    """Get the platform-specific extension filename."""
    system = platform.system()

    if system == "Darwin":
        return "graphqlite.dylib"
    elif system == "Linux":
        return "graphqlite.so"
    elif system == "Windows":
        return "graphqlite.dll"
    else:
        raise OSError(f"Unsupported platform: {system}")


def get_extension_search_paths() -> list[Path]:
    """Get ordered list of paths to search for the extension."""
    ext_name = get_extension_name()
    package_dir = Path(__file__).parent

    paths = [
        # Bundled with package
        package_dir / ext_name,
        # Development build
        package_dir.parent.parent.parent.parent / "build" / ext_name,
        # System-wide
        Path("/usr/local/lib") / ext_name,
        Path("/usr/lib") / ext_name,
    ]

    # Check environment variable first
    env_path = os.environ.get("GRAPHQLITE_EXTENSION_PATH")
    if env_path:
        paths.insert(0, Path(env_path))

    return paths


def find_extension(extension_path: Optional[str] = None) -> str:
    """
    Find the GraphQLite extension library.

    Args:
        extension_path: Explicit path to extension (skips search if provided)

    Returns:
        Full path to the extension file

    Raises:
        FileNotFoundError: If extension cannot be found
    """
    if extension_path:
        path = Path(extension_path)
        if path.exists():
            return str(path.resolve())
        raise FileNotFoundError(f"Extension not found at specified path: {extension_path}")

    search_paths = get_extension_search_paths()

    for path in search_paths:
        if path.exists():
            return str(path.resolve())

    raise FileNotFoundError(
        f"GraphQLite extension not found. Searched: {[str(p) for p in search_paths]}\n"
        f"Set GRAPHQLITE_EXTENSION_PATH or build the extension with 'make extension'"
    )


def get_loadable_path(extension_path: Optional[str] = None) -> str:
    """
    Get extension path in SQLite-loadable format (without file extension).

    Args:
        extension_path: Explicit path to extension (optional)

    Returns:
        Path suitable for sqlite3.Connection.load_extension()
    """
    full_path = find_extension(extension_path)
    ext_path = Path(full_path)
    return str(ext_path.parent / ext_path.stem)
