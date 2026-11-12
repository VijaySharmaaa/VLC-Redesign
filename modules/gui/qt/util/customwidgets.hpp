/*****************************************************************************
 * customwidgets.hpp: Custom widgets
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * Copyright (C) 2004 Daniel Molkentin <molkentin@kde.org>
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 * The "ClickLineEdit" control is based on code by  Daniel Molkentin
 * <molkentin@kde.org> for libkdepim
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

#ifndef VLC_QT_CUSTOMWIDGETS_HPP_
#define VLC_QT_CUSTOMWIDGETS_HPP_

#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QToolButton>
#include <QDial>

#include "animators.hpp"
#include "qt.hpp"

class QFrame;
class QWidget;

class QFramelessButton : public QPushButton
{
    Q_OBJECT
public:
    QFramelessButton( QWidget *parent = NULL );
    QSize sizeHint() const Q_DECL_OVERRIDE { return iconSize(); }
protected:
    void paintEvent( QPaintEvent * event ) Q_DECL_OVERRIDE;
};

class VLCQDial : public QDial
{
    Q_OBJECT
public:
    VLCQDial( QWidget *parent = NULL );
protected:
    void paintEvent( QPaintEvent * event ) Q_DECL_OVERRIDE;
};

class QToolButtonExt : public QToolButton
{
    Q_OBJECT
public:
    QToolButtonExt( QWidget *parent = 0, int ms = 0 );
private:
    bool shortClick;
    bool longClick;
private slots:
    void releasedSlot();
    void clickedSlot();
signals:
    void shortClicked();
    void longClicked();
};

class QElidingLabel : public QLabel
{
public:
    QElidingLabel( const QString &s = QString(),
                      Qt::TextElideMode mode = Qt::ElideRight,
                      QWidget * parent = NULL );
    void setElideMode( Qt::TextElideMode );
protected:
    void paintEvent( QPaintEvent * event ) Q_DECL_OVERRIDE;
private:
    Qt::TextElideMode elideMode;
};


class QVLCStackedWidget : public QStackedWidget
{
public:
    QVLCStackedWidget( QWidget *parent ) : QStackedWidget( parent ) { }
    QSize minimumSizeHint () const
    {
        return currentWidget() ? currentWidget()->minimumSizeHint() : QSize();
    }
};

class QVLCDebugLevelSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    QVLCDebugLevelSpinBox( QWidget *parent ) : QSpinBox( parent ) { };
protected:
    QString textFromValue( int ) const Q_DECL_OVERRIDE;
    /* QVLCDebugLevelSpinBox is read-only */
    int valueFromText( const QString& ) const Q_DECL_OVERRIDE { return -1; }
};

/** This spinning icon, to the colors of the VLC cone, will show
 * that there is some background activity running
 **/
class SpinningIcon : public QLabel
{
    Q_OBJECT

public:
    SpinningIcon( QWidget *parent );
    void play( int loops = -1, int fps = 0 )
    {
        animator->setLoopCount( loops );
        if ( fps ) animator->setFps( fps );
        animator->start();
    }
    void stop() { animator->stop(); }
    bool isPlaying() { return animator->state() == PixmapAnimator::Running; }
private:
    PixmapAnimator *animator;
};

class YesNoCheckBox : public QCheckBox
{
    Q_OBJECT
public:
    YesNoCheckBox( QWidget *parent );
};

/* VLC Key/Wheel hotkeys interactions */

class QKeyEvent;
class QWheelEvent;
class QInputEvent;

int qtKeyModifiersToVLC( const QInputEvent* e );
int qtEventToVLCKey( const QKeyEvent *e );
int qtWheelEventToVLCKey( const QWheelEvent *e );
QString VLCKeyToString( unsigned val, bool );
/* The text of a shortcut: a key-* hotkey's first binding, or a Qt key
 * sequence such as a menu's "Ctrl+E" (MENU_KEY_*, translated as the menu
 * does); empty when unbound or NULL. p_intf may be NULL for a sequence. */
QString hotkeyText( intf_thread_t *, const char *id );
/* "Label (key)" with hotkeyText(), the bare label when there is no key */
QString withHotkey( intf_thread_t *, const QString &label, const char *id );
/* Keeps the shortcut of w's "hotkey" property (an id as hotkeyText takes)
 * current: added to its tooltip, which holds the bare label, when the
 * tooltip shows, and its accessible description on focus and hover, so a
 * rebound key shows without a restart */
void watchHotkey( intf_thread_t *, QWidget *w );
/* Sets a dynamic property that the stylesheet selects on and repolishes
 * w, as selectors are only re-read on polish; nothing when unchanged */
void setStyleProperty( QWidget *w, const char *name, const QVariant &value );
/* "9:36" under an hour, "1:02:03" above: no leading zero (DESIGN_SPEC 3) */
QString timeString( int64_t secs );
/* The position as InputManager::positionUpdated gives it: pattern ("%1 of
 * %2") with the time and the length, the time alone when the length is
 * unknown, empty without a position */
QString positionText( float pos, int64_t time, int length, const QString &pattern );
/* The playback speed: "1.50×" */
QString rateText( float rate );
/* A row of a card (Preferences, the Synchronization tab): title in Medium
 * Body over a wrapping Meta hint (none when desc is empty), the controls on
 * the right, gap between them. The row's QSS role is role; item 0 of its
 * layout is the text column. */
QFrame *cardRow( QLabel *title, const QString &desc, const QList<QWidget *> &controls,
                 const char *role, const QMargins &pad, int gap, int descGap,
                 int controlGap );
/* UI Automation reads neither a buddy label nor an editable combo box's
 * name for its edit: under root, each label's buddy without a name gets
 * the label's text, and each editable combo box's edit the box's name */
void nameFromLabels( QWidget *root );
/* Call once the widgets under root are built: see customwidgets.cpp */
void dropEarlyAccessibles( QWidget *root );

/**
 * @brief The WheelToVLCConverter class aggregates wheel events and
 * emit a signal once it gathers a full scroll step, as VLC doesn't handle
 * fractionnal scroll events
 */
class WheelToVLCConverter : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

public:
    Qt::Orientations getWheelOrientation(int x, int y);

signals:
    /**
     * @param vlcKey the VLC hotkey representation
     */
    void vlcWheelKey(int vlcKey);
    /**
     * @param steps: positive value indicated UP wheel events, negative DOWN wheel event
     * @param modifiers are keyboard pressed modifiers
     */
    void wheelUpDown(int steps, Qt::KeyboardModifiers modifiers);
    /**
     * @param steps: positive value indicated UP wheel events, negative DOWN wheel event
     * @param modifiers are keyboard pressed modifiers
     */
    void wheelLeftRight(int steps, Qt::KeyboardModifiers modifiers);

public slots:
    /**
     * @brief qmlWheelEvent handles wheel events as emitted by QWidget
     * @param e the wheel event
     */
    void wheelEvent(const QWheelEvent* e);

private:
    QPoint m_scrollAmount = {};
    Qt::KeyboardModifiers m_modifiers = {};
    Qt::MouseButtons m_buttons = {};
};

#endif
