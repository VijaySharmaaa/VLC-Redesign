/*****************************************************************************
 * title_bar.hpp : the main window's title bar with its menu bar
 *****************************************************************************
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_QT_TITLE_BAR_HPP_
#define VLC_QT_TITLE_BAR_HPP_

#include "qt.hpp"

#include <QWidget>

class QMenuBar;
class QToolButton;

/* The main window's menu widget (QMainWindow::setMenuWidget): the cone, the
 * menu bar, the window title and, when the window has no native caption,
 * the minimise / maximise / close buttons. It only paints and reports what
 * lies under a point; removing the native caption is the platform's job
 * (MainInterfaceWin32). */
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    enum Area { Client, Caption, MaximizeButton };

    explicit TitleBar( QWidget *window );

    QMenuBar *menuBar() const { return bar; }

    /* Without a native caption: show the window buttons and the title. */
    void setCaptionButtonsVisible( bool );
    bool hasCaptionButtons() const { return b_captionButtons; }
    /* Minimal view keeps a slim strip with the window buttons, or nothing
     * under a native caption. Video full screen hides the bar; interface
     * full screen (F11) keeps the menus, as before the redesign, without
     * the window buttons. */
    void setMinimal( bool );
    void setFullScreen( bool fullScreen, bool video );
    /* Alt in minimal view: show or hide the menus inside the strip. */
    void toggleMenus();
    /* Height the bar takes in the window, 0 when hidden. */
    int barHeight() const { return isHidden() ? 0 : maximumHeight(); }

    /* What is under pos (title bar coordinates), for the window's hit test. */
    Area areaAt( const QPoint &pos ) const;
    /* The maximise button is non-client (snap layouts): Qt sees neither
     * its hover nor its clicks, the window's message handler reports them;
     * clicked() still comes from the keyboard and accessibility clients. */
    void setMaximizeHovered( bool );
    void toggleMaximized();

signals:
    /* The cone: "go to library" */
    void libraryRequested();

protected:
    bool eventFilter( QObject *, QEvent * ) Q_DECL_OVERRIDE;
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;

private:
    QToolButton *addButton( const QString &name );
    void updateIcons();
    void updateLayout();

    QWidget *window;
    QMenuBar *bar;
    QToolButton *cone, *minButton, *maxButton, *closeButton;
    bool b_captionButtons, b_minimal, b_fullScreen, b_videoFullScreen;
};

#endif
