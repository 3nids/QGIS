/***************************************************************************
    qgsowsconnection.h  -  OWS connection
                             -------------------
    begin                : 3 April 2005
    original             : (C) 2005 by Brendan Morley email  : morb at ozemail dot com dot au
    wms search           : (C) 2009 Mathias Walker <mwa at sourcepole.ch>, Sourcepole AG

    generalized          : (C) 2012 Radim Blazek, based on qgswmsconnection.h


 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSOWSCONNECTION_H
#define QGSOWSCONNECTION_H

#include "qgis_core.h"
#include "qgsdatasourceuri.h"
#include "qgssettingsentryimpl.h"
#include "qgssettingsentryenumflag.h"
#include "qgssettingsregistrycore.h"

#include <QStringList>
#include <QPushButton>


/**
 * \ingroup core
 * \brief Connections management
 */
class CORE_EXPORT QgsOwsConnection : public QObject
{
    Q_OBJECT

  public:

#ifndef SIP_RUN
    static const inline QgsSettingsEntryString settingsConnectionSelected = QgsSettingsEntryString( QStringLiteral( "connections-%1/selected" ), QgsSettings::Prefix::QGIS ) ;

    static inline QgsSettingsTreeElement sTtreeConnections = *QgsSettingsTreeElement::createNamedListElement( &QgsSettingsRegistryCore::sTreeQgis, QStringLiteral( "connections" ) );
    static inline QgsSettingsTreeElement sTreeConnectionServices = *QgsSettingsTreeElement::createNamedListElement( &sTtreeConnections, QStringLiteral( "services" ) );

    static const inline QgsSettingsEntryString settingsConnectionUrl = QgsSettingsEntryString( QStringLiteral( "url" ), &sTreeConnectionServices, QString() ) ;
    static const inline QgsSettingsEntryVariantMap settingsConnectionHeader = QgsSettingsEntryVariantMap( QStringLiteral( "http-header" ), &sTreeConnectionServices ) ;
    static const inline QgsSettingsEntryString settingsConnectionVersion = QgsSettingsEntryString( QStringLiteral( "version" ), &sTreeConnectionServices, QString() ) ;
    static const inline QgsSettingsEntryBool settingsConnectionIgnoreGetMapURI = QgsSettingsEntryBool( QStringLiteral( "ignoreGetMapURI" ), &sTreeConnectionServices, false ) ;
    static const inline QgsSettingsEntryBool settingsConnectionIgnoreGetFeatureInfoURI = QgsSettingsEntryBool( QStringLiteral( "ignoreGetFeatureInfoURI" ), &sTreeConnectionServices, false ) ;
    static const inline QgsSettingsEntryBool settingsConnectionSmoothPixmapTransform = QgsSettingsEntryBool( QStringLiteral( "smoothPixmapTransform" ), &sTreeConnectionServices, false ) ;
    static const inline QgsSettingsEntryBool settingsConnectionReportedLayerExtents = QgsSettingsEntryBool( QStringLiteral( "reportedLayerExtents" ), &sTreeConnectionServices, false ) ;
    static const inline QgsSettingsEntryEnumFlag<Qgis::DpiMode> settingsConnectionDpiMode = QgsSettingsEntryEnumFlag<Qgis::DpiMode>( QStringLiteral( "dpiMode" ), &sTreeConnectionServices, Qgis::DpiMode::All, QString(), Qgis::SettingsOption::SaveEnumFlagAsInt ) ;
    static const inline QgsSettingsEntryEnumFlag<Qgis::TilePixelRatio> settingsConnectionTilePixelRatio = QgsSettingsEntryEnumFlag<Qgis::TilePixelRatio>( QStringLiteral( "tilePixelRatio" ), &sTreeConnectionServices, Qgis::TilePixelRatio::Undefined, QString(), Qgis::SettingsOption::SaveEnumFlagAsInt ) ;
    static const inline QgsSettingsEntryString settingsConnectionMaxNumFeatures = QgsSettingsEntryString( QStringLiteral( "maxnumfeatures" ), &sTreeConnectionServices ) ;
    static const inline QgsSettingsEntryString settingsConnectionPagesize = QgsSettingsEntryString( QStringLiteral( "pagesize" ), &sTreeConnectionServices ) ;
    static const inline QgsSettingsEntryBool settingsConnectionPagingEnabled = QgsSettingsEntryBool( QStringLiteral( "pagingenabled" ), &sTreeConnectionServices, true ) ;
    static const inline QgsSettingsEntryBool settingsConnectionPreferCoordinatesForWfsT11 = QgsSettingsEntryBool( QStringLiteral( "preferCoordinatesForWfsT11" ), &sTreeConnectionServices, false ) ;
    static const inline QgsSettingsEntryBool settingsConnectionIgnoreAxisOrientation = QgsSettingsEntryBool( QStringLiteral( "ignoreAxisOrientation" ), &sTreeConnectionServices, false ) ;
    static const inline QgsSettingsEntryBool settingsConnectionInvertAxisOrientation = QgsSettingsEntryBool( QStringLiteral( "invertAxisOrientation" ), &sTreeConnectionServices, false ) ;

