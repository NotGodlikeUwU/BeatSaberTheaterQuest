"""Check the delivered QMOD, embedded runtime assets and pinned source inputs."""
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile

root = Path(__file__).resolve().parents[1]
sha = lambda data: hashlib.sha256(data).hexdigest().upper()
package = root / "BeatSaberTheaterQuest-0.3.7.qmod"
with ZipFile(package) as archive:
    manifest = json.loads(archive.read("mod.json"))
    assert manifest["version"] == "0.3.7"
    assert manifest["packageVersion"] == "1.40.8_7379"
    assert manifest["modloader"] == "Scotland2"
    assert manifest["modFiles"] == ["libbstheater.so"]
    assert manifest["libraryFiles"] == ["libbstheaterjs.so"]
    expected = {"beatsaber-hook": "=6.4.2", "custom-types": "=0.18.4", "paper2_scotland2": "=4.8.0", "songcore": "=1.1.26", "bsml": "=0.4.55"}
    assert {item["id"]: item["version"] for item in manifest["dependencies"]} == expected
    for name in archive.namelist():
        assert "cookie" not in name.lower(), "Account session must never be packaged"
    for name in ("libbstheater.so", "libbstheaterjs.so"):
        binary = (root / "build" / name).read_bytes()
        assert archive.read(name) == binary
        assert binary[:5] == b"\x7fELF\x02" and int.from_bytes(binary[18:20], "little") == 183
        print(name + " SHA256 " + sha(binary))
    for name in ("quickjs-MIT.txt", "ejs-Unlicense.txt", "ejs-dependencies.txt", "songcore-icon-MIT.txt", "Rhino-MPL-2.0.txt", "LICENSE"):
        assert len(archive.read(name)) > 500
mod = (root / "build/libbstheater.so").read_bytes()
for name in ("bstheater-download.dex", "bstheater-desugar.dex"):
    assert (root / "java" / name).read_bytes() in mod
solver = (root / "build/libbstheaterjs.so").read_bytes()
for path in (root / "resources/ejs").glob("*.js"):
    assert path.read_bytes() in solver
for item in json.loads((root / "java/dependencies.sha256.json").read_text(encoding="utf-8-sig")):
    assert sha((root / "java/libs" / item["Name"]).read_bytes()) == item["SHA256"]
for item in json.loads((root / "third_party/player-solver.sha256.json").read_text(encoding="utf-8-sig")):
    assert sha((root / item["Path"]).read_bytes()) == item["SHA256"]
print("QMOD SHA256 " + sha(package.read_bytes()))
print("Package, embedded DEX/EJS, source and dependency hashes passed")










