/***************************************************************************
    qgssettingseditorregistry.cpp
    ---------------------
    begin                : April 2023
    copyright            : (C) 2023 by Denis Rouzaud
    email                : denis@opengis.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgssettingseditorfactory.h"

#include "qgslogger.h"
#include "qgssettingseditorregistry.h"
#include "qgssettingsentry.h"

QgsSettingsEditorRegistry::QgsSettingsEditorRegistry()
{

}

QgsSettingsEditorRegistry::~QgsSettingsEditorRegistry()
{
  qDeleteAll( mEditors );
}

bool QgsSettingsEditorRegistry::addEditor( QgsSettingsEditorFactory *editor )
{
  if ( mEditors.contains( editor->id() ) )
    return false;

  mEditors.insert( editor->id(), editor );
  return true;
}

QgsSettingsEditorFactory *QgsSettingsEditorRegistry::editor( const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList )
{
  QgsSettingsEditorFactory *editor = mEditors.value( setting->typeId() );
  if ( editor )
  {
    return editor;
  }
  else
  {
    QgsDebugMsg( QStringLiteral( "Setting editor was not found for '%1', returning the default string editor" ) );
    return new QgsSettingsEditorStringFactory();
  }
}




