/***************************************************************************
    qgsmacnative.cpp - abstracted interface to native Mac objective-c
                             -------------------
    begin                : January 2014
    copyright            : (C) 2014 by Larry Shaffer
    email                : larrys at dakotacarto dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmacnative.h"

#import <UserNotifications/UserNotifications.h>

#include <Cocoa/Cocoa.h>

#include <QString>
#include <QPixmap>

static int notifyNum = 0;


QgsMacNative::QgsMacNative()
{}

QgsMacNative::~QgsMacNative()
{}

void QgsMacNative::setIconPath( const QString &iconPath )
{
  mIconPath = iconPath;
}

const char *QgsMacNative::currentAppLocalizedName()
{
  return [[[NSRunningApplication currentApplication] localizedName] UTF8String];
}

void QgsMacNative::currentAppActivateIgnoringOtherApps()
{
  [[NSRunningApplication currentApplication] activateWithOptions:
                                             ( NSApplicationActivateAllWindows )];
}

void QgsMacNative::openFileExplorerAndSelectFile( const QString &path )
{
  NSString *pathStr = [[NSString alloc] initWithUTF8String:path.toUtf8().constData()];
  NSArray *fileURLs = [NSArray arrayWithObjects:[NSURL fileURLWithPath:pathStr], nil];
  [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:fileURLs];
}

QgsNative::Capabilities QgsMacNative::capabilities() const
{
  return NativeDesktopNotifications;
}

QgsNative::NotificationResult QgsMacNative::showDesktopNotification( const QString &summary, const QString &body, const QgsNative::NotificationSettings &settings )
{
  Q_UNUSED( settings )

  UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];

  NSString *titleNS = summary.toNSString();
  NSString *bodyNS = body.toNSString();
  /*
  NSImage *imageNS = nil;
  if ( settings.image.isNull() )
  {
    // image application (qgis.icns) seems not to be set for now, although present in the plist
    // whenever fixed, try following line (and remove corresponding code in QgsMacNative::QgsUserNotificationCenter)
    // image = [[NSImage imageNamed:@"NSApplicationIcon"] retain]
    imageNS = [[NSImage alloc] initWithCGImage:QPixmap( mIconPath ).toImage().toCGImage() size:NSZeroSize];
  }
  else
  {
    const QPixmap px = QPixmap::fromImage( settings.image );
    imageNS = [[NSImage alloc] initWithCGImage:px.toImage().toCGImage() size:NSZeroSize];
  }
  */

  UNAuthorizationOptions options = UNAuthorizationOptionAlert;
  [center requestAuthorizationWithOptions:options
          completionHandler: ^ ( BOOL granted, NSError * _Nullable error )
         {
           if ( !granted )
           {
             if ( error != NULL )
             {
               NSLog( @"Error asking for permission to send notifications %@", error );
               // this happens if our app was not signed
             }
             else
             {
               NSLog( @"Unable to get permission to send notifications" );
      }
    }
    else
    {
      doSendNotification( center, titleNS, bodyNS );
    }
  }];

  NotificationResult result;
  result.successful = true;
  return result;
}

void QgsMacNative::doSendNotification( void *voidCenter, NSString *title, NSString *body )
{
  UNUserNotificationCenter *center = ( UNUserNotificationCenter * )voidCenter;
  UNMutableNotificationContent *content = [UNMutableNotificationContent new];
  [content autorelease];
  content.title = title;
  content.body = body;
  // TODO https://stackoverflow.com/questions/43999882/how-to-show-image-in-unnotificationserviceextension
  //content.image = imageNS;

  notifyNum++;
  NSString *identifier = [NSString stringWithFormat:@"qgis-notify-%d", notifyNum];
  UNNotificationRequest *request = [UNNotificationRequest requestWithIdentifier:identifier
                                                          content:content trigger:nil];

  [center addNotificationRequest:request withCompletionHandler: ^ ( NSError * _Nullable error )
         {
           if ( error != nil )
           {
             NSLog( @"Could not send notification: %@", error );
           }
         }];
}

bool QgsMacNative::hasDarkTheme()
{
  // See: https://developer.apple.com/library/archive/documentation/DeveloperTools/Conceptual/cross_development/Using/using.html
  //      Section "Conditionally Compiling for Different SDKs"
  if ( [NSApp respondsToSelector:@selector( effectiveAppearance )] )
  {
    // running on macos 10.14+
    // check the settings of effective appearance of the user
    NSAppearanceName appearanceName = [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]];
    return ( [appearanceName isEqualToString:NSAppearanceNameDarkAqua] );
  }
  else
  {
    // DarkTheme was introduced in MacOS 10.14, fallback to light theme
    return false;
  }
}
