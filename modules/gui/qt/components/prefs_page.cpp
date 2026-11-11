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
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <vlc_config_cat.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>

using namespace RedesignTokens;

const PrefsPage::Section PrefsPage::sections[] = {
    { N_( "Interface" ), RedesignIcons::IconPrefInterface },
    { N_( "Audio" ),     RedesignIcons::IconPrefAudio },
    { N_( "Video" ),     RedesignIcons::IconPrefVideo },
    { SUBPIC_TITLE,      RedesignIcons::IconPrefSubtitles },
    { INPUT_TITLE,       RedesignIcons::IconPrefInput },
    { N_( "Hotkeys" ),   RedesignIcons::IconPrefHotkeys },
    { N_( "Advanced" ),  RedesignIcons::IconPrefAdvanced },
};

/* A row of a card: label and description on the left, the controls on the
 * right; the rows after the first draw a hairline above them */
static QFrame *cardRow( QLabel *name, const QString &desc,
                        const QList<QWidget *> &controls, bool first )
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
    QFont font = t->font( Theme::Body );
    font.setWeight( QFont::Medium );
    name->setFont( font );
    name->setWordWrap( true );
    text->addWidget( name );
    if( !desc.isEmpty() )
    {
        QLabel *hint = new QLabel( desc );
        hint->setProperty( "role", "hint" );
        hint->setFont( t->font( Theme::Meta ) );
        hint->setWordWrap( true );
        text->addWidget( hint );
    }
    layout->addLayout( text, 1 );

    QHBoxLayout *right = new QHBoxLayout;
    right->setSpacing( t->px( Prefs::ButtonGap ) );
    foreach( QWidget *control, controls )
        right->addWidget( control );
    layout->addLayout( right );
    return row;
}

static QFrame *cardRow( const QString &label, const QString &desc,
                        QWidget *control, bool first )
{
    control->setAccessibleName( label );
    control->setAccessibleDescription( desc );
    return cardRow( new QLabel( label ), desc, QList<QWidget *>() << control, first );
}

/* Keeps a row in step with the widget that leads it: hidden with it (the
 * panels hide what the chosen module lacks) and, for a switch, its separate
 * name label greyed with it */
class RowFollower : public QObject
{
public:
    RowFollower( QWidget *lead, QWidget *_row, QLabel *_name )
        : QObject( lead ), row( _row ), name( _name )
    {
        lead->installEventFilter( this );
    }

protected:
    bool eventFilter( QObject *o, QEvent *e ) Q_DECL_OVERRIDE
    {
        if( e->type() == QEvent::HideToParent )
            row->hide();
        else if( e->type() == QEvent::ShowToParent )
            row->show();
        else if( e->type() == QEvent::EnabledChange && name )
            name->setEnabled( !static_cast<QWidget *>( o )->testAttribute( Qt::WA_ForceDisabled ) );
        return false;
    }

private:
    QWidget *row;
    QLabel *name;
};

/* A list's rows at the redesign's list row height; the hotkey columns of
 * a list that names them ("keycapsFrom") show each key as a keycap */
class ListRowDelegate : public QStyledItemDelegate
{
public:
    explicit ListRowDelegate( QObject *parent ) : QStyledItemDelegate( parent ) {}

