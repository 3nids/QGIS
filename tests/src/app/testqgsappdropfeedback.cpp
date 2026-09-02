/***************************************************************************
     testqgsappdropfeedback.cpp
     --------------------------
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

#include "qgisapp.h"
#include "qgsappdropfeedback.h"
#include "qgsapplication.h"
#include "qgsdropfeedbackoverlay.h"
#include "qgslayertreeview.h"
#include "qgstest.h"

#include <QDragEnterEvent>
#include <QMimeData>
#include <QString>
#include <QUrl>

using namespace Qt::StringLiterals;

/**
 * \ingroup UnitTests
 * Tests the feedback shown over the main window while data is dragged onto it.
 */
class TestQgsAppDropFeedback : public QgsTest
{
    Q_OBJECT

  public:
    TestQgsAppDropFeedback()
      : QgsTest( u"App Drop Feedback Tests"_s )
    {}

  private slots:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();

    void announcesAProjectDraggedOverAPanel();
    void staysWhileTheDragMovesBetweenPanels();
    void forgetsAnAnnouncementWhenAnotherDragBegins();
    void saysNothingAboutTheDropsWhichNeedNoWarning();
    void ignoresWhatIsNotADatasetDrag();

  private:
    //! Returns the overlay the window announces drags with
    QgsDropFeedbackOverlay *overlay() const;
    //! Returns mime data carrying \a files as urls, as a file manager would
    static std::unique_ptr<QMimeData> fileMimeData( const QStringList &files );
    //! What a widget made of a drag delivered to it
    struct DragOutcome
    {
        bool accepted = false;
        Qt::DropAction action = Qt::IgnoreAction;
    };
    static DragOutcome sendDragEnter( QWidget *widget, const QMimeData *mimeData );
    static void sendDragLeave( QWidget *widget );
    /**
     * Runs the deferred decision that a drag has ended.
     *
     * Only the events posted to the feedback itself are delivered: a QgisApp built by the
     * constructor the tests use is too bare to survive a full round of the event loop.
     */
    void settleDragEnd();

    QgisApp *mQgisApp = nullptr;
};

QgsDropFeedbackOverlay *TestQgsAppDropFeedback::overlay() const
{
  return mQgisApp->findChild<QgsDropFeedbackOverlay *>();
}

std::unique_ptr<QMimeData> TestQgsAppDropFeedback::fileMimeData( const QStringList &files )
{
  QList<QUrl> urls;
  for ( const QString &file : files )
    urls << QUrl::fromLocalFile( file );

  auto data = std::make_unique<QMimeData>();
  data->setUrls( urls );
  return data;
}

TestQgsAppDropFeedback::DragOutcome TestQgsAppDropFeedback::sendDragEnter( QWidget *widget, const QMimeData *mimeData )
{
  QDragEnterEvent event( widget->rect().center(), Qt::CopyAction, mimeData, Qt::LeftButton, Qt::NoModifier );
  QApplication::sendEvent( widget, &event );
  return { event.isAccepted(), event.dropAction() };
}

void TestQgsAppDropFeedback::sendDragLeave( QWidget *widget )
{
  QDragLeaveEvent event;
  QApplication::sendEvent( widget, &event );
}

void TestQgsAppDropFeedback::settleDragEnd()
{
  QCoreApplication::sendPostedEvents( mQgisApp->findChild<QgsAppDropFeedback *>(), QEvent::MetaCall );
}

void TestQgsAppDropFeedback::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
  mQgisApp = new QgisApp();
  mQgisApp->show();
}

void TestQgsAppDropFeedback::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestQgsAppDropFeedback::cleanup()
{
  sendDragLeave( mQgisApp );
  settleDragEnd();
}

void TestQgsAppDropFeedback::announcesAProjectDraggedOverAPanel()
{
  QVERIFY( overlay() );
  QVERIFY( overlay()->isHidden() );

  // the drag is delivered to the panel under the cursor and never to the window, yet what
  // it announces is the window's: dropping this closes the whole project
  QWidget *panel = mQgisApp->layerTreeView()->viewport();
  const std::unique_ptr<QMimeData> project = fileMimeData( { testDataPath( u"joins.qgs"_s ) } );
  sendDragEnter( panel, project.get() );

  QVERIFY( overlay()->isVisible() );
  QCOMPARE( overlay()->geometry(), mQgisApp->rect() );

  sendDragLeave( panel );
  // the end is only certain once the widgets have had their say
  QVERIFY( overlay()->isVisible() );
  settleDragEnd();
  QVERIFY( overlay()->isHidden() );
}

void TestQgsAppDropFeedback::staysWhileTheDragMovesBetweenPanels()
{
  QWidget *layerTree = mQgisApp->layerTreeView()->viewport();
  QWidget *centre = mQgisApp->centralWidget();
  const std::unique_ptr<QMimeData> project = fileMimeData( { testDataPath( u"joins.qgs"_s ) } );

  sendDragEnter( layerTree, project.get() );
  QVERIFY( overlay()->isVisible() );

  // moving to a neighbouring panel leaves the first before entering the second, which must
  // not read as the drag being over
  sendDragLeave( layerTree );
  sendDragEnter( centre, project.get() );
  settleDragEnd();

  QVERIFY( overlay()->isVisible() );
}

void TestQgsAppDropFeedback::forgetsAnAnnouncementWhenAnotherDragBegins()
{
  QWidget *panel = mQgisApp->layerTreeView()->viewport();
  const std::unique_ptr<QMimeData> project = fileMimeData( { testDataPath( u"joins.qgs"_s ) } );
  sendDragEnter( panel, project.get() );
  QVERIFY( overlay()->isVisible() );

  // a drag which never reported its end must not leave the window announcing it
  QMimeData other;
  other.setText( u"hello"_s );
  sendDragEnter( panel, &other );

  QVERIFY( overlay()->isHidden() );
}

void TestQgsAppDropFeedback::saysNothingAboutTheDropsWhichNeedNoWarning()
{
  QWidget *panel = mQgisApp->layerTreeView()->viewport();

  const std::unique_ptr<QMimeData> layer = fileMimeData( { testDataPath( u"points.shp"_s ) } );
  const DragOutcome outcome = sendDragEnter( panel, layer.get() );

  // adding layers is what dropping is for
  QVERIFY( overlay()->isHidden() );
  QVERIFY( outcome.accepted );
  QCOMPARE( outcome.action, Qt::CopyAction );
}

void TestQgsAppDropFeedback::ignoresWhatIsNotADatasetDrag()
{
  QWidget *panel = mQgisApp->layerTreeView()->viewport();

  // the layer tree reordering its own nodes is none of the window's business
  QMimeData layerTreeDrag;
  layerTreeDrag.setData( u"application/qgis.layertreemodeldata"_s, QByteArray() );
  const DragOutcome outcome = sendDragEnter( panel, &layerTreeDrag );

  QVERIFY( overlay()->isHidden() );
  QCOMPARE( outcome.action, Qt::CopyAction );
}

QGSTEST_MAIN( TestQgsAppDropFeedback )
#include "testqgsappdropfeedback.moc"
