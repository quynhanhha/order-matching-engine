"""Read-only checks of the published source, result accounting, and artifact hashes."""

import hashlib
import json
import pathlib
import subprocess
import sys
import tarfile


def main() -> None:
    bundle = pathlib.Path(__file__).resolve().parent
    repo = bundle.parents[2]
    sys.path.insert(0, str(repo))
    from scripts.summarize_throughput import summarize

    for line in (bundle / "SHA256SUMS").read_text().splitlines():
        digest, name = line.split("  ", 1)
        assert hashlib.sha256((bundle / name).read_bytes()).hexdigest() == digest, name
    source_hashes = dict(
        (name, digest) for digest, name in
        (line.split("  ", 1) for line in (bundle / "source-sha256.txt").read_text().splitlines())
    )
    provenance = json.loads((bundle / "provenance.json").read_text())
    with tarfile.open(bundle / "measured-source.tar.gz", "r:gz") as archive:
        assert set(archive.getnames()) == set(source_hashes)
        for member in archive.getmembers():
            assert member.isfile() and not pathlib.PurePosixPath(member.name).is_absolute()
            assert ".." not in pathlib.PurePosixPath(member.name).parts
            stream = archive.extractfile(member)
            assert stream is not None
            data = stream.read()
            assert hashlib.sha256(data).hexdigest() == source_hashes[member.name]
            committed = subprocess.check_output([
                "git", "show", f"{provenance['measured_code_equivalent_commit']}:{member.name}"
            ], cwd=repo)
            assert data == committed, member.name
    assert len(source_hashes) == provenance["source_file_count"] == 21
    assert (bundle / "binary-sha256.txt").read_text().split()[0] == provenance["executable_sha256"]
    for prefix, key in (("benchmark", "google_benchmark"), ("gtest", "google_test")):
        assert (bundle / f"{prefix}-commit.txt").read_text().strip() == provenance[key]["commit"]
        assert (bundle / f"{prefix}-version.txt").read_text().strip() == provenance[key]["tag"]
    assert summarize(bundle) == json.loads((bundle / "summary.json").read_text())
    assert "100% tests passed, 0 tests failed out of 81" in (bundle / "tests-sanitizers.txt").read_text()
    assert "Ran 6 tests" in (bundle / "tests-summary.txt").read_text()
    assert "OK" in (bundle / "tests-summary.txt").read_text()
    for path in bundle.iterdir():
        if path.is_file():
            ignored = subprocess.run(["git", "check-ignore", "-q", str(path)], cwd=repo)
            assert ignored.returncode == 1, f"Ignored or inaccessible evidence: {path}"
    print("Verified artifact hashes, 21 source files against the measured-code commit,")
    print("all 60 raw repetitions and outcome/rate accounting, summary statistics,")
    print("dependency identities, recorded test results, and evidence Git visibility.")


if __name__ == "__main__":
    main()
