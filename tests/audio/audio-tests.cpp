// Original generated fixtures are CC0-1.0; test code GPL-3.0-or-later.
#include "FlacDecoder.h"
#include "QualityPolicy.h"
#include <FLAC/stream_encoder.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#ifdef __APPLE__
#include "RaopPcmReader.h"
#endif
using cspot::audio::FlacDecoder;
static void require(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
template<class F> static void fails(F action) {
  bool failed = false;
  try { action(); } catch (const std::exception&) { failed = true; }
  require(failed, "expected a checked failure");
}
struct Fixture {
  std::string path;
  unsigned rate, bits, channels;
  std::vector<std::int32_t> pcm;
  Fixture(const std::string& folder, const std::string& name,
          unsigned rate = 44100, unsigned bits = 16, unsigned channels = 2,
          bool tone = false) : rate(rate), bits(bits), channels(channels) {
    std::filesystem::create_directories(folder);
    path = folder + "/" + name + ".flac";
    const auto amplitude = (std::uint64_t{1} << (bits - 1)) - 1;
    pcm.resize(static_cast<std::size_t>(rate) * channels);
    for (unsigned i = 0; i < rate; ++i)
      for (unsigned c = 0; c < channels; ++c)
        pcm[i * channels + c] = tone
          ? static_cast<std::int32_t>(std::sin(2 * 3.141592653589793 *
                (c ? 439 : 997) * i / rate) * amplitude * 0.5)
          : static_cast<std::int32_t>(static_cast<std::int64_t>(
              static_cast<std::uint64_t>(i * 7919 + c * 3571) %
                (amplitude * 2 + 1)) - static_cast<std::int64_t>(amplitude));
    // Edge values test sign extension, precision and output saturation.
    if (!tone && pcm.size() >= 4) {
      pcm[0] = static_cast<std::int32_t>(amplitude);
      pcm[1] = static_cast<std::int32_t>(-static_cast<std::int64_t>(amplitude) - 1);
      pcm[2] = 0; pcm[3] = -1;
    }
    auto* encoder = FLAC__stream_encoder_new();
    require(encoder != nullptr, "encoder allocation");
    FLAC__stream_encoder_set_channels(encoder, channels);
    FLAC__stream_encoder_set_bits_per_sample(encoder, bits);
    FLAC__stream_encoder_set_sample_rate(encoder, rate);
    FLAC__stream_encoder_set_total_samples_estimate(encoder, rate);
    FLAC__stream_encoder_set_verify(encoder, true);
    const auto init = FLAC__stream_encoder_init_file(encoder, path.c_str(), nullptr, nullptr);
    const bool written = init == FLAC__STREAM_ENCODER_INIT_STATUS_OK &&
        FLAC__stream_encoder_process_interleaved(encoder, pcm.data(), rate);
    const bool finished = FLAC__stream_encoder_finish(encoder);
    FLAC__stream_encoder_delete(encoder);
    require(written && finished, "fixture FLAC encoding and encoder verification");
  }
};
static std::vector<std::int32_t> decode(FlacDecoder& decoder) {
  std::vector<std::int32_t> all;
  for (;;) {
    auto block = decoder.nextBlock();
    if (block.empty()) break;
    require(block.size() <= 65535 * 2, "bounded FLAC output buffer");
    all.insert(all.end(), block.begin(), block.end());
  }
  return all;
}
static void quality(const std::string& test) {
  using namespace cspot;
  if (test == "auto") {
    auto s = selectAudioFile({16, 1, 22, 2, 0}, QualityMode::Auto);
    require(s.index == 3 && s.format == 2 && s.fallback, "AUTO honest Vorbis 320 fallback");
  } else if (test == "lossless") {
    auto s = selectAudioFile({16, 22, 2}, QualityMode::Lossless);
    require(s.index == -1 && !spotifyFlacTransportAvailable, "LOSSLESS fail closed");
  } else if (test == "vorbis320") {
    auto s = selectAudioFile({0, 16, 2, 1}, parseQualityMode("VORBIS_320"));
    require(s.index == 2 && s.format == 2 && !s.fallback, "fixed Vorbis 320");
  } else if (test == "strict320") {
    require(selectAudioFile({0, 1, 16}, QualityMode::Vorbis320).index == -1,
            "320 mode must not silently select 96");
    require(selectAudioFile({0, 1, 22}, QualityMode::Auto).index == -1,
            "AUTO fallback requires 320");
  } else if (test == "unknown") {
    fails([] { parseQualityMode("LOSSLES"); });
    require(selectAudioFile({99, -1, 16, 22}, QualityMode::Auto).index == -1,
            "unknown formats must not enter Vorbis decoder");
  } else if (test == "aac24") {
    require(!isFlacFormat(8) && isFlacFormat(16) && isFlacFormat(22),
            "AAC_24 must not be labeled FLAC");
    require(selectAudioFile({8, 9}, QualityMode::Auto).index == -1,
            "AAC is not playable in the Vorbis path");
  } else {
    const int desired = test == "legacy96" ? 0 : test == "legacy160" ? 1 : 2;
    require(selectAudioFile({0, 2, 1}, QualityMode::Legacy, desired).format == desired,
            "preserve upstream 96/160/320 selection");
    require(selectAudioFile({0}, QualityMode::Legacy, desired).format == 0,
            "preserve upstream legacy 96 fallback");
  }
}
#ifdef __APPLE__
static std::vector<std::int16_t> convert(cspot::audio::RaopPcmReader& reader) {
  std::vector<std::int16_t> all;
  for (;;) {
    auto block = reader.read();
    if (block.empty()) break;
    all.insert(all.end(), block.begin(), block.end());
  }
  return all;
}
static double toneAmplitude(const std::vector<std::int16_t>& pcm, unsigned channel,
                            double frequency) {
  double real = 0, imaginary = 0;
  const auto frames = pcm.size() / 2;
  for (std::size_t i = 1000; i + 1000 < frames; ++i) {
    const double angle = 2 * 3.141592653589793 * frequency * i / 44100;
    real += pcm[i * 2 + channel] * std::cos(angle);
    imaginary -= pcm[i * 2 + channel] * std::sin(angle);
  }
  return 2 * std::hypot(real, imaginary) / (frames - 2000);
}
#endif
int main(int argc, char** argv) {
  try {
    require(argc == 3, "usage: audio-tests case fixture-directory");
    const std::string test = argv[1], folder = argv[2];
    if (test == "auto" || test == "lossless" || test == "vorbis320" ||
        test.starts_with("legacy") || test == "strict320" || test == "unknown" || test == "aac24") {
      quality(test);
    } else if (test == "missing") {
      fails([&] { FlacDecoder d(folder + "/does-not-exist.flac"); });
    } else if (test == "malformed") {
      std::filesystem::create_directories(folder);
      const std::string file = folder + "/malformed.flac";
      { std::ofstream out(file); out << "this is not FLAC"; }
      fails([&] { FlacDecoder d(file); decode(d); });
    } else {
      unsigned bits = test == "decode24" || test == "pcm24" ? 24 : test == "decode32" ? 32 : 16;
      unsigned channels = test == "mono" || test == "pcm_mono" ? 1 : 2;
      unsigned rate = test == "resample96" ? 96000 :
          test == "resample48" || test == "seek_resample" ? 48000 : 44100;
      Fixture fixture(folder, test, rate, bits, channels, rate != 44100);
      if (test == "corrupt" || test == "md5") {
        std::fstream file(fixture.path, std::ios::binary | std::ios::in | std::ios::out);
        file.seekg(test == "md5" ? 26 : std::filesystem::file_size(fixture.path) - 5);
        char byte; file.read(&byte, 1); file.seekp(test == "md5" ? 26 :
            std::filesystem::file_size(fixture.path) - 5);
        byte ^= 0x55; file.write(&byte, 1); file.close();
        fails([&] { FlacDecoder d(fixture.path); decode(d); });
      } else if (test == "truncated") {
        std::filesystem::resize_file(fixture.path, std::filesystem::file_size(fixture.path) / 2);
        fails([&] { FlacDecoder d(fixture.path); decode(d); });
      } else if (test == "seek") {
        FlacDecoder d(fixture.path);
        auto first = d.nextBlock(); require(!first.empty(), "decode before seek");
        d.seekMs(500);
        auto part = d.nextBlock();
        require(std::equal(part.begin(), part.end(), fixture.pcm.begin() + 22050 * channels),
                "seek exact PCM at 500 ms");
        d.seekMs(0); part = d.nextBlock();
        require(std::equal(part.begin(), part.end(), fixture.pcm.begin()), "seek back to zero");
        fails([&] { d.seekMs(2000); });
        fails([&] { d.seekMs(std::numeric_limits<std::uint64_t>::max()); });
#ifdef __APPLE__
      } else if (test.starts_with("pcm") || test.starts_with("resample") || test == "seek_resample") {
        FlacDecoder d(fixture.path);
        cspot::audio::RaopPcmReader reader(d);
        if (test == "seek_resample") { reader.read(); reader.seekMs(500); }
        const auto pcm = convert(reader);
        if (rate != 44100) {
          const std::size_t expected = test == "seek_resample" ? 22050 : 44100;
          require(std::abs(static_cast<long>(pcm.size()/2) - static_cast<long>(expected)) < 32,
                  "resampled duration must match source duration");
          require(toneAmplitude(pcm, 0, 997) > 15000 && toneAmplitude(pcm, 1, 439) > 15000,
                  "preserve measured test tone amplitude, pitch and channel mapping");
          require(toneAmplitude(pcm, 0, 439) < 150, "no channel cross-talk");
        } else {
          require(pcm.size() == rate * 2, "output stereo frame count");
          if (bits == 16) {
            for (std::size_t i = 0; i < pcm.size(); ++i)
              require(pcm[i] == fixture.pcm[(i / 2) * channels + (channels == 1 ? 0 : i % 2)],
                      "16-bit exact PCM and mono duplication");
          } else {
            require(pcm[0] == 32767 && pcm[1] == -32768 && pcm[2] == 0 && pcm[3] == 0,
                    "24-bit rounding and saturation");
            for (std::size_t i = 0; i < pcm.size(); ++i)
              require(std::abs(static_cast<double>(pcm[i]) - fixture.pcm[i] / 256.0) <= 1,
                      "24 to 16 conversion error bounded to one LSB");
          }
        }
#endif
      } else {
        FlacDecoder d(fixture.path);
        require(d.format().sampleRate == rate && d.format().bitsPerSample == bits &&
                d.format().channels == channels && d.format().totalFrames == rate,
                "report actual STREAMINFO format");
        require(decode(d) == fixture.pcm, "bit-for-bit libFLAC decode against original PCM");
      }
    }
    std::cout << "PASS " << test << '\n';
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << '\n'; return 1;
  }
}
