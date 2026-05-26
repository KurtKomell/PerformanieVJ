import re
from pathlib import Path
cat = Path("src/core/FilterCatalog.cpp").read_text(encoding="utf-8")
fei = Path("src/core/FilterEffectIds.cpp").read_text(encoding="utf-8")
print("catalog adds:", len(re.findall(r'add\(', cat)))
print("kEntries:", len(re.findall(r'\{ "', fei)))
