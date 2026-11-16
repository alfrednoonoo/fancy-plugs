#!/bin/bash
# Downloads the newest successful build of the current branch and installs its Mac bundles with
# install.sh (docs/mac.md, step 4). Needs the GitHub CLI (gh), signed in.

set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(dirname "$script_dir")"
branch="$(git -C "$repo_root" rev-parse --abbrev-ref HEAD)"

run_id="$(gh run list --workflow build.yml --branch "$branch" --status success --limit 1 --json databaseId --jq '.[0].databaseId')"

if [ -z "$run_id" ]; then
    echo "No successful build found for branch '$branch'" >&2
    exit 1
fi

download_dir="${TMPDIR:-/tmp}/fancy-plugs-$run_id"
rm -rf "$download_dir"

echo "Downloading build $run_id of '$branch'..."
gh run download "$run_id" --pattern 'FancyPlugs-macos-*' --dir "$download_dir"

"$script_dir/install.sh" "$download_dir"

rm -rf "$download_dir"
