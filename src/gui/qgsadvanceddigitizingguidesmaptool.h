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

#ifndef QGSADVANCEDDIGITIZINGGUIDESMAPTOOL_H
#define QGSADVANCEDDIGITIZINGGUIDESMAPTOOL_H

#include "qgsmaptool.h"
#include "qobjectuniqueptr.h"

class QgsSnapIndicator;
class QgsRubberBand;

#define SIP_NO_FILE

/**
 * \ingroup gui
 * @brief The QgsAdvancedDigitizingGuidesMapTool is a base class for map tools drawing map guides
 * \since QGIS 3.34
 */
class GUI_EXPORT QgsAdvancedDigitizingGuidesMapTool : public QgsMapTool
{
    Q_OBJECT
  public:
    //! Constructor
    QgsAdvancedDigitizingGuidesMapTool( QgsMapCanvas *canvas );

    Flags flags() const override {return AllowZoomRect;}
    void keyPressEvent( QKeyEvent *e ) override;

  protected:
    //! Restores the previous map tool on the map canvas
    void restorePreviousMapTool() const;

  private:
    QPointer< QgsMapTool > mPreviousTool;
};


/**
 * \ingroup gui
 * @brief The QgsConstructionMapToolDistanceToPoints is a map tool to place a point guide at a given distance to 2 points
 * \since QGIS 3.34
 */
class GUI_EXPORT QgsConstructionMapToolDistanceToPoints : public QgsAdvancedDigitizingGuidesMapTool
{
    Q_OBJECT
  public:
    //! Constructor
    QgsConstructionMapToolDistanceToPoints( QgsMapCanvas *canvas );

    void canvasMoveEvent( QgsMapMouseEvent *e ) override;
    void canvasReleaseEvent( QgsMapMouseEvent *e ) override;
    void deactivate() override;

  private:
    void updateRubberband();
    QgsPoint intersectionSolution( const QgsMapMouseEvent *e ) const;

    QList<std::pair<QgsPointXY, double>> mPoints;
    std::unique_ptr<QgsSnapIndicator> mSnapIndicator;
    QObjectUniquePtr<QgsRubberBand> mCircleRubberBand;
    QObjectUniquePtr<QgsRubberBand> mCenterRubberBand;

};



#endif // QGSADVANCEDDIGITIZINGGUIDESMAPTOOL_H
