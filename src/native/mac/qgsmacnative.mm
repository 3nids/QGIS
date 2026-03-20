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

#include <Cocoa/Cocoa.h>
#include <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <QFileDialog>
#include <QFileInfo>
#include <QPixmap>
#include <QRegularExpression>
#include <QString>

@interface QgsUserNotificationCenterDelegate
    : NSObject <NSUserNotificationCenterDelegate>
@end

@implementation QgsUserNotificationCenterDelegate

- (BOOL)userNotificationCenter:(NSUserNotificationCenter *)center
     shouldPresentNotification:(NSUserNotification *)notification {
#pragma unused(notification)
#pragma unused(center)
  return YES;
}

@end

class QgsMacNative::QgsUserNotificationCenter {
public:
  QgsUserNotificationCenterDelegate *_qgsUserNotificationCenter;
  NSImage *_qgisIcon;
};

QgsMacNative::QgsMacNative()
    : mQgsUserNotificationCenter(
          new QgsMacNative::QgsUserNotificationCenter()) {
  mQgsUserNotificationCenter->_qgsUserNotificationCenter =
      [[QgsUserNotificationCenterDelegate alloc] init];
  [[NSUserNotificationCenter defaultUserNotificationCenter]
      setDelegate:mQgsUserNotificationCenter->_qgsUserNotificationCenter];
}

QgsMacNative::~QgsMacNative() {
  [mQgsUserNotificationCenter->_qgsUserNotificationCenter dealloc];
  delete mQgsUserNotificationCenter;
}

void QgsMacNative::setIconPath(const QString &iconPath) {
  mQgsUserNotificationCenter->_qgisIcon =
      [[NSImage alloc] initWithCGImage:QPixmap(iconPath).toImage().toCGImage()
                                  size:NSZeroSize];
}

const char *QgsMacNative::currentAppLocalizedName() {
  return [[[NSRunningApplication currentApplication] localizedName] UTF8String];
}

void QgsMacNative::currentAppActivateIgnoringOtherApps() {
  [[NSRunningApplication currentApplication]
      activateWithOptions:(NSApplicationActivateAllWindows |
                           NSApplicationActivateIgnoringOtherApps)];
}

void QgsMacNative::openFileExplorerAndSelectFile(const QString &path) {
  NSString *pathStr =
      [[NSString alloc] initWithUTF8String:path.toUtf8().constData()];
  NSArray *fileURLs =
      [NSArray arrayWithObjects:[NSURL fileURLWithPath:pathStr], nil];
  [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:fileURLs];
}

QgsNative::Capabilities QgsMacNative::capabilities() const {
  return NativeDesktopNotifications;
}

QgsNative::NotificationResult QgsMacNative::showDesktopNotification(
    const QString &summary, const QString &body,
    const QgsNative::NotificationSettings &settings) {
  NSUserNotification *notification = [[NSUserNotification alloc] init];
  notification.title = summary.toNSString();
  notification.informativeText = body.toNSString();
  notification.soundName =
      NSUserNotificationDefaultSoundName; // Will play a default sound
  NSImage *image = nil;
  if (settings.image.isNull()) {
    // image application (qgis.icns) seems not to be set for now, although
    // present in the plist whenever fixed, try following line (and remove
    // corresponding code in QgsMacNative::QgsUserNotificationCenter) image =
    // [[NSImage imageNamed:@"NSApplicationIcon"] retain]
    image = mQgsUserNotificationCenter->_qgisIcon;
  } else {
    const QPixmap px = QPixmap::fromImage(settings.image);
    image = [[NSImage alloc] initWithCGImage:px.toImage().toCGImage()
                                        size:NSZeroSize];
  }
  notification.contentImage = image;

  [[NSUserNotificationCenter defaultUserNotificationCenter]
      deliverNotification:notification];
  [notification autorelease];

  //[userCenterDelegate dealloc];

  NotificationResult result;
  result.successful = true;
  return result;
}

