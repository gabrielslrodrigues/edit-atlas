"""Runner-independent provenance checks for generated media fixtures.

The rendered-video fixtures are produced by a built generator rather than
committed, so a checkout can hold fixtures from an older generator revision.
CI regenerates them whenever the inputs that determine their content change,
because its fixture cache key hashes those inputs. Nothing local does.

A fixture directory therefore records the digest of the inputs that produced
it, and the suite refuses to run against a directory whose recorded digest is
absent or no longer matches the tree. The digest covers the same inputs as the
CI cache key, so both agree on what counts as fresh.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import sys


#: Inputs that determine generated fixture content. Kept identical to the
#: fixture cache key in `.github/workflows/build-and-package.yml`.
GENERATOR_INPUTS = (
    Path("tests/integration/media/e2e_fixture_generator.cpp"),
    Path("tests/integration/media/media_fixture.cpp"),
    Path("tests/integration/media/include/edit_atlas/test/media_fixture.hpp"),
    Path("vcpkg.json"),
)

STAMP_NAME = "generator-digest.txt"

GENERATOR_NAME = "edit_atlas_e2e_media_fixture_generator"


def regeneration_command(fixture_directory: Path) -> str:
    """Spell the entry-point invocation that refreshes a fixture directory."""
    generator = "build/<preset>/tests/integration/media/" + GENERATOR_NAME
    if os.name == "nt":
        return (
            "tests/e2e/generate-media-fixtures.ps1 `\n"
            f'  -Generator "{generator}.exe" `\n'
            f"  -FixtureDirectory {fixture_directory}"
        )
    return (
        "tests/e2e/generate-media-fixtures.sh \\\n"
        f"  {generator} \\\n"
        f"  {fixture_directory}"
    )


def generator_digest(repository_root: Path) -> str | None:
    """Digest the inputs that determine fixture content.

    Returns None when an input is absent, such as a run from a tree without
    the generator sources, so provenance is reported as unknown rather than
    as a mismatch.
    """
    digest = hashlib.sha256()
    for relative in GENERATOR_INPUTS:
        path = repository_root / relative
        if not path.is_file():
            return None
        payload = path.read_bytes()
        digest.update(relative.as_posix().encode("utf-8"))
        digest.update(len(payload).to_bytes(8, "big"))
        digest.update(payload)
    return digest.hexdigest()


def read_recorded_digest(fixture_directory: Path) -> str | None:
    """Return the digest a fixture directory records, if it records one."""
    stamp = fixture_directory / STAMP_NAME
    if not stamp.is_file():
        return None
    recorded = stamp.read_text(encoding="utf-8").strip()
    return recorded or None


def record_digest(repository_root: Path, fixture_directory: Path) -> str:
    """Record the tree's generator digest against a fixture directory."""
    digest = generator_digest(repository_root)
    if digest is None:
        raise FileNotFoundError(
            "cannot record a generator digest without the generator inputs"
        )
    (fixture_directory / STAMP_NAME).write_text(
        digest + "\n", encoding="utf-8"
    )
    return digest


def stale_fixture_reason(
    repository_root: Path, fixture_directory: Path
) -> str | None:
    """Describe why a fixture directory is not current, or return None."""
    expected = generator_digest(repository_root)
    if expected is None:
        return None

    recorded = read_recorded_digest(fixture_directory)
    if recorded is None:
        return (
            "the directory records no generator digest, so the revision that "
            "produced it is unknown"
        )
    if recorded != expected:
        return (
            f"the recorded generator digest {recorded[:12]} does not match "
            f"the generator in this tree, {expected[:12]}"
        )
    return None


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Record the generator digest for a fixture directory."
    )
    parser.add_argument("fixture_directory", type=Path)
    parser.add_argument(
        "--repository-root",
        type=Path,
        default=Path(__file__).resolve().parents[3],
    )
    arguments = parser.parse_args(argv)
    try:
        digest = record_digest(
            arguments.repository_root.resolve(),
            arguments.fixture_directory.resolve(),
        )
    except (FileNotFoundError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(digest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
