#ifndef EDIT_ATLAS_MEDIA_VIDEO_DECODER_HPP_
#define EDIT_ATLAS_MEDIA_VIDEO_DECODER_HPP_

#include <edit_atlas/core/rgb_image.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace edit_atlas::media {

/// A rational value whose denominator is never zero when present in the API.
struct Rational final {
    /// The numerator.
    std::int32_t numerator;
    /// The denominator.
    std::int32_t denominator;

    /// Compares both rational components.
    bool operator==(const Rational &) const = default;
};

/// Identifies the kind of content carried by a media stream.
enum class MediaStreamType {
    /// A video stream.
    kVideo,
    /// An audio stream.
    kAudio,
    /// A subtitle stream.
    kSubtitle,
    /// A data or metadata stream.
    kData,
    /// A stream type not represented by this API.
    kOther,
};

/// Identifies a normalized media container.
enum class MediaContainer {
    /// A QuickTime Movie container.
    kMov,
    /// An MPEG-4 Part 14 container.
    kMp4,
    /// A Material Exchange Format container.
    kMxf,
    /// A container outside the supported media policy.
    kOther,
};

/// A stable metadata key and its textual value.
struct MediaMetadataEntry final {
    /// The metadata key exactly as reported by the container.
    std::string key;
    /// The metadata value exactly as reported by the container.
    std::string value;

    /// Compares the key and value.
    bool operator==(const MediaMetadataEntry &) const = default;
};

/// Presentation-neutral information about one media stream.
struct MediaStreamInfo final {
    /// The zero-based stream index in the container.
    std::size_t index;
    /// The kind of content carried by the stream.
    MediaStreamType type;
    /// The canonical codec identifier, or an empty string when unknown.
    std::string codec_name;
    /// The human-readable codec name, or an empty string when unknown.
    std::string codec_long_name;
    /// The stream time base, when valid.
    std::optional<Rational> time_base;
    /// The average frame rate reported for a video stream, when valid.
    std::optional<Rational> average_frame_rate;
    /// The nominal frame rate reported for a video stream, when valid.
    std::optional<Rational> nominal_frame_rate;
    /// The stream duration expressed in units of `time_base`.
    std::optional<std::int64_t> duration;
    /// The first stream timestamp expressed in units of `time_base`.
    std::optional<std::int64_t> start_time;
    /// The reported frame count, when present.
    std::optional<std::int64_t> frame_count;
    /// The coded video width, or zero for non-video streams.
    std::int32_t width;
    /// The coded video height, or zero for non-video streams.
    std::int32_t height;
    /// Container metadata attached to this stream.
    std::vector<MediaMetadataEntry> metadata;
};

/// Information discovered while opening a video media file.
struct VideoMediaInfo final {
    /// The source path supplied by the caller.
    std::filesystem::path path;
    /// The normalized detected container family.
    MediaContainer container;
    /// The comma-separated canonical identifiers for the detected container.
    std::string container_names;
    /// The human-readable detected container name.
    std::string container_long_name;
    /// The container duration in microseconds, when present.
    std::optional<std::int64_t> duration_microseconds;
    /// The first container timestamp in microseconds, when present.
    std::optional<std::int64_t> start_time_microseconds;
    /// Metadata attached to the container.
    std::vector<MediaMetadataEntry> metadata;
    /// Every stream discovered in the container.
    std::vector<MediaStreamInfo> streams;
    /// The selected video stream's index in `streams`.
    std::size_t selected_video_stream;
};

/// Maximum dimensions for decoded frame output.
struct VideoFrameSizeLimit final {
    /// Maximum output width in pixels.
    std::int32_t maximum_width;
    /// Maximum output height in pixels.
    std::int32_t maximum_height;
};

/// Options controlling decoded frame output without changing its timing.
struct VideoFrameOutputOptions final {
    /// Optional bounds; absent bounds preserve the source dimensions.
    std::optional<VideoFrameSizeLimit> size_limit;
};

/// An RGB24 frame decoded from the selected video stream.
struct DecodedVideoFrame final {
    /// Zero-based frame index relative to the beginning of the video stream.
    std::optional<std::int64_t> frame_index;
    /// The presentation timestamp in the selected stream's time base.
    std::optional<std::int64_t> presentation_timestamp;
    /// Owned RGB24 output at the requested size.
    core::RgbImage image;
    /// Whether the decoder identifies this frame as a key frame.
    bool key_frame;
};

