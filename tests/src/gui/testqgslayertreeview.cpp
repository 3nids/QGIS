/***************************************************************************
  testqgslayertreeview.cpp - %{Cpp:License:ClassName}

 ---------------------
 begin                : 26.10.2020
 copyright            : (C) 2020 by Alessandro Pasotti
 email                : elpaso at itopen dot it
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsapplication.h"
#include "qgscategorizedsymbolrenderer.h"
#include "qgslayertree.h"
#include "qgslayertreemodel.h"
#include "qgslayertreeview.h"
#include "qgsproject.h"
#include "qgssymbol.h"
#include "qgstest.h"
#include "qgsvectorlayer.h"

#include <QDragEnterEvent>
#include <QMimeData>
#include <QString>
#include <QUrl>

using namespace Qt::StringLiterals;

/**
 * \ingroup UnitTests
 * Tests the feedback the layer tree gives while datasets are dragged onto it.
 */
class TestQgsLayerTreeView : public QgsTest
{
    Q_OBJECT

  public:
    TestQgsLayerTreeView()
      : QgsTest( u"Layer Tree View Tests"_s )
    {}

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void insertsAboveTheLayerUnderTheCursor();
    void insertsIntoTheGroupUnderTheCursor();
    void resolvesALegendNodeToItsLayer();
    void keepsTheCurrentNodeWhenNothingIsUnderTheCursor();
    void marksTheInsertionWhileDatasetsAreDragged();
    void marksNothingForAProjectWhichReplacesTheTree();
    void marksNothingOnceTheDragIsOver();

  private:
    //! Returns mime data carrying \a file, as a file manager would
    std::unique_ptr<QMimeData> mimeDataFor( const QString &file ) const;
    //! Returns mime data carrying a layer file
    std::unique_ptr<QMimeData> datasetMimeData() const;
    //! Drags \a mimeData onto the view and holds it over \a pos
    void hoverWith( QPoint pos, const QMimeData *mimeData );
    //! Drags \a mimeData onto the view and releases it over \a pos
    void dropAt( QPoint pos, const QMimeData *mimeData );
    //! Takes a hovering drag back out of the view
    void endDrag();
    //! Returns the index of \a node in the view
    QModelIndex indexOf( QgsLayerTreeNode *node ) const;

    std::unique_ptr<QgsLayerTree> mRoot;
    std::unique_ptr<QgsLayerTreeModel> mModel;
    std::unique_ptr<QgsLayerTreeView> mView;
    QgsVectorLayer *mTop = nullptr;
    QgsVectorLayer *mGrouped = nullptr;
    QgsLayerTreeGroup *mGroup = nullptr;
};

std::unique_ptr<QMimeData> TestQgsLayerTreeView::mimeDataFor( const QString &file ) const
{
  auto data = std::make_unique<QMimeData>();
  data->setUrls( { QUrl::fromLocalFile( testDataPath( file ) ) } );
  return data;
}

std::unique_ptr<QMimeData> TestQgsLayerTreeView::datasetMimeData() const
{
  return mimeDataFor( u"points.shp"_s );
}

void TestQgsLayerTreeView::hoverWith( QPoint pos, const QMimeData *mimeData )
{
  // Qt forwards no further drag event until an enter has been accepted
  QDragEnterEvent enter( pos, Qt::CopyAction, mimeData, Qt::LeftButton, Qt::NoModifier );
  QApplication::sendEvent( mView->viewport(), &enter );
  QDragMoveEvent move( pos, Qt::CopyAction, mimeData, Qt::LeftButton, Qt::NoModifier );
  QApplication::sendEvent( mView->viewport(), &move );
}

void TestQgsLayerTreeView::dropAt( QPoint pos, const QMimeData *mimeData )
{
  hoverWith( pos, mimeData );
  QDropEvent drop( QPointF( pos ), Qt::CopyAction, mimeData, Qt::LeftButton, Qt::NoModifier );
  QApplication::sendEvent( mView->viewport(), &drop );
}

void TestQgsLayerTreeView::endDrag()
{
  QDragLeaveEvent event;
  QApplication::sendEvent( mView->viewport(), &event );
}

QModelIndex TestQgsLayerTreeView::indexOf( QgsLayerTreeNode *node ) const
{
  return mView->node2index( node );
}

void TestQgsLayerTreeView::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
}

