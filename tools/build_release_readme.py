"""Build the single GitHub guide from the maintained platform guides.

Shared sections must match so a control correction cannot silently diverge.
Platform downloads retain their own standalone installation guide.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def sections(text):
    parts = re.split(r"^## (.+)\n", text, flags=re.M)
    return parts[0], dict(zip(parts[1::2], parts[2::2]))

def build():
    intro, windows = sections((ROOT / "docs/releases/windows/README.md").read_text(encoding="utf-8"))
    _, quest = sections((ROOT / "docs/releases/quest/README.md").read_text(encoding="utf-8"))
    for title in windows.keys() & quest.keys() - {"First installation"}:
        if windows[title] != quest[title]:
            raise ValueError("Shared player instructions differ: " + title)
    intro = intro.replace("# Majora's Mask VR — Windows player guide", "# Majora's Mask VR")
    intro += "One project for **Windows PCVR** and **standalone Meta Quest**. Choose the download for your platform.\n\n"
    intro += "**Release preparation:** the VR downloads are not public yet. This guide describes the upcoming version 0.1 beta. The repository currently contains the upstream source; the VR source update is still being prepared.\n\n"
    intro += "[Installation](#first-installation) · [Controls](#default-controls) · [Physical items](#physical-items-and-combat) · [Forms and gestures](#forms-movement-and-songs) · [Save states](#exact-save-states-and-ordinary-saves)\n\n"
    text = intro + "## First installation\n\n"
    text += "| Platform | Download | Runs on |\n| --- | --- | --- |\n| PCVR | `MMVR-Windows-version.zip` | Windows PC connected to your VR headset |\n| Quest standalone | `MMVR-Quest-version.apk` | Quest itself; no gaming PC while playing |\n\n"
    text += "Download the platform asset from this repository's **Releases** page once available. GitHub's automatically generated **Source code** ZIP/TAR files are not playable builds.\n\n"
    for label, guide in (("Windows PCVR", windows), ("Quest standalone", quest)):
        # The legal paragraphs are identical; print them once below both installations.
        steps = guide["First installation"].split("No Nintendo game ROM", 1)[0].strip()
        text += "### " + label + "\n\n" + steps + "\n\n"
    legal = windows["First installation"].split("No Nintendo game ROM", 1)[1]
    text += "No Nintendo game ROM" + legal
    for title in ("Before updating", "Beta release status", "PC runtime and headset setup", "Desktop recording view", "Gaming laptops and GPUs", "PC files, mods and updates"):
        text += "## " + title + "\n" + windows[title]
    for title in ("Quest files, mods and updates", "Quest refresh and comfort"):
        text += "## " + title + "\n" + quest[title]
    start = False
    for title, body in windows.items():
        if title == "How to play: saving and resuming":
            start = True
        if start:
            text += "## " + title + "\n" + body
    text += "\n## Credits\n\nBuilt on [2Ship2Harkinian](https://github.com/2ship2harkinian/2ship2harkinian) and its contributors, with VR work by Full Dive Games. Retain the project and dependency license notices distributed with each build.\n"
    target = ROOT / "docs/releases/README.md"
    target.write_text(text, encoding="utf-8")
    return target

if __name__ == "__main__":
    print(build())
