/***************************************************************************
     testqgsdropfeedbackoverlay.cpp
     ------------------------------
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
#include "qgsdropfeedbackoverlay.h"
#include "qgstest.h"

#include <QString>
#include <QWidget>

using namespace Qt::StringLiterals;

class TestQgsDropFeedbackOverlay : public QgsTest
{
    Q_OBJECT

  public:
    TestQgsDropFeedbackOverlay()
      : QgsTest( u"Drop Feedback Overlay Tests"_s )
    {}

  private slots:
    void initTestCase();
    void cleanupTestCase();

    void hiddenUntilAnnounced();
    void announcesOnlyWhatIsWorthWarningAbout();
    void coversItsWidget();
    void letsTheDragThrough();
};

void TestQgsDropFeedbackOverlay::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
}

void TestQgsDropFeedbackOverlay::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgsDropFeedbackOverlay::hiddenUntilAnnounced()
{
  QWidget widget;
  widget.resize( 400, 300 );
  QgsDropFeedbackOverlay overlay( &widget );
  widget.show();
  QVERIFY( QTest::qWaitForWindowExposed( &widget ) );

  QVERIFY( overlay.isHidden() );

  overlay.announce( Qgis::DropPayloadType::Project, u"city.qgz"_s );
  QVERIFY( overlay.isVisible() );

  overlay.clear();
  QVERIFY( overlay.isHidden() );
}

void TestQgsDropFeedbackOverlay::announcesOnlyWhatIsWorthWarningAbout()
{
  QWidget widget;
  widget.resize( 400, 300 );
  QgsDropFeedbackOverlay overlay( &widget );
  widget.show();
  QVERIFY( QTest::qWaitForWindowExposed( &widget ) );

  // dropping a project replaces what is open, and a payload nothing reads does nothing
  // at all: both are worth saying out loud before the mouse is released
  overlay.announce( Qgis::DropPayloadType::Project );
  QVERIFY( overlay.isVisible() );
  overlay.announce( Qgis::DropPayloadType::Unsupported, u"notes.rtf"_s );
  QVERIFY( overlay.isVisible() );

  // adding layers is what dropping is for, and needs no announcement
  overlay.announce( Qgis::DropPayloadType::Layers );
  QVERIFY( overlay.isHidden() );

  overlay.announce( Qgis::DropPayloadType::Project );
  QVERIFY( overlay.isVisible() );
  overlay.announce( Qgis::DropPayloadType::CustomHandler );
  QVERIFY( overlay.isHidden() );

  overlay.announce( Qgis::DropPayloadType::Project );
  QVERIFY( overlay.isVisible() );
  overlay.announce( Qgis::DropPayloadType::Unknown );
  QVERIFY( overlay.isHidden() );
}

void TestQgsDropFeedbackOverlay::coversItsWidget()
{
  QWidget widget;
  widget.resize( 400, 300 );
  QgsDropFeedbackOverlay overlay( &widget );
  widget.show();
  QVERIFY( QTest::qWaitForWindowExposed( &widget ) );

  QCOMPARE( overlay.geometry(), widget.rect() );

  widget.resize( 640, 480 );
  QCOMPARE( overlay.geometry(), widget.rect() );
}

void TestQgsDropFeedbackOverlay::letsTheDragThrough()
{
  QWidget widget;
  QgsDropFeedbackOverlay overlay( &widget );

  // the overlay speaks about the drop, it must never be the one taking it
  QVERIFY( overlay.testAttribute( Qt::WA_TransparentForMouseEvents ) );
  QVERIFY( !overlay.acceptDrops() );
}

QGSTEST_MAIN( TestQgsDropFeedbackOverlay )
#include "testqgsdropfeedbackoverlay.moc"
