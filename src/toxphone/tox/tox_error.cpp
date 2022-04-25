#include "tox_error.h"
#include "commands/error.h"

#include <QtGlobal>
#include <QObject>

using namespace pproto;

static const char* unknown_error  = QT_TRANSLATE_NOOP("ToxError", "Unknown error");
static const char* return_success = QT_TRANSLATE_NOOP("ToxError", "The function returned successfully");

bool toxError(TOX_ERR_NEW code, data::MessageError& err)
{
    const char* msg;
    err = error::tox_err_new;

    switch (code)
    {
        /**
         * The function returned successfully.
         */
        case TOX_ERR_NEW_OK:
            msg = return_success;
            break;

        /**
         * One of the arguments to the function was NULL when it was not expected.
         */
        case TOX_ERR_NEW_NULL:
            msg = QT_TRANSLATE_NOOP("ToxError", "One of the arguments to the function was NULL "
                                                "when it was not expected");
            break;

        /**
         * The function was unable to allocate enough memory to store the internal
         * structures for the Tox object.
         */
        case TOX_ERR_NEW_MALLOC:
            msg = QT_TRANSLATE_NOOP("ToxError", "The function was unable to allocate enough memory");
            break;

        /**
         * The function was unable to bind to a port. This may mean that all ports
         * have already been bound, e.g. by other Tox instances, or it may mean
         * a permission error. You may be able to gather more information from errno.
         */
        case TOX_ERR_NEW_PORT_ALLOC:
            msg = QT_TRANSLATE_NOOP("ToxError", "The function was unable to bind to a port");
            break;

        /**
         * proxy_type was invalid.
         */
        case TOX_ERR_NEW_PROXY_BAD_TYPE:
            msg = QT_TRANSLATE_NOOP("ToxError", "proxy_type was invalid");
            break;

        /**
         * proxy_type was valid but the proxy_host passed had an invalid format
         * or was NULL.
         */
        case TOX_ERR_NEW_PROXY_BAD_HOST:
            msg = QT_TRANSLATE_NOOP("ToxError", "proxy_type was valid but the proxy_host passed had "
                                                "an invalid format or was NULL");
            break;

        /**
         * proxy_type was valid, but the proxy_port was invalid.
         */
        case TOX_ERR_NEW_PROXY_BAD_PORT:
            msg = QT_TRANSLATE_NOOP("ToxError", "proxy_type was valid, but the proxy_port was invalid");
            break;

        /**
         * The proxy address passed could not be resolved.
         */
        case TOX_ERR_NEW_PROXY_NOT_FOUND:
            msg = QT_TRANSLATE_NOOP("ToxError", "The proxy address passed could not be resolved");
            break;

        /**
         * The byte array to be loaded contained an encrypted save.
         */
        case TOX_ERR_NEW_LOAD_ENCRYPTED:
            msg = QT_TRANSLATE_NOOP("ToxError", "The byte array to be loaded contained an encrypted save");
            break;

        /**
         * The data format was invalid. This can happen when loading data that was
         * saved by an older version of Tox, or when the data has been corrupted.
         * When loading from badly formatted data, some data may have been loaded,
         * and the rest is discarded. Passing an invalid length parameter also
         * causes this error.
         */
        case TOX_ERR_NEW_LOAD_BAD_FORMAT:
            msg = QT_TRANSLATE_NOOP("ToxError", "The data format was invalid");
            break;

        default:
            msg = unknown_error;
    }

    // В группу ошибки записываем tox-код ошибки
    // err.group = static_cast<qint32>(code);
    err.description = QObject::tr(msg);
    return (code != TOX_ERR_NEW_OK);
}

