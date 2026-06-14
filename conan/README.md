# Conan recipe — `pbr-memory-pool`

A [Conan 2.x](https://conan.io) recipe for `pbr-cpp-memory-pool`, pinned to the `v1.0.0` tag. This is **Phase 2 distribution** ([ADR-0004](../docs/adr/0004-versioning-and-release-policy.md) §5, [ADR-0031](../docs/adr/0031-conan-recipe.md)); the Phase 1 `find_package` / pkg-config install is [ADR-0028](../docs/adr/0028-install-and-packaging-layout.md), and the sibling vcpkg port is [ADR-0030](../docs/adr/0030-vcpkg-port.md).

The recipe builds the library from source through the project's own CMake build/install rules. A Conan (CMakeDeps) consumer links the same imported target as every other consumption mode:

```cmake
find_package(pbr_memory_pool CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE pbr::memory_pool)
```

## Use it now — from this recipe

No registry upload is required to consume the recipe today. Create the package into your local Conan cache, then depend on `pbr-memory-pool/1.0.0`:

```bash
conan create conan/            # builds + runs the test_package
# then, from a consumer:
#   [requires]
#   pbr-memory-pool/1.0.0
conan install . --build=missing
```

## Files

- [`conanfile.py`](conanfile.py) — the recipe: fetches the `v<version>` source tag (SHA256-pinned), builds with tests/benchmarks off via `CMakeToolchain` / `CMake`, installs, drops the upstream-bundled CMake config + `.pc` (Conan provides its own through `package_info`), and sets `cmake_file_name` / `cmake_target_name` so consumers get `pbr::memory_pool`.
- [`test_package/`](test_package/) — a minimal consumer (`find_package` + link + run) that Conan builds during `conan create` to verify the package.

## CI smoke test

The [`packaging-smoke`](../.github/workflows/packaging-smoke.yml) workflow runs `conan create conan/` — which builds the package from the pinned `v1.0.0` tag and then builds + runs the [`test_package/`](test_package/) consumer — on every change to `conan/**` and on a weekly schedule (the recipe is version-pinned, so the schedule is the main signal for Conan / toolchain drift). Conan cannot be run on the maintainer's box, so CI is where the recipe is exercised.

## Publishing

The recipe is written to ConanCenter conventions and can be contributed to [conan-io/conan-center-index](https://github.com/conan-io/conan-center-index) when desired (its layout there is `recipes/pbr-memory-pool/all/conanfile.py` + a `config.yml` mapping the version to the source). Alternatively, upload to a self-hosted Artifactory / `conan remote`:

```bash
conan create conan/
conan upload pbr-memory-pool/1.0.0 -r <your-remote> --confirm
```

Registry publication (ConanCenter or self-hosted) is **deferred** (it depends on the index maintainers' review cadence and adds a per-release maintenance obligation — ADR-0031); creating the package locally from this recipe is the supported path in the meantime.

## Bumping the version

On a new release tag, set `version` in `conanfile.py` and replace the `sha256` with the hash of the new source tarball:

```bash
curl -sSL https://github.com/danielPoloWork/pbr-cpp-memory-pool/archive/v<X.Y.Z>.tar.gz | sha256sum
```

(The source URL is derived from `version`, so only the version and the hash change.)
