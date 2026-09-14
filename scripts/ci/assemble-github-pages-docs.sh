#!/usr/bin/env bash

set -euo pipefail

# Assemble current and versioned API documentation for GitHub Pages.

# Remove the temporary copy after current documentation is assembled.
# Globals: preserved_versions (local to the active main invocation).
cleanup() {
  rm -rf "$preserved_versions"
}

# Assemble the site while retaining published release directories.
# Arguments: generated directory, site directory, optional release version.
# Outputs: validation errors to stderr; HTML files below the site directory.
# Returns: 2 for invalid arguments, 1 for invalid inputs.
main() {
  local generated_directory
  local preserved_versions
  local script_directory
  local site_directory
  local unpublished_version
  local unpublished_versions_file
  local version
  local version_directories
  local version_directory
  local version_name
  local versions_page

  if (( $# < 2 || $# > 3 )); then
    echo "Usage: $0 <generated-directory> <site-directory> [version]" >&2
    exit 2
  fi

  generated_directory=$1
  site_directory=$2
  version=${3:-}

  if [[ ! -d "$generated_directory" ]]; then
    printf 'Generated documentation directory does not exist: %s\n' \
      "$generated_directory" >&2
    exit 1
  fi

  if [[ -n "$version" && ! "$version" =~ ^v[0-9]+ ]]; then
    printf '%s%s\n' \
      'Release documentation version must start with v followed by a number: ' \
      "$version" >&2
    exit 1
  fi

  mkdir -p "$site_directory"

  if [[ -z "$version" ]]; then
    preserved_versions=$(mktemp -d)
    trap cleanup EXIT

    shopt -s nullglob
    for version_directory in "$site_directory"/v[0-9]*; do
      cp -a "$version_directory" "$preserved_versions/"
    done
    shopt -u nullglob

    rm -rf "$site_directory/latest"
    mkdir -p "$site_directory/latest"
    cp -a "$generated_directory"/. "$site_directory/latest"/
    cp -a "$preserved_versions"/. "$site_directory"/ 2>/dev/null || true
  else
    version_directory="$site_directory/$version"
    rm -rf "$version_directory"
    mkdir -p "$version_directory"
    cp -a "$generated_directory"/. "$version_directory"/
  fi

  script_directory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
  unpublished_versions_file="$script_directory/../../docs/unpublished-versions.txt"
  while IFS= read -r unpublished_version || [[ -n "$unpublished_version" ]]; do
    if [[ -z "$unpublished_version" || "$unpublished_version" == \#* ]]; then
      continue
    fi
    if [[ ! "$unpublished_version" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
      echo \
        "Invalid unpublished documentation version: $unpublished_version" \
        >&2
      exit 1
    fi
    rm -rf -- "${site_directory:?}/$unpublished_version"
  done < "$unpublished_versions_file"

  versions_page="$site_directory/versions.html"
  {
    cat <<'EOF'
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Edit Atlas API Documentation</title>
  </head>
  <body>
    <h1>Edit Atlas API Documentation</h1>
    <p><a href="latest/index.html">Current master documentation</a></p>
    <h2>Release documentation</h2>
    <ul>
EOF

    shopt -s nullglob
    version_directories=("$site_directory"/v[0-9]*)
    shopt -u nullglob
    if ((${#version_directories[@]} == 0)); then
      echo '      <li>No release documentation has been published yet.</li>'
    else
      printf '%s\n' "${version_directories[@]}" \
        | sort -Vr \
        | while IFS= read -r version_directory; do
            version_name="${version_directory##*/}"
            printf '      <li><a href="%s/index.html">%s</a></li>\n' \
              "$version_name" "$version_name"
          done
    fi

    cat <<'EOF'
    </ul>
  </body>
</html>
EOF
  } > "$versions_page"

  cat > "$site_directory/index.html" <<'EOF'
<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Edit Atlas API Documentation</title>
  </head>
  <body>
    <h1>Edit Atlas API Documentation</h1>
    <p><a href="latest/index.html">Current master documentation</a></p>
    <p><a href="versions.html">Release documentation</a></p>
  </body>
</html>
EOF

  # Exit before main unwinds the local state used by EXIT callbacks.
  exit "$?"
}

main "$@"
