/*****************************************************************************
 * searchlineedit.cpp: Custom widgets
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "searchlineedit.hpp"
#include "customwidgets.hpp"   /* withHotkey */
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <QAction>
#include <QKeyEvent>

#include <vlc_intf_strings.h>

namespace {
/* Esc empties the field when it has text, else goes on (a page's back,
 * a panel's close) */
class EscClears : public QObject
{
public:
    explicit EscClears( QLineEdit *edit ) : QObject( edit ) { edit->installEventFilter( this ); }

protected:
    bool eventFilter( QObject *o, QEvent *e ) Q_DECL_OVERRIDE
    {
        QLineEdit *edit = static_cast<QLineEdit *>( o );
        if( ( e->type() != QEvent::KeyPress && e->type() != QEvent::ShortcutOverride )
         || static_cast<QKeyEvent *>( e )->key() != Qt::Key_Escape
         || static_cast<QKeyEvent *>( e )->modifiers() != Qt::NoModifier || edit->text().isEmpty() )
            return false;
        /* before an Esc shortcut of the window can take it */
        if( e->type() == QEvent::KeyPress )
            edit->clear();
        e->accept();
        return true;
    }
};
}

void setupSearchField( QLineEdit *edit, const QString &searchName, const QString &clearName )
{
    using RedesignTokens::Library::FieldGlyph;
    edit->setClearButtonEnabled( false );   /* Qt's own X is not themed */
    edit->addAction( Theme::themedIcon( RedesignIcons::IconSearch, Theme::Dim, FieldGlyph ),
                     QLineEdit::LeadingPosition )->setText( searchName );
    QAction *clear = new QAction( Theme::themedIcon( RedesignIcons::IconClose, Theme::Text2, FieldGlyph ),
                                  clearName, edit );
    clear->setToolTip( withHotkey( NULL, clearName, "Esc" ) );
    QObject::connect( clear, &QAction::triggered, edit, &QLineEdit::clear );
    /* shown only with text: added and taken away, as Qt 5.6 keeps the
     * button of a hidden action */
    QObject::connect( edit, &QLineEdit::textChanged, clear, [edit, clear]( const QString &text ) {
        if( text.isEmpty() )
            edit->removeAction( clear );
        else if( !edit->actions().contains( clear ) )
            edit->addAction( clear, QLineEdit::TrailingPosition );
    } );
    new EscClears( edit );
}

SearchLineEdit::SearchLineEdit( QWidget *parent ) : QLineEdit( parent )
{
    setPlaceholderText( qtr( I_PL_SEARCH ) );
    setAccessibleName( qtr( I_PL_SEARCH ) );
    setupSearchField( this, qtr( I_PL_SEARCH ), qtr( "Clear search" ) );
    setMinimumHeight( Theme::instance()->metric( Theme::SelectH ) );

    connect( this, &QLineEdit::editingFinished, this, [this]() {
        emit searchDelayedChanged( text() );
    } );
}
