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

  if ( mMapTools.contains( mapTool->id() ) )
    return;

  mMapTools.insert( mapTool->id(), mapTool );
}

void QgsMapToolShapeRegistry::removeMapTool( const QString &mapToolId )
{
  mMapTools.remove( mapToolId );
}

QgsMapToolShapeAbstract *QgsMapToolShapeRegistry::mapTool(const QString &mapToolId, QgsMapToolCapture* parentTool)
{
  if (!mMapTools.contains(mapToolId))
    return nullptr;

  return mMapTools.value(mapToolId)->factory(parentTool);
}

QStringList QgsMapToolShapeRegistry::mapToolNames() const
{
  return mMapTools.keys();
}

void QgsMapToolShapeRegistry::setupToolbar(QToolBar *toolbar)
{
  // This will fill the tool bar with actions for each shape map tool
  // They are divided by ShapeCategory (circle, rectangle, etc)

  QMap<ShapeCategory, QToolButton*> mainButtons;

  QMap<QString, QgsMapToolShapeMetadata *>::const_iterator it = mMapTools.constBegin();
  for (; it != mMapTools.constEnd(); ++it)
  {
    const QgsMapToolShapeMetadata* metadata = it.value();

    QToolButton *mainButton = nullptr;
    if (!mainButtons.contains(metadata->category()))
    {
    mainButton = new QToolButton( toolbar );
    mainButton->setPopupMode( QToolButton::MenuButtonPopup );
    toolbar->addWidget(mainButton);
    QObject::connect( mainButton, &QToolButton::triggered, [=](QAction *action){
      settingMapToolShapeDefault.setValue( metadata->id(), qgsEnumValueToKey(metadata->category()) );
      mainButton->setDefaultAction(action);
    } );

    mainButtons.insert(metadata->category(), mainButton);
    }
    else
    {
      mainButton = mainButtons[metadata->category()];
    }

    QAction* action = new QAction(metadata->icon(), metadata->name(), mainButton);
    mainButton->addAction(action);
    if ( settingMapToolShapeDefault.value( qgsEnumValueToKey(metadata->category()) ) == metadata->id() )
      mainButton->setDefaultAction(action);

  }
}