bool toxError(TOXAV_ERR_CALL code, data::MessageError& err)
{
    const char* msg;
    err = error::toxav_err_call;

    switch (code)
    {
        /**
         * The function returned successfully.
         */
        case TOXAV_ERR_CALL_OK:
            msg = return_success;
            break;

        /**
         * A resource allocation error occurred while trying to create the structures
         * required for the call.
         */
        case TOXAV_ERR_CALL_MALLOC:
            msg = QT_TRANSLATE_NOOP("ToxError", "The function was unable to allocate enough memory");
            break;

        /**
         * Synchronization error occurred.
         */
        case TOXAV_ERR_CALL_SYNC:
            msg = QT_TRANSLATE_NOOP("ToxError", "Synchronization error occurred");
            break;

        /**
         * The friend number did not designate a valid friend.
         */
        case TOXAV_ERR_CALL_FRIEND_NOT_FOUND:
            msg = QT_TRANSLATE_NOOP("ToxError", "The friend number did not designate a valid friend");
            break;

        /**
         * The friend was valid, but not currently connected.
         */
        case TOXAV_ERR_CALL_FRIEND_NOT_CONNECTED:
            msg = QT_TRANSLATE_NOOP("ToxError", "The friend was valid, but not currently connected");
            break;

        /**
         * Attempted to call a friend while already in an audio or video call with
         * them.
         */
        case TOXAV_ERR_CALL_FRIEND_ALREADY_IN_CALL:
            msg = QT_TRANSLATE_NOOP("ToxError", "Attempted to call a friend while already in an audio "
                                                "or video call with them");
            break;

        /**
         * Audio or video bit rate is invalid.
         */
        case TOXAV_ERR_CALL_INVALID_BIT_RATE:
            msg = QT_TRANSLATE_NOOP("ToxError", "Audio or video bit rate is invalid");
            break;

        default:
            msg = unknown_error;
    }

    err.description = QObject::tr(msg);
    return (code != TOXAV_ERR_CALL_OK);
}

bool toxError(TOXAV_ERR_ANSWER code, data::MessageError& err)
{
    const char* msg;
    err = error::toxav_err_answer;

    switch (code)
    {
        /**
         * The function returned successfully.
         */
        case TOXAV_ERR_ANSWER_OK:
            msg = return_success;
            break;

        /**
         * Synchronization error occurred.
         */
        case TOXAV_ERR_ANSWER_SYNC:
            msg = QT_TRANSLATE_NOOP("ToxError", "Synchronization error occurred");
            break;

        /**
         * Failed to initialize codecs for call session. Note that codec initiation
         * will fail if there is no receive callback registered for either audio or
         * video.
         */
        case TOXAV_ERR_ANSWER_CODEC_INITIALIZATION:
            msg = QT_TRANSLATE_NOOP("ToxError", "Failed to initialize codecs for call session");
            break;

        /**
         * The friend number did not designate a valid friend.
         */
        case TOXAV_ERR_ANSWER_FRIEND_NOT_FOUND:
            msg = QT_TRANSLATE_NOOP("ToxError", "The friend number did not designate a valid friend");
            break;

        /**
         * The friend was valid, but they are not currently trying to initiate a call.
         * This is also returned if this client is already in a call with the friend.
         */
        case TOXAV_ERR_ANSWER_FRIEND_NOT_CALLING:
            msg = QT_TRANSLATE_NOOP("ToxError", "The friend was valid, but they are not currently "
                                                "trying to initiate a call");
            break;

        /**
         * Audio or video bit rate is invalid.
         */
        case TOXAV_ERR_ANSWER_INVALID_BIT_RATE:
            msg = QT_TRANSLATE_NOOP("ToxError", "Audio or video bit rate is invalid");
            break;

        default:
            msg = unknown_error;
    }

    err.description = QObject::tr(msg);
    return (code != TOXAV_ERR_ANSWER_OK);
}

bool toxError(TOXAV_ERR_CALL_CONTROL code, data::MessageError& err)
{
    const char* msg;
    err = error::toxav_err_call_control;

    switch (code)
    {
        /**
         * The function returned successfully.
         */
        case TOXAV_ERR_CALL_CONTROL_OK:
            msg = return_success;
            break;

        /**
         * Synchronization error occurred.
         */
        case TOXAV_ERR_CALL_CONTROL_SYNC:
            msg = QT_TRANSLATE_NOOP("ToxError", "Synchronization error occurred");
            break;

        /**
         * The friend_number passed did not designate a valid friend.
         */
        case TOXAV_ERR_CALL_CONTROL_FRIEND_NOT_FOUND:
            msg = QT_TRANSLATE_NOOP("ToxError", "The friend_number passed did not designate "
                                                "a valid friend");
            break;

        /**
         * This client is currently not in a call with the friend. Before the call is
         * answered, only CANCEL is a valid control.
         */
        case TOXAV_ERR_CALL_CONTROL_FRIEND_NOT_IN_CALL:
            msg = QT_TRANSLATE_NOOP("ToxError", "This client is currently not in a call "
                                                "with the friend");
            break;

        /**
         * Happens if user tried to pause an already paused call or if trying to
         * resume a call that is not paused.
         */
        case TOXAV_ERR_CALL_CONTROL_INVALID_TRANSITION:
            msg = QT_TRANSLATE_NOOP("ToxError", "Happens if user tried to pause an already "
                                                "paused call or if trying to resume a call that "
                                                "is not paused");
            break;

        default:
            msg = unknown_error;
    }

    err.description = QObject::tr(msg);
    return (code != TOXAV_ERR_CALL_CONTROL_OK);
}

