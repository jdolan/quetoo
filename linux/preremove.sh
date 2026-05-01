#!/bin/sh
# Pre-remove: stop and disable quetoo services before uninstall.

systemctl stop 'quetoo-dedicated@*' 2>/dev/null || true
systemctl disable 'quetoo-dedicated@*' 2>/dev/null || true
systemctl stop quetoo-master 2>/dev/null || true
systemctl disable quetoo-master 2>/dev/null || true
systemctl daemon-reload 2>/dev/null || true
