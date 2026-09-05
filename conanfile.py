from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, rmdir
import os


class CobaltConan(ConanFile):
    name = "cobalt"
    version = "2.4.0"
    description = "A lightweight, zero-dependency C11 framework providing object-oriented capabilities"
    license = "MIT"
    homepage = "https://github.com/quintin-lee/cobalt"
    url = "https://github.com/quintin-lee/cobalt"
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tests": [True, False],
        "with_examples": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tests": False,
        "with_examples": False,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self, **self.conan_data["sources"][0], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["COBALT_BUILD_TESTS"] = self.options.with_tests
        tc.variables["COBALT_BUILD_EXAMPLES"] = self.options.with_examples
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.libs = ["cobalt"]
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["pthread"]

    def exports_sources(self):
        pass

    def package_id(self):
        del self.info.options.with_tests
        del self.info.options.with_examples
