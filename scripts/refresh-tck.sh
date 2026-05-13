#!/usr/bin/env bash
# Refresh vendor/tck/ from upstream openCypher.
#
# Usage:
#   scripts/refresh-tck.sh            # bump to upstream HEAD
#   scripts/refresh-tck.sh <sha>      # pin to a specific commit
#
# Re-run angreal test tck after this and reconcile the new baseline.

set -euo pipefail

UPSTREAM_URL="https://github.com/opencypher/openCypher.git"
TARGET_SHA="${1:-}"

repo_root="$(git rev-parse --show-toplevel)"
vendor_dir="$repo_root/vendor/tck"
tmpdir="$(mktemp -d -t opencypher-tck-XXXXXX)"
trap 'rm -rf "$tmpdir"' EXIT

echo ">> Cloning openCypher (shallow, sparse tck/) into $tmpdir"
git clone --depth=1 --filter=blob:none --sparse "$UPSTREAM_URL" "$tmpdir/oc" >/dev/null
(cd "$tmpdir/oc" && git sparse-checkout set tck)

if [[ -n "$TARGET_SHA" ]]; then
  echo ">> Checking out $TARGET_SHA"
  (cd "$tmpdir/oc" && git fetch --depth=1 origin "$TARGET_SHA" && git checkout "$TARGET_SHA")
fi

pinned_sha="$(cd "$tmpdir/oc" && git rev-parse HEAD)"
today="$(date -u +%Y-%m-%d)"
echo ">> Pinned commit: $pinned_sha"

echo ">> Replacing vendor/tck/features and vendor/tck/graphs"
rm -rf "$vendor_dir/features" "$vendor_dir/graphs"
cp -R "$tmpdir/oc/tck/features" "$vendor_dir/features"
cp -R "$tmpdir/oc/tck/graphs"   "$vendor_dir/graphs"
cp    "$tmpdir/oc/tck/README.adoc" "$vendor_dir/UPSTREAM-README.adoc"
cp    "$tmpdir/oc/LICENSE"         "$vendor_dir/LICENSE"
cp    "$tmpdir/oc/NOTICE"          "$vendor_dir/NOTICE"

echo ">> Rewriting vendor/tck/UPSTREAM.md pin"
python3 - "$vendor_dir/UPSTREAM.md" "$pinned_sha" "$today" <<'PY'
import re, sys, pathlib
path, sha, date = sys.argv[1], sys.argv[2], sys.argv[3]
p = pathlib.Path(path)
text = p.read_text()
text = re.sub(r"(\*\*Pinned commit:\*\* `)[0-9a-f]+(`)", lambda m: f"{m.group(1)}{sha}{m.group(2)}", text)
text = re.sub(r"(\*\*Date pinned:\*\* )\d{4}-\d{2}-\d{2}", lambda m: f"{m.group(1)}{date}", text)
p.write_text(text)
PY

feature_count="$(find "$vendor_dir/features" -name '*.feature' | wc -l | tr -d ' ')"
echo ">> Done. $feature_count .feature files vendored."
echo ">> Next: run 'angreal test tck' and reconcile the new baseline."
