/***************************************************************************
  qgssettingseditorwidgetwrapper.h
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

#ifndef QGSSETTINGSEDITOR_H
#define QGSSETTINGSEDITOR_H

#include <QVariant>

#include "qgis_gui.h"
#include "qgssettingseditorwidgetwrapper.h"

class QLineEdit;

class QgsSettingsEntryBase;
class QgsSettingsEntryInteger;
class QgsSettingsEntryString;


/**
 * \ingroup gui
 * \brief This class is a factory of editor for string settings
 *
 * \since QGIS 3.32
 */
class GUI_EXPORT QgsSettingsStringEditorWidgetWrapper : public QgsSettingsEditorWidgetWrapper
{
  public:
    QgsSettingsStringEditorWidgetWrapper( );

    QString id() const override;

    QWidget *createEditor( const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList = QStringList(), QWidget *parent = nullptr ) override;

    virtual bool configureEditorImplementation( QWidget *editor, const QgsSettingsEntryBase *setting ) override;

    bool setWidgetFromSetting( ) const override;

    bool setSettingFromWidget( ) const override;

  private:
    const QgsSettingsEntryString *mSettingsString = nullptr;
    QLineEdit *mLineEdit = nullptr;
};



#endif // QGSSETTINGSEDITOR_H
