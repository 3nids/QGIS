/***************************************************************************
  qgsdroputils.cpp
  --------------------------------------
  Date                 : August 2026
  Copyright            : (C) 2026 by Denis Rouzaud
  Email                : denis@opengis.ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsdroputils.h"

#include "qgsmimedatautils.h"

#include <QFileInfo>
#include <QMimeData>
#include <QString>
#include <QUrl>

using namespace Qt::StringLiterals;

//! Mime type of a drag of the layer tree's own nodes
static const QString LAYER_TREE_MIMETYPE = u"application/qgis.layertreemodeldata"_s;

bool QgsDropUtils::isDatasetDrag( const QMimeData *data )
{
  if ( !data )
    return false;

  // a drag of the layer tree's own nodes reorders them, it does not bring datasets in
  if ( data->hasFormat( LAYER_TREE_MIMETYPE ) )
    return false;

  return data->hasUrls() || QgsMimeDataUtils::isUriList( data );
}

QStringList QgsDropUtils::files( const QMimeData *data )
{
  if ( !data )
    return QStringList();

  QStringList files;
  const QList<QUrl> urls = data->urls();
  for ( const QUrl &url : urls )
  {
    // some drag and drop operations include an empty url
    const QString file = url.toLocalFile();
    if ( !file.isEmpty() )
      files << file;
  }
  return files;
}

bool QgsDropUtils::hasCustomUri( const QMimeData *data, const QString &providerKey )
{
  if ( providerKey.isEmpty() || !data || !QgsMimeDataUtils::isUriList( data ) )
    return false;

  const QgsMimeDataUtils::UriList uris = QgsMimeDataUtils::decodeUriList( data );
  for ( const QgsMimeDataUtils::Uri &uri : uris )
  {
    if ( uri.layerType == "custom"_L1 && uri.providerKey == providerKey )
      return true;
  }
  return false;
}

bool QgsDropUtils::hasFileExtension( const QMimeData *data, const QStringList &extensions )
{
  const QStringList droppedFiles = files( data );
  for ( const QString &file : droppedFiles )
  {
    const QString suffix = QFileInfo( file ).completeSuffix();
    for ( const QString &extension : extensions )
    {
      if ( suffix.compare( extension, Qt::CaseInsensitive ) == 0 )
        return true;
    }
  }
  return false;
}
