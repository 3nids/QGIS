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



void QgsSettingsTreeElement::init()
{
  QList<const QgsSettingsTreeElement *> parents;
  const QgsSettingsTreeElement *te = mParent;
  while ( true )
  {
    parents << te;
    if ( te->parent() )
      te = te->parent();
    else
    {
      Q_ASSERT( te->type() == Type::Root );
      mRegistryInstance = te->registry();
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
}

void QgsSettingsTreeElement::deleteNamedEntry( const QString &entry )
{
  Q_ASSERT( mType == Type::NamedList );
  Q_ASSERT( mNamedElementsCount == 1 );
  QString key = mCompleteKey.arg( entry );
  QgsSettings().remove( key );
}

void QgsSettingsTreeElement::deleteNamedEntry( const QString &parentEntry, const QString &entry )
{
  Q_ASSERT( mType == Type::NamedList );
  Q_ASSERT( mNamedElementsCount == 2 );

  QString key = mCompleteKey.arg( parentEntry, entry );
  QgsSettings().remove( key );
}
