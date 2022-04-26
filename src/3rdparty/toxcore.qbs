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

    ToxBase {
        id: toxcore
        name: "ToxCore"
        targetName: "toxcore"

        files: [
            toxPrefix + "third_party/cmp/cmp.c",
            toxPrefix + "third_party/cmp/cmp.h",
            toxPrefix + "toxcore/announce.c",
            toxPrefix + "toxcore/announce.h",
            toxPrefix + "toxcore/bin_pack.c",
            toxPrefix + "toxcore/bin_pack.h",
            toxPrefix + "toxcore/bin_unpack.c",
            toxPrefix + "toxcore/bin_unpack.h",
            toxPrefix + "toxcore/ccompat.c",
            toxPrefix + "toxcore/ccompat.h",
            toxPrefix + "toxcore/crypto_core.c",
            toxPrefix + "toxcore/crypto_core.h",
            toxPrefix + "toxcore/DHT.c",
            toxPrefix + "toxcore/DHT.h",
            toxPrefix + "toxcore/events/conference_connected.c",
            toxPrefix + "toxcore/events/conference_invite.c",
            toxPrefix + "toxcore/events/conference_message.c",
            toxPrefix + "toxcore/events/conference_peer_list_changed.c",
            toxPrefix + "toxcore/events/conference_peer_name.c",
            toxPrefix + "toxcore/events/conference_title.c",
            toxPrefix + "toxcore/events/events_alloc.c",
            toxPrefix + "toxcore/events/events_alloc.h",
            toxPrefix + "toxcore/events/file_chunk_request.c",
            toxPrefix + "toxcore/events/file_recv.c",
            toxPrefix + "toxcore/events/file_recv_chunk.c",
            toxPrefix + "toxcore/events/file_recv_control.c",
            toxPrefix + "toxcore/events/friend_connection_status.c",
            toxPrefix + "toxcore/events/friend_lossless_packet.c",
            toxPrefix + "toxcore/events/friend_lossy_packet.c",
            toxPrefix + "toxcore/events/friend_message.c",
            toxPrefix + "toxcore/events/friend_name.c",
            toxPrefix + "toxcore/events/friend_read_receipt.c",
            toxPrefix + "toxcore/events/friend_request.c",
            toxPrefix + "toxcore/events/friend_status.c",
            toxPrefix + "toxcore/events/friend_status_message.c",
            toxPrefix + "toxcore/events/friend_typing.c",
            toxPrefix + "toxcore/events/self_connection_status.c",
            toxPrefix + "toxcore/forwarding.c",
            toxPrefix + "toxcore/forwarding.h",
            toxPrefix + "toxcore/friend_connection.c",
            toxPrefix + "toxcore/friend_connection.h",
            toxPrefix + "toxcore/friend_requests.c",
            toxPrefix + "toxcore/friend_requests.h",
            toxPrefix + "toxcore/group.c",
            toxPrefix + "toxcore/group.h",
            toxPrefix + "toxcore/group_announce.c",
            toxPrefix + "toxcore/group_announce.h",
            toxPrefix + "toxcore/group_moderation.c",
            toxPrefix + "toxcore/group_moderation.h",
            toxPrefix + "toxcore/LAN_discovery.c",
            toxPrefix + "toxcore/LAN_discovery.h",
            toxPrefix + "toxcore/list.c",
            toxPrefix + "toxcore/list.h",

            /* В субпроекте ToxPhone выполнена подмена реализации логгера */
            //toxPrefix + "toxcore/logger.c",

            toxPrefix + "toxcore/logger.h",
            toxPrefix + "toxcore/Messenger.c",
            toxPrefix + "toxcore/Messenger.h",
            toxPrefix + "toxcore/mono_time.c",
            toxPrefix + "toxcore/mono_time.h",
            toxPrefix + "toxcore/net_crypto.c",
            toxPrefix + "toxcore/net_crypto.h",
            toxPrefix + "toxcore/network.c",
            toxPrefix + "toxcore/network.h",
            toxPrefix + "toxcore/onion_announce.c",
            toxPrefix + "toxcore/onion_announce.h",
            toxPrefix + "toxcore/onion.c",
            toxPrefix + "toxcore/onion_client.c",
            toxPrefix + "toxcore/onion_client.h",
            toxPrefix + "toxcore/onion.h",
            toxPrefix + "toxcore/ping_array.c",
            toxPrefix + "toxcore/ping_array.h",
            toxPrefix + "toxcore/ping.c",
            toxPrefix + "toxcore/ping.h",
            toxPrefix + "toxcore/state.c",
            toxPrefix + "toxcore/state.h",
            toxPrefix + "toxcore/TCP_client.c",
            toxPrefix + "toxcore/TCP_client.h",
            toxPrefix + "toxcore/TCP_common.c",
            toxPrefix + "toxcore/TCP_common.h",
            toxPrefix + "toxcore/TCP_connection.c",
            toxPrefix + "toxcore/TCP_connection.h",
            toxPrefix + "toxcore/TCP_server.c",
            toxPrefix + "toxcore/TCP_server.h",
            toxPrefix + "toxcore/timed_auth.c",
            toxPrefix + "toxcore/timed_auth.h",
            toxPrefix + "toxcore/tox_api.c",
            toxPrefix + "toxcore/tox.c",
            toxPrefix + "toxcore/tox_dispatch.c",
            toxPrefix + "toxcore/tox_dispatch.h",
            toxPrefix + "toxcore/tox_events.c",
            toxPrefix + "toxcore/tox_events.h",
            toxPrefix + "toxcore/tox.h",
            toxPrefix + "toxcore/tox_private.c",
            toxPrefix + "toxcore/tox_private.h",
            toxPrefix + "toxcore/tox_unpack.c",
            toxPrefix + "toxcore/tox_unpack.h",
            toxPrefix + "toxcore/util.c",
            toxPrefix + "toxcore/util.h",

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

            // :: Block encryption libraries
            toxPrefix + "toxencryptsave/toxencryptsave.c",
            toxPrefix + "toxencryptsave/toxencryptsave.h",
        ]
    }

    // :: Bootstrap daemon
    ToxBase {
        id: dht_bootstrap
        type: "application"
        consoleApplication: true
        destinationDirectory: "./bin"
        condition: false

        name: "DHT_Bootstrap"
        targetName: "dht-bootstrap"

        Depends { name: "ToxCore" }

        //cpp.defines : base.concat([
        //    "HAVE_CONFIG_H",
        //])

        cpp.dynamicLibraries: QbsUtl.concatPaths(
            ["pthread"],
            lib.sodium.dynamicLibraries
        );

        files: [
            toxPrefix + "other/DHT_bootstrap.c",
            toxPrefix + "other/bootstrap_node_packets.c",
            toxPrefix + "other/bootstrap_node_packets.h",
            toxPrefix + "testing/misc_tools.c",
            toxPrefix + "testing/misc_tools.h",

            // Реализация оригинального логгера
            toxPrefix + "toxcore/logger.c",
        ]
    }

    // :: Bootstrap daemon
    ToxBase {
        id: tox_bootstrap
        type: "application"
        consoleApplication: true
        destinationDirectory: "./bin"
        condition: false

        name: "ToxBootstrap"
        targetName: "tox-bootstrap"

        Depends { name: "ToxCore" }

        cpp.dynamicLibraries: QbsUtl.concatPaths(
            ["pthread", "config"],
            lib.sodium.dynamicLibraries
        );

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
