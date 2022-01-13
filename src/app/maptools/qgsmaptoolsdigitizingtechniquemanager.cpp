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
#include "qgsspinbox.h"
#include "qgssettingsregistrycore.h"


#include <QAction>
#include <QToolButton>
#include <QMenu>

QgsMapToolsDigitizingTechniqueManager::QgsMapToolsDigitizingTechniqueManager( QgsAppMapTools *mapTools, QObject *parent )
  : QObject( parent )
  , mMapTools( mapTools )
{
  mTechniqueActions.insert( QgsMapToolCapture::CaptureTechnique::StraightSegments, QgisApp::instance()->mActionDigitizeWithSegment );
  mTechniqueActions.insert( QgsMapToolCapture::CaptureTechnique::CircularString, QgisApp::instance()->mActionDigitizeWithCurve );
  mTechniqueActions.insert( QgsMapToolCapture::CaptureTechnique::Streaming, QgisApp::instance()->mActionStreamDigitize );
  mTechniqueActions.insert( QgsMapToolCapture::CaptureTechnique::Shape, QgisApp::instance()->mActionDigitizeShape );
}

void QgsMapToolsDigitizingTechniqueManager::setupCanvasTools()
{
  const QList< QgsMapToolCapture * > captureTools = mMapTools->captureTools();
  for ( QgsMapToolCapture *tool : captureTools )
  {
    connect( tool->action(), &QAction::toggled, this, [this, tool]( bool checked ) {  enableDigitizingTechnique( checked, tool->action() ); } );
  }
}

void QgsMapToolsDigitizingTechniqueManager::setupToolBars()
{
  // digitize mode button

  mDigitizeModeToolButton = new QToolButton();
  mDigitizeModeToolButton->setPopupMode( QToolButton::MenuButtonPopup );

  QMenu *digitizeMenu = new QMenu( mDigitizeModeToolButton );
  QActionGroup *actionGroup = new QActionGroup( digitizeMenu );

  QHash<QgsMapToolCapture::CaptureTechnique, QAction *>::const_iterator it = mTechniqueActions.constBegin();
  for ( ; it != mTechniqueActions.constEnd(); ++ it )
  {
    digitizeMenu->addAction( it.value() );
    actionGroup->addAction( it.value() );
    // TODO revert to qmenu triggered?
    connect( it.value(), &QAction::triggered, this, [ = ]( bool checked )
    {
      Q_UNUSED( checked );
      setCaptureTechnique( it.key() );
    } );
  }
  QgisApp::instance()->mActionStreamDigitize->setShortcut( tr( "R", "Keyboard shortcut: toggle stream digitizing" ) );

// connect( digitizeMenu, &QMenu::triggered, this, &QgsMapToolsDigitizingTechniqueManager::setCaptureTechnique );

  mStreamDigitizingSettingsAction = new QgsStreamDigitizingSettingsAction( QgisApp::instance() );

  digitizeMenu->addSeparator();
  digitizeMenu->addAction( mStreamDigitizingSettingsAction );
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
  QMap<QgsMapToolShapeAbstract::ShapeCategory, QToolButton *> shapeButtons;
  const QList<QgsMapToolShapeMetadata *> mapTools = QgsGui::mapToolShapeRegistry()->mapToolMetadatas();
  for ( const QgsMapToolShapeMetadata *metadata : mapTools )
  {
    QToolButton *shapeButton = nullptr;
    if ( !shapeButtons.contains( metadata->category() ) )
    {
      shapeButton = new QToolButton( QgisApp::instance()->mShapeDigitizeToolBar );
      shapeButton->setPopupMode( QToolButton::MenuButtonPopup );
      shapeButton->setMenu( new QMenu( ) );

      QgisApp::instance()->mShapeDigitizeToolBar->addWidget( shapeButton );
      QObject::connect( shapeButton, &QToolButton::triggered, this, [ = ]( QAction * action ) {shapeActionTriggered( action, shapeButton );} );

      shapeButtons.insert( metadata->category(), shapeButton );
    }
    else
    {
      shapeButton = shapeButtons[metadata->category()];
    }

    QMenu *shapeMenu = shapeButton->menu();
    QAction *action = new QAction( metadata->icon(), metadata->name(), shapeMenu );
    action->setCheckable( true );
    action->setData( metadata->id() );
    shapeMenu->addAction( action );
    if ( settingMapToolShapeDefaultForShape.value( qgsEnumValueToKey( metadata->category() ) ) == metadata->id() )
      shapeButton->setDefaultAction( action );
    if ( settingMapToolShapeCurrent.value() == metadata->id() )
      action->setChecked( true );

    mShapeActions.insert( metadata->id(), action );
  }
}

