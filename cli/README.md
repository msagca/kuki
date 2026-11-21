# kuki

The command a game is started and looked after with.

The installer's **Kuki CLI** component puts `kuki` on the PATH. Then make the game's repository, `./my-game`: git, the engine pinned in `external/kuki`, and the template named `MyGame`.

```bash
kuki new my-game
```

Go into it:

```bash
cd my-game
```

Build it (`kuki build --release` for a release build):

```bash
kuki build
```

Run it; any arguments after `run` go to the game:

```bash
kuki run
```

Later, move the engine to its latest main:

```bash
kuki update
```

Nothing is committed by `kuki new`; the first commit is yours. Git and Git LFS are needed, and the Windows SDK for the HLSL compiler.

To build `kuki` from source, see [../docs/building/README.md](../docs/building/README.md).
