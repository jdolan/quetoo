#!/bin/sh
# Post-install: create the quetoo system user and reload systemd.
useradd --system \
  --home-dir /var/lib/quetoo \
  --create-home \
  --shell /usr/sbin/nologin \
  quetoo 2>/dev/null || true

systemctl daemon-reload 2>/dev/null || true
