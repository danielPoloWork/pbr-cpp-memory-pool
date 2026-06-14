# vcpkg port — `pbr-memory-pool`

A [vcpkg](https://vcpkg.io) port for `pbr-cpp-memory-pool`, pinned to the `v1.0.0` tag. This is **Phase 2 distribution** ([ADR-0004](../docs/adr/0004-versioning-and-release-policy.md) §5, [ADR-0030](../docs/adr/0030-vcpkg-port.md)); the Phase 1 `find_package` / pkg-config install is [ADR-0028](../docs/adr/0028-install-and-packaging-layout.md).

The port builds the library from source via the project's own CMake install rules, so a vcpkg consumer gets the exact same imported target as every other consumption mode:

```cmake
find_package(pbr_memory_pool CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE pbr::memory_pool)
```

## Use it now — as an overlay port

No upstream registration is required to consume the port today. Point vcpkg at this directory with `--overlay-ports`:

```bash
vcpkg install pbr-memory-pool --overlay-ports=/path/to/pbr-cpp-memory-pool/ports
```

Or, in manifest mode, add `"pbr-memory-pool"` to your `vcpkg.json` `dependencies` and pass `--overlay-ports` (or set `VCPKG_OVERLAY_PORTS`) at configure time.

## Files

- [`pbr-memory-pool/vcpkg.json`](pbr-memory-pool/vcpkg.json) — manifest: name, version (`1.0.0`), license, and the `vcpkg-cmake` / `vcpkg-cmake-config` host tools.
- [`pbr-memory-pool/portfile.cmake`](pbr-memory-pool/portfile.cmake) — fetches the `v${VERSION}` source tag (SHA512-pinned), configures with tests/benchmarks off, installs, then relocates the CMake config (`vcpkg_cmake_config_fixup`) and the pkg-config `.pc` (`vcpkg_fixup_pkgconfig`) into vcpkg's layout.

## Submitting upstream to microsoft/vcpkg

The port is written to the upstream conventions so it can be contributed to [microsoft/vcpkg](https://github.com/microsoft/vcpkg) when desired:

1. Copy `pbr-memory-pool/` into `ports/pbr-memory-pool/` in a vcpkg checkout.
2. Run `vcpkg x-add-version pbr-memory-pool` to update `versions/` (the registry baseline).
3. Open a PR against microsoft/vcpkg.

Upstream registration is **deferred** (it depends on the registry maintainers' review cadence and adds an ongoing per-release maintenance obligation — ADR-0030); the overlay port above is the supported path in the meantime.

## Bumping the version

On a new release tag, set `version` in `vcpkg.json` and replace the `SHA512` in `portfile.cmake` with the hash of the new source tarball:

```bash
curl -sSL https://github.com/danielPoloWork/pbr-cpp-memory-pool/archive/v<X.Y.Z>.tar.gz | sha512sum
```

(`REF` is `v${VERSION}`, so only the manifest version and the SHA512 change.)
