#ifndef EDIT_ATLAS_STORAGE_LOCAL_FILE_HPP_
#define EDIT_ATLAS_STORAGE_LOCAL_FILE_HPP_

#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <system_error>
#include <vector>

namespace edit_atlas::storage {

/// Selects how an atomic write treats an existing destination.
enum class ExistingFilePolicy {
    /// Fails without changing an existing destination.
    kPreserve,
    /// Atomically replaces an existing destination.
    kReplace,
};

/// Identifies the stage at which a local-file operation failed.
enum class LocalFileFailureKind {
    /// The source file could not be opened.
    kOpenFailed,
    /// The source file could not be read completely.
    kReadFailed,
    /// The source or output is too large for the platform stream API.
    kTooLarge,
    /// No safe temporary sibling path could be prepared.
    kTemporaryPathFailed,
    /// The destination exists and preservation was requested.
    kDestinationExists,
    /// The output could not be written to its temporary sibling.
    kWriteFailed,
    /// The completed temporary file could not be committed.
    kCommitFailed,
};

/// Describes a presentation-neutral local-file failure.
struct LocalFileFailure final {
    /// Stage at which the operation failed.
    LocalFileFailureKind kind;
    /// Source, destination, or temporary path associated with the failure.
    std::filesystem::path path;
    /// Native filesystem or stream error.
    std::error_code filesystem_error;
};

/// Complete local-file bytes or a structured failure.
using ReadLocalFileResult =
    std::expected<std::vector<std::byte>, LocalFileFailure>;

/// Successful atomic commit or a structured failure.
using WriteLocalFileResult = std::expected<void, LocalFileFailure>;

class AtomicFileTransaction;

/// Prepared atomic-file transaction or a structured failure.
using PrepareAtomicFileResult =
    std::expected<std::unique_ptr<AtomicFileTransaction>, LocalFileFailure>;

/// Owns one temporary sibling until it is committed or cleaned up.
class AtomicFileTransaction final {
  public:
    ~AtomicFileTransaction(void);

    AtomicFileTransaction(const AtomicFileTransaction &) = delete;
    AtomicFileTransaction &operator=(const AtomicFileTransaction &) = delete;
    AtomicFileTransaction(AtomicFileTransaction &&) = delete;
    AtomicFileTransaction &operator=(AtomicFileTransaction &&) = delete;

    /// Commits the temporary sibling according to the prepared policy.
    [[nodiscard]] WriteLocalFileResult Commit(void);

    /// Returns the temporary sibling path to be populated by the caller.
    [[nodiscard]] const std::filesystem::path &
    temporary_path(void) const noexcept;

  private:
    friend PrepareAtomicFileResult
    PrepareAtomicFile(const std::filesystem::path &, ExistingFilePolicy);

    AtomicFileTransaction(std::filesystem::path destination,
                          std::filesystem::path temporary,
                          ExistingFilePolicy existing_file_policy);

    std::filesystem::path destination_;
    std::filesystem::path temporary_;
    ExistingFilePolicy existing_file_policy_;
};

/// Reads all bytes from p path after checking platform size limits.
[[nodiscard]] ReadLocalFileResult
ReadLocalFile(const std::filesystem::path &path);

/// Reserves a temporary sibling for caller-controlled output generation.
[[nodiscard]] PrepareAtomicFileResult
PrepareAtomicFile(const std::filesystem::path &path,
                  ExistingFilePolicy existing_file_policy);

/// Writes p content to a temporary sibling and atomically commits it.
[[nodiscard]] WriteLocalFileResult
WriteLocalFileAtomically(const std::filesystem::path &path,
                         std::span<const std::byte> content,
                         ExistingFilePolicy existing_file_policy);

} // namespace edit_atlas::storage

#endif // EDIT_ATLAS_STORAGE_LOCAL_FILE_HPP_
