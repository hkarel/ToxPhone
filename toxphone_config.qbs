import qbs
import "toxphone_base.qbs" as ToxPhoneBase

ToxPhoneBase {
    name: "ToxPhone Config"

    references: [
        "src/commands/commands.qbs",
        "src/shared/shared.qbs",
        "src/pproto/pproto.qbs",
        "src/yaml/yaml.qbs",
        "src/toxphone_config/toxphone_config.qbs",
    ]
}
