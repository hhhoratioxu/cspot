# HoriSpotConnect cspot fork / cspot 核心分支

**Preview / Experimental. Genuine Spotify FLAC access is not implemented.**

This GPL-3.0-or-later fork retains upstream Git history, copyright notices and bell
submodule. Audited base: 99e5c95ad6d862a1b7845dda81fd9c8233b45b9f, 2026-10-09.
[Original README](README.upstream.md) ·
[macOS bridge and full evidence](https://github.com/hhhoratioxu/SpotConnect/tree/feature/spotify-lossless)

中文：独立 libFLAC 解码和 PCM 转换已实现并经原始样本比对；它只解码合法提供的
本地文件，不等于已经取得 Spotify FLAC 源。AUTO 明确降级到可用 Vorbis 320，
LOSSLESS 明确失败，VORBIS_320 固定选择 320。原有 96/160/320 调用保持兼容。

识别 FLAC 元数据格式 16/22，AAC_24 不被误当 FLAC。未伪造 Connect 无损能力，
未绕过订阅、DRM、服务器权限，未提取官方客户端凭据。CDNAudioFile 仍是 Vorbis
专用的 167 字节前缀适配器，不能直接用于 FLAC。

Strict selection, actual metadata format reporting, safe Vorbis open/seek/PCM,
bounded CDN reads, final-byte/small-file fixes, key-length checks, redacted signed
URLs, and explicit missing-credential handling are included. The generic callback
still outputs signed 16-bit PCM. Bridge callers can constrain sink rate/channels.

The optional local audio components are independent of the embedded build. The
standalone tests/audio suite builds without bell, Spotify credentials or accounts.
The parent bridge runs the actual cspot metadata/CDN/AES/Vorbis tests too. Current
complete parent suite: **44/44 PASS**, with **44/44 PASS** under project address/
undefined behavior instrumentation.

```sh
cmake -S tests/audio -B build-audio -DCMAKE_BUILD_TYPE=Release
cmake --build build-audio --parallel
ctest --test-dir build-audio --output-on-failure --no-tests=error
```

Install libFLAC and CMake >=3.25. On macOS, hori-audio-probe --help describes local
FLAC decode/PCM conversion. AudioToolbox resampling and bit-depth reduction are
not bit-perfect. Original generated fixtures are CC0-1.0, not commercial recordings.

See [audio verification](docs/HORI-AUDIO-TESTS.md), parent audit and acceptance matrix.
Spotify FLAC playback, account/server access, audible HomePod output, real-account
controls and sustained streaming are unverified. No upstream PR was submitted.