void TestQgsLayerTreeView::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgsLayerTreeView::init()
{
  const QString definition = u"Point?crs=epsg:4326&field=name:string"_s;
  mTop = new QgsVectorLayer( definition, u"top"_s, u"memory"_s );
  mGrouped = new QgsVectorLayer( definition, u"grouped"_s, u"memory"_s );
  QVERIFY( mTop->isValid() && mGrouped->isValid() );
  QgsProject::instance()->addMapLayers( { mTop, mGrouped } );

  mRoot = std::make_unique<QgsLayerTree>();
  mRoot->addLayer( mTop );
  mGroup = mRoot->addGroup( u"group"_s );
  mGroup->addLayer( mGrouped );

  mModel = std::make_unique<QgsLayerTreeModel>( mRoot.get() );
  mModel->setFlag( QgsLayerTreeModel::ShowLegend, true );
  mView = std::make_unique<QgsLayerTreeView>();
  mView->setModel( mModel.get() );
  // a layer drawn with a single symbol has it shown on its own row, and only one with
  // several classes has the legend nodes this exercises
  QgsCategorizedSymbolRenderer *renderer = new QgsCategorizedSymbolRenderer( u"name"_s, {} );
  renderer->addCategory( QgsRendererCategory( u"a"_s, QgsSymbol::defaultSymbol( Qgis::GeometryType::Point ), u"a"_s ) );
  renderer->addCategory( QgsRendererCategory( u"b"_s, QgsSymbol::defaultSymbol( Qgis::GeometryType::Point ), u"b"_s ) );
  mTop->setRenderer( renderer );
  mModel->refreshLayerLegend( mRoot->findLayer( mTop ) );
  mView->expandAll();
  mView->resize( 300, 400 );
  mView->show();
  QVERIFY( QTest::qWaitForWindowExposed( mView.get() ) );
}

void TestQgsLayerTreeView::cleanup()
{
  mView.reset();
  mModel.reset();
  mRoot.reset();
  QgsProject::instance()->removeAllMapLayers();
}

void TestQgsLayerTreeView::insertsAboveTheLayerUnderTheCursor()
{
  const std::unique_ptr<QMimeData> data = datasetMimeData();
  const QModelIndex layer = indexOf( mRoot->findLayer( mTop ) );
  QVERIFY( layer.isValid() );

  dropAt( mView->visualRect( layer ).center(), data.get() );

  // the layers land in front of whatever the drop made current
  QCOMPARE( mView->currentIndex(), layer );
}

void TestQgsLayerTreeView::insertsIntoTheGroupUnderTheCursor()
{
  const std::unique_ptr<QMimeData> data = datasetMimeData();
  const QModelIndex group = indexOf( mGroup );
  QVERIFY( group.isValid() );

  dropAt( mView->visualRect( group ).center(), data.get() );

  QCOMPARE( mView->currentIndex(), group );
}

void TestQgsLayerTreeView::resolvesALegendNodeToItsLayer()
{
  const std::unique_ptr<QMimeData> data = datasetMimeData();
  const QModelIndex layer = indexOf( mRoot->findLayer( mTop ) );
  const QModelIndex legend = mView->model()->index( 0, 0, layer );
  QVERIFY( legend.isValid() );
  QVERIFY( !mView->index2node( legend ) );

  dropAt( mView->visualRect( legend ).center(), data.get() );

  // a symbol is not somewhere layers can be inserted, the layer showing it is
  QCOMPARE( mView->currentIndex(), layer );
}

void TestQgsLayerTreeView::keepsTheCurrentNodeWhenNothingIsUnderTheCursor()
{
  const std::unique_ptr<QMimeData> data = datasetMimeData();
  const QModelIndex group = indexOf( mGroup );
  mView->setCurrentIndex( group );

  // past the last row the drop points at nothing, and must not move the insertion
  dropAt( QPoint( 10, mView->viewport()->height() - 1 ), data.get() );

  QCOMPARE( mView->currentIndex(), group );
}

void TestQgsLayerTreeView::marksTheInsertionWhileDatasetsAreDragged()
{
  const std::unique_ptr<QMimeData> data = datasetMimeData();
  const QImage before = mView->viewport()->grab().toImage();

  hoverWith( mView->visualRect( indexOf( mRoot->findLayer( mTop ) ) ).center(), data.get() );
  const QImage overLayer = mView->viewport()->grab().toImage();
  QVERIFY( overLayer != before );

  // a group is outlined rather than having a line drawn above it, since the layers go inside
  hoverWith( mView->visualRect( indexOf( mGroup ) ).center(), data.get() );
  const QImage overGroup = mView->viewport()->grab().toImage();
  QVERIFY( overGroup != before );
  QVERIFY( overGroup != overLayer );
}

void TestQgsLayerTreeView::marksNothingForAProjectWhichReplacesTheTree()
{
  const std::unique_ptr<QMimeData> project = mimeDataFor( u"joins.qgs"_s );
  const QImage before = mView->viewport()->grab().toImage();

  // dropping a project replaces the whole tree, so there is no insertion to point at
  hoverWith( mView->visualRect( indexOf( mGroup ) ).center(), project.get() );

  QCOMPARE( mView->viewport()->grab().toImage(), before );
}

void TestQgsLayerTreeView::marksNothingOnceTheDragIsOver()
{
  const std::unique_ptr<QMimeData> data = datasetMimeData();
  const QImage before = mView->viewport()->grab().toImage();

  hoverWith( mView->visualRect( indexOf( mGroup ) ).center(), data.get() );
  QVERIFY( mView->viewport()->grab().toImage() != before );

  endDrag();
  QCOMPARE( mView->viewport()->grab().toImage(), before );
}

QGSTEST_MAIN( TestQgsLayerTreeView )
#include "testqgslayertreeview.moc"
