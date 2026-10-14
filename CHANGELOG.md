# HoriSpotConnect cspot changelog

## 0.1.0-preview.1 — 2026-10-15

- Optional tested local libFLAC 1.5.0 decoder and macOS PCM conversion.
- Strict AUTO / LOSSLESS / VORBIS_320; preserve legacy choices.
- Recognize public FLAC IDs without authorization/capability claims.
- Actual metadata encoding / decoded Vorbis format logs and sink constraints.
- Bound HTTP/AES reads, fix EOF/footer/seek handling, validate audio keys.
- Preserve seek-to-zero and safely reject failed Vorbis opening.
- Redact signed URLs, reject missing credentials, contain token errors.
- Validate checksum/truncation, signed 16/24/32-bit samples, mono, resampling and EOF seek.
- Original CC0 fixtures, actual loopback CDN/AES/Vorbis tests and bilingual docs.

Spotify FLAC retrieval is not implemented. GPL and upstream notices are retained.
