/***************************************************************************
  qgssettingseditorwidgetwrapperimpl.cpp
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


#include "qgssettingseditorwidgetwrapperimpl.h"
#include "qgslogger.h"
#include "qgssettingsentryimpl.h"

#include <QLineEdit>
#include <QCheckBox>


// *******
// Bool
// *******

QString QgsSettingsStringEditorWidgetWrapper::id() const
{
  return QString::fromUtf8( sSettingsTypeMetaEnum.valueToKey( static_cast<int>( Qgis::SettingsType::String ) ) );
}

QWidget *QgsSettingsStringEditorWidgetWrapper::createEditorPrivate( QWidget *parent )
{
  QLineEdit *editor = new QLineEdit( parent );
  return editor;
}

bool QgsSettingsStringEditorWidgetWrapper::configureEditorPrivate( QWidget *editor, const QgsSettingsEntryBase *setting )
{
  mSettingsString = dynamic_cast<const QgsSettingsEntryString *>( setting );
  mLineEdit = qobject_cast<QLineEdit *>( editor );
  if ( mLineEdit )
  {
    return true;
  }
  return false;
}

bool QgsSettingsStringEditorWidgetWrapper::setWidgetFromSetting() const
{
  if ( mLineEdit )
  {
    mLineEdit->setText( mSettingsString->value( mDynamicKeyPartList ) );
    return true;
  }
  else
  {
    QgsDebugMsg( QStringLiteral( "Settings editor not set for %1" ).arg( mSetting->definitionKey() ) );
  }
  return false;
}

bool QgsSettingsStringEditorWidgetWrapper::setSettingFromWidget() const
{
  if ( mLineEdit )
  {
    mSettingsString->setValue( mLineEdit->text(), mDynamicKeyPartList );
    return true;
  }
  else
  {
    QgsDebugMsg( QStringLiteral( "Settings editor not set for %1" ).arg( mSetting->definitionKey() ) );
  }
  return false;
}

QVariant QgsSettingsStringEditorWidgetWrapper::valueFromWidget() const
{
  if ( mLineEdit )
  {
    return mLineEdit->text();
  }
  return QVariant();
}


// *******
// Boolean
// *******

QString QgsSettingsBoolEditorWidgetWrapper::id() const
{
  return QString::fromUtf8( sSettingsTypeMetaEnum.valueToKey( static_cast<int>( Qgis::SettingsType::Bool ) ) );
}

QWidget *QgsSettingsBoolEditorWidgetWrapper::createEditorPrivate( QWidget *parent )
{
  QLineEdit *editor = new QLineEdit( parent );
  return editor;
}

bool QgsSettingsBoolEditorWidgetWrapper::configureEditorPrivate( QWidget *editor, const QgsSettingsEntryBase *setting )
{
  mSettingsBool = dynamic_cast<const QgsSettingsEntryBool *>( setting );
  mCheckBox = qobject_cast<QCheckBox *>( editor );
  if ( mCheckBox )
  {
    return true;
  }
  return false;
}

bool QgsSettingsBoolEditorWidgetWrapper::setWidgetFromSetting() const
{
  if ( mCheckBox )
  {
    mCheckBox->setChecked( mSettingsBool->value( mDynamicKeyPartList ) );
    return true;
  }
  else
  {
    QgsDebugMsg( QStringLiteral( "Settings editor not set for %1" ).arg( mSetting->definitionKey() ) );
  }
  return false;
}

bool QgsSettingsBoolEditorWidgetWrapper::setSettingFromWidget() const
{
  if ( mCheckBox )
  {
    mSettingsBool->setValue( mCheckBox->isChecked(), mDynamicKeyPartList );
    return true;
  }
  else
  {
    QgsDebugMsg( QStringLiteral( "Settings editor not set for %1" ).arg( mSetting->definitionKey() ) );
  }
  return false;
}

QVariant QgsSettingsBoolEditorWidgetWrapper::valueFromWidget() const
{
  if ( mCheckBox )
  {
    return mCheckBox->isChecked();
  }
  return QVariant();
}
