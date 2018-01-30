import qbs
import "toxphone_base.qbs" as ToxPhoneBase

ToxPhoneBase {
    name: "ToxPhone Config"

    references: [
        "src/yaml/yaml.qbs",
        "src/shared/shared.qbs",
        "src/kernel/kernel.qbs",
        "src/toxphone_config/toxphone_config.qbs",
    ]
}
