"""Synthesised forward declarations, so clang knows what is a type.

sipify needs no semantics -- it slices types out of the header text -- but it
does need clang to *parse*, and parsing C++ requires knowing which identifiers
name types.  Without that, a nested template declaration is not a declaration
at all but a chain of comparisons, and clang drops it with no usable
diagnostic::

    virtual QMap<QString, QString> headers() const = 0;              // survives
    virtual QMap<QString, QList<QString>> fullHeaders() const = 0;   // lost

(``qgsserverresponse.h:89`` and ``:96``.  The first recovers, the second does
not, and nothing in the diagnostics distinguishes them from the hundreds of
"undeclared identifier" notes every header produces.)

Maintaining a hand-written list of Qt types would be a permanent chore and
would fail on every QGIS type used before its own header is reachable.  Instead
the identifiers are harvested from the header itself and declared opaquely.  A
forward declaration is all clang needs to treat the name as a type, and an
opaque type is all sipify ever wants.

Arity is the one thing a forward declaration must get right, so anything used
with ``<`` and not in :data:`KNOWN_TEMPLATES` is declared variadic, which
accepts any argument list.
"""

from __future__ import annotations

import re

# Identifiers that look like Qt or QGIS types.
_CANDIDATE = re.compile(r"\b(Q[A-Z][A-Za-z0-9_]*|Qgs[A-Z][A-Za-z0-9_]*)\b")
# Real namespaces must never be declared as classes -- `class Qt;` would break
# every `Qt::AlignLeft`.  Being *used* with `::` is not the test: `QVariant`
# appears as both `operator QVariant()` and `QVariant::fromValue(...)`, and
# skipping it there cost 11 headers a declaration.  Only genuine namespaces
# belong here, plus any the header declares itself.
_NAMESPACES = frozenset({"Qt", "QtPrivate", "QtConcurrent", "QtCharts", "std"})
_NAMESPACE_DECL = re.compile(r"\bnamespace\s+([A-Za-z_][A-Za-z0-9_]*)")
# Declared in this header already; redeclaring with the wrong kind is an error
# (`class` vs `struct` is fine, `class` vs `enum` is not).
_DECLARED = re.compile(
    r"\b(?:class|struct|union|enum(?:\s+class)?)\s+"
    r"(?:[A-Z][A-Z0-9_]*_EXPORT\s+)?([A-Za-z_][A-Za-z0-9_]*)"
)
_TEMPLATE_USE = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*<")

# Declared with real arity in prelude.h; do not redeclare them here.
KNOWN_TEMPLATES = frozenset(
    {
        "QList",
        "QVector",
        "QSet",
        "QStack",
        "QQueue",
        "QPointer",
        "QFuture",
        "QFutureWatcher",
        "QSharedPointer",  # skip-keyword-check
        "QWeakPointer",
        "QScopedPointer",  # skip-keyword-check
        "QSharedDataPointer",
        "QExplicitlySharedDataPointer",
        "QThreadStorage",
        "QFlags",
        "QObjectUniquePtr",
        "QObjectParentUniquePtr",
        "QMap",
        "QHash",
        "QMultiMap",
        "QMultiHash",
        "QCache",
        "QPair",
    }
)


def synthesize(text: str) -> str:
    """Forward declarations covering every Qt/QGIS type name ``text`` mentions."""
    declared = {m.group(1) for m in _DECLARED.finditer(text)}
    namespaces = _NAMESPACES | {m.group(1) for m in _NAMESPACE_DECL.finditer(text)}
    templated = {m.group(1) for m in _TEMPLATE_USE.finditer(text)}

    names = {m.group(1) for m in _CANDIDATE.finditer(text)}
    names -= declared | namespaces | KNOWN_TEMPLATES

    lines = ["// synthesised by sipify/forward.py -- opaque, for parsing only"]
    for name in sorted(names):
        if name in templated:
            # Arity is unknown, and getting it wrong is worse than not knowing:
            # a variadic parameter pack matches any argument list.
            lines.append(f"template<typename... T> class {name};")
        else:
            lines.append(f"class {name};")
    return "\n".join(lines) + "\n"
