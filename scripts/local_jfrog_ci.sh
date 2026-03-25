#!/usr/bin/env bash
# Mirrors the GitHub Actions JFrog/Conan/Xray path for local testing:
# Conan remotes → install → CMake build → collect Build Info → audit/scan → upload + build-publish.
# Requires: jf, conan (~2), cmake, python3, git (repo root).
#
# Usage:
#   cp scripts/local_jfrog_ci.env.example scripts/local_jfrog_ci.env
#   # edit ...env with JF_URL, JF_ACCESS_TOKEN, JF_CONAN_USER
#   ./scripts/local_jfrog_ci.sh
#
# Options:
#   -e FILE   load env file (default: scripts/local_jfrog_ci.env if it exists)
#   -n NUM    build number (default: local-<epoch>)
#   --art-buildinfo    use conan-extensions art:build-info (after upload to LOCAL Conan repo)
#   --no-publish       do not rt upload / build-publish
#   --no-conan-upload  skip jf conan upload (incompatible with --art-buildinfo)
#   --no-scan          skip jf scan and jf audit

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

ENV_FILE=""
BUILD_NUMBER_OVERRIDE=""
NO_PUBLISH=0
NO_CONAN_UPLOAD=0
NO_SCAN=0
CONAN_ART_BUILDINFO_FLAG=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    -e) ENV_FILE="$2"; shift 2 ;;
    -n) BUILD_NUMBER_OVERRIDE="$2"; shift 2 ;;
    --art-buildinfo) CONAN_ART_BUILDINFO_FLAG=1; shift ;;
    --no-publish) NO_PUBLISH=1; shift ;;
    --no-conan-upload) NO_CONAN_UPLOAD=1; shift ;;
    --no-scan) NO_SCAN=1; shift ;;
    -h|--help)
      grep '^#' "$0" | head -20 | sed 's/^# //' | sed 's/^#//'
      exit 0
      ;;
    *) echo "Unknown option: $1" >&2; exit 2 ;;
  esac
done

if [[ -z "$ENV_FILE" && -f "$ROOT/scripts/local_jfrog_ci.env" ]]; then
  ENV_FILE="$ROOT/scripts/local_jfrog_ci.env"
fi
if [[ -n "$ENV_FILE" ]]; then
  # shellcheck source=/dev/null
  set -a && source "$ENV_FILE" && set +a
fi

: "${JF_URL:?Set JF_URL (e.g. in scripts/local_jfrog_ci.env)}"
: "${JF_ACCESS_TOKEN:?Set JF_ACCESS_TOKEN}"
: "${JF_CONAN_USER:?Set JF_CONAN_USER for Conan remote login}"

JFROG_BUILD_NAME="${JFROG_BUILD_NAME:-pixelfrog}"
CONAN_VIRTUAL_REPO_KEY="${CONAN_VIRTUAL_REPO_KEY:-mwpf-conan-virtual-dev}"
CONAN_LOCAL_REPO_KEY="${CONAN_LOCAL_REPO_KEY:-mwpf-conan-local-dev}"
GENERIC_REPO_KEY="${GENERIC_REPO_KEY:-mwpf-generic-local}"

if [[ -n "$BUILD_NUMBER_OVERRIDE" ]]; then
  BUILD_NUMBER="$BUILD_NUMBER_OVERRIDE"
else
  BUILD_NUMBER="${BUILD_NUMBER:-local-$(date +%s)}"
fi

export JF_URL="${JF_URL%/}"
export JFROG_CLI_BUILD_NAME="$JFROG_BUILD_NAME"
export JFROG_CLI_BUILD_NUMBER="$BUILD_NUMBER"
export JFROG_BUILD_NAME="$JFROG_BUILD_NAME"
export GITHUB_RUN_NUMBER="$BUILD_NUMBER"
export GITHUB_WORKSPACE="$ROOT"
# manifest helper uses GITHUB_SHA when set; otherwise uses git rev-parse
export GITHUB_SHA="${GITHUB_SHA:-}"

BUILD_URL="${BUILD_URL:-local://$(hostname -s 2>/dev/null || hostname)/$BUILD_NUMBER}"

# CONAN_ART_BUILDINFO from env file or --art-buildinfo (see https://docs.conan.io/2/security/sboms.html)
CONAN_ART_BUILDINFO="${CONAN_ART_BUILDINFO:-0}"
[[ "$CONAN_ART_BUILDINFO_FLAG" -eq 1 ]] && CONAN_ART_BUILDINFO=1
if [[ "$CONAN_ART_BUILDINFO" -eq 1 && "$NO_CONAN_UPLOAD" -eq 1 ]]; then
  echo "error: --art-buildinfo requires uploading Conan packages to the local repo; remove --no-conan-upload" >&2
  exit 2
fi

