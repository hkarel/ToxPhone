import qbs
import QbsUtl

Product {
    name: "PProto"
    targetName: "pproto"

    type: "staticlibrary"
    //destinationDirectory: "./lib"

    Depends { name: "cpp" }
    Depends { name: "SharedLib" }
    Depends { name: "lib.sodium" }
    Depends { name: "Qt"; submodules: ["core", "network"] }

    lib.sodium.version:   project.sodiumVersion
    lib.sodium.useSystem: project.useSystemSodium

    cpp.defines: project.cppDefines
    cpp.cxxFlags: project.cxxFlags //.concat(["-fPIC"])
    cpp.cxxLanguageVersion: project.cxxLanguageVersion

    property var includePaths: [
        "./",
        "./pproto",
    ]
    cpp.includePaths: includePaths;

    cpp.systemIncludePaths: QbsUtl.concatPaths(
        Qt.core.cpp.includePaths, // Декларация нужна для подавления Qt warning-ов
        lib.sodium.includePath
    )

    files: [
        "pproto/commands/base.cpp",
        "pproto/commands/base.h",
        "pproto/commands/paging.cpp",
        "pproto/commands/paging.h",
        "pproto/commands/pool.cpp",
        "pproto/commands/pool.h",
        "pproto/commands/time_range.cpp",
        "pproto/commands/time_range.h",
        "pproto/serialize/byte_array.cpp",
        "pproto/serialize/byte_array.h",
        "pproto/serialize/functions.cpp",
        "pproto/serialize/functions.h",
      //"pproto/serialize/json.cpp",
      //"pproto/serialize/json.h",
        "pproto/serialize/qbinary.h",
        "pproto/serialize/result.cpp",
        "pproto/serialize/result.h",
        "pproto/transport/base.cpp",
        "pproto/transport/base.h",
        "pproto/transport/local.cpp",
        "pproto/transport/local.h",
        "pproto/transport/tcp.cpp",
        "pproto/transport/tcp.h",
        "pproto/transport/udp.cpp",
        "pproto/transport/udp.h",
        "pproto/bserialize_space.h",
        "pproto/error_sender.h",
        "pproto/func_invoker.h",
        "pproto/host_point.cpp",
        "pproto/host_point.h",
        "pproto/logger_operators.cpp",
        "pproto/logger_operators.h",
        "pproto/message.cpp",
        "pproto/message.h",
        "pproto/utils.cpp",
        "pproto/utils.h",
    ]

    Export {
        Depends { name: "cpp" }
        cpp.includePaths: exportingProduct.includePaths
    }
}
