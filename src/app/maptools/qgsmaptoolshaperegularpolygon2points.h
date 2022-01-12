/***************************************************************************
    qgmaptoolregularpolygon2points.h  -  map tool for adding regular
    polygon from 2 points
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

#ifndef QGSMAPTOOLSHAPEREGULARPOLYGON2POINTS_H
#define QGSMAPTOOLSHAPEREGULARPOLYGON2POINTS_H

#include "qgsmaptooladdregularpolygon.h"
#include "qgis_app.h"
#include "qgsmaptoolshaperegistry.h"

class APP_EXPORT Metadata : public QgsMapToolShapeMetadata
{
  public:
    Metadata()
      : QgsMapToolShapeMetadata()
    {}

    static const QString TOOL_ID;

    QString id() const override;
    QString name() const override;
    QIcon icon() const override;
    QgsMapToolShapeAbstract::ShapeCategory category() const override;
    QgsMapToolShapeAbstract *factory( QgsMapToolCapture *parentTool ) const override;
};

class APP_EXPORT QgsMapToolShapeRegularPolygon2Points: public QgsMapToolAddRegularPolygon
{
    Q_OBJECT

  public:
    QgsMapToolShapeRegularPolygon2Points( QgsMapToolCapture *parentTool, QgsMapCanvas *canvas, CaptureMode mode = CaptureLine );
    ~QgsMapToolShapeRegularPolygon2Points() override;

    bool cadCanvasReleaseEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer ) override;
    void cadCanvasMoveEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer ) override;

};

#endif // QGSMAPTOOLSHAPEREGULARPOLYGON2POINTS_H
