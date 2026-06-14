# vcpkg port for pbr-cpp-memory-pool (ADR-0030, ROADMAP §7.8).
#
# Builds the library from its v<VERSION> source tag and installs it through the
# project's own CMake install/export rules (ADR-0028), so the port is a thin
# wrapper: vcpkg_cmake_config_fixup relocates the find_package config and
# vcpkg_fixup_pkgconfig the .pc into vcpkg's layout. VERSION comes from
# vcpkg.json, so the tag and the package version stay single-sourced.

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO danielPoloWork/pbr-cpp-memory-pool
    REF "v${VERSION}"
    SHA512 a559f0fb4ad7243e3f3ab0a96474d7322a5bac949d854bcf1afcbb0990c0e2072892d81a680e34eaf812caf7854b7391b1944aeb547a86afb431de2419897cdf
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DPBR_MEMORY_POOL_BUILD_TESTS=OFF
        -DPBR_MEMORY_POOL_BUILD_BENCHMARKS=OFF
        -DPBR_MEMORY_POOL_INSTALL=ON
)

vcpkg_cmake_install()

# The installed config lives at lib/cmake/pbr_memory_pool (CMake package name
# uses the underscore form; the vcpkg port name is the hyphenated form).
vcpkg_cmake_config_fixup(
    PACKAGE_NAME pbr_memory_pool
    CONFIG_PATH lib/cmake/pbr_memory_pool
)

vcpkg_fixup_pkgconfig()

# Header-only public API duplicated across configs → drop the debug copy; the
# library is static-only (ADR-0004 §4) so there is no debug/bin to keep. Our
# install also drops a LICENSE under share/doc — superseded by the canonical
# vcpkg copyright below, so remove it from both trees.
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
    "${CURRENT_PACKAGES_DIR}/share/doc")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
