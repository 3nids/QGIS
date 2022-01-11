/***************************************************************************
    qgsmaptoolshapecircle.cpp
    ---------------------
    begin                : January 2022
    copyright            : (C) Denis Rouzaud
    email                : denis@opengis.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "qgsmaptoolshapecircle.h"
#include "qgsapplication.h"
#include "qgsmaptoolcapturerubberband.h"
#include "qgsmapmouseevent.h"
#include "qgsmaptoolcapture.h"




QString QgsMapToolShapeCircleMetadata::id() const
{
  return QStringLiteral("circle2points");
}

QString QgsMapToolShapeCircleMetadata::name() const
{
  return QObject::tr( "Circle from 2 points" );
}

QIcon QgsMapToolShapeCircleMetadata::icon() const
{
  return QgsApplication::getThemeIcon( QStringLiteral( "/mActionCircle2Points.svg" ) );
}

QgsMapToolShapeRegistry::ShapeCategory QgsMapToolShapeCircleMetadata::category() const
{
  return QgsMapToolShapeRegistry::ShapeCategory::Circle;
}

QgsMapToolShapeAbstract *QgsMapToolShapeCircleMetadata::factory(QgsMapToolCapture* parentTool) const
{
  return new QgsMapToolShapeCircle(parentTool);
}


bool QgsMapToolShapeCircle::cadCanvasReleaseEvent(QgsMapMouseEvent *e, const QgsVectorLayer *layer)
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

    mParentTool->clearCurve();

    std::unique_ptr<QgsCircularString> lineString( mCircle.toCircularString() );

    mParentTool->addCurve( lineString.release() );

    return true;
  }

  return false;
}

void QgsMapToolShapeCircle::cadCanvasMoveEvent(QgsMapMouseEvent *e)
{
  if (mPoints.count() == 0 )
    return;

  mCircle = QgsCircle::from2Points( mPoints.at( 0 ), mParentTool->mapPoint( *e ) );
  mTempRubberBand->setGeometry( mCircle.toCircularString( true ) );
}

void QgsMapToolShapeCircle::clean()
{
  if ( mTempRubberBand )
  {
    delete mTempRubberBand;
    mTempRubberBand = nullptr;
  }

  mPoints.clear();

}
