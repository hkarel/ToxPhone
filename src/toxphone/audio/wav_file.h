
#pragma once
#include <QtCore>

class WavFile : public QFile
{
public:
    // Описание WAV формата:
    // http://soundfile.sapp.org/doc/WaveFormat/
    // http://www.topherlee.com/software/pcm-tut-wavformat.html

    // Пример разбора WAV файла
    // https://gist.github.com/tkaczenko/21ced83c69e30cfbc82b
    struct Header
    {
        char    chunkID[4];  // "RIFF" = 0x46464952
        quint32 chunkSize;
        char    format[4];   // "WAVE" = 0x45564157
        char    headerID[4]; // "fmt " = 0x20746D66
        quint32 headerSize;
        quint16 audioFormat; // PCM = 1
        quint16 numChannels;
        quint32 sampleRate;
        quint32 byteRate;
        quint16 blockAlign;
        quint16 bitsPerSample;
        //[WORD wExtraFormatBytes;]
        //[Extra format bytes]
    };


public:
    WavFile() : QFile() {}
    WavFile(const QString& name) : QFile(name) {}

    bool open();
    const Header& header() const {return _header;}

    // Размер потока данных
    quint32 dataSize() const {return _dataSize;}

    // Размер сэмпла
    quint32 sampleSize() const {return _sampleSize;}

    // Количество сэмплов
    quint32 samplesCount() const {return _samplesCount;}

    // Начало потока данных, используется когда поток нужно проиграть по кругу
    quint32 dataChunkPos() const {return _dataChunkPos;}

private:
    Header  _header;
    quint32 _dataSize;
    quint32 _sampleSize;
    quint32 _samplesCount;
    quint32 _dataChunkPos;
};
