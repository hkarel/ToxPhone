import qbs
import qbs.File
import "qbs/imports/QbsUtl/qbsutl.js" as QbsUtl

Project {
    minimumQbsVersion: "1.10.0"
    qbsSearchPaths: ["qbs"]

    // Признак вывода дополнительной информации в файл package_build_info,
    // используется для сборки deb-пакета
    readonly property bool printPackegeBuildInfo: false

    property string toxPrefix: "toxcore/"
    property string sodiumVersion: "1.0.16"
    //property string ffmpegVersion: "3.3.3"

    Probe {
        id: projectProbe
        readonly property string projectBuildDirectory: project.buildDirectory
        property var projectVersion;
        property string projectGitRevision;
        configure: {
            projectVersion = QbsUtl.getVersions(sourceDirectory + "/VERSION");
            projectGitRevision = QbsUtl.gitRevision(sourceDirectory);
            if (File.exists(projectBuildDirectory + "/package_build_info"))
                File.remove(projectBuildDirectory + "/package_build_info")
        }
    }

    property var cppDefines: {
        var version = projectProbe.projectVersion;
        var def = [
            "VERSION_PROJECT=" + version[0],
            "VERSION_PROJECT_MAJOR=" + version[1],
            "VERSION_PROJECT_MINOR=" + version[2],
            "VERSION_PROJECT_PATCH=" + version[3],
            "GIT_REVISION=\"" + projectProbe.projectGitRevision + "\"",
            "Q_DATA_STREAM_VERSION=QDataStream::Qt_4_8",
            "BPROTOCOL_VERSION_LOW=0",
            "BPROTOCOL_VERSION_HIGH=1",
            "UDP_SIGNATURE=\"TPPR\"", // 'T'OX 'P'HONE 'PR'OJECT
        ]

//        if (qbs.targetOS.contains("windows")
//            && qbs.toolchain && qbs.toolchain.contains("mingw"))
//        {
//            def.push("_POSIX");
//        }

        if (qbs.targetOS.contains("windows")
            && qbs.toolchain && qbs.toolchain.contains("mingw"))
            def.push("CONFIG_DIR=\".config/ToxPhone\"");
        else
            def.push("CONFIG_DIR=\"ToxPhone\"");

        if (qbs.buildVariant === "release")
            def.push("NDEBUG");

        return def;
    }

    property var cxxFlags: [
        "-std=c++11",
        "-ggdb3",
        "-Winline",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Wno-unused-parameter",
        "-Wno-variadic-macros",
        //"-Wconversion",
        //"-fopenmp",
    ]
}
