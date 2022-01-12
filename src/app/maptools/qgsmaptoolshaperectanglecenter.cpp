/***************************************************************************
    qgsmaptoolshaperectanglecenter.cpp  -  map tool for adding rectangle
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

#include "qgsmaptoolshaperectanglecenter.h"
#include "qgsgeometryrubberband.h"
#include "qgsgeometryutils.h"
#include "qgsmapcanvas.h"
#include "qgslinestring.h"
#include "qgspoint.h"
#include "qgsmapmouseevent.h"
#include "qgsmaptoolcapture.h"
#include "qgsquadrilateral.h"
#include "qgsapplication.h"

#include <memory>

const QString QgsMapToolShapeCircularStringRadiusMetadata::TOOL_ID = QStringLiteral( "circular-string-by-radius" );

QString QgsMapToolShapeCircularStringRadiusMetadata::id() const
{
  return QgsMapToolShapeCircularStringRadiusMetadata::TOOL_ID;
}

QString QgsMapToolShapeCircle2PointsMetadata::name() const
{
  return QObject::tr( "Circle from 2 points" );
}

QIcon QgsMapToolShapeCircle2PointsMetadata::icon() const
{
  return QgsApplication::getThemeIcon( QStringLiteral( "/mActionCircle2Points.svg" ) );
}

QgsMapToolShapeAbstract::ShapeCategory QgsMapToolShapeCircle2PointsMetadata::category() const
{
  return QgsMapToolShapeAbstract::ShapeCategory::Circle;
}

QgsMapToolShapeAbstract *QgsMapToolShapeCircle2PointsMetadata::factory( QgsMapToolCapture *parentTool ) const
{
  return new QgsMapToolShapeCircle2Points( parentTool );
}

QgsMapToolShapeRectangleCenter::QgsMapToolShapeRectangleCenter( QgsMapToolCapture *parentTool,
    QgsMapCanvas *canvas, CaptureMode mode )
  : QgsMapToolAddRectangle( parentTool, canvas, mode )
{
  mToolName = tr( "Add rectangle from center and a point" );
}

bool QgsMapToolShapeRectangleCenter::cadCanvasReleaseEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer )
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
    mPoints.append( point );

    release( e );
  }
}

void QgsMapToolShapeRectangleCenter::cadCanvasMoveEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer )
{
  const QgsPoint point = mapPoint( *e );

  mSnapIndicator->setMatch( e->mapPointMatch() );

  if ( mTempRubberBand )
  {
    switch ( mPoints.size() )
    {
      case 1:
      {

        const double dist = mPoints.at( 0 ).distance( point );
        const double angle = mPoints.at( 0 ).azimuth( point );

        mRectangle = QgsQuadrilateral::rectangleFromExtent( mPoints.at( 0 ).project( -dist, angle ), mPoints.at( 0 ).project( dist, angle ) );
        mTempRubberBand->setGeometry( mRectangle.toPolygon() );

      }
      break;
      default:
        break;
    }
  }
}
