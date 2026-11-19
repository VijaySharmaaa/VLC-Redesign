/*****************************************************************************
 * deck_popover.cpp : the control deck's tracks and speed popovers
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

#include "components/deck_popover.hpp"
#include "components/interface_widgets.hpp" /* SpeedControlWidget */
#include "components/overlay_host.hpp"      /* OverlayWindow::enterCurve */
#include "dialogs_provider.hpp"
#include "input_manager.hpp"
#include "menus.hpp"
#include "util/redesign_icons.hpp"
#include "util/theme.hpp"
#include "util/customwidgets.hpp"   /* setStyleProperty, rateText */

#include <vlc_input_item.h>

#include <QAbstractButton>
#include <QApplication>
#include <QBoxLayout>
#include <QDesktopWidget>
#include <QHash>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QStyle>
#include <QStyledItemDelegate>

using namespace RedesignTokens;

QPoint popupPos( QWidget *anchor, const QSize &size )
{
    const QRect a( anchor->mapToGlobal( QPoint( 0, 0 ) ), anchor->size() );
    const QRect screen = QApplication::desktop()->availableGeometry( anchor );
    const int gap = Theme::instance()->px( Deck::PopoverGap );
    QPoint pos( a.center().x() - size.width() / 2, a.top() - gap - size.height() );
    if( pos.y() < screen.top() )
        pos.setY( a.bottom() + 1 + gap );
    pos.setX( qBound( screen.left(), pos.x(), screen.right() + 1 - size.width() ) );
    return pos;
}

namespace
{
    enum { DetailRole = Qt::UserRole + 1, CheckedRole };

    /* A row: accent check (or nothing) | label, and a dimmed detail line */
    class RowDelegate : public QStyledItemDelegate
    {
    public:
        explicit RowDelegate( QObject *parent ) : QStyledItemDelegate( parent ) {}

        QSize sizeHint( const QStyleOptionViewItem &opt, const QModelIndex &index ) const Q_DECL_OVERRIDE
        {
            const Theme *t = Theme::instance();
            int h = QFontMetrics( t->font( Theme::Body ) ).height();
            if( !index.data( DetailRole ).toString().isEmpty() )
                h += QFontMetrics( t->font( Theme::Meta ) ).height();
            return QSize( opt.rect.width(), h + 2 * t->px( Deck::PopoverRowPadY ) );
        }

        void paint( QPainter *p, const QStyleOptionViewItem &opt,
                    const QModelIndex &index ) const Q_DECL_OVERRIDE
        {
            Theme *t = Theme::instance();
            const bool enabled = index.flags() & Qt::ItemIsEnabled;
            p->save();
            p->setRenderHint( QPainter::Antialiasing );
            /* hover, and the keyboard's current row */
            if( enabled && ( opt.state & ( QStyle::State_MouseOver | QStyle::State_HasFocus ) ) )
            {
                p->setPen( Qt::NoPen );
                p->setBrush( t->ink( InkAlpha::HoverFill ) );
                p->drawRoundedRect( opt.rect, t->metric( Theme::RadiusMd ), t->metric( Theme::RadiusMd ) );
            }
            const QRect r = opt.rect.adjusted( t->px( Deck::PopoverRowPadX ), t->px( Deck::PopoverRowPadY ),
                                               -t->px( Deck::PopoverRowPadX ), -t->px( Deck::PopoverRowPadY ) );
            const int col = t->px( Deck::PopoverCheckCol );
            if( index.data( CheckedRole ).toBool() )
            {
                const int check = t->px( Deck::PopoverCheck );
                /* the check column is on the leading side (right in RTL) */
                const QRect box = QStyle::visualRect( opt.direction, opt.rect,
                    QRect( r.left() + ( col - check ) / 2, r.center().y() - check / 2 + 1, check, check ) );
                p->drawPixmap( box, t->icon( RedesignIcons::IconCheck, Theme::AccentFocusRing,
                                             box.size() ).pixmap( box.size() ) );
            }
            const QRect text = QStyle::visualRect( opt.direction, opt.rect,
                                                   r.adjusted( col + t->px( Deck::PopoverRowGap ), 0, 0, 0 ) );
            const QString detail = index.data( DetailRole ).toString();
            const QFont body = t->font( Theme::Body ), meta = t->font( Theme::Meta );
            const int bodyH = QFontMetrics( body ).height();
            const int top = text.top() + ( text.height() - bodyH
                            - ( detail.isEmpty() ? 0 : QFontMetrics( meta ).height() ) ) / 2;
            p->setFont( body );
            p->setPen( t->color( enabled ? Theme::Text : Theme::Dim ) );
            p->drawText( QRect( text.left(), top - t->textNudge( p->font() ), text.width(), bodyH ),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics( body ).elidedText( index.data().toString(), Qt::ElideRight, text.width() ) );
            if( !detail.isEmpty() )
            {
                p->setFont( meta );
                p->setPen( t->color( enabled ? Theme::Mute : Theme::Dim ) );
                p->drawText( QRect( text.left(), top + bodyH - t->textNudge( p->font() ), text.width(),
                                    QFontMetrics( meta ).height() ),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QFontMetrics( meta ).elidedText( detail, Qt::ElideRight, text.width() ) );
            }
            p->restore();
        }

