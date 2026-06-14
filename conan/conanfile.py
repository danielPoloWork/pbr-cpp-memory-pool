# Conan 2.x recipe for pbr-cpp-memory-pool (ADR-0031, ROADMAP §7.9).
#
# Builds the library from its v<version> source tag through the project's own
# CMake build/install rules (ADR-0028). The upstream-installed CMake package
# config and pkg-config are dropped from the Conan package: consumers get the
# target from Conan's own generators (package_info below), so a CMakeDeps
# consumer writes find_package(pbr_memory_pool) + link pbr::memory_pool — the
# same target name as every other consumption mode.

import os

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, rmdir


class PbrMemoryPoolConan(ConanFile):
    name = "pbr-memory-pool"
    version = "1.0.0"
    license = "MIT"
    homepage = "https://github.com/danielPoloWork/pbr-cpp-memory-pool"
    url = "https://github.com/danielPoloWork/pbr-cpp-memory-pool"
    description = (
        "Purpose-built reference high-performance fixed-block O(1) memory pool "
        "(C++17 with an ANSI C public surface, zero external dependencies)."
    )
    topics = ("memory-pool", "allocator", "fixed-block", "cpp17", "ansi-c")

    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"fPIC": [True, False]}
    default_options = {"fPIC": True}

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def validate(self):
        check_min_cppstd(self, 17)

    def source(self):
        get(
            self,
            f"{self.homepage}/archive/v{self.version}.tar.gz",
            sha256="54e99b43bc3807f3e00296b30aa381c5db8bd5748ec6f322ad1f7807f91e53c0",
            strip_root=True,
        )

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["PBR_MEMORY_POOL_BUILD_TESTS"] = False
        tc.cache_variables["PBR_MEMORY_POOL_BUILD_BENCHMARKS"] = False
        tc.cache_variables["CMAKE_POSITION_INDEPENDENT_CODE"] = bool(
            self.options.get_safe("fPIC", True)
        )
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(
            self,
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        # Consumers get the target from Conan's generators (package_info), so
        # drop the upstream-installed CMake package config + pkg-config + docs
        # to avoid two competing configs in the same package.
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))
        rmdir(self, os.path.join(self.package_folder, "lib", "pkgconfig"))
        rmdir(self, os.path.join(self.package_folder, "share"))

    def package_info(self):
        self.cpp_info.libs = ["pbr_memory_pool"]
        # Mirror the upstream CMake package so find_package(pbr_memory_pool) +
        # pbr::memory_pool work identically through Conan's CMakeDeps generator.
        self.cpp_info.set_property("cmake_file_name", "pbr_memory_pool")
        self.cpp_info.set_property("cmake_target_name", "pbr::memory_pool")
        self.cpp_info.set_property("pkg_config_name", "pbr-cpp-memory-pool")
