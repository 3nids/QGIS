/***************************************************************************
    qgsnative.cpp - abstracted interface to native system calls
                             -------------------
    begin                : January 2017
    copyright            : (C) 2017 by Matthias Kuhn
    email                : matthias@opengis.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsnative.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QString>
#include <QUrl>

#include "moc_qgsnative.cpp"

void QgsNative::cleanup()
{}

QgsNative::Capabilities QgsNative::capabilities() const
{
  return QgsNative::Capabilities();
}

void QgsNative::initializeMainWindow( QWindow *, const QString &, const QString &, const QString & )
{}

void QgsNative::currentAppActivateIgnoringOtherApps()
{}

void QgsNative::openFileExplorerAndSelectFile( const QString &path )
{
  const QFileInfo fi( path );
  const QString folder = fi.path();
  QDesktopServices::openUrl( QUrl::fromLocalFile( folder ) );
}

void QgsNative::showFileProperties( const QString & )
{}

void QgsNative::showUndefinedApplicationProgress()
{}

void QgsNative::setApplicationProgress( double )
{}

void QgsNative::hideApplicationProgress()
{}

void QgsNative::setApplicationBadgeCount( int )
{}

bool QgsNative::hasDarkTheme()
{
  return false;
}

bool QgsNative::openTerminalAtPath( const QString & )
{
  return false;
}

QgsNative::NotificationResult QgsNative::showDesktopNotification( const QString &, const QString &, const NotificationSettings & )
{
  NotificationResult result;
  result.successful = false;
  return result;
}

void QgsNative::onRecentProjectsChanged( const std::vector<QgsNative::RecentProjectProperties> & )
{}

QString QgsNative::getSaveFileName( QWidget *parent, const QString &caption, const QString &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options )
{
  return QFileDialog::getSaveFileName( parent, caption, dir, filter, selectedFilter, options );
}

QString QgsNative::getOpenFileName( QWidget *parent, const QString &caption, const QString &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options )
{
  return QFileDialog::getOpenFileName( parent, caption, dir, filter, selectedFilter, options );
}

QStringList QgsNative::getOpenFileNames( QWidget *parent, const QString &caption, const QString &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options )
{
  return QFileDialog::getOpenFileNames( parent, caption, dir, filter, selectedFilter, options );
}

QString QgsNative::getExistingDirectory( QWidget *parent, const QString &caption, const QString &dir, QFileDialog::Options options )
{
  return QFileDialog::getExistingDirectory( parent, caption, dir, options );
}
