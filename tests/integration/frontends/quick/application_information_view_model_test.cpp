#include <edit_atlas/frontends/quick/application_information_view_model.hpp>

#include <edit_atlas/services/built_in_formats.hpp>

#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

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

} // namespace
} // namespace edit_atlas::frontends::quick
