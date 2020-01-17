import qbs
import GccUtl
import QbsUtl

Product {
    type: "staticlibrary"

    name: "Kernel"
    targetName: "kernel"

    Depends { name: "cpp" }
    Depends { name: "Yaml" }
    Depends { name: "SharedLib" }
    Depends { name: "Qt"; submodules: ["core", "network"] }

    cpp.archiverName: GccUtl.ar(cpp.toolchainPathPrefix)
    cpp.defines: project.cppDefines
    cpp.cxxFlags: project.cxxFlags
    cpp.cxxLanguageVersion: project.cxxLanguageVersion

    property var exportIncludePaths: [
        "./",
    ]
    cpp.includePaths: exportIncludePaths;

    // Эта декларация нужна для подавления Qt warning-ов
    cpp.systemIncludePaths: Qt.core.cpp.includePaths

    files: [
        "communication/commands.cpp",
        "communication/commands.h",
        "communication/error.h",
        //"network/interfaces.cpp",
        //"network/interfaces.h",
    ]
    Export {
        Depends { name: "cpp" }
        cpp.includePaths: product.exportIncludePaths
    }
}
