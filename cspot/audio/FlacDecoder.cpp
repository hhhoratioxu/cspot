// Copyright (C) 2026 hhhoratioxu. GPL-3.0-or-later.
#include "FlacDecoder.h"
#include <limits>
#include <stdexcept>
#include <utility>

namespace cspot::audio {
FlacDecoder::FlacDecoder(const std::string& path) : path_(path) {
  initialize();
}
void FlacDecoder::initialize() {
  format_ = {};
  block_.clear();
  error_.clear();
  finished_ = false;
  sought_ = false;
  decodedFrames_ = 0;
  decoder_ = FLAC__stream_decoder_new();
  if (!decoder_) throw std::bad_alloc();
  try {
    FLAC__stream_decoder_set_md5_checking(decoder_, true);
    const auto status = FLAC__stream_decoder_init_file(
        decoder_, path_.c_str(), writeCallback, metadataCallback, errorCallback, this);
    if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK)
      throw std::runtime_error(FLAC__StreamDecoderInitStatusString[status]);
    if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder_))
      throw std::runtime_error("invalid FLAC metadata");
    checkError();
    if (format_.sampleRate < 8000 || format_.sampleRate > 192000 ||
        format_.channels < 1 || format_.channels > 2 ||
        format_.bitsPerSample < 4 || format_.bitsPerSample > 32)
      throw std::runtime_error("unsupported FLAC format: require 8-192 kHz, mono/stereo, 4-32 bits");
  } catch (...) {
    FLAC__stream_decoder_delete(decoder_);
    decoder_ = nullptr;
    throw;
  }
}
FlacDecoder::~FlacDecoder() {
  if (decoder_) {
    if (!finished_) FLAC__stream_decoder_finish(decoder_);
    FLAC__stream_decoder_delete(decoder_);
  }
}
void FlacDecoder::checkError() const {
  if (!error_.empty()) throw std::runtime_error(error_);
}
void FlacDecoder::metadataCallback(const FLAC__StreamDecoder*,
                                  const FLAC__StreamMetadata* metadata,
                                  void* opaque) noexcept {
  auto& self = *static_cast<FlacDecoder*>(opaque);
  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    const auto& info = metadata->data.stream_info;
    self.format_ = {info.sample_rate, info.bits_per_sample, info.channels,
                    info.total_samples};
  }
}
FLAC__StreamDecoderWriteStatus FlacDecoder::writeCallback(
    const FLAC__StreamDecoder*, const FLAC__Frame* frame,
    const FLAC__int32* const pcm[], void* opaque) noexcept {
  auto& self = *static_cast<FlacDecoder*>(opaque);
  try {
    if (frame->header.channels != self.format_.channels ||
        frame->header.bits_per_sample != self.format_.bitsPerSample ||
        frame->header.sample_rate != self.format_.sampleRate ||
        frame->header.blocksize > 65535)
      throw std::runtime_error("FLAC frame format changed or exceeded buffer limit");
    self.block_.resize(static_cast<std::size_t>(frame->header.blocksize) *
                       frame->header.channels);
    for (unsigned f = 0; f < frame->header.blocksize; ++f)
      for (unsigned c = 0; c < frame->header.channels; ++c)
        self.block_[f * frame->header.channels + c] = pcm[c][f];
    self.decodedFrames_ += frame->header.blocksize;
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
  } catch (...) {
    // Do not allow a C++ exception to cross a libFLAC C callback.
    try { self.error_ = "FLAC frame decode/buffer failure"; } catch (...) {}
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
}
void FlacDecoder::errorCallback(const FLAC__StreamDecoder*,
                               FLAC__StreamDecoderErrorStatus status,
                               void* opaque) noexcept {
  try {
    static_cast<FlacDecoder*>(opaque)->error_ =
        FLAC__StreamDecoderErrorStatusString[status];
  } catch (...) {}
}
std::vector<std::int32_t> FlacDecoder::nextBlock() {
  checkError();
  if (finished_) return {};
  if (!block_.empty()) return std::exchange(block_, {});
  while (FLAC__stream_decoder_get_state(decoder_) !=
         FLAC__STREAM_DECODER_END_OF_STREAM) {
    if (!FLAC__stream_decoder_process_single(decoder_))
      throw std::runtime_error("FLAC decoding failed");
    checkError();
    if (!block_.empty()) return std::exchange(block_, {});
  }
  const bool checksumValid = FLAC__stream_decoder_finish(decoder_);
  finished_ = true;
  if (!checksumValid) throw std::runtime_error("FLAC MD5 mismatch");
  if (!sought_ && format_.totalFrames && decodedFrames_ != format_.totalFrames)
    throw std::runtime_error("truncated FLAC stream: decoded frame count mismatch");
  return {};
}
void FlacDecoder::seekMs(std::uint64_t ms) {
  if (finished_) {
    FLAC__stream_decoder_delete(decoder_);
    decoder_ = nullptr;
    initialize();  // Every callback keeps this object's address as client data.
  }
  if (ms > std::numeric_limits<std::uint64_t>::max() / format_.sampleRate)
    throw std::out_of_range("FLAC seek overflow");
  const auto frame = ms * format_.sampleRate / 1000;
  if (format_.totalFrames && frame >= format_.totalFrames)
    throw std::out_of_range("FLAC seek beyond end");
  checkError();
  block_.clear();
  if (!FLAC__stream_decoder_seek_absolute(decoder_, frame))
    throw std::runtime_error("FLAC seek failed");
  checkError();
  sought_ = true;  // libFLAC disables whole-file MD5 after seek.
}
}  // namespace cspot::audio
