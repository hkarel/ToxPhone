import qbs
import GccUtl

Product {
    type: "staticlibrary"

    name: "FilterAudio"
    targetName: "filter_audio"

    Depends { name: "cpp" }

    cpp.archiverName: GccUtl.ar(cpp.toolchainPathPrefix)
    cpp.cFlags: [
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
    ]

    property var exportIncludePaths: [
        "./filter_audio",
    ]
    cpp.systemIncludePaths: exportIncludePaths;

    files: [
        "filter_audio/aec/*.c",
        "filter_audio/aec/*.h",
        "filter_audio/aec/include/*.h",
        "filter_audio/agc/*.c",
        "filter_audio/agc/*.h",
        "filter_audio/agc/include/*.h",
        "filter_audio/ns/*.c",
        "filter_audio/ns/*.h",
        "filter_audio/ns/include/*.h",
        "filter_audio/other/*.c",
        "filter_audio/other/*.h",
        "filter_audio/zam/*.c",
        "filter_audio/zam/*.h",
        "filter_audio/vad/*.c",
        "filter_audio/vad/*.h",
        "filter_audio/vad/include/*.h",
        "filter_audio/filter_audio.c",
        "filter_audio/filter_audio.h",
    ]
    excludeFiles: [
        "filter_audio/other/resample_sse.h",
    ]

    Export {
        Depends { name: "cpp" }
        cpp.systemIncludePaths: product.exportIncludePaths
    }
}
