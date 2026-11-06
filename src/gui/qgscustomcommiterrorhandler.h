/***************************************************************************
    qgscustomcommiterrorhandler.h
    ---------------------
    begin                : November 2026
    copyright            : (C) 2026 by Denis Rouzaud
    email                : denis at opengis dot ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSCUSTOMCOMMITERRORHANDLER_H
#define QGSCUSTOMCOMMITERRORHANDLER_H

#include "qgis_gui.h"

#include <QObject>
#include <QStringList>

class QgsVectorLayer;

/**
 * \ingroup gui
 * \brief Abstract base class that may be implemented to report commit errors of a vector layer.
 *
 * QGIS reports these errors in its message bar. A handler allows plugins to report them
 * in their own way.
 *
 * \since QGIS 4.6
 */
class GUI_EXPORT QgsCustomCommitErrorHandler : public QObject
{
    Q_OBJECT

  public:
    /**
     * Called when the commit of the changes of \a layer failed with the given \a errors.
     * If TRUE is returned, then the handler has reported the errors and they should not
     * be further reported (e.g. by other QgsCustomCommitErrorHandler, or by QGIS).
     *
     * Reimplemented methods should return FALSE for errors they do not report.
     */
    virtual bool handleCommitErrors( QgsVectorLayer *layer, const QStringList &errors ) = 0;
};

#endif // QGSCUSTOMCOMMITERRORHANDLER_H
