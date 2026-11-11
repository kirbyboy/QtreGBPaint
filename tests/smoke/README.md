# Smoke tests

The SDL/ImGui smoke tests exercise the real frontend with injected SDL events.
They cover tools, menus, dialogs, file loading/saving, rendering, selection,
transparency, zoom and window behavior.

Build the application first (and fetch the local dependencies once):

```sh
make deps
make -j"$(nproc)"
./tests/smoke/run.sh
```

Run selected smokes:

```sh
./tests/smoke/run.sh circle buttons
```

Useful options:

```sh
./tests/smoke/run.sh --list
./tests/smoke/run.sh --build-only
./tests/smoke/run.sh --no-build
```

The runner writes binaries, logs and captured `.ppm` files to `temp/` (ignored
by git). It runs the smokes serially because they open SDL windows. On a
headless machine use Xvfb:

```sh
xvfb-run -a -s "-screen 0 1024x768x24" ./tests/smoke/run.sh
```

CI runs the full suite this way and retries a failed smoke up to two times with
`SMOKE_RETRIES=2`.
