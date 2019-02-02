#include "extensions/common/tap/tap_config_base.h"

#include <fstream>

#include "common/common/assert.h"
#include "common/common/stack_array.h"

#include "extensions/common/tap/tap_matcher.h"

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Tap {

void Utility::addBufferToProtoBytes(envoy::data::tap::v2alpha::Body& output_bytes,
                                    uint32_t max_buffered_bytes, const Buffer::Instance& data) {
  const uint64_t num_slices = data.getRawSlices(nullptr, 0);
  STACK_ARRAY(slices, Buffer::RawSlice, num_slices);
  data.getRawSlices(slices.begin(), num_slices);
  for (const Buffer::RawSlice& slice : slices) {
    if (slice.len_ > max_buffered_bytes - output_bytes.as_bytes().size()) {
      output_bytes.set_truncated(true);
    }

    output_bytes.mutable_as_bytes()->append(
        static_cast<const char*>(slice.mem_),
        std::min(slice.len_, max_buffered_bytes - output_bytes.as_bytes().size()));
  }
}

TapConfigBaseImpl::TapConfigBaseImpl(envoy::service::tap::v2alpha::TapConfig&& proto_config,
                                     Common::Tap::Sink* admin_streamer)
    : proto_config_(std::move(proto_config)) {
  ASSERT(proto_config_.output_config().sinks().size() == 1);
  sink_format_ = proto_config_.output_config().sinks()[0].format();
  switch (proto_config_.output_config().sinks()[0].output_sink_type_case()) {
  case envoy::service::tap::v2alpha::OutputSink::kStreamingAdmin:
    // TODO(mattklein123): Graceful failure, error message, and test if someone specifies an
    // admin stream output without configuring via /tap.
    RELEASE_ASSERT(admin_streamer != nullptr, "admin output must be configured via admin");
    sink_to_use_ = admin_streamer;
    break;
  case envoy::service::tap::v2alpha::OutputSink::kFilePerTap:
    sink_ =
        std::make_unique<FilePerTapSink>(proto_config_.output_config().sinks()[0].file_per_tap());
    sink_to_use_ = sink_.get();
    break;
  default:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }

  buildMatcher(proto_config_.match_config(), matchers_);
}

Matcher& TapConfigBaseImpl::rootMatcher() {
  ASSERT(matchers_.size() >= 1);
  return *matchers_[0];
}

namespace {
void swapBytesToString(envoy::data::tap::v2alpha::Body& body) {
  body.set_allocated_as_string(body.release_as_bytes());
}
} // namespace

void TapConfigBaseImpl::submitBufferedTrace(
    const std::shared_ptr<envoy::data::tap::v2alpha::BufferedTraceWrapper>& trace,
    uint64_t trace_id) {
  // Swap the "bytes" string into the "string" string. This is done purely so that JSON
  // serialization will serialize as a string vs. doing base64 encoding.
  if (sink_format_ == envoy::service::tap::v2alpha::OutputSink::Format::JSON_BODY_AS_STRING) {
    switch (trace->trace_case()) {
    case envoy::data::tap::v2alpha::BufferedTraceWrapper::kHttpBufferedTrace: {
      auto* http_trace = trace->mutable_http_buffered_trace();
      if (http_trace->has_request() && http_trace->request().has_body()) {
        swapBytesToString(*http_trace->mutable_request()->mutable_body());
      }
      if (http_trace->has_response() && http_trace->response().has_body()) {
        swapBytesToString(*http_trace->mutable_response()->mutable_body());
      }
      break;
    }
    case envoy::data::tap::v2alpha::BufferedTraceWrapper::kSocketBufferedTrace:
      ASSERT(false); // fixfix
    case envoy::data::tap::v2alpha::BufferedTraceWrapper::TRACE_NOT_SET:
      NOT_REACHED_GCOVR_EXCL_LINE;
    }
  }

  sink_to_use_->submitBufferedTrace(trace, sink_format_, trace_id);
}

void FilePerTapSink::submitBufferedTrace(
    const std::shared_ptr<envoy::data::tap::v2alpha::BufferedTraceWrapper>& trace,
    envoy::service::tap::v2alpha::OutputSink::Format::Enum format, uint64_t trace_id) {
  std::string path = fmt::format("{}_{}", config_.path_prefix(), trace_id);
  switch (format) {
  case envoy::service::tap::v2alpha::OutputSink::Format::PROTO_BINARY:
    path += ".pb";
    break;
  case envoy::service::tap::v2alpha::OutputSink::Format::PROTO_TEXT:
    path += ".pb_text";
    break;
  case envoy::service::tap::v2alpha::OutputSink::Format::JSON_BODY_AS_BYTES:
    ASSERT(false); // fixfix
  case envoy::service::tap::v2alpha::OutputSink::Format::JSON_BODY_AS_STRING:
    ASSERT(false); // fixfix
  default:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }

  ENVOY_LOG_MISC(debug, "Writing tap for [id={}] to {}", trace_id, path);
  ENVOY_LOG_MISC(trace, "Tap for [id={}]: {}", trace_id, trace->DebugString());
  std::ofstream proto_stream(path);

  switch (format) {
  case envoy::service::tap::v2alpha::OutputSink::Format::PROTO_BINARY:
    trace->SerializeToOstream(&proto_stream);
    break;
  case envoy::service::tap::v2alpha::OutputSink::Format::PROTO_TEXT:
    proto_stream << trace->DebugString();
    break;
  case envoy::service::tap::v2alpha::OutputSink::Format::JSON_BODY_AS_BYTES:
    ASSERT(false); // fixfix
  case envoy::service::tap::v2alpha::OutputSink::Format::JSON_BODY_AS_STRING:
    ASSERT(false); // fixfix
  default:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }
}

} // namespace Tap
} // namespace Common
} // namespace Extensions
} // namespace Envoy
