#include "wav_file.h"

#include "shared/logger/logger.h"
#include "shared/qt/logger/logger_operators.h"

#define log_error_m   alog::logger().error  (__FILE__, __func__, __LINE__, "WavFile")
#define log_warn_m    alog::logger().warn   (__FILE__, __func__, __LINE__, "WavFile")
#define log_info_m    alog::logger().info   (__FILE__, __func__, __LINE__, "WavFile")
#define log_verbose_m alog::logger().verbose(__FILE__, __func__, __LINE__, "WavFile")
#define log_debug_m   alog::logger().debug  (__FILE__, __func__, __LINE__, "WavFile")
#define log_debug2_m  alog::logger().debug2 (__FILE__, __func__, __LINE__, "WavFile")


bool WavFile::open()
{
    if (!QFile::open(QIODevice::ReadOnly))
    {
        log_error_m << "Failed open the file " << fileName();
        return false;
    }

    QDataStream stream(this);
    stream.setByteOrder(QDataStream::LittleEndian);

    memset(&_header, 0, sizeof _header);
    stream.readRawData(_header.chunkID, 4);
    if (strncmp(_header.chunkID, "RIFF", 4) != 0)
    {
        log_error_m << "Error in reading RIFF. File: " << fileName();
        close();
        return false;
    }
    stream >> _header.chunkSize;

    stream.readRawData(_header.format, 4);
    if (strncmp(_header.format, "WAVE", 4) != 0)
    {
        log_warn_m << "WAVE format not found. File: " << fileName();
        close();
        return false;
    }

    stream.readRawData(_header.headerID, 4);
    if (strncmp(_header.headerID, "fmt", 3) != 0)
    {
        log_warn_m << "fmt format not found. File: " << fileName();
        close();
        return false;
    }

    stream >> _header.headerSize;
    if (_header.headerSize > 16)
    {
        log_error_m << "WAV header greater than 16, it unsupported. File: " << fileName();
        close();
        return false;
    }

    stream >> _header.audioFormat;
    if (_header.audioFormat != 1)
    {
        log_error_m << "Support only PCM format. File: " << fileName();
        close();
        return false;
    }

    stream >> _header.numChannels;
    stream >> _header.sampleRate;
    stream >> _header.byteRate;
    stream >> _header.blockAlign;
    stream >> _header.bitsPerSample;

    char chunkId[4];   //"data" = 0x61746164
    quint32 chunkSize; //Chunk data bytes
    while (true)
    {
        stream.readRawData(chunkId, 4);
        stream >> chunkSize;
        if (stream.status() != QDataStream::Ok)
        {
            log_error_m << "Failed get 'data' chunk. File: " << fileName();
            close();
            return false;
        }
        if (strncmp(chunkId, "data", 4) == 0)
        {
            _dataSize = chunkSize;
            _dataChunkPos = pos();
            break;
        }
        stream.skipRawData(chunkSize);
    }

    //Number of samples
    _sampleSize = _header.bitsPerSample / 8;
    _samplesCount = _dataSize * 8 / _header.bitsPerSample;

    return true;
}


#undef log_error_m
#undef log_warn_m
#undef log_info_m
#undef log_verbose_m
#undef log_debug_m
#undef log_debug2_m
