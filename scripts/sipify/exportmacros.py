"""The export-macro allowlist, and the fail-closed rule that guards it.

When an ``*_EXPORT`` macro is not defined, libclang does not complain in any
way you can detect -- it reports the *macro* as the class and the *class* as a
variable, and the class body vanishes::

    class CORE_DEPRECATED_EXPORT QgsSettingsRegistry { ... };
    -> CLASS_DECL spelling='CORE_DEPRECATED_EXPORT'
       VAR_DECL   spelling='QgsSettingsRegistry'

Five headers hit exactly that today.  So the list is not a convenience: an
unknown ``*_EXPORT`` token is a hard error, because the alternative is losing
declarations silently.
"""

from __future__ import annotations

import re

# Every export macro in src/, measured.  Grouped by module for reading.
KNOWN_EXPORT_MACROS = frozenset(
    {
        "CORE_EXPORT",
        "CORE_DEPRECATED_EXPORT",
        "CORE_NO_EXPORT",
        "GUI_EXPORT",
        "ANALYSIS_EXPORT",
        "SERVER_EXPORT",
        "APP_EXPORT",
        "CUSTOMWIDGETS_EXPORT",
        "GRASS_LIB_EXPORT",
        "QUICK_EXPORT",
        "NATIVE_EXPORT",
        "TEST_EXPORT",
        "PYTHON_EXPORT",
        "Q_SQL_EXPORT",
        "_3D_EXPORT",
        "_3D_NO_EXPORT",
    }
)

# The leading underscore and the digit both matter: `_3D_EXPORT` accounts for
# 70 uses, and a naive `\b[A-Z][A-Z0-9_]*_EXPORT\b` misses every one of them.
EXPORT_TOKEN = re.compile(r"\b_?[A-Za-z][A-Za-z0-9_]*_EXPORT\b")


class UnknownExportMacro(Exception):
    """An ``*_EXPORT`` spelling the prelude does not define."""


def find_unknown(text: str) -> set[str]:
    return {m.group(0) for m in EXPORT_TOKEN.finditer(text)} - KNOWN_EXPORT_MACROS


def check(path: str, text: str) -> None:
    unknown = find_unknown(text)
    if unknown:
        raise UnknownExportMacro(
            f"{path}: undefined export macro(s) {sorted(unknown)} -- add them to "
            f"KNOWN_EXPORT_MACROS and scripts/sipify/prelude.h, or libclang will "
            f"silently drop the decorated class bodies"
        )