command -v jf >/dev/null || { echo "jf (JFrog CLI) not found" >&2; exit 1; }
command -v conan >/dev/null || { echo "conan not found" >&2; exit 1; }
command -v cmake >/dev/null || { echo "cmake not found" >&2; exit 1; }
command -v python3 >/dev/null || { echo "python3 not found" >&2; exit 1; }

echo "==> JFrog config / ping"
if ! jf rt ping &>/dev/null; then
  jf config add local-pixelfrog-ci --overwrite \
    --url="$JF_URL" --interactive=false --access-token="$JF_ACCESS_TOKEN"
fi
jf rt ping

# Optional --project for jf; do not use a global empty array with set -u (unbound JF_EXTRA[@] on some Bash builds).
jf_with_project() {
  local -a cmd=(jf "$@")
  if [[ -n "${JF_PROJECT_KEY:-}" ]]; then
    cmd+=(--project="$JF_PROJECT_KEY")
  fi
  "${cmd[@]}"
}

echo "==> Conan profile + remotes (virtual install, local deploy)"
conan profile detect --force
CONAN_REMOTE_URL="${JF_URL}/artifactory/api/conan/${CONAN_VIRTUAL_REPO_KEY}"
conan remote add jfrog-conan "$CONAN_REMOTE_URL" --force
conan remote login jfrog-conan "$JF_CONAN_USER" -p "$JF_ACCESS_TOKEN"
CONAN_LOCAL_URL="${JF_URL}/artifactory/api/conan/${CONAN_LOCAL_REPO_KEY}"
conan remote add jfrog-conan-deploy "$CONAN_LOCAL_URL" --force
conan remote login jfrog-conan-deploy "$JF_CONAN_USER" -p "$JF_ACCESS_TOKEN"

echo "==> Conan install + CMake build (matches CI build job)"
jf_with_project conan install . --profile:build=default --profile:host=default \
  -s build_type=Release --build=missing -r jfrog-conan
cmake --preset conan-release
cmake --build --preset conan-release

echo "==> Tests (optional; same preset as CI)"
if [[ -f build/Release/pixelfrog_tests ]]; then
  ( cd build/Release && ./pixelfrog_tests ) || true
fi

echo "==> Staging (binary for generic upload)"
mkdir -p staging
cp -f build/Release/pixelfrog staging/pixelfrog
chmod +x staging/pixelfrog

if [[ "$CONAN_ART_BUILDINFO" -eq 1 ]]; then
  echo "==> Conan extensions: art:build-info (local repo only for create — avoids virtual set_properties errors)"
  # Upload all cache refs to LOCAL so conanmanifest.txt paths exist for create + upload.
  if [[ "${LOCAL_PUBLISH_DEPS:-0}" != "1" ]]; then
    echo "    (CONAN_ART_BUILDINFO=1 implies Conan upload to $CONAN_LOCAL_REPO_KEY)"
  fi
  conan list "*:*" -c --format=json --out-file=staging/conan-upload-list.json
  # Without --force, Conan often skips binaries with "already in server" while they are NOT under the
  # LOCAL repo path — conan art:build-info upload then fails set_properties on missing conan_package.tgz.
  ART_UPLOAD_FLAGS=( --confirm )
  if [[ "${CONAN_UPLOAD_FORCE:-1}" != "0" ]]; then
    ART_UPLOAD_FLAGS+=( --force )
    echo "    conan upload --force (CONAN_UPLOAD_FORCE=0 to disable)"
  fi
  jf_with_project conan upload -l staging/conan-upload-list.json -r jfrog-conan-deploy \
    "${ART_UPLOAD_FLAGS[@]}" \
    --build-name="$JFROG_BUILD_NAME" \
    --build-number="$BUILD_NUMBER"

  conan config install https://github.com/conan-io/conan-extensions.git \
    -sf=extensions/commands/art -tf=extensions/commands/art
  conan art:server remove pixelfrog-art 2>/dev/null || true
  conan art:server add pixelfrog-art "${JF_URL}/artifactory" \
    --user="$JF_CONAN_USER" --token="$JF_ACCESS_TOKEN"

  conan install . --profile:build=default --profile:host=default \
    -s build_type=Release --build=missing -r jfrog-conan \
    --format=json > staging/conan-install.json

  # Single LOCAL repo: extension assigns all artifact paths to it; virtual would 400 on upload (not local).
  ART_CREATE=( conan art:build-info create staging/conan-install.json \
    "$JFROG_BUILD_NAME" "$BUILD_NUMBER" \
    "$CONAN_LOCAL_REPO_KEY" \
    --server pixelfrog-art \
    --with-dependencies \
    --build-url="$BUILD_URL" )
  if [[ "${CONAN_ART_ADD_CACHED_DEPS:-1}" != "0" ]]; then
    ART_CREATE+=( --add-cached-deps )
  else
    echo "    (omitting --add-cached-deps: CONAN_ART_ADD_CACHED_DEPS=0)"
  fi
  "${ART_CREATE[@]}" > staging/pixelfrog-conan-buildinfo.json

  ART_UPLOAD=( --server pixelfrog-art )
  [[ -n "${JF_PROJECT_KEY:-}" ]] && ART_UPLOAD+=( --project="$JF_PROJECT_KEY" )
  ART_UPLOAD+=( staging/pixelfrog-conan-buildinfo.json )
  conan art:build-info upload "${ART_UPLOAD[@]}"
  echo "    Conan modules published via conan art:build-info upload; jf rt build-publish will merge generic + env/git."
