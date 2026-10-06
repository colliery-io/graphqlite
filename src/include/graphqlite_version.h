/*
 * graphqlite_version.h — single source of truth for the extension's own
 * version and the capabilities() schema (GQLITE-T-0100).
 *
 * Release procedure: bump GRAPHQLITE_VERSION here together with
 * bindings/rust/Cargo.toml, bindings/python/src/graphqlite/__init__.py and
 * CHANGELOG.md.
 */
#ifndef GRAPHQLITE_VERSION_H
#define GRAPHQLITE_VERSION_H

#define GRAPHQLITE_VERSION "0.9.1"

/* Bumped whenever a key is added to, renamed in or removed from the JSON
 * document returned by cypher_capabilities(). Adding a feature flag does
 * NOT bump it; clients must treat unknown flags as absent/false. */
#define GRAPHQLITE_CAPABILITIES_SCHEMA 1

#define GRAPHQLITE_CYPHER_DIALECT "openCypher 9"

#endif /* GRAPHQLITE_VERSION_H */
