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

QgsSettingsTreeElement::QgsSettingsTreeElement()
  : mType( Type::Root )
{
}

QgsSettingsTreeElement::~QgsSettingsTreeElement()
{
}

void QgsSettingsTreeElement::init( QgsSettingsTreeElement *parent, const QString &key )
{
  mKey = key;
  mCompleteKey = QString();

  QList<const QgsSettingsTreeElement *> parents;
  const QgsSettingsTreeElement *te = parent;
  while ( true )
  {
    parents << te;
    if ( te->parent() )
      te = te->parent();
    else
    {
      // TODO habndle python?
      Q_ASSERT( te->type() == Type::Root );
      break;
    }
  }

  QList<const QgsSettingsTreeElement *>::const_iterator it = parents.constEnd();
  while ( it != parents.constBegin() )
  {
    --it;

    if ( !mCompleteKey.isEmpty() )
      mCompleteKey.append( QStringLiteral( "/" ) );

    if ( !te->key().isEmpty() )
      mCompleteKey.append( QString( "%1/" ).arg( te->key() ) );

    if ( te->type() == QgsSettingsTreeElement::Type::NamedList )
    {
      mNamedElementsCount++;
      mCompleteKey.append( QString( "%%1/" ).arg( mNamedElementsCount ) );
    }
  }

  mCompleteKey.append( key );
}

QgsSettingsTreeElement *QgsSettingsTreeElement::createChildElement( const QString &key )
{
  QgsSettingsTreeElement *te = new QgsSettingsTreeElement();
  te->mType = Type::Normal;
  te->init( this, key );
  addChildElement( te );
  return te;
}

QgsSettingsTreeNamedListElement *QgsSettingsTreeElement::createNamedListElement( const QString &key, const QgsSettingsTreeElement::NamedListOptions &options )
{
  QgsSettingsTreeNamedListElement *te = new QgsSettingsTreeNamedListElement( key, this, options );
  addChildElement( te );
  return te;
}


QgsSettingsTreeNamedListElement::QgsSettingsTreeNamedListElement( const QString &key, QgsSettingsTreeElement *parent, const QgsSettingsTreeElement::NamedListOptions &options )
{
  mType = QgsSettingsTreeElement::Type::NamedList;
  init( parent, key );
  if ( options.testFlag( NamedListOption::CreateCurrentItemSetting ) )
  {
    mSelectedElementSetting = new QgsSettingsEntryString( QStringLiteral( "selected" ), this );
    addChildSetting( mSelectedElementSetting );
  }
  parent->addChildElement( this );
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

void QgsSettingsTreeNamedListElement::setSelectedNamedEntryElement( const QString &entry )
{
  Q_ASSERT( mType == Type::NamedList );
  Q_ASSERT( namedElementsCount() == 1 );
  mSelectedElementSetting->setValue( entry );
}

void QgsSettingsTreeNamedListElement::setSelectedNamedEntryElement( const QString &parentEntry, const QString &entry )
{
  Q_ASSERT( mType == Type::NamedList );
  Q_ASSERT( namedElementsCount() == 2 );
  mSelectedElementSetting->setValue( entry, parentEntry );
}

QString QgsSettingsTreeNamedListElement::selectedNamedEntryElement( const QString &parentEntry )
{
  Q_ASSERT( mType == Type::NamedList );
  if ( parentEntry.isEmpty() )
  {
    Q_ASSERT( namedElementsCount() == 1 );
    return mSelectedElementSetting->value();
  }
  else
  {
    Q_ASSERT( namedElementsCount() == 2 );
    return mSelectedElementSetting->value( parentEntry );
  }
}

void QgsSettingsTreeNamedListElement::deleteNamedEntry( const QString &entry )
{
  Q_ASSERT( mType == Type::NamedList );
  Q_ASSERT( namedElementsCount() == 1 );
  QString key = completeKey().arg( entry );
  QgsSettings().remove( key );
}

void QgsSettingsTreeNamedListElement::deleteNamedEntry( const QString &parentEntry, const QString &entry )
{
  Q_ASSERT( mType == Type::NamedList );
  Q_ASSERT( namedElementsCount() == 2 );
  QString key = completeKey().arg( parentEntry, entry );
  QgsSettings().remove( key );
}