bool toxError(TOXAV_ERR_SEND_FRAME code, data::MessageError& err)
{
    const char* msg;
    err = error::toxav_err_send_frame;

    switch (code)
    {
        /**
         * The function returned successfully.
         */
        case TOXAV_ERR_SEND_FRAME_OK:
            msg = return_success;
            break;

        /**
         * In case of video, one of Y, U, or V was NULL. In case of audio, the samples
         * data pointer was NULL.
         */
        case TOXAV_ERR_SEND_FRAME_NULL:
            msg = QT_TRANSLATE_NOOP("ToxError", "In case of video, one of Y, U, or V "
                                                "was NULL. In case of audio, the samples "
                                                "data pointer was NULL");
            break;

        /**
         * The friend_number passed did not designate a valid friend.
         */
        case TOXAV_ERR_SEND_FRAME_FRIEND_NOT_FOUND:
            msg = QT_TRANSLATE_NOOP("ToxError", "The friend_number passed did not designate "
                                                "a valid friend.");
            break;

        /**
         * This client is currently not in a call with the friend.
         */
        case TOXAV_ERR_SEND_FRAME_FRIEND_NOT_IN_CALL:
            msg = QT_TRANSLATE_NOOP("ToxError", "This client is currently not in a call with the friend");
            break;

        /**
         * Synchronization error occurred.
         */
        case TOXAV_ERR_SEND_FRAME_SYNC:
            msg = QT_TRANSLATE_NOOP("ToxError", "Synchronization error occurred");
            break;

        /**
         * One of the frame parameters was invalid. E.g. the resolution may be too
         * small or too large, or the audio sampling rate may be unsupported.
         */
        case TOXAV_ERR_SEND_FRAME_INVALID:
            msg = QT_TRANSLATE_NOOP("ToxError", "One of the frame parameters was invalid");
            break;

        /**
         * Either friend turned off audio or video receiving or we turned off sending
         * for the said payload.
         */
        case TOXAV_ERR_SEND_FRAME_PAYLOAD_TYPE_DISABLED:
            msg = QT_TRANSLATE_NOOP("ToxError", "Either friend turned off audio or video receiving "
                                                "or we turned off sending for the said payload");
            break;

        /**
         * Failed to push frame through rtp interface.
         */
        case TOXAV_ERR_SEND_FRAME_RTP_FAILED:
            msg = QT_TRANSLATE_NOOP("ToxError", "Failed to push frame through rtp interface");
            break;

        default:
            msg = unknown_error;
    }

    err.description = QObject::tr(msg);
    return (code != TOXAV_ERR_SEND_FRAME_OK);
}

bool toxError(TOX_ERR_FRIEND_CUSTOM_PACKET code, data::MessageError& err)
{
    const char* msg;
    err = error::tox_err_friend_custom_packet;

    switch (code)
    {
        /**
         * The function returned successfully.
         */
        case TOX_ERR_FRIEND_CUSTOM_PACKET_OK:
            msg = return_success;
            break;

        /**
         * One of the arguments to the function was NULL when it was not expected.
         */
        case TOX_ERR_FRIEND_CUSTOM_PACKET_NULL:
            msg = QT_TRANSLATE_NOOP("ToxError", "One of the arguments to the function "
                                                "was NULL when it was not expected");
            break;

        /**
         * The friend number did not designate a valid friend.
         */
        case TOX_ERR_FRIEND_CUSTOM_PACKET_FRIEND_NOT_FOUND:
            msg = QT_TRANSLATE_NOOP("ToxError", "The friend number did not designate "
                                                "a valid friend");
            break;

        /**
         * This client is currently not connected to the friend.
         */
        case TOX_ERR_FRIEND_CUSTOM_PACKET_FRIEND_NOT_CONNECTED:
            msg = QT_TRANSLATE_NOOP("ToxError", "This client is currently not connected "
                                                "to the friend");
            break;

        /**
         * The first byte of data was not in the specified range for the packet type.
         * This range is 200-254 for lossy, and 160-191 for lossless packets.
         */
        case TOX_ERR_FRIEND_CUSTOM_PACKET_INVALID:
            msg = QT_TRANSLATE_NOOP("ToxError", "The first byte of data was not in "
                                                "the specified range for the packet type");
            break;

        /**
         * Attempted to send an empty packet.
         */
        case TOX_ERR_FRIEND_CUSTOM_PACKET_EMPTY:
            msg = QT_TRANSLATE_NOOP("ToxError", "Attempted to send an empty packet");
            break;

        /**
         * Packet data length exceeded TOX_MAX_CUSTOM_PACKET_SIZE.
         */
        case TOX_ERR_FRIEND_CUSTOM_PACKET_TOO_LONG:
            msg = QT_TRANSLATE_NOOP("ToxError", "Packet data length exceeded "
                                                "TOX_MAX_CUSTOM_PACKET_SIZE");
            break;

        /**
         * Packet queue is full.
         */
        case TOX_ERR_FRIEND_CUSTOM_PACKET_SENDQ:
            msg = QT_TRANSLATE_NOOP("ToxError", "Packet queue is full");
            break;

        default:
            msg = unknown_error;
    }

    err.description = QObject::tr(msg);
    return (code != TOX_ERR_FRIEND_CUSTOM_PACKET_OK);
}