    void paint( QPainter *p, const QStyleOptionViewItem &option,
                const QModelIndex &index ) const Q_DECL_OVERRIDE
    {
        const QVariant from = option.widget ? option.widget->property( "keycapsFrom" ) : QVariant();
        const QString key = index.data().toString();
        if( !from.isValid() || index.column() < from.toInt() || key.isEmpty() )
        {
            QStyledItemDelegate::paint( p, option, index );
            return;
        }
        /* the row's background and selection, without the text */
        QStyleOptionViewItem opt = option;
        initStyleOption( &opt, index );
        opt.text.clear();
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl( QStyle::CE_ItemViewItem, &opt, p, opt.widget );

        const Theme *t = Theme::instance();
        const QFont font = t->font( Theme::Mono );
        const QFontMetrics fm( font );
        const int padX = t->px( Prefs::KeycapPadX ), padY = t->px( Prefs::KeycapPadY );
        const int bottom = t->px( Prefs::KeycapBottom ), radius = t->px( Prefs::KeycapRadius );
        const int margin = style->pixelMetric( QStyle::PM_FocusFrameHMargin, 0, opt.widget ) + 1;
        const QRect cell = opt.rect.adjusted( margin, 0, -margin, 0 );
        const QString text = fm.elidedText( key, Qt::ElideRight, qMax( 0, cell.width() - 2 * padX ) );
        /* the face's padding inside a 1 px outline, 2 px at the bottom */
        QRect cap( 0, 0, qMin( cell.width(), fm.width( text ) + 2 * padX + 2 ),
                   fm.height() + 2 * padY + 1 + bottom );
        cap.moveTopLeft( QPoint( opt.direction == Qt::RightToLeft ? cell.right() + 1 - cap.width() : cell.left(),
                                 cell.top() + ( cell.height() - cap.height() ) / 2 ) );

        p->save();
        p->setRenderHint( QPainter::Antialiasing );
        p->setPen( Qt::NoPen );
        /* the outline, its bottom thicker, under the face */
        p->setBrush( t->ink( InkAlpha::Keycap ) );
        p->drawRoundedRect( cap, radius, radius );
        const QRect face = cap.adjusted( 1, 1, -1, -bottom );
        p->setBrush( t->color( Theme::Raised ) );
        p->drawRoundedRect( face, radius - 1, radius - 1 );
        p->setFont( font );
        p->setPen( t->color( Theme::Text ) );
        p->drawText( face.translated( 0, -t->textNudge( font ) ), Qt::AlignCenter, text );
        p->restore();
    }

    QSize sizeHint( const QStyleOptionViewItem &option,
                    const QModelIndex &index ) const Q_DECL_OVERRIDE
    {
        QSize s = QStyledItemDelegate::sizeHint( option, index );
        s.setHeight( qMax( s.height(), Theme::instance()->metric( Theme::PlaylistRowH ) ) );
        return s;
    }
};

/* The widgets of a layout, nested layouts included, in reading order */
static void flatten( QLayout *l, QList<QWidget *> &out )
{
    QGridLayout *grid = qobject_cast<QGridLayout *>( l );
    QList<QPair<int, QLayoutItem *> > items;
    for( int i = 0; i < l->count(); i++ )
    {
        int row = 0, col = 0, rs, cs;
        if( grid )
            grid->getItemPosition( i, &row, &col, &rs, &cs );
        items.append( qMakePair( grid ? row * grid->columnCount() + col : i, l->itemAt( i ) ) );
    }
    std::stable_sort( items.begin(), items.end(),
                      []( const QPair<int, QLayoutItem *> &a, const QPair<int, QLayoutItem *> &b )
                      { return a.first < b.first; } );
    for( int i = 0; i < items.size(); i++ )
        if( items[i].second->widget() )
            out.append( items[i].second->widget() );
        else if( items[i].second->layout() )
            flatten( items[i].second->layout(), out );
}

/* a widget and what in it takes Tab, in creation order */
static void addFocus( QWidget *w, QList<QWidget *> &order )
{
    order.append( w );
    order.append( w->findChildren<QWidget *>() );
}

/* A form zone (a plain QWidget with a layout, or a page stack) holds
 * options of its own; one holding only a slider and its value is a control */
static bool isZone( QWidget *w )
{
    return ( w->metaObject() == &QWidget::staticMetaObject || qobject_cast<QStackedWidget *>( w ) )
        && ( !w->findChildren<QLabel *>().isEmpty() || !w->findChildren<QAbstractButton *>().isEmpty() );
}

namespace
{
    /* An option: its leading widget (name label, check box, or the first
     * control where the form has no label), its other controls and the
     * wrapped labels that explain it. Or a group, zone or list kept whole. */
    struct Row
    {
        QWidget *lead;
        QList<QWidget *> controls;
        QList<QLabel *> notes;
        QWidget *block;
    };
}

