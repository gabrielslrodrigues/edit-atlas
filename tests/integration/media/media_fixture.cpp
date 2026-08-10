#include <edit_atlas/test/media_fixture.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
}

#include <array>
#include <cerrno>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace edit_atlas::media::test {
namespace {

struct OutputContextDeleter final {
    void operator()(AVFormatContext *context) const noexcept {
        if (context == nullptr) {
            return;
        }
        if (context->pb != nullptr) {
            avio_closep(&context->pb);
        }
        avformat_free_context(context);
    }
};

struct CodecContextDeleter final {
    void operator()(AVCodecContext *context) const noexcept {
        avcodec_free_context(&context);
    }
};

struct FrameDeleter final {
    void operator()(AVFrame *frame) const noexcept { av_frame_free(&frame); }
};

struct PacketDeleter final {
    void operator()(AVPacket *packet) const noexcept {
        av_packet_free(&packet);
    }
};

using OutputContextPointer =
    std::unique_ptr<AVFormatContext, OutputContextDeleter>;
using CodecContextPointer =
    std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using FramePointer = std::unique_ptr<AVFrame, FrameDeleter>;
using PacketPointer = std::unique_ptr<AVPacket, PacketDeleter>;

[[nodiscard]] std::string PathText(const std::filesystem::path &path) {
    const auto value = path.u8string();
    return {reinterpret_cast<const char *>(value.data()), value.size()};
}

[[nodiscard]] std::string Failure(std::string_view operation, int error) {
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer{};
    static_cast<void>(av_strerror(error, buffer.data(), buffer.size()));
    auto message = std::string{operation};
    message.append(": ");
    message.append(buffer.data());
    return message;
}

[[nodiscard]] std::expected<void, std::string>
WriteAvailablePackets(AVFormatContext &format_context,
                      AVCodecContext &codec_context, AVStream &stream,
                      AVPacket &packet) {
    while (true) {
        const auto receive_result =
            avcodec_receive_packet(&codec_context, &packet);
        if (receive_result == AVERROR(EAGAIN) ||
            receive_result == AVERROR_EOF) {
            return {};
        }
        if (receive_result < 0) {
            return std::unexpected(
                Failure("could not receive an encoded packet", receive_result));
        }

        av_packet_rescale_ts(&packet, codec_context.time_base,
                             stream.time_base);
        packet.stream_index = stream.index;
        const auto write_result =
            av_interleaved_write_frame(&format_context, &packet);
        av_packet_unref(&packet);
        if (write_result < 0) {
            return std::unexpected(
                Failure("could not write a fixture packet", write_result));
        }
    }
}

void FillFrame(AVFrame &frame, int frame_index) {
    if (frame.format == static_cast<int>(AV_PIX_FMT_RGB24)) {
        for (int row = 0; row < frame.height; ++row) {
            for (int column = 0; column < frame.width; ++column) {
                const auto offset = row * frame.linesize[0] + column * 3;
                frame.data[0][offset] =
                    static_cast<std::uint8_t>((column + frame_index * 7) % 256);
                frame.data[0][offset + 1] =
                    static_cast<std::uint8_t>((row + frame_index * 11) % 256);
                frame.data[0][offset + 2] = 128U;
            }
        }
        return;
    }

    for (int row = 0; row < frame.height; ++row) {
        for (int column = 0; column < frame.width; ++column) {
            frame.data[0][row * frame.linesize[0] + column] =
                static_cast<std::uint8_t>((column + row + frame_index * 7) %
                                          256);
        }
    }
    for (int row = 0; row < frame.height / 2; ++row) {
        for (int column = 0; column < frame.width / 2; ++column) {
            frame.data[1][row * frame.linesize[1] + column] =
                static_cast<std::uint8_t>(96 + frame_index * 4);
            frame.data[2][row * frame.linesize[2] + column] =
                static_cast<std::uint8_t>(160 - frame_index * 4);
        }
    }
}

} // namespace

