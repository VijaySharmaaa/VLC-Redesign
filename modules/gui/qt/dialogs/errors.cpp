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
#include "util/redesign_icons.hpp"
#include "util/theme.hpp"

#include <QTextCursor>
#include <QTextEdit>
#include <QCheckBox>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>

using namespace RedesignTokens;

ErrorsDialog::ErrorsDialog( intf_thread_t *_p_intf )
             : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )
{
    setWindowTitle( qtr( "Errors" ) );
    setWindowRole( "vlc-errors" );
    resize( 500 , 300 );

    Theme *t = Theme::instance();
    QGridLayout *layout = new QGridLayout( this );

    /* DESIGN_SPEC 7: a round error icon, the latest title, the detail in mono */
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

    QDialogButtonBox *buttonBox = new QDialogButtonBox( Qt::Horizontal, this );
    QPushButton *clearButton = new QPushButton( qtr( "Cl&ear" ), this );
    buttonBox->addButton( clearButton, QDialogButtonBox::ActionRole );
    buttonBox->addButton( new QPushButton( qtr("&Close"), this ), QDialogButtonBox::RejectRole );
    retryButton = new QPushButton( qtr( "&Retry" ), this );
    retryButton->setProperty( "role", "primary" );
    retryButton->setAutoDefault( false ); /* Enter keeps closing (QVLCDialog) */
    retryButton->setToolTip( qtr( "Play the current playlist item again" ) );
    buttonBox->addButton( retryButton, QDialogButtonBox::AcceptRole );

    QPushButton *logButton = new QPushButton( qtr( "Show &log" ), this );
    logButton->setProperty( "role", "link" );
    logButton->setToolTip( qtr( "Open the messages window" ) );

    messages = new QTextEdit();
    messages->setReadOnly( true );
    messages->setFont( t->font( Theme::Mono ) );
    messages->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    messages->setAccessibleName( qtr( "Error details" ) );
    stopShowing = new QCheckBox( qtr( "Hide future errors" ) );
    /* checked means hidden: the option says whether errors show */
    stopShowing->setChecked( !var_InheritBool( p_intf, "qt-error-dialogs" ) );

    layout->addWidget( icon, 0, 0, Qt::AlignTop );
    layout->addWidget( title, 0, 1, 1, 3 );
    layout->addWidget( messages, 1, 1, 1, 3 );
    layout->addWidget( stopShowing, 2, 1, 1, 3 );
    layout->addWidget( logButton, 3, 0, 1, 2, Qt::AlignLeft );
    layout->addWidget( buttonBox, 3, 2, 1, 2 );
    layout->setColumnStretch( 2, 1 );

    setTabOrder( messages, stopShowing );
    setTabOrder( stopShowing, logButton );
    setTabOrder( logButton, buttonBox );

    connect( buttonBox, &QDialogButtonBox::rejected, this, &ErrorsDialog::close );
    BUTTONACT( clearButton, clear );
    BUTTONACT( retryButton, retry );
    connect( logButton, &QPushButton::clicked, THEDP, &DialogsProvider::messagesDialog );
    /* toggled, not clicked: assistive tools toggle without a click */
    connect( stopShowing, &QCheckBox::toggled, this, &ErrorsDialog::dontShow );
}

void ErrorsDialog::addError( const QString& title, const QString& text, bool retry )
{
    /* Retry replays the playlist item: only for errors of the core (input) */
    retryButton->setVisible( retry );
    add( true, title, text );
}

/*void ErrorsDialog::addWarning( QString title, QString text )
{
    add( false, title, text );
}*/

void ErrorsDialog::add( bool error, const QString& title, const QString& text )
{
    this->title->setText( title );
    messages->textCursor().movePosition( QTextCursor::End );
    messages->setTextColor( Theme::instance()->color( error ? Theme::Error : Theme::Accent ) );
    messages->insertPlainText( title + QString( ":\n" ) );
    messages->setTextColor( messages->palette().color( QPalette::Text ) );
    messages->insertPlainText( text + QString( "\n" ) );
    messages->ensureCursorVisible();
    if ( var_InheritBool( p_intf, "qt-error-dialogs" ) )
        show();
}

void ErrorsDialog::close()
{
    hide();
}

void ErrorsDialog::clear()
{
    messages->clear();
    title->clear();
}

/* P5-3: play the current playlist item again */
void ErrorsDialog::retry()
{
    {
        vlc_playlist_locker pl_lock( THEPL );
        playlist_item_t *item = playlist_CurrentPlayingItem( THEPL );
        if( item )
            playlist_ViewPlay( THEPL, NULL, item );
    }
    hide();
}

void ErrorsDialog::dontShow()
{
    config_PutInt( p_intf, "qt-error-dialogs", !stopShowing->isChecked() );
}
