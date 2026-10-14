# HoriSpotConnect audio verification / 音频验证

Preview, 2026-10-15 (Asia/Shanghai). Spotify FLAC access is **UNAVAILABLE in this client**.
Local FLAC decoder tests do not prove Spotify lossless access or HomePod playback.

## Executed locally / 已执行的本地测试

Apple Silicon arm64, AppleClang 21, libFLAC 1.5.0, macOS AudioToolbox.
Release: **25/25 PASS**. Debug with address/undefined behavior sanitizers: **25/25 PASS**.

Tests generate original, freely distributable PCM and encode it with libFLAC's
verified encoder. The original PCM is the comparison oracle, not another decode.
No commercial recording, account token, audio key or Spotify CDN file is a fixture.
Generated audio fixtures are dedicated to the public domain under CC0-1.0.

- AUTO explicitly falls back to an available Vorbis 320 file; LOSSLESS fails closed.
- VORBIS_320 never silently selects 96/160; the existing legacy 96/160/320 path remains.
- AAC_24 and unknown/missing formats are not treated as FLAC.
- 16/24/32-bit FLAC decoding matches original signed PCM sample for sample.
- Mono, metadata, missing/malformed/corrupt/truncated files and MD5 errors.
- Exact FLAC seek, seek to zero, out-of-range/overflow rejection.
- Exact 16-bit 44.1kHz PCM output; mono duplication; bounded 24-to-16 rounding/clipping.
- 48/96kHz to 44.1kHz AudioToolbox conversion: measured pitch, amplitude, duration,
  channel separation, plus converter reset on seek.

These are executable tests in `tests/audio`, with assertions active in Release.

## Not yet accepted / 尚未验收

**UNVERIFIED**: Spotify server-provided FLAC bytes and authorized source selection;
Premium account eligibility; Spotify FLAC decoding; HomePod delivery and controls;
full-session stability; end-to-end bit-perfect output. No Spotify lossless capability
is advertised. The public FLAC metadata enum IDs are recognition only.

The standalone local decoder deliberately does not implement Spotify DRM or file
retrieval. CDNAudioFile remains the existing Spotify Vorbis transport, whose 167-byte
prefix must not be applied to raw local FLAC.

See [librespot maintainer restriction](https://github.com/librespot-org/librespot/issues/1583)
and [SpotConnect maintainer's lossless response](https://github.com/philippe44/SpotConnect/issues/64#issuecomment-3798257485).
The exact restriction permalink is recorded in the parent fork's upstream audit.

## Reproduce / 复现

Install CMake >=3.25, a C++20 compiler, and libFLAC. On macOS:

```sh
cmake -S tests/audio -B build-audio -DCMAKE_BUILD_TYPE=Release
cmake --build build-audio --parallel
ctest --test-dir build-audio --output-on-failure --no-tests=error
cmake -S tests/audio -B build-audio-asan -DHORI_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-audio-asan --parallel
ctest --test-dir build-audio-asan --output-on-failure --no-tests=error
build-audio/hori-audio-probe --output test.s16le build-audio/fixtures/decode24.flac
```

日志明确标为 `local_FLAC`，不会将本地合成音频标为 Spotify 音源。
输出适配当前桥接发送端的 44.1kHz / 16-bit / 2-channel PCM；24-bit 降位深和
重采样会改变样本，因此这些路径不称为 bit-perfect。HomePod 是否接受及其内部
处理仍须真实设备验收。
