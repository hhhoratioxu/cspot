// Copyright (C) 2026 hhhoratioxu. GPL-3.0-or-later.
#include "FlacDecoder.h"
#include "RaopPcmReader.h"
#include <charconv>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
int main(int argc, char** argv) {
  try {
    std::string file, output;
    std::uint64_t seek = 0;
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if ((arg == "--output" || arg == "--seek-ms") && i + 1 < argc) {
        const std::string value = argv[++i];
        if (arg == "--output") output = value;
        else {
          const auto r = std::from_chars(value.data(), value.data() + value.size(), seek);
          if (r.ec != std::errc{} || r.ptr != value.data() + value.size())
            throw std::invalid_argument("invalid --seek-ms");
        }
      } else if (arg == "--help") {
        std::cout << "hori-audio-probe [--seek-ms N] [--output FILE.s16le] FILE.flac\n"
                     "Local FLAC decode and RAOP PCM conversion; no Spotify retrieval or AirPlay transmission.\n";
        return 0;
      } else if (!arg.starts_with("-") && file.empty()) file = arg;
      else throw std::invalid_argument("invalid argument; use --help");
    }
    if (file.empty()) throw std::invalid_argument("missing FLAC file; use --help");
    if (!output.empty() && output == file) throw std::invalid_argument("output must differ from input");
    if (!output.empty() && std::filesystem::exists(output))
      throw std::invalid_argument("output already exists; choose a new file");
    cspot::audio::FlacDecoder decoder(file);
    cspot::audio::RaopPcmReader reader(decoder);
    if (seek) reader.seekMs(seek);
    std::ofstream out;
    if (!output.empty()) {
      out.open(output, std::ios::binary);
      if (!out) throw std::runtime_error("cannot open PCM output");
    }
    std::uint64_t frames = 0;
    for (;;) {
      const auto pcm = reader.read();
      if (pcm.empty()) break;
      frames += pcm.size() / 2;
      if (out.is_open()) {
        out.write(reinterpret_cast<const char*>(pcm.data()), pcm.size() * sizeof(std::int16_t));
        if (!out) throw std::runtime_error("PCM output write failed");
      }
    }
    const auto& f = decoder.format();
    std::cout << "{\"source\":\"local_FLAC\",\"spotify_flac_verified\":false,"
              << "\"source_sample_rate\":" << f.sampleRate
              << ",\"source_bits\":" << f.bitsPerSample
              << ",\"source_channels\":" << f.channels
              << ",\"output\":\"PCM_S16LE\",\"output_sample_rate\":44100,\"output_bits\":16,"
                 "\"output_channels\":2,\"output_frames\":" << frames
              << ",\"resampled\":" << (reader.resampling() ? "true" : "false")
              << ",\"homepod_verified\":false}\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "AUDIO_PROBE_ERROR " << e.what() << '\n'; return 1;
  }
}
