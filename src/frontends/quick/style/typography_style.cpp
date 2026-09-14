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

#include "edit_atlas/frontends/quick/style/typography_style.hpp"

#include "QObject"
#include "QString"

#include "edit_atlas/presentation/typography.hpp"

namespace edit_atlas::frontends::quick::style {

TypographyStyle::TypographyStyle(QObject* parent) : QObject{parent} {}

QString TypographyStyle::Family(void) const {
  return presentation::ResolvedTypographyFamily();
}

int TypographyStyle::BodyPointSize(void) const {
  return presentation::ApplicationTypographyPolicy().body_point_size;
}

int TypographyStyle::HeadingPointSize(void) const {
  return presentation::ApplicationTypographyPolicy().heading_point_size;
}

int TypographyStyle::TitlePointSize(void) const {
  return presentation::ApplicationTypographyPolicy().title_point_size;
}

int TypographyStyle::BodyWeight(void) const {
  return presentation::ApplicationTypographyPolicy().body_weight;
}

int TypographyStyle::HeadingWeight(void) const {
  return presentation::ApplicationTypographyPolicy().heading_weight;
}

int TypographyStyle::TitleWeight(void) const {
  return presentation::ApplicationTypographyPolicy().title_weight;
}

}  // namespace edit_atlas::frontends::quick::style
