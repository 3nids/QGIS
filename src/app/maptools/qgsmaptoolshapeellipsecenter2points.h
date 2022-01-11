/***************************************************************************
    qgsmaptoolshapeellipsecenter2points.h  -  map tool for adding ellipse
    from center and 2 points
    ---------------------
    begin                : July 2017
    copyright            : (C) 2017 by Loïc Bartoletti
    email                : lbartoletti at tuxfamily dot org
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMAPTOOLSHAPEELLIPSECENTER2POINTS_H
#define QGSMAPTOOLSHAPEELLIPSECENTER2POINTS_H

#include "qgsmaptoolshapeellipseabstract.h"
#include "qgis_app.h"
#include "qgsmaptoolshaperegistry.h"

class APP_EXPORT Metadata : public QgsMapToolShapeMetadata
{
  public:
    Metadata()
      : QgsMapToolShapeMetadata()
    {}
    QString id() const override;
    QString name() const override;
    QIcon icon() const override;
    QgsMapToolShapeRegistry::ShapeCategory category() const override;
    QgsMapToolShapeAbstract *factory( QgsMapToolCapture *parentTool ) const override;
};

class APP_EXPORT QgsMapToolShapeEllipseCenter2Points: public QgsMapToolShapeEllipseAbstract
{
    Q_OBJECT

  public:
    QgsMapToolShapeEllipseCenter2Points( QgsMapToolCapture *parentTool ) : QgsMapToolShapeCircleAbstract(parentTool) {}

    bool cadCanvasReleaseEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer ) override;
    void cadCanvasMoveEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer ) override;
};

#endif // QGSMAPTOOLSHAPEELLIPSECENTER2POINTS_H
