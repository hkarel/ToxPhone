import qbs
import qbs.TextFile
import QbsUtl
import GccUtl

Product {
    type: "application"
    destinationDirectory: "./bin"

    name: "Example3"
    targetName: "example3"

    Depends { name: "cpp" }

    cpp.defines: {
        var def = [
            "HAVE_ATOMIC_BUILTINS",
            "PACKAGE",
        ];
        return def;
    }

    cpp.cFlags: [
        "-ggdb3",
        //"-Wall",
        //"-Wextra",
        "-Wno-unused-parameter",
        "-Wno-variadic-macros",
        "-Wno-unused-but-set-variable",
        "-Wno-unused-label",
    ]

    cpp.cxxFlags: [
        "-std=c++11",
        "-ggdb3",
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-variadic-macros",
    ]

    cpp.includePaths: [
        "./",
        "../",
    ]

    cpp.dynamicLibraries: {
        var libs = [
            "pthread",
            "pulse",
            "sndfile",
         ];
        return libs;
    }

    files: [
//        "pulsecore/core-util.c",
//        "pulsecore/core-util.h",
//        "pulsecore/sndfile-util.c",
//        "pulsecore/sndfile-util.h",
//        "pulsecore/strbuf.c",
//        "pulsecore/strbuf.h",
//        "pulsecore/thread-posix.c",
//        "pulsecore/thread.h",
//        "pulsecore/strlist.c",
//        "pulsecore/strlist.h",
//        "pulsecore/core-error.c",
//        "pulsecore/core-error.h",
        //"pulsecore/*.c",
        //"pulsecore/*.h",
        "pulsecore/log.c",
        "pulsecore/log.h",

        "example3.c",
    ]
//    excludeFiles: [
//        "pulsecore/socket-server.*",
//        "pulsecore/mix_neon.*",
//        "pulsecore/semaphore-win32.*",
//        "pulsecore/protocol-dbus.*",
//        "pulsecore/mutex-win32.*",
//        "pulsecore/socket-util.*",
//        "pulsecore/socket-client.*",
//        "pulsecore/protocol-http.*",
//        "pulsecore/semaphore-osx.*",
//        "pulsecore/cli-command.*",
//        "pulsecore/rtkit.*",
//        "pulsecore/pipe.*",
//        "pulsecore/ipacl.*",
//        "pulsecore/sink*",
//        "pulsecore/sconv*",
//        "pulsecore/thread-win32*",
//        "pulsecore/database-gdbm*",
//        "pulsecore/parseaddr*",
//        "pulsecore/dbus*",
//    ]


} // Product
