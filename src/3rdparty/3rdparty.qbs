import qbs

Project {
    name: "3rdparty"
    references: [
        //"celt.qbs",
        "filter_audio.qbs",
        "rnnoise.qbs",
        "toxcore.qbs",
    ]
}
