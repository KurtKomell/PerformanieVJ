import pathlib

root = pathlib.Path(__file__).resolve().parents[1] / "shaders"
common = (root / "effect_common.glsl").read_text(encoding="utf-8")
for path in sorted(root.glob("effect_*.frag")):
    if path.name == "effect_identity.frag":
        continue
    text = path.read_text(encoding="utf-8")
    needle = '#include "effect_common.glsl"\n'
    if needle not in text:
        print("skip", path.name)
        continue
    text = text.replace(needle, common + "\n")
    path.write_text(text, encoding="utf-8", newline="\n")
    print("updated", path.name)
