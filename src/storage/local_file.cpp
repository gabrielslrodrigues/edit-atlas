#include <edit_atlas/storage/local_file.hpp>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace edit_atlas::storage {
namespace {

constexpr std::size_t kMaximumTemporaryPathAttempts = 16;

[[nodiscard]] std::error_code IoError(void) {
    return errno == 0 ? std::make_error_code(std::errc::io_error)
                      : std::error_code{errno, std::generic_category()};
}

[[nodiscard]] LocalFileFailure Failure(LocalFileFailureKind kind,
                                       const std::filesystem::path &path,
                                       std::error_code error) {
    if (!error) {
        error = std::make_error_code(std::errc::io_error);
    }
    return LocalFileFailure{
        .kind = kind,
        .path = path,
        .filesystem_error = error,
    };
}

[[nodiscard]] std::filesystem::path
TemporaryPathFor(const std::filesystem::path &destination,
                 std::uint64_t suffix) {
    auto filename = destination.filename();
    filename += ".edit-atlas-";
    filename += std::to_string(suffix);
    filename += ".tmp";
    return destination.parent_path() / filename;
}

[[nodiscard]] std::expected<std::filesystem::path, std::error_code>
AvailableTemporaryPath(const std::filesystem::path &destination) {
    std::random_device random;
    for (std::size_t attempt = 0; attempt < kMaximumTemporaryPathAttempts;
         ++attempt) {
        const auto suffix = (static_cast<std::uint64_t>(random()) << 32U) |
                            static_cast<std::uint64_t>(random());
        auto candidate = TemporaryPathFor(destination, suffix);
        std::error_code error;
        const auto exists = std::filesystem::exists(candidate, error);
        if (error) {
            return std::unexpected(error);
        }
        if (!exists) {
            return candidate;
        }
    }
    return std::unexpected(std::make_error_code(std::errc::file_exists));
}

[[nodiscard]] std::error_code
CommitTemporaryFile(const std::filesystem::path &temporary,
                    const std::filesystem::path &destination,
                    ExistingFilePolicy existing_file_policy) {
#if defined(_WIN32)
    DWORD flags = MOVEFILE_WRITE_THROUGH;
    if (existing_file_policy == ExistingFilePolicy::kReplace) {
        flags |= MOVEFILE_REPLACE_EXISTING;
    }
    if (MoveFileExW(temporary.c_str(), destination.c_str(), flags) != 0) {
        return {};
    }
    return {static_cast<int>(GetLastError()), std::system_category()};
#else
    std::error_code error;
    if (existing_file_policy == ExistingFilePolicy::kReplace) {
        std::filesystem::rename(temporary, destination, error);
        return error;
    }
    std::filesystem::create_hard_link(temporary, destination, error);
    if (error) {
        return error;
    }
    static_cast<void>(std::filesystem::remove(temporary, error));
    return {};
#endif
}

} // namespace

AtomicFileTransaction::AtomicFileTransaction(
    std::filesystem::path destination, std::filesystem::path temporary,
    ExistingFilePolicy existing_file_policy)
    : destination_(std::move(destination)), temporary_(std::move(temporary)),
      existing_file_policy_(existing_file_policy) {}

AtomicFileTransaction::~AtomicFileTransaction(void) {
    std::error_code error;
    static_cast<void>(std::filesystem::remove(temporary_, error));
}

WriteLocalFileResult AtomicFileTransaction::Commit(void) {
    const auto error =
        CommitTemporaryFile(temporary_, destination_, existing_file_policy_);
    if (error) {
        const auto kind =
            existing_file_policy_ == ExistingFilePolicy::kPreserve &&
                    error == std::errc::file_exists
                ? LocalFileFailureKind::kDestinationExists
                : LocalFileFailureKind::kCommitFailed;
        return std::unexpected(Failure(kind, destination_, error));
    }
    temporary_.clear();
    return {};
}

const std::filesystem::path &
AtomicFileTransaction::temporary_path(void) const noexcept {
    return temporary_;
}

ReadLocalFileResult ReadLocalFile(const std::filesystem::path &path) {
    errno = 0;
    std::ifstream input{path, std::ios::binary};
    if (!input.is_open()) {
        return std::unexpected(
            Failure(LocalFileFailureKind::kOpenFailed, path, IoError()));
    }

    std::error_code error;
    const auto file_size = std::filesystem::file_size(path, error);
    if (error) {
        return std::unexpected(
            Failure(LocalFileFailureKind::kReadFailed, path, error));
    }
    if (file_size > std::numeric_limits<std::size_t>::max() ||
        file_size > static_cast<std::uintmax_t>(
                        std::numeric_limits<std::streamsize>::max())) {
        return std::unexpected(
            Failure(LocalFileFailureKind::kTooLarge, path,
                    std::make_error_code(std::errc::value_too_large)));
    }

    std::vector<std::byte> content(static_cast<std::size_t>(file_size));
    if (!content.empty()) {
        errno = 0;
        input.read(reinterpret_cast<char *>(content.data()),
                   static_cast<std::streamsize>(content.size()));
        if (!input) {
            return std::unexpected(
                Failure(LocalFileFailureKind::kReadFailed, path, IoError()));
        }
    }
    return content;
}

PrepareAtomicFileResult
PrepareAtomicFile(const std::filesystem::path &path,
                  ExistingFilePolicy existing_file_policy) {
    std::error_code error;
    const auto destination_exists = std::filesystem::exists(path, error);
    if (error) {
        return std::unexpected(
            Failure(LocalFileFailureKind::kWriteFailed, path, error));
    }
    if (destination_exists &&
        existing_file_policy == ExistingFilePolicy::kPreserve) {
        return std::unexpected(
            Failure(LocalFileFailureKind::kDestinationExists, path,
                    std::make_error_code(std::errc::file_exists)));
    }
    auto temporary_path = AvailableTemporaryPath(path);
    if (!temporary_path.has_value()) {
        return std::unexpected(
            Failure(LocalFileFailureKind::kTemporaryPathFailed, path,
                    temporary_path.error()));
    }
    return std::unique_ptr<AtomicFileTransaction>{new AtomicFileTransaction{
        path, std::move(*temporary_path), existing_file_policy}};
}

WriteLocalFileResult
WriteLocalFileAtomically(const std::filesystem::path &path,
                         std::span<const std::byte> content,
                         ExistingFilePolicy existing_file_policy) {
    if (content.size() >
        static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        return std::unexpected(
            Failure(LocalFileFailureKind::kTooLarge, path,
                    std::make_error_code(std::errc::value_too_large)));
    }

    auto transaction = PrepareAtomicFile(path, existing_file_policy);
    if (!transaction.has_value()) {
        return std::unexpected(std::move(transaction.error()));
    }
    errno = 0;
    std::ofstream output{
        (*transaction)->temporary_path(),
        std::ios::binary | std::ios::out | std::ios::trunc,
    };
    if (!output.is_open()) {
        return std::unexpected(Failure(LocalFileFailureKind::kWriteFailed,
                                       (*transaction)->temporary_path(),
                                       IoError()));
    }
    output.write(reinterpret_cast<const char *>(content.data()),
                 static_cast<std::streamsize>(content.size()));
    output.close();
    if (!output) {
        return std::unexpected(Failure(LocalFileFailureKind::kWriteFailed,
                                       (*transaction)->temporary_path(),
                                       IoError()));
    }

    return (*transaction)->Commit();
}

} // namespace edit_atlas::storage