static QFrame *buildRow( const Row &r, const QString &orphan, bool first, QList<QWidget *> &order )
{
    QLabel *name = qobject_cast<QLabel *>( r.lead );
    QCheckBox *toggle = qobject_cast<QCheckBox *>( r.lead );
    const bool hidden = r.lead->isHidden() && r.lead->testAttribute( Qt::WA_WState_ExplicitShowHide );
    QList<QWidget *> controls = r.controls;
    if( toggle )
    {
        /* the check box's text becomes the row's name, its mnemonic too */
        name = new QLabel( toggle->text() );
        name->setBuddy( toggle );
        name->setEnabled( !toggle->testAttribute( Qt::WA_ForceDisabled ) );
        if( toggle->accessibleName().isEmpty() )
            toggle->setAccessibleName( toggle->text().remove( QLatin1Char( '&' ) ) );
        toggle->setText( QString() );
        toggle->setProperty( "role", "toggle" );
        /* as tall as the row, as its state label is: both on the row's
         * exact centre (drawToggle draws on the widget's), where a fixed
         * height was placed on a centre rounded to whole pixels */
        toggle->setSizePolicy( toggle->sizePolicy().horizontalPolicy(), QSizePolicy::Preferred );
        QLabel *state = new QLabel;
        state->setProperty( "role", "hint" );
        state->setFont( Theme::instance()->font( Theme::Meta ) );
        const auto showState = [state]( bool on ) { state->setText( on ? qtr( "On" ) : qtr( "Off" ) ); };
        showState( toggle->isChecked() );
        QObject::connect( toggle, &QAbstractButton::toggled, state, showState );
        controls << state << toggle;
    }
    else if( !name )
    {
        name = new QLabel( orphan );
        controls.prepend( r.lead );
    }

    /* the description: the option's own long text */
    QString desc;
    QList<QWidget *> tagged = controls;
    tagged.prepend( r.lead );
    foreach( QWidget *w, tagged )
    {
        const QByteArray option = w->property( "vlcOption" ).toByteArray();
        module_config_t *p_config = option.isEmpty() ? NULL : config_FindConfig( option.constData() );
        if( p_config && p_config->psz_longtext )
        {
            /* its first line: the rest stays in the tooltip; none when that
             * line only leads in to the rest ("You have two choices:") */
            desc = qtr( p_config->psz_longtext ).section( QLatin1Char( '\n' ), 0, 0 );
            if( desc.trimmed().endsWith( QLatin1Char( ':' ) ) )
                desc.clear();
            break;
        }
    }
    const QString plainName = name->text().remove( QLatin1Char( '&' ) );
    int fields = 0;
    foreach( QWidget *w, controls )
        fields += qobject_cast<QComboBox *>( w ) || qobject_cast<QLineEdit *>( w );
    foreach( QWidget *w, controls )
    {
        QPushButton *button = qobject_cast<QPushButton *>( w );
        if( button && button->property( "role" ).isNull() )
            button->setProperty( "role", "outline" );
        /* the form's grid gave them its column; a row gives them its end,
         * at the width PrefsPage::sizeFields() gives */
        if( qobject_cast<QComboBox *>( w ) || qobject_cast<QLineEdit *>( w ) )
        {
            if( fields == 1 )
                w->setProperty( "prefsField", true );
            if( w->sizePolicy().horizontalPolicy() == QSizePolicy::Ignored )
                w->setSizePolicy( QSizePolicy::Preferred, w->sizePolicy().verticalPolicy() );
        }
        if( w->focusPolicy() & Qt::TabFocus )
        {
            /* buttons with their own text (radio choices) keep it; a value
             * beside a switch (the days between update checks) has its
             * option's own name, not the switch's */
            QAbstractButton *b = qobject_cast<QAbstractButton *>( w );
            const QByteArray option = w->property( "vlcOption" ).toByteArray();
            module_config_t *p_config = toggle && w != toggle && !option.isEmpty()
                                      ? config_FindConfig( option.constData() ) : NULL;
            if( w->accessibleName().isEmpty() && p_config && p_config->psz_text )
                w->setAccessibleName( qtr( p_config->psz_text ) );
            else if( w->accessibleName().isEmpty() && !( b && !b->text().isEmpty() ) )
                w->setAccessibleName( plainName );
            if( w->accessibleDescription().isEmpty() )
                w->setAccessibleDescription( desc );
        }
        addFocus( w, order );
    }

    QFrame *row = cardRow( name, desc, controls, first );
    QVBoxLayout *text = static_cast<QVBoxLayout *>( row->layout()->itemAt( 0 )->layout() );
    foreach( QLabel *note, r.notes )
    {
        note->setProperty( "role", "hint" );
        note->setWordWrap( true );  /* a one-line link would set the page's width */
        note->setFont( Theme::instance()->font( Theme::Meta ) );
        text->addWidget( note );
    }
    new RowFollower( r.lead, row, toggle ? name : NULL );
    if( hidden )
        row->hide();
    return row;
}

