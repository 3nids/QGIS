/***************************************************************************
  qgsappdropfeedback.cpp
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

#include "qgsappdropfeedback.h"

#include "qgisapp.h"
#include "qgsdropfeedbackoverlay.h"
#include "qgsdroputils.h"
#include "qgsmimedatautils.h"

#include <QApplication>
#include <QDropEvent>
#include <QFileInfo>
#include <QMainWindow>
#include <QMimeData>
#include <QString>

#include "moc_qgsappdropfeedback.cpp"

/**
 * Returns what to call the dragged data in the message, when a single item is dragged and
 * it has a name worth showing. Naming one of several would be misleading, so nothing is
 * returned then.
 */
static QString draggedItemName( const QMimeData *mimeData )
{
  const QStringList files = QgsDropUtils::files( mimeData );
  if ( files.size() == 1 )
    return QFileInfo( files.at( 0 ) ).fileName();

  if ( files.isEmpty() && QgsMimeDataUtils::isUriList( mimeData ) )
  {
    const QgsMimeDataUtils::UriList uris = QgsMimeDataUtils::decodeUriList( mimeData );
    if ( uris.size() == 1 )
      return uris.at( 0 ).name.isEmpty() ? QFileInfo( uris.at( 0 ).uri ).fileName() : uris.at( 0 ).name;
  }

  return QString();
}

QgsAppDropFeedback::QgsAppDropFeedback( QMainWindow *window )
  : QObject( window )
  , mWindow( window )
  , mOverlay( new QgsDropFeedbackOverlay( window ) )
{
  // the drags this is about are delivered to whichever panel happens to be under the
  // cursor, and never to the window, so there is nowhere narrower to listen
  qApp->installEventFilter( this );
}

bool QgsAppDropFeedback::isInWindow( QObject *watched ) const
{
  const QWidget *widget = qobject_cast<QWidget *>( watched );
  return widget && mWindow && widget->window() == mWindow;
}

bool QgsAppDropFeedback::eventFilter( QObject *watched, QEvent *event )
{
  switch ( event->type() )
  {
    case QEvent::DragEnter:
    case QEvent::DragMove:
      if ( isInWindow( watched ) )
        dragEntered( static_cast<QDragMoveEvent *>( event )->mimeData() );
      break;

    case QEvent::DragLeave:
    case QEvent::Drop:
      if ( isInWindow( watched ) )
        dragMayHaveEnded();
      break;

    default:
      break;
  }

  // only a bystander: the widgets keep deciding what to do with their own drags
  return QObject::eventFilter( watched, event );
}

void QgsAppDropFeedback::dragEntered( const QMimeData *mimeData )
{
  // a neighbouring widget taking the drag over is not the end of it
  mEndPending = false;

  // drag move events arrive with every mouse move, and what the data holds cannot change
  // while the same data is dragged
  if ( mimeData == mDraggedData )
    return;

  // this is another drag, so nothing is known about it yet, however the last one ended
  forgetDrag();

  // only a drag which brings data into QGIS is announced: any other, such as a dock being
  // moved or text from another application, is none of the window's business
  if ( !QgsDropUtils::isDatasetDrag( mimeData ) )
    return;

  mDraggedData = mimeData;
  mPayloadType = QgsDropUtils::payloadType( mimeData, QgisApp::instance()->customDropHandlers() );
  mOverlay->announce( mPayloadType, draggedItemName( mimeData ) );
}

void QgsAppDropFeedback::dragMayHaveEnded()
{
  if ( !mDraggedData )
    return;

  // moving from one panel to the next leaves the first before entering the second, so the
  // end is only certain once the widgets have had their say
  mEndPending = true;
  QMetaObject::invokeMethod( this, &QgsAppDropFeedback::endDragIfNothingTookItOver, Qt::QueuedConnection );
}

void QgsAppDropFeedback::endDragIfNothingTookItOver()
{
  if ( !mEndPending )
    return;

  mEndPending = false;
  forgetDrag();
}

void QgsAppDropFeedback::forgetDrag()
{
  mDraggedData = nullptr;
  mPayloadType = Qgis::DropPayloadType::Unknown;
  mOverlay->clear();
}
