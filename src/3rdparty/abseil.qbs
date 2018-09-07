import qbs
import GccUtl

Product {
    type: "staticlibrary"

    name: "Abseil"
    targetName: "abseil"

    Depends { name: "cpp" }

    cpp.archiverName: GccUtl.ar(cpp.toolchainPathPrefix)
    cpp.cxxFlags: project.cxxFlags

    property var exportIncludePaths: [
        "./abseil",
    ]
    cpp.includePaths: exportIncludePaths;

    files: [
        "abseil/absl/algorithm/*.cc",
        "abseil/absl/algorithm/*.h",
        "abseil/absl/base/internal/*.cc",
        "abseil/absl/base/internal/*.h",
        "abseil/absl/base/*.cc",
        "abseil/absl/base/*.h",
        "abseil/absl/types/*.cc",
        "abseil/absl/types/*.h",
        "abseil/absl/types/internal/*.cc",
        "abseil/absl/types/internal/*.h",
    ]
     excludeFiles: [
         "abseil/absl/algorithm/*_test.cc",
         "abseil/absl/algorithm/*_benchmark.cc",
         "abseil/absl/base/internal/*_test.cc",
         "abseil/absl/base/internal/*_testing.*",
         "abseil/absl/base/internal/*_benchmark.cc",
         "abseil/absl/base/*_test.cc",
         "abseil/absl/base/*_benchmark.cc",
         "abseil/absl/base/spinlock_test_common.cc",
         "abseil/absl/types/*_test.cc",
         "abseil/absl/types/*_benchmark.cc",
         "abseil/absl/types/internal/*_test.cc",
     ]

    Export {
        Depends { name: "cpp" }
        cpp.systemIncludePaths: product.exportIncludePaths
    }
}
