"""Shared utilities for GraphQLite angreal tasks."""

import subprocess
import os
import angreal


def get_project_root() -> str:
    """Get the project root (parent of .angreal directory)."""
    return os.path.dirname(angreal.get_root())


def run_make(target: str, release: bool = False, verbose: bool = False, **env_vars) -> int:
    """Run a make target with optional flags.

    Args:
        target: The make target to run
        release: If True, adds RELEASE=1 to build optimized
        verbose: If True, prints the command being run
        **env_vars: Additional environment variables to pass to make (e.g., PYTHON=python3.12)

    Returns:
        The return code from make
    """
    root = get_project_root()
    cmd = ["make", target]

    if release:
        cmd.append("RELEASE=1")

    for key, value in env_vars.items():
        cmd.append(f"{key}={value}")

    if verbose:
        print(f"Running: {' '.join(cmd)}")

    result = subprocess.run(cmd, cwd=root)
    return result.returncode


def ensure_extension_built() -> bool:
    """Check if the extension is built, build if not.

    Returns:
        True if extension exists or was built successfully, False otherwise
    """
    root = get_project_root()

    extensions = ["build/graphqlite.dylib", "build/graphqlite.so", "build/graphqlite.dll"]
    extension_exists = any(os.path.exists(os.path.join(root, ext)) for ext in extensions)

    if not extension_exists:
        print("Extension not built. Building...")
        result = run_make("extension")
        if result != 0:
            print("Failed to build extension!")
            return False
    return True
