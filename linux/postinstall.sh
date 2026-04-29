#!/bin/sh
# Post-install: create the quetoo system user if it does not already exist.
useradd --system --no-create-home --shell /usr/sbin/nologin quetoo 2>/dev/null || true
