# vendor/tck/

Vendored snapshot of the [openCypher TCK](https://github.com/opencypher/openCypher).
See [UPSTREAM.md](./UPSTREAM.md) for the pinned commit, source, and refresh
procedure.

This directory exists so `angreal test tck` can run conformance scenarios
without network access. Do not edit the files under `features/` or `graphs/`
in place — they are upstream artifacts.

To refresh:

```
scripts/refresh-tck.sh
```

License: Apache-2.0 (see `LICENSE` and `NOTICE`).
