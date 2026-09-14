// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "edit_atlas/presentation/diagnostic_model.hpp"

#include <optional>
#include <vector>

#include "QString"
#include "QVariant"
#include "Qt"
#include "gtest/gtest.h"

#include "edit_atlas/core/editorial_timeline.hpp"

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

}  // namespace
}  // namespace edit_atlas::presentation
