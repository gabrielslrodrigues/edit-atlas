#include <edit_atlas/presentation/diagnostic_model.hpp>

#include <edit_atlas/presentation/diagnostic_text.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QString>
#include <QVariant>
#include <Qt>
#include <QtGlobal>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>
#include <string>

namespace edit_atlas::presentation {
namespace {

enum Column {
    kSeverity,
    kLine,
    kMessage,
    kColumnCount,
};

[[nodiscard]] QString Utf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] QString SeverityText(core::DiagnosticSeverity severity) {
    switch (severity) {
    case core::DiagnosticSeverity::kInfo:
        return DiagnosticModel::tr("Info");
    case core::DiagnosticSeverity::kWarning:
        return DiagnosticModel::tr("Warning");
    case core::DiagnosticSeverity::kError:
        return DiagnosticModel::tr("Error");
    }
    return {};
}

[[nodiscard]] QVariant LineData(const core::Diagnostic &diagnostic) {
    return diagnostic.location.has_value() && diagnostic.location->line != 0
               ? QVariant::fromValue(
                     static_cast<qulonglong>(diagnostic.location->line))
               : QVariant{};
}

} // namespace

DiagnosticModel::DiagnosticModel(QObject *parent)
    : QAbstractTableModel{parent} {}

int DiagnosticModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    const auto maximum =
        static_cast<std::size_t>(std::numeric_limits<int>::max());
    return static_cast<int>(std::min(diagnostics_.size(), maximum));
}

int DiagnosticModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : kColumnCount;
}

QVariant DiagnosticModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount() ||
        index.column() < 0 || index.column() >= kColumnCount) {
        return {};
    }

    const auto &diagnostic =
        diagnostics_[static_cast<std::size_t>(index.row())];
    if (role == Qt::ToolTipRole) {
        return Utf8(diagnostic.code);
    }
    if (role == kSortRole) {
        switch (index.column()) {
        case kSeverity:
            return static_cast<int>(diagnostic.severity);
        case kLine:
            return LineData(diagnostic);
        case kMessage:
            return Utf8(diagnostic.message);
        default:
            return {};
        }
    }
    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (index.column()) {
    case kSeverity:
        return SeverityText(diagnostic.severity);
    case kLine:
        return LineData(diagnostic);
    case kMessage:
        return diagnostic_text::Message(diagnostic);
    default:
        return {};
    }
}

QVariant DiagnosticModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
    case kSeverity:
        return tr("Severity");
    case kLine:
        return tr("Line");
    case kMessage:
        return tr("Message");
    default:
        return {};
    }
}

void DiagnosticModel::SetDiagnostics(
    std::span<const core::Diagnostic> diagnostics) {
    beginResetModel();
    diagnostics_ = {diagnostics.begin(), diagnostics.end()};
    endResetModel();
}

void DiagnosticModel::Retranslate(void) {
    emit headerDataChanged(Qt::Horizontal, 0, kColumnCount - 1);
    if (rowCount() != 0) {
        emit dataChanged(index(0, 0),
                         index(rowCount() - 1, kColumnCount - 1));
    }
}

} // namespace edit_atlas::presentation
