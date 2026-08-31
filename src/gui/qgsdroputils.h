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

#include "qgis.h"
#include "qgis_gui.h"
#include "qgis_sip.h"

#include <QPointer>
#include <QStringList>
#include <QVector>

class QMimeData;
class QgsCustomDropHandler;

/**
 * \ingroup gui
 * \brief Inspects the mime data of a drag which brings map data into QGIS.
 *
 * Any consuming widget (main window, layer tree, map canvas)
 * or any custom drop handlers all gets the infos it needs from this helper.
 *
 * payloadType() tells the widget what they will receive.
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

    /**
     * Returns what \a data holds, so that a widget can refuse a payload it has no use
     * for and tell the user what dropping it would do.
     *
     * The \a customHandlers are asked first, through QgsCustomDropHandler::payloadType(),
     * as they recognize payloads no data provider knows about, and they alone can speak
     * for the custom uris QGIS created for them. What is left is matched against QGIS' own
     * document formats and the data providers.
     *
     * A drag which carries several items is reported as the most consequential of them:
     * a project dropped along with a layer replaces the project.
     *
     * Because this runs while data is dragged, files are matched by extension only. One
     * which has no extension, or which is a directory, is reported as
     * Qgis::DropPayloadType::Unknown and left for the drop itself to resolve.
     *
     * Data which no handler claims and which is not a drag of datasets at all is reported
     * as Qgis::DropPayloadType::Unsupported. A widget which also drags its own contents, as
     * the layer tree does when its nodes are reordered, therefore has to tell the two apart
     * with isDatasetDrag() before asking this, or it refuses its own drags.
     */
    static Qgis::DropPayloadType payloadType( const QMimeData *data, const QVector<QPointer<QgsCustomDropHandler>> &customHandlers = QVector<QPointer<QgsCustomDropHandler>>() ) SIP_SKIP;
};

#endif // QGSDROPUTILS_H
