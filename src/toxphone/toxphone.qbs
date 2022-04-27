import qbs
import qbs.File
import qbs.TextFile
import QbsUtl
import ProbExt

Product {
    name: "ToxPhone"
    targetName: "toxphone"
    condition: !qbs.toolchain.contains("mingw")

    type: "application"
    destinationDirectory: "./bin"

    Depends { name: "cpp" }
  //Depends { name: "Celt" }
    Depends { name: "Commands" }
    Depends { name: "PProto" }
    Depends { name: "SharedLib" }
    Depends { name: "ToxCore" }
    Depends { name: "FilterAudio" }
    Depends { name: "RNNoise" }
    Depends { name: "Yaml" }
    Depends { name: "lib.sodium" }
    Depends { name: "Qt"; submodules: ["core", "network"] }

    lib.sodium.version:   project.sodiumVersion
    lib.sodium.useSystem: project.useSystemSodium

    ProbExt.LibValidationProbe {
        id: libValidation
        checkingLibs: [lib.sodium]
    }

    cpp.defines: {
        var def = project.cppDefines;
        return def;
    }

    cpp.cxxFlags: project.cxxFlags
    cpp.cxxLanguageVersion: project.cxxLanguageVersion

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
            "vpx",
        ].concat(
            lib.sodium.dynamicLibraries
        );

        //if (project.osName !== "ubuntu") {
        //    libs.push("vpx");
        //}
        //if (qbs.architecture.startsWith("arm") === true) {
        //    libs.push("vpx");
        //}
        return libs;
    }

//    cpp.staticLibraries: {
//        var libs = [];
//        //if (project.osName === "ubuntu") {
//        //    // Version VPX must be not less than 1.5.0
//        //    libs.push("/usr/lib/x86_64-linux-gnu/libvpx.a");
//        //}

//        // Version VPX must be not less than 1.5.0
//        if (qbs.architecture.startsWith("x86_64") === true) {
//            libs.push("/usr/lib/x86_64-linux-gnu/libvpx.a");
//        }
//        return libs;
//    }

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
        "toxphone.cpp",
        "toxphone_appl.cpp",
        "toxphone_appl.h",
    ]

//    property var test: {
//        console.info("=== cpp.staticLibraries ===");
//        console.info(cpp.staticLibraries);
//        console.info("=== qbs.architecture ===");
//        console.info(qbs.architecture);
//    }

} // Product
