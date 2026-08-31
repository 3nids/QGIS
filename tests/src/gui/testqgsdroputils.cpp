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
#include "qgsdroputils.h"
#include "qgsmimedatautils.h"
#include "qgstest.h"

#include <QFile>
#include <QMimeData>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>

using namespace Qt::StringLiterals;

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

QGSTEST_MAIN( TestQgsDropUtils )
#include "testqgsdroputils.moc"
