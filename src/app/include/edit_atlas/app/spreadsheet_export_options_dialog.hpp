#ifndef EDIT_ATLAS_APP_SPREADSHEET_EXPORT_OPTIONS_DIALOG_HPP_
#define EDIT_ATLAS_APP_SPREADSHEET_EXPORT_OPTIONS_DIALOG_HPP_

#include <edit_atlas/app/translation.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <QDialog>

#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QWidget;

namespace edit_atlas::app {

/// Collects XLSX presentation options without exposing widget details to the
/// main application window.
class SpreadsheetExportOptionsDialog final : public QDialog {
    Q_OBJECT

  public:
    explicit SpreadsheetExportOptionsDialog(
        ApplicationLanguage application_language, QWidget *parent = nullptr);
    ~SpreadsheetExportOptionsDialog(void) override = default;

    [[nodiscard]] std::vector<core::MetadataEntry> Options(void) const;
    [[nodiscard]] std::vector<core::TimelineEventField>
    EventProjection(void) const;

    SpreadsheetExportOptionsDialog(const SpreadsheetExportOptionsDialog &) =
        delete;
    SpreadsheetExportOptionsDialog &
    operator=(const SpreadsheetExportOptionsDialog &) = delete;
    SpreadsheetExportOptionsDialog(SpreadsheetExportOptionsDialog &&) = delete;
    SpreadsheetExportOptionsDialog &
    operator=(SpreadsheetExportOptionsDialog &&) = delete;

  private:
    void MoveCurrentColumn(int offset);
    void UpdateColumnControls(void);

    QComboBox *workbook_language_ = nullptr;
    QCheckBox *timeline_ = nullptr;
    QCheckBox *diagnostics_ = nullptr;
    QListWidget *event_columns_ = nullptr;
    QPushButton *move_up_ = nullptr;
    QPushButton *move_down_ = nullptr;
    QPushButton *continue_ = nullptr;
    QLabel *column_error_ = nullptr;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_SPREADSHEET_EXPORT_OPTIONS_DIALOG_HPP_
