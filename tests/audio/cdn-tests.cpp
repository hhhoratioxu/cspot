// Copyright (C) 2026 hhhoratioxu. GPL-3.0-or-later.
// Loopback-only HTTP/AES fixtures, never Spotify files or account credentials.
#include "CDNAudioFile.h"
#include "CSpotContext.h"
#include "TrackQueue.h"
#include "BellLogger.h"
#include <vorbis/vorbisenc.h>
#include <vorbis/vorbisfile.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> static void fails(F f) {
  bool failed = false; try { f(); } catch (const std::exception&) { failed = true; }
  require(failed, "expected checked rejection");
}
static const std::vector<uint8_t> testKey(16, 0x31);
static std::vector<uint8_t> testIv = {0x72, 0xe0, 0x67, 0xfb, 0xdd, 0xcb, 0xcf, 0x77,
                                     0xeb, 0xe8, 0xbc, 0x64, 0x3f, 0x63, 0x0d, 0x93};
class CdnServer {
 public:
  std::string url;
  CdnServer(const std::vector<uint8_t>& payload, const std::string& mode) : mode_(mode) {
    wire_.assign(167, 0);
    wire_.insert(wire_.end(), payload.begin(), payload.end());
    if (mode == "tiny") wire_.resize(100);
    auto iv = testIv;
    Crypto crypto;
    crypto.aesCTRXcrypt(testKey, iv, wire_.data(), wire_.size());
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    require(socket_ >= 0, "loopback socket");
    sockaddr_in address{}; address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    require(bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
            "loopback bind");
    socklen_t size = sizeof(address); getsockname(socket_, reinterpret_cast<sockaddr*>(&address), &size);
    require(listen(socket_, 8) == 0, "loopback listen");
    url = "http://127.0.0.1:" + std::to_string(ntohs(address.sin_port)) + "/synthetic-audio";
    thread_ = std::thread([this] { run(); });
  }
  ~CdnServer() { stop_ = true; if (thread_.joinable()) thread_.join(); close(socket_); }
 private:
  int socket_ = -1;
  std::vector<uint8_t> wire_;
  std::string mode_;
  std::atomic<bool> stop_ = false;
  std::thread thread_;
  static bool sendAll(int client, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const char*>(data);
    while (size) {
      const auto n = send(client, bytes, size, 0);
      if (n <= 0) return false;
      bytes += n; size -= n;
    }
    return true;
  }
  void run() {
    while (!stop_) {
      pollfd descriptor{socket_, POLLIN, 0};
      if (poll(&descriptor, 1, 50) <= 0) continue;
      const int client = accept(socket_, nullptr, nullptr);
      if (client < 0) continue;
      timeval timeout{2, 0}; setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
      int noPipe = 1; setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &noPipe, sizeof(noPipe));
      std::string request; char buffer[1024];
      while (request.find("\r\n\r\n") == std::string::npos && request.size() < 8192) {
        const auto n = recv(client, buffer, sizeof(buffer), 0);
        if (n <= 0) break;
        request.append(buffer, n);
      }
      const auto pos = request.find("Range: bytes=");
      std::size_t first = 0, last = wire_.size() - 1;
      bool suffix = false;
      if (pos != std::string::npos) {
        const std::string range = request.substr(pos + 13);
        if (range.starts_with("-")) {
          suffix = true;
          const auto length = std::stoul(range.substr(1));
          first = length < wire_.size() ? wire_.size() - length : 0;
        } else {
          unsigned a, b;
          if (sscanf(range.c_str(), "%u-%u", &a, &b) == 2) { first = a; last = b; }
        }
      }
      last = std::min(last, wire_.size() - 1);
      if (first > last) { close(client); continue; }
      std::size_t count = last - first + 1, declared = count;
      if (mode_ == "oversized" && first && !suffix) declared = 70000;
      const std::string headers = "HTTP/1.1 206 Partial Content\r\nContent-Length: " +
          std::to_string(declared) + "\r\nContent-Range: bytes " + std::to_string(first) +
          "-" + std::to_string(last) + "/" + std::to_string(wire_.size()) +
          "\r\nConnection: close\r\n\r\n";
      sendAll(client, headers.data(), headers.size());
      if (mode_ == "short_header" && !first) count /= 2;
      sendAll(client, wire_.data() + first, count);
      shutdown(client, SHUT_RDWR); close(client);
    }
  }
};
static std::vector<uint8_t> vorbis(unsigned bitrate) {
  vorbis_info info; vorbis_info_init(&info);
  require(vorbis_encode_init(&info, 2, 44100, -1, bitrate * 1000, -1) == 0, "Vorbis encoder");
  vorbis_comment comment; vorbis_comment_init(&comment);
  vorbis_dsp_state dsp; vorbis_analysis_init(&dsp, &info);
  vorbis_block block; vorbis_block_init(&dsp, &block);
  ogg_stream_state stream; ogg_stream_init(&stream, bitrate);
  ogg_packet h1, h2, h3; vorbis_analysis_headerout(&dsp, &comment, &h1, &h2, &h3);
  ogg_stream_packetin(&stream, &h1); ogg_stream_packetin(&stream, &h2); ogg_stream_packetin(&stream, &h3);
  std::vector<uint8_t> encoded;
  auto pageOut = [&](bool flush) {
    ogg_page page;
    while ((flush ? ogg_stream_flush(&stream, &page) : ogg_stream_pageout(&stream, &page))) {
      encoded.insert(encoded.end(), page.header, page.header + page.header_len);
      encoded.insert(encoded.end(), page.body, page.body + page.body_len);
    }
  };
  pageOut(true);
  float** samples = vorbis_analysis_buffer(&dsp, 44100);
  for (int i = 0; i < 44100; ++i)
    for (int c = 0; c < 2; ++c) samples[c][i] = 0.5 * std::sin(2 * 3.141592653589793 * (c ? 439 : 997) * i / 44100);
  vorbis_analysis_wrote(&dsp, 44100);
  vorbis_analysis_wrote(&dsp, 0);
  while (vorbis_analysis_blockout(&dsp, &block) == 1) {
    vorbis_analysis(&block, nullptr); vorbis_bitrate_addblock(&block);
    ogg_packet packet;
    while (vorbis_bitrate_flushpacket(&dsp, &packet)) { ogg_stream_packetin(&stream, &packet); pageOut(false); }
  }
  pageOut(true);
  ogg_stream_clear(&stream); vorbis_block_clear(&block); vorbis_dsp_clear(&dsp);
  vorbis_comment_clear(&comment); vorbis_info_clear(&info);
  return encoded;
}
static size_t readCb(void* p, size_t size, size_t count, void* opaque) {
  return size ? static_cast<cspot::CDNAudioFile*>(opaque)->readBytes(static_cast<uint8_t*>(p), size * count) / size : 0;
}
static int seekCb(void* opaque, ogg_int64_t offset, int whence) {
  auto& file = *static_cast<cspot::CDNAudioFile*>(opaque);
  const int64_t base = whence == SEEK_SET ? 0 : whence == SEEK_CUR ? file.getPosition() : file.getSize();
  if (offset < -base || offset > static_cast<int64_t>(file.getSize()) - base) return -1;
  file.seek(base + offset); return 0;
}
static long tellCb(void* opaque) { return static_cast<cspot::CDNAudioFile*>(opaque)->getPosition(); }
static void queue(const std::string& test) {
  auto ctx = std::make_shared<cspot::Context>(); ctx->config.countryCode = "US";
  ctx->config.qualityMode = test == "queue_lossless" ? cspot::QualityMode::Lossless :
      test == "queue_320" ? cspot::QualityMode::Vorbis320 : cspot::QualityMode::Auto;
  cspot::TrackReference ref; ref.type = cspot::TrackReference::Type::TRACK;
  cspot::QueuedTrack track(ref, ctx);
  PB_BYTES_ARRAY_T(20) gid{}; gid.size = 20;
  AudioFile files[3]{};
  for (int i = 0; i < 3; ++i) {
    files[i].has_format = true;
    files[i].format = i == 0 ? AudioFormat_FLAC_FLAC : i == 1 ? AudioFormat_OGG_VORBIS_320 : AudioFormat_OGG_VORBIS_96;
    files[i].file_id = reinterpret_cast<pb_bytes_array_t*>(&gid);
  }
  if (test == "queue_missing_format") files[1].has_format = false;
  Track pb{}; pb.name = const_cast<char*>("Original test tone");
  pb.gid = reinterpret_cast<pb_bytes_array_t*>(&gid); pb.file = files; pb.file_count = 3;
  Restriction restriction{}; restriction.countries_forbidden = const_cast<char*>("US");
  if (test == "queue_restriction") { pb.restriction = &restriction; pb.restriction_count = 1; }
  track.stepParseMetadata(&pb, nullptr);
  if (test == "queue_lossless" || test == "queue_missing_format" || test == "queue_restriction")
    require(track.state == cspot::QueuedTrack::State::FAILED, "queue fail closed");
  else require(track.state == cspot::QueuedTrack::State::KEY_REQUIRED && track.selectedAudioFormat == 2,
               "queue must select actual Vorbis 320 metadata before requesting a key");
}
int main(int argc, char** argv) {
  bell::setDefaultLogger();
  try {
    require(argc == 2, "native test case required");
    const std::string test = argv[1];
    if (test.starts_with("queue")) queue(test);
    else if (test == "key_invalid") fails([] { cspot::CDNAudioFile file("http://127.0.0.1", {1, 2}); });
    else if (test.starts_with("vorbis")) {
      CdnServer server(vorbis(std::stoul(test.substr(6))), "normal");
      cspot::CDNAudioFile file(server.url, testKey); file.openStream();
      OggVorbis_File decoder{}; ov_callbacks callbacks{readCb, seekCb, nullptr, tellCb};
      require(ov_open_callbacks(&file, &decoder, nullptr, 0, callbacks) == 0, "real Vorbis decode open");
      const auto* info = ov_info(&decoder, -1);
      require(info && info->rate == 44100 && info->channels == 2, "Vorbis output format");
      require(info->bitrate_nominal == static_cast<long>(std::stoul(test.substr(6)) * 1000),
              "actual Vorbis header nominal bitrate matches 96/160/320 fixture");
      std::vector<int16_t> pcm; int section = 0; char buffer[4096]; long n;
      while ((n = ov_read(&decoder, buffer, sizeof(buffer), 0, 2, 1, &section)) > 0) {
        const auto* samples = reinterpret_cast<int16_t*>(buffer);
        pcm.insert(pcm.end(), samples, samples + n / 2);
      }
      require(n == 0 && pcm.size() == 44100 * 2, "complete Vorbis fixture decode");
      double power = 0; for (auto sample : pcm) power += double(sample) * sample;
      require(std::sqrt(power / pcm.size()) > 10000, "non-silent decoded Vorbis PCM");
      require(ov_time_seek(&decoder, 0.5) == 0, "Vorbis seek on actual CDN adapter");
      require(ov_read(&decoder, buffer, sizeof(buffer), 0, 2, 1, &section) > 0, "decode after Vorbis seek");
      require(ov_time_seek(&decoder, 0.0) == 0, "Vorbis seek to zero");
      ov_clear(&decoder);
    } else {
      std::vector<uint8_t> payload(test == "cdn_small" ? 200 : 50000);
      for (size_t i = 0; i < payload.size(); ++i) payload[i] = (i * 29 + 17) % 256;
      const std::string mode = test == "cdn_short_header" ? "short_header" :
          test == "cdn_oversized" ? "oversized" : test == "cdn_tiny" ? "tiny" : "normal";
      CdnServer server(payload, mode);
      cspot::CDNAudioFile file(server.url, testKey);
      if (mode == "short_header" || mode == "tiny") fails([&] { file.openStream(); });
      else {
        file.openStream(); require(file.getSize() == payload.size(), "strip only the 167-byte Vorbis prefix");
        std::vector<uint8_t> received(payload.size() + 1000);
        if (test == "cdn_seek") {
          for (const size_t pos : {0, 8123, 17000, 39999, 49999}) {
            file.seek(pos); const auto n = file.readBytes(received.data(), 256);
            require(n > 0 && std::equal(received.begin(), received.begin()+n, payload.begin()+pos),
                    "aligned AES-CTR random seeks preserve original payload");
          }
          fails([&] { file.seek(payload.size()+1); });
        } else if (test == "cdn_eof") {
          file.seek(payload.size()-1);
          require(file.readBytes(received.data(), 1000) == 1 && received[0] == payload.back(), "last byte and clamped read");
          require(file.readBytes(received.data(), 1) == 0, "EOF after final byte");
        } else if (test == "cdn_oversized") {
          file.seek(17000); require(file.readBytes(received.data(), 1000) == 0, "reject oversized HTTP body before buffer write");
        } else {
          size_t total = 0, n;
          while ((n = file.readBytes(received.data()+total, 2000))) total += n;
          require(total == payload.size() && std::equal(payload.begin(), payload.end(), received.begin()),
                  "HTTP range reads and AES-CTR decode match every original byte");
        }
      }
    }
    std::cout << "PASS " << test << '\n'; return 0;
  } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
