/***************************************************************************
    qgsadvanceddigitizingguidelayer.h
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


#ifndef QGSADVANCEDDIGITIZINGGUIDELAYER_H
#define QGSADVANCEDDIGITIZINGGUIDELAYER_H

#include "qgis_core.h"
#include "qgsannotationlayer.h"

#define SIP_NO_FILE

/**
 * \ingroup core
 * @brief The QgsAdvancedDigitizingGuideLayer class holds map guides information saved in the project file.
 * \since QGIS 3.34
 */
class CORE_EXPORT QgsAdvancedDigitizingGuideLayer : public QgsAnnotationLayer
{
    Q_OBJECT
  public:
    //! Constructor
    QgsAdvancedDigitizingGuideLayer( const QString &name, const QgsAnnotationLayer::LayerOptions &options );

    class GuideItem
    {
      public:
        GuideItem( const QString &title, const QString &mainItemUuid, const QList<QString> &additionalItemsUuids = QList<QString>() )
          : mTitle( title ), mUuid( mainItemUuid ), mAdditionalItems( additionalItemsUuids ) {}

      private:
        QString mTitle;
        QString mUuid;
        QList<QString> mAdditionalItems;
    };

    //! Adds a point guide
    void addPointDistanceToPoints( const QgsPoint &point, const QList<std::pair<QgsPointXY, double> > &distances );

  private:
    QString addPoint( const QgsPoint &point );

    QList<GuideItem> mItems;
};

#endif // QGSADVANCEDDIGITIZINGGUIDELAYER_H
