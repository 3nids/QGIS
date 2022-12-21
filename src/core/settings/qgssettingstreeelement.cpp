/***************************************************************************
  qgssettingstreeelement.cpp
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

#include "qgssettingstreeelement.h"
#include "qgssettingsentryimpl.h"
#include "qgsexception.h"


QgsSettingsTreeElement::~QgsSettingsTreeElement()
{
  if ( mType != Type::Root )
    mParent->unregisterChildElement( this );

  while ( !mChildrenElements.isEmpty() )
    delete mChildrenElements.takeFirst();
}

QgsSettingsTreeElement *QgsSettingsTreeElement::createRootElement()
{
  QgsSettingsTreeElement *te = new QgsSettingsTreeElement();
  te->mType = Type::Root;
  return te;
}

QgsSettingsTreeElement *QgsSettingsTreeElement::createChildElement( const QString &key )
{
  if ( childElement( key ) )
    throw QgsSettingsException( QObject::tr( "Settings tree element '%1' already holds the key '%2'." ).arg( this->key(), key ) );

  QgsSettingsTreeElement *te = new QgsSettingsTreeElement();
  te->mType = Type::Normal;
  te->init( this, key );
  registerChildElement( te );
  return te;
}

QgsSettingsTreeNamedListElement *QgsSettingsTreeElement::createNamedListElement( const QString &key, const QgsSettingsTreeElement::NamedListOptions &options )
{
  Q_ASSERT( !childElement( key ) );
  QgsSettingsTreeNamedListElement *te = new QgsSettingsTreeNamedListElement();
  te->init( this, key );
  te->initNamedList( options );
  registerChildElement( te );
  return te;
}

QgsSettingsTreeElement *QgsSettingsTreeElement::childElement( const QString &key )
{
  for ( int i = 0; i < mChildrenElements.count(); i++ )
  {
    if ( mChildrenElements[i]->key() == key )
      return mChildrenElements[i];
  }
  return nullptr;
}

void QgsSettingsTreeElement::unregisterChildSetting( QgsSettingsEntryBase *setting, bool deleteSettingValues, const QStringList &parentsNamedEntries )
{
  if ( deleteSettingValues )
    setting->remove( parentsNamedEntries );
  mChildrenSettings.removeAll( setting );
}

void QgsSettingsTreeElement::unregisterChildElement( QgsSettingsTreeElement *element )
{
  mChildrenElements.removeAll( element );
}

QList<QgsSettingsEntryBase *> QgsSettingsTreeElement::childrenSettings() const
{
  return mChildrenSettings;
}

void QgsSettingsTreeElement::init( QgsSettingsTreeElement *parent, const QString &key )
{
  mParent = parent;
  mKey = key;
  mCompleteKey = QString();

  QList<QgsSettingsTreeElement *> parents;
  QgsSettingsTreeElement *te = parent;
  while ( true )
  {
    parents << te;
    if ( te->parent() )
      te = te->parent();
    else
    {
      // TODO handle python?
      Q_ASSERT( te->type() == Type::Root );
      break;
    }
  }

  QList<QgsSettingsTreeElement *>::const_iterator it = parents.constEnd();
  while ( it != parents.constBegin() )
  {
    --it;

    if ( !mCompleteKey.isEmpty() )
      mCompleteKey.append( QStringLiteral( "/" ) );

    if ( !( *it )->key().isEmpty() )
      mCompleteKey.append( QString( "%1/" ).arg( te->key() ) );

    if ( ( *it )->type() == QgsSettingsTreeElement::Type::NamedList )
    {
      mNamedElementsCount++;
      mCompleteKey.append( QString( "%%1/" ).arg( mNamedElementsCount ) );
    }
  }

  mCompleteKey.append( key );
}


void QgsSettingsTreeNamedListElement::initNamedList( const QgsSettingsTreeElement::NamedListOptions &options )
{
  if ( options.testFlag( NamedListOption::CreateCurrentItemSetting ) )
  {
    mSelectedElementSetting = new QgsSettingsEntryString( QStringLiteral( "selected" ), this );
    registerChildSetting( mSelectedElementSetting );
  }
}

QgsSettingsTreeNamedListElement::~QgsSettingsTreeNamedListElement()
{
  if ( mSelectedElementSetting )
    delete mSelectedElementSetting;
}

const QStringList QgsSettingsTreeNamedListElement::entries( const QString &parentEntry ) const
{
  QgsSettings settings;
  if ( parentEntry.isEmpty() )
    settings.beginGroup( completeKey() );
  else
    settings.beginGroup( completeKey().arg( parentEntry ) );

  return settings.childGroups();
}

void QgsSettingsTreeNamedListElement::setSelectedNamedEntryElement( const QString &entry, const QStringList &parentsNamedEntries )
{
  Q_ASSERT( namedElementsCount() == parentsNamedEntries.count() + 1 );
  mSelectedElementSetting->setValue( entry, parentsNamedEntries );
}

QString QgsSettingsTreeNamedListElement::selectedNamedEntryElement( const QStringList &parentsNamedEntries )
{
  Q_ASSERT( mType == Type::NamedList );
  Q_ASSERT( namedElementsCount() == parentsNamedEntries.count() + 1 );
  return mSelectedElementSetting->value( parentsNamedEntries );
}

void QgsSettingsTreeNamedListElement::deleteNamedEntry( const QString &entry, const QStringList &parentsNamedEntries )
{
  Q_ASSERT( mType == Type::NamedList );
  Q_ASSERT( namedElementsCount() == parentsNamedEntries.count() + 1 );
  QString key = completeKey().arg( entry );
  QgsSettings().remove( key );
}


