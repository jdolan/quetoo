# Quetoo Docker

Two Docker images are provided: a **build image** for compiling packages and a
**dedicated server image** for running a headless server.

## Build image (`Dockerfile.build`)

`quetoo-build` is a Debian bookworm environment with all build dependencies
pre-installed (SDL3, glib, Objectively, ObjectivelyMVC, linuxdeploy, fpm,
etc.). It produces `.deb` and `.rpm` packages from the quetoo source tree.

### Build the image

From the repository root:

```sh
docker build -t quetoo-build -f linux/docker/Dockerfile.build .
```

### Produce packages

Mount the repo source at `/src` and an output directory at `/out`:

```sh
docker run --rm \
  -v "$(pwd)":/src \
  -v /tmp/pkgs:/out \
  quetoo-build
```

Packages are written to `/out/` (and also left in `linux/target/`).

## Dedicated server image (`Dockerfile.dedicated`)

`quetoo-dedicated` is a minimal Debian bookworm-slim image with the quetoo
`.deb` installed. It runs `quetoo-dedicated` directly (not via the init
script).

### Build the image

Builds quetoo internally using `quetoo-build` as a build stage:

```sh
docker build -t quetoo-dedicated -f linux/docker/Dockerfile.dedicated .
```

### Run the server

Game data is not bundled — mount a `quetoo-data` volume or extend the image
with the quetoo-data `.deb`.

```sh
docker run -d \
  --name quetoo \
  -p 1998:1998/udp \
  -v quetoo-data:/usr/lib/quetoo/share/quetoo/default:ro \
  -v ./my-server.cfg:/etc/quetoo-dedicated/default.cfg:ro \
  quetoo-dedicated
```

### Attach to the server console

```sh
docker attach quetoo
```

Detach without stopping with `Ctrl-P Ctrl-Q`.

## docker-compose (`docker-compose.yml`)

A compose file is provided for convenience. Before starting, populate the
`quetoo-data` named volume:

```sh
docker run --rm \
  -v quetoo-data:/usr/lib/quetoo/share/quetoo/default \
  -v /path/to/quetoo-data/target/default:/src:ro \
  busybox cp -a /src/. /usr/lib/quetoo/share/quetoo/default/
```

Then start the server:

```sh
docker compose up -d
```
