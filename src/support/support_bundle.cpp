#include <edit_atlas/support/support_bundle.hpp>

#include <edit_atlas/support/application_logging.hpp>

#include <edit_atlas/storage/local_file.hpp>

#include <minizip/ioapi.h>
#include <minizip/zip.h>

#include <zlib.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace edit_atlas::support {
namespace {

constexpr std::size_t kIoBufferSize = 65'536;

enum class LogWriteResult {
    kSuccess,
    kReadFailed,
    kArchiveWriteFailed,
};

struct ZipStream final {
    std::fstream file;
    bool error = false;
};

[[nodiscard]] voidpf ZCALLBACK OpenZipStream(voidpf opaque, const void *,
                                             int) noexcept {
    try {
        const auto *path = static_cast<const std::filesystem::path *>(opaque);
        auto stream = std::unique_ptr<ZipStream>{new (std::nothrow) ZipStream};
        if (stream == nullptr) {
            return nullptr;
        }
        stream->file.open(*path, std::ios::binary | std::ios::in |
                                     std::ios::out | std::ios::trunc);
        if (!stream->file.is_open()) {
            return nullptr;
        }
        return stream.release();
    } catch (...) {
        return nullptr;
    }
}

[[nodiscard]] uLong ZCALLBACK ReadZipStream(voidpf, voidpf stream, void *buffer,
                                            uLong size) noexcept {
    auto *zip_stream = static_cast<ZipStream *>(stream);
    zip_stream->file.read(static_cast<char *>(buffer),
                          static_cast<std::streamsize>(size));
    if (zip_stream->file.bad()) {
        zip_stream->error = true;
        return 0;
    }
    return static_cast<uLong>(zip_stream->file.gcount());
}

[[nodiscard]] uLong ZCALLBACK WriteZipStream(voidpf, voidpf stream,
                                             const void *buffer,
                                             uLong size) noexcept {
    auto *zip_stream = static_cast<ZipStream *>(stream);
    zip_stream->file.write(static_cast<const char *>(buffer),
                           static_cast<std::streamsize>(size));
    if (!zip_stream->file) {
        zip_stream->error = true;
        return 0;
    }
    return size;
}

[[nodiscard]] ZPOS64_T ZCALLBACK TellZipStream(voidpf, voidpf stream) noexcept {
    auto *zip_stream = static_cast<ZipStream *>(stream);
    const auto position = zip_stream->file.tellp();
    if (position < 0) {
        zip_stream->error = true;
        return 0;
    }
    return static_cast<ZPOS64_T>(position);
}

[[nodiscard]] long ZCALLBACK SeekZipStream(voidpf, voidpf stream,
                                           ZPOS64_T offset,
                                           int origin) noexcept {
    auto *zip_stream = static_cast<ZipStream *>(stream);
    std::ios::seekdir direction;
    switch (origin) {
    case ZLIB_FILEFUNC_SEEK_CUR:
        direction = std::ios::cur;
        break;
    case ZLIB_FILEFUNC_SEEK_END:
        direction = std::ios::end;
        break;
    case ZLIB_FILEFUNC_SEEK_SET:
        direction = std::ios::beg;
        break;
    default:
        zip_stream->error = true;
        return -1;
    }

    if (offset >
        static_cast<ZPOS64_T>(std::numeric_limits<std::streamoff>::max())) {
        zip_stream->error = true;
        return -1;
    }
    zip_stream->file.clear();
    zip_stream->file.seekp(static_cast<std::streamoff>(offset), direction);
    if (!zip_stream->file) {
        zip_stream->error = true;
        return -1;
    }
    const auto position = zip_stream->file.tellp();
    if (position < 0) {
        zip_stream->error = true;
        return -1;
    }
    zip_stream->file.seekg(static_cast<std::streamoff>(position),
                           std::ios::beg);
    if (!zip_stream->file) {
        zip_stream->error = true;
        return -1;
    }
    return 0;
}

[[nodiscard]] int ZCALLBACK CloseZipStream(voidpf, voidpf stream) noexcept {
    auto zip_stream =
        std::unique_ptr<ZipStream>{static_cast<ZipStream *>(stream)};
    zip_stream->file.flush();
    zip_stream->file.close();
    return zip_stream->error || zip_stream->file.fail() ? -1 : 0;
}

[[nodiscard]] int ZCALLBACK ErrorZipStream(voidpf, voidpf stream) noexcept {
    return static_cast<ZipStream *>(stream)->error ? 1 : 0;
}

[[nodiscard]] zlib_filefunc64_def
ZipFileFunctions(std::filesystem::path &path) noexcept {
    return zlib_filefunc64_def{
        .zopen64_file = OpenZipStream,
        .zread_file = ReadZipStream,
        .zwrite_file = WriteZipStream,
        .ztell64_file = TellZipStream,
        .zseek64_file = SeekZipStream,
        .zclose_file = CloseZipStream,
        .zerror_file = ErrorZipStream,
        .opaque = &path,
    };
}

[[nodiscard]] std::string SafeLine(std::string value) {
    std::ranges::replace_if(
        value,
        [](char character) { return character == '\r' || character == '\n'; },
        ' ');
    return value;
}

[[nodiscard]] std::string Utf8Filename(const std::filesystem::path &path) {
    const auto utf8 = path.filename().u8string();
    return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
}

[[nodiscard]] std::string JoinValues(std::vector<std::string> values) {
    std::ranges::sort(values);
    std::string output;
    for (auto &value : values) {
        if (!output.empty()) {
            output += ", ";
        }
        output += SafeLine(std::move(value));
    }
    return output.empty() ? "none" : output;
}

[[nodiscard]] SupportBundleFailure Failure(const SupportBundleRequest &request,
                                           SupportBundleFailureKind kind,
                                           std::error_code error,
                                           std::string detail) {
    return SupportBundleFailure{
        .path = request.path,
        .kind = kind,
        .filesystem_error = error,
        .detail = std::move(detail),
    };
}

[[nodiscard]] std::expected<std::vector<std::filesystem::path>,
                            SupportBundleFailure>
ApplicationLogFiles(const SupportBundleRequest &request) {
    std::vector<std::filesystem::path> logs;
    std::error_code error;
    if (!std::filesystem::exists(request.log_directory, error)) {
        if (error) {
            return std::unexpected(
                Failure(request, SupportBundleFailureKind::kReadLogsFailed,
                        error, error.message()));
        }
        return logs;
    }

    std::filesystem::directory_iterator entries{request.log_directory, error};
    if (error) {
        return std::unexpected(
            Failure(request, SupportBundleFailureKind::kReadLogsFailed, error,
                    error.message()));
    }
    for (const auto &entry : entries) {
        const auto status = entry.symlink_status(error);
        if (error) {
            return std::unexpected(
                Failure(request, SupportBundleFailureKind::kReadLogsFailed,
                        error, error.message()));
        }
        if (std::filesystem::is_regular_file(status) &&
            IsApplicationLogFilename(Utf8Filename(entry.path()))) {
            logs.emplace_back(entry.path());
        }
    }
    std::ranges::sort(logs);
    return logs;
}

[[nodiscard]] bool WriteZipEntry(zipFile archive, std::string_view name,
                                 std::span<const std::byte> content) {
    const std::string entry_name{name};
    const auto uses_zip64 =
        content.size() >=
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max());
    if (zipOpenNewFileInZip64(archive, entry_name.c_str(), nullptr, nullptr, 0,
                              nullptr, 0, nullptr, Z_DEFLATED,
                              Z_DEFAULT_COMPRESSION,
                              uses_zip64 ? 1 : 0) != ZIP_OK) {
        return false;
    }

