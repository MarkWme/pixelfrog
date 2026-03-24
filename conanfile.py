from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout


class PixelfrogConan(ConanFile):
    name = "pixelfrog"
    version = "1.0.0"
    settings = "os", "compiler", "build_type", "arch"
    options = {"demo_vuln_mode": [True, False]}
    default_options = {"demo_vuln_mode": False}

    def configure(self):
        if self.options.demo_vuln_mode:
            self.output.warning(
                "demo_vuln_mode enabled: using older spdlog for demo CVE visibility only."
            )

    def requirements(self):
        # Spec referenced cci.20230908; ConanCenter provides cci.20230920 (nearest).
        self.requires("stb/cci.20230920")
        self.requires("nlohmann_json/3.11.3")
        if self.options.demo_vuln_mode:
            # Spec suggested 1.8.0; ConanCenter currently publishes 1.9.x+ as oldest spdlog.
            self.requires("spdlog/1.9.2")
        else:
            self.requires("spdlog/1.13.0")
        self.requires("cli11/2.4.1")

    def build_requirements(self):
        self.test_requires("catch2/3.5.3")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.user_presets_path = False  # use committed [CMakePresets.json](CMakePresets.json)
        tc.variables["DEMO_VULN_MODE"] = "ON" if self.options.demo_vuln_mode else "OFF"
        tc.generate()

    def validate(self):
        if self.settings.compiler == "msvc":
            raise ConanInvalidConfiguration("Windows / MSVC is not supported.")
        if self.settings.compiler == "gcc":
            major = int(str(self.settings.compiler.version).split(".")[0])
            if major < 9:
                raise ConanInvalidConfiguration("GCC 9 or newer is required.")
        if self.settings.compiler == "clang":
            major = int(str(self.settings.compiler.version).split(".")[0])
            if major < 10:
                raise ConanInvalidConfiguration("Clang 10 or newer is required.")
        if self.settings.compiler == "apple-clang":
            major = int(str(self.settings.compiler.version).split(".")[0])
            if major < 13:
                raise ConanInvalidConfiguration("Apple Clang 13 or newer is required.")
