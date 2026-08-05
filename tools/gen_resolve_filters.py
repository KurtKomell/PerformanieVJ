#!/usr/bin/env python3
"""Generate Resolve filter C++ snippets and validate inventory against FilterCatalog."""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INVENTORY = ROOT / "tools" / "resolve_effects_inventory.json"
CATALOG_CPP = ROOT / "src" / "core" / "FilterCatalog.cpp"

FAMILY_ENUM = {
    "Blur": "FilterEffectFamily::Blur",
    "Color": "FilterEffectFamily::Color",
    "Transform": "FilterEffectFamily::Transform",
    "Distort": "FilterEffectFamily::Distort",
    "Kaleido": "FilterEffectFamily::Kaleido",
    "Generate": "FilterEffectFamily::Generate",
    "Stylize": "FilterEffectFamily::Stylize",
    "Key": "FilterEffectFamily::Key",
    "Mask": "FilterEffectFamily::Mask",
    "Blend": "FilterEffectFamily::Blend",
    "Pattern": "FilterEffectFamily::Pattern",
    "Utility": "FilterEffectFamily::Utility",
    "Light": "FilterEffectFamily::Light",
    "Revival": "FilterEffectFamily::Revival",
    "Temporal": "FilterEffectFamily::Temporal",
    "Texture": "FilterEffectFamily::Texture",
    "Maxine": "FilterEffectFamily::Maxine",
}


def load_existing_type_ids() -> set[str]:
    text = CATALOG_CPP.read_text(encoding="utf-8")
    return set(re.findall(r'add\("[^"]+",\s*"([^"]+)"', text))


def main() -> int:
    data = json.loads(INVENTORY.read_text(encoding="utf-8"))
    effects = data["effects"]
    existing = load_existing_type_ids()
    new_ids = [e["typeId"] for e in effects]
    dupes = [tid for tid in new_ids if tid in existing]
    diff_path = ROOT / "tools" / "resolve_catalog_diff.json"
    diff_path.write_text(
        json.dumps(
            {
                "existing_catalog_count": len(existing),
                "new_resolve_count": len(new_ids),
                "duplicates": dupes,
                "new_type_ids": new_ids,
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    if dupes:
        print("ERROR: duplicates with existing catalog:", dupes, file=sys.stderr)
        return 1

    # FilterCatalog.cpp block
    catalog_lines = ["    // --- DaVinci Resolve Effects Library (missing filters only) ---"]
    last_cat = None
    for e in effects:
        if e["category"] != last_cat:
            catalog_lines.append(f'    // {e["category"]}')
            last_cat = e["category"]
        catalog_lines.append(
            f'    add("{e["category"]}", "{e["typeId"]}", "{e["name"]}");'
        )
    (ROOT / "tools" / "generated_resolve_catalog.txt").write_text(
        "\n".join(catalog_lines) + "\n", encoding="utf-8"
    )

    # FilterEffectIds.cpp block
    effect_lines = ["    // DaVinci Resolve Effects Library"]
    for e in effects:
        fam = FAMILY_ENUM[e["family"]]
        backend = ""
        if e.get("backend") == "Maxine":
            backend = ", FilterExecutionBackend::Maxine"
        effect_lines.append(
            f'    {{ "{e["typeId"]}", {fam}, {e["familyId"]}, 1, -1{backend} }},'
        )
    (ROOT / "tools" / "generated_resolve_effects.txt").write_text(
        "\n".join(effect_lines) + "\n", encoding="utf-8"
    )

    # FilterParamSchema.cpp block — blend + 3 generic params per Resolve effect
    schema_lines = ["    // DaVinci Resolve — blend + 3 core params (V1 simplification)"]
    for e in effects:
        if e.get("backend") == "Maxine":
            schema_lines.append(
                f'    setParams("{e["typeId"]}", '
                f'{{ percentParam("blend", "Blend", 1.0), '
                f'enumParam("strength", "Strength", {{ "Weak", "Strong" }}, 0), '
                f'percentParam("amount", "Amount", 0.5) }});'
            )
        else:
            schema_lines.append(
                f'    setParams("{e["typeId"]}", '
                f'{{ percentParam("blend", "Blend", 1.0), '
                f'percentParam("strength", "Strength", 0.5), '
                f'floatParam("detail", "Detail", 0.0, 1.0, 0.5), '
                f'floatParam("size", "Size", 0.0, 1.0, 0.5) }});'
            )
    (ROOT / "tools" / "generated_resolve_schemas.txt").write_text(
        "\n".join(schema_lines) + "\n", encoding="utf-8"
    )

    print(f"OK: {len(new_ids)} new Resolve effects, {len(existing)} existing catalog entries")
    print(f"Wrote tools/resolve_catalog_diff.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