bool toxError(TOX_ERR_FILE_SEND code, data::MessageError& err)
{
    const char* msg;
    err = error::tox_err_file_send;

    switch (code)
    {
        /**
         * The function returned successfully.
         */
        case TOX_ERR_FILE_SEND_OK:
            msg = return_success;
            break;

        /**
         * One of the arguments to the function was NULL when it was not expected.
         */
        case TOX_ERR_FILE_SEND_NULL:
            msg = QT_TRANSLATE_NOOP("ToxError", "One of the arguments to the function "
                                                "was NULL when it was not expected");
            break;

        /**
         * The friend_number passed did not designate a valid friend.
         */
        case TOX_ERR_FILE_SEND_FRIEND_NOT_FOUND:
            msg = QT_TRANSLATE_NOOP("ToxError", "The friend_number passed "
                                                "did not designate a valid friend");
            break;

        /**
         * This client is currently not connected to the friend.
         */
        case TOX_ERR_FILE_SEND_FRIEND_NOT_CONNECTED:
            msg = QT_TRANSLATE_NOOP("ToxError", "This client is currently "
                                                "not connected to the friend");
            break;

        /**
         * Filename length exceeded TOX_MAX_FILENAME_LENGTH bytes.
         */
        case TOX_ERR_FILE_SEND_NAME_TOO_LONG:
            msg = QT_TRANSLATE_NOOP("ToxError", "Filename length exceeded "
                                                "TOX_MAX_FILENAME_LENGTH bytes");
            break;

        /**
         * Too many ongoing transfers. The maximum number of concurrent file transfers
         * is 256 per friend per direction (sending and receiving).
         */
        case TOX_ERR_FILE_SEND_TOO_MANY:
            msg = QT_TRANSLATE_NOOP("ToxError", "Too many ongoing transfers");
            break;

        default:
            msg = unknown_error;
    }

    err.description = QObject::tr(msg);
    return (code != TOX_ERR_FILE_SEND_OK);
}

