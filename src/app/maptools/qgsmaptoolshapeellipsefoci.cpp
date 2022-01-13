/***************************************************************************
    qgmaptoolellipsefoci.cpp  -  map tool for adding ellipse
    from foci and a point
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

#include "qgsmaptoolshapeellipsefoci.h"
#include "qgsgeometryrubberband.h"
#include "qgslinestring.h"
#include "qgsmapcanvas.h"
#include "qgspoint.h"
#include "qgsmapmouseevent.h"
#include "qgsmaptoolcapture.h"
#include <memory>
#include "qgsapplication.h"

const QString QgsMapToolShapeEllipseFociMetadata::TOOL_ID = QStringLiteral( "ellipse-from-foci" );

QString QgsMapToolShapeEllipseFociMetadata::id() const
{
  return QgsMapToolShapeEllipseFociMetadata::TOOL_ID;
}

QString QgsMapToolShapeEllipseFociMetadata::name() const
{
  return QObject::tr( "Ellipse from Foci" );
}

QIcon QgsMapToolShapeEllipseFociMetadata::icon() const
{
  return QgsApplication::getThemeIcon( QStringLiteral( "/mActionEllipseFoci.svg" ) );
}

QgsMapToolShapeAbstract::ShapeCategory QgsMapToolShapeEllipseFociMetadata::category() const
{
  return QgsMapToolShapeAbstract::ShapeCategory::Ellipse;
}

QgsMapToolShapeAbstract *QgsMapToolShapeEllipseFociMetadata::factory( QgsMapToolCapture *parentTool ) const
{
  return new QgsMapToolShapeEllipseFoci( parentTool );
}

QgsMapToolShapeEllipseFoci::QgsMapToolShapeEllipseFoci( QgsMapToolCapture *parentTool )
  : QgsMapToolShapeEllipseAbstract( QgsMapToolShapeEllipseFociMetadata::TOOL_ID, parentTool )
{
}

bool QgsMapToolShapeEllipseFoci::cadCanvasReleaseEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer )
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
    if ( mPoints.size() < 2 )
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

void QgsMapToolShapeEllipseFoci::cadCanvasMoveEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer )
{
  const QgsPoint point = mapPoint( *e );

  mSnapIndicator->setMatch( e->mapPointMatch() );

  if ( mTempRubberBand )
  {
    switch ( mPoints.size() )
    {
      case 1:
      {
        std::unique_ptr<QgsLineString> line( new QgsLineString() );
        line->addVertex( mPoints.at( 0 ) );
        line->addVertex( point );
        mTempRubberBand->setGeometry( line.release() );
      }
      break;
      case 2:
      {
        mEllipse = QgsEllipse::fromFoci( mPoints.at( 0 ), mPoints.at( 1 ), point );
        mTempRubberBand->setGeometry( mEllipse.toPolygon() );
      }
      break;
      default:
        break;
    }
  }
}
