import qbs
import "toxphone_base.qbs" as ToxPhoneBase

ToxPhoneBase {
    name: "ToxPhone Project"

    references: [
        "setup/packege_build.qbs",
        "src/3rdparty/3rdparty.qbs",
        "src/yaml/yaml.qbs",
        "src/shared/shared.qbs",
        "src/kernel/kernel.qbs",
        "src/toxphone/toxphone.qbs",
        "src/toxphone_config/toxphone_config.qbs",
        //"src/examples/example2.qbs",
        //"src/examples/example3.qbs",

    ]
}
