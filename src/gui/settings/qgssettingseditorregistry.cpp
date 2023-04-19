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

#include "qgssettingseditor.h"
#include "qgssettingseditorregistry.h"


QgsSettingsEditorRegistry::QgsSettingsEditorRegistry()
{

}

QgsSettingsEditorRegistry::~QgsSettingsEditorRegistry()
{
  qDeleteAll( mEditors );
}

bool QgsSettingsEditorRegistry::addEditor( QgsSettingsEditor *editor )
{
  if ( mEditors.contains( editor->id() ) )
    return false;

  mEditors.insert( editor->id(), editor );
  return true;
}

QgsSettingsEditor *QgsSettingsEditorRegistry::editor( const QString &id )
{
  QgsSettingsEditor *editor = mEditors.value( id, new QgsSettingsEditorString() );
  return editor->clone();
}

QMap<QString, QString> QgsSettingsEditorRegistry::editorNames() const
{
  QMap<QString, QString> editors;
  for ( const QgsSettingsEditor *editor : std::as_const( mEditors ) )
    editors.insert( editor->name(), editor->id() );
  return editors;
}

QIcon QgsSettingsEditorRegistry::icon( const QString &id ) const
{
  QgsSettingsEditor *editor = mEditors.value( id, nullptr );
  if ( editor )
    return editor->icon();
  else
    return QIcon();
}