elif [[ "${LOCAL_PUBLISH_DEPS:-0}" == "1" && "$NO_CONAN_UPLOAD" -eq 0 ]]; then
  echo "==> Collect Build Info (jf conan) + upload deps"
  jf_with_project conan install . --profile:build=default --profile:host=default \
    -s build_type=Release --build=missing -r jfrog-conan \
    --build-name="$JFROG_BUILD_NAME" \
    --build-number="$BUILD_NUMBER"
  echo "==> jf conan upload (LOCAL_PUBLISH_DEPS=1)"
  conan list "*:*" -c --format=json --out-file=staging/conan-upload-list.json
  jf_with_project conan upload -l staging/conan-upload-list.json -r jfrog-conan-deploy --confirm \
    --build-name="$JFROG_BUILD_NAME" \
    --build-number="$BUILD_NUMBER"
else
  echo "==> Collect Build Info (jf conan only; set LOCAL_PUBLISH_DEPS=1 or --art-buildinfo for upload)"
  jf_with_project conan install . --profile:build=default --profile:host=default \
    -s build_type=Release --build=missing -r jfrog-conan \
    --build-name="$JFROG_BUILD_NAME" \
    --build-number="$BUILD_NUMBER"
  echo "==> Skipping jf conan upload (set LOCAL_PUBLISH_DEPS=1 to enable)"
fi

if [[ "$NO_SCAN" -eq 0 ]]; then
  echo "==> jf audit (CycloneDX best-effort)"
  mkdir -p local-xray-out
  if jf audit --format=cyclonedx --sbom --sca --without-contextual-analysis --fail=false \
    > staging/pixelfrog-sbom.cdx.json 2>/dev/null && [[ -s staging/pixelfrog-sbom.cdx.json ]]; then
    echo "    wrote staging/pixelfrog-sbom.cdx.json"
  else
    rm -f staging/pixelfrog-sbom.cdx.json
    echo "    (audit produced no CycloneDX; expected for many Conan-only trees)"
  fi

  echo "==> jf scan → local-xray-out/"
  jf scan . --sbom --fail=false --recursive \
    > local-xray-out/pixelfrog-xray.sarif 2>/dev/null || true
  [[ -s local-xray-out/pixelfrog-xray.sarif ]] || echo "    (jf scan produced empty/no sarif)"
else
  echo "==> Skipping jf audit / jf scan (--no-scan)"
fi

echo "==> manifest.json"
conan graph info . -r jfrog-conan -s build_type=Release --format=json > graph-info.json
python3 scripts/ci_write_manifest.py staging/manifest.json

if [[ "$NO_PUBLISH" -ne 0 ]]; then
  echo "==> --no-publish: skipping Artifactory upload + build-publish"
  exit 0
fi

echo "==> Generic upload + build-publish"
jf_with_project rt upload "staging/pixelfrog" \
  "${GENERIC_REPO_KEY}/pixelfrog/${BUILD_NUMBER}/pixelfrog" \
  --build-name="$JFROG_BUILD_NAME" \
  --build-number="$BUILD_NUMBER"
jf_with_project rt upload "staging/manifest.json" \
  "${GENERIC_REPO_KEY}/pixelfrog/${BUILD_NUMBER}/manifest.json" \
  --build-name="$JFROG_BUILD_NAME" \
  --build-number="$BUILD_NUMBER"
if [[ -f staging/pixelfrog-sbom.cdx.json ]]; then
  jf_with_project rt upload "staging/pixelfrog-sbom.cdx.json" \
    "${GENERIC_REPO_KEY}/pixelfrog/${BUILD_NUMBER}/pixelfrog-sbom.cdx.json" \
    --build-name="$JFROG_BUILD_NAME" \
    --build-number="$BUILD_NUMBER"
fi

GIT_PATH="$ROOT/.git"
if [[ -d "$GIT_PATH" ]]; then
  COLLECT_GIT=(--collect-git-info=true --dot-git-path="$GIT_PATH")
else
  COLLECT_GIT=(--collect-git-info=false)
fi

jf_with_project rt build-publish "$JFROG_BUILD_NAME" "$BUILD_NUMBER" \
  --collect-env=true \
  "${COLLECT_GIT[@]}" \
  --build-url="$BUILD_URL"

echo "==> Done. Build: $JFROG_BUILD_NAME / $BUILD_NUMBER"
echo "    Optional: jf bs $JFROG_BUILD_NAME $BUILD_NUMBER   # Xray build scan v2"
