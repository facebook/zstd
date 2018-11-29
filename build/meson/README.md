This Meson project is provided with no guarantee and maintained
by Dima Krasner <dima@dimakrasner.com>.

It outputs one `libzstd`, either shared or static, depending on
`default_library` option.

How to build
============

`cd` to this meson directory (`zstd/contrib/meson`) and type:

```sh
meson --buildtype=release --strip --prefix=/usr builddir
cd builddir
ninja             # to build
ninja install     # to install
```

You might want to install it in staging directory:

```sh
DESTDIR=./staging ninja install
```

To configure the build, use:

```sh
meson configure
```

See [man meson(1)](https://manpages.debian.org/testing/meson/meson.1.en.html).
