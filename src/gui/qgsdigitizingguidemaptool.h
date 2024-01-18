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

#ifndef QGSDIGITIZINGGUIDESMAPTOOL_H
#define QGSDIGITIZINGGUIDESMAPTOOL_H

#include "qgsmaptool.h"
#include "qobjectuniqueptr.h"
#include "qgspointlocator.h"


class QgsSnapIndicator;
class QgsRubberBand;

#define SIP_NO_FILE

/**
 * \ingroup gui
 * @brief The QgsDigitizingGuideMapTool is a base class for map tools drawing map guides
 * \since QGIS 3.34
 */
class GUI_EXPORT QgsDigitizingGuideMapTool : public QgsMapTool
{
    Q_OBJECT
  public:
    //! Constructor
    QgsDigitizingGuideMapTool( QgsMapCanvas *canvas );

    Flags flags() const override {return AllowZoomRect;}
    void keyPressEvent( QKeyEvent *e ) override;

  protected:
    //! Restores the previous map tool on the map canvas
    void restorePreviousMapTool() const;

    struct EdgesOnlyFilter : public QgsPointLocator::MatchFilter
    {
      bool acceptMatch( const QgsPointLocator::Match &m ) override { return m.hasEdge(); }
    };

  private:
    QPointer< QgsMapTool > mPreviousTool;
};


/**
 * \ingroup gui
 * @brief The QgsDigitizingGuideMapToolDistanceToPoints is a map tool to place a point guide at a given distance to 2 points
 * \since QGIS 3.34
 */
class GUI_EXPORT QgsDigitizingGuideMapToolDistanceToPoints : public QgsDigitizingGuideMapTool
{
    Q_OBJECT
  public:
    //! Constructor
    QgsDigitizingGuideMapToolDistanceToPoints( QgsMapCanvas *canvas );

    void canvasMoveEvent( QgsMapMouseEvent *e ) override;
    void canvasReleaseEvent( QgsMapMouseEvent *e ) override;
    void deactivate() override;

  private:
    void createPointDistanceToPointsGuide( const QgsPoint &guidePoint, const QList<std::pair<QgsPointXY, double>> &distances, const QString &title );
    void updateRubberband();
    QgsPoint intersectionSolution( const QgsMapMouseEvent *e ) const;

    QList<std::pair<QgsPointXY, double>> mDistances;
    std::unique_ptr<QgsSnapIndicator> mSnapIndicator;
    QObjectUniquePtr<QgsRubberBand> mCircleRubberBand;
    QObjectUniquePtr<QgsRubberBand> mCenterRubberBand;

};

/**
 * \ingroup gui
 * @brief The QgsDigitizingGuideMapToolLineAbstract is an absctract class for line extension/parallel/perpendicular implementations
 * \since QGIS 3.34
 */
class GUI_EXPORT QgsDigitizingGuideMapToolLineAbstract : public QgsDigitizingGuideMapTool
{
    Q_OBJECT
  public:
    //! Constructor
    QgsDigitizingGuideMapToolLineAbstract( QgsMapCanvas *canvas );

    void canvasMoveEvent( QgsMapMouseEvent *e ) override;
    void canvasReleaseEvent( QgsMapMouseEvent *e ) override;
    void deactivate() override;

  protected:
    std::optional<std::pair<QgsPointXY, QgsPointXY>> mSegment;

  private:

    virtual QgsLineString *createLine( const QgsPointXY &point ) = 0;

    std::unique_ptr<QgsSnapIndicator> mSnapIndicator;
    QObjectUniquePtr<QgsRubberBand> mRubberBand;

};

/**
 * \ingroup gui
 * @brief The QgsDigitizingGuideMapToolLineExtension is a map tool to place a line guide as an extension of a segment
 * \since QGIS 3.34
 */
class GUI_EXPORT QgsDigitizingGuideMapToolLineExtension : public QgsDigitizingGuideMapToolLineAbstract
{
    Q_OBJECT
  public:
    //! Constructor
    QgsDigitizingGuideMapToolLineExtension( QgsMapCanvas *canvas );

  private:
    QgsLineString *createLine( const QgsPointXY &point ) override;
};

/**
 * \ingroup gui
 * @brief The QgsDigitizingGuideMapToolLineParallel is a map tool to place a line guide as a parallel to a segment
 * \since QGIS 3.34
 */
class GUI_EXPORT QgsDigitizingGuideMapToolLineParallel : public QgsDigitizingGuideMapToolLineAbstract
{
    Q_OBJECT
  public:
    //! Constructor
    QgsDigitizingGuideMapToolLineParallel( QgsMapCanvas *canvas );

  private:
    QgsLineString *createLine( const QgsPointXY &point ) override;
};


#endif // QGSDIGITIZINGGUIDESMAPTOOL_H
