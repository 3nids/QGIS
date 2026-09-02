/***************************************************************************
  qgsappdropfeedback.h
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

#ifndef QGSAPPDROPFEEDBACK_H
#define QGSAPPDROPFEEDBACK_H

#include "qgis.h"
#include "qgis_app.h"

#include <QObject>
#include <QPointer>

class QMainWindow;
class QMimeData;
class QgsDropFeedbackOverlay;

/**
 * \ingroup app
 * \brief Announces over the whole main window what dropping the data being dragged
 * anywhere over it would do.
 *
 * Qt delivers a drag to the innermost widget which accepts drops, so the window itself
 * hears nothing of the drags hovering its panels, and the panel under the cursor has no
 * business speaking for the window: dropping a project over the layer tree closes the
 * whole project, not just that panel. So the drags are watched application wide and the
 * message covers everything.
 */
class APP_EXPORT QgsAppDropFeedback : public QObject
{
    Q_OBJECT

  public:
    //! Constructor for QgsAppDropFeedback, announcing drags over \a window.
    explicit QgsAppDropFeedback( QMainWindow *window );

  protected:
    bool eventFilter( QObject *watched, QEvent *event ) override;

  private:
    //! Returns TRUE if \a watched is a widget of the window the drags are announced over
    bool isInWindow( QObject *watched ) const;

    void dragEntered( const QMimeData *mimeData );
    void dragMayHaveEnded();
    void endDragIfNothingTookItOver();
    //! Stops announcing whatever was being dragged, as nothing is known of it any more
    void forgetDrag();

    QPointer<QMainWindow> mWindow;
    QgsDropFeedbackOverlay *mOverlay = nullptr;

    //! The data being dragged, held to recognize the drag it belongs to, never dereferenced
    const QMimeData *mDraggedData = nullptr;
    Qgis::DropPayloadType mPayloadType = Qgis::DropPayloadType::Unknown;
    //! Set when a drag left a widget, until it is known whether a neighbouring one took it over
    bool mEndPending = false;
};

#endif // QGSAPPDROPFEEDBACK_H
