# Repository instructions

## Tool output safety

- Never emit an unbounded full-page DOM snapshot. Scope browser inspection to the smallest relevant element and cap any textual snapshot or tool output at 20,000 characters.
- Do not repeat large tool results in follow-up calls. Extract only the fields needed for the next decision.
- When a task history becomes unusually large, continue in a fresh task or compact summary instead of repeatedly reloading the full transcript.

## C++ maintenance

- Follow Google C++ Style Guide and the documented exceptions in `docs/development.md`. Use lowercase snake_case `.cpp` and `.h` paths; the user explicitly selected `.cpp`.
- Use CamelCase types/functions, snake_case variables/public struct fields/namespaces, trailing underscores for private class fields, and `kCamelCase` constants/enumerators. Do not invent meanings for unresolved protocol fields.
- Resolve internal includes from `src/`; use the path-derived `BUBBLEFIGHTER_SERVER_..._` guard for every handwritten header. Keep headers independently compilable.
- Preserve Windows, MSVC v143, C++20, Visual Studio projects, `BubbleFighter.Server.exe`, and `Artifacts/Bin/x64/<configuration>`.
- Preserve wire IDs, widths, ordering, defaults, checks, strings and packet order; CLI/config keys, schema and historical migrations are also compatibility boundaries. Record functional defects separately.
- Preserve ownership, lifetimes, validation order, transaction locks/commit/rollback and exception propagation. Keep the three commerce notification sequences distinct.
- Keep generated tables compiled into the program. Do not format, reorder or regenerate `src/**/generated/*` without recovering and verifying the original generation chain. Numeric/string data and type semantics must remain equivalent.
- Read `docs/development.md` and `docs/refactoring.md` before changing the relevant modules. Run the checks in `build/verify.py`; frozen fixtures must not be regenerated from the implementation being tested.
- Refresh project items/filters with `build/update_project_files.py` after adding or moving production sources. Tests remain in the same `.sln`/`.vcxproj` system.
- Formatting success is not a claim of compliance with every Google rule. A build is not a live-client or database acceptance test.
