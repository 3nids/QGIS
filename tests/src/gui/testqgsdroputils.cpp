/***************************************************************************
     testqgsdroputils.cpp
     --------------------
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

#include "qgsapplication.h"
#include "qgscustomdrophandler.h"
#include "qgsdroputils.h"
#include "qgsmimedatautils.h"
#include "qgstest.h"

#include <QFile>
#include <QMimeData>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>

using namespace Qt::StringLiterals;

//! A handler which predates payloadType() and only says it handles a file when it is dropped
class LegacyDropHandler : public QgsCustomDropHandler
{
    Q_OBJECT

  public:
    bool handleFileDrop( const QString & ) override { return true; }
};

//! A handler which consumes .test files
class TestFileDropHandler : public QgsCustomDropHandler
{
    Q_OBJECT

  public:
    Qgis::DropPayloadType payloadType( const QMimeData *data ) override
    {
      return QgsDropUtils::hasFileExtension( data, { u"test"_s } ) ? Qgis::DropPayloadType::CustomHandler : Qgis::DropPayloadType::Unsupported;
    }
};

//! A handler which adds layers of its own, from uris carrying its provider key
class TestLayerDropHandler : public QgsCustomDropHandler
{
    Q_OBJECT

  public:
    QString customUriProviderKey() const override { return u"test_layers"_s; }

    Qgis::DropPayloadType payloadType( const QMimeData *data ) override
    {
      return QgsCustomDropHandler::payloadType( data ) == Qgis::DropPayloadType::CustomHandler ? Qgis::DropPayloadType::Layers : Qgis::DropPayloadType::Unsupported;
    }
};

class TestQgsDropUtils : public QgsTest
{
    Q_OBJECT

  public:
    TestQgsDropUtils()
      : QgsTest( u"Drop Utils Tests"_s )
    {}

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void isDatasetDrag();
    void files();
    void hasCustomUri();
    void hasFileExtension();

    void payloadTypeOfFiles();
    void payloadTypeOfUris();
    void payloadTypeOfSeveralItems();
    void payloadTypeWithHandlers();

  private:
    //! Returns mime data carrying \a files as urls, as a file manager would
    static std::unique_ptr<QMimeData> fileMimeData( const QStringList &files );
    //! Returns a path in the temporary directory of a file created with the given \a name
    QString touch( const QString &name );

    QTemporaryDir mDir;
};

std::unique_ptr<QMimeData> TestQgsDropUtils::fileMimeData( const QStringList &files )
{
  QList<QUrl> urls;
  for ( const QString &file : files )
    urls << QUrl::fromLocalFile( file );

  auto data = std::make_unique<QMimeData>();
  data->setUrls( urls );
  return data;
}

QString TestQgsDropUtils::touch( const QString &name )
{
  const QString path = mDir.filePath( name );
  QFile file( path );
  file.open( QIODevice::WriteOnly );
  file.close();
  return path;
}

void TestQgsDropUtils::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
  QVERIFY( mDir.isValid() );
}

void TestQgsDropUtils::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgsDropUtils::isDatasetDrag()
{
  QVERIFY( !QgsDropUtils::isDatasetDrag( nullptr ) );

  const std::unique_ptr<QMimeData> withFile = fileMimeData( { testDataPath( u"points.shp"_s ) } );
  QVERIFY( QgsDropUtils::isDatasetDrag( withFile.get() ) );

  QgsMimeDataUtils::Uri uri;
  uri.layerType = u"vector"_s;
  uri.uri = testDataPath( u"points.shp"_s );
  const std::unique_ptr<QMimeData> encoded( QgsMimeDataUtils::encodeUriList( { uri } ) );
  QVERIFY( QgsDropUtils::isDatasetDrag( encoded.get() ) );

  // a drag of the layer tree's own nodes reorders them, it brings no dataset in
  QMimeData layerTreeDrag;
  layerTreeDrag.setData( u"application/qgis.layertreemodeldata"_s, QByteArray() );
  layerTreeDrag.setUrls( { QUrl::fromLocalFile( testDataPath( u"points.shp"_s ) ) } );
  QVERIFY( !QgsDropUtils::isDatasetDrag( &layerTreeDrag ) );

  QMimeData text;
  text.setText( u"hello"_s );
  QVERIFY( !QgsDropUtils::isDatasetDrag( &text ) );
}

void TestQgsDropUtils::files()
{
  QCOMPARE( QgsDropUtils::files( nullptr ), QStringList() );

  const std::unique_ptr<QMimeData> data = fileMimeData( { testDataPath( u"points.shp"_s ), testDataPath( u"lines.shp"_s ) } );
  QCOMPARE( QgsDropUtils::files( data.get() ), QStringList() << testDataPath( u"points.shp"_s ) << testDataPath( u"lines.shp"_s ) );

  // remote urls are not local files, and empty urls creep into some drags
  QMimeData mixed;
  mixed.setUrls( { QUrl( u"https://qgis.org"_s ), QUrl(), QUrl::fromLocalFile( testDataPath( u"points.shp"_s ) ) } );
  QCOMPARE( QgsDropUtils::files( &mixed ), QStringList() << testDataPath( u"points.shp"_s ) );
}

void TestQgsDropUtils::hasCustomUri()
{
  QgsMimeDataUtils::Uri custom;
  custom.layerType = u"custom"_s;
  custom.providerKey = u"processing"_s;
  custom.uri = u"/models/buffer.model3"_s;
  const std::unique_ptr<QMimeData> data( QgsMimeDataUtils::encodeUriList( { custom } ) );

  QVERIFY( QgsDropUtils::hasCustomUri( data.get(), u"processing"_s ) );
  QVERIFY( !QgsDropUtils::hasCustomUri( data.get(), u"qlr"_s ) );
  QVERIFY( !QgsDropUtils::hasCustomUri( data.get(), QString() ) );
  QVERIFY( !QgsDropUtils::hasCustomUri( nullptr, u"processing"_s ) );

  // a layer uri is dropped by QGIS itself, not handed to a handler
  QgsMimeDataUtils::Uri vector;
  vector.layerType = u"vector"_s;
  vector.providerKey = u"ogr"_s;
  vector.uri = testDataPath( u"points.shp"_s );
  const std::unique_ptr<QMimeData> layerData( QgsMimeDataUtils::encodeUriList( { vector } ) );
  QVERIFY( !QgsDropUtils::hasCustomUri( layerData.get(), u"ogr"_s ) );
}

void TestQgsDropUtils::hasFileExtension()
{
  const std::unique_ptr<QMimeData> data = fileMimeData( { touch( u"model.model3"_s ) } );

  QVERIFY( QgsDropUtils::hasFileExtension( data.get(), { u"model3"_s } ) );
  QVERIFY( QgsDropUtils::hasFileExtension( data.get(), { u"MODEL3"_s } ) );
  QVERIFY( QgsDropUtils::hasFileExtension( data.get(), { u"qpt"_s, u"model3"_s } ) );
  QVERIFY( !QgsDropUtils::hasFileExtension( data.get(), { u"qpt"_s } ) );
  QVERIFY( !QgsDropUtils::hasFileExtension( data.get(), {} ) );
}

void TestQgsDropUtils::payloadTypeOfFiles()
{
  auto payloadTypeOf = []( const QString &file ) {
    const std::unique_ptr<QMimeData> data = fileMimeData( { file } );
    return QgsDropUtils::payloadType( data.get() );
  };

  QCOMPARE( payloadTypeOf( testDataPath( u"joins.qgs"_s ) ), Qgis::DropPayloadType::Project );
  QCOMPARE( payloadTypeOf( testDataPath( u"broken_relations2.qgz"_s ) ), Qgis::DropPayloadType::Project );
  QCOMPARE( payloadTypeOf( testDataPath( u"invalid_source.qlr"_s ) ), Qgis::DropPayloadType::Layers );
  QCOMPARE( payloadTypeOf( testDataPath( u"points.shp"_s ) ), Qgis::DropPayloadType::Layers );
  QCOMPARE( payloadTypeOf( testDataPath( u"rgb256x256.png"_s ) ), Qgis::DropPayloadType::Layers );

  // the extension is matched case insensitively, as QgisApp::openFile() does
  QCOMPARE( payloadTypeOf( touch( u"project.QGZ"_s ) ), Qgis::DropPayloadType::Project );

  // nothing in QGIS reads these
  QCOMPARE( payloadTypeOf( touch( u"notes.rtf"_s ) ), Qgis::DropPayloadType::Unsupported );

  // an extension is all a drag can afford to look at, so what carries none is left to the drop
  QCOMPARE( payloadTypeOf( touch( u"README"_s ) ), Qgis::DropPayloadType::Unknown );
  QCOMPARE( payloadTypeOf( testDataPath( QString() ) ), Qgis::DropPayloadType::Unknown );

  // a widget dragging its own contents carries no dataset at all
  QMimeData layerTreeDrag;
  layerTreeDrag.setData( u"application/qgis.layertreemodeldata"_s, QByteArray() );
  QCOMPARE( QgsDropUtils::payloadType( &layerTreeDrag ), Qgis::DropPayloadType::Unsupported );
  QCOMPARE( QgsDropUtils::payloadType( nullptr ), Qgis::DropPayloadType::Unsupported );
}

void TestQgsDropUtils::payloadTypeOfUris()
{
  auto payloadTypeOf = []( const QString &layerType, const QString &providerKey ) {
    QgsMimeDataUtils::Uri uri;
    uri.layerType = layerType;
    uri.providerKey = providerKey;
    uri.uri = u"/some/path"_s;
    const std::unique_ptr<QMimeData> data( QgsMimeDataUtils::encodeUriList( { uri } ) );
    return QgsDropUtils::payloadType( data.get() );
  };

  QCOMPARE( payloadTypeOf( u"vector"_s, u"ogr"_s ), Qgis::DropPayloadType::Layers );
  QCOMPARE( payloadTypeOf( u"raster"_s, u"gdal"_s ), Qgis::DropPayloadType::Layers );
  QCOMPARE( payloadTypeOf( u"project"_s, QString() ), Qgis::DropPayloadType::Project );

  // a custom uri is nothing without the handler it was created for, and none was given here
  QCOMPARE( payloadTypeOf( u"custom"_s, u"processing"_s ), Qgis::DropPayloadType::Unsupported );
}

void TestQgsDropUtils::payloadTypeOfSeveralItems()
{
  auto payloadTypeOf = []( const QStringList &files ) {
    const std::unique_ptr<QMimeData> data = fileMimeData( files );
    return QgsDropUtils::payloadType( data.get() );
  };

  const QString project = testDataPath( u"joins.qgs"_s );
  const QString layer = testDataPath( u"points.shp"_s );
  const QString unsupported = touch( u"notes.rtf"_s );
  const QString unknown = touch( u"README"_s );

  // the item which dominates what the drop does is the one reported
  QCOMPARE( payloadTypeOf( { layer, project } ), Qgis::DropPayloadType::Project );
  QCOMPARE( payloadTypeOf( { unsupported, layer } ), Qgis::DropPayloadType::Layers );

  // one item QGIS cannot classify is enough for the drag not to be refused
  QCOMPARE( payloadTypeOf( { unsupported, unknown } ), Qgis::DropPayloadType::Unknown );
  QCOMPARE( payloadTypeOf( { unsupported, unsupported } ), Qgis::DropPayloadType::Unsupported );
}

void TestQgsDropUtils::payloadTypeWithHandlers()
{
  TestFileDropHandler fileHandler;
  TestLayerDropHandler layerHandler;
  LegacyDropHandler legacyHandler;

  const std::unique_ptr<QMimeData> claimed = fileMimeData( { touch( u"data.test"_s ) } );
  const std::unique_ptr<QMimeData> unclaimed = fileMimeData( { touch( u"notes.rtf"_s ) } );

  // a handler recognizes payloads no data provider knows about
  QCOMPARE( QgsDropUtils::payloadType( claimed.get() ), Qgis::DropPayloadType::Unsupported );
  QCOMPARE( QgsDropUtils::payloadType( claimed.get(), { &fileHandler } ), Qgis::DropPayloadType::CustomHandler );
  QCOMPARE( QgsDropUtils::payloadType( unclaimed.get(), { &fileHandler } ), Qgis::DropPayloadType::Unsupported );

  // a handler which brings in layers of its own says so
  QgsMimeDataUtils::Uri uri;
  uri.layerType = u"custom"_s;
  uri.providerKey = u"test_layers"_s;
  const std::unique_ptr<QMimeData> layerUri( QgsMimeDataUtils::encodeUriList( { uri } ) );
  QCOMPARE( QgsDropUtils::payloadType( layerUri.get(), { &layerHandler } ), Qgis::DropPayloadType::Layers );

  // a handler which predates payloadType() cannot say what it accepts, so nothing is refused
  // on its behalf
  QCOMPARE( QgsDropUtils::payloadType( unclaimed.get(), { &legacyHandler } ), Qgis::DropPayloadType::Unknown );

  // ... but it does not hide what the rest of QGIS does know
  const std::unique_ptr<QMimeData> project = fileMimeData( { testDataPath( u"joins.qgs"_s ) } );
  QCOMPARE( QgsDropUtils::payloadType( project.get(), { &legacyHandler } ), Qgis::DropPayloadType::Project );

  // a destroyed handler is skipped rather than dereferenced
  QCOMPARE( QgsDropUtils::payloadType( claimed.get(), { nullptr, &fileHandler } ), Qgis::DropPayloadType::CustomHandler );
}

QGSTEST_MAIN( TestQgsDropUtils )
#include "testqgsdroputils.moc"
