/*****************************************************************************
 * prefs_page.hpp : the Preferences page of the main window
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_QT_PREFS_PAGE_HPP_
#define VLC_QT_PREFS_PAGE_HPP_

#include "qt.hpp"
#include "components/simple_preferences.hpp"

#include <QDateTime>
#include <QWidget>

class QLabel;
class QPushButton;
class QStackedWidget;

/* DESIGN_SPEC 8: Simple Preferences as a page of the window, not modal.
 * The sections are the dialog's SPrefsPanels, built on first view; edits
 * stay pending until Save or Cancel, as in PrefsDialog, after which the
 * panels are built again from the configuration. Advanced holds Reset and
 * opens the complete tree, which stays the PrefsDialog. The sidebar lists
 * the sections and picks the one shown. */
class PrefsPage : public QWidget
{
    Q_OBJECT
public:
    /* the SPrefs categories in their order, then Advanced */
    enum { Advanced = SPrefsMax, SectionCount };
    static const struct Section { const char *label; const char *icon; } sections[SectionCount];

    /* Tab comes into the page from tabAnchor, the last section */
    PrefsPage( intf_thread_t *, QWidget *tabAnchor );

public slots:
    void showSection( int );
    /* drop the edits not saved */
    void cancel();
    /* drop the panels: the next view builds them from the configuration */
    void rebuild();

protected:
    void showEvent( QShowEvent * ) Q_DECL_OVERRIDE;

private slots:
    void save();
    void reset();
    void showAllSettings();
    void configSaved();

private:
    intf_thread_t *p_intf;
    QWidget *tabAnchor;     /* NULL once the page's own buttons follow it */
    QLabel *title;
    QStackedWidget *stack;
    QWidget *advanced;
    QPushButton *showAll, *resetButton, *cancelButton, *saveButton;
    SPrefsPanel *panels[SPrefsMax];
    int section;
    QString configFile;     /* vlcrc */
    QDateTime configTime;   /* its time when the panels last matched it */
};

#endif