std::expected<void, std::string>
WriteVideoFixture(const std::filesystem::path &path,
                  std::string_view container_name,
                  const VideoFixtureOptions &options) {
    if (options.frame_rate_numerator <= 0 ||
        options.frame_rate_denominator <= 0 || options.frame_count <= 0) {
        return std::unexpected("the fixture timing must be positive");
    }
    AVFormatContext *raw_format_context = nullptr;
    const auto path_text = PathText(path);
    const auto container_text = std::string{container_name};
    const auto allocate_result = avformat_alloc_output_context2(
        &raw_format_context, nullptr, container_text.c_str(),
        path_text.c_str());
    OutputContextPointer format_context{raw_format_context};
    if (allocate_result < 0) {
        return std::unexpected(
            Failure("could not create the fixture container", allocate_result));
    }
    if (format_context == nullptr) {
        return std::unexpected("could not allocate the fixture container");
    }

    const auto codec_id = options.codec == FixtureVideoCodec::kMpeg2Video
                              ? AV_CODEC_ID_MPEG2VIDEO
                              : AV_CODEC_ID_RAWVIDEO;
    const auto *encoder = avcodec_find_encoder(codec_id);
    if (encoder == nullptr) {
        return std::unexpected("the requested fixture encoder is unavailable");
    }
    auto *stream = avformat_new_stream(format_context.get(), nullptr);
    if (stream == nullptr) {
        return std::unexpected("could not allocate the fixture video stream");
    }

    CodecContextPointer codec_context{avcodec_alloc_context3(encoder)};
    if (codec_context == nullptr) {
        return std::unexpected("could not allocate the fixture encoder");
    }
    codec_context->codec_id = codec_id;
    codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_context->width = 720;
    codec_context->height = 576;
    codec_context->pix_fmt = options.codec == FixtureVideoCodec::kMpeg2Video
                                 ? AV_PIX_FMT_YUV420P
                                 : AV_PIX_FMT_RGB24;
    codec_context->time_base = AVRational{options.frame_rate_denominator,
                                          options.frame_rate_numerator};
    codec_context->framerate = AVRational{options.frame_rate_numerator,
                                          options.frame_rate_denominator};
    if (options.codec == FixtureVideoCodec::kMpeg2Video) {
        codec_context->bit_rate = 1'000'000;
        codec_context->gop_size = 1;
        codec_context->max_b_frames = 0;
    }
    if ((format_context->oformat->flags & AVFMT_GLOBALHEADER) != 0) {
        codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    const auto open_result =
        avcodec_open2(codec_context.get(), encoder, nullptr);
    if (open_result < 0) {
        return std::unexpected(
            Failure("could not open the fixture encoder", open_result));
    }
    const auto parameters_result =
        avcodec_parameters_from_context(stream->codecpar, codec_context.get());
    if (parameters_result < 0) {
        return std::unexpected(Failure("could not configure the fixture stream",
                                       parameters_result));
    }
    stream->time_base = codec_context->time_base;
    stream->avg_frame_rate = codec_context->framerate;
    stream->r_frame_rate = codec_context->framerate;
    if (options.starting_timecode.has_value()) {
        const auto *timecode = options.starting_timecode->c_str();
        if (av_dict_set(&format_context->metadata, "timecode", timecode, 0) <
                0 ||
            av_dict_set(&stream->metadata, "timecode", timecode, 0) < 0) {
            return std::unexpected(
                "could not set the fixture starting timecode");
        }
    }

    if ((format_context->oformat->flags & AVFMT_NOFILE) == 0) {
        const auto io_result =
            avio_open(&format_context->pb, path_text.c_str(), AVIO_FLAG_WRITE);
        if (io_result < 0) {
            return std::unexpected(
                Failure("could not open the fixture output", io_result));
        }
    }
    const auto header_result =
        avformat_write_header(format_context.get(), nullptr);
    if (header_result < 0) {
        return std::unexpected(
            Failure("could not write the fixture header", header_result));
    }

    FramePointer frame{av_frame_alloc()};
    PacketPointer packet{av_packet_alloc()};
    if (frame == nullptr || packet == nullptr) {
        return std::unexpected("could not allocate fixture encoding buffers");
    }
    frame->format = codec_context->pix_fmt;
    frame->width = codec_context->width;
    frame->height = codec_context->height;
    const auto frame_buffer_result = av_frame_get_buffer(frame.get(), 0);
    if (frame_buffer_result < 0) {
        return std::unexpected(Failure("could not allocate the fixture frame",
                                       frame_buffer_result));
    }

    for (int frame_index = 0; frame_index < options.frame_count;
         ++frame_index) {
        const auto writable_result = av_frame_make_writable(frame.get());
        if (writable_result < 0) {
            return std::unexpected(Failure(
                "could not make the fixture frame writable", writable_result));
        }
        FillFrame(*frame, frame_index);
        frame->pts = frame_index;
        const auto send_result =
            avcodec_send_frame(codec_context.get(), frame.get());
        if (send_result < 0) {
            return std::unexpected(
                Failure("could not encode a fixture frame", send_result));
        }
        auto write_result = WriteAvailablePackets(
            *format_context, *codec_context, *stream, *packet);
        if (!write_result.has_value()) {
            return write_result;
        }
    }

    const auto drain_result = avcodec_send_frame(codec_context.get(), nullptr);
    if (drain_result < 0) {
        return std::unexpected(
            Failure("could not drain the fixture encoder", drain_result));
    }
    auto write_result = WriteAvailablePackets(*format_context, *codec_context,
                                              *stream, *packet);
    if (!write_result.has_value()) {
        return write_result;
    }
    const auto trailer_result = av_write_trailer(format_context.get());
    if (trailer_result < 0) {
        return std::unexpected(
            Failure("could not write the fixture trailer", trailer_result));
    }
    return {};
}

} // namespace edit_atlas::media::test
