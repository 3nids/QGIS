/***************************************************************************
    qgsoapifutils.h
    ---------------------
    begin                : October 2019
    copyright            : (C) 2019 by Even Rouault
    email                : even.rouault at spatialys.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSOAPIFUTILS_H
#define QGSOAPIFUTILS_H

#include <nlohmann/json.hpp>

using namespace nlohmann;

#include <QList>
#include <QNetworkReply>
#include <QString>
#include <QStringList>

/**
 * Utility class
*/
class QgsOAPIFJson
{
  public:
    //! A OAPIF Link
    struct Link
    {
        QString href;
        QString rel;
        QString type;
        QString title;
        QStringList profiles;
        qint64 length = -1;
    };

    //! Parses the "link" property of jParent
    static std::vector<Link> parseLinks( const json &jParent );

    //! Find among links the one that matches rel, by using an optional list of preferable types.
    static QString findLink( const std::vector<Link> &links, const QString &rel, const QStringList &preferableTypes = QStringList() );
};

// Return the href for the next link from the response headers and the MIME type of the format
QString QgsOAPIFGetNextLinkFromResponseHeader( const QList<QNetworkReply::RawHeaderPair> &responseHeaders, const QString &formatType );

extern const QString OAPIF_PROVIDER_DEFAULT_CRS;

// Originally there was a plan to have a proper mime type for JSON-FG,
// but that was changed to using the one of GeoJSON + a profile. For the
// sake of simplicity, internally, use the pseudo mime type.
extern const QString PSEUDO_JSONFG_MEDIA_TYPE;

// Media type of the Arrow IPC stream format, with GeoArrow geometries
extern const QString OAPIF_ARROW_IPC_STREAM_MEDIA_TYPE;

// Return whether mediaType is the Arrow IPC stream one, ignoring case, spaces and parameters
bool QgsOAPIFIsArrowMediaType( const QString &mediaType );

// Return whether the GDAL in use can read GeoArrow IPC streams
bool QgsOAPIFArrowDriverUsable();

class QgsGeometry;

// If geometry has circular arcs, which a GeoJSON "geometry" can only carry linearized, set it as the
// JSON-FG "place" of object, in crs (CRS84 if empty), and return true
bool QgsOAPIFSetJsonFgPlace( json &object, const QgsGeometry &geometry, int precision, const QString &crs );

// JSON-FG conformance classes of a feature with circular arcs in its "place"
json QgsOAPIFJsonFgCircularArcsConformance();

#endif // QGSOAPIFUTILS_H
