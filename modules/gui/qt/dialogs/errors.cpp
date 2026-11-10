/*****************************************************************************
 * errors.cpp : Errors
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#include "dialogs/errors.hpp"
#include "dialogs_provider.hpp"
#include "input_manager.hpp"
#include "util/redesign_icons.hpp"
#include "util/theme.hpp"

#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>

#include <vlc_input.h>
#include <vlc_playlist.h>

using namespace RedesignTokens;

ErrorsDialog::ErrorsDialog( intf_thread_t *_p_intf )
             : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf ), failedItem( NULL )
{
    setWindowTitle( qtr( "Errors" ) );
    setWindowRole( "vlc-errors" );
    /* a modal sheet in the design (1.6.5): Close only, no min/max */
    setWindowFlags( Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                    Qt::WindowCloseButtonHint );

    Theme *t = Theme::instance();
    resize( t->px( Dialogs::ErrorW ), t->px( Dialogs::ErrorH ) );
    QGridLayout *layout = new QGridLayout( this );

    /* DESIGN_SPEC 7 (1.6.5): a round error icon, the latest title, the
     * explanation in the body font with only the MRL in mono */
    QLabel *icon = new QLabel;
    icon->setProperty( "role", "errorIcon" );
    icon->setFixedSize( t->px( Dialogs::ErrorIcon ), t->px( Dialogs::ErrorIcon ) );
    icon->setAlignment( Qt::AlignCenter );
    const int glyph = t->px( Dialogs::ErrorGlyph );
    icon->setPixmap( t->icon( RedesignIcons::IconError, Theme::Error, QSize( glyph, glyph ) )
                         .pixmap( glyph, glyph ) );

    title = new QLabel;
    title->setFont( t->font( Theme::PanelTitle ) );
    title->setWordWrap( true );
    title->setTextInteractionFlags( Qt::TextSelectableByMouse );

    body = new QLabel;
    body->setFont( t->font( Theme::Body ) );
    body->setProperty( "role", "hint" );
    body->setWordWrap( true );
    body->setTextFormat( Qt::RichText );
    body->setAlignment( Qt::AlignLeft | Qt::AlignTop );
    body->setTextInteractionFlags( Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard );
    body->setFocusPolicy( Qt::StrongFocus );
    body->setAccessibleName( qtr( "Error details" ) );
    /* plain text, no box: it scrolls only once several errors pile up */
    QScrollArea *scroll = new QScrollArea;
    scroll->setFrameShape( QFrame::NoFrame );
    scroll->setWidgetResizable( true );
    scroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    scroll->setWidget( body );
    /* the named details are the Tab stop, not the unnamed area around them */
    scroll->setFocusPolicy( Qt::NoFocus );

    stopShowing = new QCheckBox( qtr( "Hide future errors" ) );
    /* checked means hidden: the option says whether errors show */
    stopShowing->setChecked( !var_InheritBool( p_intf, "qt-error-dialogs" ) );

    /* the footer: Show log and Clear on the left, Close then the accent
     * Retry on the right */
    QPushButton *logButton = new QPushButton( qtr( "Show &log" ), this );
    logButton->setProperty( "role", "link" );
    logButton->setToolTip( qtr( "Open the messages window" ) );
    QPushButton *clearButton = new QPushButton( qtr( "Cl&ear" ), this );
    clearButton->setProperty( "role", "link" );
    clearButton->setToolTip( qtr( "Clear the errors listed" ) );
    QPushButton *closeButton = new QPushButton( qtr( "&Close" ), this );
    /* Enter keeps closing (QVLCDialog): no button is the default */
    logButton->setAutoDefault( false );
    clearButton->setAutoDefault( false );
    closeButton->setAutoDefault( false );
    retryButton = new QPushButton( qtr( "&Retry" ), this );
    retryButton->setProperty( "role", "primary" );
    retryButton->setAutoDefault( false ); /* Enter keeps closing (QVLCDialog) */
    retryButton->setToolTip( qtr( "Open the media that failed again" ) );
    retryButton->hide();

    QHBoxLayout *foot = new QHBoxLayout;
    foot->addWidget( logButton );
    foot->addWidget( clearButton );
    foot->addStretch( 1 );
    foot->addWidget( closeButton );
    foot->addWidget( retryButton );

    layout->addWidget( icon, 0, 0, 2, 1, Qt::AlignTop );
    layout->addWidget( title, 0, 1 );
    layout->addWidget( scroll, 1, 1 );
    layout->addWidget( stopShowing, 2, 1 );
    layout->addLayout( foot, 3, 0, 1, 2 );
    layout->setColumnStretch( 1, 1 );
    layout->setRowStretch( 1, 1 );

    setTabOrder( body, stopShowing );
    setTabOrder( stopShowing, logButton );
    setTabOrder( logButton, clearButton );
    setTabOrder( clearButton, closeButton );
    setTabOrder( closeButton, retryButton );

    BUTTONACT( closeButton, close );
    BUTTONACT( clearButton, clear );
    BUTTONACT( retryButton, retry );
    connect( logButton, &QPushButton::clicked, THEDP, &DialogsProvider::messagesDialog );
    /* toggled, not clicked: assistive tools toggle without a click */
    connect( stopShowing, &QCheckBox::toggled, this, &ErrorsDialog::dontShow );
}

