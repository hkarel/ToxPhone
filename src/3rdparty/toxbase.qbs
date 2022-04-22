import qbs
import QbsUtl
import ProbExt

Product {
    type: "staticlibrary"
    destinationDirectory: "./lib"

    Depends { name: "cpp" }
    Depends { name: "lib.sodium" }

    lib.sodium.version:   project.sodiumVersion
    lib.sodium.useSystem: project.useSystemSodium

    ProbExt.LibValidationProbe {
        id: libValidation
        checkingLibs: [lib.sodium]
    }

    cpp.defines: [
        "USE_IPV6=1",
        "TCP_SERVER_USE_EPOLL",
        "MIN_LOGGER_LEVEL=2",
    ]

    property var warnFlags: [
        // Add all warning flags we can.
        "-Wall",
        "-Wextra",
        //"-Weverything",
        "-Wno-unused-variable",

        // Disable specific warning flags for both C and C++.
        "-Wno-cast-align",
        "-Wno-conversion",
        "-Wno-covered-switch-default",

        // Due to clang's tolower() macro being recursive https://github.com/TokTok/c-toxcore/pull/481
        "-Wno-disabled-macro-expansion",
        "-Wno-documentation-deprecated-sync",
        "-Wno-format-nonliteral",
        "-Wno-missing-field-initializers",
        "-Wno-missing-prototypes",
        "-Wno-padded",
        "-Wno-parentheses",
        "-Wno-return-type",
        "-Wno-sign-compare",
        "-Wno-sign-conversion",
        "-Wno-tautological-constant-out-of-range-compare",

        // Our use of mutexes results in a false positive, see 1bbe446
        "-Wno-thread-safety-analysis",
        "-Wno-type-limits",
        "-Wno-undef",
        "-Wno-unreachable-code",
        "-Wno-unused-macros",
        "-Wno-unused-parameter",
        "-Wno-vla",

        // Disable specific warning flags for C.
        "-Wno-assign-enum",
        "-Wno-bad-function-cast",
        "-Wno-double-promotion",
        "-Wno-gnu-zero-variadic-macro-arguments",
        "-Wno-packed",
        "-Wno-reserved-id-macro",
        "-Wno-shadow",
        "-Wno-shorten-64-to-32",
        "-Wno-unreachable-code-return",
        "-Wno-unused-but-set-variable",
        "-Wno-used-but-marked-unused",

//        // Disable specific warning flags for C++.
//        "-Wno-c++11-compat",
//        "-Wno-c++11-extensions",
//        "-Wno-c++11-narrowing",
//        "-Wno-c99-extensions",
//        "-Wno-narrowing",
//        "-Wno-old-style-cast",
//        "-Wno-variadic-macros",
//        "-Wno-vla-extension",
    ]

    cpp.cFlags: [
        "-std=c99",
        "-Wno-unused-function",
        //"-pedantic", // Warn on non-ISO C.
    ].concat(warnFlags)

    cpp.cxxFlags: [
        "-std=c++17",
        //"-Wno-unused-parameter",
    ].concat(warnFlags)

    property var exportIncludePaths: [
        "./toxcore",
    ]
    cpp.systemIncludePaths: QbsUtl.concatPaths(
        ["/usr/include/opus"]
       ,lib.sodium.includePath
    )

    cpp.rpaths: QbsUtl.concatPaths(
        lib.sodium.libraryPath
       ,"$ORIGIN/../lib"
    )

    cpp.libraryPaths: QbsUtl.concatPaths(
        lib.sodium.libraryPath
       ,project.buildDirectory + "/lib"
    )

    Export {
        Depends { name: "cpp" }
        cpp.includePaths: product.exportIncludePaths
    }
}
