"""
GraphQLite Graph Storage - Re-export from graphqlite bindings.

This module re-exports the Graph class from graphqlite for backwards compatibility.
The implementation now lives in the core graphqlite package.
"""

from graphqlite import Graph as GraphQLiteGraph

__all__ = ["GraphQLiteGraph"]