/* DESIGN_SPEC 8: lays a Simple Preferences form out again as cards of rows,
 * moving its widgets, and so their config bindings, into the rows. Group
 * boxes become cards; rows outside them are gathered in cards of their own.
 * `placed`: something is above in the same card (the first row draws a
 * hairline then). The rows' Tab order is appended to `order`. */
static void toRows( QWidget *box, const QString &orphan, bool inCard, bool placed,
                    QList<QWidget *> &order )
{
    QLayout *old = box->layout();
    if( !old )
        return;
    QList<QWidget *> widgets;
    flatten( old, widgets );
    delete old;

    QList<Row> rows;
    foreach( QWidget *w, widgets )
    {
        QFrame *frame = qobject_cast<QFrame *>( w );
        QLabel *label = qobject_cast<QLabel *>( w );
        if( ( frame && !label && ( frame->frameShape() == QFrame::HLine
                                   || frame->frameShape() == QFrame::VLine ) )
         || w->inherits( "InterfacePreviewWidget" ) )
        {
            w->hide();  /* separators, and the previews: decorative */
            continue;
        }
        const bool afterRow = !rows.isEmpty() && !rows.last().block;
        if( label && afterRow && ( label->wordWrap() || label->textFormat() == Qt::RichText ) )
        {
            rows.last().notes.append( label );
            continue;
        }
        const bool block = qobject_cast<QGroupBox *>( w ) || qobject_cast<QAbstractItemView *>( w )
                        || isZone( w );
        if( !block && !label && !qobject_cast<QCheckBox *>( w ) && afterRow )
        {
            rows.last().controls.append( w );
            continue;
        }
        Row r = { w, QList<QWidget *>(), QList<QLabel *>(), block ? w : NULL };
        rows.append( r );
    }

    const Theme *t = Theme::instance();
    QVBoxLayout *layout = new QVBoxLayout( box );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( inCard ? 0 : t->px( Prefs::HeaderBelow ) );
    QVBoxLayout *card = NULL;
    bool filled = false;
    foreach( const Row &r, rows )
    {
        if( r.block )
        {
            card = NULL;
            QAbstractItemView *list = qobject_cast<QAbstractItemView *>( r.block );
            if( list && !inCard )
            {
                /* a list (Hotkeys) takes the page's free height, enough for
                 * a screenful of rows at least (its header about one more) */
                list->setSizePolicy( list->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding );
                list->setMinimumHeight( ( Prefs::HotkeysMinRows + 1 ) * t->metric( Theme::PlaylistRowH ) );
                list->setItemDelegate( new ListRowDelegate( list ) );
                filled = true;
            }
            layout->addWidget( r.block, list && !inCard );
            if( QGroupBox *group = qobject_cast<QGroupBox *>( r.block ) )
                toRows( group, group->title(), true, false, order );
            else if( QStackedWidget *pages = qobject_cast<QStackedWidget *>( r.block ) )
                for( int p = 0; p < pages->count(); p++ )
                    toRows( pages->widget( p ), orphan, inCard, inCard && placed, order );
            else if( isZone( r.block ) )
                toRows( r.block, orphan, inCard, inCard && placed, order );
            else
                addFocus( r.block, order );
        }
        else if( inCard )
            layout->addWidget( buildRow( r, orphan, !placed, order ) );
        else
        {
            if( !card )
            {
                QFrame *frame = new QFrame;
                frame->setProperty( "role", "prefsCard" );
                card = new QVBoxLayout( frame );
                card->setContentsMargins( 0, 0, 0, 0 );
                card->setSpacing( 0 );
                layout->addWidget( frame );
            }
            card->addWidget( buildRow( r, orphan, card->count() == 0, order ) );
        }
        placed = true;
    }
    if( !inCard && !filled )
        layout->addStretch( 1 );
}

