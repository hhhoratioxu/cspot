// HoriSpotConnect additions, Copyright (C) 2026 hhhoratioxu. GPL-3.0-or-later.
#pragma once
#include <cstddef>
#include <string_view>
#include <vector>

namespace cspot {
enum class QualityMode { Legacy, Auto, Lossless, Vorbis320 };
QualityMode parseQualityMode(std::string_view name);
const char* qualityModeName(QualityMode mode);
const char* audioFormatName(int format);
bool isFlacFormat(int format);

// There is no authorized, verified Spotify FLAC provider in this client.
// Local libFLAC support must never enable a Connect lossless capability.
inline constexpr bool spotifyFlacTransportAvailable = false;

struct AudioSelection {
  int index = -1;
  int format = -1;
  bool fallback = false;
  const char* reason = "NO_SUPPORTED_AUDIO_FILE";
};
AudioSelection selectAudioFile(const std::vector<int>& formats,
                               QualityMode mode, int legacyPreferred = 1);
}  // namespace cspot
