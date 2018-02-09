import qbs
import QbsUtl
import "toxbase.qbs" as ToxBase

Project {
    name: "ToxCore"

    readonly property string toxPrefix: "toxcore/"
    PropertyOptions {
        name: "toxPrefix"
        description: "Базовая директория расположения tox-библиотеки"
    }

    // LAYER 1: Crypto core
    ToxBase {
        id: toxcrypto
        name: "ToxCrypto"
        targetName: "toxcrypto"

        files: [
            toxPrefix + "toxcore/ccompat.h",
            toxPrefix + "toxcore/crypto_core.c",
            toxPrefix + "toxcore/crypto_core.h",
            toxPrefix + "toxcore/crypto_core_mem.c",
        ]
    }

    // LAYER 2: Basic networking
    ToxBase {
        id: toxnetwork
        name: "ToxNetwork"
        targetName: "toxnetwork"

        files: [
            /* В субпроекте ToxPhone выполнена подмена реализации логгера */
            //toxPrefix + "toxcore/logger.c",
            //toxPrefix + "../toxcore_logger.cpp",

            toxPrefix + "toxcore/logger.h",
            toxPrefix + "toxcore/network.c",
            toxPrefix + "toxcore/network.h",
            toxPrefix + "toxcore/util.c",
            toxPrefix + "toxcore/util.h",
        ]
    }

    // LAYER 3: Distributed Hash Table
    ToxBase {
        id: toxdht
        name: "ToxDHT"
        targetName: "toxdht"

        files: [
            toxPrefix + "toxcore/DHT.c",
            toxPrefix + "toxcore/DHT.h",
            toxPrefix + "toxcore/LAN_discovery.c",
            toxPrefix + "toxcore/LAN_discovery.h",
            toxPrefix + "toxcore/ping.c",
            toxPrefix + "toxcore/ping.h",
            toxPrefix + "toxcore/ping_array.c",
            toxPrefix + "toxcore/ping_array.h",
        ]
    }

    // LAYER 4: Onion routing, TCP connections, crypto connections
    ToxBase {
        id: toxnetcrypto
        name: "ToxNetCrypto"
        targetName: "toxnetcrypto"

        Depends { name: "ToxCrypto" }

        files: [
            toxPrefix + "toxcore/TCP_client.c",
            toxPrefix + "toxcore/TCP_client.h",
            toxPrefix + "toxcore/TCP_connection.c",
            toxPrefix + "toxcore/TCP_connection.h",
            toxPrefix + "toxcore/TCP_server.c",
            toxPrefix + "toxcore/TCP_server.h",
            toxPrefix + "toxcore/list.c",
            toxPrefix + "toxcore/list.h",
            toxPrefix + "toxcore/net_crypto.c",
            toxPrefix + "toxcore/net_crypto.h",
            toxPrefix + "toxcore/onion.c",
            toxPrefix + "toxcore/onion.h",
            toxPrefix + "toxcore/onion_announce.c",
            toxPrefix + "toxcore/onion_announce.h",
            toxPrefix + "toxcore/onion_client.c",
            toxPrefix + "toxcore/onion_client.h",
        ]
    }

    // LAYER 5: Friend requests and connections
    ToxBase {
        id: toxfriends
        name: "ToxFriends"
        targetName: "toxfriends"
        files: [
            toxPrefix + "toxcore/friend_connection.c",
            toxPrefix + "toxcore/friend_connection.h",
            toxPrefix + "toxcore/friend_requests.c",
            toxPrefix + "toxcore/friend_requests.h",
        ]
    }

    // LAYER 6: Tox messenger
    ToxBase {
        id: toxmessenger
        name: "ToxMessenger"
        targetName: "toxmessenger"

        Depends { name: "ToxNetCrypto" }
        Depends { name: "ToxFriends" }

        files: [
            toxPrefix + "toxcore/Messenger.c",
            toxPrefix + "toxcore/Messenger.h",
        ]
    }

    // LAYER 7: Group chats
    ToxBase {
        id: toxgroup
        name: "ToxGroup"
        targetName: "toxgroup"
        files: [
            toxPrefix + "toxcore/group.c",
            toxPrefix + "toxcore/group.h",
        ]
    }

    // LAYER 8: Public API
    ToxBase {
        id: toxcore
        name: "ToxCore"
        targetName: "toxcore"

        Depends { name: "ToxCrypto" }
        Depends { name: "ToxNetwork" }
        Depends { name: "ToxMessenger" }
        Depends { name: "ToxDHT" }
        Depends { name: "ToxGroup" }

        files: [
            toxPrefix + "toxcore/tox_api.c",
            toxPrefix + "toxcore/tox.c",
            toxPrefix + "toxcore/tox.h",

            // :: Audio/Video Library
            toxPrefix + "toxav/audio.c",
            toxPrefix + "toxav/audio.h",
            toxPrefix + "toxav/bwcontroller.c",
            toxPrefix + "toxav/bwcontroller.h",
            toxPrefix + "toxav/groupav.c",
            toxPrefix + "toxav/groupav.h",
            toxPrefix + "toxav/msi.c",
            toxPrefix + "toxav/msi.h",
            toxPrefix + "toxav/ring_buffer.c",
            toxPrefix + "toxav/ring_buffer.h",
            toxPrefix + "toxav/rtp.c",
            toxPrefix + "toxav/rtp.h",
            toxPrefix + "toxav/toxav.c",
            toxPrefix + "toxav/toxav.h",
            toxPrefix + "toxav/toxav_old.c",
            toxPrefix + "toxav/video.c",
            toxPrefix + "toxav/video.h",

            // :: ToxDNS and block encryption libraries
            //toxPrefix + "toxdns/toxdns.c",
            toxPrefix + "toxencryptsave/toxencryptsave.c",
            toxPrefix + "toxencryptsave/toxencryptsave.h",
        ]
    }

//    // :: Audio/Video Library
//    ToxBase {
//        id: toxav
//        name: "ToxAV"
//        targetName: "toxav"

//        Depends { name: "ToxNetwork" }
//        Depends { name: "ToxMessenger" }

//        files: [
//            toxPrefix + "toxav/audio.c",
//            toxPrefix + "toxav/audio.h",
//            toxPrefix + "toxav/bwcontroller.c",
//            toxPrefix + "toxav/bwcontroller.h",
//            toxPrefix + "toxav/groupav.c",
//            toxPrefix + "toxav/groupav.h",
//            toxPrefix + "toxav/msi.c",
//            toxPrefix + "toxav/msi.h",
//            toxPrefix + "toxav/ring_buffer.c",
//            toxPrefix + "toxav/ring_buffer.h",
//            toxPrefix + "toxav/rtp.c",
//            toxPrefix + "toxav/rtp.h",
//            toxPrefix + "toxav/toxav.c",
//            toxPrefix + "toxav/toxav.h",
//            toxPrefix + "toxav/toxav_old.c",
//            toxPrefix + "toxav/video.c",
//            toxPrefix + "toxav/video.h",
//        ]
//    }

//    // :: ToxDNS and block encryption libraries
//    ToxBase {
//        id: toxdns
//        name: "ToxDNS"
//        targetName: "toxdns"
//        files: [
//            toxPrefix + "toxdns/toxdns.c",
//            toxPrefix + "toxencryptsave/toxencryptsave.c",
//            toxPrefix + "toxencryptsave/toxencryptsave.h",
//        ]
//    }

    // :: Bootstrap daemon
    ToxBase {
        id: dht_bootstrap
        type: "application"
        consoleApplication: true
        destinationDirectory: "./bin"
        condition: true

        name: "DHT_Bootstrap"
        targetName: "dht-bootstrap"

        Depends { name: "ToxCrypto" }
        Depends { name: "ToxNetwork" }
        Depends { name: "ToxNetCrypto" }
        Depends { name: "ToxDHT" }

//        cpp.defines : base.concat([
//            "HAVE_CONFIG_H",
//        ])

        cpp.dynamicLibraries: [
            "pthread",
        ]

        cpp.staticLibraries: QbsUtl.concatPaths(
            lib.sodium.staticLibrariesPaths(product)
        )

        files: [
            toxPrefix + "other/DHT_bootstrap.c",
            toxPrefix + "other/bootstrap_node_packets.c",
            toxPrefix + "other/bootstrap_node_packets.h",

            // Реализация оригинального логгера
            toxPrefix + "toxcore/logger.c",
        ]

//        property var test: {
//            console.info("=== cpp.defines ===");
//            console.info(cpp.defines);
//        }
    }

    // :: Bootstrap daemon
    ToxBase {
        id: tox_bootstrap
        type: "application"
        consoleApplication: true
        destinationDirectory: "./bin"
        condition: true

        name: "ToxBootstrap"
        targetName: "tox-bootstrap"

        Depends { name: "ToxCrypto" }
        Depends { name: "ToxNetwork" }
        Depends { name: "ToxNetCrypto" }
        Depends { name: "ToxDHT" }

        cpp.dynamicLibraries: [
            "pthread",
            "config",
        ]

        cpp.staticLibraries: QbsUtl.concatPaths(
            lib.sodium.staticLibrariesPaths(product)
        )

        files: [
            toxPrefix + "other/bootstrap_daemon/src/command_line_arguments.c",
            toxPrefix + "other/bootstrap_daemon/src/command_line_arguments.h",
            toxPrefix + "other/bootstrap_daemon/src/config.c",
            toxPrefix + "other/bootstrap_daemon/src/config.h",
            toxPrefix + "other/bootstrap_daemon/src/config_defaults.h",
            toxPrefix + "other/bootstrap_daemon/src/global.h",
            toxPrefix + "other/bootstrap_daemon/src/log.c",
            toxPrefix + "other/bootstrap_daemon/src/log.h",
            toxPrefix + "other/bootstrap_daemon/src/log_backend_stdout.c",
            toxPrefix + "other/bootstrap_daemon/src/log_backend_stdout.h",
            toxPrefix + "other/bootstrap_daemon/src/log_backend_syslog.c",
            toxPrefix + "other/bootstrap_daemon/src/log_backend_syslog.h",
            toxPrefix + "other/bootstrap_daemon/src/tox-bootstrapd.c",
            toxPrefix + "other/bootstrap_node_packets.c",
            toxPrefix + "other/bootstrap_node_packets.h",

            // Реализация оригинального логгера
            toxPrefix + "toxcore/logger.c",
        ]
    }

}