        /* the check state is only for screen readers: a click or Space must
         * not flip it, choosing a row (itemClicked/itemActivated) sets it */
        bool editorEvent( QEvent *, QAbstractItemModel *, const QStyleOptionViewItem &,
                          const QModelIndex & ) Q_DECL_OVERRIDE
        {
            return false;
        }
    };

    class Popover : public QFrame
    {
    public:
        explicit Popover( QAbstractButton *_anchor )
            : QFrame( _anchor->window(), Qt::Popup ), anchor( _anchor )
        {
            setAttribute( Qt::WA_DeleteOnClose );
            /* a click on the button that opened it only closes it */
            setAttribute( Qt::WA_NoMouseReplay );
            setProperty( "role", "popover" );
            setOpen( true );
            /* a toolbar rebuild deletes the button: forget it, then close */
            connect( anchor, &QObject::destroyed, this, [this]() {
                anchor = NULL;
                close();
            } );
        }

        /* above the button (below when there is no room), rising into place */
        void popup()
        {
            const Theme *t = Theme::instance();
            adjustSize();
            const QPoint pos = popupPos( anchor, size() );
            int rise = t->px( Motion::RiseOffset );
            if( pos.y() > anchor->mapToGlobal( QPoint( 0, 0 ) ).y() )
                rise = -rise;   /* below the button: it drops into place */
            const int ms = t->duration( Theme::Popover );
            move( ms ? pos + QPoint( 0, rise ) : pos );
            show();
            if( ms )
            {
                QPropertyAnimation *anim = new QPropertyAnimation( this, "pos", this );
                anim->setDuration( ms );
                anim->setStartValue( pos + QPoint( 0, rise ) );
                anim->setEndValue( pos );
                anim->setEasingCurve( OverlayWindow::enterCurve() );
                anim->start( QAbstractAnimation::DeleteWhenStopped );
            }
        }

    protected:
        void hideEvent( QHideEvent *e ) Q_DECL_OVERRIDE
        {
            setOpen( false );
            QFrame::hideEvent( e );
        }

    private:
        void setOpen( bool open )
        {
            if( anchor )
                setStyleProperty( anchor, "open", open );
        }

        QPointer<QAbstractButton> anchor;
    };

    /* the accent check, also as a check state so screen readers say "checked" */
    void setChecked( QListWidgetItem *item, bool checked )
    {
        item->setData( CheckedRole, checked );
        item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
        item->setCheckState( checked ? Qt::Checked : Qt::Unchecked );
    }

    QListWidget *makeList( QWidget *parent, const QString &name )
    {
        QListWidget *list = new QListWidget( parent );
        list->setItemDelegate( new RowDelegate( list ) );
        list->setAccessibleName( name );
        list->setFrameShape( QFrame::NoFrame );
        list->setSelectionMode( QAbstractItemView::NoSelection );
        list->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
        list->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
        list->viewport()->setAttribute( Qt::WA_Hover );
        return list;
    }

    QListWidgetItem *addRow( QListWidget *list, const QString &label,
                             const QString &detail, bool checked )
    {
        QListWidgetItem *item = new QListWidgetItem( label, list );
        item->setData( DetailRole, detail );
        item->setData( Qt::AccessibleDescriptionRole, detail );
        setChecked( item, checked );
        if( checked )
            list->setCurrentItem( item );
        return item;
    }

    /* up to PopoverMaxRows rows high; more rows scroll */
    void fitRows( QListWidget *list )
    {
        int h = 0;
        for( int i = 0; i < qMin( list->count(), static_cast<int>( Deck::PopoverMaxRows ) ); i++ )
            h += list->sizeHintForRow( i );
        list->setFixedHeight( h + 2 * list->frameWidth() );
    }

