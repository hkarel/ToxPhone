import qbs
import QbsUtl
import "toxbase.qbs" as ToxBase

Project {
    name: "ToxCore"

    // LAYER 1: Crypto core
    ToxBase {
        id: toxcrypto
        name: "ToxCrypto"
        targetName: "toxcrypto"

        files: [
            project.toxPrefix + "toxcore/ccompat.h",
            project.toxPrefix + "toxcore/crypto_core.c",
            project.toxPrefix + "toxcore/crypto_core.h",
            project.toxPrefix + "toxcore/crypto_core_mem.c",
        ]
    }

    // LAYER 2: Basic networking
    ToxBase {
        id: toxnetwork
        name: "ToxNetwork"
        targetName: "toxnetwork"

        files: [
            project.toxPrefix + "toxcore/logger.c",
            project.toxPrefix + "toxcore/logger.h",
            project.toxPrefix + "toxcore/network.c",
            project.toxPrefix + "toxcore/network.h",
            project.toxPrefix + "toxcore/util.c",
            project.toxPrefix + "toxcore/util.h",
        ]
    }

    // LAYER 3: Distributed Hash Table
    ToxBase {
        id: toxdht
        name: "ToxDHT"
        targetName: "toxdht"

        files: [
            project.toxPrefix + "toxcore/DHT.c",
            project.toxPrefix + "toxcore/DHT.h",
            project.toxPrefix + "toxcore/LAN_discovery.c",
            project.toxPrefix + "toxcore/LAN_discovery.h",
            project.toxPrefix + "toxcore/ping.c",
            project.toxPrefix + "toxcore/ping.h",
            project.toxPrefix + "toxcore/ping_array.c",
            project.toxPrefix + "toxcore/ping_array.h",
        ]
    }

    // LAYER 4: Onion routing, TCP connections, crypto connections
    ToxBase {
        id: toxnetcrypto
        name: "ToxNetCrypto"
        targetName: "toxnetcrypto"

        Depends { name: "ToxCrypto" }

        files: [
            project.toxPrefix + "toxcore/TCP_client.c",
            project.toxPrefix + "toxcore/TCP_client.h",
            project.toxPrefix + "toxcore/TCP_connection.c",
            project.toxPrefix + "toxcore/TCP_connection.h",
            project.toxPrefix + "toxcore/TCP_server.c",
            project.toxPrefix + "toxcore/TCP_server.h",
            project.toxPrefix + "toxcore/list.c",
            project.toxPrefix + "toxcore/list.h",
            project.toxPrefix + "toxcore/net_crypto.c",
            project.toxPrefix + "toxcore/net_crypto.h",
            project.toxPrefix + "toxcore/onion.c",
            project.toxPrefix + "toxcore/onion.h",
            project.toxPrefix + "toxcore/onion_announce.c",
            project.toxPrefix + "toxcore/onion_announce.h",
            project.toxPrefix + "toxcore/onion_client.c",
            project.toxPrefix + "toxcore/onion_client.h",
        ]
    }

    // LAYER 5: Friend requests and connections
    ToxBase {
        id: toxfriends
        name: "ToxFriends"
        targetName: "toxfriends"
        files: [
            project.toxPrefix + "toxcore/friend_connection.c",
            project.toxPrefix + "toxcore/friend_connection.h",
            project.toxPrefix + "toxcore/friend_requests.c",
            project.toxPrefix + "toxcore/friend_requests.h",
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
            project.toxPrefix + "toxcore/Messenger.c",
            project.toxPrefix + "toxcore/Messenger.h",
        ]
    }

    // LAYER 7: Group chats
    ToxBase {
        id: toxgroup
        name: "ToxGroup"
        targetName: "toxgroup"
        files: [
            project.toxPrefix + "toxcore/group.c",
            project.toxPrefix + "toxcore/group.h",
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
            project.toxPrefix + "toxcore/tox_api.c",
            project.toxPrefix + "toxcore/tox.c",
            project.toxPrefix + "toxcore/tox.h",

            // :: Audio/Video Library
            project.toxPrefix + "toxav/audio.c",
            project.toxPrefix + "toxav/audio.h",
            project.toxPrefix + "toxav/bwcontroller.c",
            project.toxPrefix + "toxav/bwcontroller.h",
            project.toxPrefix + "toxav/groupav.c",
            project.toxPrefix + "toxav/groupav.h",
            project.toxPrefix + "toxav/msi.c",
            project.toxPrefix + "toxav/msi.h",
            project.toxPrefix + "toxav/ring_buffer.c",
            project.toxPrefix + "toxav/ring_buffer.h",
            project.toxPrefix + "toxav/rtp.c",
            project.toxPrefix + "toxav/rtp.h",
            project.toxPrefix + "toxav/toxav.c",
            project.toxPrefix + "toxav/toxav.h",
            project.toxPrefix + "toxav/toxav_old.c",
            project.toxPrefix + "toxav/video.c",
            project.toxPrefix + "toxav/video.h",

            // :: ToxDNS and block encryption libraries
            //project.toxPrefix + "toxdns/toxdns.c",
            project.toxPrefix + "toxencryptsave/toxencryptsave.c",
            project.toxPrefix + "toxencryptsave/toxencryptsave.h",
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
//            project.toxPrefix + "toxav/audio.c",
//            project.toxPrefix + "toxav/audio.h",
//            project.toxPrefix + "toxav/bwcontroller.c",
//            project.toxPrefix + "toxav/bwcontroller.h",
//            project.toxPrefix + "toxav/groupav.c",
//            project.toxPrefix + "toxav/groupav.h",
//            project.toxPrefix + "toxav/msi.c",
//            project.toxPrefix + "toxav/msi.h",
//            project.toxPrefix + "toxav/ring_buffer.c",
//            project.toxPrefix + "toxav/ring_buffer.h",
//            project.toxPrefix + "toxav/rtp.c",
//            project.toxPrefix + "toxav/rtp.h",
//            project.toxPrefix + "toxav/toxav.c",
//            project.toxPrefix + "toxav/toxav.h",
//            project.toxPrefix + "toxav/toxav_old.c",
//            project.toxPrefix + "toxav/video.c",
//            project.toxPrefix + "toxav/video.h",
//        ]
//    }

//    // :: ToxDNS and block encryption libraries
//    ToxBase {
//        id: toxdns
//        name: "ToxDNS"
//        targetName: "toxdns"
//        files: [
//            project.toxPrefix + "toxdns/toxdns.c",
//            project.toxPrefix + "toxencryptsave/toxencryptsave.c",
//            project.toxPrefix + "toxencryptsave/toxencryptsave.h",
//        ]
//    }

    // :: Bootstrap daemon
    ToxBase {
        id: DHT_bootstrap
        type: "application"
        destinationDirectory: "./bin"
        condition: false

        name: "DHT_Bootstrap"
        targetName: "DHT_bootstrap"

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
            project.toxPrefix + "other/DHT_bootstrap.c",
            project.toxPrefix + "other/bootstrap_node_packets.c",
            project.toxPrefix + "other/bootstrap_node_packets.h",
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
        destinationDirectory: "./bin"

        condition: false
        //condition: !qbs.targetOS.contains("windows")

        name: "ToxBootstrap"
        targetName: "tox-bootstrap"

        Depends { name: "ToxCrypto" }
        //Depends { name: "ToxNetwork" }
        //Depends { name: "ToxNetCrypto" }
        //Depends { name: "ToxDHT" }

        cpp.dynamicLibraries: [
            "pthread",
        ]

        cpp.staticLibraries: QbsUtl.concatPaths(
            lib.sodium.staticLibrariesPaths(product)
        )

        files: [
            project.toxPrefix + "other/bootstrap_daemon/src/command_line_arguments.c",
            project.toxPrefix + "other/bootstrap_daemon/src/command_line_arguments.h",
            project.toxPrefix + "other/bootstrap_daemon/src/config.c",
            project.toxPrefix + "other/bootstrap_daemon/src/config.h",
            project.toxPrefix + "other/bootstrap_daemon/src/config_defaults.h",
            project.toxPrefix + "other/bootstrap_daemon/src/global.h",
            project.toxPrefix + "other/bootstrap_daemon/src/log.c",
            project.toxPrefix + "other/bootstrap_daemon/src/log.h",
            project.toxPrefix + "other/bootstrap_daemon/src/log_backend_stdout.c",
            project.toxPrefix + "other/bootstrap_daemon/src/log_backend_stdout.h",
            project.toxPrefix + "other/bootstrap_daemon/src/log_backend_syslog.c",
            project.toxPrefix + "other/bootstrap_daemon/src/log_backend_syslog.h",
            project.toxPrefix + "other/bootstrap_daemon/src/tox-bootstrapd.c",
            project.toxPrefix + "other/bootstrap_node_packets.c",
            project.toxPrefix + "other/bootstrap_node_packets.h",
        ]
    }

}