/// Identifies the stage at which media decoding failed.
enum class VideoDecoderFailureKind {
    /// The input path could not be opened as media.
    kOpenInput,
    /// Stream information could not be read.
    kReadStreamInformation,
    /// The detected container is outside the supported MOV, MP4, and MXF set.
    kUnsupportedContainer,
    /// The media contains no video stream.
    kNoVideoStream,
    /// The selected stream uses a codec outside the supported policy.
    kUnsupportedCodec,
    /// A decoder could not be created or opened.
    kOpenDecoder,
    /// Seeking the selected stream failed.
    kSeek,
    /// Reading a packet failed.
    kReadPacket,
    /// Decoding a frame failed.
    kDecodeFrame,
    /// Converting a decoded frame to RGB24 failed.
    kConvertFrame,
};

/// A structured, presentation-neutral media decoding failure.
struct VideoDecoderFailure final {
    /// The source associated with the failure.
    std::filesystem::path path;
    /// The stage at which the failure occurred.
    VideoDecoderFailureKind kind;
    /// The backend error code, or zero when not applicable.
    int backend_error;
    /// A stable codec name when a codec was identified.
    std::string codec_name;
    /// An actionable, nonlocalized technical description.
    std::string message;
};

/// Reports the exact linked video backend for diagnostics and compliance.
struct VideoBackendInformation final {
    /// The backend's human-readable name.
    std::string name;
    /// The linked backend version string.
    std::string version;
    /// The license reported by the linked backend.
    std::string license;
    /// The configure arguments used to build the linked backend.
    std::string configuration;
};

/// Decodes a selected video stream without depending on a frontend toolkit.
///
/// Each instance owns its decoding state and must be used by one thread at a
/// time. Different instances may be used concurrently.
class VideoDecoder {
  public:
    /// Opens a supported MOV, MP4, or MXF file and selects its best video
    /// stream.
    [[nodiscard]] static std::expected<std::unique_ptr<VideoDecoder>,
                                       VideoDecoderFailure>
    Open(const std::filesystem::path &path);

    /// Destroys the decoder through its interface.
    virtual ~VideoDecoder(void) = default;

    /// Decoders are non-copyable.
    VideoDecoder(const VideoDecoder &) = delete;
    /// Decoders are non-copy-assignable.
    VideoDecoder &operator=(const VideoDecoder &) = delete;
    /// Decoders are non-movable.
    VideoDecoder(VideoDecoder &&) = delete;
    /// Decoders are non-move-assignable.
    VideoDecoder &operator=(VideoDecoder &&) = delete;

    /// Returns information discovered while opening the media.
    [[nodiscard]] virtual const VideoMediaInfo &
    Information(void) const noexcept = 0;

    /// Seeks backward to the nearest usable point at or before `timestamp`.
    ///
    /// The timestamp uses the selected video stream's time base. A caller can
    /// decode forward from the returned position to reach an exact frame.
    [[nodiscard]] virtual std::expected<void, VideoDecoderFailure>
    Seek(std::int64_t timestamp) = 0;

    /// Seeks backward to a usable point at or before a zero-based frame index.
    [[nodiscard]] virtual std::expected<void, VideoDecoderFailure>
    SeekToFrame(std::int64_t frame_index) = 0;

    /// Decodes the next selected-stream frame at its source dimensions.
    [[nodiscard]] std::expected<std::optional<DecodedVideoFrame>,
                                VideoDecoderFailure>
    ReadFrame(void) {
        return ReadFrame(VideoFrameOutputOptions{});
    }

    /// Decodes the next selected-stream frame, or returns no frame at EOF.
    ///
    /// When maximum dimensions are supplied, the implementation preserves the
    /// source aspect ratio and never enlarges the image.
    [[nodiscard]] virtual std::expected<std::optional<DecodedVideoFrame>,
                                        VideoDecoderFailure>
    ReadFrame(const VideoFrameOutputOptions &options) = 0;

  protected:
    /// Constructs the interface for a concrete decoder.
    VideoDecoder(void) = default;
};

/// Returns build and license information from the linked video backend.
[[nodiscard]] VideoBackendInformation GetVideoBackendInformation(void);

} // namespace edit_atlas::media

#endif // EDIT_ATLAS_MEDIA_VIDEO_DECODER_HPP_
