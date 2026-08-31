/***************************************************************************
  qgsdroputils.h
  --------------------------------------
  Date                 : August 2026
  Copyright            : (C) 2026 by Denis Rouzaud
  Email                : denis@opengis.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSDROPUTILS_H
#define QGSDROPUTILS_H

#include "qgis_gui.h"

#include <QStringList>

class QMimeData;

/**
 * \ingroup gui
 * \brief Inspects the mime data of a drag which brings map data into QGIS.
 *
 * Any consuming widget (main window, layer tree, map canvas)
 * or any custom drop handlers all gets the infos it needs from this helper.
 *
 * \since QGIS 4.4
 */
class GUI_EXPORT QgsDropUtils
{
  public:
    /**
     * Returns TRUE if \a data is a drag of datasets into QGIS: local files, or uris
     * coming from within QGIS itself such as browser panel entries.
     *
     * Widgets which accept these alongside drags of their own contents, as the layer
     * tree does when its nodes are reordered, use this to tell the two apart.
     */
    static bool isDatasetDrag( const QMimeData *data );

    /**
     * Returns the local files carried by \a data, in the order they were dragged.
     */
    static QStringList files( const QMimeData *data );

    /**
     * Returns TRUE if \a data carries a custom uri whose provider key is \a providerKey,
     * that is, one QgsCustomDropHandler::handleCustomUriDrop() would be called with.
     *
     * This is a convenience for a QgsCustomDropHandler which accepts the browser entries
     * it created.
     */
    static bool hasCustomUri( const QMimeData *data, const QString &providerKey );

    /**
     * Returns TRUE if \a data carries a local file whose complete suffix is one of
     * \a extensions, given without a leading dot and matched case insensitively.
     *
     * This is a convenience for a QgsCustomDropHandler which accepts a fixed set of file
     * types.
     */
    static bool hasFileExtension( const QMimeData *data, const QStringList &extensions );
};

#endif // QGSDROPUTILS_H
