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


#include "qgis_core.h"
#include "qgssettingsregistry.h"
#include "qgssettingsentryimpl.h"


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

    //! Default constructor with no reference of a registry
    QgsSettingsTreeElement() = default;

    ~QgsSettingsTreeElement()
    {
      if ( mCurrentItemSetting )
        delete mCurrentItemSetting;
    }


    //! Creates the root element, with an optional pointer to a settings registry, on which any settings having this root as top parent will be automatically registered into.
    static QgsSettingsTreeElement *createRootElement( QgsSettingsRegistry *registry = nullptr )
    {
      QgsSettingsTreeElement *te = new QgsSettingsTreeElement();
      te->mType = Type::Root;
      te->mRegistryInstance = registry;
      te->init();
      return te;
    };

    //! Creates a normal tree element
    static QgsSettingsTreeElement *createElement( QgsSettingsTreeElement *parent, const QString &key, const NamedListOptions &options = NamedListOptions() )
    {
      Q_ASSERT( parent );
      // TODO handle python
      QgsSettingsTreeElement *te = new QgsSettingsTreeElement();
      te->mType = Type::Normal;
      te->mParent = parent;
      te->mKey = key;
      te->init();
      parent->addChildElement( te );
      if ( options.testFlag( NamedListOption::CreateCurrentItemSetting ) )
        te->mCurrentItemSetting = new QgsSettingsEntryBool( QStringLiteral( "selected" ), te );
      return te;
    }

    /**
     * Creates a named list tree element.
     * This is useful to register group of settings for several named entries (for instance credentials for several named services)
     */
    static QgsSettingsTreeElement *createNamedListElement( QgsSettingsTreeElement *parent, const QString &key )
    {
      Q_ASSERT( parent );
      // TODO handle python
      QgsSettingsTreeElement *te = new QgsSettingsTreeElement();
      te->mType = Type::NamedList;
      te->mParent = parent;
      te->mKey = key;
      te->init();
      parent->addChildElement( te );
      return te;
    };

    //! Returns the type of element
    Type type() const {return mType;}

    //! Adds a child elements
    void addChildElement( const QgsSettingsTreeElement *element ) {mChildrenElements.append( element );}

    //! Adds a child setting
    void addChildSetting( const QgsSettingsEntryBase *setting ) {mChildrenSettings.append( setting );}

    //! Returns the parent of the element or nullptr if it does not exists
    const QgsSettingsTreeElement *parent() const {return mParent;}

    //! Returns the registry which must be set for the top element
    QgsSettingsRegistry *registry() const {return mRegistryInstance;}

    //! Returns the key of the element (without its parents)
    QString key() const {return mKey;}

    //! Returns the complete key of the element (including its parents)
    QString completeKey() const {return mCompleteKey;}

    //! Returns the number of named elements in the complete key
    int namedElementsCount() const {return mNamedElementsCount;}

    /**
     * Delete a named entry from the named list element
     * \note This must not be called on a element which is not a named list
     */
    void deleteNamedEntry( const QString &entry );

    /**
     * Delete a named entry from the named list element
     * \The \a parentEntry is used if the named list is nested in anoter named list
     * \note This must not be called on a element which is not a named list
     */
    void deleteNamedEntry( const QString &entry, const QString &parentEntry );


  private:
    void init();

    QList<const QgsSettingsTreeElement *> mChildrenElements;
    QList<const QgsSettingsEntryBase *> mChildrenSettings;
    Type mType = Type::Root;
    QgsSettingsRegistry *mRegistryInstance = nullptr;
    const QgsSettingsTreeElement *mParent = nullptr;
    const QgsSettingsEntryBool *mCurrentItemSetting = nullptr;

    QString mKey;
    QString mCompleteKey;
    int mNamedElementsCount = 0;
};

#endif  // QGSSETTINGSTREEELEMENT_H
