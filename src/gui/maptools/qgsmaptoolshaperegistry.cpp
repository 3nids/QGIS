/***************************************************************************
                         qgsmaptoolshaperegistry.cpp
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

#include "qgsmaptoolshaperegistry.h"
#include "qgsmaptoolshapeabstract.h"

#include <QAction>
#include <QToolBar>
#include <QToolButton>


QgsMapToolShapeRegistry::QgsMapToolShapeRegistry()
{
}

QgsMapToolShapeRegistry::~QgsMapToolShapeRegistry()
{
  qDeleteAll( mMapTools );
  mMapTools.clear();
}

void QgsMapToolShapeRegistry::addMapTool( QgsMapToolShapeMetadata *mapTool )
{
  if ( !mapTool )
    return;

  if ( mapToolMetadata( mapTool->id() ) )
    return;

  mMapTools.append( mapTool );
}

void QgsMapToolShapeRegistry::removeMapTool( const QString &id )
{
  QList<QgsMapToolShapeMetadata *>::iterator it = mMapTools.begin();
  while ( it != mMapTools.end() )
  {
    if ( ( *it )->id() == id )
    {
      mMapTools.erase( it );
    }
    else
    {
      ++it;
    }
  }
}

QgsMapToolShapeMetadata *QgsMapToolShapeRegistry::mapToolMetadata( const QString &id ) const
{
  for ( QgsMapToolShapeMetadata *md : std::as_const( mMapTools ) )
  {
    if ( md->id() == id )
      return md;
  }

  return nullptr;
}

QgsMapToolShapeAbstract *QgsMapToolShapeRegistry::mapTool( const QString &id, QgsMapToolCapture *parentTool ) const
{
  QgsMapToolShapeMetadata *md = mapToolMetadata( id );
  if ( !md )
    return nullptr;

  return md->factory( parentTool );
}

void QgsMapToolShapeRegistry::setupToolbar( QToolBar *toolbar ) const
{
  // This will fill the tool bar with actions for each shape map tool
  // They are divided by ShapeCategory (circle, rectangle, etc)

  QMap<ShapeCategory, QToolButton *> mainButtons;

  QList<QgsMapToolShapeMetadata *>::const_iterator it = mMapTools.constBegin();
  for ( ; it != mMapTools.constEnd(); ++it )
  {
    const QgsMapToolShapeMetadata *metadata = *it;

    QToolButton *mainButton = nullptr;
    if ( !mainButtons.contains( metadata->category() ) )
    {
      mainButton = new QToolButton( toolbar );
      mainButton->setPopupMode( QToolButton::MenuButtonPopup );
      toolbar->addWidget( mainButton );
      QObject::connect( mainButton, &QToolButton::triggered, mainButton, [ = ]( QAction * action )
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
      } );

      mainButtons.insert( metadata->category(), mainButton );
    }
    else
    {
      mainButton = mainButtons[metadata->category()];
    }

    QAction *action = new QAction( metadata->icon(), metadata->name(), mainButton );
    action->setCheckable( true );
    action->setData( metadata->id() );
    mainButton->addAction( action );
    if ( settingMapToolShapeDefaultForShape.value( qgsEnumValueToKey( metadata->category() ) ) == metadata->id() )
      mainButton->setDefaultAction( action );
    if ( settingMapToolShapeCurrent.value() == metadata->id() )
      action->setChecked( true );
  }
}

