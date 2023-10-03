/***************************************************************************
    qgsadvanceddigitizingguidesmaptool.h
    ----------------------
    begin                : August 2023
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

#include "qgsadvanceddigitizingguidesmaptool.h"

#include "qgsadvanceddigitizingguidelayer.h"
#include "qgscircle.h"
#include "qgsguiutils.h"
#include "qgsmapcanvas.h"
#include "qgsmapmouseevent.h"
#include "qgsmultipoint.h"
#include "qgsproject.h"
#include "qgsrubberband.h"
#include "qgssettingsregistrycore.h"
#include "qgssettingsentryimpl.h"
#include "qgssnapindicator.h"

#include <QInputDialog>


QgsAdvancedDigitizingGuidesMapTool::QgsAdvancedDigitizingGuidesMapTool( QgsMapCanvas *canvas )
  : QgsMapTool( canvas )
{
  connect( canvas, &QgsMapCanvas::mapToolSet, this, [ = ]( QgsMapTool * tool, QgsMapTool * )
  {
    if ( tool != this )
      mPreviousTool = tool;
  } );
  mPreviousTool = canvas->mapTool();
}

void QgsAdvancedDigitizingGuidesMapTool::keyPressEvent( QKeyEvent *e )
{
  QgsMapTool::keyPressEvent( e );

  if ( e->key() == Qt::Key_Escape )
  {
    restorePreviousMapTool();
  }
}

void QgsAdvancedDigitizingGuidesMapTool::restorePreviousMapTool() const
{
  canvas()->setMapTool( mPreviousTool );
}


QgsConstructionMapToolDistanceToPoints::QgsConstructionMapToolDistanceToPoints( QgsMapCanvas *canvas )
  : QgsAdvancedDigitizingGuidesMapTool( canvas )
{
  mSnapIndicator.reset( new QgsSnapIndicator( canvas ) );
}


void QgsConstructionMapToolDistanceToPoints::deactivate()
{
  mPoints.clear();

  mSnapIndicator->setMatch( QgsPointLocator::Match() );
  mCircleRubberBand->reset( Qgis::GeometryType::Line );
  mCenterRubberBand->reset( Qgis::GeometryType::Point );

  QgsAdvancedDigitizingGuidesMapTool::deactivate();
}

void QgsConstructionMapToolDistanceToPoints::updateRubberband()
{
  if ( !mCircleRubberBand )
  {
    mCircleRubberBand.reset( new QgsRubberBand( mCanvas, Qgis::GeometryType::Line ) );
    QColor color = QgsSettingsRegistryCore::settingsDigitizingLineColor->value();
    mCircleRubberBand->setLineStyle( Qt::DotLine );
    mCircleRubberBand->setStrokeColor( color );
  }
  else
  {
    mCircleRubberBand->reset( Qgis::GeometryType::Line );
  }

  if ( !mCenterRubberBand )
  {
    mCenterRubberBand.reset( new QgsRubberBand( mCanvas, Qgis::GeometryType::Point ) );
    mCenterRubberBand->setIcon( QgsRubberBand::ICON_CROSS );
    //mCenterRubberBand->setWidth( QgsGuiUtils::scaleIconSize( 8 ) );
    mCenterRubberBand->setIconSize( QgsGuiUtils::scaleIconSize( 10 ) );
    mCenterRubberBand->setColor( QgsSettingsRegistryCore::settingsDigitizingLineColor->value() );
  }
  else
  {
    mCenterRubberBand->reset( Qgis::GeometryType::Point );
  }

  for ( const auto &point : std::as_const( mPoints ) )
  {
    mCenterRubberBand->addPoint( point.first );
    QgsCircle *c = new QgsCircle( QgsPoint( point.first ), point.second );
    QgsGeometry g = QgsGeometry( c->toCircularString() );
    mCircleRubberBand->addGeometry( g, QgsProject::instance()->advancedDigitizingGuideLayer()->crs() );
  }
}

QgsPoint QgsConstructionMapToolDistanceToPoints::intersectionSolution( const QgsMapMouseEvent *e ) const
{
  QgsPointXY p1, p2;
  QgsVertexId vertexId;
  QgsGeometryUtils::circleCircleIntersections( mPoints.first().first, mPoints.first().second, mPoints.constLast().first, mPoints.constLast().second, p1, p2 );
  QgsMultiPoint solutions( QVector<QgsPointXY>() << p1 << p2 );
  QgsPoint solution = QgsGeometryUtils::closestVertex( solutions, QgsPoint( e->mapPoint() ), vertexId );
  double distance = solution.distance( QgsPoint( e->mapPoint() ) );
  double tolerance = QgsTolerance::vertexSearchRadius( mCanvas->mapSettings() );
  if ( distance < tolerance )
  {
    return solution;
  }
  else
  {
    return QgsPoint();
  }
}


void QgsConstructionMapToolDistanceToPoints::canvasMoveEvent( QgsMapMouseEvent *e )
{
  e->snapPoint();

  if ( mPoints.count() < 2 )
  {
    mSnapIndicator->setMatch( e->mapPointMatch() );
  }
  else
  {
    QgsPoint solution = intersectionSolution( e );
    if ( !solution.isEmpty() )
    {
      QgsPointLocator::Match match( QgsPointLocator::Type::Vertex, nullptr, QgsFeatureId(), 0, solution );
      mSnapIndicator->setMatch( match );
    }
    else
    {
      mSnapIndicator->setMatch( QgsPointLocator::Match() );
    }
  }
}

void QgsConstructionMapToolDistanceToPoints::canvasReleaseEvent( QgsMapMouseEvent *e )
{
  qDebug() << e->button();
  if ( e->button() == Qt::MouseButton::RightButton )
  {
    mPoints.clear();
  }
  else
  {
    e->snapPoint();
    mSnapIndicator->setMatch( e->mapPointMatch() );

    if ( mPoints.count() < 2 )
    {
      bool ok;
      double d = QInputDialog::getDouble( nullptr, tr( "Distance to point" ),
                                          tr( "Distance:" ), 0, 0, std::numeric_limits<double>::max(), 3, &ok );
      if ( ok )
      {
        QgsPointXY pt = toLayerCoordinates( QgsProject::instance()->advancedDigitizingGuideLayer(), e->mapPoint() );
        mPoints.append( std::pair<QgsPointXY, double>( pt, d ) );
      }
    }
    else
    {
      QgsPoint solution = intersectionSolution( e );
      if ( !solution.isEmpty() )
      {
        QgsProject::instance()->advancedDigitizingGuideLayer()->addPointDistanceToPoints( solution, mPoints );
        mPoints.clear();
        restorePreviousMapTool();
        return;
      }
    }
  }
  mSnapIndicator->setMatch( QgsPointLocator::Match() );
  updateRubberband();
}

