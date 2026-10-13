"""Translate only BSML bindings/defaults needed by Quest; retain the PC layout."""
from pathlib import Path
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
tree = ET.fromstring((root / "resources/video-menu.bsml").read_text())
bindings = {
    "root-object":"menuRoot", "no-video-bg":"noVideoRoot", "no-video-text":"noVideoLabel",
    "search-button":"searchButton", "video-details":"detailsRoot", "video-title":"titleLabel",
    "delete-config-button":"deleteConfigButton", "video-thumbnail":"detailsThumbnail",
    "video-author":"authorLabel", "video-duration":"durationLabel", "video-status":"statusLabel",
    "delete-video-button":"mediaButton", "offset-controls":"offsetRoot", "video-offset":"offsetLabel",
    "customize-offset-toggle":"customizeOffsetRoot", "preview-button":"previewButton",
    "video-search-results":"searchRoot", "video-list":"resultsList", "search-results-loading":"searchLabel",
    "search-keyboard":"queryKeyboard", "download-button":"resultDownloadButton",
}
actions = {
    "on-search-action":"SearchClick", "on-delete-config-action":"DeleteConfigClick",
    "on-delete-video-action":"MediaClick", "on-preview-action":"PreviewClick",
    "on-offset-decrease-action-high":"OffsetMinusHigh", "on-offset-decrease-action-mid":"OffsetMinusMid",
    "on-offset-decrease-action-low":"OffsetMinusLow", "on-offset-increase-action-high":"OffsetPlusHigh",
    "on-offset-increase-action-mid":"OffsetPlusMid", "on-offset-increase-action-low":"OffsetPlusLow",
    "on-select-cell":"SelectCell", "on-query":"QueryEntered", "on-back-action":"BackClick",
    "on-download-action":"DownloadClick", "on-refine-action":"RefineClick",
}
for element in tree.iter():
    element.attrib.pop("{http://www.w3.org/2001/XMLSchema-instance}noNamespaceSchemaLocation", None)
    if "id" in element.attrib:
        element.set("id", bindings.get(element.get("id"), element.get("id")))
    for key in ("on-click", "select-cell", "on-enter"):
        if key in element.attrib: element.set(key, actions[element.get(key)])
    if element.tag == "button-with-icon":
        element.tag = "icon-button"
        element.attrib.pop("icon") # Same PC PNG assigned by the native host.
    if element.tag in ("vertical", "horizontal"):
        # PC BSML controls child sizes; Quest's raw Unity layout defaults do not.
        element.attrib.setdefault("child-control-width", "true")
        element.attrib.setdefault("child-control-height", "true")
        if element.tag == "vertical": element.attrib.setdefault("vertical-fit", "PreferredSize")
        align = element.attrib.pop("align", None)
        if align: element.set("child-align", {"Center":"MiddleCenter","top":"UpperCenter","Top":"UpperCenter","Left":"MiddleLeft","Right":"MiddleRight"}[align])
    if element.tag == "bool-setting":
        element.set("value", "customizeOffset")
        element.set("id", "customizeToggle")
    if element.get("id") == "previewButton":
        element.set("hover-hint", "Preview video and map with stereo audio")
ET.indent(tree)
markup = ET.tostring(tree, encoding="unicode")
(root / "resources/video-menu-quest.bsml").write_bytes((markup + "\n").encode("utf-8"))
icon = (root / "resources/theater-trash.png").read_bytes()
(root / "include/theater_trash_icon.inc").write_text("static const std::vector<uint8_t> theaterTrashIconBytes = {\n" + ",\n".join(",".join(f"0x{b:02x}" for b in icon[i:i+24]) for i in range(0,len(icon),24)) + "\n};\n")
