/***************************************************************************
  qgssettingseditorwidgetwrapper.cpp
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


#include "qgssettingseditorwidgetwrapper.h"
#include "qgssettingsentry.h"

#include <QWidget>


QgsSettingsEditorWidgetWrapper *QgsSettingsEditorWidgetWrapper::fromWidget( const QWidget *widget )
{
  QVariant editorDataVariant = widget->property( "SETTING-EDITOR-WIDGET-WRAPPER" );
  if ( editorDataVariant.isValid() )
  {
    return editorDataVariant.value<QgsSettingsEditorWidgetWrapper *>();
  }

  return nullptr;
}

QgsSettingsEditorWidgetWrapper::QgsSettingsEditorWidgetWrapper( QObject *parent )
  : QObject( parent )
{
}

bool QgsSettingsEditorWidgetWrapper::configureEditor( QWidget *editor, const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList )
{
  mSetting = setting;
  mDynamicKeyPartList = dynamicKeyPartList;

  bool ok = configureEditorImplementation( editor, setting );

  if ( ok )
    editor->setProperty( "SETTING-EDITOR-WIDGET-WRAPPER", QVariant::fromValue( this ) );

  return ok;
}
