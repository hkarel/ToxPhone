import qbs
import qbs.FileInfo

Product {
    name: "ToxFunc"
    targetName: "toxfunc"

    type: "staticlibrary"

    Depends { name: "cpp" }
    Depends { name: "PProto" }
    Depends { name: "SharedLib" }
    Depends { name: "ToxCore" }
    Depends { name: "Qt"; submodules: ["core", "network"] }

    cpp.defines: project.cppDefines
    cpp.cxxFlags: project.cxxFlags
    cpp.cxxLanguageVersion: project.cxxLanguageVersion

     cpp.includePaths: [".", "toxfunc"]

     files: [
         "toxfunc/pproto_error.h",
         "toxfunc/tox_error.cpp",
         "toxfunc/tox_error.h",
         "toxfunc/tox_func.cpp",
         "toxfunc/tox_func.h",
         "toxfunc/tox_logger.cpp",
         "toxfunc/tox_logger.h",
     ]

     Export {
         Depends { name: "cpp" }
         cpp.includePaths: [
             FileInfo.joinPaths(exportingProduct.sourceDirectory, "."),
             FileInfo.joinPaths(exportingProduct.sourceDirectory, "toxfunc"),
         ]
     }
}
