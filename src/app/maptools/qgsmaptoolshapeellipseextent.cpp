/***************************************************************************
    qgmaptoolellipseextent.cpp  -  map tool for adding ellipse
    from extent
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

#include "qgsmaptoolshapeellipseextent.h"
#include "qgsgeometryrubberband.h"
#include "qgsmapcanvas.h"
#include "qgspoint.h"
#include "qgsgeometryutils.h"
#include "qgslinestring.h"
#include "qgsmapmouseevent.h"
#include "qgsmaptoolcapture.h"
#include "qgsapplication.h"

QString QgsMapToolShapeCircle2PointsMetadata::id() const
{
  return QStringLiteral( "circle2points" );
}

QString QgsMapToolShapeCircle2PointsMetadata::name() const
{
  return QObject::tr( "Circle from 2 points" );
}

QIcon QgsMapToolShapeCircle2PointsMetadata::icon() const
{
  return QgsApplication::getThemeIcon( QStringLiteral( "/mActionCircle2Points.svg" ) );
}

QgsMapToolShapeRegistry::ShapeCategory QgsMapToolShapeCircle2PointsMetadata::category() const
{
  return QgsMapToolShapeRegistry::ShapeCategory::Circle;
}

QgsMapToolShapeAbstract *QgsMapToolShapeCircle2PointsMetadata::factory( QgsMapToolCapture *parentTool ) const
{
  return new QgsMapToolShapeCircle2Points( parentTool );
}


QgsMapToolShapeEllipseExtent::QgsMapToolShapeEllipseExtent( QgsMapToolCapture *parentTool,
    QgsMapCanvas *canvas, CaptureMode mode )
  : QgsMapToolShapeEllipseAbstract( parentTool, canvas, mode )
{
}

bool QgsMapToolShapeEllipseExtent::cadCanvasReleaseEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer )
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

void QgsMapToolShapeEllipseExtent::cadCanvasMoveEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer )
{
  const QgsPoint point = mapPoint( *e );

  mSnapIndicator->setMatch( e->mapPointMatch() );

  if ( mTempRubberBand )
  {
    switch ( mPoints.size() )
    {
      case 1:
      {
        if ( qgsDoubleNear( mCanvas->rotation(), 0.0 ) )
        {
          mEllipse = QgsEllipse::fromExtent( mPoints.at( 0 ), point );
          mTempRubberBand->setGeometry( mEllipse.toPolygon( segments() ) );
        }
        else
        {
          const double dist = mPoints.at( 0 ).distance( point );
          const double angle = mPoints.at( 0 ).azimuth( point );

          mEllipse = QgsEllipse::fromExtent( mPoints.at( 0 ), mPoints.at( 0 ).project( dist, angle ) );
          mTempRubberBand->setGeometry( mEllipse.toPolygon( segments() ) );
        }
      }
      break;
      default:
        break;
    }
  }
}
