/***************************************************************************
    qgsmaptoolshaperegularpolygoncenterpoint.cpp  -  map tool for adding regular
    polygon from center and a point
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

#include "qgsmaptoolshaperegularpolygoncenterpoint.h"
#include "qgsgeometryrubberband.h"
#include "qgsmapcanvas.h"
#include "qgspoint.h"
#include "qgsmapmouseevent.h"
#include "qgsmaptoolcapture.h"
#include "qgsapplication.h"

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

QgsMapToolShapeRegularPolygonCenterPoint::QgsMapToolShapeRegularPolygonCenterPoint( QgsMapToolCapture *parentTool,
    QgsMapCanvas *canvas, CaptureMode mode )
  : QgsMapToolAddRegularPolygon( parentTool, canvas, mode )
{
  mToolName = tr( "Add regular polygon from center and a point " );
}

QgsMapToolShapeRegularPolygonCenterPoint::~QgsMapToolShapeRegularPolygonCenterPoint()
{
  deleteNumberSidesSpinBox();
}

bool QgsMapToolShapeRegularPolygonCenterPoint::cadCanvasReleaseEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer )
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
    if ( mPoints.size() < 1 )
      mPoints.append( point );

    if ( !mPoints.isEmpty() )
    {
      if ( !mTempRubberBand )
      {
        mTempRubberBand = createGeometryRubberBand( mLayerType, true );
        mTempRubberBand->show();

        createNumberSidesSpinBox();
      }
    }
  }
  else if ( e->button() == Qt::RightButton )
  {
    mPoints.append( point );

    release( e );
  }
}

void QgsMapToolShapeRegularPolygonCenterPoint::cadCanvasMoveEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer )
{
  const QgsPoint point = mapPoint( *e );

  mSnapIndicator->setMatch( e->mapPointMatch() );

  if ( mTempRubberBand )
  {
    const QgsRegularPolygon::ConstructionOption option = QgsRegularPolygon::CircumscribedCircle;
    mRegularPolygon = QgsRegularPolygon( mPoints.at( 0 ), point, mNumberSidesSpinBox->value(), option );
    mTempRubberBand->setGeometry( mRegularPolygon.toPolygon() );
  }
}
