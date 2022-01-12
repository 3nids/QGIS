/***************************************************************************
                         qgsmaptoolsdigitizingtechniquemanager.cpp
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

#include "qgsmaptoolsdigitizingtechniquemanager.h"
#include "qgisapp.h"
#include "qgsappmaptools.h"
#include "qgsmaptoolcapture.h"
#include "qgsmaptoolshapeabstract.h"
#include "qgsmaptoolshaperegistry.h"
#include "qgsgui.h"
#include "qgsmapcanvas.h"

#include <QAction>
#include <QToolButton>
#include <QMenu>

QgsMapToolsDigitizingTechniqueManager::QgsMapToolsDigitizingTechniqueManager( QObject *parent )
  : QObject( parent )
{

  mTechniqueActions =
  {
    { QgsMapToolCapture::CaptureTechnique::StraightSegments, QgisApp::instance()->mActionDigitizeWithSegment},
    {QgsMapToolCapture::CaptureTechnique::CircularString, QgisApp::instance()->mActionDigitizeWithCurve},
    {QgsMapToolCapture::CaptureTechnique::Streaming, QgisApp::instance()->mActionStreamDigitize},
    {QgsMapToolCapture::CaptureTechnique::Shape, QgisApp::instance()->mActionDigitizeShape}
  };

  // digitize mode button

  mDigitizeModeToolButton = new QToolButton();
  mDigitizeModeToolButton->setPopupMode( QToolButton::MenuButtonPopup );

  QMenu *digitizeMenu = new QMenu( mDigitizeModeToolButton );
  QActionGroup *actionGroup = new QActionGroup( digitizeMenu );


  for ( const std::pair<QgsMapToolCapture::CaptureTechnique, QAction *> &techniqueAction : std::as_const( mTechniqueActions ) )
  {
    digitizeMenu->addAction( techniqueAction.second );
    actionGroup->addAction( techniqueAction.second );
    // TODO revert to qmenu triggered?
    connect( techniqueAction.second, &QAction::triggered, this, [ = ]( bool checked ) {setCaptureTechnique( techniqueAction.first );} );
  }
  QgisApp::instance()->mActionStreamDigitize->setShortcut( tr( "R", "Keyboard shortcut: toggle stream digitizing" ) );

// connect( digitizeMenu, &QMenu::triggered, this, &QgsMapToolsDigitizingTechniqueManager::setCaptureTechnique );


  digitizeMenu->addSeparator();
  digitizeMenu->addAction( QgisApp::instance()->mMapTools->streamDigitizingSettingsAction() );
  mDigitizeModeToolButton->setMenu( digitizeMenu );


  const QgsMapToolCapture::CaptureTechnique technique = settingsDigitizingTechnique.value();
  switch ( technique )
  {
    case QgsMapToolCapture::CaptureTechnique::StraightSegments:
      mDigitizeModeToolButton->setDefaultAction( QgisApp::instance()->mActionDigitizeWithSegment );
      break;
    case QgsMapToolCapture::CaptureTechnique::CircularString:
      mDigitizeModeToolButton->setDefaultAction( QgisApp::instance()->mActionDigitizeWithCurve );
      break;
    case QgsMapToolCapture::CaptureTechnique::Streaming:
      mDigitizeModeToolButton->setDefaultAction( QgisApp::instance()->mActionStreamDigitize );
      break;
    case QgsMapToolCapture::CaptureTechnique::Shape:
      mDigitizeModeToolButton->setDefaultAction( QgisApp::instance()->mActionDigitizeShape );
      break;
  }

  QgisApp::instance()->mAdvancedDigitizeToolBar->insertWidget( QgisApp::instance()->mAdvancedDigitizeToolBar->actions().at( 0 ), mDigitizeModeToolButton );

  // Digitizing shape tools
  QMap<QgsMapToolShapeAbstract::ShapeCategory, QToolButton *> mainShapeButtons;
  const QList<QgsMapToolShapeMetadata *> mapTools = QgsGui::mapToolShapeRegistry()->mapToolMetadatas();
  for ( const QgsMapToolShapeMetadata *metadata : mapTools )
  {
    QToolButton *mainButton = nullptr;
    if ( !mainShapeButtons.contains( metadata->category() ) )
    {
      mainButton = new QToolButton( QgisApp::instance()->mShapeDigitizeToolBar );
      mainButton->setPopupMode( QToolButton::MenuButtonPopup );
      QgisApp::instance()->mShapeDigitizeToolBar->addWidget( mainButton );
      QObject::connect( mainButton, &QToolButton::triggered, this, [ = ]( QAction * action ) {shapeActionTriggered( action, mainButton );} );

      mainShapeButtons.insert( metadata->category(), mainButton );
    }
    else
    {
      mainButton = mainShapeButtons[metadata->category()];
    }

    QAction *action = new QAction( metadata->icon(), metadata->name(), mainButton );
    action->setCheckable( true );
    action->setData( metadata->id() );
    mainButton->addAction( action );
    if ( settingMapToolShapeDefaultForShape.value( qgsEnumValueToKey( metadata->category() ) ) == metadata->id() )
      mainButton->setDefaultAction( action );
    if ( settingMapToolShapeCurrent.value() == metadata->id() )
      action->setChecked( true );
    //mActions.insert( metadata->id(), action );
  }
}

void QgsMapToolsDigitizingTechniqueManager::setCaptureTechnique( QgsMapToolCapture::CaptureTechnique technique )
{
  switch ( technique )
  {
    case QgsMapToolCapture::StraightSegments:
      mDigitizeModeToolButton->setDefaultAction( QgisApp::instance()->mActionDigitizeWithSegment );
      break;
    case QgsMapToolCapture::CircularString:
      mDigitizeModeToolButton->setDefaultAction( QgisApp::instance()->mActionDigitizeWithCurve );
      break;
    case QgsMapToolCapture::Streaming:
      mDigitizeModeToolButton->setDefaultAction( QgisApp::instance()->mActionStreamDigitize );
      break;
    case QgsMapToolCapture::Shape:
      mDigitizeModeToolButton->setDefaultAction( QgisApp::instance()->mActionDigitizeShape );
      break;
  }

  const QList< QgsMapToolCapture * > tools = QgisApp::instance()->captureTools();
  for ( QgsMapToolCapture *tool : tools )
  {
    if ( tool->supportsTechnique( technique ) )
      tool->setCurrentCaptureTechnique( technique );
  }

  settingsDigitizingTechnique.setValue( technique );

}

void QgsMapToolsDigitizingTechniqueManager::shapeActionTriggered( QAction *action, QToolButton *mainButton )
{
  QString id = action->data().toString();
  const QgsMapToolShapeMetadata *md = QgsGui::mapToolShapeRegistry()->mapToolMetadata( id );
  if ( md )
  {
    settingMapToolShapeDefaultForShape.setValue( md->id(), qgsEnumValueToKey( md->category() ) );
    settingMapToolShapeCurrent.setValue( md->id() );
    mainButton->setDefaultAction( action );
  }
  const QList<QAction *> constActions = mainButton->actions();
  for ( const QAction *mtAction : constActions )
    action->setChecked( mtAction == action );

  setCaptureTechnique( QgsMapToolCapture::Shape );
}

void QgsMapToolsDigitizingTechniqueManager::enableDigitizeTechniqueActions( bool enable, QAction *triggeredFromToolAction )
{
  QgsSettings settings;

  const QList< QgsMapToolCapture * > tools = QgisApp::instance()->captureTools();

  QSet< QgsMapToolCapture::CaptureTechnique > supportedTechniques;
  for ( QgsMapToolCapture *tool : tools )
  {
    if ( triggeredFromToolAction == tool->action() || ( !triggeredFromToolAction && QgisApp::instance()->mapCanvas()->mapTool() == tool ) )
    {
      for ( QgsMapToolCapture::CaptureTechnique technique : { QgsMapToolCapture::CaptureTechnique::StraightSegments, QgsMapToolCapture::CaptureTechnique::CircularString, QgsMapToolCapture::CaptureTechnique::Streaming, QgsMapToolCapture::CaptureTechnique::Shape } )
      {
        if ( tool->supportsTechnique( technique ) )
          supportedTechniques.insert( technique );
      }
      break;
    }
  }

  const QgsMapToolCapture::CaptureTechnique currentTechnique = settings.enumValue( QStringLiteral( "UI/digitizeTechnique" ), QgsMapToolCapture::CaptureTechnique::StraightSegments );

  for ( const auto &techniqueAction : mTechniqueActions )
  {
    techniqueAction.second->setEnabled( enable && supportedTechniques.contains( techniqueAction.first ) );
    techniqueAction.second->setChecked( currentTechnique == techniqueAction.first && techniqueAction.second->isEnabled() );
  }


  for ( QgsMapToolCapture *tool : tools )
  {
    if ( tool->supportsTechnique( currentTechnique ) )
    {
      tool->setCurrentCaptureTechnique( currentTechnique );
      if ( currentTechnique == QgsMapToolCapture::CaptureTechnique::Shape )
      {
        QgsMapToolShapeMetadata *md = QgsGui::mapToolShapeRegistry()->mapToolMetadata( settingMapToolShapeCurrent.value() );
        tool->setCurrentShapeMapTool( md );
      }
    }
  }
}
