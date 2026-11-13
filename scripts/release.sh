#!/usr/bin/env bash
# Cut a SemVer release from CHANGELOG.md (Linux/macOS). Windows equivalent: scripts/release.ps1.
#
#   scripts/release.sh [--dry-run] [--push] [-h|--help]
#
# Reads "## [Unreleased]" in CHANGELOG.md and picks the next vX.Y.Z from the change categories that have entries:
#   Breaking, Removed          -> major
#   Added, Deprecated          -> minor
#   Fixed, Changed, Security   -> patch
# Then promotes [Unreleased] to "## [X.Y.Z] - <today>", updates the compare links, writes X.Y.Z to VERSION (read by
# CMakeLists.txt), commits CHANGELOG.md + VERSION as "release vX.Y.Z" and creates the annotated tag vX.Y.Z.
# --push also pushes the branch and the tag (atomically) to $RELEASE_REMOTE (default: origin); pushing the tag
# triggers the GitHub release workflow.
set -euo pipefail

push=0
dry_run=0
for arg in "$@"; do
    case "$arg" in
        --push) push=1 ;;
        --dry-run) dry_run=1 ;;
        -h|--help) sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "error: unknown argument '$arg' (try --help)" >&2; exit 2 ;;
    esac
done

die() { echo "error: $*" >&2; exit 1; }

cd "$(git rev-parse --show-toplevel)"
changelog="CHANGELOG.md"
version_file="VERSION"
remote="${RELEASE_REMOTE:-origin}"
[ -f "$changelog" ] || die "$changelog not found"

# Work on an LF copy so the awk below is line-ending agnostic; restore CRLF on write if the file used it.
crlf=0
if grep -q $'\r' "$changelog"; then crlf=1; fi
work="$(mktemp)"
out="$(mktemp)"
trap 'rm -f "$work" "$out"' EXIT
tr -d '\r' < "$changelog" > "$work"

grep -q '^## \[Unreleased\][[:space:]]*$' "$work" || die "no '## [Unreleased]' heading in $changelog"

# Categories that contain at least one non-blank line, in file order ("(none)" = entries outside any ### heading).
categories="$(awk '
    /^## \[Unreleased\]/ { in_u = 1; next }
    in_u && (/^## \[/ || /^\[[^]]+\]:/) { exit }
    in_u && /^###[ \t]+/ { cur = $0; sub(/^###[ \t]+/, "", cur); sub(/[ \t]+$/, "", cur); next }
    in_u && /^[ \t]*$/ { next }
    in_u && /^<!--/ { next }
    in_u { name = (cur == "") ? "(none)" : cur; if (!(name in seen)) { seen[name] = 1; print name } }
' "$work")"

[ -n "$categories" ] || die "[Unreleased] has no entries; add some under a ### category first"

bump=""
raise() { # raise <level>: keep the highest of major > minor > patch
    case "$1:$bump" in
        major:*) bump=major ;;
        minor:major) ;;
        minor:*) bump=minor ;;
        patch:major|patch:minor) ;;
        patch:*) bump=patch ;;
    esac
}
while IFS= read -r category; do
    case "$category" in
        Breaking|Removed) raise major ;;
        Added|Deprecated) raise minor ;;
        Fixed|Changed|Security) raise patch ;;
        "(none)") die "entries in [Unreleased] must be placed under a ### category" ;;
        *) die "unknown category '### $category' in [Unreleased] (use Breaking, Added, Changed, Deprecated, Removed, Fixed, Security)" ;;
    esac
done <<< "$categories"

# Current version = highest of the latest vX.Y.Z tag and the newest "## [X.Y.Z]" heading.
current="$( { git tag --list 'v[0-9]*.[0-9]*.[0-9]*' | sed 's/^v//'
              sed -n 's/^## \[\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\)\].*/\1/p' "$work"; } \
            | grep -E '^[0-9]+\.[0-9]+\.[0-9]+$' | sort -t. -k1,1n -k2,2n -k3,3n | tail -n 1 || true)"
has_previous=1
if [ -z "$current" ]; then current="0.0.0"; has_previous=0; fi

IFS=. read -r major minor patch <<< "$current"
case "$bump" in
    major) major=$((major + 1)); minor=0; patch=0 ;;
    minor) minor=$((minor + 1)); patch=0 ;;
    patch) patch=$((patch + 1)) ;;
esac
version="$major.$minor.$patch"
tag="v$version"
today="$(date +%Y-%m-%d)"

echo "Categories : $(echo "$categories" | paste -sd, - | sed 's/,/, /g')"
echo "Release    : v$current -> $tag ($bump), $today"

if [ "$dry_run" -eq 1 ]; then
    echo "Dry run: nothing changed."
    exit 0
fi

git rev-parse -q --verify "refs/tags/$tag" > /dev/null && die "tag $tag already exists"
if ! git diff --quiet --cached; then die "the index has staged changes; commit or unstage them first"; fi
others="$(git status --porcelain --untracked-files=no | grep -v -E "^.. \"?($changelog|$version_file)\"?$" || true)"
[ -z "$others" ] || die "working tree has uncommitted changes besides $changelog and $version_file:"$'\n'"$others"

awk -v ver="$version" -v tag="$tag" -v prev="v$current" -v has_prev="$has_previous" -v date="$today" '
    function flush(   c, i, first, last) {
        print "## [" ver "] - " date
        print ""
        for (c = 1; c <= nc; c++) {
            first = 1; last = cnt[c]
            while (first <= last && body[c, first] ~ /^[ \t]*$/) first++
            while (last >= first && body[c, last] ~ /^[ \t]*$/) last--
            if (first > last) continue
            print head[c]
            print ""
            for (i = first; i <= last; i++) print body[c, i]
            print ""
        }
    }
    state == 0 && /^## \[Unreleased\]/ { print; print ""; state = 1; next }
    state == 1 && (/^## \[/ || /^\[[^]]+\]:/) { flush(); state = 2 }
    state == 1 {
        if ($0 ~ /^###[ \t]+/) { nc++; head[nc] = $0; cnt[nc] = 0 }
        else if (nc > 0) body[nc, ++cnt[nc]] = $0
        next
    }
    /^\[Unreleased\]:[ \t]/ && index($0, "/compare/") {
        base = $0; sub(/^\[Unreleased\]:[ \t]+/, "", base); sub(/\/compare\/.*$/, "", base)
        print "[Unreleased]: " base "/compare/" tag "...HEAD"
        if (has_prev == 1) print "[" ver "]: " base "/compare/" prev "..." tag
        else print "[" ver "]: " base "/releases/tag/" tag
        next
    }
    { print }
    END { if (state == 1) flush() }
' "$work" > "$out"

if [ "$crlf" -eq 1 ]; then sed 's/$/\r/' "$out" > "$changelog"; else cp "$out" "$changelog"; fi

printf '%s\n' "$version" > "$version_file"

git add -- "$changelog" "$version_file"
git commit -m "release $tag" -- "$changelog" "$version_file"
git tag -a "$tag" -m "release $tag"
echo "Created commit and annotated tag $tag."

if [ "$push" -eq 1 ]; then
    git push --atomic "$remote" HEAD "refs/tags/$tag"
    echo "Pushed HEAD and $tag to $remote."
else
    echo "Not pushed. To publish: git push --atomic $remote HEAD refs/tags/$tag   (or rerun with --push)"
fi
