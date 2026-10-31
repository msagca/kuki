---
name: add-compile-fail-check
description: Add a test that a mistake is refused at compile time (a static_assert or constraint in the Kuki engine). Use when adding or changing such a check -- every promise the engine makes to refuse something at compile time has a test in test/compile_fail.
---

# Add a compile-fail check

`test/compile_fail/*.cpp` are picked up by a glob in `test/CMakeLists.txt`; nothing to register.
Each becomes an object library left out of the build (`KukiCompileFail_<name>`) and a test
`CompileFail.<name>` that builds it and passes only if the compiler's output matches the source's
first line.

1. Name the file for the mistake, as the others are (`world_same_component_id.cpp`,
   `builder_mesh_without_handle.cpp`).
2. First line, exactly: `// expect: <text>` -- a piece of the `static_assert` message or diagnostic,
   used as a regular expression. Pick words from the engine's own message, not the compiler's
   wording around it, which differs between cl and clang.
3. Second line: a comment saying what the mistake is, in a game's terms.
4. The smallest program that makes the mistake. It must fail for this reason only -- check that the
   same file without the mistake compiles, or the test passes on an unrelated error.
5. Reconfigure (the glob is `CONFIGURE_DEPENDS`) and run it:

```bash
cmake --preset Debug -DKUKI_BUILD_TESTS=ON
ctest --test-dir build -R CompileFail.<name> --output-on-failure
```

The tests run serially, since each one invokes the build.
