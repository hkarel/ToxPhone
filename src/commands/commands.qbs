import qbs
import QbsUtl

Product {
    name: "Commands"
    targetName: "commands"

    type: "staticlibrary"

    Depends { name: "cpp" }
    Depends { name: "SharedLib" }
    Depends { name: "PProto" }
    Depends { name: "Yaml" }
    Depends { name: "Qt"; submodules: ["core", "network"] }

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
        "commands.cpp",
        "commands.h",
        "error.h",
    ]
    Export {
        Depends { name: "cpp" }
        cpp.includePaths: product.exportIncludePaths
    }
}
