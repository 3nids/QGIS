/***************************************************************************
    qgmaptoolcircle2points.h  -  map tool for adding circle
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

#ifndef QGSMAPTOOLCIRCLE2POINTS_H
#define QGSMAPTOOLCIRCLE2POINTS_H

#include "qgsmaptooladdcircle.h"
#include "qgis_app.h"
#include "qgsmaptoolshaperegistry.h"


class APP_EXPORT QgsMapToolShapeCircle2PointsMetadata : public QgsMapToolShapeMetadata
{
  public:
    QgsMapToolShapeCircleMetadata()
      : QgsMapToolShapeMetadata()
    {}
    QString id() const override;
    QString name() const override;
    QIcon icon() const override;
    QgsMapToolShapeRegistry::ShapeCategory category() const override;
    QgsMapToolShapeAbstract *factory( QgsMapToolCapture *parentTool ) const override;
};

class APP_EXPORT QgsMapToolCircle2Points : public QgsMapToolShapeCircleAbstract
{
  public:
    QgsMapToolCircle2Points( QgsMapToolCapture *parentTool )
      : QgsMapToolShapeCircleAbstract( parentTool )
    {}

    void cadCanvasReleaseEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer ) override;
    void cadCanvasMoveEvent( QgsMapMouseEvent *e ) override;
};

#endif // QGSMAPTOOLCIRCLE2POINTS_H
