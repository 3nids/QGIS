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

class QLineEdit;

class QgsSettingsEntryBase;
class QgsSettingsEntryInteger;
class QgsSettingsEntryString;

/**
 * \ingroup gui
 * \brief Base class for settings editor wrappers
 *
 * \since QGIS 3.32
 */
class GUI_EXPORT QgsSettingsEditorWidgetWrapper
{
  public:
    //! Constructor
    QgsSettingsEditorWidgetWrapper();

    virtual ~QgsSettingsEditorWidgetWrapper() = default;

    /**
     * This id of the type of settings it handles
     * \note This mostly correspond to the content of Qgis::SettingsType but it's a string since custom Python implementation are possible.
     */
    virtual QString id() const = 0;

//! Creates the editor for the given widget
    virtual QWidget *createEditor( const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList = QStringList(), QWidget *parent = nullptr ) const = 0;

    //! Configures the \a editor according the setting
    virtual bool configureEditor( QWidget *editor, const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList = QStringList() ) const;

    virtual bool configureEditorImplementation(QWidget *editor, const QgsSettingsEntryBase *setting) const = 0;

//    virtual bool setWidgetFromSetting( ) const = 0;

//    virtual bool setSettingFromWidget( ) const = 0;

protected:
    class EditorData : public QObject {
    public:
      EditorData( const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList, QObject* parent = nullptr )
        : QObject(parent)
        , mSetting(setting)
        , mDynamicKeyPartList(dynamicKeyPartList)
      {};

      const QgsSettingsEntryBase *mSetting = nullptr;
      const QStringList &mDynamicKeyPartList;
    };

};



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

    QWidget *createEditor( const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList = QStringList(), QWidget *parent = nullptr ) const override;

    virtual bool configureEditorImplementation( QWidget *editor, const QgsSettingsEntryBase *setting ) const override;

//    bool setWidgetFromSetting( ) const override;

//    bool setSettingFromWidget( ) const override;
};



#endif // QGSSETTINGSEDITOR_H
