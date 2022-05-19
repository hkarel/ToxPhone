import qbs

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

     property var includePaths: [
         "./",
         "./toxfunc",
     ]
     cpp.includePaths: includePaths;

     // Декларация нужна для подавления Qt warning-ов
     cpp.systemIncludePaths: Qt.core.cpp.includePaths

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
         cpp.includePaths: exportingProduct.includePaths
     }
}
