/*****************************************************************************
 * gototime.cpp : GotoTime and About dialogs
 ****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/gototime.hpp"

#include "input_manager.hpp"
#include "util/theme.hpp"
#include "util/customwidgets.hpp"     /* timeString */

#include <QLabel>
#include <QTimeEdit>
#include <QPushButton>
#include <QBoxLayout>
#include <QFocusEvent>
#include <QLineEdit>

namespace
{

/* opens with a caret, not the hours selected in accent; Tab still
 * selects a section, to type over */
class TimeEdit : public QTimeEdit
{
protected:
    void focusInEvent( QFocusEvent *e ) Q_DECL_OVERRIDE
    {
        QTimeEdit::focusInEvent( e );
        if( e->reason() != Qt::TabFocusReason && e->reason() != Qt::BacktabFocusReason )
            lineEdit()->deselect();
    }
};

}

GotoTimeDialog::GotoTimeDialog( intf_thread_t *_p_intf)
               : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )
{
    setWindowFlags( Qt::Tool );
    setWindowTitle( qtr( "Go to Time" ) );
    setWindowRole( "vlc-goto-time" );

    /* DESIGN_SPEC 7 (2.1.6): the title, an h:mm:ss field, a hint naming
     * the media length, then Reset, Cancel and the accent Go */
    QVBoxLayout *mainLayout = new QVBoxLayout( this );
    mainLayout->setSizeConstraint( QLayout::SetFixedSize );

    Theme *t = Theme::instance();
    QLabel *title = new QLabel( qtr( "Go to time" ) );
    title->setFont( t->font( Theme::PanelTitle ) );

    timeEdit = new TimeEdit();
    timeEdit->setDisplayFormat( "H:mm:ss" );
    timeEdit->setButtonSymbols( QAbstractSpinBox::NoButtons );
    timeEdit->setAccessibleName( qtr( "Go to time" ) );
    /* a large mono timecode, scaled from Theme's mono font so it follows
     * the Windows text scale as Theme's fonts do */
    QFont mono = t->font( Theme::Mono );
    mono.setPointSizeF( mono.pointSizeF() * RedesignTokens::Dialogs::JumpFont
                                          / RedesignTokens::Type::Mono );
    timeEdit->setFont( mono );
    timeEdit->setMinimumHeight( t->px( RedesignTokens::Dialogs::JumpInputH ) );
    title->setBuddy( timeEdit );

    hint = new QLabel;
    hint->setFont( t->font( Theme::Meta ) );
    hint->setProperty( "role", "hint" );

    QPushButton *gotoButton = new QPushButton( qtr( "&Go" ) );
    gotoButton->setProperty( "role", "primary" );
    gotoButton->setDefault( true );
    QPushButton *cancelButton = new QPushButton( qtr( "&Cancel" ) );
    cancelButton->setAutoDefault( false ); /* Go stays the default button */
    /* stock's Reset (to 0:00:00), a secondary text button at the start */
    QPushButton *resetButton = new QPushButton( qtr( "Reset" ) );
    resetButton->setAutoDefault( false );
    QHBoxLayout *foot = new QHBoxLayout;
    foot->addWidget( resetButton );
    foot->addStretch( 1 );
    foot->addWidget( cancelButton );
    foot->addWidget( gotoButton );

    mainLayout->addWidget( title );
    mainLayout->addWidget( timeEdit );
    mainLayout->addWidget( hint );
    mainLayout->addLayout( foot );

    BUTTONACT( gotoButton, close );
    BUTTONACT( cancelButton, cancel );
    BUTTONACT( resetButton, reset );

    QVLCTools::restoreWidgetPosition( p_intf, "gototimedialog", this );
}

GotoTimeDialog::~GotoTimeDialog()
{
    QVLCTools::saveWidgetPosition( p_intf, "gototimedialog", this );
}

void GotoTimeDialog::toggleVisible()
{
    reset();
    int64_t i_length = 0;
    if ( !isVisible() && THEMIM->getIM()->hasInput() )
    {
        int64_t i_time = var_GetInteger( THEMIM->getInput(), "time" );
        timeEdit->setTime( timeEdit->time().addSecs( i_time / CLOCK_FREQ ) );
        i_length = var_GetInteger( THEMIM->getInput(), "length" ) / CLOCK_FREQ;
    }
    hint->setText( i_length > 0
        ? qtr( "Format h:mm:ss — media length %1" ).arg( timeString( i_length ) )
        : qtr( "Format h:mm:ss" ) );
    QVLCDialog::toggleVisible();
    if(isVisible())
        activateWindow();
}

void GotoTimeDialog::cancel()
{
    reset();
    toggleVisible();
}

void GotoTimeDialog::close()
{
    if ( THEMIM->getIM()->hasInput() )
    {
        int64_t i_time = (int64_t)
            ( QTime( 0, 0, 0 ).msecsTo( timeEdit->time() ) ) * 1000;
        var_SetInteger( THEMIM->getInput(), "time", i_time );
    }
    toggleVisible();
}

void GotoTimeDialog::reset()
{
    timeEdit->setTime( QTime( 0, 0, 0) );
}
