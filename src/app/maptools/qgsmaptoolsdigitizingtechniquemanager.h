/***************************************************************************
                         qgsmaptoolsdigitizingtechniquemanager.h
                         ----------------------
    begin                : January 2022
    copyright            : (C) 2022 by Denis Rouzaud
    email                : denis@opengis.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMAPTOOLSDIGITIZINGTECHNIQUEMANAGER_H
#define QGSMAPTOOLSDIGITIZINGTECHNIQUEMANAGER_H

#include "qgis_app.h"
#include "qgssettingsentry.h"
#include "qgsmaptoolcapture.h"
#include "qgsmaptoolshapeabstract.h"

#include <QWidgetAction>


class QgsSpinBox;
class QgsAppMapTools;

class QAction;
class QToolButton;


class APP_EXPORT QgsStreamDigitizingSettingsAction: public QWidgetAction
{
    Q_OBJECT

  public:

    QgsStreamDigitizingSettingsAction( QWidget *parent = nullptr );
    ~QgsStreamDigitizingSettingsAction() override;

  private:
    QgsSpinBox *mStreamToleranceSpinBox = nullptr;
};

class APP_EXPORT QgsMapToolsDigitizingTechniqueManager : public QObject
{
    Q_OBJECT
  public:
#ifdef _MSC_VER
    static const inline  QgsSettingsEntryInteger settingsDigitizingTechnique = QgsSettingsEntryInteger( QStringLiteral( "digitizingTechnique" ), QgsSettings::App, static_cast<int>( QgsMapToolCapture::CaptureTechnique::StraightSegments ) ) SIP_SKIP;
#else
    static const inline  QgsSettingsEntryEnumFlag<QgsMapToolCapture::CaptureTechnique> settingsDigitizingTechnique = QgsSettingsEntryEnumFlag<QgsMapToolCapture::CaptureTechnique>( QStringLiteral( "digitizingTechnique" ), QgsSettings::App, QgsMapToolCapture::CaptureTechnique::StraightSegments ) SIP_SKIP;
#endif

    static const inline QgsSettingsEntryString settingMapToolShapeDefaultForShape = QgsSettingsEntryString( QStringLiteral( "shape-map-tools/%1/default" ), QgsSettings::App, QString(), QObject::tr( "Default map tool for given shape category" ) ) SIP_SKIP;
    static const inline QgsSettingsEntryString settingMapToolShapeCurrent = QgsSettingsEntryString( QStringLiteral( "shape-map-tools/current" ), QgsSettings::App, QString(), QObject::tr( "Current shape map tool" ) ) SIP_SKIP;

    QgsMapToolsDigitizingTechniqueManager( QgsAppMapTools *mapTools, QObject *parent );
    ~QgsMapToolsDigitizingTechniqueManager();
    void setupToolBars();
    void setupCanvasTools();

  public slots:
    void enableDigitizingTechniqueActions( bool enabled, QAction *triggeredFromToolAction = nullptr );


  private slots:
    void setCaptureTechnique( QgsMapToolCapture::CaptureTechnique technique, bool alsoSetShapeTool = true );
    void setShapeTool( const QString &shapeToolId );

  private:
    QgsAppMapTools *mMapTools = nullptr;

    QMap<QgsMapToolCapture::CaptureTechnique, QAction *> mTechniqueActions;
    QHash<QString, QAction *> mShapeActions;
    QMap<QgsMapToolShapeAbstract::ShapeCategory, QToolButton *> mShapeCategoryButtons;

    QToolButton *mDigitizeModeToolButton = nullptr;
    QgsStreamDigitizingSettingsAction *mStreamDigitizingSettingsAction = nullptr;


};

#endif // QGSMAPTOOLSDIGITIZINGTECHNIQUEMANAGER_H
