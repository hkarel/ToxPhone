import qbs
import GccUtl

Product {
    type: "staticlibrary"

    name: "WebRTC"
    targetName: "webrtc"

    Depends { name: "cpp" }
    Depends { name: "Abseil" }

    cpp.archiverName: GccUtl.ar(cpp.toolchainPathPrefix)
    cpp.defines: [
        "WEBRTC_POSIX",
        "WEBRTC_APM_DEBUG_DUMP=0",
    ]

    cpp.cFlags: [
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
    ]
    cpp.cxxFlags: project.cxxFlags

    property var exportIncludePaths: [
        "./webrtc",
    ]
    cpp.includePaths: exportIncludePaths;

    property var filesDir: [
        "api/audio",
        "common_audio",
        "common_audio/include",
        "common_audio/resampler",
        "common_audio/resampler/include",
        "common_audio/signal_processing",
        "common_audio/signal_processing/include",
        "common_audio/third_party/fft4g",
        "common_audio/third_party/spl_sqrt_floor",
        "modules/audio_processing/",
        "modules/audio_processing/aec",
        "modules/audio_processing/aec3",
        "modules/audio_processing/aecm",
        "modules/audio_processing/agc2",
        "modules/audio_processing/agc2/rnn_vad",
        "modules/audio_processing/audio_generator",
        "modules/audio_processing/echo_detector",
        "modules/audio_processing/include",
        "modules/audio_processing/logging",
        "modules/audio_processing/utility",
        "rtc_base",
        "rtc_base/memory",
        "rtc_base/numerics",
        "rtc_base/strings",
        "rtc_base/system",
        "rtc_base/third_party/base64",
        "rtc_base/third_party/sigslot",
        "system_wrappers/include",
    ]

    files: {
        var fls = [];
        for (var i in filesDir) {
            fls.push("webrtc/" + filesDir[i] + "/*.cc");
            fls.push("webrtc/" + filesDir[i] + "/*.c");
            fls.push("webrtc/" + filesDir[i] + "/*.h");
        }
        fls.push("webrtc/common_types.h");
        return fls;
    }

    excludeFiles: {
        var fls = [];
        for (var i in filesDir) {
             fls.push("webrtc/" + filesDir[i] + "/*_unittest.cc");
             fls.push("webrtc/" + filesDir[i] + "/*_mips.cc");
             fls.push("webrtc/" + filesDir[i] + "/*_mips.c");
        }

        //fls.push("webrtc/modules/audio_processing/utility/ooura_fft_mips.cc");
        //fls.push("webrtc/common_audio/signal_processing/min_max_operations_mips.c");

        var simdFiles = [];
        if (qbs.architecture.startsWith("arm") === true) {
            for (var i in filesDir) {
                simdFiles.push("webrtc/" + filesDir[i] + "/*_sse.cc");
                simdFiles.push("webrtc/" + filesDir[i] + "/*_sse.c");
                simdFiles.push("webrtc/" + filesDir[i] + "/*_sse.h");
            }

            //fls.push("webrtc/modules/audio_processing/utility/ooura_fft_sse2.cc");
            //fls.push("webrtc/common_audio/fir_filter_sse.*");
            //fls.push("webrtc/common_audio/signal_processing/min_max_operations.c");
         }
        else {
            for (var i in filesDir) {
                simdFiles.push("webrtc/" + filesDir[i] + "/*_neon.cc");
                simdFiles.push("webrtc/" + filesDir[i] + "/*_neon.c");
                simdFiles.push("webrtc/" + filesDir[i] + "/*_neon.h");
            }

            //fls.push("webrtc/modules/audio_processing/utility/ooura_fft_neon.cc");
            //fls.push("webrtc/common_audio/fir_filter_neon.*");
            //fls.push("webrtc/common_audio/signal_processing/min_max_operations_neon.c");
        }
        fls = fls.concat(simdFiles);

        return fls;
    }

    Export {
        Depends { name: "cpp" }
        cpp.systemIncludePaths: product.exportIncludePaths
    }
}
