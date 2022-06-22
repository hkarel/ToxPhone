import qbs
import "toxphone_base.qbs" as ToxPhoneBase

ToxPhoneBase {
    name: "ToxPhone Project"

    references: [
        "src/3rdparty/3rdparty.qbs",
        "src/commands/commands.qbs",
        "src/shared/shared.qbs",
        "src/pproto/pproto.qbs",
        "src/toxfunc/toxfunc.qbs",
        "src/toxphone/toxphone.qbs",
        "src/toxphone_config/toxphone_config.qbs",
        "src/yaml/yaml.qbs",
        "setup/package_build.qbs",
        //"src/examples/example2.qbs",
        //"src/examples/example3.qbs",

    ]
}