QgsMapToolsDigitizingTechniqueManager::~QgsMapToolsDigitizingTechniqueManager()
{

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

  // QgisApp::captureTools returns all registered capture tools + the eventual current capture tool
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

void QgsMapToolsDigitizingTechniqueManager::enableDigitizingTechnique( bool enabled, QAction *triggeredFromToolAction )
{
  QgsSettings settings;

  // QgisApp::captureTools returns all registered capture tools + the eventual current capture tool
  const QList< QgsMapToolCapture * > tools = QgisApp::instance()->captureTools();

  const QgsMapToolCapture::CaptureTechnique currentTechnique = settingsDigitizingTechnique.value();
  const QString currentShapeToolId = settingMapToolShapeCurrent.value();

  QSet< QgsMapToolCapture::CaptureTechnique > supportedTechniques;
  if ( enabled )
  {
    for ( QgsMapToolCapture *tool : tools )
    {
      if ( triggeredFromToolAction == tool->action() || ( !triggeredFromToolAction && QgisApp::instance()->mapCanvas()->mapTool() == tool ) )
      {
        for ( QgsMapToolCapture::CaptureTechnique technique : mTechniqueActions.keys() )
        {
          if ( tool->supportsTechnique( technique ) )
            supportedTechniques.insert( technique );
        }
        break;
      }
    }
  }

  QHash<QgsMapToolCapture::CaptureTechnique, QAction *>::const_iterator cit = mTechniqueActions.constBegin();
  for ( ; cit != mTechniqueActions.constEnd(); ++ cit )
  {
    cit.value()->setEnabled( enabled && supportedTechniques.contains( cit.key() ) );
    cit.value()->setChecked( cit.value()->isEnabled() && currentTechnique == cit.key() );
  }
  QHash<QString, QAction *>::const_iterator sit = mShapeActions.constBegin();
  for ( ; sit != mShapeActions.constEnd(); ++ sit )
  {
    sit.value()->setEnabled( enabled && currentTechnique == QgsMapToolCapture::CaptureTechnique::Shape &&  supportedTechniques.contains( QgsMapToolCapture::CaptureTechnique::Shape ) );
    sit.value()->setChecked( sit.value()->isEnabled() && sit.key() == currentShapeToolId );
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

//
// QgsStreamDigitizingSettingsAction
//

QgsStreamDigitizingSettingsAction::QgsStreamDigitizingSettingsAction( QWidget *parent )
  : QWidgetAction( parent )
{
  QGridLayout *gLayout = new QGridLayout();
  gLayout->setContentsMargins( 3, 2, 3, 2 );

  mStreamToleranceSpinBox = new QgsSpinBox();
  mStreamToleranceSpinBox->setSuffix( tr( "px" ) );
  mStreamToleranceSpinBox->setKeyboardTracking( false );
  mStreamToleranceSpinBox->setRange( 1, 200 );
  mStreamToleranceSpinBox->setWrapping( false );
  mStreamToleranceSpinBox->setSingleStep( 1 );
  mStreamToleranceSpinBox->setClearValue( 2 );
  mStreamToleranceSpinBox->setValue( QgsSettingsRegistryCore::settingsDigitizingStreamTolerance.value() );

  QLabel *label = new QLabel( tr( "Streaming Tolerance" ) );
  gLayout->addWidget( label, 1, 0 );
  gLayout->addWidget( mStreamToleranceSpinBox, 1, 1 );
  connect( mStreamToleranceSpinBox, qOverload<int>( &QgsSpinBox::valueChanged ), this, [ = ]( int value )
  {
    QgsSettingsRegistryCore::settingsDigitizingStreamTolerance.setValue( value );
  } );

  QWidget *w = new QWidget( parent );
  w->setLayout( gLayout );
  setDefaultWidget( w );
}

QgsStreamDigitizingSettingsAction::~QgsStreamDigitizingSettingsAction() = default;
