#pragma once

#include "envoy/buffer/buffer.h"
#include "envoy/service/tap/v2alpha/common.pb.h"

#include "extensions/common/tap/tap.h"
#include "extensions/common/tap/tap_matcher.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Tap {

/**
 * fixfix
 */
class Utility {
public:
  /**
   * fixfix
   */
  static void addBufferToProtoBytes(envoy::data::tap::v2alpha::Body& output_body,
                                    uint32_t max_buffered_bytes, const Buffer::Instance& data);
};

/**
 * Base class for all tap configurations.
 * TODO(mattklein123): This class will handle common functionality such as rate limiting, etc.
 */
class TapConfigBaseImpl {
public:
  size_t numMatchers() { return matchers_.size(); }
  const envoy::service::tap::v2alpha::TapConfig& protoConfig() { return proto_config_; }
  Matcher& rootMatcher();
  void
  submitBufferedTrace(const std::shared_ptr<envoy::data::tap::v2alpha::BufferedTraceWrapper>& trace,
                      uint64_t trace_id);

protected:
  TapConfigBaseImpl(envoy::service::tap::v2alpha::TapConfig&& proto_config,
                    Common::Tap::Sink* admin_streamer);

private:
  const envoy::service::tap::v2alpha::TapConfig proto_config_;
  Sink* sink_to_use_;
  SinkPtr sink_;
  envoy::service::tap::v2alpha::OutputSink::Format::Enum sink_format_;
  std::vector<MatcherPtr> matchers_;
};

/**
 * A tap sink that writes each tap trace to a discrete output file.
 */
class FilePerTapSink : public Sink {
public:
  FilePerTapSink(const envoy::service::tap::v2alpha::FilePerTapSink& config) : config_(config) {}

  // Sink
  void
  submitBufferedTrace(const std::shared_ptr<envoy::data::tap::v2alpha::BufferedTraceWrapper>& trace,
                      envoy::service::tap::v2alpha::OutputSink::Format::Enum format,
                      uint64_t trace_id) override;

private:
  const envoy::service::tap::v2alpha::FilePerTapSink config_;
};

} // namespace Tap
} // namespace Common
} // namespace Extensions
} // namespace Envoy