bool toxError(TOX_ERR_FILE_SEND_CHUNK code, data::MessageError& err)
{
    const char* msg;
    err = error::tox_err_file_send_chunk;

    switch (code)
    {
        /**
         * The function returned successfully.
         */
        case TOX_ERR_FILE_SEND_CHUNK_OK:
            msg = return_success;
            break;

        /**
         * The length parameter was non-zero, but data was NULL.
         */
        case TOX_ERR_FILE_SEND_CHUNK_NULL:
            msg = QT_TRANSLATE_NOOP("ToxError", "The length parameter was non-zero, "
                                                "but data was NULL");
            break;

        /**
         * The friend_number passed did not designate a valid friend.
         */
        case TOX_ERR_FILE_SEND_CHUNK_FRIEND_NOT_FOUND:
            msg = QT_TRANSLATE_NOOP("ToxError", "The friend_number passed "
                                                "did not designate a valid friend");
            break;

        /**
         * This client is currently not connected to the friend.
         */
        case TOX_ERR_FILE_SEND_CHUNK_FRIEND_NOT_CONNECTED:
            msg = QT_TRANSLATE_NOOP("ToxError", "");
            break;

        /**
         * No file transfer with the given file number was found for the given friend.
         */
        case TOX_ERR_FILE_SEND_CHUNK_NOT_FOUND:
            msg = QT_TRANSLATE_NOOP("ToxError", "No file transfer with the given "
                                                "file number was found for the given friend");
            break;

        /**
         * File transfer was found but isn't in a transferring state: (paused, done,
         * broken, etc...) (happens only when not called from the request chunk callback).
         */
        case TOX_ERR_FILE_SEND_CHUNK_NOT_TRANSFERRING:
            msg = QT_TRANSLATE_NOOP("ToxError", "File transfer was found but "
                                                "isn't in a transferring state: "
                                                "(paused, done, broken, etc...)");
            break;

        /**
         * Attempted to send more or less data than requested. The requested data size is
         * adjusted according to maximum transmission unit and the expected end of
         * the file. Trying to send less or more than requested will return this error.
         */
        case TOX_ERR_FILE_SEND_CHUNK_INVALID_LENGTH:
            msg = QT_TRANSLATE_NOOP("ToxError", "Attempted to send more or less "
                                                "data than requested");
            break;

        /**
         * Packet queue is full.
         */
        case TOX_ERR_FILE_SEND_CHUNK_SENDQ:
            msg = QT_TRANSLATE_NOOP("ToxError", "Packet queue is full");
            break;

        /**
         * Position parameter was wrong.
         */
        case TOX_ERR_FILE_SEND_CHUNK_WRONG_POSITION:
            msg = QT_TRANSLATE_NOOP("ToxError", "Position parameter was wrong");
            break;

        default:
            msg = unknown_error;
    }

    err.description = QObject::tr(msg);
    return (code != TOX_ERR_FILE_SEND_CHUNK_OK);
}

bool toxError(TOX_ERR_FRIEND_ADD code, pproto::data::MessageError& err)
{
    const char* msg;
    err = error::tox_err_file_send_chunk;

    switch (code)
    {
        /**
         * The function returned successfully.
         */
        case TOX_ERR_FRIEND_ADD_OK:
            msg = return_success;
            break;

        /**
         * One of the arguments to the function was NULL when it was not expected.
         */
        case TOX_ERR_FRIEND_ADD_NULL:
            msg = QT_TRANSLATE_NOOP("ToxError", "One of the arguments to the function "
                                                "was NULL when it was not expected");
            break;

        /**
         * The length of the friend request message exceeded
         * TOX_MAX_FRIEND_REQUEST_LENGTH.
         */
        case TOX_ERR_FRIEND_ADD_TOO_LONG:
            msg = QT_TRANSLATE_NOOP("ToxError", "The length of the friend request "
                                                "message exceeded TOX_MAX_FRIEND_REQUEST_LENGTH");
            break;

        /**
         * The friend request message was empty. This, and the TOO_LONG code will
         * never be returned from tox_friend_add_norequest.
         */
        case TOX_ERR_FRIEND_ADD_NO_MESSAGE:
            msg = QT_TRANSLATE_NOOP("ToxError", "The friend request message was empty");
            break;

        /**
         * The friend address belongs to the sending client.
         */
        case TOX_ERR_FRIEND_ADD_OWN_KEY:
            msg = QT_TRANSLATE_NOOP("ToxError", "The friend address belongs to the sending client");
            break;

        /**
         * A friend request has already been sent, or the address belongs to a friend
         * that is already on the friend list.
         */
        case TOX_ERR_FRIEND_ADD_ALREADY_SENT:
            msg = QT_TRANSLATE_NOOP("ToxError", "A friend request has already been sent");
            break;

        /**
         * The friend address checksum failed.
         */
        case TOX_ERR_FRIEND_ADD_BAD_CHECKSUM:
            msg = QT_TRANSLATE_NOOP("ToxError", "The friend address checksum failed");
            break;

        /**
         * The friend was already there, but the nospam value was different.
         */
        case TOX_ERR_FRIEND_ADD_SET_NEW_NOSPAM:
            msg = QT_TRANSLATE_NOOP("ToxError", "The friend was already there, "
                                                "but the nospam value was different");
            break;

        /**
         * A memory allocation failed when trying to increase the friend list size.
         */
        case TOX_ERR_FRIEND_ADD_MALLOC:
            msg = QT_TRANSLATE_NOOP("ToxError", "A memory allocation failed when trying "
                                                "to increase the friend list size");
            break;

        default:
            msg = unknown_error;
    }

    err.description = QObject::tr(msg);
    return (code != TOX_ERR_FRIEND_ADD_OK);
}
