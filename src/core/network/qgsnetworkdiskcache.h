/***************************************************************************
 qgsnetworkdiskcache.h  -  Thread-safe interface for QNetworkDiskCache
    -------------------
    begin                : 2016-03-05
    copyright            : (C) 2016 by Juergen E. Fischer
    email                : jef at norbit dot de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSNETWORKDISKCACHE_H
#define QGSNETWORKDISKCACHE_H

#define SIP_NO_FILE

#include "qgis_core.h"

#include <QNetworkDiskCache>
#include <QMutex>

class QNetworkDiskCache;

///@cond PRIVATE

class ExpirableNetworkDiskCache : public QNetworkDiskCache
{
    Q_OBJECT

  public:
    explicit ExpirableNetworkDiskCache( QObject *parent = nullptr ) : QNetworkDiskCache( parent ) {}
    qint64 runExpire() { return QNetworkDiskCache::expire(); }
};

///@endcond

/**
 * \ingroup core
 * \brief Wrapper implementation of QNetworkDiskCache with all methods guarded by a
 * mutex solely for internal use of QgsNetworkAccessManagers
 *
 * \note not available in Python bindings
 */
class CORE_EXPORT QgsNetworkDiskCache : public QNetworkDiskCache
{
    Q_OBJECT

  public:

    //! \see QNetworkDiskCache::cacheDirectory
    QString cacheDirectory() const;

    //! \see QNetworkDiskCache::setCacheDirectory
    void setCacheDirectory( const QString &cacheDir );

    //! \see QNetworkDiskCache::maximumCacheSize()
    qint64 maximumCacheSize() const;

    //! \see QNetworkDiskCache::setMaximumCacheSize()
    void setMaximumCacheSize( qint64 size );

    //! \see QNetworkDiskCache::metaData()
    QNetworkCacheMetaData metaData( const QUrl &url ) override;

    //! \see QNetworkDiskCache::updateMetaData()
    void updateMetaData( const QNetworkCacheMetaData &metaData ) override;

    //! \see QNetworkDiskCache::data()
    QIODevice *data( const QUrl &url ) override;

    //! \see QNetworkDiskCache::remove()
    bool remove( const QUrl &url ) override;

    //! \see QNetworkDiskCache::cacheSize()
    qint64 cacheSize() const override;

    //! \see QNetworkDiskCache::prepare()
    QIODevice *prepare( const QNetworkCacheMetaData &metaData ) override;

    //! \see QNetworkDiskCache::insert()
    void insert( QIODevice *device ) override;

    //! \see QNetworkDiskCache::fileMetaData()
    QNetworkCacheMetaData fileMetaData( const QString &fileName ) const;

    /**
     * Returns a smart cache size, in bytes, based on available free space
     * \since QGIS 3.40
     */
    static qint64 smartCacheSize( const QString &path );

  public slots:
    //! \see QNetworkDiskCache::clear()
    void clear() override;

  protected:
    //! \see QNetworkDiskCache::expire()
    qint64 expire() override;

    /**
     * Returns TRUE if the response described by the metadata should be cached.
     * This inspects standard HTTP response headers and applies conservative
     * rules:
     *  - Cache-Control: no-store | no-cache => not cached
     *  - Cache-Control: max-age=0 => not cached
     *  - Pragma: no-cache => not cached
     *  - Vary: Authorization => not cached (QNetworkDiskCache cannot vary by
     *    Authorization header; avoiding potential leakage)
     *  - Presence of Authorization request header cannot be detected here;
     *    callers should avoid enabling disk cache for such requests unless
     *    server marks them explicitly cacheable (e.g. Cache-Control: public).
     *
     * If none of the disqualifying conditions are met the metadata is treated
     * as cacheable.
     */
    static bool isCacheable( const QNetworkCacheMetaData &metaData );

  public:
    /**
     * Sets the current authorization cache key (typically the full value of the
     * HTTP Authorization request header). When a response contains
     * `Vary: Authorization` this value will be used to create a distinct cache
     * entry by augmenting the request URL with a hashed variant token.
     * If set to an empty string the variant is disabled and responses declaring
     * variation by Authorization will not be cached.
     */
    static void setCurrentAuthorizationCacheKey( const QString &authorizationHeaderValue );

  private:
    //! Returns an URL augmented with an authorization hash if variation required.
    static QUrl authorizationVariantUrl( const QUrl &original );
    static QString sCurrentAuthorizationCacheKey; // raw value (not hashed)

  private:
    explicit QgsNetworkDiskCache( QObject *parent );

    static ExpirableNetworkDiskCache sDiskCache;
    static QMutex sDiskCacheMutex;

    friend class QgsNetworkAccessManager;
};

#endif // QGSNETWORKDISKCACHE_H
