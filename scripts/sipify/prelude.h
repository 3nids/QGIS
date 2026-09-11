/* Stub prelude: what clang needs to parse a QGIS header with no Qt, no system
 * headers and no build tree.
 *
 * Injected via -include, never prepended, so buffer line 1 stays file line 1.
 *
 * The point of this file is that sipify does not need semantics.  It needs to
 * know *where* each declaration is and *which* SIP_ macros decorate it; the
 * text it emits is sliced from the header itself.  So unknown types are
 * harmless, but unknown *macros* are not: an undefined macro in a declarator
 * makes clang mis-parse, or silently swallow, the declarations around it.
 * Everything that can appear in a declaration must therefore be defined here.
 */
#ifndef SIPIFY_PRELUDE_H
#define SIPIFY_PRELUDE_H

/* Export macros.  exportmacros.py fails closed on any *_EXPORT token that is
 * not in this list -- when one is missing, libclang reports the macro as the
 * class and the class as a variable, and the class body disappears. */
#define CORE_EXPORT
#define CORE_DEPRECATED_EXPORT
#define CORE_NO_EXPORT
#define GUI_EXPORT
#define ANALYSIS_EXPORT
#define SERVER_EXPORT
#define APP_EXPORT
#define CUSTOMWIDGETS_EXPORT
#define GRASS_LIB_EXPORT
#define QUICK_EXPORT
#define NATIVE_EXPORT
#define TEST_EXPORT
#define PYTHON_EXPORT
#define Q_SQL_EXPORT
#define _3D_EXPORT
#define _3D_NO_EXPORT

/* Qt object macros.  `signals`/`slots` matter more than they look: without
 * them `private slots:` is a parse error and clang drops the first member of
 * every such section. */
#define signals public
#define slots
#define Q_SIGNALS public
#define Q_SLOTS
#define Q_EMIT
#define Q_OBJECT
#define Q_GADGET
#define Q_PROPERTY( ... )
#define Q_ENUM( ... )
#define Q_ENUM_NS( ... )
#define Q_FLAG( ... )
#define Q_FLAG_NS( ... )
#define Q_FLAGS( ... )
#define Q_ENUMS( ... )
#define Q_DECLARE_FLAGS( ... )
#define Q_DECLARE_OPERATORS_FOR_FLAGS( ... )
#define Q_DECLARE_METATYPE( ... )
#define Q_DECLARE_TYPEINFO( ... )
#define Q_DECLARE_TR_FUNCTIONS( ... )
#define Q_DECLARE_PRIVATE( ... )
#define Q_DECLARE_PUBLIC( ... )
#define Q_DISABLE_COPY( ... )
#define Q_DISABLE_COPY_MOVE( ... )
#define Q_GLOBAL_STATIC( ... )
#define Q_GLOBAL_STATIC_WITH_ARGS( ... )
#define Q_PRIVATE_SLOT( ... )
/* Q_CLASSINFO is the cautionary tale for this whole file: undefined, it cost
 * `qgis.h` its `version()` declaration -- and every diagnostic pointed at the
 * Q_CLASSINFO line, none at the method that vanished.  That is why
 * failclosed.py counts declarations instead of trusting diagnostics. */
#define Q_CLASSINFO( ... )
#define Q_ASSERT( ... )
#define Q_ASSERT_X( ... )
#define Q_UNUSED( ... )
#define Q_INVOKABLE
#define Q_REQUIRED_RESULT
#define Q_DECL_DEPRECATED
#define Q_DECL_UNUSED
#define Q_DECL_CONSTEXPR constexpr
#define Q_DECL_RELAXED_CONSTEXPR constexpr
#define Q_DECL_NOTHROW noexcept
#define Q_DECL_OVERRIDE override
#define Q_DECL_FINAL final
#define Q_NOWARN_DEPRECATED_PUSH
#define Q_NOWARN_DEPRECATED_POP
#define Q_NOWARN_UNREACHABLE_PUSH
#define Q_NOWARN_UNREACHABLE_POP
#define QT_DEPRECATED_VERSION_X_6_0( ... )

/* QGIS-local macros that appear in declaration position. */
#define QHASH_FOR_CLASS_ENUM( ... )
#define FINAL final

/* Opaque forward declarations for the container templates.
 *
 * Non-template types need nothing: clang recovers from `QString name()` and
 * still reports the method.  Templates are different -- with `QMap` undeclared,
 * `QMap<QString, QList<QString>> fullHeaders()` is not a declaration at all but
 * a chain of comparisons, and clang drops it silently
 * (`qgsserverresponse.h:96`).  Declaring the template with the right arity is
 * enough; no definition is needed, because sipify slices types out of the
 * header text and never asks clang what they mean.
 *
 * Anything missing here shows up as a lost declaration, which is what
 * failclosed.py exists to catch.
 */
template<typename T> class QList;
template<typename T> class QVector;
template<typename T> class QSet;
template<typename T> class QStack;
template<typename T> class QQueue;
template<typename T> class QPointer;
template<typename T> class QFuture;
template<typename T> class QFutureWatcher;
template<typename T> class QSharedPointer; // skip-keyword-check
template<typename T> class QWeakPointer;
template<typename T> class QScopedPointer; // skip-keyword-check
template<typename T> class QSharedDataPointer;
template<typename T> class QExplicitlySharedDataPointer;
template<typename T> class QThreadStorage;
template<typename T> class QFlags;
template<typename T> class QObjectUniquePtr;
template<typename T> class QObjectParentUniquePtr;
template<typename K, typename V> class QMap;
template<typename K, typename V> class QHash;
template<typename K, typename V> class QMultiMap;
template<typename K, typename V> class QMultiHash;
template<typename K, typename V> class QCache;
template<typename T1, typename T2> struct QPair;

namespace std
{
  template<typename T> class unique_ptr;
  template<typename T> class shared_ptr;
  template<typename T> class weak_ptr;
  template<typename T> class vector;
  template<typename T> class list;
  template<typename T> class optional;
  template<typename T> class function;
  template<typename T> class numeric_limits;
  template<typename K, typename V> class map;
  template<typename K, typename V> class unordered_map;
  template<typename T1, typename T2> struct pair;
  template<typename... T> class tuple;
} //namespace std

/* Fixed-width integer names, so `int64_t f();` is a declaration and not an
 * error that eats the rest of the class. */
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned long size_t;
typedef long ssize_t;
typedef long ptrdiff_t;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef long Py_ssize_t;
typedef long Py_hash_t;

#endif
