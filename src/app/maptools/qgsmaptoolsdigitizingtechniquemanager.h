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


#include <QObject>


//class QAction;
//class QToolBar;
class QToolButton;
class QgisApp;

#ifdef _MSC_VER
template class CORE_EXPORT QgsSettingsEntryEnumFlag<QgsMapToolCapture::CaptureTechnique> SIP_SKIP;
#endif

class APP_EXPORT QgsMapToolsDigitizingTechniqueManager : public QObject
{
    Q_OBJECT
  public:
    static const inline QgsSettingsEntryEnumFlag<QgsMapToolCapture::CaptureTechnique> settingsDigitizingTechnique = QgsSettingsEntryEnumFlag<QgsMapToolCapture::CaptureTechnique>( QStringLiteral( "UI/digitizeTechnique" ), QgsSettings::NoSection, QgsMapToolCapture::CaptureTechnique::StraightSegments ) SIP_SKIP;

    static const inline QgsSettingsEntryString settingMapToolShapeDefaultForShape = QgsSettingsEntryString( QStringLiteral( "UI/shape-map-tools/%1/default" ), QgsSettings::Gui, QString(), QObject::tr( "Default map tool for given shape category" ) ) SIP_SKIP;
    static const inline QgsSettingsEntryString settingMapToolShapeCurrent = QgsSettingsEntryString( QStringLiteral( "UI/shape-map-tools/current" ), QgsSettings::Gui, QString(), QObject::tr( "Current shape map tool" ) ) SIP_SKIP;

    QgsMapToolsDigitizingTechniqueManager( QgisApp *app );

  private:
    void setCaptureTechnique( QAction *action );
    void shapeActionTriggered( QAction *action );

    /**
     * Enables the action that toggles digitizing with curve
     */
    void enableDigitizeTechniqueActions( bool enable, QAction *triggeredFromToolAction = nullptr );

    QToolButton *mDigitizeModeToolButton = nullptr;

    //    QHash<QString, QAction *> mActions;


};

#endif // QGSMAPTOOLSDIGITIZINGTECHNIQUEMANAGER_H
