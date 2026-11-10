/*****************************************************************************
 * prefs_page.cpp : the Preferences page of the main window
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "components/prefs_page.hpp"
#include "dialogs/preferences.hpp"
#include "util/breakpoints.hpp"
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <vlc_config_cat.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

using namespace RedesignTokens;

/* the SPrefs categories in their order, then Advanced */
static const struct { const char *label; const char *icon; } sectionList[] = {
    { N_( "Interface" ), RedesignIcons::IconPrefInterface },
    { N_( "Audio" ),     RedesignIcons::IconPrefAudio },
    { N_( "Video" ),     RedesignIcons::IconPrefVideo },
    { SUBPIC_TITLE,      RedesignIcons::IconPrefSubtitles },
    { INPUT_TITLE,       RedesignIcons::IconPrefInput },
    { N_( "Hotkeys" ),   RedesignIcons::IconPrefHotkeys },
    { N_( "Advanced" ),  RedesignIcons::IconPrefAdvanced },
};

/* A row of a card: label and description on the left, the control on the
 * right; the rows after the first draw a hairline above them */
static QFrame *cardRow( const QString &label, const QString &desc,
                        QWidget *control, bool first )
{
    const Theme *t = Theme::instance();
    QFrame *row = new QFrame;
    row->setProperty( "role", first ? "prefsRow" : "prefsRowNext" );
    row->setMinimumHeight( t->px( Prefs::RowMinH ) );
    QHBoxLayout *layout = new QHBoxLayout( row );
    layout->setContentsMargins( t->px( Prefs::RowPadX ), t->px( Prefs::RowPadY ),
                                t->px( Prefs::RowPadX ), t->px( Prefs::RowPadY ) );
    layout->setSpacing( t->px( Prefs::RowGap ) );

    QVBoxLayout *text = new QVBoxLayout;
    text->setSpacing( t->px( Prefs::DescGap ) );
    QLabel *name = new QLabel( label );
    QFont font = t->font( Theme::Body );
    font.setWeight( QFont::Medium );
    name->setFont( font );
    QLabel *hint = new QLabel( desc );
    hint->setProperty( "role", "hint" );
    hint->setFont( t->font( Theme::Meta ) );
    hint->setWordWrap( true );
    text->addWidget( name );
    text->addWidget( hint );
    layout->addLayout( text, 1 );

    control->setAccessibleName( label );
    control->setAccessibleDescription( desc );
    layout->addWidget( control );
    return row;
}

PrefsPage::PrefsPage( intf_thread_t *_p_intf, QWidget *parent )
    : QWidget( parent ), p_intf( _p_intf ), rail( false )
{
    static_assert( sizeof( sectionList ) / sizeof( sectionList[0] ) == SectionCount,
                   "one list entry per section" );
    const Theme *t = Theme::instance();
    setAccessibleName( qtr( "Preferences" ) );
    for( int i = 0; i < SPrefsMax; i++ )
        panels[i] = NULL;

    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( 0 );

    /* the section list */
    QFrame *listPane = new QFrame;
    listPane->setProperty( "role", "prefsList" );
    listPane->setFixedWidth( t->px( Prefs::ListW ) );
    QVBoxLayout *listLayout = new QVBoxLayout( listPane );
    listLayout->setContentsMargins( t->px( Prefs::ListPadX ), t->px( Prefs::ListPadY ),
                                    t->px( Prefs::ListPadX ), t->px( Prefs::ListPadY ) );
    listLayout->setSpacing( 0 );
    title = new QLabel( qtr( "Preferences" ) );
    title->setFont( t->font( Theme::PageTitle ) );
    title->setContentsMargins( t->px( Prefs::ItemPadX ), 0, 0, t->px( Prefs::TitleBelow ) );
    listLayout->addWidget( title );

    sections = new QListWidget;
    sections->setProperty( "role", "prefsSections" );
    sections->setAccessibleName( qtr( "Preferences sections" ) );
    sections->setFrameShape( QFrame::NoFrame );
    sections->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    sections->setIconSize( QSize( t->px( Prefs::ItemIcon ), t->px( Prefs::ItemIcon ) ) );
    for( int i = 0; i < SectionCount; i++ )
    {
        const QString label = qtr( sectionList[i].label );
        QListWidgetItem *item = new QListWidgetItem( label, sections );
        item->setData( Qt::ToolTipRole, label );
        item->setData( Qt::AccessibleTextRole, label );
    }
    updateIcons();
    listLayout->addWidget( sections, 1 );
    layout->addWidget( listPane );

    /* the section, then Cancel and Save */
    QVBoxLayout *right = new QVBoxLayout;
    right->setContentsMargins( t->px( Prefs::ContentPadX ), t->px( Prefs::ContentPadY ),
                               t->px( Prefs::ContentPadX ), t->px( Prefs::ContentPadY ) );
    right->setSpacing( t->px( Prefs::HeaderBelow ) );
    stack = new QStackedWidget;
    right->addWidget( stack, 1 );

    /* Advanced: the complete tree and Reset ("Log to file" has no option, P5-4) */
    advanced = new QWidget;
    QVBoxLayout *advLayout = new QVBoxLayout( advanced );
    advLayout->setContentsMargins( 0, 0, 0, 0 );
    advLayout->setSpacing( t->px( Prefs::HeaderBelow ) );
    QLabel *advTitle = new QLabel( qtr( "Advanced" ) );
    advTitle->setFont( t->font( Theme::PanelTitle ) );
    advLayout->addWidget( advTitle );
    QFrame *card = new QFrame;
    card->setProperty( "role", "prefsCard" );
    QVBoxLayout *cardLayout = new QVBoxLayout( card );
    cardLayout->setContentsMargins( 0, 0, 0, 0 );
    cardLayout->setSpacing( 0 );
    QPushButton *showAll = new QPushButton( qtr( "Open..." ) );
    showAll->setProperty( "role", "outline" );
    connect( showAll, &QPushButton::clicked, this, &PrefsPage::showAllSettings );
    cardLayout->addWidget( cardRow( qtr( "Show all settings" ),
                                    qtr( "The full module tree, for power users." ), showAll, true ) );
    QPushButton *resetButton = new QPushButton( qtr( "Reset..." ) );
    resetButton->setProperty( "role", "outline" );
    connect( resetButton, &QPushButton::clicked, this, &PrefsPage::reset );
    cardLayout->addWidget( cardRow( qtr( "Reset preferences" ),
                                    qtr( "Restore every setting to its default." ), resetButton, false ) );
    advLayout->addWidget( card );
    advLayout->addStretch( 1 );
    stack->addWidget( advanced );

    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->setSpacing( t->px( Prefs::ButtonGap ) );
    buttons->addStretch( 1 );
    QPushButton *cancelButton = new QPushButton( qtr( "&Cancel" ) );
    cancelButton->setToolTip( qtr( "Discard the changes not saved yet" ) );
    connect( cancelButton, &QPushButton::clicked, this, &PrefsPage::cancel );
    buttons->addWidget( cancelButton );
    QPushButton *saveButton = new QPushButton( qtr( "&Save" ) );
    saveButton->setProperty( "role", "primary" );
    saveButton->setToolTip( qtr( "Save the preferences" ) );
    connect( saveButton, &QPushButton::clicked, this, &PrefsPage::save );
    buttons->addWidget( saveButton );
    right->addLayout( buttons );
    layout->addLayout( right, 1 );

    /* the panels are built when first seen, not with the window */
    sections->setCurrentRow( SPrefsDefaultCat );
    connect( sections, &QListWidget::currentRowChanged, this, &PrefsPage::showSection );
    connect( Theme::instance(), &Theme::changed, this, &PrefsPage::updateIcons );
}

