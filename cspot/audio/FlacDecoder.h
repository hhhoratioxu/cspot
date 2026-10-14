// Copyright (C) 2026 hhhoratioxu. GPL-3.0-or-later.
#pragma once
#include <FLAC/stream_decoder.h>
#include <cstdint>
#include <string>
#include <vector>

namespace cspot::audio {
struct PcmFormat {
  unsigned sampleRate = 0;
  unsigned bitsPerSample = 0;
  unsigned channels = 0;
  std::uint64_t totalFrames = 0;
};

// Bounded, streaming decoder for lawfully supplied local FLAC files.
// This class contains no Spotify authorization or DRM implementation.
class FlacDecoder {
 public:
  explicit FlacDecoder(const std::string& path);
  ~FlacDecoder();
  FlacDecoder(const FlacDecoder&) = delete;
  FlacDecoder& operator=(const FlacDecoder&) = delete;
  const PcmFormat& format() const { return format_; }
  std::vector<std::int32_t> nextBlock();
  void seekMs(std::uint64_t milliseconds);
 private:
  FLAC__StreamDecoder* decoder_ = nullptr;
  std::string path_;
  PcmFormat format_;
  std::vector<std::int32_t> block_;
  std::string error_;
  bool finished_ = false;
  bool sought_ = false;
  std::uint64_t decodedFrames_ = 0;
  static FLAC__StreamDecoderWriteStatus writeCallback(
      const FLAC__StreamDecoder*, const FLAC__Frame*,
      const FLAC__int32* const[], void*) noexcept;
  static void metadataCallback(const FLAC__StreamDecoder*,
                               const FLAC__StreamMetadata*, void*) noexcept;
  static void errorCallback(const FLAC__StreamDecoder*,
                            FLAC__StreamDecoderErrorStatus, void*) noexcept;
  void checkError() const;
  void initialize();
};
}  // namespace cspot::audio