    /* P3-3: "channels · codec · bitrate" of every elementary stream, by the
     * id the *-es variables use, from the "Stream N" info categories. */
    QHash<int, QString> esDetails( input_thread_t *p_input )
    {
        QHash<int, QString> details;
        input_item_t *p_item = input_GetItem( p_input );
        const QString stream = qtr( "Stream %d" );
        const int at = stream.indexOf( QLatin1String( "%d" ) );
        if( !p_item || at < 0 )
            return details;
        const QString prefix = stream.left( at ), suffix = stream.mid( at + 2 );
        const QString keys[] = { qtr( "Channels" ), qtr( "Codec" ), qtr( "Bitrate" ) };
        const QString originalId = qtr( "Original ID" );

        vlc_mutex_lock( &p_item->lock );
        for( int i = 0; i < p_item->i_categories; i++ )
        {
            const info_category_t *cat = p_item->pp_categories[i];
            const QString name = qfu( cat->psz_name );
            if( !name.startsWith( prefix ) || !name.endsWith( suffix ) )
                continue;
            bool ok;
            int id = name.mid( prefix.length(), name.length() - prefix.length() - suffix.length() ).toInt( &ok );
            if( !ok )
                continue;
            QString values[3];
            for( int j = 0; j < cat->i_infos; j++ )
            {
                const QString key = qfu( cat->pp_infos[j]->psz_name );
                const QString value = qfu( cat->pp_infos[j]->psz_value );
                if( key == originalId )
                    id = value.toInt();  /* the es id, when it differs from N */
                for( int k = 0; k < 3; k++ )
                    if( key == keys[k] )
                        values[k] = value;
            }
            QStringList parts;
            for( int k = 0; k < 3; k++ )
                if( !values[k].isEmpty() )
                    parts << values[k];
            details.insert( id, parts.join( QString::fromUtf8( " \xC2\xB7 " ) ) );
        }
        vlc_mutex_unlock( &p_item->lock );
        return details;
    }

    /* A column of the choices of an *-es variable. The rows mirror the
     * actions VLCMenuBar::CreateChoicesMenu makes for the Audio and
     * Subtitle menus and trigger them: a choice applies at once. */
    void addChoices( QWidget *pop, QBoxLayout *column, const QString &title,
                     const char *var, input_thread_t *p_input,
                     const QHash<int, QString> &details )
    {
        const Theme *t = Theme::instance();
        QLabel *header = new QLabel( title, pop );
        header->setFont( t->font( Theme::Overline ) );
        header->setContentsMargins( t->px( Deck::PopoverRowPadX ), 0, t->px( Deck::PopoverRowPadX ),
                                    t->px( Deck::RowGap ) );
        column->addWidget( header );

        QMenu *menu = new QMenu( pop );  /* never shown; owns the actions */
        VLCMenuBar::CreateChoicesMenu( menu, var, VLC_OBJECT( p_input ) );
        QListWidget *list = makeList( pop, title );
        foreach( QAction *action, menu->actions() )
        {
            const MenuItemData *data = action->findChild<MenuItemData *>();
            addRow( list, action->text().replace( QLatin1String( "&&" ), QLatin1String( "&" ) ),
                    data ? details.value( data->val.i_int ) : QString(), action->isChecked() );
        }
        if( list->count() == 0 )
            addRow( list, qtr( "No tracks" ), QString(), false )->setFlags( Qt::NoItemFlags );
        fitRows( list );
        column->addWidget( list );

        auto choose = [list, menu]( QListWidgetItem *item ) {
            const int row = list->row( item );
            QAction *action = menu->actions().value( row );
            if( !action )
                return;
            action->trigger();
            /* not the actions' isChecked(): CreateAndConnect's disconnect()
             * cuts them from their exclusive QActionGroup, so the previous
             * choice stays checked; setting the variable always selects */
            for( int i = 0; i < list->count(); i++ )
                setChecked( list->item( i ), i == row );
        };
        QObject::connect( list, &QListWidget::itemClicked, list, choose );
        QObject::connect( list, &QListWidget::itemActivated, list, choose );
    }

