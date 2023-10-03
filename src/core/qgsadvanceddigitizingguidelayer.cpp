/***************************************************************************
    qgsadvanceddigitizingguidelayer.cpp
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


#include "qgsadvanceddigitizingguidelayer.h"

#include "qgsannotationmarkeritem.h"
#include "qgsmarkersymbollayer.h"
#include "qgsmarkersymbol.h"


QgsAdvancedDigitizingGuideLayer::QgsAdvancedDigitizingGuideLayer( const QString &name, const LayerOptions &options )
  : QgsAnnotationLayer( name, options )
{
}

void QgsAdvancedDigitizingGuideLayer::addPointDistanceToPoints( const QgsPoint &point, const QList<std::pair<QgsPointXY, double>> &distances )
{
  QString uuid = addPoint( point );

  mItems << GuideItem( tr( "Distance to points" ), uuid );
}

QString QgsAdvancedDigitizingGuideLayer::addPoint( const QgsPoint &point )
{
  QgsAnnotationMarkerItem *item = new QgsAnnotationMarkerItem( point );

  QgsSimpleMarkerSymbolLayer *markerSymbolLayer = new QgsSimpleMarkerSymbolLayer( Qgis::MarkerShape::Cross );
  //markerSymbolLayer->setColor( markerColor );

  QgsMarkerSymbol *markerSymbol = new QgsMarkerSymbol( QgsSymbolLayerList() << markerSymbolLayer );
  item->setSymbol( markerSymbol );

  return addItem( item );
}
