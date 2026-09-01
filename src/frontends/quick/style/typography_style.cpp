#include <edit_atlas/frontends/quick/style/typography_style.hpp>

#include <edit_atlas/presentation/typography.hpp>

#include <QObject>
#include <QString>

namespace edit_atlas::frontends::quick::style {

TypographyStyle::TypographyStyle(QObject *parent) : QObject{parent} {}

QString TypographyStyle::Family(void) const {
    return presentation::ResolvedTypographyFamily();
}

int TypographyStyle::BodyPointSize(void) const {
    return presentation::ApplicationTypographyPolicy().bodyPointSize;
}

int TypographyStyle::HeadingPointSize(void) const {
    return presentation::ApplicationTypographyPolicy().headingPointSize;
}

int TypographyStyle::TitlePointSize(void) const {
    return presentation::ApplicationTypographyPolicy().titlePointSize;
}

int TypographyStyle::BodyWeight(void) const {
    return presentation::ApplicationTypographyPolicy().bodyWeight;
}

int TypographyStyle::HeadingWeight(void) const {
    return presentation::ApplicationTypographyPolicy().headingWeight;
}

int TypographyStyle::TitleWeight(void) const {
    return presentation::ApplicationTypographyPolicy().titleWeight;
}

} // namespace edit_atlas::frontends::quick::style
