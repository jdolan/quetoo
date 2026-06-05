#!/bin/sh
# Pre-remove: stop and disable dedicated services before uninstall.
# Intentionally do not touch quetoo-master here; it's typically managed
# independently of dedicated server package updates.

systemctl stop 'quetoo-dedicated@*' 2>/dev/null || true
systemctl disable 'quetoo-dedicated@*' 2>/dev/null || true
systemctl daemon-reload 2>/dev/null || true
