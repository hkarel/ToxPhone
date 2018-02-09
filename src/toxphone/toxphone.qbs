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
    Depends { name: "Qt"; submodules: ["core", "network"] }

    //property string ffmpegVersion: "3.3.3"

    lib.sodium.useSystem: false
    lib.sodium.version: project.sodiumVersion

    Probe {
        id: productProbe
        readonly property bool printPackegeBuildInfo: project.printPackegeBuildInfo
        readonly property string projectBuildDirectory: project.buildDirectory
        property string compilerLibraryPath
        configure: {
            lib.sodium.probe();
            compilerLibraryPath = GccUtl.compilerLibraryPath(cpp.compilerPath);
//            if (printPackegeBuildInfo) {
//                var file = new TextFile(projectBuildDirectory + "/package_build_info", TextFile.WriteOnly);
//                try {
//                    var libOpencv = lib.opencv.dynamicLibraries;
//                    for (var i in libOpencv)
//                        file.writeLine(lib.opencv.libraryPath + "/lib" + libOpencv[i] + ".so*");
//                    if (lib.pylon.enabled) {
//                        var libPylon = lib.pylon.dynamicLibraries;
//                        for (var i in libPylon)
//                            file.writeLine(lib.pylon.libraryPath + "/lib" + libPylon[i] + ".so*");
//                    }
//                    file.writeLine(lib.caffe.libraryPath + "/lib*.so*");
//                }
//                finally {
//                    file.close();
//                }
//            }
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
        //Qt.core.cpp.includePaths,
        lib.sodium.includePath
    )

    //cpp.rpaths: QbsUtl.concatPaths(
    //    productProbe.compilerLibraryPath,
    //    "$ORIGIN/../lib"
    //)

    //cpp.libraryPaths: QbsUtl.concatPaths(
    //    //lib.opencv.libraryPath,
    //    //lib.pylon.libraryPath
    //)

    cpp.dynamicLibraries: {
        var libs = [
            "pthread",
            "opus",
            "pulse",
         ];
        if (!(project.osName === "ubuntu" && project.osVersion === "14.04")) {
            libs.push("vpx");
        }
        return libs;
    }

    cpp.staticLibraries: {
        var libs = QbsUtl.concatPaths(
            lib.sodium.staticLibrariesPaths(product)
            //lib.ffmpeg.staticLibrariesPaths(product)
        );
        if (project.osName === "ubuntu" && project.osVersion === "14.04") {
            // Version VPX must be not less than 1.5.0
            libs.push("/usr/lib/x86_64-linux-gnu/libvpx.a");
        }
        return libs;
    }

//    Group {
//        name: "resources"
//        files: ["27fretaild.qrc",]
//    }

    files: [
        "audio/audio_dev.cpp",
        "audio/audio_dev.h",
        "audio/wav_file.cpp",
        "audio/wav_file.h",
        "common/defines.h",
        "common/functions.cpp",
        "common/functions.h",
        "common/voice_frame.cpp",
        "common/voice_frame.h",
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