    static const inline QgsSettingsEntryString settingsConnectionUsername = QgsSettingsEntryString( QStringLiteral( "username" ), &sTreeConnectionServices ) ;
    static const inline QgsSettingsEntryString settingsConnectionPassword = QgsSettingsEntryString( QStringLiteral( "password" ), &sTreeConnectionServices ) ;
    static const inline QgsSettingsEntryString settingsConnectionAuthCfg = QgsSettingsEntryString( QStringLiteral( "authcfg" ), &sTreeConnectionServices ) ;

#endif

    /**
     * Constructor
     * \param service service name: WMS,WFS,WCS
     * \param connName connection name
     */
    QgsOwsConnection( const QString &service, const QString &connName );

    /**
     * Returns the connection name.
     * \since QGIS 3.0
     */
    QString connectionName() const;

    /**
     * Returns connection info string.
     * \since QGIS 3.0
     */
    QString connectionInfo() const;

    /**
     * Returns a string representing the service type, e.g. "WMS".
     * \since QGIS 3.0
     */
    QString service() const;

    /**
     * Returns the connection uri.
     */
    QgsDataSourceUri uri() const;

    /**
     * Adds uri parameters relating to the settings for a WMS or WCS connection to a QgsDataSourceUri \a uri.
     * Connection settings are taken from the specified QSettings \a settingsKey.
     * \since QGIS 3.0
     * \deprecated since QGIS 3.26 use addWmsWcsConnectionSettings with service and connection name parameters
     */
    Q_DECL_DEPRECATED static QgsDataSourceUri &addWmsWcsConnectionSettings( QgsDataSourceUri &uri, const QString &settingsKey ) SIP_DEPRECATED;

    /**
     * Adds uri parameters relating to the settings for a WMS or WCS connection to a QgsDataSourceUri \a uri.
     * Connection settings are taken from the specified \a servcie and \a connName
     * \since QGIS 3.26
     */
    static QgsDataSourceUri &addWmsWcsConnectionSettings( QgsDataSourceUri &uri, const QString &service, const QString &connName );

    /**
     * Adds uri parameters relating to the settings for a WFS connection to a QgsDataSourceUri \a uri.
     * Connection settings are taken from the specified QSettings \a settingsKey.
     * \since QGIS 3.0
     * \deprecated since QGIS 3.26 use addWfsConnectionSettings with service and connection name parameters
     */
    Q_DECL_DEPRECATED static QgsDataSourceUri &addWfsConnectionSettings( QgsDataSourceUri &uri, const QString &settingsKey ) SIP_DEPRECATED;

    /**
     * Adds uri parameters relating to the settings for a WFS connection to a QgsDataSourceUri \a uri.
     * Connection settings are taken from the specified \a servcie and \a connName
     * \since QGIS 3.26
     */
    static QgsDataSourceUri &addWfsConnectionSettings( QgsDataSourceUri &uri, const QString &service, const QString &connName );

    //! Returns the list of connections for the specified service
    static QStringList connectionList( const QString &service );

    //! Deletes the connection for the specified service with the specified name
    static void deleteConnection( const QString &service, const QString &name );

    //! Retrieves the selected connection for the specified service
    static QString selectedConnection( const QString &service );
    //! Marks the specified connection for the specified service as selected
    static void setSelectedConnection( const QString &service, const QString &name );

  protected:
    QgsDataSourceUri mUri;

  private:

    QString mConnName;
    QString mService;
    QString mConnectionInfo;

    Q_DECL_DEPRECATED static void addCommonConnectionSettings( QgsDataSourceUri &uri, const QString &settingsKey );
    static void addCommonConnectionSettings( QgsDataSourceUri &uri, const QString &service, const QString &connectionName );

};


#endif // QGSOWSCONNECTION_H
