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

#include "QByteArray"
#include "QCoreApplication"
#include "QDir"
#include "QString"
#include "QTemporaryDir"
#include "QtGlobal"
#include "gtest/gtest.h"

#include "edit_atlas/presentation/application_state.hpp"

int main(int argc, char* argv[]) {
  QCoreApplication application{argc, argv};
  QCoreApplication::setApplicationName(QStringLiteral("Edit Atlas Unit Tests"));
  QCoreApplication::setOrganizationName(QStringLiteral("Edit Atlas Tests"));

  // Persistent state is redirected below a temporary root so settings are
  // isolated from the developer profile, and so QSettings has a store it
  // can actually write to on every platform.
  QTemporaryDir state_root{
      QDir::temp().filePath(QStringLiteral("edit-atlas-state-XXXXXX"))};
  if (!state_root.isValid()) {
    return 1;
  }
  qputenv(edit_atlas::presentation::kTestStateRootEnvironment,
          state_root.path().toUtf8());
  edit_atlas::presentation::ConfigureApplicationState();

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
