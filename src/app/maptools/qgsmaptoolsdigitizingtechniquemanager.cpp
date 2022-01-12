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

#include <QAction>
#include <QToolButton>
#include <QMenu>

QgsMapToolsDigitizingTechniqueManager::QgsMapToolsDigitizingTechniqueManager( QgisApp *app )
  : QObject( app )
{
  mDigitizeModeToolButton = new QToolButton();
  mDigitizeModeToolButton->setPopupMode( QToolButton::MenuButtonPopup );

  QMenu *digitizeMenu = new QMenu( mDigitizeModeToolButton );
  QActionGroup *actionGroup = new QActionGroup( digitizeMenu );

  for ( QAction *techniqueAction : { app->mActionDigitizeWithSegment, app->mActionDigitizeWithCurve, app->mActionStreamDigitize, app->mActionDigitizeShape} )
  {
    digitizeMenu->addAction( techniqueAction );
    actionGroup->addAction( techniqueAction );
  }
  app->mActionStreamDigitize->setShortcut( tr( "R", "Keyboard shortcut: toggle stream digitizing" ) );

  digitizeMenu->addSeparator();
  digitizeMenu->addAction( app->mMapTools->streamDigitizingSettingsAction() );
  mDigitizeModeToolButton->setMenu( digitizeMenu );

  connect( digitizeMenu, &QMenu::triggered, this, &QgsMapToolsDigitizingTechniqueManager::setCaptureTechnique );

  const QgsMapToolCapture::CaptureTechnique technique = settingsDigitizingTechnique.value();
  switch ( technique )
  {
    case QgsMapToolCapture::CaptureTechnique::StraightSegments:
      mDigitizeModeToolButton->setDefaultAction( app->mActionDigitizeWithSegment );
      break;
    case QgsMapToolCapture::CaptureTechnique::CircularString:
      mDigitizeModeToolButton->setDefaultAction( app->mActionDigitizeWithCurve );
      break;
    case QgsMapToolCapture::CaptureTechnique::Streaming:
      mDigitizeModeToolButton->setDefaultAction( app->mActionStreamDigitize );
      break;
    case QgsMapToolCapture::CaptureTechnique::Shape:
      mDigitizeModeToolButton->setDefaultAction( app->mActionDigitizeShape );
      break;
  }

  app->mAdvancedDigitizeToolBar->insertWidget( app->mAdvancedDigitizeToolBar->actions().at( 0 ), mDigitizeModeToolButton );

  // Digitizing shape tools
  QMap<QgsMapToolShapeAbstract::ShapeCategory, QToolButton *> mainShapeButtons;
  const QList<QgsMapToolShapeMetadata *> mapTools = QgsGui::mapToolShapeRegistry()->mapToolMetadatas();
  for ( const QgsMapToolShapeMetadata *metadata : mapTools )
  {
    QToolButton *mainButton = nullptr;
    if ( !mainShapeButtons.contains( metadata->category() ) )
    {
      mainButton = new QToolButton( app->mShapeDigitizeToolBar );
      mainButton->setPopupMode( QToolButton::MenuButtonPopup );
      app->mShapeDigitizeToolBar->addWidget( mainButton );
      QObject::connect( mainButton, &QToolButton::triggered, this, &QgsMapToolsDigitizingTechniqueManager::shapeActionTriggered );

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

void QgsMapToolsDigitizingTechniqueManager::setCaptureTechnique( QAction *action )
{
  QgsMapToolCapture::CaptureTechnique technique = QgsMapToolCapture::CaptureTechnique::StraightSegments;
  if ( action == mActionDigitizeWithCurve )
  {
    technique = QgsMapToolCapture::CaptureTechnique::CircularString;
    mDigitizeModeToolButton->setDefaultAction( mActionDigitizeWithCurve );
  }
  else if ( action == mActionStreamDigitize )
  {
    technique = QgsMapToolCapture::CaptureTechnique::Streaming;
    mDigitizeModeToolButton->setDefaultAction( mActionStreamDigitize );
  }
  else if ( action == mActionDigitizeShape )
  {
    technique = QgsMapToolCapture::CaptureTechnique::Shape;
    mDigitizeModeToolButton->setDefaultAction( mActionDigitizeShape );
  }
  else
  {
    mDigitizeModeToolButton->setDefaultAction( mActionDigitizeWithSegment );
  }

  const QList< QgsMapToolCapture * > tools = captureTools();
  for ( QgsMapToolCapture *tool : tools )
  {
    if ( tool->supportsTechnique( technique ) )
      tool->setCurrentCaptureTechnique( technique );
  }

  QgsSettings().setEnumValue( QStringLiteral( "UI/digitizeTechnique" ), technique );
}

void QgsMapToolsDigitizingTechniqueManager::shapeActionTriggered( QAction *action )
{
  QString id = action->data().toString();
  const QgsMapToolShapeMetadata *md = mapToolMetadata( id );
  if ( md )
  {
    settingMapToolShapeDefaultForShape.setValue( md->id(), qgsEnumValueToKey( md->category() ) );
    settingMapToolShapeCurrent.setValue( md->id() );
    mainButton->setDefaultAction( action );
  }
  const QList<QAction *> constActions = mainButton->actions();
  for ( const QAction *mtAction : constActions )
    action->setChecked( mtAction == action );

  // TODO make shape current technique
}

void QgsMapToolsDigitizingTechniqueManager::enableDigitizeTechniqueActions( bool enable, QAction *triggeredFromToolAction )
{
  if ( !mMapTools )
    return;

  QgsSettings settings;

  const QList< QgsMapToolCapture * > tools = captureTools();

  QSet< QgsMapToolCapture::CaptureTechnique > supportedTechniques;
  for ( QgsMapToolCapture *tool : tools )
  {
    if ( triggeredFromToolAction == tool->action() || ( !triggeredFromToolAction && mMapCanvas->mapTool() == tool ) )
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

  QList<std::pair<QgsMapToolCapture::CaptureTechnique, QAction *>> techniqueActions
  {
    { QgsMapToolCapture::CaptureTechnique::StraightSegments, mActionDigitizeWithSegment},
    {QgsMapToolCapture::CaptureTechnique::CircularString, mActionDigitizeWithCurve},
    {QgsMapToolCapture::CaptureTechnique::Streaming, mActionStreamDigitize},
    {QgsMapToolCapture::CaptureTechnique::Shape, mActionDigitizeShape}
  };
  for ( const auto &techniqueAction : techniqueActions )
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
        QgsMapToolShapeMetadata *md = QgsGui::mapToolShapeRegistry()->mapToolMetadata( QgsMapToolShapeRegistry::settingMapToolShapeCurrent.value() );
        tool->setCurrentShapeMapTool( md );
      }
    }
  }
}
