// HoriSpotConnect additions, Copyright (C) 2026 hhhoratioxu. GPL-3.0-or-later.
#include "QualityPolicy.h"
#include <stdexcept>

namespace cspot {
QualityMode parseQualityMode(std::string_view name) {
  if (name == "AUTO")
    return QualityMode::Auto;
  if (name == "LOSSLESS")
    return QualityMode::Lossless;
  if (name == "VORBIS_320")
    return QualityMode::Vorbis320;
  throw std::invalid_argument("quality must be AUTO, LOSSLESS or VORBIS_320");
}
const char* qualityModeName(QualityMode mode) {
  switch (mode) {
    case QualityMode::Legacy:
      return "LEGACY";
    case QualityMode::Auto:
      return "AUTO";
    case QualityMode::Lossless:
      return "LOSSLESS";
    case QualityMode::Vorbis320:
      return "VORBIS_320";
  }
  return "UNKNOWN";
}
const char* audioFormatName(int format) {
  switch (format) {
    case 0:
      return "OGG_VORBIS_96";
    case 1:
      return "OGG_VORBIS_160";
    case 2:
      return "OGG_VORBIS_320";
    case 3:
      return "MP3_256";
    case 4:
      return "MP3_320";
    case 5:
      return "MP3_160";
    case 6:
      return "MP3_96";
    case 7:
      return "MP3_160_ENC";
    case 8:
      return "AAC_24";
    case 9:
      return "AAC_48";
    case 16:
      return "FLAC_FLAC";
    case 22:
      return "FLAC_FLAC_24BIT";
    default:
      return "UNKNOWN";
  }
}
bool isFlacFormat(int format) {
  return format == 16 || format == 22;
}

AudioSelection selectAudioFile(const std::vector<int>& formats,
                               QualityMode mode, int legacyPreferred) {
  if (mode == QualityMode::Lossless)
    return {-1, -1, false, "SPOTIFY_FLAC_ACCESS_UNAVAILABLE"};
  const int preferred = mode == QualityMode::Legacy ? legacyPreferred : 2;
  // This decoder path only accepts Vorbis. Never interpret AAC_24 as FLAC,
  // or feed an unknown metadata enum to the Vorbis decoder.
  if (preferred < 0 || preferred > 2)
    return {-1, -1, false, "UNSUPPORTED_LEGACY_FORMAT"};
  for (std::size_t i = 0; i < formats.size(); ++i)
    if (formats[i] == preferred)
      return {static_cast<int>(i), preferred, mode == QualityMode::Auto,
              mode == QualityMode::Auto ? "SPOTIFY_FLAC_ACCESS_UNAVAILABLE"
                                        : "REQUESTED_FORMAT"};
  if (mode == QualityMode::Legacy)
    for (std::size_t i = 0; i < formats.size(); ++i)
      if (formats[i] == 0)
        return {static_cast<int>(i), 0, true, "LEGACY_VORBIS_96_FALLBACK"};
  return {-1, -1, false, "VORBIS_320_UNAVAILABLE"};
}
}  // namespace cspot
