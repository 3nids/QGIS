/***************************************************************************
                         qgsmaptoolshaperegistry.h
                         ----------------------
    begin                : January 2022
    copyright            : (C) 2022 by Denis Rouzaud
    email                : denis@opengis.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMAPTOOLSHAPEREGISTRY_H
#define QGSMAPTOOLSHAPEREGISTRY_H

#define SIP_NO_FILE

#include "qgsabstractrelationeditorwidget.h"
#include "qgis_gui.h"
#include "qgssettingsentry.h"

class QgsMapToolShapeMetadata;
class QgsMapToolShapeAbstract;
class QgsMapToolCapture;
class QToolBar;


/**
 * \ingroup gui
 * Keeps track of the registered shape map tools
 * \since QGIS 3.24
 */
class GUI_EXPORT QgsMapToolShapeRegistry
{
    Q_GADGET
  public:

    static const inline QgsSettingsEntryString settingMapToolShapeDefaultForShape = QgsSettingsEntryString( QStringLiteral( "UI/shape-map-tools/%1/default" ), QgsSettings::Gui, QString(), QObject::tr( "Default map tool for given shape category" ) ) SIP_SKIP;
    static const inline QgsSettingsEntryString settingMapToolShapeCurrent = QgsSettingsEntryString( QStringLiteral( "UI/shape-map-tools/current" ), QgsSettings::Gui, QString(), QObject::tr( "Current shape map tool" ) ) SIP_SKIP;

    //! List of different shapes
    enum class ShapeCategory
    {
      Curve, //!< Curve
      Circle,//!< Circle
      Ellipse,//!< Ellipse
      Rectangle,//!< Rectangle
      RegularyPolygon,//!< RegularyPolygon
    };
    Q_ENUM( ShapeCategory )

    /**
     * Constructor
     */
    QgsMapToolShapeRegistry();

    ~QgsMapToolShapeRegistry();

    /**
     * Adds a new shape map tool
     */
    void addMapTool( QgsMapToolShapeMetadata *mapTool SIP_TRANSFER );

    /**
     * Removes a registered relation widget with given \a widgetType
     */
    void removeMapTool( const QString &id );

    //! Returns the map tool metadata for the gin \a id
    QgsMapToolShapeMetadata *mapToolMetadata( const QString &id ) const;

    /**
     * Constructs the map tool at the given \a id for the given \a parentTool
     */
    QgsMapToolShapeAbstract *mapTool( const QString &id, QgsMapToolCapture *parentTool ) const SIP_FACTORY;

    //! Setup the toolbar by setting the buttons and menu
    void setupToolbar( QToolBar *toolbar ) const ;


  private:

    QList<QgsMapToolShapeMetadata *> mMapTools;

};

/**
 * \ingroup gui
 * QgsMapToolShapeMetadata is a base class for shape map tools metadata to be used in QgsMapToolShapeRegistry
 * \since QGIS 3.24
 */
class GUI_EXPORT QgsMapToolShapeMetadata
{
  public:
    //! Constructor
    QgsMapToolShapeMetadata() = default;

    virtual ~QgsMapToolShapeMetadata() = default;

    //! Unique ID for the shape map tool
    virtual QString id() const = 0;

    //! Translated readable name
    virtual QString name() const = 0;

    //! Icon to be displayed in the toolbar
    virtual QIcon icon() const = 0;

    virtual QgsMapToolShapeRegistry::ShapeCategory category() const = 0;

    //! Creates the shape map tool for the given \a parentTool
    virtual QgsMapToolShapeAbstract *factory( QgsMapToolCapture *parentlTool ) const = 0;
};


#endif // QGSMAPTOOLSHAPEREGISTRY_H
