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

    Probe {
        id: pruductProbe
        property string compilerLibraryPath
        configure: {
            compilerLibraryPath = GccUtl.compilerLibraryPath(cpp.compilerPath);
        }
    }

    cpp.defines: project.cppDefines
    cpp.cxxFlags: project.cxxFlags

    cpp.includePaths: [
        "./",
        "../",
    ]
    //cpp.systemIncludePaths: Qt.core.cpp.includePaths;

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