void PrefsPage::showEvent( QShowEvent *e )
{
    showSection( sections->currentRow() );
    QWidget::showEvent( e );
}

void PrefsPage::resizeEvent( QResizeEvent *e )
{
    /* an icon rail where the sidebar is one */
    setRail( BreakpointWatcher::fromWidth( window()->width() ) < BreakpointWatcher::M );
    QWidget::resizeEvent( e );
}

void PrefsPage::showSection( int i )
{
    if( i == Advanced )
    {
        stack->setCurrentWidget( advanced );
        return;
    }
    if( i < 0 || i >= SPrefsMax )
        return;
    if( !panels[i] )
    {
        panels[i] = new SPrefsPanel( p_intf, stack, i );
        stack->addWidget( panels[i] );
        /* built after the page, its widgets end the window's focus chain:
         * move them after the list, before Cancel and Save */
        QList<QWidget *> chain;
        for( QWidget *w = panels[i]->nextInFocusChain(); w != panels[i];
             w = w->nextInFocusChain() )
            /* setTabOrder() ignores NoFocus widgets */
            if( ( w->focusPolicy() & Qt::TabFocus ) && panels[i]->isAncestorOf( w ) )
                chain.append( w );
        QWidget *prev = sections;
        foreach( QWidget *w, chain )
        {
            setTabOrder( prev, w );
            prev = w;
        }
    }
    stack->setCurrentWidget( panels[i] );
}

void PrefsPage::rebuild()
{
    for( int i = 0; i < SPrefsMax; i++ )
        if( panels[i] )
        {
            stack->removeWidget( panels[i] );
            delete panels[i];
            panels[i] = NULL;
        }
    if( isVisible() )
        showSection( sections->currentRow() );
}

/* PrefsDialog::save for the simple panels */
void PrefsPage::save()
{
    msg_Dbg( p_intf, "Saving the simple preferences" );
    for( int i = 0; i < SPrefsMax; i++ )
        if( panels[i] )
            panels[i]->apply();
    PrefsDialog::saveConfig( p_intf );
    rebuild();
}

void PrefsPage::cancel()
{
    rebuild();
}

void PrefsPage::reset()
{
    if( PrefsDialog::resetConfig( window(), p_intf ) )
        rebuild();
}

/* the complete tree stays a dialog; what it saves shows here afterwards.
 * shortcut: edits pending on the page are dropped when the dialog saves or
 * resets (stale panels would write old values back), ask first if users
 * hit it */
void PrefsPage::showAllSettings()
{
    PrefsDialog *dialog = new PrefsDialog( window(), p_intf, true );
    connect( dialog, &QDialog::accepted, this, &PrefsPage::rebuild );
    dialog->toggleVisible();
}

void PrefsPage::updateIcons()
{
    Theme *t = Theme::instance();
    const int devicePx = qRound( t->px( Prefs::ItemIcon ) * devicePixelRatioF() );
    const QSize size( devicePx, devicePx );
    for( int i = 0; i < SectionCount; i++ )
    {
        const QLatin1String name( sectionList[i].icon );
        QIcon icon = t->icon( name, Theme::Text2, size );
        icon.addPixmap( t->icon( name, Theme::AccentText, size ).pixmap( size ), QIcon::Selected );
        sections->item( i )->setIcon( icon );
    }
}

void PrefsPage::setRail( bool b )
{
    if( b == rail )
        return;
    rail = b;
    const Theme *t = Theme::instance();
    sections->parentWidget()->setFixedWidth( t->px( rail ? Prefs::RailW : Prefs::ListW ) );
    title->setVisible( !rail );
    for( int i = 0; i < SectionCount; i++ )
        sections->item( i )->setText( rail ? QString() : qtr( sectionList[i].label ) );
}
