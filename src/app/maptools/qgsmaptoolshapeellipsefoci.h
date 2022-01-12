/***************************************************************************
    qgmaptoolellipsefoci.h  -  map tool for adding ellipse
    from foci and a point
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

#ifndef QGSMAPTOOLSHAPEELLIPSEFOCI_H
#define QGSMAPTOOLSHAPEELLIPSEFOCI_H

#include "qgsmaptoolshapeellipseabstract.h"
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
    QgsMapToolShapeRegistry::ShapeCategory category() const override;
    QgsMapToolShapeAbstract *factory( QgsMapToolCapture *parentTool ) const override;
};

class APP_EXPORT QgsMapToolShapeEllipseFoci: public QgsMapToolShapeEllipseAbstract
{
    Q_OBJECT

  public:
    QgsMapToolShapeEllipseFoci( QgsMapToolCapture *parentTool );

    bool cadCanvasReleaseEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer ) override;
    void cadCanvasMoveEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer ) override;
};

#endif // QGSMAPTOOLSHAPEELLIPSEFOCI_H
