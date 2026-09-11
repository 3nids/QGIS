# Stage-A spike: a libclang front end for sipify

Throwaway harness answering one question — **can libclang see everything the
current `scripts/sipify.py` sees, without a Qt install, a compiler, or a build
tree?** It is not an emitter and produces no `.sip` output.

## Result

Measured against all 1522 sipified headers on `master`:

| | |
|---|---|
| headers preprocessed without error | **1522 / 1522** |
| headers whose declarations are fully accounted for | **1522 / 1522** |
| declarations found | 57,366 |
| `SIP_*` macros bound to the right owner | **7,881 / 7,882 (99.99 %)** |
| parse time, 10 workers | **1.11 s** (current sipify: 15.7 s) |
| system dependencies | none — `-nostdinc -nostdinc++`, no Qt, no build tree |

The single unattributed macro is `qgsmaprendererparalleljob.h:75`, a `SIP_SKIP`
on the continuation line of a multi-line member initialiser.

## How to run it

```bash
# the list of sipified headers, as sipify_all.sh derives it
python3 - <<'PY' > /tmp/headers.txt
import re
for m in ('core', 'gui', 'analysis', 'server', '3d'):
    for line in open(f'python/PyQt6/{m}/{m}_auto.sip'):
        got = re.match(r'^%Include auto_generated/(.*)\.sip\s*$', line)
        if got:
            print(f'src/{m}/{got.group(1)}.h'.replace(f'src/{m}/./', f'src/{m}/'))
PY

PYTHONPATH=scripts python3 scripts/sipify/spike/spike.py  /tmp/headers.txt
PYTHONPATH=scripts python3 scripts/sipify/spike/oracle.py /tmp/headers.txt
```

`spike.py` reports declaration and macro-attribution coverage; `oracle.py`
cross-checks the names found against the committed `.sip.in` files, which are a
perfect oracle (regenerating them is byte-identical today). Needs Python ≥ 3.11
and the `clang` bindings.

## What the spike established, beyond the headline number

Four things that were wrong in the original design sketch, each found by
running it rather than by reasoning about it:

1. **clang's types are unusable; its structure is exact.** With no Qt headers,
   76 of 295 return types in `qgsvectorlayer.h` come back as a bogus `int`
   (`QString storageType() const` → `int`). The emitter must slice types out of
   the header text. `spans.py` exists for that.

2. **"What clang sees" and "what reaches the `.sip`" are different axes.**
   16 headers straddle a class head across `#ifdef SIP_RUN` to work around
   SIP's inability to inherit from a template (`qgsimagecache.h:171`). Both
   branches open a brace that only the shared body closes, so the C++ branch
   must stay visible to clang while being dropped from the output. Conflating
   the two corrupts the parse. See `preprocess.py`.

3. **Diagnostics cannot be a safety net.** `Q_CLASSINFO` missing from the
   prelude cost `qgis.h` its `version()` declaration — and every diagnostic
   pointed at the `Q_CLASSINFO` line, none at the method that vanished. The
   first unresolved `#include` is fatal and suppresses everything after it, so
   a header with 394 real errors reports 1. Hence `exportmacros.py`'s
   fail-closed rule, and the declaration-count cross-check still to be written.

4. **Macro attribution cannot use cursor containment.** The macros expand to
   nothing, so they fall outside the extents of what they decorate —
   inconsistently: `SIP_TRANSFER` is outside its parameter's extent, but
   `SIP_PYARGREMOVE` is inside when a default value follows. `tokens.py` uses
   clang only to locate parameter starts and slices the text between them.

## Not done here

No emitter, no docstring conversion, no `auto_additions` output, no
`class_map.yaml`. Byte-identity against the corpus is Stage E and depends on
the Stage-D normalisation commit landing first.
