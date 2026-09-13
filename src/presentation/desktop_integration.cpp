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

#include "edit_atlas/presentation/desktop_integration.hpp"

#include "QDesktopServices"
#include "QDir"
#include "QFileInfo"
#include "QProcess"
#include "QString"
#include "QUrl"

namespace edit_atlas::presentation::desktop_integration {

bool RevealFile(const QString& path) {
#if defined(Q_OS_WIN)
  return QProcess::startDetached(
      QStringLiteral("explorer.exe"),
      {QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
#elif defined(Q_OS_MACOS)
  return QProcess::startDetached(QStringLiteral("open"),
                                 {QStringLiteral("-R"), path});
#else
  return QDesktopServices::openUrl(
      QUrl::fromLocalFile(QFileInfo{path}.absolutePath()));
#endif
}

bool OpenDirectory(const QString& path) {
  const QDir directory{path};
  return directory.exists() && QDesktopServices::openUrl(QUrl::fromLocalFile(
                                   directory.absolutePath()));
}

bool OpenExternalUrl(const QUrl& url) {
  const auto scheme = url.scheme();
  if (!url.isValid() ||
      (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) {
    return false;
  }
  return QDesktopServices::openUrl(url);
}

}  // namespace edit_atlas::presentation::desktop_integration
