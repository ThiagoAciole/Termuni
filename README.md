# Termac

Termac is a GTK 4/libadwaita terminal application built with Meson. The
terminal state engine is libghostty-vt, fetched at the revision in
`subprojects/ghostty.wrap` and built by Zig.

## Project documentation

- [Implementation plan and current progress](PLANO_IMPLEMENTACAO.md)
- [Product requirements and architecture](PDR.Arquiteture.md)
- [Visual design and references](DESIGN.md)
- [Instructions for agents](AGENTS.md)

## Build in GNOME Builder

Open this directory as a Meson project. The Flatpak manifest specifies Zig
0.16.0 for the build; the Meson project rejects other Zig versions because
the pinned libghostty-vt revision requires that exact toolchain. Building and
running through the GNOME Builder interface remains to be verified.

The GNOME Builder manifest uses the `development` profile and the separate
`thiago.aciole.Termac.Devel` application ID. This keeps the development build
and its icon/settings separate from the regular app. The `default` profile is
for release builds.

For a host build, put Zig 0.16.0 on `PATH`, then run:

```sh
zig version
meson setup build -Dprofile=development
meson compile -C build
build/src/termac
```

Use `-Dprofile=default` for a regular build.

The first Meson build fetches the pinned Ghostty source and its Zig packages.
