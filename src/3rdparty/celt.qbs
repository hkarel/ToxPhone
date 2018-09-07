import qbs
import GccUtl

Product {
    type: "staticlibrary"

    name: "Celt"
    targetName: "celt"

    Depends { name: "cpp" }

    cpp.archiverName: GccUtl.ar(cpp.toolchainPathPrefix)

    cpp.defines: [
        "HAVE_STDINT_H",
        "VAR_ARRAYS",
        "HAVE_LRINTF",
        //"USE_SIMD",
        "FIXED_POINT",
    ]

    cpp.cFlags: [
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
        "-Wno-parentheses",
        "-Wunused-variable",
        "-Wno-unused-but-set-variable",
    ]

    property var exportIncludePaths: [
        "./celt",
        "./celt/libcelt",
    ]
    cpp.includePaths: [
        //"./celt",
    ].concat(exportIncludePaths);

    files: [
        "celt/libcelt/*.c",
        "celt/libcelt/*.h",
    ]
    excludeFiles: [
        "celt/libcelt/plc.c",
        "celt/libcelt/c64_fft.*",
        "celt/libcelt/testcelt.c",
    ]

    Export {
        Depends { name: "cpp" }
        cpp.systemIncludePaths: product.exportIncludePaths
    }
}