    std::size_t offset = 0;
    while (offset < content.size()) {
        const auto count = (std::min)(kIoBufferSize, content.size() - offset);
        if (zipWriteInFileInZip(archive, content.data() + offset,
                                static_cast<unsigned int>(count)) != ZIP_OK) {
            static_cast<void>(zipCloseFileInZip(archive));
            return false;
        }
        offset += count;
    }
    return zipCloseFileInZip(archive) == ZIP_OK;
}

[[nodiscard]] bool WriteZipEntry(zipFile archive, std::string_view name,
                                 std::string_view content) {
    return WriteZipEntry(
        archive, name,
        std::as_bytes(std::span{content.data(), content.size()}));
}

[[nodiscard]] LogWriteResult WriteLogFile(zipFile archive,
                                          const std::filesystem::path &path) {
    const auto content = storage::ReadLocalFile(path);
    if (!content.has_value()) {
        return LogWriteResult::kReadFailed;
    }

    const auto entry_name = "logs/" + Utf8Filename(path);
    return WriteZipEntry(archive, entry_name, *content)
               ? LogWriteResult::kSuccess
               : LogWriteResult::kArchiveWriteFailed;
}

} // namespace

std::string
FormatDiagnosticEnvironment(const DiagnosticEnvironment &environment) {
    std::string summary =
        "Edit Atlas diagnostic environment\n"
        "\n"
        "Application version: " +
        SafeLine(environment.application_version) +
        "\nOperating system: " + SafeLine(environment.operating_system) +
        "\nArchitecture: " + SafeLine(environment.architecture) +
        "\nQt version: " + SafeLine(environment.qt_version) +
        "\nQt platform plugin: " + SafeLine(environment.platform_plugin) +
        "\nRegistered import formats: " +
        JoinValues(environment.importer_formats) +
        "\nRegistered export formats: " +
        JoinValues(environment.exporter_formats) +
        "\n\n"
        "Privacy: This bundle contains this environment summary and recent "
        "Edit Atlas application logs only. It does not automatically include "
        "timelines, spreadsheets, media, environment variables, or secrets.\n";
    return summary;
}

