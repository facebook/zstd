# Bazel build

This directory documents the Bazel build for zstd. Like the CMake and Meson
builds under `build/`, it is community maintained.

## Building from the repository root

```sh
bazel build //lib:zstd
```

This produces a static library (`libzstd.a` on Linux/macOS, `zstd.lib` on
Windows).

## Layout

- `MODULE.bazel` must stay at the repository root, because Bazel treats the
  root as the workspace and pre-1.0 modules do not accept it anywhere else.
  It is the only file Bazel requires at the root.
- `lib/BUILD.bazel` defines the `zstd` target next to the existing
  `lib/BUCK`, `Makefile` and `libzstd.mk`.
- `.github/workflows/bazel.yml` builds `//lib:zstd` on every change to keep
  the build description from bit-rotting.

## Configuration

`ZSTD_LEGACY_SUPPORT` is disabled and multithreading is off by default,
matching the static library built by `make lib`. The MSVC C11 flag is applied
through `select()` in `lib/BUILD.bazel`, so no `.bazelrc` is needed.

## Using zstd from another Bazel workspace

With Bzlmod, depend on the module and use the `zstd` target:

```python
bazel_dep(name = "zstd", version = "1.6.0")
```

When zstd is vendored with `local_path_override` or `archive_override`, the
same `//lib:zstd` target is available.

## Adding languages or features

The target currently builds the C library only. The command line tools in
`programs/` are not wired into Bazel yet; a follow-up can add them as a
`cc_binary` if there is demand.