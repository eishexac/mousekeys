#!/bin/sh
# release.sh — cut a signed release tag, which triggers the CI app build.
#
# Gates on the tests, then creates a PGP-signed tag matching the Makefile
# version and pushes it. The GitHub "release" workflow takes it from there:
# it builds, Developer ID-signs (and notarizes) mousekeys.app, uploads it to
# the release, and updates the tap cask. Signing the tag needs your GPG key, so run
# this in a terminal where gpg can prompt for the passphrase.
#
# Usage:
#   scripts/release.sh            # tag v<VERSION> and push
#   scripts/release.sh --retag    # replace an existing v<VERSION> tag
#
# Bump the version in the Makefile (VERSION) before releasing.
set -eu

cd "$(dirname "$0")/.."

retag=0
[ "${1:-}" = "--retag" ] && retag=1

version="$(make -s version)"
tag="v$version"

branch="$(git rev-parse --abbrev-ref HEAD)"
[ "$branch" = "dev" ] || { echo "release: on '$branch', expected 'dev'" >&2; exit 1; }
[ -z "$(git status --porcelain)" ] || { echo "release: working tree is not clean" >&2; exit 1; }

# Gate on the build and tests before touching any tags.
make check
make build

# The tag must not already exist unless --retag replaces it. Only the local
# and remote tags are removed; the signed object is recreated below.
if git rev-parse -q --verify "refs/tags/$tag" >/dev/null 2>&1; then
  if [ "$retag" -eq 1 ]; then
    git tag -d "$tag"
    git push --delete origin "$tag" 2>/dev/null || true
  else
    echo "release: tag $tag already exists (use --retag to replace it)" >&2
    exit 1
  fi
fi

git tag -s "$tag" -m "mousekeys $version"   # prompts for your GPG passphrase
git push origin "$tag"                       # push fires the release workflow

echo "release: pushed signed $tag"
echo "release: verify with  git verify-tag $tag"
echo "release: the 'release' workflow now builds and ships the signed .app cask —"
echo "release: watch it with  gh run watch  or the repository's Actions tab."
