#!/usr/bin/env bash

set -euo pipefail

# Records the outcome of a scheduled packaged E2E run on a tracking issue, one
# per frontend, so a failure survives until someone acts on it instead of
# resting in a workflow run list nobody opens. A later passing run closes the
# issue it raised. Any number of frontends may be reported on.
#
# Set DRY_RUN=1 to print the actions instead of performing them.

if (( $# != 3 )); then
  echo "Usage: $0 <job-result> <frontends-json> <results-dir>" >&2
  exit 2
fi

result="$1"
frontends_json="$2"
results_dir="$3"
run_url="${GITHUB_SERVER_URL:-https://github.com}/${GITHUB_REPOSITORY:-}"
run_url+="/actions/runs/${GITHUB_RUN_ID:-0}"

mapfile -t frontends < <(
  printf '%s' "$frontends_json" | tr -d '[]"' | tr ',' '\n' | sed '/^$/d'
)

if (( ${#frontends[@]} == 0 )); then
  echo "No frontend to report on."
  exit 0
fi

gh_or_echo() {
  if [[ "${DRY_RUN:-0}" == "1" ]]; then
    printf 'DRY RUN: gh %s\n' "$*"
    return 0
  fi
  gh "$@"
}

find_open_issue() {
  local title="$1"
  if [[ "${DRY_RUN:-0}" == "1" ]]; then
    printf '%s' "${DRY_RUN_ISSUE:-}"
    return 0
  fi
  gh issue list \
    --state open \
    --search "\"$title\" in:title" \
    --json number,title \
    --jq "first(.[] | select(.title == \"$title\") | .number) // empty"
}

# Reports are downloaded for every frontend the run produced, so each is
# attributed by the artifact directory name, which ends with its frontend.
summarize_failures() {
  local directory="$1" frontend="$2"
  [[ -d "$directory" ]] || return 0
  python3 - "$directory" "$frontend" <<'PY'
import pathlib
import sys
import xml.etree.ElementTree as ET

root, frontend = pathlib.Path(sys.argv[1]), sys.argv[2]
reported = False
for report in sorted(root.glob("*/**/junit.xml")):
    artifact = report.relative_to(root).parts[0]
    if not artifact.endswith(f"-{frontend}"):
        continue
    reported = True
    platform = artifact.split("-", 1)[0]
    try:
        cases = list(ET.parse(report).getroot().iter("testcase"))
    except ET.ParseError:
        print(f"- `{platform}`: report could not be parsed")
        continue
    failed = [
        f"{c.get('classname', '')}::{c.get('name', '')}".strip(":")
        for c in cases
        if c.find("failure") is not None or c.find("error") is not None
    ]
    if failed:
        print(f"- `{platform}`: {len(failed)} failing scenario(s)")
        for name in failed:
            print(f"  - `{name}`")
    else:
        print(f"- `{platform}`: no failing scenario recorded in the report")
if not reported:
    print("- no JUnit report was produced")
PY
}

for frontend in "${frontends[@]}"; do
  title="Scheduled $frontend packaged E2E is failing"
  issue_number="$(find_open_issue "$title")"

  case "$result" in
    success)
      if [[ -n "$issue_number" ]]; then
        echo "$frontend passed; closing tracking issue #$issue_number."
        gh_or_echo issue comment "$issue_number" \
          --body "Scheduled \`$frontend\` packaged E2E passed in $run_url. Closing."
        gh_or_echo issue close "$issue_number" --reason completed
      else
        echo "$frontend passed; no tracking issue is open."
      fi
      ;;
    failure)
      failures="$(summarize_failures "$results_dir" "$frontend")"
      body="Scheduled packaged end-to-end testing of the"
      body+=" \`$frontend\` frontend failed."
      body+=$'\n\n'"Run: $run_url"
      body+=$'\n'"Commit: ${GITHUB_SHA:-unknown}"
      body+=$'\n\n'"Failing scenarios by platform:"
      body+=$'\n'"$failures"
      body+=$'\n\n'"This frontend is not the one production ships, so this does"
      body+=" not block the merge path. The issue stays open until a later"
      body+=" scheduled run passes, which closes it automatically."
      if [[ -n "$issue_number" ]]; then
        echo "$frontend failed; updating tracking issue #$issue_number."
        gh_or_echo issue comment "$issue_number" --body "$body"
      else
        echo "$frontend failed; opening a tracking issue."
        gh_or_echo issue create --title "$title" --body "$body"
      fi
      ;;
    *)
      echo "Result for $frontend is '$result'; recording nothing."
      ;;
  esac
done
