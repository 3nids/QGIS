/***************************************************************************
    qgmaptoolcircle2points.cpp  -  map tool for adding circle
    from 2 points
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

#include "qgsmaptoolshapecircle2points.h"
#include "qgsgeometryrubberband.h"
#include "qgsmapcanvas.h"
#include "qgsmapmouseevent.h"
#include "qgsmaptoolcapture.h"
#include "qgsapplication.h"

const QString QgsMapToolShapeCircle2PointsMetadata::TOOL_ID = QStringLiteral( "circle-from-2-points" );


QString QgsMapToolShapeCircle2PointsMetadata::id() const
{
  return QgsMapToolShapeCircle2PointsMetadata::TOOL_ID;
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


bool QgsMapToolShapeCircle2Points::cadCanvasReleaseEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer )
{
  if ( e->button() == Qt::LeftButton )
  {
    if ( mPoints.isEmpty() )
      mPoints.append( mParentTool->mapPoint( *e ) );

    if ( !mTempRubberBand )
    {
      mTempRubberBand = mParentTool->createGeometryRubberBand( layer->geometryType(), true );
      mTempRubberBand->show();
    }
  }
  else if ( e->button() == Qt::RightButton )
  {
    mPoints.append( mParentTool->mapPoint( *e ) );
    addCircleToParentTool();
    return true;
  }

  return false;
}

void QgsMapToolShapeCircle2Points::cadCanvasMoveEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer )
{
  Q_UNUSED(layer)

  if ( !mTempRubberBand )
    return;

  mCircle = QgsCircle::from2Points( mPoints.at( 0 ), mParentTool->mapPoint( *e ) );
  mTempRubberBand->setGeometry( mCircle.toCircularString( true ) );
}

