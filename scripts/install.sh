#!/bin/bash
# Installs Fancy Plugs on a Mac (docs/mac.md, step 4): copies every .vst3 and .component bundle
# found under the source folder (default: the folder this script is in) into the user's own
# plug-in folders, so no administrator password is needed:
#
#   ~/Library/Audio/Plug-Ins/VST3         VST3 (REAPER, Live, Cubase, Bitwig, Studio One)
#   ~/Library/Audio/Plug-Ins/Components   Audio Units (Logic Pro, GarageBand)
#
# The builds aren't signed with an Apple Developer ID (docs/mac.md, Decision 2 A), so the script
# removes the quarantine flag macOS puts on downloaded files; without that, macOS refuses to load
# them. FancyBench, AudioPluginHost and the bench examples go to ~/Library/Application Support/
# Fancy Plugs/Tools.
#
#   ./install.sh [source folder]

set -euo pipefail

source_dir="${1:-$(cd "$(dirname "$0")" && pwd)}"
vst3_dir="$HOME/Library/Audio/Plug-Ins/VST3"
au_dir="$HOME/Library/Audio/Plug-Ins/Components"
tools_dir="$HOME/Library/Application Support/Fancy Plugs/Tools"

if [ ! -d "$source_dir" ]; then
    echo "install.sh: no folder $source_dir" >&2
    exit 1
fi

mkdir -p "$vst3_dir" "$au_dir"

installed=0

install_bundle() {
    local bundle="$1" target_dir="$2"
    local name
    name="$(basename "$bundle")"

    # Replace, never merge: a merged bundle can hold files from two builds.
    rm -rf "${target_dir:?}/$name"
    cp -R "$bundle" "$target_dir/"
    # GitHub's build artifacts don't keep the executable bit; release zips do. Set it either way.
    chmod +x "$target_dir/$name"/Contents/MacOS/* 2>/dev/null || true
    xattr -dr com.apple.quarantine "$target_dir/$name" 2>/dev/null || true
    echo "  $name -> $target_dir"
    installed=$((installed + 1))
}

echo "Installing Fancy Plugs from $source_dir"

while IFS= read -r -d '' bundle; do
    install_bundle "$bundle" "$vst3_dir"
done < <(find "$source_dir" -type d -name '*.vst3' -prune -print0)

while IFS= read -r -d '' bundle; do
    install_bundle "$bundle" "$au_dir"
done < <(find "$source_dir" -type d -name '*.component' -prune -print0)

if [ "$installed" -eq 0 ]; then
    echo "install.sh: no .vst3 or .component bundles under $source_dir" >&2
    exit 1
fi

# Tools, if this is a full build folder.
tools_source="$(find "$source_dir" -type d -name tools -not -path '*.vst3/*' -not -path '*.component/*' | head -1)"

if [ -n "$tools_source" ]; then
    mkdir -p "$tools_dir"
    for tool in "$tools_source"/FancyBench "$tools_source"/AudioPluginHost.app; do
        if [ -e "$tool" ]; then
            rm -rf "${tools_dir:?}/$(basename "$tool")"
            cp -R "$tool" "$tools_dir/"
            if [ -d "$tools_dir/$(basename "$tool")" ]; then
                chmod +x "$tools_dir/$(basename "$tool")"/Contents/MacOS/* 2>/dev/null || true
            else
                chmod +x "$tools_dir/$(basename "$tool")"
            fi
            xattr -dr com.apple.quarantine "$tools_dir/$(basename "$tool")" 2>/dev/null || true
        fi
    done
    if [ -d "$tools_source/examples" ]; then
        mkdir -p "$tools_dir/examples"
        # Keep the user's edited copies.
        for example in "$tools_source"/examples/*.json; do
            [ -e "$tools_dir/examples/$(basename "$example")" ] || cp "$example" "$tools_dir/examples/"
        done
    fi
    echo "  tools -> $tools_dir"
fi

# Logic and GarageBand read Audio Units through a system cache; restarting its registrar makes
# them look again the next time they start.
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo "Installed $installed bundles. Restart your DAW, or rescan its plug-ins."
