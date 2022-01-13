/***************************************************************************
   qgsmaptoolshaperectangle3points.h  -  map tool for adding rectangle
   from 3 points
   ---------------------
   begin                : September 2017
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

#ifndef QGSMAPTOOLSHAPERECTANGLE3POINTS_H
#define QGSMAPTOOLSHAPERECTANGLE3POINTS_H

#include "qgsmaptoolshaperectangleabstract.h"
#include "qgis_app.h"
#include "qgsmaptoolshaperegistry.h"

class APP_EXPORT QgsMapToolShapeRectangle3PointsMetadata : public QgsMapToolShapeMetadata
{
  public:
    QgsMapToolShapeRectangle3PointsMetadata()
      : QgsMapToolShapeMetadata()
    {}

    static const QString TOOL_ID_PROJECTED;
    static const QString TOOL_ID_DISTANCE;

    QString id() const override;
    QString name() const override;
    QIcon icon() const override;
    QgsMapToolShapeAbstract::ShapeCategory category() const override;
    QgsMapToolShapeAbstract *factory( QgsMapToolCapture *parentTool ) const override;
};

class APP_EXPORT QgsMapToolShapeRectangle3Points: public QgsMapToolShapeRectangleAbstract
{
    Q_OBJECT

  public:
    enum class CreateMode
    {
      Distance,
      Projected,
    };
    Q_ENUM( CreateMode )

    QgsMapToolShapeRectangle3Points( QgsMapToolCapture *parentTool, CreateMode createMode );

    bool cadCanvasReleaseEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer ) override;
    void cadCanvasMoveEvent( QgsMapMouseEvent *e, const QgsVectorLayer *layer ) override;

  private:
    CreateMode mCreateMode;

};

#endif // QGSMAPTOOLSHAPERECTANGLE3POINTS_H
