/***************************************************************************
    qgssettingsfactoryfactoryregistry.cpp
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

#include "qgssettingseditorwidgetregistry.h"

#include "qgslogger.h"
#include "qgssettingseditorwidgetwrapper.h"
#include "qgssettingsentry.h"

QgsSettingsEditorWidgetRegistry::QgsSettingsEditorWidgetRegistry()
{

}

QgsSettingsEditorWidgetRegistry::~QgsSettingsEditorWidgetRegistry()
{
  qDeleteAll( mEditors );
}

bool QgsSettingsEditorWidgetRegistry::addWrapper( QgsSettingsEditorWidgetWrapper *wrapper )
{
  if ( mEditors.contains( wrapper->id() ) )
    return false;

  mEditors.insert( wrapper->id(), wrapper );
  return true;
}

QgsSettingsEditorWidgetWrapper *QgsSettingsEditorWidgetRegistry::wrapper( const QString &id ) const
{
  QgsSettingsEditorWidgetWrapper *wrapper = mEditors.value( id );
  if ( wrapper )
  {
    return wrapper;
  }
  else
  {
    QgsDebugMsg( QStringLiteral( "Setting factory was not found for '%1', returning the default string factory" ) );
    return new QgsSettingsStringEditorWidgetWrapper();
  }
}

QWidget *QgsSettingsEditorWidgetRegistry::createEditor(const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList, QWidget* parent) const
{
  return wrapper(setting->typeId())->createEditor(setting,dynamicKeyPartList, parent );
}




