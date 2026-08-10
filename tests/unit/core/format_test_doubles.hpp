#ifndef EDIT_ATLAS_TESTS_CORE_FORMAT_TEST_DOUBLES_HPP_
#define EDIT_ATLAS_TESTS_CORE_FORMAT_TEST_DOUBLES_HPP_

#include <edit_atlas/core/format.hpp>

#include <stdexcept>
#include <utility>

namespace edit_atlas::core::test {

class StubImporter final : public Importer {
  public:
    explicit StubImporter(FormatDescriptor descriptor,
                          ProbeConfidence confidence = ProbeConfidence::kNone)
        : descriptor_(std::move(descriptor)), confidence_(confidence) {}

    [[nodiscard]] const FormatDescriptor &
    descriptor(void) const noexcept override {
        return descriptor_;
    }

    [[nodiscard]] ProbeConfidence Probe(const ImportRequest &) const override {
        ++probe_count_;
        if (throw_during_probe_) {
            throw std::runtime_error{"probe failure"};
        }
        return confidence_;
    }

    [[nodiscard]] ImportResult Import(const ImportRequest &) const override {
        ++import_count_;
        if (throw_during_import_) {
            throw std::runtime_error{"import failure"};
        }
        return result_;
    }

    void set_result(ImportResult result) { result_ = std::move(result); }

    void set_throw_during_probe(bool enabled) noexcept {
        throw_during_probe_ = enabled;
    }

    void set_throw_during_import(bool enabled) noexcept {
        throw_during_import_ = enabled;
    }

    [[nodiscard]] int probe_count(void) const noexcept { return probe_count_; }

    [[nodiscard]] int import_count(void) const noexcept {
        return import_count_;
    }

  private:
    FormatDescriptor descriptor_;
    ProbeConfidence confidence_;
    ImportResult result_;
    bool throw_during_probe_ = false;
    bool throw_during_import_ = false;
    mutable int probe_count_ = 0;
    mutable int import_count_ = 0;
};

class StubExporter final : public Exporter {
  public:
    explicit StubExporter(FormatDescriptor descriptor)
        : descriptor_(std::move(descriptor)) {}

    [[nodiscard]] const FormatDescriptor &
    descriptor(void) const noexcept override {
        return descriptor_;
    }

    [[nodiscard]] ExportResult Export(const ExportRequest &) const override {
        ++export_count_;
        if (throw_during_export_) {
            throw std::runtime_error{"export failure"};
        }
        return result_;
    }

    void set_result(ExportResult result) { result_ = std::move(result); }

    void set_throw_during_export(bool enabled) noexcept {
        throw_during_export_ = enabled;
    }

    [[nodiscard]] int export_count(void) const noexcept {
        return export_count_;
    }

  private:
    FormatDescriptor descriptor_;
    ExportResult result_;
    bool throw_during_export_ = false;
    mutable int export_count_ = 0;
};

} // namespace edit_atlas::core::test

#endif // EDIT_ATLAS_TESTS_CORE_FORMAT_TEST_DOUBLES_HPP_
