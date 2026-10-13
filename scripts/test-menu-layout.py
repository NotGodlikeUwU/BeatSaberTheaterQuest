"""Check the Quest document against the authoritative, unmodified PC layout."""
from pathlib import Path
import hashlib
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
pc = root / "resources/video-menu.bsml"
assert hashlib.sha256(pc.read_bytes()).hexdigest().upper() == "E48B5ACD5D4C6FE692DBAE14961FB734EB33F9592733E096091D0950A9BD388B"
assert hashlib.sha256((root / "resources/theater-trash.png").read_bytes()).hexdigest().upper() == "FB0DBAA5F9FCC0274C050EE133EB7257C9032430A98CC5B065F7F8B5E9F90ED3"
original = list(ET.fromstring(pc.read_text()).iter())
quest = list(ET.fromstring((root / "resources/video-menu-quest.bsml").read_text()).iter())
assert len(original) == len(quest), "The Quest menu must not add or remove PC elements"
checks = 2
for before, after in zip(original, quest):
    assert after.tag == ("icon-button" if before.tag == "button-with-icon" else before.tag)
    for key, value in before.attrib.items():
        if key in ("id", "on-click", "on-enter", "select-cell", "icon") or key.startswith("{"): continue
        if key == "value" and before.tag == "bool-setting": continue
        if key == "hover-hint" and after.get("id") == "previewButton": continue # User requested stereo preview.
        if key == "align" and before.tag in ("vertical", "horizontal"): continue
        assert after.get(key) == value, (before.tag, key, value, after.get(key))
        checks += 1
by_id = {element.get("id"):element for element in quest if element.get("id")}
assert by_id["titleLabel"].get("font-size") == "5"
assert (by_id["detailsThumbnail"].get("pref-width"),by_id["detailsThumbnail"].get("pref-height")) == ("40","25")
assert by_id["resultsList"].get("visible-cells") == "6"
assert by_id["resultsList"].get("list-width") == "100"
assert not any("Scan all" in element.get("text", "") or "Cancel downloads" in element.get("text", "") for element in quest)
print(f"PC menu/icon hashes and {checks} original layout attributes passed")
