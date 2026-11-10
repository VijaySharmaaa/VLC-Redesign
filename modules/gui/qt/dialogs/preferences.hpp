/*****************************************************************************
 * preferences.hpp : Preferences
 *****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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

#ifndef QVLC_PREFS_DIALOG_H_
#define QVLC_PREFS_DIALOG_H_ 1

#include "util/qvlcframe.hpp"
#include "components/simple_preferences.hpp"

class PrefsTree;
class SPrefsCatList;
class SPrefsPanel;
class QTreeWidgetItem;
class QGroupBox;
class QRadioButton;
class QWidget;
class QCheckBox;
class SearchLineEdit;
class QStackedWidget;
class QSplitter;

class PrefsDialog : public QVLCDialog
{
    Q_OBJECT
public:
    /* advanced: open on the complete tree whatever the options say */
    PrefsDialog( QWidget *, intf_thread_t *, bool advanced = false );
    virtual ~PrefsDialog();
    /* Save and Reset, shared with the Preferences page: write the applied
     * configuration and reload what depends on it; ask, then reset
     * everything (true when done) */
    static void saveConfig( intf_thread_t * );
    static bool resetConfig( QWidget *, intf_thread_t * );
#if 0
    /*Called from extended settings, is not used anymore, but could be useful one day*/
    void showModulePrefs( char* );
#endif

private:
    enum { SIMPLE, ADVANCED };
    QStackedWidget *stack;

    QWidget *simple_split_widget;
    QSplitter *advanced_split_widget;

    QStackedWidget *advanced_panels_stack;
    QStackedWidget *simple_panels_stack;
    SPrefsPanel *simple_panels[SPrefsMax];

    QWidget *simple_tree_panel;
    QWidget *advanced_tree_panel;

    SPrefsCatList *simple_tree;
    PrefsTree *advanced_tree;
    size_t count;
    module_t **p_list;
    SearchLineEdit *tree_filter;
    QCheckBox *current_filter;

    QGroupBox *types;
    QRadioButton *simple,*all;

private slots:
    void setAdvanced();
    void setSimple();

    void changeAdvPanel( QTreeWidgetItem * );
    void changeSimplePanel( int );
    void advancedTreeFilterChanged( const QString & );
    void onlyLoadedToggled();

    void save();
    void cancel();
    void reset();
    void close() { save(); };
};

#endif
