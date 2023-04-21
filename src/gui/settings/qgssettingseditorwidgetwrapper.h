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

#ifndef QGSSETTINGSEDITORWIDGETWRAPPER_H
#define QGSSETTINGSEDITORWIDGETWRAPPER_H

#include <QVariant>

#include "qgis_sip.h"
#include "qgis_gui.h"

class QgsSettingsEntryBase;

/**
 * \ingroup gui
 * \brief Base class for settings editor wrappers
 *
 * \since QGIS 3.32
 */
class GUI_EXPORT QgsSettingsEditorWidgetWrapper : public QObject
{
    Q_OBJECT
  public:
    //! Creates a wrapper from the definition stored in a widget created by createEditor()
    static QgsSettingsEditorWidgetWrapper *fromWidget( const QWidget *widget ) SIP_FACTORY;

    //! Constructor
    QgsSettingsEditorWidgetWrapper( QObject *parent = nullptr );

    virtual ~QgsSettingsEditorWidgetWrapper() = default;

    /**
     * This id of the type of settings it handles
     * \note This mostly correspond to the content of Qgis::SettingsType but it's a string since custom Python implementation are possible.
     */
    virtual QString id() const = 0;

//! Creates the editor for the given widget
    virtual QWidget *createEditor( const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList = QStringList(), QWidget *parent = nullptr ) = 0;

    //! Configures the \a editor according the setting
    virtual bool configureEditor( QWidget *editor, const QgsSettingsEntryBase *setting, const QStringList &dynamicKeyPartList = QStringList() );

    virtual bool configureEditorImplementation( QWidget *editor, const QgsSettingsEntryBase *setting ) = 0;

    virtual bool setWidgetFromSetting( ) const = 0;

    virtual bool setSettingFromWidget( ) const = 0;

  protected:
    const QgsSettingsEntryBase *mSetting = nullptr;
    QStringList mDynamicKeyPartList;
};



#endif // QGSSETTINGSEDITORWIDGETWRAPPER_H
