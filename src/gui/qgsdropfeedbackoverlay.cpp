/***************************************************************************
  qgsdropfeedbackoverlay.cpp
  --------------------------------------
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

#include "qgsdropfeedbackoverlay.h"

#include "qgsapplication.h"

#include <QEvent>
#include <QFontMetrics>
#include <QIcon>
#include <QPaintEvent>
#include <QPainter>
#include <QString>

#include "moc_qgsdropfeedbackoverlay.cpp"

using namespace Qt::StringLiterals;

//! Darkens whatever the message is drawn over, so that the eye lands on the message
static const QColor VEIL_COLOR = QColor( 0, 0, 0, 100 );

//! Colors of the card, taken from the message bar so that the two speak with one voice
static const QColor WARNING_BACKGROUND = QColor( 0xff, 0xc8, 0x00 );
static const QColor WARNING_BORDER = QColor( 0xe0, 0xaa, 0x00 );
static const QColor WARNING_TEXT = QColor( Qt::black );
static const QColor CRITICAL_BACKGROUND = QColor( 0xd6, 0x52, 0x53 );
static const QColor CRITICAL_BORDER = QColor( 0x9b, 0x3d, 0x3d );
static const QColor CRITICAL_TEXT = QColor( Qt::white );

QgsDropFeedbackOverlay::QgsDropFeedbackOverlay( QWidget *parent )
  : QWidget( parent )
{
  Q_ASSERT( parent );

  // the overlay is a bystander: the drag must reach the widget underneath it
  setAttribute( Qt::WA_TransparentForMouseEvents );
  setAcceptDrops( false );
  setGeometry( parent->rect() );
  parent->installEventFilter( this );
  hide();
}

bool QgsDropFeedbackOverlay::isWorthAnnouncing( Qgis::DropPayloadType payloadType )
{
  switch ( payloadType )
  {
    case Qgis::DropPayloadType::Project:
    case Qgis::DropPayloadType::Unsupported:
      return true;

    case Qgis::DropPayloadType::Unknown:
    case Qgis::DropPayloadType::Layers:
    case Qgis::DropPayloadType::CustomHandler:
      return false;
  }
  return false;
}

void QgsDropFeedbackOverlay::announce( Qgis::DropPayloadType payloadType, const QString &name )
{
  if ( !isWorthAnnouncing( payloadType ) )
  {
    clear();
    return;
  }

  mPayloadType = payloadType;
  switch ( payloadType )
  {
    case Qgis::DropPayloadType::Project:
      mTitle = name.isEmpty() ? tr( "Open project" ) : tr( "Open project “%1”" ).arg( name );
      mSubtitle = tr( "The project currently open will be closed" );
      mIcon = QgsApplication::getThemeIcon( u"/mIconWarning.svg"_s );
      break;

    case Qgis::DropPayloadType::Unsupported:
      mTitle = name.isEmpty() ? tr( "This cannot be opened in QGIS" ) : tr( "“%1” cannot be opened in QGIS" ).arg( name );
      mSubtitle = tr( "No data provider or plugin reads it" );
      mIcon = QgsApplication::getThemeIcon( u"/mIconCritical.svg"_s );
      break;

    case Qgis::DropPayloadType::Unknown:
    case Qgis::DropPayloadType::Layers:
    case Qgis::DropPayloadType::CustomHandler:
      break;
  }

  if ( QWidget *covered = parentWidget() )
    setGeometry( covered->rect() );
  raise();
  show();
  update();
}

void QgsDropFeedbackOverlay::clear()
{
  mTitle.clear();
  mSubtitle.clear();
  mIcon = QIcon();
  mPayloadType = Qgis::DropPayloadType::Unknown;
  hide();
}

bool QgsDropFeedbackOverlay::eventFilter( QObject *watched, QEvent *event )
{
  if ( watched == parent() && event->type() == QEvent::Resize )
    setGeometry( parentWidget()->rect() );

  return QWidget::eventFilter( watched, event );
}

void QgsDropFeedbackOverlay::paintEvent( QPaintEvent * )
{
  if ( mTitle.isEmpty() )
    return;

  const bool critical = mPayloadType == Qgis::DropPayloadType::Unsupported;
  const QColor background = critical ? CRITICAL_BACKGROUND : WARNING_BACKGROUND;
  const QColor border = critical ? CRITICAL_BORDER : WARNING_BORDER;
  const QColor text = critical ? CRITICAL_TEXT : WARNING_TEXT;

  QFont titleFont = font();
  titleFont.setBold( true );
  titleFont.setPointSizeF( titleFont.pointSizeF() * 1.3 );
  const QFontMetrics titleMetrics( titleFont );
  const QFontMetrics subtitleMetrics( font() );

  // every measure follows the text, so that the message holds together at any font size
  const int unit = titleMetrics.height();
  const int padding = unit;
  const int spacing = unit / 2;
  const int iconSize = unit * 2;
  const int radius = unit / 2;

  const int available = width() - 4 * padding - iconSize - spacing;
  const QString title = titleMetrics.elidedText( mTitle, Qt::ElideMiddle, available );
  const QString subtitle = subtitleMetrics.elidedText( mSubtitle, Qt::ElideRight, available );

  const int textWidth = std::max( titleMetrics.horizontalAdvance( title ), subtitleMetrics.horizontalAdvance( subtitle ) );
  const int textHeight = titleMetrics.height() + spacing / 2 + subtitleMetrics.height();
  const QSize card( iconSize + spacing + textWidth + 2 * padding, std::max( iconSize, textHeight ) + 2 * padding );
  const QRect cardRect( QPoint( ( width() - card.width() ) / 2, ( height() - card.height() ) / 2 ), card );

  QPainter painter( this );
  painter.setRenderHint( QPainter::Antialiasing );

  painter.fillRect( rect(), VEIL_COLOR );

  painter.setPen( QPen( border, 1 ) );
  painter.setBrush( background );
  painter.drawRoundedRect( cardRect, radius, radius );

  const QRect iconRect( cardRect.left() + padding, cardRect.top() + ( cardRect.height() - iconSize ) / 2, iconSize, iconSize );
  mIcon.paint( &painter, iconRect );

  const int textLeft = iconRect.right() + spacing;
  const int textTop = cardRect.top() + ( cardRect.height() - textHeight ) / 2;

  painter.setPen( text );
  painter.setFont( titleFont );
  painter.drawText( QRect( textLeft, textTop, textWidth, titleMetrics.height() ), Qt::AlignLeft | Qt::AlignVCenter, title );

  painter.setFont( font() );
  painter.drawText( QRect( textLeft, textTop + titleMetrics.height() + spacing / 2, textWidth, subtitleMetrics.height() ), Qt::AlignLeft | Qt::AlignVCenter, subtitle );
}