ErrorsDialog::~ErrorsDialog()
{
    if( failedItem )
        input_item_Release( failedItem );
}

void ErrorsDialog::setFailedItem( input_item_t *item )
{
    if( item )
        input_item_Hold( item );
    if( failedItem )
        input_item_Release( failedItem );
    failedItem = item;
    retryButton->setVisible( item != NULL );
}

/* fromCore: from vlc_dialog_display_error, which any module may call */
void ErrorsDialog::addError( const QString& title, const QString& text, bool fromCore )
{
    /* Retry only for a failed open. The error is queued ahead of the next
     * item's "input-current", so the input here is the one that failed; a
     * snapshot or record error comes from a playing input instead, and
     * leaves the Retry of an earlier failed open alone. */
    input_thread_t *input = fromCore ? THEMIM->getInput() : NULL;
    if( input )
    {
        const int64_t state = var_GetInteger( input, "state" );
        if( state != PLAYING_S && state != PAUSE_S )
            setFailedItem( input_GetItem( input ) );
    }
    add( true, title, text );
}

/*void ErrorsDialog::addWarning( QString title, QString text )
{
    add( false, title, text );
}*/

void ErrorsDialog::add( bool, const QString& title, const QString& text )
{
    this->title->setText( title );
    entries.append( qMakePair( title, text ) );

    /* shortcut: the MRL is found as a quoted word, as the core's "unable
     * to open the MRL '%s'" puts it; mark it up if other messages need it */
    static const QRegularExpression quoted( QStringLiteral( "'(\\S+)'" ) );
    Theme *t = Theme::instance();
    const QString mono = QString( "<span style=\"font-family:'%1'; color:%2\">\\1</span>" )
                             .arg( t->font( Theme::Mono ).family(),
                                   t->color( Theme::Text ).name() );
    QStringList html;
    for( int i = 0; i < entries.count(); i++ )
    {
        QString part = entries[i].second.toHtmlEscaped();
        part.replace( quoted, mono );
        part.replace( QLatin1Char( '\n' ), QLatin1String( "<br>" ) );
        /* the latest error's title is the heading; older ones name theirs */
        if( i < entries.count() - 1 )
            part = QString( "<b>%1</b><br>%2" ).arg( entries[i].first.toHtmlEscaped(), part );
        html.prepend( part );
    }
    body->setText( html.join( QLatin1String( "<br><br>" ) ) );
    if ( var_InheritBool( p_intf, "qt-error-dialogs" ) )
    {
        /* errors show: the box may still say hidden from before a Reset */
        QSignalBlocker blocker( stopShowing );
        stopShowing->setChecked( false );
        show();
    }
}

void ErrorsDialog::close()
{
    hide();
}

void ErrorsDialog::clear()
{
    entries.clear();
    body->clear();
    title->clear();
    setFailedItem( NULL );
}

/* P5-3: open the media that failed again, while it is in the playlist */
void ErrorsDialog::retry()
{
    if( failedItem )
    {
        vlc_playlist_locker pl_lock( THEPL );
        playlist_item_t *item = playlist_ItemGetByInput( THEPL, failedItem );
        if( item )
            playlist_ViewPlay( THEPL, NULL, item );
    }
    hide();
}

void ErrorsDialog::dontShow()
{
    config_PutInt( p_intf, "qt-error-dialogs", !stopShowing->isChecked() );
}
