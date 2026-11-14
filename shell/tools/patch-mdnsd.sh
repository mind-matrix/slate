#!/bin/sh
# Prepares Apple's mDNSResponder source (run in its top directory) for Slate's build:
# - drops an Apple-only AWDL check that doesn't compile on Linux (there is no AWDL on Linux),
# - puts the daemon's socket and pid file in /run/slate-mdns, which only apps granted the
#   discovery permission can enter.
# Fails if a line it changes has moved, so a new release is looked at before it ships.
set -eu

DAEMON=mDNSShared/uds_daemon.c
MAKEFILE=mDNSPosix/Makefile

sed -i 's|    const mDNSBool is_split_awdl_query = (req->resolve_awdl \&\& question->InterfaceID == AWDLInterfaceID);|    const mDNSBool is_split_awdl_query = mDNSfalse; // Slate: no AWDL on Linux|' "$DAEMON"
grep -q "Slate: no AWDL on Linux" "$DAEMON"

sed -i 's|-DPID_FILE=\\"/var/run/mdnsd.pid\\" -DMDNS_UDS_SERVERPATH=\\"/var/run/mdnsd\\"|-DPID_FILE=\\"/run/slate-mdns/mdnsd.pid\\" -DMDNS_UDS_SERVERPATH=\\"/run/slate-mdns/mdnsd\\"|' "$MAKEFILE"
grep -q '/run/slate-mdns/mdnsd' "$MAKEFILE"