    QBoxLayout *addColumn( QBoxLayout *columns )
    {
        const Theme *t = Theme::instance();
        QVBoxLayout *column = new QVBoxLayout;
        column->setSpacing( 0 );
        column->setContentsMargins( t->px( Deck::PopoverPadX ), t->px( Deck::PopoverPadTop ),
                                    t->px( Deck::PopoverPadX ), t->px( Deck::PopoverPadBottom ) );
        columns->addLayout( column, 1 );
        return column;
    }
}

QWidget *showTracksPopover( intf_thread_t *p_intf, QAbstractButton *anchor )
{
    input_thread_t *p_input = THEMIM->getInput();
    if( !p_input )
        return NULL;
    const Theme *t = Theme::instance();
    Popover *pop = new Popover( anchor );
    pop->setAccessibleName( qtr( "Audio and subtitle tracks" ) );
    pop->setFixedWidth( t->px( Deck::TracksPopoverW ) );
    const QHash<int, QString> details = esDetails( p_input );

    QHBoxLayout *columns = new QHBoxLayout( pop );
    columns->setContentsMargins( 0, 0, 0, 0 );
    columns->setSpacing( 0 );

    QBoxLayout *audio = addColumn( columns );
    addChoices( pop, audio, qtr( "Audio track" ), "audio-es", p_input, details );
    audio->addStretch( 1 );

    QFrame *divider = new QFrame( pop );
    divider->setProperty( "role", "popoverDivider" );
    divider->setFixedWidth( Size::Hairline );
    columns->addWidget( divider );

    QBoxLayout *subtitles = addColumn( columns );
    addChoices( pop, subtitles, qtr( "Subtitles" ), "spu-es", p_input, details );
    subtitles->addSpacing( t->px( Deck::RowGap ) );
    QPushButton *addSubtitle = new QPushButton(
        Theme::themedIcon( RedesignIcons::IconAdd, Theme::AccentText ),
        qtr( "Add subtitle file…" ), pop );
    addSubtitle->setProperty( "role", "popoverLink" );
    addSubtitle->setIconSize( QSize( t->px( Deck::PopoverAddIcon ), t->px( Deck::PopoverAddIcon ) ) );
    QObject::connect( addSubtitle, &QPushButton::clicked, pop, [pop]() {
        pop->close();
        THEDP->loadSubtitlesFile();
    } );
    subtitles->addWidget( addSubtitle );
    subtitles->addStretch( 1 );

    pop->popup();
    pop->findChild<QListWidget *>()->setFocus();
    return pop;
}

QWidget *showSpeedPopover( intf_thread_t *p_intf, QAbstractButton *anchor )
{
    /* SCREEN_INVENTORY 1.4.1 */
    static const float rates[] = { .25f, .5f, .75f, 1.f, 1.25f, 1.5f, 1.75f, 2.f };
    const Theme *t = Theme::instance();
    Popover *pop = new Popover( anchor );
    pop->setAccessibleName( qtr( "Playback speed" ) );
    pop->setFixedWidth( t->px( Deck::SpeedPopoverW ) );

    QVBoxLayout *layout = new QVBoxLayout( pop );
    const int pad = t->px( Size::MenuPanelPadding );
    layout->setContentsMargins( pad, pad, pad, pad );
    QListWidget *list = makeList( pop, qtr( "Playback speed" ) );
    const float rate = currentRate( p_intf );
    for( size_t i = 0; i < sizeof( rates ) / sizeof( rates[0] ); i++ )
        addRow( list, rateText( rates[i] ), QString(),
                qAbs( rates[i] - rate ) < .001f );
    fitRows( list );
    layout->addWidget( list );

    /* any other rate: VLC's fine control; no preset is checked then */
    QFrame *divider = new QFrame( pop );
    divider->setProperty( "role", "popoverDivider" );
    divider->setFixedHeight( Size::Hairline );
    layout->addWidget( divider );
    layout->addWidget( new SpeedControlWidget( p_intf, pop ) );
    QObject::connect( THEMIM->getIM(), &InputManager::rateChanged, list, [list]( float r ) {
        for( int i = 0; i < list->count(); i++ )
            setChecked( list->item( i ), qAbs( rates[i] - r ) < .001f );
    } );

    auto choose = [p_intf, list, pop]( QListWidgetItem *item ) {
        var_SetFloat( THEPL, "rate", rates[list->row( item )] );
        pop->close();
    };
    QObject::connect( list, &QListWidget::itemClicked, pop, choose );
    QObject::connect( list, &QListWidget::itemActivated, pop, choose );

    pop->popup();
    list->setFocus();
    return pop;
}
