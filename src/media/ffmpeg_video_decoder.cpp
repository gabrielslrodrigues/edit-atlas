#include <edit_atlas/media/video_decoder.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_desc.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace edit_atlas::media {
namespace {

class FormatContext final {
  public:
    FormatContext(void) = default;
    ~FormatContext(void) { avformat_close_input(&context_); }

    FormatContext(const FormatContext &) = delete;
    FormatContext &operator=(const FormatContext &) = delete;
    FormatContext(FormatContext &&) = delete;
    FormatContext &operator=(FormatContext &&) = delete;

    [[nodiscard]] AVFormatContext **Address(void) noexcept { return &context_; }
    [[nodiscard]] AVFormatContext *Get(void) const noexcept { return context_; }

  private:
    AVFormatContext *context_ = nullptr;
};

struct CodecContextDeleter final {
    void operator()(AVCodecContext *context) const noexcept {
        avcodec_free_context(&context);
    }
};

struct PacketDeleter final {
    void operator()(AVPacket *packet) const noexcept {
        av_packet_free(&packet);
    }
};

struct FrameDeleter final {
    void operator()(AVFrame *frame) const noexcept { av_frame_free(&frame); }
};

struct ScaleContextDeleter final {
    void operator()(SwsContext *context) const noexcept {
        sws_freeContext(context);
    }
};

struct FrameDimensions final {
    int width;
    int height;
};

using CodecContextPointer =
    std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using PacketPointer = std::unique_ptr<AVPacket, PacketDeleter>;
using FramePointer = std::unique_ptr<AVFrame, FrameDeleter>;
using ScaleContextPointer = std::unique_ptr<SwsContext, ScaleContextDeleter>;

[[nodiscard]] std::string PathText(const std::filesystem::path &path) {
    const auto value = path.u8string();
    return {reinterpret_cast<const char *>(value.data()), value.size()};
}

[[nodiscard]] std::string ErrorText(int error) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer{};
    if (av_strerror(error, buffer.data(), buffer.size()) < 0) {
        return "unknown FFmpeg error";
    }
    return buffer.data();
}

[[nodiscard]] VideoDecoderFailure MakeFailure(const std::filesystem::path &path,
                                              VideoDecoderFailureKind kind,
                                              int backend_error,
                                              std::string codec_name,
                                              std::string_view operation) {
    auto message = std::string{operation};
    if (backend_error < 0) {
        message.append(": ");
        message.append(ErrorText(backend_error));
    }
    return VideoDecoderFailure{
        .path = path,
        .kind = kind,
        .backend_error = backend_error,
        .codec_name = std::move(codec_name),
        .message = std::move(message),
    };
}

[[nodiscard]] std::optional<Rational> OptionalRational(AVRational value) {
    if (value.num == 0 || value.den == 0) {
        return std::nullopt;
    }
    return Rational{.numerator = value.num, .denominator = value.den};
}

[[nodiscard]] std::optional<std::int64_t>
OptionalTimestamp(std::int64_t value) {
    return value == AV_NOPTS_VALUE ? std::nullopt
                                   : std::optional<std::int64_t>{value};
}

[[nodiscard]] std::vector<MediaMetadataEntry>
MetadataEntries(const AVDictionary *metadata) {
    std::vector<MediaMetadataEntry> entries;
    const AVDictionaryEntry *entry = nullptr;
    while ((entry = av_dict_iterate(metadata, entry)) != nullptr) {
        entries.push_back(MediaMetadataEntry{
            .key = entry->key,
            .value = entry->value,
        });
    }
    return entries;
}

[[nodiscard]] MediaStreamType StreamType(AVMediaType type) noexcept {
    switch (type) {
    case AVMEDIA_TYPE_VIDEO:
        return MediaStreamType::kVideo;
    case AVMEDIA_TYPE_AUDIO:
        return MediaStreamType::kAudio;
    case AVMEDIA_TYPE_SUBTITLE:
        return MediaStreamType::kSubtitle;
    case AVMEDIA_TYPE_DATA:
    case AVMEDIA_TYPE_ATTACHMENT:
        return MediaStreamType::kData;
    case AVMEDIA_TYPE_UNKNOWN:
    case AVMEDIA_TYPE_NB:
        return MediaStreamType::kOther;
    }
    return MediaStreamType::kOther;
}

