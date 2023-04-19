/***************************************************************************
  qgssettingseditor.cpp
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


#include "qgssettingseditor.h"
#include "qgslogger.h"
#include "qgssettingsentry.h"
#include "qgssettingsentryimpl.h"

#include <QLineEdit>


QgsSettingsEditor::QgsSettingsEditor( const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList )
  : mDynamicKeyPartList( dynamicKeyPartList )
{
}



QgsSettingsEditorString::QgsSettingsEditorString( const QgsSettingsEntryString *setting, const QStringList &dynamicKeyPartList )
  : QgsSettingsEditor( setting, dynamicKeyPartList )
{}

QString QgsSettingsEditorString::id() const
{
  return QString::fromUtf8( sSettingsTypeMetaEnum.valueToKey( static_cast<int>( Qgis::SettingsType::String ) ) );
}

bool QgsSettingsEditorString::setEditor( QWidget *editor )
{
  if ( QLineEdit *le = qobject_cast<QLineEdit *>( editor ) )
  {
    mLineEditEditor = le;
    return true;
  }
  return false;
}

QWidget *QgsSettingsEditorString::createEditor( QWidget *parent ) const
{
  QLineEdit *editor = new QLineEdit( parent );
  return editor;
}

bool QgsSettingsEditorString::setWidgetFromSetting() const
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

bool QgsSettingsEditorString::setSettingFromWidget() const
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
