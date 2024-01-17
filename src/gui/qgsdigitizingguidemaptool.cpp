/***************************************************************************
    qgsdigitizingguidemaptool.h
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

#include "qgsdigitizingguidemaptool.h"

#include "qgsannotationlineitem.h"
#include "qgsannotationmarkeritem.h"
#include "qgsannotationpointtextitem.h"
#include "qgscircle.h"
#include "qgsdigitizingguidelayer.h"
#include "qgsgeometry.h"
#include "qgsguiutils.h"
#include "qgsmapcanvas.h"
#include "qgsmapmouseevent.h"
#include "qgsmultipoint.h"
#include "qgsproject.h"
#include "qgsrubberband.h"
#include "qgssettingsentryimpl.h"
#include "qgssettingsregistrycore.h"
#include "qgssnapindicator.h"
#include "qgssnappingutils.h"

#include <QInputDialog>


QgsDigitizingGuideMapTool::QgsDigitizingGuideMapTool( QgsMapCanvas *canvas )
  : QgsMapTool( canvas )
{
  connect( canvas, &QgsMapCanvas::mapToolSet, this, [ = ]( QgsMapTool * tool, QgsMapTool * )
  {
    if ( tool != this )
      mPreviousTool = tool;
  } );
  mPreviousTool = canvas->mapTool();
}

void QgsDigitizingGuideMapTool::keyPressEvent( QKeyEvent *e )
{
  QgsMapTool::keyPressEvent( e );

  if ( e->key() == Qt::Key_Escape )
  {
    restorePreviousMapTool();
  }
}

void QgsDigitizingGuideMapTool::restorePreviousMapTool() const
{
  canvas()->setMapTool( mPreviousTool );
}


QgsDigitizingGuideMapToolDistanceToPoints::QgsDigitizingGuideMapToolDistanceToPoints( QgsMapCanvas *canvas )
  : QgsDigitizingGuideMapTool( canvas )
{
  mSnapIndicator.reset( new QgsSnapIndicator( canvas ) );
}


void QgsDigitizingGuideMapToolDistanceToPoints::deactivate()
{
  mDistances.clear();

  mSnapIndicator->setMatch( QgsPointLocator::Match() );
  mCircleRubberBand->reset( Qgis::GeometryType::Line );
  mCenterRubberBand->reset( Qgis::GeometryType::Point );

  QgsDigitizingGuideMapTool::deactivate();
}

void QgsDigitizingGuideMapToolDistanceToPoints::updateRubberband()
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

  for ( const auto &point : std::as_const( mDistances ) )
  {
    mCenterRubberBand->addPoint( point.first );
    QgsCircle *c = new QgsCircle( QgsPoint( point.first ), point.second );
    QgsGeometry g = QgsGeometry( c->toCircularString() );
    mCircleRubberBand->addGeometry( g, QgsProject::instance()->digitizingGuideLayer()->crs() );
  }
}

QgsPoint QgsDigitizingGuideMapToolDistanceToPoints::intersectionSolution( const QgsMapMouseEvent *e ) const
{
  QgsPointXY p1, p2;
  QgsVertexId vertexId;
  QgsGeometryUtils::circleCircleIntersections( mDistances.first().first, mDistances.first().second, mDistances.constLast().first, mDistances.constLast().second, p1, p2 );
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


void QgsDigitizingGuideMapToolDistanceToPoints::canvasMoveEvent( QgsMapMouseEvent *e )
{
  e->snapPoint();

  if ( mDistances.count() < 2 )
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

void QgsDigitizingGuideMapToolDistanceToPoints::canvasReleaseEvent( QgsMapMouseEvent *e )
{
  if ( e->button() == Qt::MouseButton::RightButton )
  {
    mDistances.clear();
  }
  else
  {
    e->snapPoint();
    mSnapIndicator->setMatch( e->mapPointMatch() );

    if ( mDistances.count() < 2 )
    {
      QInputDialog *d = new QInputDialog;
      //d->setDialogTitle( tr( "Distance to point" ) );
      d->setLabelText( tr( "Distance to point [m]" ) );
      d->setInputMode( QInputDialog::DoubleInput );
      d->setDoubleDecimals( 3 );

      QgsPointXY pt = toLayerCoordinates( QgsProject::instance()->digitizingGuideLayer(), e->mapPoint() );
      mDistances.append( std::pair<QgsPointXY, double>( pt, 0 ) );

      connect( d, &QInputDialog::doubleValueChanged, this, [ = ]( double distance )
      {
        mDistances.last().second = distance;
        updateRubberband();
      } );

      if ( !d->exec() )
      {
        mDistances.removeLast();
      }
    }
    else
    {
      QgsPoint solution = intersectionSolution( e );
      if ( !solution.isEmpty() )
      {
        bool ok = false;
        QString title = QInputDialog::getText( nullptr, tr( "Add Point Giude" ), tr( "Guide Title" ),  QLineEdit::EchoMode::Normal, tr( "Distance to 2 points" ), &ok );
        if ( ok )
        {
          createPointDistanceToPointsGuide( solution, mDistances, title );
        }
        mDistances.clear();
        restorePreviousMapTool();
        return;
      }
    }
  }
  mSnapIndicator->setMatch( QgsPointLocator::Match() );
  updateRubberband();
}

void QgsDigitizingGuideMapToolDistanceToPoints::createPointDistanceToPointsGuide( const QgsPoint &guidePoint, const QList<std::pair<QgsPointXY, double> > &distances, const QString &title )
{
  QgsDigitizingGuideLayer *dl = QgsProject::instance()->digitizingGuideLayer();

  QList<QgsAnnotationItem *> details;
  for ( const auto &distance : distances )
  {
    details << dl->createDetailsPoint( QgsPoint( distance.first ) );

    QgsLineString *line = new QgsLineString( {guidePoint, distance.first} );
    details << dl->createDetailsLine( line );
    details << dl->createDetailsPointTextGuide( QString::number( distance.second, 'f', 3 ), line->centroid(), guidePoint.azimuth( QgsPoint( distance.first ) ) - 90 );
  }

  dl->addPointGuide( guidePoint, title, details );
}


QgsDigitizingGuideMapToolLineExtension::QgsDigitizingGuideMapToolLineExtension( QgsMapCanvas *canvas )
  : QgsDigitizingGuideMapTool( canvas )
{
  mSnapIndicator.reset( new QgsSnapIndicator( canvas ) );
}

void QgsDigitizingGuideMapToolLineExtension::deactivate()
{
  mSnapIndicator->setMatch( QgsPointLocator::Match() );
  mRubberBand->reset( Qgis::GeometryType::Line );

  QgsDigitizingGuideMapTool::deactivate();
}

void QgsDigitizingGuideMapToolLineExtension::canvasMoveEvent( QgsMapMouseEvent *e )
{
  if ( !mSegment )
  {
    EdgesOnlyFilter filter;
    const QgsPointLocator::Match match = canvas()->snappingUtils()->snapToMap( e->mapPoint(), &filter );

    if ( match.isValid() )
    {
      mSnapIndicator->setMatch( match );
    }
    else
    {
      mSnapIndicator->setMatch( QgsPointLocator::Match() );
    }
  }
  else
  {
    if ( !mRubberBand )
    {
      mRubberBand.reset( new QgsRubberBand( mCanvas, Qgis::GeometryType::Line ) );
      QColor color = QgsSettingsRegistryCore::settingsDigitizingLineColor->value();
      mRubberBand->setLineStyle( Qt::DotLine );
      mRubberBand->setStrokeColor( color );
    }
    else
    {
      mRubberBand->reset( Qgis::GeometryType::Line );
    }

    QgsLineString *line = createLine( e->mapPoint() );
    mRubberBand->addGeometry( QgsGeometry( line ), QgsProject::instance()->digitizingGuideLayer()->crs() );
  }
}

void QgsDigitizingGuideMapToolLineExtension::canvasReleaseEvent( QgsMapMouseEvent *e )
{
  if ( e->button() == Qt::MouseButton::RightButton )
  {
    mSegment.reset();
  }
  else
  {
    if ( !mSegment )
    {
      EdgesOnlyFilter filter;
      const QgsPointLocator::Match match = canvas()->snappingUtils()->snapToMap( e->mapPoint(), &filter );

      if ( match.isValid() )
      {
        QgsPointXY p1, p2;
        match.edgePoints( p1, p2 );
        mSegment = {p1, p2};
      }
    }
    else
    {
      QgsLineString *line = createLine( e->mapPoint() );
      bool ok = false;
      if ( line )
      {
        QString title = QInputDialog::getText( nullptr, tr( "Add Point Giude" ), tr( "Guide Title" ),  QLineEdit::EchoMode::Normal, tr( "Line Extension" ), &ok );
        if ( ok )
        {
          QgsDigitizingGuideLayer *dl = QgsProject::instance()->digitizingGuideLayer();
          dl->addLineGuide( line, title );
        }
        else
        {
          delete line;
        }
      }
      restorePreviousMapTool();
      return;
    }
  }
  mSnapIndicator->setMatch( QgsPointLocator::Match() );
}

QgsLineString *QgsDigitizingGuideMapToolLineExtension::createLine( const QgsPointXY &point )
{
  if ( !mSegment )
    return nullptr;

  double d1 = mSegment->first.sqrDist( point );
  double d2 = mSegment->second.sqrDist( point );

  const QgsPointXY &startPoint = d1 < d2 ? mSegment->first : mSegment->second;

  const QgsPointXY endPoint = QgsGeometryUtils::perpendicularSegment( QgsPoint( point ), QgsPoint( mSegment->first ), QgsPoint( mSegment->second ) ).endPoint();

  return new QgsLineString( {startPoint, endPoint} );
}
