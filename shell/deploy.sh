#!/bin/bash
set -euo pipefail

HOST="${SLATE_HOST:-slate}"
BINARY="${SLATE_BUILD_ROOT:-${HOME}/slate-build}/shell/pi/slate-shell"

scp -q "${BINARY}" "${HOST}:/tmp/slate-shell"
ssh "${HOST}" 'systemctl stop slate-shell &&
	cp /tmp/slate-shell /usr/local/bin/slate-shell &&
	systemctl start slate-shell &&
	systemctl is-active slate-shell'
