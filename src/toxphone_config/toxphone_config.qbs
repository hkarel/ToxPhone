import qbs
import QbsUtl
import ProbExt

Product {
    name: "ToxPhone (Config)"
    targetName: "toxphone-config"

    type: "application"
    consoleApplication: false
    destinationDirectory: "./bin"

    Depends { name: "cpp" }
    Depends { name: "Commands" }
    Depends { name: "PProto" }
    Depends { name: "SharedLib" }
    Depends { name: "Yaml" }
    Depends { name: "lib.sodium" }
    Depends { name: "Qt"; submodules: ["core", "network", "widgets"] }

    lib.sodium.version:   project.sodiumVersion
    lib.sodium.useSystem: project.useSystemSodium

    ProbExt.LibValidationProbe {
        id: libValidation
        checkingLibs: [lib.sodium]
    }

    cpp.defines: project.cppDefines
    cpp.cxxFlags: project.cxxFlags
    cpp.cxxLanguageVersion: project.cxxLanguageVersion

    cpp.includePaths: [
        "./",
        "../",
    ]
    cpp.systemIncludePaths: QbsUtl.concatPaths(
        Qt.core.cpp.includePaths,
        lib.sodium.includePath
    )

    cpp.rpaths: QbsUtl.concatPaths(
        "/opt/toxphone/lib"
        //"$ORIGIN/../lib"
    )

    //cpp.libraryPaths: [
    //    project.buildDirectory + "/lib",
    //]

    cpp.dynamicLibraries: [
        "pthread",
    ]

    cpp.staticLibraries: lib.sodium.staticLibrariesPaths(product)

    Group {
        name: "resources"
        files: [
            "toxphone_config.qrc",
        ]
    }

    Group {
        name: "widgets"
        prefix: "widgets/"
        files: [
            "comparator.h",
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
            "list_widget_item.h",
            "main_window.cpp",
            "main_window.h",
            "main_window.ui",
            "password_window.cpp",
            "password_window.h",
            "password_window.ui",
            "stub_widget.cpp",
            "stub_widget.h",
            "stub_widget.ui"
        ]
    }

    files: [
        "toxphone_config.cpp",
    ]

} // Product
