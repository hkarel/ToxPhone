#include "tox_error.h"
#include <QtGlobal>

static const char* unknown_error  = QT_TRANSLATE_NOOP("ToxError", "Unknown error");
static const char* return_success = QT_TRANSLATE_NOOP("ToxError", "The function returned successfully");

const char* toxError(TOX_ERR_NEW code)
{
    switch (code)
    {
        /**
         * The function returned successfully.
         */
        case TOX_ERR_NEW_OK:
            return return_success;

        /**
         * One of the arguments to the function was NULL when it was not expected.
         */
        case TOX_ERR_NEW_NULL:
            return QT_TRANSLATE_NOOP("ToxError", "One of the arguments to the function was NULL "
                                                 "when it was not expected");

        /**
         * The function was unable to allocate enough memory to store the internal
         * structures for the Tox object.
         */
        case TOX_ERR_NEW_MALLOC:
            return QT_TRANSLATE_NOOP("ToxError", "The function was unable to allocate enough memory");

        /**
         * The function was unable to bind to a port. This may mean that all ports
         * have already been bound, e.g. by other Tox instances, or it may mean
         * a permission error. You may be able to gather more information from errno.
         */
        case TOX_ERR_NEW_PORT_ALLOC:
            return QT_TRANSLATE_NOOP("ToxError", "The function was unable to bind to a port");

        /**
         * proxy_type was invalid.
         */
        case TOX_ERR_NEW_PROXY_BAD_TYPE:
            return QT_TRANSLATE_NOOP("ToxError", "proxy_type was invalid");

        /**
         * proxy_type was valid but the proxy_host passed had an invalid format
         * or was NULL.
         */
        case TOX_ERR_NEW_PROXY_BAD_HOST:
            return QT_TRANSLATE_NOOP("ToxError", "proxy_type was valid but the proxy_host passed had "
                                                 "an invalid format or was NULL");
        /**
         * proxy_type was valid, but the proxy_port was invalid.
         */
        case TOX_ERR_NEW_PROXY_BAD_PORT:
            return QT_TRANSLATE_NOOP("ToxError",
                     "proxy_type was valid, but the proxy_port was invalid");

        /**
         * The proxy address passed could not be resolved.
         */
        case TOX_ERR_NEW_PROXY_NOT_FOUND:
            return QT_TRANSLATE_NOOP("ToxError", "The proxy address passed could not be resolved");

        /**
         * The byte array to be loaded contained an encrypted save.
         */
        case TOX_ERR_NEW_LOAD_ENCRYPTED:
            return QT_TRANSLATE_NOOP("ToxError", "The byte array to be loaded contained an encrypted save");

        /**
         * The data format was invalid. This can happen when loading data that was
         * saved by an older version of Tox, or when the data has been corrupted.
         * When loading from badly formatted data, some data may have been loaded,
         * and the rest is discarded. Passing an invalid length parameter also
         * causes this error.
         */
        case TOX_ERR_NEW_LOAD_BAD_FORMAT:
            return QT_TRANSLATE_NOOP("ToxError", "The data format was invalid");

        default:
            return unknown_error;
    }
}

const char* toxError(TOXAV_ERR_CALL code)
{
    switch (code)
    {
        /**
         * The function returned successfully.
         */
        case TOXAV_ERR_CALL_OK:
            return return_success;

        /**
         * A resource allocation error occurred while trying to create the structures
         * required for the call.
         */
        case TOXAV_ERR_CALL_MALLOC:
            return QT_TRANSLATE_NOOP("ToxError", "The function was unable to allocate enough memory");

        /**
         * Synchronization error occurred.
         */
        case TOXAV_ERR_CALL_SYNC:
            return QT_TRANSLATE_NOOP("ToxError", "Synchronization error occurred");

        /**
         * The friend number did not designate a valid friend.
         */
        case TOXAV_ERR_CALL_FRIEND_NOT_FOUND:
            return QT_TRANSLATE_NOOP("ToxError", "The friend number did not designate a valid friend");

        /**
         * The friend was valid, but not currently connected.
         */
        case TOXAV_ERR_CALL_FRIEND_NOT_CONNECTED:
            return QT_TRANSLATE_NOOP("ToxError", "The friend was valid, but not currently connected");

        /**
         * Attempted to call a friend while already in an audio or video call with
         * them.
         */
        case TOXAV_ERR_CALL_FRIEND_ALREADY_IN_CALL:
            return QT_TRANSLATE_NOOP("ToxError", "Attempted to call a friend while already in an audio "
                                                 "or video call with them");

        /**
         * Audio or video bit rate is invalid.
         */
        case TOXAV_ERR_CALL_INVALID_BIT_RATE:
            return QT_TRANSLATE_NOOP("ToxError", "Audio or video bit rate is invalid");

        default:
            return unknown_error;
    }
}

