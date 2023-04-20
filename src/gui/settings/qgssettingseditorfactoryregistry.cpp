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

#include "qgssettingseditorfactoryregistry.h"

#include "qgslogger.h"
#include "qgssettingseditorfactory.h"
#include "qgssettingsentry.h"

QgsSettingsEditorFactoryRegistry::QgsSettingsEditorFactoryRegistry()
{

}

QgsSettingsEditorFactoryRegistry::~QgsSettingsEditorFactoryRegistry()
{
  qDeleteAll( mEditors );
}

bool QgsSettingsEditorFactoryRegistry::addFactory( QgsSettingsEditorFactory *factory )
{
  if ( mEditors.contains( factory->id() ) )
    return false;

  mEditors.insert( factory->id(), factory );
  return true;
}

QgsSettingsEditorFactory *QgsSettingsEditorFactoryRegistry::factory( const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList ) const
{
  QgsSettingsEditorFactory *factory = mEditors.value( setting->typeId() );
  if ( factory )
  {
    return factory;
  }
  else
  {
    QgsDebugMsg( QStringLiteral( "Setting factory was not found for '%1', returning the default string factory" ) );
    return new QgsSettingsEditorStringFactory();
  }
}

QWidget *QgsSettingsEditorFactoryRegistry::createEditor(const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList) const
{
  return factory(setting,dynamicKeyPartList )->createEditor(setting,dynamicKeyPartList );
}




