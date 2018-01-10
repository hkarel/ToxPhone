import qbs
import "toxphone_base.qbs" as ToxPhoneBase

ToxPhoneBase {
    name: "ToxPhone Project"

    references: [
        //"src/3rdparty/3rdparty.qbs",
        "src/yaml/yaml.qbs",
        "src/shared/shared.qbs",
        "src/kernel/kernel.qbs",
        "src/toxphone/toxphone.qbs",
        "src/toxphone_config/toxphone_config.qbs",
    ]
}
