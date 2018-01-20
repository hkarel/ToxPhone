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
    Depends { name: "Qt"; submodules: ["core", "network"] }

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
            if (printPackegeBuildInfo) {
                var file = new TextFile(projectBuildDirectory + "/package_build_info", TextFile.WriteOnly);
                try {
//                    var libOpencv = lib.opencv.dynamicLibraries;
//                    for (var i in libOpencv)
//                        file.writeLine(lib.opencv.libraryPath + "/lib" + libOpencv[i] + ".so*");
//                    if (lib.pylon.enabled) {
//                        var libPylon = lib.pylon.dynamicLibraries;
//                        for (var i in libPylon)
//                            file.writeLine(lib.pylon.libraryPath + "/lib" + libPylon[i] + ".so*");
//                    }
//                    file.writeLine(lib.caffe.libraryPath + "/lib*.so*");
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

    cpp.cxxFlags: {
        var cxx = project.cxxFlags;
//        var isArm = qbs.architecture.startsWith("arm");
//        if (isArm === true) {
//            cxx.push("-mtune=cortex-a15.cortex-a7");
//            cxx.push("-mfloat-abi=hard");
//            cxx.push("-mfpu=neon");
//        }
//        if (!isArm && (project.useSimd === true)) {
//            cxx.push("-msse");
//            cxx.push("-msse2");
//            cxx.push("-msse3");
//            cxx.push("-msse4");
//        }
        return cxx;
    }
    //cpp.machineType: "core2"
    //cpp.cLanguageVersion: "c11"
    //cpp.cxxLanguageVersion: "c++11"

    cpp.driverFlags: [
        "--param", "inline-unit-growth=150",
        "--param", "max-inline-insns-single=1000",
        "-Wl,-z,now",
        "-Wl,-z,relro",
    ]

    cpp.includePaths: [
        "./",
        "../",
        //"../3rdparty",
        //"../common",
    ]
    cpp.systemIncludePaths: QbsUtl.concatPaths(
        //Qt.core.cpp.includePaths,
        lib.sodium.includePath
    )

    cpp.rpaths: QbsUtl.concatPaths(
        productProbe.compilerLibraryPath,
        //lib.opencv.libraryPath,
        //lib.pylon.libraryPath,
        //lib.openblas.libraryPath,
        //lib.caffe.libraryPath,
        "$ORIGIN/../lib"
    )

    cpp.libraryPaths: QbsUtl.concatPaths(
        //lib.opencv.libraryPath,
        //lib.pylon.libraryPath,
        //lib.openblas.libraryPath,
        //lib.caffe.libraryPath,
        //project.buildDirectory + "/lib",
        //"/usr/lib/x86_64-linux-gnu/hdf5/serial"
    )

    cpp.dynamicLibraries: {
        var libs = [
            "pthread",
         ];
        return libs;
    }

    //cpp.staticLibraries: lib.caffe.staticLibrariesPaths(product)

//    Group {
//        name: "resources"
//        files: ["27fretaild.qrc",]
//    }

    files: [
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
