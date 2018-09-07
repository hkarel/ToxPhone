import qbs

Project {
    name: "3rdparty"
    references: [
        "abseil.qbs",
        "filter_audio.qbs",
        "toxcore.qbs",
        "webrtc.qbs",
    ]
}
