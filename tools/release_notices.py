"""Collect local third-party license notices for both release packages."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def notices():
    result = {}
    def add(component, path):
        if path.is_file():
            result[component + "/" + path.name] = path.read_bytes()

    for component in ("2ship2harkinian", "2ship2harkinian/libultraship",
                      "2ship2harkinian/ZAPDTR", "2ship2harkinian/OTRExporter"):
        base = ROOT / "src" / component
        for name in ("LICENSE", "LICENSE.txt", "COPYING"):
            add(component.replace("/", "-"), base / name)
    for base in sorted((ROOT / "build/android-quest/_deps").glob("*-src")):
        for path in sorted(base.iterdir()):
            if path.is_file() and path.name.upper().startswith(("LICENSE", "LICENCE", "COPYING", "NOTICE", "COPYRIGHT")):
                add("quest-" + base.name[:-4], path)
    for base in sorted((ROOT / "toolchains/vcpkg/installed/x64-windows-static/share").iterdir()):
        if base.is_dir():
            add("windows-" + base.name, base / "copyright")
    for path in sorted((ROOT / "docs/releases/licenses/fonts").glob("*-OFL.txt")):
        add("fonts", path)
    stb = (ROOT / "build/android-quest/_deps/stb/stb_image.h").read_text(encoding="utf-8")
    marker = "This software is available under 2 licenses -- choose whichever you prefer."
    assert marker in stb, "Review stb license layout before packaging"
    result["stb/LICENSE.txt"] = (stb[stb.index(marker):].rsplit("*/", 1)[0].strip() + "\n").encode("utf-8")
    assert result, "No dependency notices found"
    return result


if __name__ == "__main__":
    destination = ROOT / "build/quest-package/assets/licenses"
    for name, data in notices().items():
        path = destination / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    print("Dependency notices staged for Quest.")
