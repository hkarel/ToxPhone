import qbs
import GccUtl

Product {
    type: "staticlibrary"

    name: "RNNoise"
    targetName: "rnnoise"

    Depends { name: "cpp" }
    //Depends { name: "Celt" }

    cpp.archiverName: GccUtl.ar(cpp.toolchainPathPrefix)

    cpp.defines: [
        "HAVE_STDINT_H",
        //"USE_SIMD",
        //"FIXED_POINT",
    ]

    cpp.cFlags: [
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
    ]

    property var exportIncludePaths: [
        "./rnnoise/include",
    ]
    cpp.includePaths: exportIncludePaths;

    files: [
        "rnnoise/include/rnnoise.h",
        "rnnoise/src/arch.h",
        "rnnoise/src/celt_lpc.c",
        "rnnoise/src/celt_lpc.h",
        "rnnoise/src/common.h",
        "rnnoise/src/denoise.c",
        "rnnoise/src/kiss_fft.c",
        "rnnoise/src/_kiss_fft_guts.h",
        "rnnoise/src/kiss_fft.h",
        "rnnoise/src/opus_types.h",
        "rnnoise/src/pitch.c",
        "rnnoise/src/pitch.h",
        "rnnoise/src/rnn.c",
        "rnnoise/src/rnn_data.c",
        "rnnoise/src/rnn_data.h",
        "rnnoise/src/rnn.h",
        "rnnoise/src/tansig_table.h",
    ]
//     excludeFiles: [
//     ]

    Export {
        Depends { name: "cpp" }
        cpp.systemIncludePaths: product.exportIncludePaths
    }
}
