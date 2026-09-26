"""Prepare public support assets without altering the developer installation."""
import copy
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def support_archive():
    source = ROOT / "run/vr-first-person/2ship.o2r"
    version = json.loads((ROOT / "platform/version.json").read_text(encoding="utf-8-sig"))
    if version["channel"] == "preview":
        return source
    destination = ROOT / "build/public-support/2ship.o2r"
    destination.parent.mkdir(parents=True, exist_ok=True)
    # The optional overlay font's supplied terms prohibit redistribution.
    # All other resources retain their paths, metadata and uncompressed bytes.
    excluded = {"fonts/Fipps-Regular.otf"}
    with zipfile.ZipFile(source) as original:
        names = original.namelist()
        assert len(names) == len(set(names)), "Duplicate support resources"
        with zipfile.ZipFile(destination, "w") as output:
            for entry in original.infolist():
                if entry.filename not in excluded:
                    output.writestr(copy.copy(entry), original.read(entry.filename))
        with zipfile.ZipFile(destination) as output:
            assert set(output.namelist()) == set(names) - excluded
            for name in output.namelist():
                assert output.read(name) == original.read(name), name
    return destination


if __name__ == "__main__":
    print(support_archive())