PrefsPage::PrefsPage( intf_thread_t *_p_intf, QWidget *_tabAnchor )
    : QWidget( NULL ), p_intf( _p_intf ), tabAnchor( _tabAnchor ), section( SPrefsDefaultCat ),
      narrow( false )
{
    static_assert( sizeof( sections ) / sizeof( sections[0] ) == SectionCount,
                   "one entry per section" );
    const Theme *t = Theme::instance();
    setAccessibleName( qtr( "Preferences" ) );
    for( int i = 0; i < SPrefsMax; i++ )
        panels[i] = NULL;

    /* the section's name, the section, then Cancel and Save */
    QVBoxLayout *right = new QVBoxLayout( this );
    right->setContentsMargins( t->px( Prefs::ContentPadX ), t->px( Prefs::ContentPadY ),
                               t->px( Prefs::ContentPadX ), t->px( Prefs::ContentPadY ) );
    right->setSpacing( t->px( Prefs::HeaderBelow ) );
    title = new QLabel;
    QFont titleFont = t->font( Theme::PageTitle );
    titleFont.setLetterSpacing( QFont::PercentageSpacing, Type::TitleTracking );
    title->setFont( titleFont );
    right->addWidget( title );
    stack = new QStackedWidget;
    right->addWidget( stack, 1 );

    /* Advanced: the complete tree and Reset ("Log to file" has no option, P5-4) */
    advanced = new QWidget;
    QVBoxLayout *advLayout = new QVBoxLayout( advanced );
    advLayout->setContentsMargins( 0, 0, 0, 0 );
    advLayout->setSpacing( t->px( Prefs::HeaderBelow ) );
    QFrame *card = new QFrame;
    card->setProperty( "role", "prefsCard" );
    QVBoxLayout *cardLayout = new QVBoxLayout( card );
    cardLayout->setContentsMargins( 0, 0, 0, 0 );
    cardLayout->setSpacing( 0 );
    showAll = new QPushButton( qtr( "Open..." ) );
    showAll->setProperty( "role", "outline" );
    connect( showAll, &QPushButton::clicked, this, &PrefsPage::showAllSettings );
    cardLayout->addWidget( cardRow( qtr( "Show all settings" ),
                                    qtr( "The full module tree, for power users." ), showAll, true ) );
    resetButton = new QPushButton( qtr( "Reset..." ) );
    resetButton->setProperty( "role", "outline" );
    connect( resetButton, &QPushButton::clicked, this, &PrefsPage::reset );
    cardLayout->addWidget( cardRow( qtr( "Reset preferences" ),
                                    qtr( "Restore every setting to its default." ), resetButton, false ) );
    advLayout->addWidget( card );
    advLayout->addStretch( 1 );
    /* the prototype's column: at most ContentMaxW wide, on the left; the
     * sections' scroll areas keep the page's width (their bar on its edge) */
    advanced->setMaximumWidth( t->px( Prefs::ContentMaxW ) );
    stack->addWidget( advanced );

    QWidget *buttonBar = new QWidget;
    buttonBar->setMaximumWidth( t->px( Prefs::ContentMaxW ) );
    QHBoxLayout *buttons = new QHBoxLayout( buttonBar );
    buttons->setContentsMargins( 0, 0, 0, 0 );
    buttons->setSpacing( t->px( Prefs::ButtonGap ) );
    buttons->addStretch( 1 );
    cancelButton = new QPushButton( qtr( "&Cancel" ) );
    cancelButton->setToolTip( qtr( "Discard the changes not saved yet" ) );
    connect( cancelButton, &QPushButton::clicked, this, &PrefsPage::cancel );
    buttons->addWidget( cancelButton );
    /* no mnemonic: Alt+S is the Tools menu's */
    saveButton = new QPushButton( qtr( "Save" ) );
    saveButton->setProperty( "role", "primary" );
    saveButton->setToolTip( qtr( "Save the preferences" ) );
    connect( saveButton, &QPushButton::clicked, this, &PrefsPage::save );
    buttons->addWidget( saveButton );
    right->addWidget( buttonBar );

    /* where config_SaveConfigFile() writes */
    char *path = var_InheritString( p_intf, "config" );
    if( path && *path )
        configFile = qfu( path );
    else if( char *dir = config_GetUserDir( VLC_CONFIG_DIR ) )
    {
        configFile = qfu( dir ) + QLatin1String( DIR_SEP "vlcrc" );
        free( dir );
    }
    free( path );
    if( !configFile.isEmpty() )
    {
        configTime = QFileInfo( configFile ).lastModified();
        QFileSystemWatcher *watcher = new QFileSystemWatcher( this );
        watcher->addPath( QFileInfo( configFile ).absolutePath() );
        connect( watcher, &QFileSystemWatcher::directoryChanged, this, &PrefsPage::configSaved );
    }
}

