/***************************************************************************
  qgssettingseditorfactory.cpp
  --------------------------------------
  Date                 : February 2023
  Copyright            : (C) 2023 by Denis Rouzaud
  Email                : denis@opengis.ch
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
#include "qgssettingsentry.h"
#include "qgssettingsentryimpl.h"

#include <QLineEdit>


QgsSettingsEditorFactory::QgsSettingsEditorFactory( )
{
}



QgsSettingsEditorStringFactory::QgsSettingsEditorStringFactory( )
  : QgsSettingsEditorFactory( )
{}

QString QgsSettingsEditorStringFactory::id() const
{
  return QString::fromUtf8( sSettingsTypeMetaEnum.valueToKey( static_cast<int>( Qgis::SettingsType::String ) ) );
}

QWidget *QgsSettingsEditorStringFactory::createEditor( const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList, QWidget *parent ) const
{
  QLineEdit *editor = new QLineEdit( parent );
  configureEditor( editor, setting, dynamicKeyPartList );
  return editor;
}

bool QgsSettingsEditorStringFactory::configureEditor( QWidget *editor, const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList ) const
{
  if ( QLineEdit *le = qobject_cast<QLineEdit *>( editor ) )
  {
    editor->setProperty( "setting", QVariant::fromValue( setting ) );
    editor->setProperty( "dynamic-key-part-list", dynamicKeyPartList );
    return true;
  }
  return false;
}

bool QgsSettingsEditorStringFactory::setWidgetFromSetting() const
{
  if ( mLineEditEditor )
  {
    mLineEditEditor->setText( mSetting->value( mDynamicKeyPartList ) );
    return true;
  }
  else
  {
    QgsDebugMsg( QStringLiteral( "Settings editor not set for %1" ).arg( mSetting->definitionKey() ) );
  }
  return false;
}

bool QgsSettingsEditorStringFactory::setSettingFromWidget() const
{
  if ( mLineEditEditor )
  {
    mSetting->setValue( mLineEditEditor->text(), mDynamicKeyPartList );
    return true;
  }
  else
  {
    QgsDebugMsg( QStringLiteral( "Settings editor not set for %1" ).arg( mSetting->definitionKey() ) );
  }
  return false;
}
