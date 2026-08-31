/***************************************************************************
  qgsdropfeedbackoverlay.h
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

#ifndef QGSDROPFEEDBACKOVERLAY_H
#define QGSDROPFEEDBACKOVERLAY_H

#include "qgis.h"
#include "qgis_gui.h"
#include "qgis_sip.h"

#include <QWidget>

/**
 * \ingroup gui
 * \brief Tells the user, over the widget they are dragging data onto, what dropping it
 * would do.
 *
 * Only a payload worth a warning is announced: one which would replace the project, and
 * one nothing in QGIS can read. Anything else drops without ceremony, and the overlay
 * hides itself.
 *
 * The overlay covers the widget it is a child of and follows its size, so it belongs on
 * the widget whose whole surface the message is about. It is hidden until announce() is
 * called and hides again on clear().
 *
 * \since QGIS 4.4
 */
class GUI_EXPORT QgsDropFeedbackOverlay : public QWidget
{
    Q_OBJECT

  public:
    /**
     * Constructor for QgsDropFeedbackOverlay, covering \a parent.
     */
    explicit QgsDropFeedbackOverlay( QWidget *parent SIP_TRANSFERTHIS );

    /**
     * Announces that dropping data of the given \a payloadType is about to happen, naming
     * it \a name in the message when one is known, such as the file being dragged.
     *
     * Shows the overlay when the payload is one the user should be warned about, and hides
     * it otherwise.
     */
    void announce( Qgis::DropPayloadType payloadType, const QString &name = QString() );

    //! Hides the overlay, once the drag it was announcing is over.
    void clear();

  protected:
    void paintEvent( QPaintEvent *event ) override;
    bool eventFilter( QObject *watched, QEvent *event ) override;

  private:
    //! Returns TRUE if the payload is one the user should be warned about before dropping
    static bool isWorthAnnouncing( Qgis::DropPayloadType payloadType );

    Qgis::DropPayloadType mPayloadType = Qgis::DropPayloadType::Unknown;
    QString mTitle;
    QString mSubtitle;
    QIcon mIcon;
};

#endif // QGSDROPFEEDBACKOVERLAY_H
