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

#include "edit_atlas/frontends/quick/application_information_view_model.hpp"

#include "QString"
#include "QStringList"
#include "gtest/gtest.h"

#include "edit_atlas/services/built_in_formats.hpp"

namespace edit_atlas::frontends::quick {
namespace {

TEST(ApplicationInformationViewModelTest,
     ExposesRuntimeFormatsDiagnosticsAndVideoBackend) {
  auto registry = services::CreateBuiltInFormatRegistry().value();
  ApplicationInformationViewModel information{registry};

  EXPECT_FALSE(information.OperatingSystem().isEmpty());
  EXPECT_FALSE(information.Architecture().isEmpty());
  EXPECT_FALSE(information.QtVersion().isEmpty());
  EXPECT_EQ(information.VideoBackendName(), QStringLiteral("FFmpeg"));
  EXPECT_FALSE(information.VideoBackendVersion().isEmpty());
  EXPECT_FALSE(information.VideoBackendLicense().isEmpty());
  EXPECT_FALSE(information.VideoBackendConfiguration().isEmpty());
  EXPECT_EQ(information.ImportFormats(),
            QStringList{QStringLiteral("cmx-3600")});
  EXPECT_EQ(information.ExportFormats(), QStringList{QStringLiteral("xlsx")});
  EXPECT_FALSE(information.LogDirectory().isEmpty());
}

}  // namespace
}  // namespace edit_atlas::frontends::quick