bool QgsMacNative::hasDarkTheme() {
#ifdef __MAC_OS_X_VERSION_MAX_ALLOWED
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101400
  // Version comparison needs to be numeric, in case __MAC_10_10_4 is not
  // defined, e.g. some pre-10.14 SDKs See:
  // https://developer.apple.com/library/archive/documentation/DeveloperTools/Conceptual/cross_development/Using/using.html
  //      Section "Conditionally Compiling for Different SDKs"
  if ([NSApp respondsToSelector:@selector(effectiveAppearance)]) {
    // compiled on macos 10.14+ AND running on macos 10.14+
    // check the settings of effective appearance of the user
    NSAppearanceName appearanceName =
        [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:@[
          NSAppearanceNameAqua, NSAppearanceNameDarkAqua
        ]];
    return ([appearanceName isEqualToString:NSAppearanceNameDarkAqua]);
  } else {
    // compiled on macos 10.14+ BUT running on macos 10.13-
    // DarkTheme was introduced in MacOS 10.14, fallback to light theme
    return false;
  }
#endif
#endif
  // compiled on macos 10.13-
  // NSAppearanceNameDarkAqua is not in SDK headers
  // fallback to light theme
  return false;
}

// ---------- helpers for DontConfirmOverwrite (NSSavePanel) ----------------

static void qgsMacSetOverwriteAlertSuppression(NSSavePanel *panel, NSURL *url) {
  SEL sel = NSSelectorFromString(@"_setOverwritingAlertSuppressionURL:");
  if (![panel respondsToSelector:sel])
    return;
  NSMethodSignature *sig = [panel methodSignatureForSelector:sel];
  NSInvocation *inv = [NSInvocation invocationWithMethodSignature:sig];
  [inv setTarget:panel];
  [inv setSelector:sel];
  NSURL *arg = url;
  [inv setArgument:&arg atIndex:2];
  [inv invoke];
}

// Delegate that keeps the private overwrite-alert suppression URL in sync
// with whatever the user types / navigates to in the save panel.
// panelSelectionDidChange / didChangeToDirectoryURL don't fire on typing, so
// we also rely on a polling timer (started before runModal) to catch changes.
@interface QgsOverwriteSuppressionDelegate
    : NSObject <NSOpenSavePanelDelegate> {
  NSURL *_currentDir;
  NSSavePanel *_panel;
}
- (instancetype)initWithPanel:(NSSavePanel *)panel;
- (void)pollSuppression;
@end

@implementation QgsOverwriteSuppressionDelegate

- (instancetype)initWithPanel:(NSSavePanel *)panel {
  self = [super init];
  if (self)
    _panel = panel;
  return self;
}

- (void)panel:(id)sender didChangeToDirectoryURL:(NSURL *)url {
  _currentDir = url;
  [self pollSuppression];
}

- (void)panelSelectionDidChange:(id)sender {
  [self pollSuppression];
}

- (BOOL)panel:(id)sender validateURL:(NSURL *)url error:(NSError **)outError {
  qgsMacSetOverwriteAlertSuppression(sender, url);
  return YES;
}

- (void)pollSuppression {
  NSURL *dir = _currentDir ? _currentDir : [_panel directoryURL];
  NSString *name = [_panel nameFieldStringValue];
  if (dir && name.length > 0) {
    NSURL *fileURL = [dir URLByAppendingPathComponent:name];
    qgsMacSetOverwriteAlertSuppression(_panel, fileURL);
  }
}

@end

// -------------------------------------------------------------------------

// Helper: parse a Qt filter string like "GeoPackage (*.gpkg);;All files (*)"
// into a list of { displayName, [extensions] } pairs.
struct QgsMacFilterEntry {
  QString displayName;
  QStringList extensions; // without the leading dot, e.g. "gpkg"
};

static QList<QgsMacFilterEntry> parseSaveFilters(const QString &filter) {
  QList<QgsMacFilterEntry> result;
  if (filter.isEmpty())
    return result;

  const QStringList parts = filter.split(QStringLiteral(";;"));
  const QRegularExpression extRe(QStringLiteral("\\*\\.([\\w.]+)"));
  for (const QString &part : parts) {
    QgsMacFilterEntry entry;
    // "Description (*.ext1 *.ext2)" → description is everything before '('
    const int parenPos = part.indexOf('(');
    entry.displayName =
        (parenPos > 0) ? part.left(parenPos).trimmed() : part.trimmed();

    QRegularExpressionMatchIterator it = extRe.globalMatch(part);
    while (it.hasNext()) {
      entry.extensions.append(it.next().captured(1).toLower());
    }
    result.append(entry);
  }
  return result;
}

