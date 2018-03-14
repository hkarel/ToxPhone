#include "tox_call.h"
#include "tox_net.h"
#include "tox_func.h"
#include "tox_error.h"
#include "tox_logger.h"
#include "common/defines.h"
#include "common/functions.h"

#include "shared/break_point.h"
#include "shared/steady_timer.h"
#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"
#include "shared/qt/config/config.h"
#include "shared/qt/communication/commands_pool.h"
#include "shared/qt/communication/functions.h"
#include "shared/qt/communication/transport/tcp.h"
#include <chrono>
#include <string>

#define log_error_m   alog::logger().error_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxCall")
#define log_warn_m    alog::logger().warn_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxCall")
#define log_info_m    alog::logger().info_f   (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxCall")
#define log_verbose_m alog::logger().verbose_f(__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxCall")
#define log_debug_m   alog::logger().debug_f  (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxCall")
#define log_debug2_m  alog::logger().debug2_f (__FILE__, LOGGER_FUNC_NAME, __LINE__, "ToxCall")

static const char* tox_videocall_responce_message =
    QT_TRANSLATE_NOOP("ToxCall", "Hi, it ToxPhone client. The ToxPhone client not support a video calls.");

//--------------------------------- ToxCall ----------------------------------

ToxCall& toxCall()
{
    return ::safe_singleton<ToxCall>();
}