void PrefsPage::showEvent( QShowEvent *e )
{
    if( tabAnchor )
    {
        /* the page joined the window's focus chain at its end: its buttons
         * go after the sections, once (the panels go before them) */
        setTabOrder( tabAnchor, showAll );
        setTabOrder( showAll, resetButton );
        setTabOrder( resetButton, cancelButton );
        setTabOrder( cancelButton, saveButton );
        tabAnchor = NULL;
    }
    showSection( section );
    QWidget::showEvent( e );
}

void PrefsPage::showSection( int i )
{
    if( i < 0 || i >= SectionCount )
        return;
    section = i;
    title->setText( qtr( sections[i].label ) );
    if( i == Advanced )
    {
        stack->setCurrentWidget( advanced );
        return;
    }
    if( !panels[i] )
    {
        panels[i] = new SPrefsPanel( p_intf, stack, i );
        stack->addWidget( panels[i] );
        /* the page's title names the section: the panel's own goes */
        QLayoutItem *panelTitle = panels[i]->layout()->itemAt( 0 );
        if( panelTitle && panelTitle->widget() )
            panelTitle->widget()->hide();
        /* the form as cards of rows; its scroll area is no Tab stop (it has
         * no focus ring), Tab scrolls to the control it reaches */
        QScrollArea *scroller = panels[i]->findChild<QScrollArea *>();
        scroller->setFocusPolicy( Qt::NoFocus );
        QList<QWidget *> order;
        toRows( scroller->widget(), QString(), false, false, order );
        sizeFields( scroller->widget() );
        scroller->widget()->setMaximumWidth( Theme::instance()->px( Prefs::ContentMaxW ) );
        /* built after the page, its widgets end the window's focus chain:
         * move them after the sections and before Advanced's buttons,
         * Cancel and Save, in row order */
        QWidget *prev = showAll->previousInFocusChain();
        foreach( QWidget *w, order )
            /* setTabOrder() ignores NoFocus widgets, and refuses those of
             * another window: a combo box's list is in its pop-up */
            if( ( w->focusPolicy() & Qt::TabFocus ) && w->window() == window() )
            {
                setTabOrder( prev, w );
                prev = w;
            }
    }
    stack->setCurrentWidget( panels[i] );
}

void PrefsPage::setNarrow( bool b )
{
    narrow = b;
    sizeFields( this );
}

/* the prototype's select width: 240, 180 at XS */
void PrefsPage::sizeFields( QWidget *root )
{
    const int w = Theme::instance()->px( narrow ? Prefs::ControlMinWXS : Prefs::ControlMinW );
    foreach( QWidget *field, root->findChildren<QWidget *>() )
        if( field->property( "prefsField" ).toBool() )
            field->setMinimumWidth( w );
}

void PrefsPage::rebuild()
{
    for( int i = 0; i < SPrefsMax; i++ )
        if( panels[i] )
        {
            stack->removeWidget( panels[i] );
            panels[i]->deleteLater();  /* Esc may come from a widget in it */
            panels[i] = NULL;
        }
    if( isVisible() )
        showSection( section );
}

/* PrefsDialog::save for the simple panels */
void PrefsPage::save()
{
    msg_Dbg( p_intf, "Saving the simple preferences" );
    for( int i = 0; i < SPrefsMax; i++ )
        if( panels[i] )
            panels[i]->apply();
    PrefsDialog::saveConfig( p_intf );
    configTime = QFileInfo( configFile ).lastModified();  /* not a change from elsewhere */
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

/* the complete tree stays a dialog; what it saves shows here afterwards
 * (configSaved) */
void PrefsPage::showAllSettings()
{
    PrefsDialog *dialog = new PrefsDialog( window(), p_intf, true );
    dialog->toggleVisible();
}

/* The configuration was saved elsewhere (the dialog, its Reset, Effects):
 * built panels hold the values from before and would write them back on
 * Save, so build them again. Windows VLC deletes and renames vlcrc, so its
 * folder is watched and the file's time compared.
 * shortcut: edits pending on the page are dropped then, keep them per
 * option if users hit it */
void PrefsPage::configSaved()
{
    const QDateTime saved = QFileInfo( configFile ).lastModified();
    if( !saved.isValid() || saved == configTime )
        return;
    configTime = saved;
    rebuild();
}
