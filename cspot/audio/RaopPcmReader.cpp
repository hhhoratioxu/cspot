// Copyright (C) 2026 hhhoratioxu. GPL-3.0-or-later.
#include "RaopPcmReader.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cspot::audio {
static std::int16_t toS16(std::int32_t sample, unsigned bits) {
  std::int64_t value = sample;
  if (bits > 16) {
    const std::int64_t divisor = std::int64_t{1} << (bits - 16);
    // Symmetric rounding, with saturation at the signed output limits.
    value = value >= 0 ? (value + divisor / 2) / divisor
                      : -((-value + divisor / 2) / divisor);
  } else if (bits < 16) {
    value *= std::int64_t{1} << (16 - bits);
  }
  return static_cast<std::int16_t>(std::clamp<std::int64_t>(value, -32768, 32767));
}
RaopPcmReader::RaopPcmReader(FlacDecoder& source) : source_(source) {
  if (!resampling()) return;
  const auto& f = source_.format();
  AudioStreamBasicDescription input{};
  input.mSampleRate = f.sampleRate;
  input.mFormatID = kAudioFormatLinearPCM;
  input.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  input.mBytesPerPacket = input.mBytesPerFrame = f.channels * sizeof(float);
  input.mFramesPerPacket = 1;
  input.mChannelsPerFrame = f.channels;
  input.mBitsPerChannel = 32;
  AudioStreamBasicDescription output{};
  output.mSampleRate = 44100;
  output.mFormatID = kAudioFormatLinearPCM;
  output.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
  output.mBytesPerPacket = output.mBytesPerFrame = 4;
  output.mFramesPerPacket = 1;
  output.mChannelsPerFrame = 2;
  output.mBitsPerChannel = 16;
  if (AudioConverterNew(&input, &output, &converter_) != noErr)
    throw std::runtime_error("AudioConverter creation failed");
  UInt32 quality = kAudioConverterQuality_Max;
  auto status = AudioConverterSetProperty(converter_, kAudioConverterSampleRateConverterQuality,
                                         sizeof(quality), &quality);
  if (status == noErr && f.channels == 1) {
    SInt32 map[2] = {0, 0};
    status = AudioConverterSetProperty(converter_, kAudioConverterChannelMap,
                                       sizeof(map), map);
  }
  if (status != noErr) {
    AudioConverterDispose(converter_);
    converter_ = nullptr;
    throw std::runtime_error("AudioConverter configuration failed");
  }
}
RaopPcmReader::~RaopPcmReader() {
  if (converter_) AudioConverterDispose(converter_);
}
OSStatus RaopPcmReader::inputCallback(AudioConverterRef, UInt32* count,
                                     AudioBufferList* data,
                                     AudioStreamPacketDescription**, void* opaque) noexcept {
  auto& self = *static_cast<RaopPcmReader*>(opaque);
  try {
    const auto& f = self.source_.format();
    if (self.inputOffset_ == self.input_.size()) {
      auto block = self.source_.nextBlock();
      self.input_.resize(block.size());
      self.inputOffset_ = 0;
      const float scale = std::ldexp(1.0f, 1 - static_cast<int>(f.bitsPerSample));
      for (std::size_t i = 0; i < block.size(); ++i) self.input_[i] = block[i] * scale;
    }
    *count = std::min(*count, static_cast<UInt32>(
        (self.input_.size() - self.inputOffset_) / f.channels));
    data->mNumberBuffers = 1;
    data->mBuffers[0].mNumberChannels = f.channels;
    data->mBuffers[0].mDataByteSize = *count * f.channels * sizeof(float);
    data->mBuffers[0].mData = self.input_.empty() ? nullptr : self.input_.data() + self.inputOffset_;
    self.inputOffset_ += *count * f.channels;
    return noErr;
  } catch (const std::exception& e) {
    try { self.error_ = e.what(); } catch (...) {}
  } catch (...) {
    try { self.error_ = "PCM converter input failed"; } catch (...) {}
  }
  *count = 0;
  return -1;
}
std::vector<std::int16_t> RaopPcmReader::read() {
  if (!converter_) {
    const auto block = source_.nextBlock();
    const auto& f = source_.format();
    std::vector<std::int16_t> result(block.size() / f.channels * 2);
    for (std::size_t i = 0; i < block.size() / f.channels; ++i)
      for (unsigned c = 0; c < 2; ++c)
        result[i * 2 + c] = toS16(block[i * f.channels + (f.channels == 1 ? 0 : c)],
                                 f.bitsPerSample);
    return result;
  }
  std::vector<std::int16_t> result(4096 * 2);
  AudioBufferList output{};
  output.mNumberBuffers = 1;
  output.mBuffers[0] = {2, static_cast<UInt32>(result.size() * sizeof(std::int16_t)),
                        result.data()};
  UInt32 count = 4096;
  const auto status = AudioConverterFillComplexBuffer(converter_, inputCallback,
                                                     this, &count, &output, nullptr);
  if (!error_.empty()) throw std::runtime_error(error_);
  if (status != noErr) throw std::runtime_error("AudioConverter conversion failed");
  result.resize(static_cast<std::size_t>(count) * 2);
  return result;
}
void RaopPcmReader::seekMs(std::uint64_t ms) {
  source_.seekMs(ms);
  input_.clear();
  inputOffset_ = 0;
  error_.clear();
  if (converter_ && AudioConverterReset(converter_) != noErr)
    throw std::runtime_error("AudioConverter seek reset failed");
}
}  // namespace cspot::audio