QString QgsMacNative::getSaveFileName(QWidget *parent, const QString &caption,
                                      const QString &dir, const QString &filter,
                                      QString *selectedFilter,
                                      QFileDialog::Options options) {
  if (!options.testFlag(QFileDialog::DontConfirmOverwrite)) {
    // Normal case — just use QFileDialog which shows the native panel with
    // the default overwrite confirmation.
    return QFileDialog::getSaveFileName(parent, caption, dir, filter,
                                        selectedFilter, options);
  }

  // --- DontConfirmOverwrite requested: drive NSSavePanel ourselves ----------

  @autoreleasepool {
    NSSavePanel *panel = [NSSavePanel savePanel];

    // Title
    if (!caption.isEmpty())
      [panel setTitle:caption.toNSString()];

    // Directory & initial filename
    const QFileInfo dirInfo(dir);
    if (dirInfo.isDir()) {
      [panel setDirectoryURL:[NSURL fileURLWithPath:dir.toNSString()]];
    } else if (!dir.isEmpty()) {
      const QString directory = dirInfo.absolutePath();
      const QString fileName = dirInfo.fileName();
      [panel setDirectoryURL:[NSURL fileURLWithPath:directory.toNSString()]];
      if (!fileName.isEmpty())
        [panel setNameFieldStringValue:fileName.toNSString()];
    }

    // Parse filters and build allowed content types for the first filter
    const QList<QgsMacFilterEntry> filters = parseSaveFilters(filter);

    // Collect all extensions from the first (default) filter for allowed types
    NSMutableArray<UTType *> *allowedTypes = [NSMutableArray array];
    if (!filters.isEmpty()) {
      for (const QString &ext : filters.first().extensions) {
        UTType *type = [UTType typeWithFilenameExtension:ext.toNSString()];
        if (type)
          [allowedTypes addObject:type];
      }
    }
    if ([allowedTypes count] > 0)
      [panel setAllowedContentTypes:allowedTypes];
    else
      [panel setAllowsOtherFileTypes:YES];

    [panel setCanCreateDirectories:YES];
    [panel setExtensionHidden:NO];

    // Suppress overwrite confirmation via private API.
    // A dispatch timer polls the filename field every 100ms and keeps the
    // suppression URL in sync so it is already set when the user clicks Save.
    QgsOverwriteSuppressionDelegate *delegate =
        [[QgsOverwriteSuppressionDelegate alloc] initWithPanel:panel];
    [panel setDelegate:delegate];

    // Set initial suppression URL from the pre-populated filename.
    [delegate pollSuppression];

    // Start a repeating timer that runs during the modal session.
    dispatch_source_t timer = dispatch_source_create(
        DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(timer, DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC,
                              50 * NSEC_PER_MSEC);
    dispatch_source_set_event_handler(timer, ^{
        [delegate pollSuppression];
    });
    dispatch_resume(timer);

    // Show the panel
    const NSModalResponse result = [panel runModal];
    dispatch_source_cancel(timer);
    if (result != NSModalResponseOK)
      return QString();

    const QString selectedPath = QString::fromNSString([[panel URL] path]);

    // Determine which filter matched, for selectedFilter output
    if (selectedFilter && !filters.isEmpty()) {
      const QString ext = QFileInfo(selectedPath).suffix().toLower();
      bool found = false;
      for (int i = 0; i < filters.size(); ++i) {
        if (filters[i].extensions.contains(ext)) {
          // Reconstruct the original filter string for this entry
          const QStringList parts = filter.split(QStringLiteral(";;"));
          if (i < parts.size())
            *selectedFilter = parts[i].trimmed();
          found = true;
          break;
        }
      }
      if (!found && !filter.isEmpty()) {
        // Default to first filter
        *selectedFilter = filter.split(QStringLiteral(";;")).first().trimmed();
      }
    }

    return selectedPath;
  }
}
