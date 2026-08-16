#include <edit_atlas/presentation/diagnostic_model.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>

#include <QString>
#include <QVariant>
#include <Qt>

#include <gtest/gtest.h>

#include <optional>
#include <vector>

namespace edit_atlas::presentation {
namespace {

TEST(DiagnosticModelTest, PresentsLocalizedDiagnosticsAndStableSortValues) {
    const std::vector<core::Diagnostic> diagnostics{
        core::Diagnostic{
            .severity = core::DiagnosticSeverity::kWarning,
            .code = "test.warning",
            .message = "Review this event",
            .location =
                core::SourceLocation{
                    .source = "example.edl",
                    .line = 17,
                    .column = 3,
                },
        },
        core::Diagnostic{
            .severity = core::DiagnosticSeverity::kInfo,
            .code = "test.info",
            .message = "Imported metadata",
            .location = std::nullopt,
        },
    };
    DiagnosticModel model;
    model.SetDiagnostics(diagnostics);

    EXPECT_EQ(model.rowCount(), 2);
    EXPECT_EQ(model.columnCount(), 3);
    EXPECT_EQ(model.headerData(0, Qt::Horizontal).toString(),
              QStringLiteral("Severity"));
    EXPECT_EQ(model.data(model.index(0, 0)).toString(),
              QStringLiteral("Warning"));
    EXPECT_EQ(model.data(model.index(0, 1)).toULongLong(), 17U);
    EXPECT_EQ(model.data(model.index(0, 2)).toString(),
              QStringLiteral("Review this event"));
    EXPECT_EQ(model.data(model.index(0, 2), Qt::ToolTipRole).toString(),
              QStringLiteral("test.warning"));
    EXPECT_EQ(model.data(model.index(0, 0), DiagnosticModel::kSortRole).toInt(),
              static_cast<int>(core::DiagnosticSeverity::kWarning));
    EXPECT_FALSE(model.data(model.index(1, 1)).isValid());

    model.SetDiagnostics({});
    EXPECT_EQ(model.rowCount(), 0);
}

} // namespace
} // namespace edit_atlas::presentation
