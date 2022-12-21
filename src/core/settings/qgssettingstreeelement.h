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
#include "qgis_sip.h"
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
class CORE_EXPORT QgsSettingsTreeElement SIP_NODEFAULTCTORS
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

    virtual ~QgsSettingsTreeElement();

    /**
     * Creates a tree root elemenet
     * \note This is not available in Python bindings. Use QgsSettings.createPluginTreeElement instead.
     */
    static QgsSettingsTreeElement *createRootElement() SIP_SKIP;

    //! Creates a normal tree element
    QgsSettingsTreeElement *createChildElement( const QString &key ) SIP_THROW( QgsSettingsException ) SIP_FACTORY;

    /**
     * Creates a named list tree element.
     * This is useful to register group of settings for several named entries (for instance credentials for several named services)
     */
    QgsSettingsTreeNamedListElement *createNamedListElement( const QString &key, const QgsSettingsTreeElement::NamedListOptions &options = QgsSettingsTreeElement::NamedListOptions() ) SIP_THROW( QgsSettingsException ) SIP_FACTORY;

    //! Returns the existing child element if it exists at the given \a key
    QgsSettingsTreeElement *childElement( const QString &key );


    //! Returns the type of element
    Type type() const {return mType;}

    /**
     * Adds a child setting
     * It does not take ownership of the setting
     */
    void registerChildSetting( QgsSettingsEntryBase *setting ) {mChildrenSettings.append( setting );}

    /**
     * Unregisters the child setting
     * \arg deleteSettingValues if TRUE, the values of the settings will also be deleted
     * \arg parentsNamedEntries the list of named entries in the parent named list (if any)
     */
    void unregisterChildSetting( QgsSettingsEntryBase *setting, bool deleteSettingValues = false, const QStringList &parentsNamedEntries = QStringList() );

    //! Unregisters the child tree \a element
    void unregisterChildElement( QgsSettingsTreeElement *element );

    //! Returns the children elements
    QList<QgsSettingsTreeElement *> childrenElements() const {return mChildrenElements;}

    //! Returns the children settings
#ifndef SIP_RUN
    QList<QgsSettingsEntryBase *> childrenSettings() const;
#else
    QList<const QgsSettingsEntryBase *> childrenSettings() const;
    % MethodCode
    // ! Dirty Hack !
    // No idea why SIP is trying to make a list of const when generating the cpp code
    sipRes = new QList<const QgsSettingsEntryBase *>();
    const QList<QgsSettingsEntryBase *> children = sipCpp->childrenSettings();
    for ( const QgsSettingsEntryBase *child : children )
      sipRes->append( child );
    % End
#endif

    //! Returns the parent of the element or nullptr if it does not exists
    QgsSettingsTreeElement *parent() const {return mParent;}

    //! Returns the key of the element (without its parents)
    QString key() const {return mKey;}

    //! Returns the complete key of the element (including its parents)
    QString completeKey() const {return mCompleteKey;}

    //! Returns the number of named elements in the complete key
    int namedElementsCount() const {return mNamedElementsCount;}

  protected:
    //! Adds a child elements
    void registerChildElement( QgsSettingsTreeElement *element ) {mChildrenElements.append( element );}

    void init( QgsSettingsTreeElement *parent, const QString &key );
    Type mType = Type::Root;


  private:

    /**
     * \note This is not available in Python bindings. Use method createElement on an existing tree element.
     * \see QgsSettings.createPluginTreeElement
     */
    QgsSettingsTreeElement() = default SIP_FORCE;

    friend class QgsSettingsTreeNamedListElement;

    QgsSettingsTreeElement *childElementAtKey( const QString &key );

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
class CORE_EXPORT QgsSettingsTreeNamedListElement : public QgsSettingsTreeElement SIP_NODEFAULTCTORS
{
  public:
    virtual ~QgsSettingsTreeNamedListElement();

    //! Init the elements with the specific \a options
    void initNamedList( const QgsSettingsTreeElement::NamedListOptions &options );

    /**
     *  Returns the list of entries
    * \arg parentsNamedEntries the list of named entries in the parent named list (if any)
    */
    const QStringList entries( const QString &parentEntry = QString() ) const;


    /**
     * Sets the selected named entry from the named list element
    * \arg parentsNamedEntries the list of named entries in the parent named list (if any)
     */
    void setSelectedNamedEntryElement( const QString &entry, const QStringList &parentsNamedEntries = QStringList() );

    /**
     * Returns the selected named entry from the named list element
    * \arg parentsNamedEntries the list of named entries in the parent named list (if any)
     */
    QString selectedNamedEntryElement( const QStringList &parentsNamedEntries = QStringList() );

    /**
     * Deletes a named entry from the named list element
     * \arg entry the entry to delete
    * \arg parentsNamedEntries the list of named entries in the parent named list (if any)
     */
    void deleteNamedEntry( const QString &entry, const QStringList &parentsNamedEntries = QStringList() );



  private:
    friend class QgsSettingsTreeElement;

    /**
     * \note This is not available in Python bindings. Use method createNamedListElement on an existing tree element.
     * \see QgsSettings.createPluginTreeElement
     */
    QgsSettingsTreeNamedListElement() = default SIP_FORCE;

    QgsSettingsEntryString *mSelectedElementSetting = nullptr;
};

Q_DECLARE_OPERATORS_FOR_FLAGS( QgsSettingsTreeElement::NamedListOptions )


#endif  // QGSSETTINGSTREEELEMENT_H
