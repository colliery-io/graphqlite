# Linux image for the libFuzzer target (GQLITE-T-0496): Apple clang has no
# libFuzzer. Ubuntu 24.04: clang 18 with libFuzzer, SQLite 3.45 (as CI Linux).
#   docker build -t graphqlite-fuzz tests/fuzz
#   angreal test fuzz --docker
FROM ubuntu:24.04
RUN apt-get update \
 && apt-get install -y --no-install-recommends clang libclang-rt-18-dev llvm make bison flex \
      libsqlite3-dev python3 ca-certificates \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
