/***************************************************************************
  qgssettingseditorwidgetwrapperimpl.h
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

#ifndef QGSSETTINGSEDITORWIDGETWRAPPERIMPL_H
#define QGSSETTINGSEDITORWIDGETWRAPPERIMPL_H

#include <QColor>

#include "qgis_gui.h"
#include "qgssettingseditorwidgetwrapper.h"
#include "qgslogger.h"

#include "qgssettingsentryimpl.h"
#include "qgscolorbutton.h"
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTableWidget>


//TODO variant map, enum

class QgsColorButton;

/**
 * \ingroup gui
 * \brief This class is a base factory of editor for settings
 *
 * \since QGIS 3.32
 */
template<class T, class V, class U>
class GUI_EXPORT QgsSettingsEditorWidgetWrapperTemplate : public QgsSettingsEditorWidgetWrapper
{
  public:
    QgsSettingsEditorWidgetWrapperTemplate()
      : QgsSettingsEditorWidgetWrapper() {}

    virtual QString id() const override = 0;

    virtual bool setWidgetFromSetting() const override
    {
      if ( mSetting )
        return setWidgetValue( mSetting->value( mDynamicKeyPartList ) );

      QgsDebugMsg( "editor is not configured" );
      return false;
    }

    virtual bool setSettingFromWidget() const override = 0;

    void setWidgetFromVariant( const QVariant &value ) const override
    {
      setWidgetValue( mSetting->convertFromVariant( value ) );
    }

    virtual bool setWidgetValue( const U &value ) const = 0;

    QVariant variantValueFromWidget() const override
    {
      return valueFromWidget();
    };

    virtual U valueFromWidget() const = 0;

    //! Returns the editor
    V *editor() const {return mEditor;}

    //! Returns the setting
    const T *setting() const {return mSetting;}

  protected:
    virtual QWidget *createEditorPrivate( QWidget *parent = nullptr ) override
    {
      return new V( parent );
    }

    bool configureEditorPrivate( QWidget *editor, const QgsSettingsEntryBase *setting ) override
    {
      mSetting = dynamic_cast<const T *>( setting );
      mEditor = qobject_cast<V *>( editor );
      if ( mEditor )
      {
        configureEditorPrivateImplementation();
        return true;
      }
      return false;
    }

    virtual void configureEditorPrivateImplementation() {}

    const T *mSetting = nullptr;
    V *mEditor = nullptr;
};


/**
 * \ingroup gui
 * \brief This class is a factory of editor for string settings
 *
 * \since QGIS 3.32
 */
class GUI_EXPORT QgsSettingsStringEditorWidgetWrapper : public QgsSettingsEditorWidgetWrapperTemplate<QgsSettingsEntryString, QLineEdit, QString>
{
  public:
    QgsSettingsStringEditorWidgetWrapper()
      : QgsSettingsEditorWidgetWrapperTemplate<QgsSettingsEntryString, QLineEdit, QString>() {}

    QString id() const override;

    bool setSettingFromWidget() const override;

    QString valueFromWidget() const override;

    bool setWidgetValue( const QString &value ) const override;
};

/**
 * \ingroup gui
 * \brief This class is a factory of editor for boolean settings
 *
 * \since QGIS 3.32
 */
class GUI_EXPORT QgsSettingsBoolEditorWidgetWrapper : public QgsSettingsEditorWidgetWrapperTemplate<QgsSettingsEntryBool, QCheckBox, bool>
{
  public:
    QgsSettingsBoolEditorWidgetWrapper()
      : QgsSettingsEditorWidgetWrapperTemplate<QgsSettingsEntryBool, QCheckBox, bool>() {}

    QString id() const override;

    bool setSettingFromWidget() const override;

    bool valueFromWidget() const override;

    bool setWidgetValue( const bool &value ) const override;
};

/**
 * \ingroup gui
 * \brief This class is a factory of editor for integer settings
 *
 * \since QGIS 3.32
 */
class GUI_EXPORT QgsSettingsIntegerEditorWidgetWrapper : public QgsSettingsEditorWidgetWrapperTemplate<QgsSettingsEntryInteger, QSpinBox, int>
{
  public:
    QgsSettingsIntegerEditorWidgetWrapper()
      : QgsSettingsEditorWidgetWrapperTemplate<QgsSettingsEntryInteger, QSpinBox, int>() {}

    QString id() const override;

    bool setSettingFromWidget() const override;

    int valueFromWidget() const override;

    bool setWidgetValue( const int &value ) const override;
};


/**
 * \ingroup gui
 * \brief This class is a factory of editor for double settings
 *
 * \since QGIS 3.32
 */
class GUI_EXPORT QgsSettingsDoubleEditorWidgetWrapper : public QgsSettingsEditorWidgetWrapperTemplate<QgsSettingsEntryDouble, QDoubleSpinBox, double>
{
  public:
    QgsSettingsDoubleEditorWidgetWrapper()
      : QgsSettingsEditorWidgetWrapperTemplate<QgsSettingsEntryDouble, QDoubleSpinBox, double>() {}

    QString id() const override;

    bool setSettingFromWidget() const override;

    double valueFromWidget() const override;

    bool setWidgetValue( const double &value ) const override;
};


/**
 * \ingroup gui
 * \brief This class is a factory of editor for color settings
 *
 * \since QGIS 3.32
 */
class GUI_EXPORT QgsSettingsColorEditorWidgetWrapper : public QgsSettingsEditorWidgetWrapperTemplate<QgsSettingsEntryColor, QgsColorButton, QColor>
{
  public:
    QgsSettingsColorEditorWidgetWrapper()
      : QgsSettingsEditorWidgetWrapperTemplate<QgsSettingsEntryColor, QgsColorButton, QColor>() {}

    QString id() const override;

    bool setSettingFromWidget() const override;

    QColor valueFromWidget() const override;

    bool setWidgetValue( const QColor &value ) const override;
};

///**
// * \ingroup gui
// * \brief This class is a factory of editor for boolean settings
// *
// * \since QGIS 3.32
// */
//class GUI_EXPORT QgsSettingsStringListEditorWidgetWrapper : public QgsSettingsEditorWidgetWrapperTemplate<QgsSettingsEntryStringList, QTableWidget, QStringList>
//{
//  public:
//    QgsSettingsStringListEditorWidgetWrapper()
//      : QgsSettingsEditorWidgetWrapperTemplate<QgsSettingsEntryStringList, QTableWidget, QStringList>() {}

//    QString id() const override;

//    bool setWidgetFromSetting() const override;

//    bool setSettingFromWidget() const override;

//    QStringList valueFromWidget() const override;
//};


#endif // QGSSETTINGSEDITORWIDGETWRAPPERIMPL_H
