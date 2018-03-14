import qbs
import GccUtl
import QbsUtl

Product {
    type: "staticlibrary"

    name: "SharedLib"
    targetName: "shared"

    Depends { name: "cpp" }
    Depends { name: "Yaml" }
    Depends { name: "Qt"; submodules: ["core", "network"] }

    cpp.archiverName: GccUtl.ar(cpp.toolchainPathPrefix)
    cpp.defines: project.cppDefines
    cpp.cxxFlags: project.cxxFlags //.concat(["-Wpedantic"]);

    property var exportIncludePaths: [
        "./",
        "./shared",
    ]
    cpp.includePaths: exportIncludePaths;

    // Эта декларация нужна для подавления Qt warning-ов
    cpp.systemIncludePaths: Qt.core.cpp.includePaths

    files: [
        "shared/config/yaml_config.cpp",
        "shared/config/yaml_config.h",
        "shared/logger/config.cpp",
        "shared/logger/config.h",
        "shared/logger/logger.cpp",
        "shared/logger/logger.h",
        "shared/qt/communication/commands_base.cpp",
        "shared/qt/communication/commands_base.h",
        "shared/qt/communication/commands_pool.cpp",
        "shared/qt/communication/commands_pool.h",
        "shared/qt/communication/communication_bserialize.h",
        "shared/qt/communication/func_invoker.h",
        "shared/qt/communication/functions.cpp",
        "shared/qt/communication/functions.h",
        "shared/qt/communication/host_point.cpp",
        "shared/qt/communication/host_point.h",
        "shared/qt/communication/logger_operators.cpp",
        "shared/qt/communication/logger_operators.h",
        "shared/qt/communication/message.cpp",
        "shared/qt/communication/message.h",
        "shared/qt/communication/transport/base.cpp",
        "shared/qt/communication/transport/base.h",
        "shared/qt/communication/transport/local.cpp",
        "shared/qt/communication/transport/local.h",
        "shared/qt/communication/transport/tcp.cpp",
        "shared/qt/communication/transport/tcp.h",
        "shared/qt/communication/transport/udp.cpp",
        "shared/qt/communication/transport/udp.h",
        //"shared/qt/compression/qlzma.cpp",
        //"shared/qt/compression/qlzma.h",
        //"shared/qt/compression/qppmd.cpp",
        //"shared/qt/compression/qppmd.h",
        "shared/qt/config/config.cpp",
        "shared/qt/config/config.h",
        "shared/qt/logger/logger_operators.cpp",
        "shared/qt/logger/logger_operators.h",
        "shared/qt/thread/qthreadex.cpp",
        "shared/qt/thread/qthreadex.h",
        "shared/qt/version/version_number.cpp",
        "shared/qt/version/version_number.h",
        "shared/qt/bserialize.cpp",
        "shared/qt/bserialize.h",
        "shared/qt/quuidex.h",
        "shared/thread/thread_base.cpp",
        "shared/thread/thread_base.h",
        "shared/thread/thread_info.cpp",
        "shared/thread/thread_info.h",
        "shared/thread/thread_pool.cpp",
        "shared/thread/thread_pool.h",
        "shared/_list.h",
        "shared/break_point.h",
        "shared/clife_alloc.h",
        "shared/clife_base.h",
        "shared/clife_ptr.h",
        "shared/container_ptr.h",
        "shared/ring_buffer.cpp",
        "shared/ring_buffer.h",
        "shared/safe_singleton.h",
        "shared/simple_ptr.h",
        "shared/spin_locker.h",
        "shared/steady_timer.h",
        "shared/utils.cpp",
        "shared/utils.h",
    ]

    Export {
        Depends { name: "cpp" }
        cpp.includePaths: product.exportIncludePaths
    }
}