[[nodiscard]] std::string CodecName(AVCodecID codec_id) {
    const auto *name = avcodec_get_name(codec_id);
    return name == nullptr ? std::string{} : std::string{name};
}

[[nodiscard]] bool IsSupportedCodec(AVCodecID codec_id) noexcept {
    switch (codec_id) {
    case AV_CODEC_ID_H264:
    case AV_CODEC_ID_HEVC:
    case AV_CODEC_ID_PRORES:
    case AV_CODEC_ID_DNXHD:
    case AV_CODEC_ID_MPEG2VIDEO:
    case AV_CODEC_ID_MJPEG:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool ContainsFormatName(std::string_view names,
                                      std::string_view expected) noexcept {
    while (!names.empty()) {
        const auto separator = names.find(',');
        const auto name = names.substr(0, separator);
        if (name == expected) {
            return true;
        }
        if (separator == std::string_view::npos) {
            break;
        }
        names.remove_prefix(separator + 1);
    }
    return false;
}

[[nodiscard]] MediaContainer ContainerType(const AVFormatContext &context) {
    if (context.iformat == nullptr || context.iformat->name == nullptr) {
        return MediaContainer::kOther;
    }
    const auto names = std::string_view{context.iformat->name};
    if (ContainsFormatName(names, "mxf")) {
        return MediaContainer::kMxf;
    }
    if (ContainsFormatName(names, "mov") || ContainsFormatName(names, "mp4")) {
        const auto *major_brand =
            av_dict_get(context.metadata, "major_brand", nullptr, 0);
        if (major_brand == nullptr || major_brand->value == nullptr ||
            std::string_view{major_brand->value}.starts_with("qt")) {
            return MediaContainer::kMov;
        }
        return MediaContainer::kMp4;
    }
    return MediaContainer::kOther;
}

[[nodiscard]] MediaStreamInfo StreamInformation(const AVStream &stream) {
    const auto &parameters = *stream.codecpar;
    const auto *descriptor = avcodec_descriptor_get(parameters.codec_id);
    return MediaStreamInfo{
        .index = static_cast<std::size_t>(stream.index),
        .type = StreamType(parameters.codec_type),
        .codec_name = CodecName(parameters.codec_id),
        .codec_long_name =
            descriptor == nullptr || descriptor->long_name == nullptr
                ? std::string{}
                : std::string{descriptor->long_name},
        .time_base = OptionalRational(stream.time_base),
        .average_frame_rate = OptionalRational(stream.avg_frame_rate),
        .nominal_frame_rate = OptionalRational(stream.r_frame_rate),
        .duration = OptionalTimestamp(stream.duration),
        .start_time = OptionalTimestamp(stream.start_time),
        .frame_count = stream.nb_frames > 0
                           ? std::optional<std::int64_t>{stream.nb_frames}
                           : std::nullopt,
        .width = parameters.width,
        .height = parameters.height,
        .metadata = MetadataEntries(stream.metadata),
    };
}

class FfmpegVideoDecoder final : public VideoDecoder {
  public:
    FfmpegVideoDecoder(std::filesystem::path path,
                       std::unique_ptr<FormatContext> format_context,
                       CodecContextPointer codec_context,
                       std::size_t selected_video_stream,
                       VideoMediaInfo information)
        : path_{std::move(path)}, format_context_{std::move(format_context)},
          codec_context_{std::move(codec_context)}, packet_{av_packet_alloc()},
          frame_{av_frame_alloc()},
          selected_video_stream_{selected_video_stream},
          information_{std::move(information)} {}

    [[nodiscard]] bool HasBuffers(void) const noexcept {
        return packet_ != nullptr && frame_ != nullptr;
    }

    [[nodiscard]] const VideoMediaInfo &
    Information(void) const noexcept override {
        return information_;
    }

    [[nodiscard]] std::expected<void, VideoDecoderFailure>
    Seek(std::int64_t timestamp) override {
        const auto result = av_seek_frame(
            format_context_->Get(), static_cast<int>(selected_video_stream_),
            timestamp, AVSEEK_FLAG_BACKWARD);
        if (result < 0) {
            return std::unexpected(MakeFailure(
                path_, VideoDecoderFailureKind::kSeek, result,
                information_.streams[selected_video_stream_].codec_name,
                "could not seek the selected video stream"));
        }

        avcodec_flush_buffers(codec_context_.get());
        av_packet_unref(packet_.get());
        av_frame_unref(frame_.get());
        draining_ = false;
        exhausted_ = false;
        return {};
    }

    [[nodiscard]] std::expected<void, VideoDecoderFailure>
    SeekToFrame(std::int64_t frame_index) override {
        const auto *stream =
            format_context_->Get()->streams[selected_video_stream_];
        if (frame_index < 0 || stream->avg_frame_rate.num <= 0 ||
            stream->avg_frame_rate.den <= 0 || stream->time_base.num <= 0 ||
            stream->time_base.den <= 0) {
            return std::unexpected(MakeFailure(
                path_, VideoDecoderFailureKind::kSeek, 0,
                information_.streams[selected_video_stream_].codec_name,
                "could not convert the requested frame index to a media "
                "timestamp"));
        }
        const auto stream_start = stream->start_time == AV_NOPTS_VALUE
                                      ? std::int64_t{0}
                                      : stream->start_time;
        const auto timestamp_offset = av_rescale_q(
            frame_index, av_inv_q(stream->avg_frame_rate), stream->time_base);
        if (timestamp_offset < 0 ||
            stream_start >
                std::numeric_limits<std::int64_t>::max() - timestamp_offset) {
            return std::unexpected(MakeFailure(
                path_, VideoDecoderFailureKind::kSeek, 0,
                information_.streams[selected_video_stream_].codec_name,
                "the requested frame index is outside the supported media "
                "timestamp range"));
        }
        return Seek(stream_start + timestamp_offset);
    }

    [[nodiscard]] std::expected<std::optional<DecodedVideoFrame>,
                                VideoDecoderFailure>
    ReadFrame(const VideoFrameOutputOptions &options) override {
        if (exhausted_) {
            return std::optional<DecodedVideoFrame>{};
        }

        while (true) {
            const auto receive_result =
                avcodec_receive_frame(codec_context_.get(), frame_.get());
            if (receive_result == 0) {
                auto converted = ConvertFrame(options);
                av_frame_unref(frame_.get());
                if (!converted.has_value()) {
                    return std::unexpected(std::move(converted.error()));
                }
                return std::optional<DecodedVideoFrame>{std::move(*converted)};
            }
            if (receive_result == AVERROR_EOF) {
                exhausted_ = true;
                return std::optional<DecodedVideoFrame>{};
            }
            if (receive_result != AVERROR(EAGAIN)) {
                return std::unexpected(MakeFailure(
                    path_, VideoDecoderFailureKind::kDecodeFrame,
                    receive_result,
                    information_.streams[selected_video_stream_].codec_name,
                    "could not receive a decoded video frame"));
            }

            if (draining_) {
                return std::unexpected(MakeFailure(
                    path_, VideoDecoderFailureKind::kDecodeFrame, AVERROR_BUG,
                    information_.streams[selected_video_stream_].codec_name,
                    "the decoder requested input after end of stream"));
            }

            const auto packet_result = ReadVideoPacket();
            if (!packet_result.has_value()) {
                return std::unexpected(std::move(packet_result.error()));
            }
            if (!*packet_result) {
                draining_ = true;
                const auto send_result =
                    avcodec_send_packet(codec_context_.get(), nullptr);
                if (send_result < 0 && send_result != AVERROR_EOF) {
                    return std::unexpected(MakeFailure(
                        path_, VideoDecoderFailureKind::kDecodeFrame,
                        send_result,
                        information_.streams[selected_video_stream_].codec_name,
                        "could not drain the video decoder"));
                }
            }
        }
    }

  private:
    [[nodiscard]] std::expected<bool, VideoDecoderFailure>
    ReadVideoPacket(void) {
        while (true) {
            av_packet_unref(packet_.get());
            const auto read_result =
                av_read_frame(format_context_->Get(), packet_.get());
            if (read_result == AVERROR_EOF) {
                return false;
            }
            if (read_result < 0) {
                return std::unexpected(MakeFailure(
                    path_, VideoDecoderFailureKind::kReadPacket, read_result,
                    information_.streams[selected_video_stream_].codec_name,
                    "could not read the next media packet"));
            }
            if (packet_->stream_index !=
                static_cast<int>(selected_video_stream_)) {
                continue;
            }

            const auto send_result =
                avcodec_send_packet(codec_context_.get(), packet_.get());
            if (send_result == AVERROR(EAGAIN)) {
                return std::unexpected(MakeFailure(
                    path_, VideoDecoderFailureKind::kDecodeFrame, send_result,
                    information_.streams[selected_video_stream_].codec_name,
                    "the decoder requested output while requesting input"));
            }
            if (send_result < 0) {
                return std::unexpected(MakeFailure(
                    path_, VideoDecoderFailureKind::kDecodeFrame, send_result,
                    information_.streams[selected_video_stream_].codec_name,
                    "could not submit a video packet to the decoder"));
            }
            return true;
        }
    }

    [[nodiscard]] std::expected<FrameDimensions, VideoDecoderFailure>
    ResolveOutputDimensions(const VideoFrameOutputOptions &options) const {
        if (frame_->width <= 0 || frame_->height <= 0 ||
            frame_->width > std::numeric_limits<int>::max() / 3) {
            return std::unexpected(MakeFailure(
                path_, VideoDecoderFailureKind::kConvertFrame, 0,
                information_.streams[selected_video_stream_].codec_name,
                "the decoded frame dimensions are invalid"));
        }

        auto output_width = frame_->width;
        auto output_height = frame_->height;
        if (options.size_limit.has_value()) {
            const auto &size_limit = *options.size_limit;
            if (size_limit.maximum_width <= 0 ||
                size_limit.maximum_height <= 0) {
                return std::unexpected(MakeFailure(
                    path_, VideoDecoderFailureKind::kConvertFrame, 0,
                    information_.streams[selected_video_stream_].codec_name,
                    "the requested frame output dimensions are invalid"));
            }
            if (output_width > size_limit.maximum_width ||
                output_height > size_limit.maximum_height) {
                const auto width_limited =
                    static_cast<std::int64_t>(size_limit.maximum_width) *
                        output_height <=
                    static_cast<std::int64_t>(size_limit.maximum_height) *
                        output_width;
                if (width_limited) {
                    output_height = std::max(
                        1, static_cast<int>(
                               static_cast<std::int64_t>(frame_->height) *
                               size_limit.maximum_width / frame_->width));
                    output_width = size_limit.maximum_width;
                } else {
                    output_width = std::max(
                        1, static_cast<int>(
                               static_cast<std::int64_t>(frame_->width) *
                               size_limit.maximum_height / frame_->height));
                    output_height = size_limit.maximum_height;
                }
            }
        }

        return FrameDimensions{
            .width = output_width,
            .height = output_height,
        };
    }

    [[nodiscard]] std::expected<core::RgbImage, VideoDecoderFailure>
    ConvertImage(const VideoFrameOutputOptions &options) {
        auto dimensions = ResolveOutputDimensions(options);
        if (!dimensions.has_value()) {
            return std::unexpected(std::move(dimensions.error()));
        }

        const auto row_stride =
            static_cast<std::size_t>(dimensions->width) * 3U;
        if (static_cast<std::size_t>(dimensions->height) >
            std::numeric_limits<std::size_t>::max() / row_stride) {
            return std::unexpected(MakeFailure(
                path_, VideoDecoderFailureKind::kConvertFrame, 0,
                information_.streams[selected_video_stream_].codec_name,
                "the decoded frame is too large to store"));
        }
        const auto pixel_count =
            row_stride * static_cast<std::size_t>(dimensions->height);
        std::vector<std::byte> pixels(pixel_count);

        auto *scale_context = sws_getCachedContext(
            scale_context_.release(), frame_->width, frame_->height,
            static_cast<AVPixelFormat>(frame_->format), dimensions->width,
            dimensions->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr,
            nullptr, nullptr);
        scale_context_.reset(scale_context);
        if (scale_context == nullptr) {
            return std::unexpected(MakeFailure(
                path_, VideoDecoderFailureKind::kConvertFrame, 0,
                information_.streams[selected_video_stream_].codec_name,
                "could not create the RGB frame converter"));
        }

        auto *destination = reinterpret_cast<std::uint8_t *>(pixels.data());
        std::array<std::uint8_t *, 4> destination_planes{destination, nullptr,
                                                         nullptr, nullptr};
        std::array<int, 4> destination_strides{static_cast<int>(row_stride), 0,
                                               0, 0};
        const auto converted_height = sws_scale(
            scale_context, frame_->data, frame_->linesize, 0, frame_->height,
            destination_planes.data(), destination_strides.data());
        if (converted_height != dimensions->height) {
            return std::unexpected(MakeFailure(
                path_, VideoDecoderFailureKind::kConvertFrame, 0,
                information_.streams[selected_video_stream_].codec_name,
                "could not convert the complete frame to RGB24"));
        }

        return core::RgbImage{
            .width = dimensions->width,
            .height = dimensions->height,
            .row_stride = row_stride,
            .pixels = std::move(pixels),
        };
    }

    [[nodiscard]] std::expected<DecodedVideoFrame, VideoDecoderFailure>
    ConvertFrame(const VideoFrameOutputOptions &options) {
        auto image = ConvertImage(options);
        if (!image.has_value()) {
            return std::unexpected(std::move(image.error()));
        }
        return DecodedVideoFrame{
            .frame_index = FrameIndex(frame_->best_effort_timestamp),
            .presentation_timestamp =
                OptionalTimestamp(frame_->best_effort_timestamp),
            .image = std::move(*image),
            .key_frame = (frame_->flags & AV_FRAME_FLAG_KEY) != 0,
        };
    }

    [[nodiscard]] std::optional<std::int64_t>
    FrameIndex(std::int64_t timestamp) const noexcept {
        const auto *stream =
            format_context_->Get()->streams[selected_video_stream_];
        if (timestamp == AV_NOPTS_VALUE || stream->avg_frame_rate.num <= 0 ||
            stream->avg_frame_rate.den <= 0 || stream->time_base.num <= 0 ||
            stream->time_base.den <= 0) {
            return std::nullopt;
        }
        const auto stream_start = stream->start_time == AV_NOPTS_VALUE
                                      ? std::int64_t{0}
                                      : stream->start_time;
        if ((stream_start > 0 &&
             timestamp <
                 std::numeric_limits<std::int64_t>::min() + stream_start) ||
            (stream_start < 0 &&
             timestamp >
                 std::numeric_limits<std::int64_t>::max() + stream_start)) {
            return std::nullopt;
        }
        const auto timestamp_offset = timestamp - stream_start;
        if (timestamp_offset < 0) {
            return std::nullopt;
        }
        const auto frame_index =
            av_rescale_q(timestamp_offset, stream->time_base,
                         av_inv_q(stream->avg_frame_rate));
        return frame_index < 0 ? std::nullopt
                               : std::optional<std::int64_t>{frame_index};
    }

    std::filesystem::path path_;
    std::unique_ptr<FormatContext> format_context_;
    CodecContextPointer codec_context_;
    PacketPointer packet_;
    FramePointer frame_;
    ScaleContextPointer scale_context_;
    std::size_t selected_video_stream_;
    VideoMediaInfo information_;
    bool draining_ = false;
    bool exhausted_ = false;
};

} // namespace

std::expected<std::unique_ptr<VideoDecoder>, VideoDecoderFailure>
VideoDecoder::Open(const std::filesystem::path &path) {
    auto format_context = std::make_unique<FormatContext>();
    const auto path_text = PathText(path);
    const auto open_result = avformat_open_input(
        format_context->Address(), path_text.c_str(), nullptr, nullptr);
    if (open_result < 0) {
        return std::unexpected(
            MakeFailure(path, VideoDecoderFailureKind::kOpenInput, open_result,
                        {}, "could not open the media input"));
    }

    const auto stream_result =
        avformat_find_stream_info(format_context->Get(), nullptr);
    if (stream_result < 0) {
        return std::unexpected(MakeFailure(
            path, VideoDecoderFailureKind::kReadStreamInformation,
            stream_result, {}, "could not read media stream information"));
    }

    const auto container = ContainerType(*format_context->Get());
    if (container == MediaContainer::kOther) {
        return std::unexpected(
            MakeFailure(path, VideoDecoderFailureKind::kUnsupportedContainer, 0,
                        {}, "the detected container is not MOV, MP4, or MXF"));
    }

    const auto best_stream = av_find_best_stream(
        format_context->Get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (best_stream < 0) {
        return std::unexpected(MakeFailure(
            path, VideoDecoderFailureKind::kNoVideoStream, best_stream, {},
            "the media contains no usable video stream"));
    }

    const auto selected_video_stream = static_cast<std::size_t>(best_stream);
    const auto *stream = format_context->Get()->streams[best_stream];
    const auto codec_name = CodecName(stream->codecpar->codec_id);
    if (!IsSupportedCodec(stream->codecpar->codec_id)) {
        auto failure = MakeFailure(
            path, VideoDecoderFailureKind::kUnsupportedCodec, 0, codec_name,
            "the selected video codec is unsupported; supported codecs are "
            "H.264, HEVC, ProRes, DNxHD/DNxHR, MPEG-2 Video, and Motion JPEG");
        return std::unexpected(std::move(failure));
    }

    const auto *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (codec == nullptr) {
        return std::unexpected(MakeFailure(
            path, VideoDecoderFailureKind::kOpenDecoder, 0, codec_name,
            "the linked FFmpeg build has no decoder for the selected codec"));
    }

    CodecContextPointer codec_context{avcodec_alloc_context3(codec)};
    if (codec_context == nullptr) {
        return std::unexpected(MakeFailure(
            path, VideoDecoderFailureKind::kOpenDecoder, AVERROR(ENOMEM),
            codec_name, "could not allocate the video decoder"));
    }
    const auto parameter_result =
        avcodec_parameters_to_context(codec_context.get(), stream->codecpar);
    if (parameter_result < 0) {
        return std::unexpected(MakeFailure(
            path, VideoDecoderFailureKind::kOpenDecoder, parameter_result,
            codec_name, "could not configure the video decoder"));
    }
    codec_context->pkt_timebase = stream->time_base;
    const auto decoder_result =
        avcodec_open2(codec_context.get(), codec, nullptr);
    if (decoder_result < 0) {
        return std::unexpected(MakeFailure(
            path, VideoDecoderFailureKind::kOpenDecoder, decoder_result,
            codec_name, "could not open the video decoder"));
    }

    std::vector<MediaStreamInfo> streams;
    streams.reserve(format_context->Get()->nb_streams);
    for (unsigned int index = 0; index < format_context->Get()->nb_streams;
         ++index) {
        streams.push_back(
            StreamInformation(*format_context->Get()->streams[index]));
    }

    const auto *input_format = format_context->Get()->iformat;
    VideoMediaInfo information{
        .path = path,
        .container = container,
        .container_names =
            input_format->name == nullptr ? std::string{} : input_format->name,
        .container_long_name = input_format->long_name == nullptr
                                   ? std::string{}
                                   : input_format->long_name,
        .duration_microseconds =
            OptionalTimestamp(format_context->Get()->duration),
        .start_time_microseconds =
            OptionalTimestamp(format_context->Get()->start_time),
        .metadata = MetadataEntries(format_context->Get()->metadata),
        .streams = std::move(streams),
        .selected_video_stream = selected_video_stream,
    };

    auto decoder = std::make_unique<FfmpegVideoDecoder>(
        path, std::move(format_context), std::move(codec_context),
        selected_video_stream, std::move(information));
    if (!decoder->HasBuffers()) {
        return std::unexpected(MakeFailure(
            path, VideoDecoderFailureKind::kOpenDecoder, AVERROR(ENOMEM),
            codec_name, "could not allocate video decoding buffers"));
    }
    return std::unique_ptr<VideoDecoder>{std::move(decoder)};
}

VideoBackendInformation GetVideoBackendInformation(void) {
    return VideoBackendInformation{
        .name = "FFmpeg",
        .version = av_version_info(),
        .license = avcodec_license(),
        .configuration = avcodec_configuration(),
    };
}

} // namespace edit_atlas::media
