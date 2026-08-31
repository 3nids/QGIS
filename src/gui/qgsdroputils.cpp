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

#include "qgscustomdrophandler.h"
#include "qgsmimedatautils.h"
#include "qgsproviderregistry.h"
#include "qgsprovidersublayerdetails.h"

#include <QFileInfo>
#include <QMimeData>
#include <QString>
#include <QUrl>

using namespace Qt::StringLiterals;

//! Mime type of a drag of the layer tree's own nodes
static const QString LAYER_TREE_MIMETYPE = u"application/qgis.layertreemodeldata"_s;

/**
 * Returns how consequential \a type is, so that a drag carrying several items can be
 * reported as the one which dominates what the drop does.
 *
 * Unknown outranks Unsupported: a single item QGIS cannot classify is enough for the
 * drag not to be refused.
 */
static int payloadRank( Qgis::DropPayloadType type )
{
  switch ( type )
  {
    case Qgis::DropPayloadType::Unsupported:
      return 0;
    case Qgis::DropPayloadType::Unknown:
      return 1;
    case Qgis::DropPayloadType::CustomHandler:
      return 2;
    case Qgis::DropPayloadType::Layers:
      return 3;
    case Qgis::DropPayloadType::Project:
      return 4;
  }
  return 0;
}

static Qgis::DropPayloadType dominant( Qgis::DropPayloadType a, Qgis::DropPayloadType b )
{
  return payloadRank( a ) >= payloadRank( b ) ? a : b;
}

//! Returns what a single browser \a uri holds
static Qgis::DropPayloadType uriPayloadType( const QgsMimeDataUtils::Uri &uri )
{
  if ( uri.layerType == "project"_L1 )
    return Qgis::DropPayloadType::Project;

  // a custom uri is a handler's business alone, and the handlers have already had their
  // say by the time this is reached
  if ( uri.layerType == "custom"_L1 )
    return Qgis::DropPayloadType::Unsupported;

  return Qgis::DropPayloadType::Layers;
}

//! Returns what a single local \a file holds
static Qgis::DropPayloadType filePayloadType( const QString &file )
{
  const QFileInfo fileInfo( file );

  // a directory, or a file named without one, cannot be told apart by extension
  const QString suffix = fileInfo.suffix();
  if ( suffix.isEmpty() || fileInfo.isDir() )
    return Qgis::DropPayloadType::Unknown;

  // QGIS' own documents, which QgisApp::openFile() recognizes by extension alone
  if ( suffix.compare( "qgs"_L1, Qt::CaseInsensitive ) == 0 || suffix.compare( "qgz"_L1, Qt::CaseInsensitive ) == 0 )
    return Qgis::DropPayloadType::Project;
  if ( suffix.compare( "qlr"_L1, Qt::CaseInsensitive ) == 0 )
    return Qgis::DropPayloadType::Layers;

  // a fast scan settles for matching the extension against the ones providers read,
  // which is all a drag can afford
  if ( !QgsProviderRegistry::instance()->querySublayers( file, Qgis::SublayerQueryFlag::FastScan ).isEmpty() )
    return Qgis::DropPayloadType::Layers;

  return Qgis::DropPayloadType::Unsupported;
}

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

Qgis::DropPayloadType QgsDropUtils::payloadType( const QMimeData *data, const QVector<QPointer<QgsCustomDropHandler>> &customHandlers )
{
  // handlers come first: they recognize payloads no data provider knows about, such as a
  // print template or a Processing model, and some accept mime data carrying no dataset
  // at all
  Qgis::DropPayloadType type = Qgis::DropPayloadType::Unsupported;
  for ( QgsCustomDropHandler *handler : customHandlers )
  {
    if ( handler )
      type = dominant( type, handler->payloadType( data ) );
  }
  if ( type != Qgis::DropPayloadType::Unsupported && type != Qgis::DropPayloadType::Unknown )
    return type;

  // what no handler claimed is either a drag of datasets, or nothing QGIS deals with
  if ( !isDatasetDrag( data ) )
    return Qgis::DropPayloadType::Unsupported;

  if ( QgsMimeDataUtils::isUriList( data ) )
  {
    const QgsMimeDataUtils::UriList uris = QgsMimeDataUtils::decodeUriList( data );
    for ( const QgsMimeDataUtils::Uri &uri : uris )
      type = dominant( type, uriPayloadType( uri ) );
  }

  const QStringList droppedFiles = files( data );
  for ( const QString &file : droppedFiles )
    type = dominant( type, filePayloadType( file ) );

  return type;
}
