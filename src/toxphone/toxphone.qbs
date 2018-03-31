import qbs
import qbs.TextFile
import QbsUtl
import GccUtl

Product {
    type: "application"
    destinationDirectory: "./bin"

    name: "ToxPhone"
    targetName: "toxphone"
    condition: !qbs.toolchain.contains("mingw")

    Depends { name: "cpp" }
    Depends { name: "lib.sodium" }
    Depends { name: "Yaml" }
    Depends { name: "SharedLib" }
    Depends { name: "Kernel" }
    Depends { name: "ToxNetwork" }
    Depends { name: "ToxCore" }
    Depends { name: "FilterAudio" }
    Depends { name: "Qt"; submodules: ["core", "network"] }

    lib.sodium.version:   project.sodiumVersion
    lib.sodium.useSystem: project.useSystemSodium

    Probe {
        id: productProbe
        readonly property bool printPackegeBuildInfo: project.printPackegeBuildInfo
        readonly property string projectBuildDirectory: project.buildDirectory
        property string compilerLibraryPath
        configure: {
            lib.sodium.probe();
            compilerLibraryPath = GccUtl.compilerLibraryPath(cpp.compilerPath);
            if (printPackegeBuildInfo) {
                var file = new TextFile(projectBuildDirectory + "/package_build_info", TextFile.WriteOnly);
                try {
                    var libSodium = lib.sodium.dynamicLibraries;
                    for (var i in libSodium)
                        file.writeLine(lib.sodium.libraryPath + "/lib" + libSodium[i] + ".so*");
                }
                finally {
                    file.close();
                }
            }
        }
    }

    cpp.defines: {
        var def = project.cppDefines;
        return def;
    }

    cpp.cxxFlags: project.cxxFlags
    cpp.driverFlags: [
        //"--param", "inline-unit-growth=150",
        //"--param", "max-inline-insns-single=1000",
        "-Wl,-z,now",
        "-Wl,-z,relro",
    ]

    cpp.includePaths: [
        "./",
        "../",
    ]
    cpp.systemIncludePaths: QbsUtl.concatPaths(
        Qt.core.cpp.includePaths,
        lib.sodium.includePath
    )

    cpp.rpaths: QbsUtl.concatPaths(
        productProbe.compilerLibraryPath,
        lib.sodium.libraryPath,
        "/opt/toxphone/lib"
        //"$ORIGIN/../lib"
    )

    cpp.libraryPaths: QbsUtl.concatPaths(
        lib.sodium.libraryPath
    )

    cpp.dynamicLibraries: {
        var libs = [
            "pthread",
            "opus",
            "pulse",
            "usb",
        ].concat(
            lib.sodium.dynamicLibraries
        );

        if (!(project.osName === "ubuntu" && project.osVersion === "14.04")) {
            libs.push("vpx");
        }
        return libs;
    }

    cpp.staticLibraries: {
        var libs = [];
        if (project.osName === "ubuntu" && project.osVersion === "14.04") {
            // Version VPX must be not less than 1.5.0
            libs.push("/usr/lib/x86_64-linux-gnu/libvpx.a");
        }
        return libs;
    }

    files: [
        "audio/audio_dev.cpp",
        "audio/audio_dev.h",
        "audio/wav_file.cpp",
        "audio/wav_file.h",
        "common/defines.h",
        "common/functions.cpp",
        "common/functions.h",
        "common/voice_filters.cpp",
        "common/voice_filters.h",
        "common/voice_frame.cpp",
        "common/voice_frame.h",
        "diverter/phone_diverter.cpp",
        "diverter/phone_diverter.h",
        "diverter/phone_ring.cpp",
        "diverter/phone_ring.h",
        "diverter/yealink_protocol.cpp",
        "diverter/yealink_protocol.h",
        "tox/tox_call.cpp",
        "tox/tox_call.h",
        "tox/tox_error.cpp",
        "tox/tox_error.h",
        "tox/tox_func.cpp",
        "tox/tox_func.h",
        "tox/tox_logger.cpp",
        "tox/tox_logger.h",
        "tox/tox_net.cpp",
        "tox/tox_net.h",
        "toxphone_appl.cpp",
        "toxphone_appl.h",
        "toxphone.cpp",
    ]

//    property var test: {
//        console.info("=== cpp.staticLibraries ===");
//        console.info(cpp.staticLibraries);
//        console.info("=== qbs.architecture ===");
//        console.info(qbs.architecture);
//    }

} // Product
