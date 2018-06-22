import qbs
import QbsUtl
import GccUtl

Product {
    type: "application"
    consoleApplication: false
    destinationDirectory: "./bin"

    name: "ToxPhone (Config)"
    targetName: "toxphone-config"

    Depends { name: "cpp" }
    Depends { name: "lib.sodium" }
    Depends { name: "Yaml" }
    Depends { name: "SharedLib" }
    Depends { name: "Kernel" }
    Depends { name: "Qt"; submodules: ["core", "network"] }
    Depends {
        condition: Qt.core.versionMajor < 5
        name: "Qt.gui";
    }
    Depends {
        condition: Qt.core.versionMajor >= 5
        name: "Qt.widgets";
    }

    lib.sodium.version:   project.sodiumVersion
    lib.sodium.useSystem: project.useSystemSodium

    Probe {
        id: pruductProbe
        property string compilerLibraryPath
        configure: {
            lib.sodium.probe();
            compilerLibraryPath = GccUtl.compilerLibraryPath(cpp.compilerPath);
        }
    }

    cpp.defines: project.cppDefines
    cpp.cxxFlags: project.cxxFlags

    cpp.includePaths: [
        "./",
        "../",
    ]
    cpp.systemIncludePaths: QbsUtl.concatPaths(
        Qt.core.cpp.includePaths,
        lib.sodium.includePath
    )

    //cpp.rpaths: [
    //    pruductProbe.compilerLibraryPath,
    //    "$ORIGIN/../lib",
    //]

    //cpp.libraryPaths: [
    //    project.buildDirectory + "/lib",
    //]

    cpp.dynamicLibraries: [
        "pthread",
    ]

    cpp.staticLibraries: lib.sodium.staticLibrariesPaths(product)

    Group {
        name: "widgets"
        prefix: "widgets/"
        files: [
            "connection.cpp",
            "connection.h",
            "connection.ui",
            "connection_window.cpp",
            "connection_window.h",
            "connection_window.ui",
            "friend.cpp",
            "friend.h",
            "friend.ui",
            "friend_request.cpp",
            "friend_request.h",
            "friend_request.ui",
            "main_window.cpp",
            "main_window.h",
            "main_window.ui",
            "password_window.cpp",
            "password_window.h",
            "password_window.ui",
        ]
    }

//
//     Group {
//         name: "resources"
//         files: [
//             "27fretail.qrc",
//         ]
//     }

    files: [
        "toxphone_config.cpp",
    ]

} // Product
