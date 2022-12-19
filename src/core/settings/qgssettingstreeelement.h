/***************************************************************************
  qgssettingstreeelement.h
  --------------------------------------
  Date                 : December 2022
  Copyright            : (C) 2022 by Denis Rouzaud
  Email                : denis@opengis.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSSETTINGSTREEELEMENT_H
#define QGSSETTINGSTREEELEMENT_H

#include <QObject>

#include "qgis_core.h"
#include "qgssettingsregistry.h"

class QgsSettingsTreeNamedListElement;
class QgsSettingsEntryString;


/**
 * \ingroup core
 * \class QgsSettingsTreeElement
 * \brief QgsSettingsTreeElement is a tree element for the settings registry
 * to help organizing and introspecting the registry.
 * It is either a root element, a normal element or
 * a named list (to store a group of settings under a dynamic named key).
 * The root element holds a pointer to a registry (might be null)
 * to automatically register a settings entry on its creation when a parent is provided.
 *
 * \see QgsSettingsEntryBase
 * \see QgsSettingsRegistry
 *
 * \since QGIS 3.30
 */
class CORE_EXPORT QgsSettingsTreeElement
{
    Q_GADGET

  public:
    enum class Type
    {
      Root,
      Normal,
      NamedList,
    };
    Q_ENUM( Type )

    //! Options for named list elements
    enum class NamedListOption
    {
      CreateCurrentItemSetting,
    };

    Q_ENUM( NamedListOption )
    Q_DECLARE_FLAGS( NamedListOptions, NamedListOption )
    Q_FLAG( NamedListOptions )

    //! Creates a root tree element
    QgsSettingsTreeElement();
#ifdef SIP_RUN
    % MethodCode
    sipCpp = nullptr;
    PyErr_SetString( PyExc_ValueError, QStringLiteral( "From Python, the tree element must be created from QgsSettings.createPluginTreeElement" ).toUtf8().constData() );
    sipIsErr = 1;
    % End
#endif

    virtual ~QgsSettingsTreeElement();

    //! Creates a normal tree element
    QgsSettingsTreeElement *createChildElement( const QString &key );

    /**
     * Creates a named list tree element.
     * This is useful to register group of settings for several named entries (for instance credentials for several named services)
     */
    QgsSettingsTreeNamedListElement *createNamedListElement( const QString &key, const QgsSettingsTreeElement::NamedListOptions &options = QgsSettingsTreeElement::NamedListOptions() );

    //! Returns the type of element
    Type type() const {return mType;}

    //! Adds a child elements
    void addChildElement( QgsSettingsTreeElement *element ) {mChildrenElements.append( element );}

    //! Adds a child setting
    void addChildSetting( QgsSettingsEntryBase *setting ) {mChildrenSettings.append( setting );}

    //! Returns the children elements
    QList<QgsSettingsTreeElement *> childrenElements() const {return mChildrenElements;}

    //! Returns the children settings
    QList<QgsSettingsEntryBase *> childrenSettings() const {return mChildrenSettings;}

    //! Returns the parent of the element or nullptr if it does not exists
    const QgsSettingsTreeElement *parent() const {return mParent;}

    //! Returns the key of the element (without its parents)
    QString key() const {return mKey;}

    //! Returns the complete key of the element (including its parents)
    QString completeKey() const {return mCompleteKey;}

    //! Returns the number of named elements in the complete key
    int namedElementsCount() const {return mNamedElementsCount;}


  protected:
    void init( QgsSettingsTreeElement *parent, const QString &key );
    Type mType = Type::Root;


  private:
    QList<QgsSettingsTreeElement *> mChildrenElements;
    QList<QgsSettingsEntryBase *> mChildrenSettings;
    QgsSettingsTreeElement *mParent = nullptr;

    QString mKey;
    QString mCompleteKey;
    int mNamedElementsCount = 0;
};



/**
 * \ingroup core
 * \class QgsSettingsTreeNamedListElement
 * \brief QgsSettingsTreeNamedListElement is a named list tree element for the settings registry
 * to help organizing and introspecting the registry.
 * the named list element is used to store a group of settings under a dynamic named key.
 *
 * \see QgsSettingsTreeElement
 * \see QgsSettingsEntryBase
 * \see QgsSettingsRegistry
 *
 * \since QGIS 3.30
 */
class CORE_EXPORT QgsSettingsTreeNamedListElement : public QgsSettingsTreeElement
{
  public:

    //! Default constructor with no reference of a registry
    QgsSettingsTreeNamedListElement( const QString &key, QgsSettingsTreeElement *parent, const QgsSettingsTreeElement::NamedListOptions &options = QgsSettingsTreeElement::NamedListOptions() );

    ~QgsSettingsTreeNamedListElement();

    /**
     *  Returns the list of entries
     * \The \a parentEntry is used if the named list is nested in anoter named list
    */
    const QStringList entries( const QString &parentEntry = QString() ) const;


    /**
     * Sets the selected named entry from the named list element
     * \note This must not be called on a element which is not a named list
     */
    void setSelectedNamedEntryElement( const QString &entry );

    /**
     * Sets the selected named entry from the named list element
     * \The \a parentEntry is used if the named list is nested in anoter named list
     * \note This must not be called on a element which is not a named list
     */
    void setSelectedNamedEntryElement( const QString &parentEntry, const QString &entry );

    /**
     * Returns the selected named entry from the named list element
     * \The \a parentEntry is used if the named list is nested in anoter named list
     * \note This must not be called on a element which is not a named list
     */
    QString selectedNamedEntryElement( const QString &parentEntry = QString() );

    /**
     * Deletes a named entry from the named list element
     * \note This must not be called on a element which is not a named list
     */
    void deleteNamedEntry( const QString &entry );

    /**
     * Deletes a named entry from the named list element
     * \The \a parentEntry is used if the named list is nested in anoter named list
     * \note This must not be called on a element which is not a named list
     */
    void deleteNamedEntry( const QString &parentEntry, const QString &entry );


  private:
    QgsSettingsEntryString *mSelectedElementSetting = nullptr;

};

Q_DECLARE_OPERATORS_FOR_FLAGS( QgsSettingsTreeElement::NamedListOptions )


#endif  // QGSSETTINGSTREEELEMENT_H
