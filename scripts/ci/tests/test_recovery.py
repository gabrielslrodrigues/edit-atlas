"""Recovery policy regressions using recorded GitHub failure signatures."""

import json
import unittest
from unittest.mock import patch

from scripts.ci import recover_scheduled_ci as recovery


class RecoveryTest(unittest.TestCase):
    def setUp(self):
        self.workflow_run = {
            "repository": {"full_name": "owner/project"},
            "head_repository": {"full_name": "owner/project"},
            "path": ".github/workflows/ci.yml",
            "event": "schedule",
            "head_branch": "master",
            "head_sha": "abc",
            "run_attempt": 1,
            "status": "completed",
            "conclusion": "failure",
            "html_url": "https://example.invalid/run/42",
        }
        self.job = {
            "id": 7,
            "name": "Build and package / Universal / macOS",
            "conclusion": "cancelled",
            "steps": [],
            "check_run_url": "https://api.github.com/check-runs/7",
        }
        self.annotations = [
            {
                "message": (
                    "The job was not acquired by Runner of type hosted "
                    "even after multiple attempts"
                ),
            }
        ]

    def test_only_current_first_scheduled_failure_is_eligible(self):
        self.assertTrue(
            recovery.eligible(self.workflow_run, "owner/project", "abc")
        )
        for key, value in [
            ("event", "pull_request"),
            ("event", "workflow_dispatch"),
            ("head_sha", "old"),
            ("run_attempt", 2),
            ("conclusion", "cancelled"),
            ("head_branch", "feature"),
            ("path", ".github/workflows/release.yml"),
            ("head_repository", {"full_name": "fork/project"}),
        ]:
            with self.subTest(key=key, value=value):
                changed = self.workflow_run | {key: value}
                self.assertFalse(
                    recovery.eligible(changed, "owner/project", "abc")
                )

    def failed_job(self, name="Renamed operation"):
        return self.job | {
            "id": 8,
            "name": "Renamed job",
            "conclusion": "failure",
            "steps": [
                {
                    "name": name,
                    "conclusion": "failure",
                    "started_at": "2026-11-11T15:00:00Z",
                    "completed_at": "2026-11-11T15:15:00Z",
                }
            ],
        }

    def evidence_log(self, category, label="Renamed operation"):
        record = json.dumps({"version": 1, "category": category})
        return (
            f"job\t{label}\t2026-11-11T15:01:00.000Z "
            f"{recovery.EVIDENCE_PREFIX}{record}"
        )

    def test_renamed_gate_requires_dependency_evidence(self):
        gate = self.failed_job()
        evidence = {
            7: (self.annotations, ""),
            8: ([], self.evidence_log("dependency-failure")),
        }
        self.assertTrue(recovery.decide([self.job, gate], evidence)[0])
        self.assertFalse(recovery.decide([gate], evidence)[0])
        self.assertFalse(recovery.decide([self.job, gate], {})[0])

    def test_unknown_or_test_failure_prevents_mixed_run_retry(self):
        for name in ["Run tests", "Build Debug", "CI gate"]:
            job = self.failed_job(name)
            evidence = {
                7: (self.annotations, ""),
                8: ([], f"job\t{name}\t2026-11-11T15:01:00Z ETIMEDOUT"),
            }
            self.assertFalse(recovery.decide([self.job, job], evidence)[0])
        self.assertFalse(recovery.decide([], {})[0])

    def test_evidence_is_scoped_to_failed_step_without_name_whitelist(self):
        job = self.failed_job()
        for label in ["Renamed operation", "UNKNOWN STEP"]:
            log = self.evidence_log("transient-network", label)
            self.assertIsNotNone(recovery.classify_job(job, [], log))
        unrelated = self.evidence_log("transient-network", "Other step")
        self.assertIsNone(recovery.classify_job(job, [], unrelated))
        outside = self.evidence_log("transient-network", "UNKNOWN STEP")
        outside = outside.replace("15:01:00", "14:00:00")
        self.assertIsNone(recovery.classify_job(job, [], outside))

    def test_unknown_malformed_and_future_records_fail_closed(self):
        job = self.failed_job()
        log = self.evidence_log("transient-network")
        for invalid in [
            self.evidence_log("unknown"),
            self.evidence_log("future-category"),
            log.replace('"version": 1', '"version": 2'),
            log.replace('"version": 1', 'not json'),
        ]:
            self.assertIsNone(recovery.classify_job(job, [], invalid))
            self.assertIsNone(
                recovery.classify_job(job, [], log + "\n" + invalid)
            )

    def test_each_failed_step_requires_its_own_evidence(self):
        job = self.failed_job()
        job["steps"].append({"name": "Test", "conclusion": "failure"})
        log = self.evidence_log("transient-network")
        self.assertIsNone(recovery.classify_job(job, [], log))
        job = self.failed_job()
        job["steps"][0]["conclusion"] = "cancelled"
        self.assertIsNone(recovery.classify_job(job, [], log))

    def invoke_recovery(self, final_run, final_master):
        with (
            patch.dict(
                "os.environ",
                {
                    "GITHUB_REPOSITORY": "owner/project",
                    "GITHUB_EVENT_PATH": "event.json",
                },
            ),
            patch.object(
                recovery.Path,
                "read_text",
                return_value='{"workflow_run": {"id": 42}}',
            ),
            patch.object(
                recovery,
                "api",
                side_effect=[
                    self.workflow_run,
                    {"object": {"sha": "abc"}},
                    final_run,
                    {"object": {"sha": final_master}},
                ],
            ),
            patch.object(
                recovery, "pages", side_effect=[[self.job], self.annotations]
            ),
            patch.object(recovery, "summary"),
            patch.object(recovery, "gh") as gh,
        ):
            recovery.recover()
            return gh

    def test_second_read_prevents_duplicate_or_stale_rerun(self):
        for changed, master in [
            (self.workflow_run | {"run_attempt": 2}, "abc"),
            (self.workflow_run, "new"),
            (self.workflow_run | {"conclusion": "cancelled"}, "abc"),
        ]:
            self.invoke_recovery(changed, master).assert_not_called()

    def test_recovers_entire_run_once(self):
        gh = self.invoke_recovery(self.workflow_run, "abc")
        gh.assert_called_once_with(
            "api",
            "--method",
            "POST",
            "repos/owner/project/actions/runs/42/rerun",
        )
