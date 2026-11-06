from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout

class CalderaConan(ConanFile):
    name = "caldera"
    version = "0.0.1"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False], "withTests" : [True, False]}
    default_options = {"shared": False, "fPIC": True, "withTests" : False}

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("glfw/3.4")
        self.requires("vulkan-memory-allocator/3.3.0")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.components["render-graph"].libs = ["caldera-render-graph"]
        self.cpp_info.components["render-graph"].includedirs = ["include"]
        self.cpp_info.components["render-graph"].requires = ["glfw::glfw", "vulkan-memory-allocator::vulkan-memory-allocator"]

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.variables["CALDERA_BUILD_TESTS"] = "ON" if self.options.withTests else "OFF"
        tc.generate()