import qbs
import qbs.File
import "qbs/imports/QbsUtl/qbsutl.js" as QbsUtl
import "qbs/imports/ProbExt/OsProbe.qbs" as OsProbe

Project {
    minimumQbsVersion: "1.20.0"
    qbsSearchPaths: ["qbs"]

    // Признак вывода дополнительной информации в файл package_build_info,
    // используется для сборки deb-пакета
    readonly property bool printPackegeBuildInfo: false

    readonly property string sodiumVersion: "1.0.18"
    readonly property bool   useSystemSodium: false

    readonly property string osName: osProbe.osName
    readonly property string osVersion: osProbe.osVersion

    readonly property var projectVersion: projectProbe.projectVersion
    readonly property string projectGitRevision: projectProbe.projectGitRevision

    OsProbe {
        id: osProbe
    }
    Probe {
        id: projectProbe
        property var projectVersion;
        property string projectGitRevision;

        readonly property string projectBuildDirectory: project.buildDirectory
        readonly property string projectSourceDirectory: project.sourceDirectory
        configure: {
            projectVersion = QbsUtl.getVersions(projectSourceDirectory + "/VERSION");
            projectGitRevision = QbsUtl.gitRevision(projectSourceDirectory);
            if (File.exists(projectBuildDirectory + "/package_build_info"))
                File.remove(projectBuildDirectory + "/package_build_info")
        }
    }

    property var cppDefines: {
        var def = [
            "VERSION_PROJECT=" + projectVersion[0],
            "VERSION_PROJECT_MAJOR=" + projectVersion[1],
            "VERSION_PROJECT_MINOR=" + projectVersion[2],
            "VERSION_PROJECT_PATCH=" + projectVersion[3],
            "GIT_REVISION=\"" + projectGitRevision + "\"",
            "QDATASTREAM_VERSION=QDataStream::Qt_5_12",
            "PPROTO_VERSION_LOW=0",
            "PPROTO_VERSION_HIGH=0",
            "PPROTO_QBINARY_SERIALIZE",
            "PPROTO_UDP_SIGNATURE=\"TPPR\"", // 'T'OX 'P'HONE 'PR'OJECT
            //"UDP_LONGSIG=1",
            //"UDP_SIGNATURE=\"TOXPHONE\"", // Long signature
            "LOGGER_LESS_SNPRINTF",
            "TOX_MESSAGE_SIGNATURE=\"TOXPHONE\"",
            "TOX_PHONE=\"ToxPhone\"",
            "CONFIG_DIR=\"/etc/toxphone\"",
            "VAROPT_DIR=\"/var/opt/toxphone\"",
        ]
        return def;
    }

    property var cxxFlags: [
        //"-std=c++11",
        "-ggdb3",
        "-Winline",
        "-Wall",
        "-Wextra",
        //"-Wpedantic",
        "-Wno-unused-parameter",
        "-Wno-variadic-macros",
        //"-Wconversion",
    ]
    property string cxxLanguageVersion: "c++17"
}