CreateSupportBundleResult CreateSupportBundle(SupportBundleRequest request) {
    auto logs = ApplicationLogFiles(request);
    if (!logs.has_value()) {
        return std::unexpected(std::move(logs.error()));
    }
    const auto existing_file_policy =
        request.replace_existing ? storage::ExistingFilePolicy::kReplace
                                 : storage::ExistingFilePolicy::kPreserve;
    auto transaction =
        storage::PrepareAtomicFile(request.path, existing_file_policy);
    if (!transaction.has_value()) {
        const auto kind =
            transaction.error().kind ==
                    storage::LocalFileFailureKind::kDestinationExists
                ? SupportBundleFailureKind::kDestinationExists
                : SupportBundleFailureKind::kWriteBundleFailed;
        return std::unexpected(
            Failure(request, kind, transaction.error().filesystem_error,
                    transaction.error().filesystem_error.message()));
    }
    auto zip_path = (*transaction)->temporary_path();
    auto file_functions = ZipFileFunctions(zip_path);
    auto *archive =
        zipOpen2_64(&zip_path, APPEND_STATUS_CREATE, nullptr, &file_functions);
    if (archive == nullptr) {
        return std::unexpected(
            Failure(request, SupportBundleFailureKind::kWriteBundleFailed, {},
                    "The ZIP archive could not be opened."));
    }

    std::optional<SupportBundleFailureKind> write_failure;
    if (!WriteZipEntry(archive, "environment.txt",
                       FormatDiagnosticEnvironment(request.environment))) {
        write_failure = SupportBundleFailureKind::kWriteBundleFailed;
    }
    for (const auto &log : *logs) {
        if (write_failure.has_value()) {
            break;
        }
        switch (WriteLogFile(archive, log)) {
        case LogWriteResult::kSuccess:
            break;
        case LogWriteResult::kReadFailed:
            write_failure = SupportBundleFailureKind::kReadLogsFailed;
            break;
        case LogWriteResult::kArchiveWriteFailed:
            write_failure = SupportBundleFailureKind::kWriteBundleFailed;
            break;
        }
    }
    const auto close_result = zipClose(archive, nullptr);
    if (write_failure.has_value() || close_result != ZIP_OK) {
        const auto kind = write_failure.value_or(
            SupportBundleFailureKind::kWriteBundleFailed);
        const auto detail = kind == SupportBundleFailureKind::kReadLogsFailed
                                ? "An application log could not be read."
                                : "The ZIP archive could not be written.";
        return std::unexpected(Failure(request, kind, {}, detail));
    }

    const auto commit_result = (*transaction)->Commit();
    if (!commit_result.has_value()) {
        const auto kind =
            commit_result.error().kind ==
                    storage::LocalFileFailureKind::kDestinationExists
                ? SupportBundleFailureKind::kDestinationExists
                : SupportBundleFailureKind::kCommitFailed;
        return std::unexpected(
            Failure(request, kind, commit_result.error().filesystem_error,
                    commit_result.error().filesystem_error.message()));
    }

    return SupportBundleReceipt{
        .path = std::move(request.path),
        .log_file_count = logs->size(),
    };
}

} // namespace edit_atlas::support
