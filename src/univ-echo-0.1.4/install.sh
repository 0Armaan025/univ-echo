#!/usr/bin/env bash
set -Eeuo pipefail

REPO="0Armaan025/univ-echo"
ASSET="univecho-linux-x86_64.tar.gz"
BASE_URL="https://github.com/${REPO}/releases/latest/download"
INSTALL_DIR="${HOME}/.local/bin"

printf '\nUNIV-ECHO Linux installer\n\n'

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "This installer supports Linux only." >&2
  exit 1
fi

case "$(uname -m)" in
x86_64 | amd64) ;;
*)
  echo "Unsupported architecture. This release supports x86_64." >&2
  exit 1
  ;;
esac

for cmd in curl tar sha256sum install; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "Missing required command: $cmd" >&2
    exit 1
  fi
done

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

echo "Downloading latest release..."

curl --fail --location --silent --show-error \
  "${BASE_URL}/${ASSET}" \
  -o "${TMP_DIR}/${ASSET}"

curl --fail --location --silent --show-error \
  "${BASE_URL}/${ASSET}.sha256" \
  -o "${TMP_DIR}/${ASSET}.sha256"

echo "Verifying checksum..."

(
  cd "$TMP_DIR"
  sha256sum --check "${ASSET}.sha256"
)

echo "Extracting..."

tar -xzf "${TMP_DIR}/${ASSET}" -C "$TMP_DIR"

if [[ ! -f "${TMP_DIR}/univecho" ]]; then
  echo "Release archive does not contain the univecho executable." >&2
  exit 1
fi

mkdir -p "$INSTALL_DIR"
install -Dm755 "${TMP_DIR}/univecho" "${INSTALL_DIR}/univecho"

echo
echo "UNIV-ECHO installed successfully."
echo "Location: ${INSTALL_DIR}/univecho"

case ":${PATH}:" in
*":${INSTALL_DIR}:"*) ;;
*)
  echo
  echo "Add ~/.local/bin to your PATH:"
  echo 'export PATH="$HOME/.local/bin:$PATH"'
  ;;
esac

echo
echo "Run: univecho"
