/***************************************************************************
    qgmaptoolellipsecenterpoint.cpp  -  map tool for adding ellipse
    from center and a point
    ---------------------
    begin                : July 2017
    copyright            : (C) 2017 by Loïc Bartoletti
    email                : lbartoletti at tuxfamily dot org
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmaptoolshapeellipsecenterpoint.h"
#include "qgsgeometryrubberband.h"
#include "qgsmapcanvas.h"
#include "qgspoint.h"
#include "qgsmapmouseevent.h"
#include "qgsmaptoolcapture.h"
#include "qgsapplication.h"

const QString QgsMapToolShapeEllipseCenterPointMetadata::TOOL_ID = QStringLiteral( "ellipse-center-point" );

QString QgsMapToolShapeEllipseCenterPointMetadata::id() const
{
  return QgsMapToolShapeEllipseCenterPointMetadata::TOOL_ID;
}

QString QgsMapToolShapeEllipseCenterPointMetadata::name() const
{
  return QObject::tr( "Ellipse from center and a point" );
}

QIcon QgsMapToolShapeEllipseCenterPointMetadata::icon() const
{
  return QgsApplication::getThemeIcon( QStringLiteral( "/mActionEllipseCenterPoint.svg" ) );
}

QgsMapToolShapeAbstract::ShapeCategory QgsMapToolShapeEllipseCenterPointMetadata::category() const
{
  return QgsMapToolShapeAbstract::ShapeCategory::Ellipse;
}

QgsMapToolShapeAbstract *QgsMapToolShapeEllipseCenterPointMetadata::factory( QgsMapToolCapture *parentTool ) const
{
  return new QgsMapToolShapeEllipseCenterPoint( parentTool );
}


QgsMapToolShapeEllipseCenterPoint::QgsMapToolShapeEllipseCenterPoint( QgsMapToolCapture *parentTool )
  : QgsMapToolShapeEllipseAbstract( QgsMapToolShapeEllipseCenterPointMetadata::TOOL_ID, parentTool )
{
}

bool QgsMapToolShapeEllipseCenterPoint::cadCanvasReleaseEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer )
{
  const QgsPoint point = mapPoint( *e );

  if ( !currentVectorLayer() )
  {
    notifyNotVectorLayer();
    clean();
    stopCapturing();
    e->ignore();
    return;
  }

  if ( e->button() == Qt::LeftButton )
  {
    if ( mPoints.empty() )
      mPoints.append( point );

    if ( !mTempRubberBand )
    {
      mTempRubberBand = createGeometryRubberBand( mLayerType, true );
      mTempRubberBand->show();
    }
  }
  else if ( e->button() == Qt::RightButton )
  {
    release( e );
  }
}

void QgsMapToolShapeEllipseCenterPoint::cadCanvasMoveEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer )
{
  const QgsPoint point = mapPoint( *e );

  mSnapIndicator->setMatch( e->mapPointMatch() );

  if ( mTempRubberBand )
  {
    mEllipse = QgsEllipse::fromCenterPoint( mPoints.at( 0 ), point );
    mTempRubberBand->setGeometry( mEllipse.toPolygon( segments() ) );
  }
}
