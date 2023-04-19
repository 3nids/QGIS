/***************************************************************************
  qgssettingseditorfactory.h
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

class QLineEdit;

class QgsSettingsEntryBase;
class QgsSettingsEntryInteger;
class QgsSettingsEntryString;


class GUI_EXPORT QgsSettingsEditorFactory
{
  public:
    QgsSettingsEditorFactory( const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList = QStringList() );

    virtual ~QgsSettingsEditorFactory() = default;

    virtual QString id() const = 0;

    virtual bool setEditor( QWidget *editor ) = 0;

    virtual QWidget *createEditor( QWidget *parent = nullptr ) const = 0;

    virtual bool setWidgetFromSetting( ) const = 0;

    virtual bool setSettingFromWidget( ) const = 0;

  protected:
    QStringList mDynamicKeyPartList;
};


class GUI_EXPORT QgsSettingsEditorString : public QgsSettingsEditorFactory
{
  public:
    QgsSettingsEditorString( const QgsSettingsEntryString *setting, const QStringList &dynamicKeyPartList = QStringList() );

    QString id() const override;

    bool setEditor( QWidget *editor ) override;

    QWidget *createEditor( QWidget *parent = nullptr ) const override;

    bool setWidgetFromSetting( ) const override;

    bool setSettingFromWidget( ) const override;

  private:
    const QgsSettingsEntryString *mSetting = nullptr;
    QLineEdit *mLineEditEditor = nullptr;
};



#endif // QGSSETTINGSEDITOR_H
