// Copyright (C) 2026 hhhoratioxu. GPL-3.0-or-later.
#pragma once
#include "FlacDecoder.h"
#include <AudioToolbox/AudioToolbox.h>

namespace cspot::audio {
// Output contract of the existing SpotConnect RAOP sender: s16, 44.1 kHz,
// stereo. Receiver acceptance is a separate real-device test.
class RaopPcmReader {
 public:
  explicit RaopPcmReader(FlacDecoder& source);
  ~RaopPcmReader();
  RaopPcmReader(const RaopPcmReader&) = delete;
  RaopPcmReader& operator=(const RaopPcmReader&) = delete;
  std::vector<std::int16_t> read();
  void seekMs(std::uint64_t ms);
  bool resampling() const { return source_.format().sampleRate != 44100; }
 private:
  FlacDecoder& source_;
  AudioConverterRef converter_ = nullptr;
  std::vector<float> input_;
  std::size_t inputOffset_ = 0;
  std::string error_;
  static OSStatus inputCallback(AudioConverterRef, UInt32*, AudioBufferList*,
                                AudioStreamPacketDescription**, void*) noexcept;
};
}  // namespace cspot::audio
