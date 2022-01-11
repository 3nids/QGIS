/***************************************************************************
    qgsmaptoolshapecircle.h
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

#ifndef QGSMAPTOOLSHAPECIRCLE_H
#define QGSMAPTOOLSHAPECIRCLE_H

#include "qgsmaptoolshaperegistry.h"
#include "qgsmaptoolshapeabstract.h"
#include "qgscircle.h"

class QgsGeometryRubberBand;

class QgsMapToolShapeCircleMetadata : public QgsMapToolShapeMetadata
{
public:
  QgsMapToolShapeCircleMetadata()
    : QgsMapToolShapeMetadata()
  {}

      QString id() const override;
      QString name() const override;
      QIcon icon() const override;
      QgsMapToolShapeRegistry::ShapeCategory category() const override;
      QgsMapToolShapeAbstract* factory(QgsMapToolCapture *parentTool) const override;
};

class QgsMapToolShapeCircle : public QgsMapToolShapeAbstract
{
public:
  QgsMapToolShapeCircle(QgsMapToolCapture* parentTool)
    : QgsMapToolShapeAbstract(parentTool)
  {}

      bool cadCanvasReleaseEvent(QgsMapMouseEvent *e, const QgsVectorLayer* layer) override;
      void cadCanvasMoveEvent(QgsMapMouseEvent *e) override;

            void clean() override;

private:
      QgsCircle mCircle;

      //! points (in map coordinates)
      QgsPointSequence mPoints;

      //! The rubberband to show the geometry currently working on
      QgsGeometryRubberBand *mTempRubberBand = nullptr;

};

#endif // QGSMAPTOOLSHAPECIRCLE_H
