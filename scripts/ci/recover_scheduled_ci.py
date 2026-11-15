"""Classify scheduled CI failures and request at most one complete rerun."""

from __future__ import annotations

import json
import os
import subprocess
from pathlib import Path
from urllib.parse import urlparse

RUNNER_FAILURE = "The job was not acquired by Runner of type hosted"
EVIDENCE_PREFIX = "EDIT_ATLAS_CI_FAILURE "
FAILURES = {"failure", "cancelled", "timed_out", "action_required", "stale"}


def eligible(run: dict, repository: str, master_sha: str) -> bool:
    """Reject other events, obsolete commits, and repeat attempts."""
    return (
        run.get("repository", {}).get("full_name") == repository
        and run.get("head_repository", {}).get("full_name") == repository
        and run.get("path") == ".github/workflows/ci.yml"
        and run.get("event") == "schedule"
        and run.get("head_branch") == "master"
        and run.get("head_sha") == master_sha
        and run.get("run_attempt") == 1
        and run.get("status") == "completed"
        # A manually cancelled run must never be restarted automatically.
        and run.get("conclusion") == "failure"
    )


def step_evidence(step: dict, log: str) -> str | None:
    """Read the final structured record belonging to this failed step."""
    category = None
    for line in log.splitlines():
        fields = line.split("\t", 2)
        if len(fields) != 3:
            continue
        label, output = fields[1:]
        timestamp, separator, message = output.partition(" ")
        if not separator:
            continue
        if label != step["name"]:
            if label != "UNKNOWN STEP":
                continue
            start = step.get("started_at", "")[:19]
            end = step.get("completed_at", "")[:19]
            if not start or not end or not start <= timestamp[:19] <= end:
                continue
        if not message.startswith(EVIDENCE_PREFIX):
            continue
        category = None
        try:
            record = json.loads(message[len(EVIDENCE_PREFIX) :])
        except json.JSONDecodeError:
            continue
        if isinstance(record, dict) and record.get("version") == 1:
            category = record.get("category")
    return category


def classify_job(job: dict, annotations: list[dict], log: str) -> str | None:
    """Require explicit evidence for every failed step, regardless of name."""
    steps = job.get("steps", [])
    if not steps and any(
        RUNNER_FAILURE in annotation.get("message", "")
        for annotation in annotations
    ):
        return "hosted runner unavailable"
    failed = [step for step in steps if step.get("conclusion") in FAILURES]
    if not failed:
        return None
    categories = set()
    for step in failed:
        if step.get("conclusion") != "failure":
            return None
        category = step_evidence(step, log)
        if category not in ("transient-network", "dependency-failure"):
            return None
        categories.add(category)
    if categories == {"dependency-failure"}:
        return "dependency failure"
    return "setup or artifact network failure"


def decide(jobs: list[dict], evidence: dict) -> tuple[bool, list[str]]:
    """Require infrastructure evidence for every originating failed job."""
    reasons = []
    for job in jobs:
        if job.get("conclusion") in ("success", "skipped"):
            continue
        annotations, log = evidence.get(job["id"], ([], ""))
        reason = classify_job(job, annotations, log)
        if reason is None:
            return False, [f"Unclassified failure: {job['name']}"]
        if reason != "dependency failure":
            reasons.append(f"{job['name']}: {reason}")
    return bool(reasons), reasons


def gh(*arguments: str) -> str:
    """Call GitHub CLI without a shell; API failures fail recovery closed."""
    return subprocess.run(
        ["gh", *arguments], check=True, capture_output=True, text=True
    ).stdout


def api(path: str) -> dict:
    return json.loads(gh("api", path))


def pages(path: str, key: str | None = None) -> list:
    result = json.loads(gh("api", "--paginate", "--slurp", path))
    return [item for page in result for item in (page[key] if key else page)]


def summary(lines: list[str]) -> None:
    text = "\n".join(lines) + "\n"
    print(text)
    with Path(os.environ["GITHUB_STEP_SUMMARY"]).open("a") as stream:
        stream.write(text)


def recover() -> None:
    repository = os.environ["GITHUB_REPOSITORY"]
    event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text())
    run_id = int(event["workflow_run"]["id"])
    base = f"repos/{repository}"
    run_path = f"{base}/actions/runs/{run_id}"
    run = api(run_path)
    heading = [
        "## Scheduled CI recovery",
        f"Run: {run['html_url']}; attempt {run['run_attempt']}; "
        f"result: {run['conclusion']}",
    ]
    master_sha = api(f"{base}/git/ref/heads/master")["object"]["sha"]
    if not eligible(run, repository, master_sha):
        summary(heading + ["No automatic retry: run is not eligible."])
        return
    jobs = pages(f"{run_path}/attempts/1/jobs?per_page=100", "jobs")
    evidence = {}
    for job in jobs:
        if job.get("conclusion") not in FAILURES:
            continue
        check_id = int(urlparse(job["check_run_url"]).path.rsplit("/", 1)[1])
        annotations = pages(
            f"{base}/check-runs/{check_id}/annotations?per_page=100"
        )
        log = ""
        if job.get("steps"):
            log = gh(
                "run",
                "view",
                str(run_id),
                "--repo",
                repository,
                "--attempt",
                "1",
                "--job",
                str(job["id"]),
                "--log",
            )
        evidence[job["id"]] = annotations, log
    retry, reasons = decide(jobs, evidence)
    summary(heading + reasons)
    if not retry:
        summary(["No automatic retry: validation remains blocking."])
        return
    # The workflow serializes by original run ID. Re-read state immediately
    # before mutation to reject duplicate events or a concurrent manual run.
    run = api(run_path)
    master_sha = api(f"{base}/git/ref/heads/master")["object"]["sha"]
    if not eligible(run, repository, master_sha):
        summary(["No automatic retry: run or master changed during review."])
        return
    gh("api", "--method", "POST", f"{run_path}/rerun")
    summary(
        [
            "Requested one complete rerun; all validation and artifacts will "
            "be recreated. Original evidence remains in attempt 1.",
        ]
    )


if __name__ == "__main__":
    recover()
