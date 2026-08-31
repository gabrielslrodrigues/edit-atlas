from __future__ import annotations

from pathlib import Path

from application.media_fixtures import (
    GENERATOR_INPUTS,
    STAMP_NAME,
    generator_digest,
    read_recorded_digest,
    record_digest,
    stale_fixture_reason,
)


def _tree(tmp_path: Path, marker: str = "original") -> tuple[Path, Path]:
    root = tmp_path / "tree"
    for index, relative in enumerate(GENERATOR_INPUTS):
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(f"{marker} {index}\n", encoding="utf-8")
    fixtures = tmp_path / "media-fixtures"
    fixtures.mkdir()
    return root, fixtures


def test_digest_is_stable_for_an_unchanged_tree(tmp_path: Path) -> None:
    root, _ = _tree(tmp_path)
    assert generator_digest(root) == generator_digest(root)


def test_digest_changes_with_any_input(tmp_path: Path) -> None:
    root, _ = _tree(tmp_path)
    baseline = generator_digest(root)
    digests = set()
    for relative in GENERATOR_INPUTS:
        path = root / relative
        original = path.read_text(encoding="utf-8")
        path.write_text(original + "// changed\n", encoding="utf-8")
        digests.add(generator_digest(root))
        path.write_text(original, encoding="utf-8")
    assert baseline not in digests
    assert len(digests) == len(GENERATOR_INPUTS)


def test_digest_is_unknown_without_the_generator_inputs(
    tmp_path: Path,
) -> None:
    root, _ = _tree(tmp_path)
    (root / GENERATOR_INPUTS[0]).unlink()
    assert generator_digest(root) is None


def test_recorded_digest_matches_the_tree_that_recorded_it(
    tmp_path: Path,
) -> None:
    root, fixtures = _tree(tmp_path)
    recorded = record_digest(root, fixtures)
    assert (fixtures / STAMP_NAME).is_file()
    assert read_recorded_digest(fixtures) == recorded
    assert stale_fixture_reason(root, fixtures) is None


def test_fixtures_without_a_recorded_digest_are_stale(tmp_path: Path) -> None:
    root, fixtures = _tree(tmp_path)
    reason = stale_fixture_reason(root, fixtures)
    assert reason is not None
    assert "records no generator digest" in reason


def test_blank_recorded_digest_is_treated_as_absent(tmp_path: Path) -> None:
    root, fixtures = _tree(tmp_path)
    (fixtures / STAMP_NAME).write_text("  \n", encoding="utf-8")
    assert read_recorded_digest(fixtures) is None
    assert stale_fixture_reason(root, fixtures) is not None


def test_fixtures_from_an_older_generator_are_stale(tmp_path: Path) -> None:
    root, fixtures = _tree(tmp_path)
    stale = record_digest(root, fixtures)
    (root / GENERATOR_INPUTS[0]).write_text("changed\n", encoding="utf-8")
    reason = stale_fixture_reason(root, fixtures)
    assert reason is not None
    assert stale[:12] in reason
    assert generator_digest(root)[:12] in reason


def test_provenance_is_not_judged_without_the_generator_inputs(
    tmp_path: Path,
) -> None:
    root, fixtures = _tree(tmp_path)
    record_digest(root, fixtures)
    (root / GENERATOR_INPUTS[1]).unlink()
    assert stale_fixture_reason(root, fixtures) is None