const char* toxError(TOXAV_ERR_ANSWER code)
{
    switch (code)
    {
        /**
         * The function returned successfully.
         */
        case TOXAV_ERR_ANSWER_OK:
            return return_success;

        /**
         * Synchronization error occurred.
         */
        case TOXAV_ERR_ANSWER_SYNC:
            return QT_TRANSLATE_NOOP("ToxError", "Synchronization error occurred");

        /**
         * Failed to initialize codecs for call session. Note that codec initiation
         * will fail if there is no receive callback registered for either audio or
         * video.
         */
        case TOXAV_ERR_ANSWER_CODEC_INITIALIZATION:
            return QT_TRANSLATE_NOOP("ToxError", "Failed to initialize codecs for call session");

        /**
         * The friend number did not designate a valid friend.
         */
        case TOXAV_ERR_ANSWER_FRIEND_NOT_FOUND:
            return QT_TRANSLATE_NOOP("ToxError", "The friend number did not designate a valid friend");

        /**
         * The friend was valid, but they are not currently trying to initiate a call.
         * This is also returned if this client is already in a call with the friend.
         */
        case TOXAV_ERR_ANSWER_FRIEND_NOT_CALLING:
            return QT_TRANSLATE_NOOP("ToxError", "The friend was valid, but they are not currently "
                                                 "trying to initiate a call");

        /**
         * Audio or video bit rate is invalid.
         */
        case TOXAV_ERR_ANSWER_INVALID_BIT_RATE:
            return QT_TRANSLATE_NOOP("ToxError", "Audio or video bit rate is invalid");

        default:
            return unknown_error;
    }
}

const char* toxError(TOXAV_ERR_CALL_CONTROL code)
{
    switch (code)
    {
        /**
         * The function returned successfully.
         */
        case TOXAV_ERR_CALL_CONTROL_OK:
            return return_success;

        /**
         * Synchronization error occurred.
         */
        case TOXAV_ERR_CALL_CONTROL_SYNC:
            return QT_TRANSLATE_NOOP("ToxError", "Synchronization error occurred");

        /**
         * The friend_number passed did not designate a valid friend.
         */
        case TOXAV_ERR_CALL_CONTROL_FRIEND_NOT_FOUND:
            return QT_TRANSLATE_NOOP("ToxError", "The friend_number passed did not designate "
                                                 "a valid friend");

        /**
         * This client is currently not in a call with the friend. Before the call is
         * answered, only CANCEL is a valid control.
         */
        case TOXAV_ERR_CALL_CONTROL_FRIEND_NOT_IN_CALL:
            return QT_TRANSLATE_NOOP("ToxError", "This client is currently not in a call "
                                                 "with the friend");

        /**
         * Happens if user tried to pause an already paused call or if trying to
         * resume a call that is not paused.
         */
        case TOXAV_ERR_CALL_CONTROL_INVALID_TRANSITION:
            return QT_TRANSLATE_NOOP("ToxError", "Happens if user tried to pause an already "
                                                 "paused call or if trying to resume a call that "
                                                 "is not paused");
        default:
            return unknown_error;
    }
}

const char* toxError(TOXAV_ERR_SEND_FRAME code)
{
    switch (code)
    {
        /**
         * The function returned successfully.
         */
        case TOXAV_ERR_SEND_FRAME_OK:
            return return_success;

        /**
         * In case of video, one of Y, U, or V was NULL. In case of audio, the samples
         * data pointer was NULL.
         */
        case TOXAV_ERR_SEND_FRAME_NULL:
            return QT_TRANSLATE_NOOP("ToxError", "In case of video, one of Y, U, or V "
                                                 "was NULL. In case of audio, the samples "
                                                 "data pointer was NULL");
        /**
         * The friend_number passed did not designate a valid friend.
         */
        case TOXAV_ERR_SEND_FRAME_FRIEND_NOT_FOUND:
            return QT_TRANSLATE_NOOP("ToxError", "The friend_number passed did not designate "
                                                 "a valid friend.");
        /**
         * This client is currently not in a call with the friend.
         */
        case TOXAV_ERR_SEND_FRAME_FRIEND_NOT_IN_CALL:
            return QT_TRANSLATE_NOOP("ToxError", "This client is currently not in a call with the friend");

        /**
         * Synchronization error occurred.
         */
        case TOXAV_ERR_SEND_FRAME_SYNC:
            return QT_TRANSLATE_NOOP("ToxError", "Synchronization error occurred");

        /**
         * One of the frame parameters was invalid. E.g. the resolution may be too
         * small or too large, or the audio sampling rate may be unsupported.
         */
        case TOXAV_ERR_SEND_FRAME_INVALID:
            return QT_TRANSLATE_NOOP("ToxError", "One of the frame parameters was invalid");

        /**
         * Either friend turned off audio or video receiving or we turned off sending
         * for the said payload.
         */
        case TOXAV_ERR_SEND_FRAME_PAYLOAD_TYPE_DISABLED:
            return QT_TRANSLATE_NOOP("ToxError", "Either friend turned off audio or video receiving "
                                                 "or we turned off sending for the said payload.");
        /**
         * Failed to push frame through rtp interface.
         */
        case TOXAV_ERR_SEND_FRAME_RTP_FAILED:
            return QT_TRANSLATE_NOOP("ToxError", "Failed to push frame through rtp interface");

        default:
            return unknown_error;
    }
}