ToxCall::ToxCall() : QThreadEx(0)
{
    chk_connect_d(&tcp::listener(), SIGNAL(message(communication::Message::Ptr)),
                  this, SLOT(message(communication::Message::Ptr)))

    #define FUNC_REGISTRATION(COMMAND) \
        _funcInvoker.registration(command:: COMMAND, &ToxCall::command_##COMMAND, this);

    FUNC_REGISTRATION(IncomingConfigConnection)
    FUNC_REGISTRATION(ToxCallAction)
    _funcInvoker.sort();

    #undef FUNC_REGISTRATION
}

bool ToxCall::init(Tox* tox)
{
    _toxav = toxav_new(tox, 0);
    if (_toxav == 0)
    {
        log_error_m << "Failed create ToxAV instance";
        return false;
    }

    toxav_callback_call               (_toxav, toxav_call_cb, this);
    toxav_callback_call_state         (_toxav, toxav_call_state, this);
#if TOX_VERSION_IS_API_COMPATIBLE(0, 2, 0)
    toxav_callback_audio_bit_rate     (_toxav, toxav_audio_bit_rate, this);
    toxav_callback_video_bit_rate     (_toxav, toxav_video_bit_rate, this);
#else
    toxav_callback_bit_rate_status    (_toxav, toxav_bit_rate_status, this);
#endif
    toxav_callback_audio_receive_frame(_toxav, toxav_audio_receive_frame, this);
    toxav_callback_video_receive_frame(_toxav, toxav_video_receive_frame, this);

    return true;
}

void ToxCall::run()
{
    log_info_m << "Started";

    Message::List messages;
    steady_timer iterationTimer;
    int iterationSleepTime;

    while (true)
    {
        CHECK_THREAD_STOP

        toxav_iterate(_toxav);

        // Параметр iterationSleepTime вычисляется с учетом времени потраченного
        // на выполнение toxav_iterate()
        iterationSleepTime = int(toxav_iteration_interval(_toxav));
        iterationTimer.reset();

        if (_callState.direction != data::ToxCallState::Direction::Undefined)
        {
            // Примечание: по всей видимости, желая сделать работу цикла ToxAV
            // более экономичным, разработчики для холостого режима сделали
            // достаточно большие значения iterationSleepTime (500 мс). При этом
            // перестройка к коротким таймингам в режиме активности происходит
            // только через 3 итерации. Такая стратегия приводит к медленной
            // перестройке opus-декодера в режим соответствующий входящим данным,
            // и накоплению большого объема аудио-данных во входящем буфере.
            // Пока декодер не перестроился в правильный режим - все входящие
            // данные обрабатываются некорректно. Чтобы ускорить перестройку
            // opus-декодера в правильный режим работы мы сами минимизируем
            // параметр iterationSleepTime как только состояние звонка становится
            // отличным от Undefined.

            if (iterationSleepTime > 50)
                iterationSleepTime = 50;
            //log_debug2_m << "iterationSleepTime: " << iterationSleepTime;
        }

        iterateVoiceFrame();

        { //Block for QMutexLocker
            QMutexLocker locker(&_threadLock); (void) locker;
            if (!_messages.empty())
            {
                for (int i = 0; i < _messages.count(); ++i)
                    messages.add(_messages.release(i, lst::NO_COMPRESS_LIST));
                _messages.clear();
            }
        }
        while (!messages.empty())
        {
            Message::Ptr m {messages.release(0), false};
            _funcInvoker.call(m);
            if (iterationTimer.elapsed() > iterationSleepTime)
                break;
        }
        if (!messages.empty())
            continue;

        iterationSleepTime -= iterationTimer.elapsed();
        if (iterationSleepTime > 0)
        {
            QMutexLocker locker(&_threadLock); (void) locker;
            if (!_messages.empty())
                continue;
            _threadCond.wait(&_threadLock, iterationSleepTime);
        }
    } // while (true)

    if (_toxav)
        toxav_kill(_toxav);

    log_info_m << "Stopped";
}

void ToxCall::message(const communication::Message::Ptr& message)
{
    if (message->processed())
        return;

    if (_funcInvoker.containsCommand(message->command()))
    {
        if (!commandsPool().commandIsMultiproc(message->command()))
            message->markAsProcessed();

        message->add_ref();
        QMutexLocker locker(&_threadLock); (void) locker;
        _messages.add(message.get());
        _threadCond.wakeAll();
    }
}

void ToxCall::command_IncomingConfigConnection(const Message::Ptr& /*message*/)
{
    sendCallState();
}

void ToxCall::command_ToxCallAction(const Message::Ptr& message)
{
    data::ToxCallAction toxCallAction;
    readFromMessage(message, toxCallAction);

    // Новый вызов
    if (toxCallAction.action == data::ToxCallAction::Action::Call)
    {
        log_verbose_m << "Begin outgoing call (state: WaitingAnswer). "
                      << ToxFriendLog(toxav_get_tox(_toxav), toxCallAction.friendNumber);

        _sendVoiceFriendNumber = quint32(-1);

        _callState.direction = data::ToxCallState::Direction::Outgoing;
        _callState.callState = data::ToxCallState::CallState::WaitingAnswer;
        _callState.callEnd = data::ToxCallState::CallEnd::Undefined;
        _callState.friendNumber = toxCallAction.friendNumber;

        TOXAV_ERR_CALL err;
        { //Block for ToxGlobalLock
            ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
            toxav_call(_toxav, toxCallAction.friendNumber, 64 /*Kb/sec*/, 0, &err);
        }
        if (err != TOXAV_ERR_CALL_OK)
        {
            log_error_m << "Failed toxav_call: " << toxError(err);

            if (configConnected())
            {
                data::MessageError error;
                error.description = tr(toxError(err));
                error.code = 1;

                Message::Ptr answer = message->cloneForAnswer();
                writeToMessage(error, answer);
                tcp::listener().send(answer);
            }
            { //Block for ToxGlobalLock
                ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
                toxav_call_control(_toxav, toxCallAction.friendNumber, TOXAV_CALL_CONTROL_CANCEL, 0);
            }
            _callState.direction = data::ToxCallState::Direction::Undefined;
            _callState.callState = data::ToxCallState::CallState::Undefined;
            _callState.friendNumber = quint32(-1); // toxCallAction.friendNumber;

            if (err == TOXAV_ERR_CALL_FRIEND_NOT_FOUND
                || err == TOXAV_ERR_CALL_FRIEND_NOT_CONNECTED)
            {
                _callState.callEnd = data::ToxCallState::CallEnd::NotConnected;
            }
            else if (err == TOXAV_ERR_CALL_FRIEND_ALREADY_IN_CALL)
            {
                _callState.callEnd = data::ToxCallState::CallEnd::FriendInCall;
            }
            else
                _callState.callEnd = data::ToxCallState::CallEnd::Error;
        }
        sendCallState();
    }

    // Принять входящий вызов
    else if (toxCallAction.action == data::ToxCallAction::Action::Accept)
    {
        log_verbose_m << "Accept incoming call (state: InProgress). "
                      << ToxFriendLog(toxav_get_tox(_toxav), toxCallAction.friendNumber);

        _skipFirstFrames = 0;
        _callState.direction = data::ToxCallState::Direction::Incoming;
        _callState.callState = data::ToxCallState::CallState::InProgress;
        _callState.callEnd = data::ToxCallState::CallEnd::Undefined;
        _callState.friendNumber = toxCallAction.friendNumber;

        TOXAV_ERR_ANSWER err;
        { //Block for ToxGlobalLock
            ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
            toxav_answer(_toxav, toxCallAction.friendNumber, 64 /*Kb/sec*/, 0, &err);
        }
        if (err == TOXAV_ERR_ANSWER_OK)
        {
            _recordBytes = 0;
            _sendVoiceFriendNumber = toxCallAction.friendNumber;
        }
        else
        {
            log_error_m << "Failed toxav_answer: " << toxError(err);

            if (configConnected())
            {
                data::MessageError error;
                error.description = tr(toxError(err));
                error.code = 1;

                Message::Ptr answer = message->cloneForAnswer();
                writeToMessage(error, answer);
                tcp::listener().send(answer);
            }
            { //Block for ToxGlobalLock
                ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
                toxav_call_control(_toxav, toxCallAction.friendNumber, TOXAV_CALL_CONTROL_CANCEL, 0);
            }
            _callState.direction = data::ToxCallState::Direction::Undefined;
            _callState.callState = data::ToxCallState::CallState::Undefined;
            _callState.friendNumber = quint32(-1); // toxCallAction.friendNumber;

            if (err == TOXAV_ERR_ANSWER_FRIEND_NOT_FOUND
                || err == TOXAV_ERR_ANSWER_FRIEND_NOT_CALLING)
            {
                _callState.callEnd = data::ToxCallState::CallEnd::NotConnected;
            }
            else
                _callState.callEnd = data::ToxCallState::CallEnd::Error;
        }
        sendCallState();
    }

    // Отклонить входящий вызов или завершить активный вызов
    else if (toxCallAction.action == data::ToxCallAction::Action::Reject
             || toxCallAction.action == data::ToxCallAction::Action::End)
    {
        { //Block for alog::Line
            const char* logStr = "End ";
            if (toxCallAction.action == data::ToxCallAction::Action::Reject)
                logStr = "Reject ";

            alog::Line logLine = log_verbose_m << logStr;
            if (_callState.direction == data::ToxCallState::Direction::Incoming)
                logLine << "incoming call. ";
            else
                logLine << "outgoing call. ";

            logLine << ToxFriendLog(toxav_get_tox(_toxav), toxCallAction.friendNumber);
        }

        endCalling();

        _callState.direction = data::ToxCallState::Direction::Undefined;
        _callState.callState = data::ToxCallState::CallState::Undefined;
        _callState.friendNumber = toxCallAction.friendNumber;

        if (toxCallAction.action == data::ToxCallAction::Action::Reject)
            _callState.callEnd = data::ToxCallState::CallEnd::Reject;
        else
            _callState.callEnd = data::ToxCallState::CallEnd::SelfEnd;

        TOXAV_ERR_CALL_CONTROL err;
        { //Block for ToxGlobalLock
            ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
            toxav_call_control(_toxav, toxCallAction.friendNumber, TOXAV_CALL_CONTROL_CANCEL, &err);
        }
        if (err != TOXAV_ERR_CALL_CONTROL_OK)
        {
            log_error_m << "Failed toxav_call_control: " << toxError(err);

            if (configConnected())
            {
                data::MessageError error;
                error.description = tr(toxError(err));
                error.code = 1;

                Message::Ptr answer = message->cloneForAnswer();
                writeToMessage(error, answer);
                tcp::listener().send(answer);
            }
        }
        sendCallState();
    }
}

void ToxCall::iterateVoiceFrame()
{
    if (_sendVoiceFriendNumber == quint32(-1))
        return;

    VoiceFrameInfo::Ptr voiceFrameInfo = getRecordFrameInfo();
    if (voiceFrameInfo.empty())
        return;

    //quint32 audioLength = 5; // ms
    //quint32 dataSize = (voiceFrameInfo->samplingRate / 1000) * audioLength
    //                   * voiceFrameInfo->sampleSize * voiceFrameInfo->channels;
    char data[4000];
    quint32 dataSize = voiceFrameInfo->bufferSize;

    if (filterRingBuff().read(data, dataSize))
    {
        _recordBytes += dataSize;

        int retries = 0;
        TOXAV_ERR_SEND_FRAME err;
        while (retries++ < 5)
        {
            ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
            toxav_audio_send_frame(_toxav, _sendVoiceFriendNumber,
                                   (int16_t*)data,
                                   voiceFrameInfo->sampleCount,
                                   voiceFrameInfo->channels,
                                   voiceFrameInfo->samplingRate,
                                   &err);
            if (err == TOXAV_ERR_SEND_FRAME_SYNC)
            {
                QThread::usleep(500);
                continue;
            }
            break;
        }
        if (err != TOXAV_ERR_SEND_FRAME_OK)
        {
            size_t sampleCount =
                dataSize / voiceFrameInfo->sampleSize / voiceFrameInfo->channels;
            log_error_m << "Failed toxav_audio_send_frame: " << toxError(err)
                        << "; sample count: " << sampleCount
                        << "; data size: " << dataSize;
        }
    }
}

void ToxCall::endCalling()
{
    _sendVoiceFriendNumber = quint32(-1);

    log_debug_m << "Record bytes (read): " << _recordBytes;
    log_debug_m << "Playback bytes (write): " << _playbackBytes;

    _recordBytes = 0;
    _playbackBytes = 0;
}

void ToxCall::sendCallState()
{
    //log_debug_m << "Call sendCallState()";

    Message::Ptr m = createMessage(_callState, Message::Type::Event);
    emit internalMessage(m);

    if (configConnected())
    {
        tcp::listener().send(m);
        // Костыль: отправка повторно сообщения. Если отправлять одно сообщение,
        // то оно иногда не приходит из сокета на стороне клиента. Проблема
        // замечена только для Incoming/WaitingAnswer сообщений.
        if (_callState.direction == data::ToxCallState::Direction::Incoming
            && _callState.callState == data::ToxCallState::CallState::WaitingAnswer)
        {
            tcp::listener().send(m);
        }
    }
}

//----------------------------- Tox callback ---------------------------------

void ToxCall::toxav_call_cb(ToxAV* av, uint32_t friend_number,
                            bool audio_enabled, bool video_enabled, void* user_data)
{
    log_debug2_m << "toxav_call_cb()"
                 << "; friend_number: " << friend_number
                 << "; audio_enabled: " << audio_enabled
                 << "; video_enabled: " << video_enabled;

    ToxCall* tc = static_cast<ToxCall*>(user_data);

    if (video_enabled)
    {
        log_warn_m << "Video calls disabled in ToxPhone, call will be rejected. "
                   << ToxFriendLog(toxav_get_tox(av), friend_number);

        TOXAV_ERR_CALL_CONTROL err;
        { //Block for ToxGlobalLock
            ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
            toxav_call_control(av, friend_number, TOXAV_CALL_CONTROL_CANCEL, &err);
        }
        if (err != TOXAV_ERR_CALL_CONTROL_OK)
            log_error_m << "Failed toxav_call_control: " << toxError(err);

        data::ToxMessage toxMessage;
        toxMessage.friendNumber = friend_number;
        toxMessage.text = QByteArray(tox_videocall_responce_message);

        Message::Ptr m = createMessage(toxMessage);
        toxNet().message(m);
        return;
    }
    log_verbose_m << "Begin incoming call (state: WaitingAnswer). "
                  << ToxFriendLog(toxav_get_tox(av), friend_number);

    tc->_callState.direction = data::ToxCallState::Direction::Incoming;
    tc->_callState.callState = data::ToxCallState::CallState::WaitingAnswer;
    tc->_callState.callEnd = data::ToxCallState::CallEnd::Undefined;
    tc->_callState.friendNumber = friend_number;

    //emit tc->startRingtone();
    tc->sendCallState();
}

void ToxCall::toxav_call_state(ToxAV* av, uint32_t friend_number, uint32_t state,
                               void* user_data)
{
    log_debug2_m << "toxav_call_state()"
                 << "; friend_number: " << friend_number;

    ToxCall* tc = static_cast<ToxCall*>(user_data);

    if ((state & TOXAV_FRIEND_CALL_STATE_ERROR) == TOXAV_FRIEND_CALL_STATE_ERROR)
    {
        log_debug2_m << "ToxAV event: TOXAV_FRIEND_CALL_STATE_ERROR";

        { //Block for alog::Line
            alog::Line logLine = log_verbose_m << "Break ";
            if (tc->_callState.direction == data::ToxCallState::Direction::Incoming)
                logLine << "incoming call. ";
            else
                logLine << "outgoing call. ";

            logLine << ToxFriendLog(toxav_get_tox(av), friend_number);
        }
        tc->endCalling();

        tc->_callState.direction = data::ToxCallState::Direction::Undefined;
        tc->_callState.callState = data::ToxCallState::CallState::Undefined;
        tc->_callState.callEnd = data::ToxCallState::CallEnd::Error;
        tc->_callState.friendNumber = quint32(-1);

        tc->sendCallState();
    }

    if ((state & TOXAV_FRIEND_CALL_STATE_FINISHED) == TOXAV_FRIEND_CALL_STATE_FINISHED)
    {
        log_debug2_m << "ToxAV event: TOXAV_FRIEND_CALL_STATE_FINISHED";

        { //Block for alog::Line
            alog::Line logLine = log_verbose_m << "End/Reject ";
            if (tc->_callState.direction == data::ToxCallState::Direction::Incoming)
                logLine << "incoming call. ";
            else
                logLine << "outgoing call. ";

            logLine << ToxFriendLog(toxav_get_tox(av), friend_number);
        }
        tc->endCalling();

        tc->_callState.direction = data::ToxCallState::Direction::Undefined;
        tc->_callState.callState = data::ToxCallState::CallState::Undefined;
        tc->_callState.callEnd = data::ToxCallState::CallEnd::FriendEnd;
        tc->_callState.friendNumber = quint32(-1);

        tc->sendCallState();
    }

    if ((state & TOXAV_FRIEND_CALL_STATE_SENDING_A) == TOXAV_FRIEND_CALL_STATE_SENDING_A)
    {
        log_debug2_m << "ToxAV event: TOXAV_FRIEND_CALL_STATE_SENDING_A";

        // Используем данное событие как факт того, что друг принял наш вызов
        if (tc->_callState.direction == data::ToxCallState::Direction::Outgoing
            && tc->_callState.callState == data::ToxCallState::CallState::WaitingAnswer)
        {
            tc->_skipFirstFrames = 0;
            tc->_callState.callState = data::ToxCallState::CallState::InProgress;
            tc->_callState.callEnd = data::ToxCallState::CallEnd::Undefined;

            //emit tc->stopRingtone();

            log_verbose_m << "Accept outgoing call (state: InProgress). "
                          << ToxFriendLog(toxav_get_tox(av), friend_number);

            if (tc->_callState.friendNumber != friend_number)
            {
                log_error_m << "Mismatch between the numbers of friends, call will be rejected"
                            << "; Incoming -> " <<  ToxFriendLog(toxav_get_tox(av), friend_number)
                            << "; Expected -> " <<  ToxFriendLog(toxav_get_tox(av), tc->_callState.friendNumber);

                TOXAV_ERR_CALL_CONTROL err;
                { //Block for ToxGlobalLock
                    ToxGlobalLock toxGlobalLock; (void) toxGlobalLock;
                    toxav_call_control(av, friend_number, TOXAV_CALL_CONTROL_CANCEL, &err);
                }
                if (err != TOXAV_ERR_CALL_CONTROL_OK)
                    log_error_m << "Failed toxav_call_control: " << toxError(err);

                tc->_callState.direction = data::ToxCallState::Direction::Undefined;
                tc->_callState.callState = data::ToxCallState::CallState::Undefined;
                tc->_callState.callEnd = data::ToxCallState::CallEnd::Error;
                //_callState.friendNumber = toxCallAction.friendNumber;
            }
            tc->sendCallState();
        }
    }
    if ((state & TOXAV_FRIEND_CALL_STATE_SENDING_V) == TOXAV_FRIEND_CALL_STATE_SENDING_V)
    {
        log_debug2_m << "ToxAV event: TOXAV_FRIEND_CALL_STATE_SENDING_V";
    }
    if ((state & TOXAV_FRIEND_CALL_STATE_ACCEPTING_A) == TOXAV_FRIEND_CALL_STATE_ACCEPTING_A)
    {
        log_debug2_m << "ToxAV event: TOXAV_FRIEND_CALL_STATE_ACCEPTING_A";

        tc->_recordBytes = 0;
        tc->_sendVoiceFriendNumber = friend_number;

        //emit tc->startRecordVoice();
    }
    if ((state & TOXAV_FRIEND_CALL_STATE_ACCEPTING_V) == TOXAV_FRIEND_CALL_STATE_ACCEPTING_V)
    {
        log_debug2_m << "ToxAV event: TOXAV_FRIEND_CALL_STATE_ACCEPTING_V";
    }
}

#if TOX_VERSION_IS_API_COMPATIBLE(0, 2, 0)
void ToxCall::toxav_audio_bit_rate(ToxAV* av, uint32_t friend_number,
                                   uint32_t audio_bit_rate, void* user_data)
{
    log_debug2_m << "toxav_audio_bit_rate()"
                 << "; friend_number: " << friend_number
                 << "; audio_bit_rate: " << audio_bit_rate;
}

void ToxCall::toxav_video_bit_rate(ToxAV* av, uint32_t friend_number,
                                   uint32_t video_bit_rate, void* user_data)
{
    log_debug2_m << "toxav_video_bit_rate()"
                 << "; friend_number: " << friend_number
                 << "; video_bit_rate: " << video_bit_rate;
}
#else
void ToxCall::toxav_bit_rate_status(ToxAV* av, uint32_t friend_number,
                                    uint32_t audio_bit_rate, uint32_t video_bit_rate,
                                    void* user_data)
{
    log_debug2_m << "toxav_bit_rate_status()"
                 << "; friend_number: " << friend_number
                 << "; audio_bit_rate: " << audio_bit_rate
                 << "; video_bit_rate: " << video_bit_rate;
}
#endif

void ToxCall::toxav_audio_receive_frame(ToxAV* av, uint32_t friend_number,
                                        const int16_t* pcm, size_t sample_count,
                                        uint8_t channels, uint32_t sampling_rate,
                                        void* user_data)
{
    ToxCall* tc = static_cast<ToxCall*>(user_data);

    static quint32 sampleSize {sizeof(int16_t)};
    quint32 bufferSize = sample_count * sampleSize * channels;
                         //((latency * sampling_rate) / 1000000) * sampleSize * channels;

    // Пропускаем первые 10 фреймов. Таким образом даем opus-декодеру
    // возможность перестроиться в правильный режим приема данных.
    // На 11-м фрейме инициализируем аудио-систему.
    if (tc->_skipFirstFrames < 10)
    {
        log_debug2_m << "Skip frame: " << tc->_skipFirstFrames
                     << "; friend number: " << friend_number
                     << "; channels: "  << int(channels)
                     << "; sample size: "   << sampleSize
                     << "; sample count: "  << sample_count
                     << "; sampling rate: " << sampling_rate
                     << "; buffer size: "   << bufferSize;

        ++tc->_skipFirstFrames;
        return;
    }

    if (tc->_skipFirstFrames == 10)
    {
        ++tc->_skipFirstFrames;
        tc->_playbackBytes = 0;

        //quint32 latency = 20000;
        quint32 latency = (bufferSize * 1000000) / sampling_rate / sampleSize / channels;
        VoiceFrameInfo voiceFrameInfo {latency, channels, sampleSize,
                                       quint32(sample_count), sampling_rate, bufferSize};

        emit tc->startVoice(VoiceFrameInfo::Ptr::create_ptr(voiceFrameInfo));
        return;
    }

    if (voiceRingBuff().write((char*)pcm, bufferSize))
        tc->_playbackBytes += bufferSize;
}

void ToxCall::toxav_video_receive_frame(ToxAV* av, uint32_t friend_number,
                                        uint16_t width, uint16_t height,
                                        const uint8_t* y, const uint8_t* u, const uint8_t* v,
                                        int32_t ystride, int32_t ustride, int32_t vstride,
                                        void* user_data)
{
    log_debug2_m << "toxav_video_receive_frame()"
                 << "; friend_number: " << friend_number;

}

#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
