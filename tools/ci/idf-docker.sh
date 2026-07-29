#!/usr/bin/env bash
# Run an idf.py command inside espressif/idf, compatible with:
#   - bare-metal runners (bind-mount GITHUB_WORKSPACE)
#   - Gitea act_runner Docker executor (workspace exists only inside the job
#     container — bind-mount of that path on the Docker host fails with
#     "Bind mount failed: '/workspace/...' does not exist")
#
# Usage:
#   tools/ci/idf-docker.sh [workdir-relative-to-repo] -- idf.py args...
# Examples:
#   tools/ci/idf-docker.sh . -- set-target esp32s3
#   tools/ci/idf-docker.sh . -- build
#   tools/ci/idf-docker.sh test_apps/embed_unity -- build
#
# Env:
#   IDF_IMAGE   (default: espressif/idf:v5.5.1)
#   IDF_TARGET  (passed through into the container)

set -euo pipefail

IDF_IMAGE="${IDF_IMAGE:-espressif/idf:v5.5.1}"
WS="${GITHUB_WORKSPACE:-$(pwd)}"

REL_DIR="."
if [[ "${1:-}" != "--" && -n "${1:-}" ]]; then
  REL_DIR="$1"
  shift
fi
if [[ "${1:-}" == "--" ]]; then
  shift
fi
if [[ "$#" -lt 1 ]]; then
  echo "usage: $0 [rel-workdir] -- <idf.py args...>" >&2
  exit 2
fi

running_in_container() {
  [[ -f /.dockerenv ]] && return 0
  grep -qaE '(docker|containerd|kubepods|podman)' /proc/1/cgroup 2>/dev/null && return 0
  # cgroup v2 often has no docker name; hostname == 12+ hex is typical for act_runner
  local host
  host="$(cat /etc/hostname 2>/dev/null || true)"
  [[ "${host}" =~ ^[0-9a-f]{12,}$ ]] && return 0
  return 1
}

resolve_self_container() {
  local host cid
  host="$(cat /etc/hostname 2>/dev/null || true)"
  if docker inspect "${host}" >/dev/null 2>&1; then
    echo "${host}"
    return 0
  fi
  # cgroup v1: .../docker/<64-hex>
  cid="$(sed -n 's|.*/docker/\([0-9a-f]\{64\}\).*|\1|p' /proc/self/cgroup 2>/dev/null | head -1 || true)"
  if [[ -n "${cid}" ]] && docker inspect "${cid}" >/dev/null 2>&1; then
    echo "${cid}"
    return 0
  fi
  # cgroup v2: try /proc/self/mountinfo for docker overlay id (best-effort)
  cid="$(sed -n 's|.*/docker/containers/\([0-9a-f]\{64\}\)/.*|\1|p' /proc/self/mountinfo 2>/dev/null | head -1 || true)"
  if [[ -n "${cid}" ]] && docker inspect "${cid}" >/dev/null 2>&1; then
    echo "${cid}"
    return 0
  fi
  return 1
}

CMD=(idf.py "$@")
INNER='. /opt/esp/idf/export.sh && "$@"'

if running_in_container; then
  SELF="$(resolve_self_container)" || {
    echo "CI: running in a container but could not resolve self id for --volumes-from" >&2
    echo "    hostname=$(cat /etc/hostname 2>/dev/null || echo '?')" >&2
    exit 125
  }
  WORKDIR="${WS}/${REL_DIR}"
  echo "CI: nested Docker — volumes-from=${SELF} workdir=${WORKDIR}"
  docker run --rm \
    -e IDF_TARGET="${IDF_TARGET:-}" \
    --volumes-from "${SELF}" \
    -w "${WORKDIR}" \
    "${IDF_IMAGE}" \
    bash -lc "${INNER}" _ "${CMD[@]}"
else
  WORKDIR="/project/${REL_DIR}"
  echo "CI: host Docker — bind-mount ${WS} -> /project workdir=${WORKDIR}"
  docker run --rm \
    -e IDF_TARGET="${IDF_TARGET:-}" \
    -v "${WS}:/project" \
    -w "${WORKDIR}" \
    "${IDF_IMAGE}" \
    bash -lc "${INNER}" _ "${CMD[@]}"
fi
