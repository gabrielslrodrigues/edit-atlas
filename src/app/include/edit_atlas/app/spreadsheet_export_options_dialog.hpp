#ifndef EDIT_ATLAS_APP_SPREADSHEET_EXPORT_OPTIONS_DIALOG_HPP_
#define EDIT_ATLAS_APP_SPREADSHEET_EXPORT_OPTIONS_DIALOG_HPP_

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/presentation/translation.hpp>

#include <QDialog>
#include <QString>

#include <span>
#include <vector>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QWidget;

namespace edit_atlas::app {

class EventProjectionWidget;

/// Collects XLSX presentation options without exposing widget details to the
/// main application window.
class SpreadsheetExportOptionsDialog final : public QDialog {
    Q_OBJECT

  public:
    explicit SpreadsheetExportOptionsDialog(
        presentation::ApplicationLanguage application_language,
        std::span<const core::TimelineEventField> projection,
        QWidget *parent = nullptr);
    ~SpreadsheetExportOptionsDialog(void) override = default;

    [[nodiscard]] std::vector<core::MetadataEntry> Options(void) const;
    [[nodiscard]] std::vector<core::TimelineEventField>
    EventProjection(void) const;
    /// Returns the selected rendered-video path, or an empty string.
    [[nodiscard]] QString VideoPath(void) const;

    SpreadsheetExportOptionsDialog(const SpreadsheetExportOptionsDialog &) =
        delete;
    SpreadsheetExportOptionsDialog &
    operator=(const SpreadsheetExportOptionsDialog &) = delete;
    SpreadsheetExportOptionsDialog(SpreadsheetExportOptionsDialog &&) = delete;
    SpreadsheetExportOptionsDialog &
    operator=(SpreadsheetExportOptionsDialog &&) = delete;

  private:
    void BrowseForVideo(void);
    void UpdateControls(void);

    QComboBox *workbook_language_ = nullptr;
    QCheckBox *timeline_ = nullptr;
    QCheckBox *diagnostics_ = nullptr;
    EventProjectionWidget *projection_ = nullptr;
    QGroupBox *video_group_ = nullptr;
    QLineEdit *video_path_ = nullptr;
    QPushButton *continue_ = nullptr;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_SPREADSHEET_EXPORT_OPTIONS_DIALOG_HPP_
