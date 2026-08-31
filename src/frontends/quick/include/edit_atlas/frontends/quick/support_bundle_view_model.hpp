#ifndef EDIT_ATLAS_FRONTENDS_QUICK_SUPPORT_BUNDLE_VIEW_MODEL_HPP_
#define EDIT_ATLAS_FRONTENDS_QUICK_SUPPORT_BUNDLE_VIEW_MODEL_HPP_

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/presentation/support_bundle_view_model.hpp>

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <QtQmlIntegration/qqmlintegration.h>

namespace edit_atlas::frontends::quick {

/// Adapts diagnostic support-bundle state and commands for Qt Quick.
class SupportBundleViewModel final : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(SupportBundle)
    QML_UNCREATABLE("Provided by the Edit Atlas application")

    Q_PROPERTY(bool busy READ IsBusy NOTIFY busyChanged)
    Q_PROPERTY(QUrl suggestedDestinationUrl READ SuggestedDestinationUrl NOTIFY
                   suggestedDestinationChanged)
    Q_PROPERTY(QString resultPath READ ResultPath NOTIFY resultChanged)
    Q_PROPERTY(qulonglong includedLogFileCount READ IncludedLogFileCount NOTIFY
                   resultChanged)
    Q_PROPERTY(QString errorText READ ErrorText NOTIFY resultChanged)

  public:
    /// Creates a support-bundle adapter for configured logs and diagnostics.
    explicit SupportBundleViewModel(const core::FormatRegistry &registry,
                                    QObject *parent = nullptr);
    /// Destroys the adapter after the shared workflow has stopped.
    ~SupportBundleViewModel(void) override = default;

    /// Support-bundle adapters are non-copyable QObject owners.
    SupportBundleViewModel(const SupportBundleViewModel &) = delete;
    /// Support-bundle adapters are non-copy-assignable QObject owners.
    SupportBundleViewModel &operator=(const SupportBundleViewModel &) = delete;
    /// Support-bundle adapters are non-movable QObject owners.
    SupportBundleViewModel(SupportBundleViewModel &&) = delete;
    /// Support-bundle adapters are non-move-assignable QObject owners.
    SupportBundleViewModel &operator=(SupportBundleViewModel &&) = delete;

    /// Returns whether support-bundle creation is running.
    [[nodiscard]] bool IsBusy(void) const noexcept;
    /// Returns the preferred ZIP destination.
    [[nodiscard]] QUrl SuggestedDestinationUrl(void) const;
    /// Returns the completed support-bundle path.
    [[nodiscard]] QString ResultPath(void) const;
    /// Returns the number of log files included in the completed bundle.
    [[nodiscard]] qulonglong IncludedLogFileCount(void) const noexcept;
    /// Returns localized details for the latest failure.
    [[nodiscard]] QString ErrorText(void) const;

    /// Returns whether the selected local destination already exists.
    Q_INVOKABLE bool DestinationExists(const QUrl &destination) const;
    /// Starts support-bundle creation at a frontend-confirmed destination.
    Q_INVOKABLE bool Start(const QUrl &destination, bool replace_existing);
    /// Opens the completed bundle's containing directory.
    Q_INVOKABLE bool RevealResult(void) const;
    /// Clears the latest result before a new export interaction.
    Q_INVOKABLE void ClearResult(void);

  signals:
    /// Reports that bundle creation started or stopped.
    void busyChanged(void);
    /// Reports changed result presentation.
    void resultChanged(void);
    /// Reports a successfully completed support bundle.
    void exportSucceeded(void);
    /// Reports a failed support-bundle operation.
    void exportFailed(void);
    /// Reports a changed suggested destination.
    void suggestedDestinationChanged(void);

  private:
    void HandleFinished(void);
    void SetImmediateFailure(QString error);

    presentation::SupportBundleViewModel view_model_;
    QString result_path_;
    QString error_text_;
    qulonglong included_log_file_count_ = 0;
};

} // namespace edit_atlas::frontends::quick

#endif // EDIT_ATLAS_FRONTENDS_QUICK_SUPPORT_BUNDLE_VIEW_MODEL_HPP_
