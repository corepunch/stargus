/*
 * Wav.cpp
 *
 *      Author: Andreas Volz
 */

// project
#include "Wav.h"
#include "Hurricane.h"
#include "platform.h"
#include "Logger.h"
#include "FileUtil.h"

// system
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
#include <iostream>

using namespace std;

static Logger logger = Logger("startool.Wav");

Wav::Wav(std::shared_ptr<Hurricane> hurricane) :
  Converter(hurricane)
{
}

Wav::Wav(std::shared_ptr<Hurricane> hurricane, const std::string &arcfile) :
  Converter(hurricane)
{

}

Wav::~Wav()
{

}

namespace
{

uint16_t read16le(const unsigned char *p)
{
  return (uint16_t)(p[0] | (p[1] << 8));
}

uint32_t read32le(const unsigned char *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

struct WavFormat
{
  uint16_t audioFormat = 0;   // 1 = PCM, 0x11 = IMA ADPCM
  uint16_t channels = 0;
  uint32_t sampleRate = 0;
  uint16_t blockAlign = 0;
  uint16_t bitsPerSample = 0;
  uint16_t cbSize = 0;
  uint16_t samplesPerBlock = 0;
};

bool parseWav(const unsigned char *data, size_t size, WavFormat &fmt,
              const unsigned char **pcmData, size_t *pcmSize)
{
  if (size < 12 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0)
  {
    return false;
  }

  bool hasFmt = false;
  size_t pos = 12;

  while (pos + 8 <= size)
  {
    char id[5] = { 0 };
    memcpy(id, data + pos, 4);
    uint32_t chunkSize = read32le(data + pos + 4);
    pos += 8;

    if (pos + chunkSize > size)
    {
      return false;
    }

    if (memcmp(id, "fmt ", 4) == 0)
    {
      if (chunkSize < 16)
      {
        return false;
      }
      fmt.audioFormat = read16le(data + pos);
      fmt.channels = read16le(data + pos + 2);
      fmt.sampleRate = read32le(data + pos + 4);
      fmt.blockAlign = read16le(data + pos + 12);
      fmt.bitsPerSample = read16le(data + pos + 14);
      if (chunkSize >= 18)
      {
        fmt.cbSize = read16le(data + pos + 16);
      }
      if (chunkSize >= 20 && fmt.cbSize >= 2)
      {
        fmt.samplesPerBlock = read16le(data + pos + 18);
      }
      hasFmt = true;
    }
    else if (memcmp(id, "data", 4) == 0)
    {
      *pcmData = data + pos;
      *pcmSize = chunkSize;
      return hasFmt;
    }

    pos += chunkSize + (chunkSize & 1); // chunks are word-aligned
  }

  return false;
}

int clampIndex(int idx)
{
  if (idx < 0) return 0;
  if (idx > 88) return 88;
  return idx;
}

int16_t clampSample(int v)
{
  if (v < -32768) return -32768;
  if (v > 32767) return 32767;
  return (int16_t)v;
}

// IMA ADPCM decode (Microsoft WAV audioFormat 0x11) to PCM16
bool decodeImaAdpcm(const unsigned char *data, size_t size, const WavFormat &fmt,
                    std::vector<int16_t> &pcm)
{
  static const int stepTable[89] =
  {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060,
    1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026,
    4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
  };
  static const int indexTable[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };

  const int channels = fmt.channels;
  const int blockAlign = fmt.blockAlign;
  if (channels <= 0 || blockAlign <= 4 * channels)
  {
    return false;
  }

  std::vector<int> predicted(channels);
  std::vector<int> stepIndex(channels);

  size_t pos = 0;
  while (pos + (size_t)blockAlign <= size)
  {
    const unsigned char *block = data + pos;

    // block header: per channel int16 predictor, uint8 index, uint8 reserved
    for (int c = 0; c < channels; c++)
    {
      predicted[c] = (int16_t)read16le(block + c * 4);
      stepIndex[c] = clampIndex(block[c * 4 + 2]);
      pcm.push_back((int16_t)predicted[c]);
    }

    const unsigned char *nibbles = block + 4 * channels;
    const int nibbleBytes = blockAlign - 4 * channels;
    for (int i = 0; i < nibbleBytes; i++)
    {
      const int c = i % channels;
      const unsigned char b = nibbles[i];
      const int n[2] = { b & 0x0F, (b >> 4) & 0x0F };

      for (int k = 0; k < 2; k++)
      {
        int step = stepTable[stepIndex[c]];
        int diff = step >> 3;
        if (n[k] & 1) diff += step >> 2;
        if (n[k] & 2) diff += step >> 1;
        if (n[k] & 4) diff += step;
        if (n[k] & 8) predicted[c] -= diff;
        else predicted[c] += diff;
        predicted[c] = clampSample(predicted[c]);
        stepIndex[c] = clampIndex(stepIndex[c] + indexTable[n[k]]);
        pcm.push_back((int16_t)predicted[c]);
      }
    }

    pos += blockAlign;
  }

  return !pcm.empty();
}

bool writePcmWav(const std::string &filename, const std::vector<int16_t> &pcm,
                 int channels, uint32_t sampleRate)
{
  FILE *f = fopen(filename.c_str(), "wb");
  if (!f)
  {
    return false;
  }

  const uint16_t bitsPerSample = 16;
  const uint16_t blockAlign = channels * (bitsPerSample / 8);
  const uint32_t byteRate = sampleRate * blockAlign;
  const uint32_t dataSize = (uint32_t)(pcm.size() * sizeof(int16_t));
  const uint32_t riffSize = 36 + dataSize;

  auto w16 = [&](uint16_t v) { fputc(v & 0xff, f); fputc((v >> 8) & 0xff, f); };
  auto w32 = [&](uint32_t v) { fputc(v & 0xff, f); fputc((v >> 8) & 0xff, f); fputc((v >> 16) & 0xff, f); fputc((v >> 24) & 0xff, f); };

  fwrite("RIFF", 1, 4, f);
  w32(riffSize);
  fwrite("WAVE", 1, 4, f);

  fwrite("fmt ", 1, 4, f);
  w32(16);
  w16(1);            // PCM
  w16(channels);
  w32(sampleRate);
  w32(byteRate);
  w16(blockAlign);
  w16(bitsPerSample);

  fwrite("data", 1, 4, f);
  w32(dataSize);
  fwrite(pcm.data(), sizeof(int16_t), pcm.size(), f);

  fclose(f);
  return true;
}

} // anonymous namespace

bool Wav::convert(const std::string &arcfile, Storage storage)
{
  std::shared_ptr<DataChunk> data = mHurricane->extractDataChunk(arcfile);
  if (!data)
  {
    return false;
  }

  WavFormat fmt;
  const unsigned char *pcmData = nullptr;
  size_t pcmSize = 0;
  if (!parseWav(data->getDataPointer(), data->getSize(), fmt, &pcmData, &pcmSize))
  {
    return false;
  }

  string wav_file = storage.getFullPath() + ".wav";
  CheckPath(wav_file);

  if (fmt.audioFormat == 1)
  {
    // PCM: keep the original bytes untouched
    return data->write(wav_file);
  }
  else if (fmt.audioFormat == 0x11)
  {
    // IMA ADPCM: decode to PCM16 so SDL_mixer can play it
    std::vector<int16_t> pcm;
    if (!decodeImaAdpcm(pcmData, pcmSize, fmt, pcm))
    {
      return false;
    }
    return writePcmWav(wav_file, pcm, fmt.channels, fmt.sampleRate);
  }

  return false;
}
