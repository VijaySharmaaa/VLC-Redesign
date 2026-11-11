/*****************************************************************************
 * extended.hpp : Extended controls - Undocked
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef QVLC_EXTENDED_DIALOG_H_
#define QVLC_EXTENDED_DIALOG_H_ 1

#include "qt.hpp"

#include "components/extended_panels.hpp"
#include "util/breakpoints.hpp"
#include "util/singleton.hpp"

#include <QWidget>

class QBoxLayout;
class QPushButton;
class QTabWidget;
class QToolButton;
class QHBoxLayout;
class QVariantAnimation;
class OverlayHost;

/* The Effects and filters sheet (DESIGN_SPEC 7, decision P5-1). With the
 * main window it docks at the right of the central stack like the
 * PlaylistPanel (the video shrinks), and at XS it is a tool window over the
 * right of the stack. Without one (Qt only as dialog provider) it is a
 * window of its own that keeps EPanel/geometry. The two docked panels share
 * the right side: opening one closes the other. */
class ExtendedDialog : public QWidget, public Singleton<ExtendedDialog>
{
    Q_OBJECT
public:
    /* the top-level tab indexes DialogsProvider relies on */
    enum
    {
        AUDIO_TAB = 0,
        VIDEO_TAB,
        SYNCHRO_TAB,
        V4L2_TAB
    };
    void showTab( int i );
    int currentTab();

public slots:
    void setBreakpoint( BreakpointWatcher::Size );

protected:
    /* the deck's effects button shows open meanwhile */
    void showEvent( QShowEvent * ) Q_DECL_OVERRIDE;
    void hideEvent( QHideEvent * ) Q_DECL_OVERRIDE;
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    void resizeEvent( QResizeEvent * ) Q_DECL_OVERRIDE;
    void keyPressEvent( QKeyEvent * ) Q_DECL_OVERRIDE;

private:
    ExtendedDialog( intf_thread_t * );
    virtual ~ExtendedDialog();

    void setShownWidth( int );
    void place();
    int shadowWidth() const;
    void showPage( QTabWidget *pages, int page );
    void updateMore();

    intf_thread_t *p_intf;
    QBoxLayout *dock;           /* NULL without the main window */
    QWidget *dockParent;
    OverlayHost *host;
    QWidget *content;           /* full width, revealed as the sheet slides */
    QVariantAnimation *slide;
    int fullWidth;
    int shownWidth;
    bool drawer;

    SyncControls *syncW;
    ExtVideo *videoEffect;
    Equalizer *equal;
    QTabWidget *mainTabW;
    QTabWidget *audioTab;
    QTabWidget *videoTab;
    QToolButton *moreButton;
    /* "More" beside the tabs, or in the header in the narrow drawer */
    QWidget *moreCorner;
    QHBoxLayout *head;
    QPushButton *m_applyButton;
    QHash<QString, QVariant> m_hashConfigs[2];
private slots:
    void changedItem( int );
    void currentTabChanged( int );
    void saveConfig();
    void putAudioConfig( const QString& name, const QVariant value );
    void putVideoConfig( const QString& name, const QVariant value );

    friend class    Singleton<ExtendedDialog>;
};

#endif
