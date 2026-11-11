/*****************************************************************************
 * home_page.cpp : the library's Home page (continue watching, recent media)
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

/* before vlc_threads.h, which makes a macro of QThread::msleep's name */
#include <QThreadPool>

#include "qt.hpp"
#include "components/library/home_page.hpp"
#include "components/library/library_widgets.hpp"
#include "components/library/thumbnailer.hpp"
#include "components/now_playing.hpp"   /* paintStripes, fontAt */
#include "input_manager.hpp"
#include "recents.hpp"
#include "util/breakpoints.hpp"
#include "util/customwidgets.hpp"     /* timeString */
#include "util/redesign_icons.hpp"
#include "util/theme.hpp"

#include <vlc_interface.h>              /* EXTENSIONS_AUDIO */
#include <vlc_url.h>

#include <QAbstractButton>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

using namespace RedesignTokens;

namespace {

enum Column { TitleCol, PathCol, DurationCol, WhenCol, ColumnCount };
enum { MrlRole = Qt::UserRole, LocalPathRole, IconRole };

QString dash() { return QString( QChar( 0x2014 ) ); }

QString missingText()
{
    return qtr( "Not found: it may have been moved or deleted, or be on a "
                "drive or share that is not connected." );
}

/* What a recent MRL shows: title, full path, local file and type glyph */
struct Entry
{
    QString title, path, localPath;
    const char *icon;
};

bool isAudio( const QString &path )
{
    static const QStringList exts = QString( EXTENSIONS_AUDIO ).split( ';' );
    return exts.contains( QLatin1String( "*." ) + QFileInfo( path ).suffix().toLower() );
}

/* String work only: nothing here may touch the disk (offline shares) */
Entry describe( const QString &mrl )
{
    Entry e;
    const QString scheme = mrl.section( QLatin1Char( ':' ), 0, 0 ).toLower();
    const bool disc = scheme == "dvd" || scheme == "bluray";
    if( scheme == "file" || scheme == "directory" || disc )
    {
        /* directory:// (Open Folder) and the discs hold a file path too */
        const QString fileUri = QLatin1String( "file" ) + mrl.mid( scheme.length() );
        char *psz = vlc_uri2path( qtu( fileUri ) );
        if( psz )
            e.localPath = qfu( psz );
        free( psz );
    }
    if( !e.localPath.isEmpty() )
    {
        e.path = e.localPath;
        e.title = QFileInfo( e.localPath ).fileName();
    }
    else
    {
        char *psz = vlc_uri_decode_duplicate( qtu( mrl ) );
        e.path = psz ? qfu( psz ) : mrl;
        free( psz );
        const QUrl url( mrl );
        e.title = url.fileName( QUrl::FullyDecoded );
        if( e.title.isEmpty() )
            e.title = url.host();
    }
    if( e.title.isEmpty() )
        e.title = e.path;

    if( scheme == "directory" )
        e.icon = RedesignIcons::IconFolder;
    else if( disc )
        e.icon = RedesignIcons::IconOpenDisc;
    else if( e.localPath.isEmpty() )
        e.icon = RedesignIcons::IconStream;
    else
        e.icon = isAudio( e.localPath ) ? RedesignIcons::IconMusic : RedesignIcons::IconVideo;
    return e;
}

/* "Today, 09:02", "Yesterday", a weekday within the week, else the date */
QString whenText( const QDateTime &utc )
{
    if( !utc.isValid() )
        return dash();
    const QDateTime local = utc.toLocalTime();
    const qint64 days = local.date().daysTo( QDate::currentDate() );
    const QLocale locale;
    if( days <= 0 )
        return qtr( "Today, %1" ).arg( locale.toString( local.time(), QLocale::ShortFormat ) );
    if( days == 1 )
        return qtr( "Yesterday" );
    if( days < 7 )
        return locale.dayName( local.date().dayOfWeek() );
    return locale.toString( local.date(), QLocale::ShortFormat );
}

QColor focusRingColor()
{
    /* the accent is 2.4:1 on light surfaces */
    const Theme *t = Theme::instance();
    return t->color( t->isDark() ? Theme::Accent : Theme::AccentText );
}

/* Rows of DESIGN_SPEC 4 "Recent media": 44 px, the path elided in the
 * middle, and one focus ring around the current row */
class RecentDelegate : public QStyledItemDelegate
{
public:
    explicit RecentDelegate( QObject *parent ) : QStyledItemDelegate( parent ) {}

    QSize sizeHint( const QStyleOptionViewItem &option,
                    const QModelIndex &index ) const Q_DECL_OVERRIDE
    {
        QSize s = QStyledItemDelegate::sizeHint( option, index );
        s.setHeight( Theme::instance()->px( Home::RowH ) );
        return s;
    }

    void paint( QPainter *p, const QStyleOptionViewItem &option,
                const QModelIndex &index ) const Q_DECL_OVERRIDE
    {
        QStyleOptionViewItem opt( option );
        opt.state &= ~QStyle::State_HasFocus;
        QStyledItemDelegate::paint( p, opt, index );

        const QAbstractItemView *view = qobject_cast<const QAbstractItemView *>( option.widget );
        if( !view || !view->hasFocus() || view->currentIndex().row() != index.row() )
            return;
        const int w = Size::FocusRing;
        const QRect r = option.rect;
        const QColor c = focusRingColor();
        p->fillRect( r.left(), r.top(), r.width(), w, c );
        p->fillRect( r.left(), r.bottom() - w + 1, r.width(), w, c );
        const bool first = option.viewItemPosition == QStyleOptionViewItem::Beginning
                        || option.viewItemPosition == QStyleOptionViewItem::OnlyOne;
        const bool last = option.viewItemPosition == QStyleOptionViewItem::End
                       || option.viewItemPosition == QStyleOptionViewItem::OnlyOne;
        /* the first column is on the right in RTL */
        if( first )
            p->fillRect( QStyle::visualRect( option.direction, r,
                                             QRect( r.left(), r.top(), w, r.height() ) ), c );
        if( last )
            p->fillRect( QStyle::visualRect( option.direction, r,
                                             QRect( r.right() - w + 1, r.top(), w, r.height() ) ), c );
    }

protected:
    void initStyleOption( QStyleOptionViewItem *option,
                          const QModelIndex &index ) const Q_DECL_OVERRIDE
    {
        QStyledItemDelegate::initStyleOption( option, index );
        if( index.column() == PathCol )
            option->textElideMode = Qt::ElideMiddle;
    }
};

QLabel *sectionLabel( const QString &text, QWidget *parent )
{
    QLabel *label = new QLabel( text, parent );
    label->setFont( fontAt( Theme::PanelTitle, Home::SectionTitle, true ) );
    return label;
}

} // namespace

/* A "Continue watching" card: a frame of the video (a placeholder with the
 * type glyph until there is one), the time left and the progress, then the
 * title and where it resumes */
class ResumeCard : public QAbstractButton
{
public:
    ResumeCard( const QString &_mrl, const QString &title, const QString &_path,
                const QString &_localVideo, const char *_icon, int64_t _timeMs,
                int64_t _lengthMs, ArtLoader *_art, Thumbnailer *_thumbs, QWidget *parent )
        : QAbstractButton( parent ), mrl( _mrl ), path( _path ), localVideo( _localVideo ),
          icon( _icon ), timeMs( _timeMs ), lengthMs( _lengthMs ), progress( -1 ),
          missing( false ), art( _art ), thumbs( _thumbs )
    {
        setText( title );
        meta = qtr( "Resume from %1" ).arg( timeString( timeMs / 1000 ) );
        setAccessibleName( title );
        /* Tab only: a click must not take the keys from the player */
        setFocusPolicy( Qt::TabFocus );
        setAttribute( Qt::WA_Hover );
        setCursor( Qt::PointingHandCursor );
        setContextMenuPolicy( Qt::CustomContextMenu );
        setLength( lengthMs );
    }

    void setLength( int64_t _lengthMs )
    {
        lengthMs = _lengthMs;
        badge.clear();
        progress = -1;
        if( lengthMs > timeMs )
        {
            badge = qtr( "%1 left" ).arg( timeString( ( lengthMs - timeMs ) / 1000 ) );
            progress = qreal( timeMs ) / lengthMs;
        }
        setMissing( missing );
    }

    void setMissing( bool b )
    {
        missing = b;
        QStringList description( meta );
        if( !badge.isEmpty() )
            description << badge;
        if( b )
            description << missingText();
        setAccessibleDescription( description.join( ", " ) );
        setToolTip( b ? path + "\n" + missingText() : path );
        update();
    }

    static int thumbHeight( int w ) { return w * 9 / 16; }
    static QFont titleFont() { return fontAt( Theme::Body, Home::CardTitle, true ); }
    static int heightFor( int w )
    {
        const Theme *t = Theme::instance();
        return thumbHeight( w ) + t->px( Home::CardTextGap )
             + QFontMetrics( titleFont() ).height() + t->px( Home::CardMetaGap )
             + QFontMetrics( t->font( Theme::Meta ) ).height();
    }

    const QString mrl;

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    void keyPressEvent( QKeyEvent *e ) Q_DECL_OVERRIDE
    {
        if( e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter )
            click();
        else
            QAbstractButton::keyPressEvent( e );
    }

private:
    QString path, localVideo, meta, badge;
    const char *icon;
    const int64_t timeMs;
    int64_t lengthMs;
    qreal progress;     /* 0..1, -1 when the length is unknown */
    bool missing;
    ArtLoader *art;     /* the page's */
    Thumbnailer *thumbs;
};

void ResumeCard::paintEvent( QPaintEvent * )
{
    Theme *t = Theme::instance();
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );
    if( missing )
        p.setOpacity( Home::MissingOpacityPercent / 100. );

    const int thumbH = thumbHeight( width() );
    const QRectF thumb( 0, 0, width(), thumbH );
    const qreal radius = t->metric( Theme::RadiusLg );
    QPainterPath clip;
    clip.addRoundedRect( thumb, radius, radius );
    p.save();
    p.setClipPath( clip );
    const qreal dpr = devicePixelRatioF();
    QPixmap frame;
    if( !localVideo.isEmpty() && !missing )
        frame = art->get( thumbs->frame( localVideo, lengthMs ), ( thumb.size() * dpr ).toSize() );
    if( frame.isNull() )
    {
        paintStripes( p, thumb, t->px( Home::CardStripe ) );
        const int glyph = t->px( Home::CardGlyph );
        const int devicePx = qRound( glyph * dpr );
        t->icon( QLatin1String( icon ), Theme::Dim, QSize( devicePx, devicePx ) )
            .paint( &p, QRect( ( width() - glyph ) / 2, ( thumbH - glyph ) / 2, glyph, glyph ) );
    }
    else
    {
        /* cropped to fill, as the library cards */
        frame.setDevicePixelRatio( dpr );
        const QSizeF s = QSizeF( frame.size() ) / dpr;
        p.drawPixmap( QPointF( thumb.center().x() - s.width() / 2,
                               thumb.center().y() - s.height() / 2 ), frame );
    }

    if( progress >= 0 )
        paintProgress( &p, thumb, progress, layoutDirection() );
    if( !badge.isEmpty() )
    {
        const QFont f = fontAt( Theme::Overline, Type::Overline, false );
        const QFontMetrics fm( f );
        const int padX = t->px( Home::BadgePadX ), padY = t->px( Home::BadgePadY );
        const QSize s( fm.width( badge ) + 2 * padX, fm.height() + 2 * padY );
        const QRect r( width() - t->px( Home::BadgeInsetX ) - s.width(),
                       thumbH - t->px( Home::BadgeInsetY ) - s.height(), s.width(), s.height() );
        const int badgeRadius = t->px( Library::BadgeRadius );   /* as the library cards' */
        p.setPen( Qt::NoPen );
        p.setBrush( QColor::fromRgba( CardBadgeBg ) );
        const QRect at = QStyle::visualRect( layoutDirection(), rect(), r );
        p.drawRoundedRect( at, badgeRadius, badgeRadius );
        p.setFont( f );
        p.setPen( t->color( Theme::OnVideo ) );
        p.drawText( at, Qt::AlignCenter, badge );
    }
    p.restore();

    /* the outline: accent on hover, the focus ring from the keyboard; the
     * ring stays at full strength on a missing file */
    if( hasFocus() )
        p.setOpacity( 1 );
    const QPen pen = hasFocus() ? QPen( focusRingColor(), Size::FocusRing )
                   : QPen( underMouse() ? t->color( Theme::Accent ) : t->ink( InkAlpha::Divider ),
                           Size::Hairline );
    const qreal inset = pen.widthF() / 2;
    p.setPen( pen );
    p.setBrush( Qt::NoBrush );
    p.drawRoundedRect( thumb.adjusted( inset, inset, -inset, -inset ),
                       radius - inset, radius - inset );
    if( missing )
        p.setOpacity( Home::MissingOpacityPercent / 100. );

    const QFont tf = titleFont();
    const QFont mf = t->font( Theme::Meta );
    const QFontMetrics tfm( tf ), mfm( mf );
    int y = thumbH + t->px( Home::CardTextGap );
    p.setFont( tf );
    p.setPen( t->color( Theme::Text ) );
    p.drawText( QRect( 0, y - t->textNudge( p.font() ), width(), tfm.height() ), Qt::AlignLeft | Qt::AlignVCenter,
                tfm.elidedText( text(), Qt::ElideRight, width() ) );
    y += tfm.height() + t->px( Home::CardMetaGap );
    p.setFont( mf );
    p.setPen( t->color( Theme::Mute ) );
    p.drawText( QRect( 0, y - t->textNudge( p.font() ), width(), mfm.height() ), Qt::AlignLeft | Qt::AlignVCenter,
                mfm.elidedText( meta, Qt::ElideRight, width() ) );
}

/**********************************************************************
 * ExistsCheck
 **********************************************************************/
void ExistsCheck::run()
{
    QStringList missing;
    for( int i = 0; i < paths.count(); i++ )
        if( !QFileInfo::exists( paths.at( i ) ) )
            missing.append( mrls.at( i ) );
    emit done( missing );
}

/**********************************************************************
 * HomePage
 **********************************************************************/
HomePage::HomePage( intf_thread_t *_p_intf, QWidget *parent )
    : QScrollArea( parent ), p_intf( _p_intf ), art( new ArtLoader( this ) ),
      thumbs( new Thumbnailer( _p_intf, this ) ), checking( false ), recheck( false )
{
    Theme *t = Theme::instance();
    connect( art, &ArtLoader::ready, this, &HomePage::updateCards );
    connect( thumbs, &Thumbnailer::changed, this, &HomePage::updateCards );
    setAccessibleName( qtr( "Home" ) );
    /* the cards and the table are the Tab stops; QScrollArea scrolls to them */
    setFocusPolicy( Qt::NoFocus );
    setFrameShape( QFrame::NoFrame );
    setWidgetResizable( true );
    setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );

    QWidget *content = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout( content );
    layout->setContentsMargins( t->px( Size::PagePadX ), t->px( Size::PagePadTop ),
                                t->px( Size::PagePadX ), t->px( Size::PagePadX ) );
    layout->setSpacing( 0 );
    /* the library pages' title row: its search filters the cards and the
     * table, on this page's own list (never PLModel::filter) */
    header = new LibraryHeader( qtr( "Home" ), content );
    layout->addWidget( header );
    layout->addSpacing( t->px( Home::TitleBottom ) );

    /* Continue watching, then Recent media */
    lists = new QWidget( content );
    QVBoxLayout *listsLayout = new QVBoxLayout( lists );
    listsLayout->setContentsMargins( 0, 0, 0, 0 );
    listsLayout->setSpacing( t->px( Home::SectionGap ) );

    cardsSection = new QWidget( lists );
    QVBoxLayout *cardsSectionLayout = new QVBoxLayout( cardsSection );
    cardsSectionLayout->setContentsMargins( 0, 0, 0, 0 );
    cardsSectionLayout->setSpacing( t->px( Home::CardsTitleBottom ) );
    cardsSectionLayout->addWidget( sectionLabel( qtr( "Continue watching" ), cardsSection ) );
    cardsArea = new QScrollArea( cardsSection );
    cardsArea->setAccessibleName( qtr( "Continue watching" ) );
    cardsArea->setFrameShape( QFrame::NoFrame );
    cardsArea->setWidgetResizable( true );
    cardsArea->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    cardsArea->setFocusPolicy( Qt::NoFocus );
    QWidget *row = new QWidget;
    cardsLayout = new QHBoxLayout( row );
    cardsLayout->setContentsMargins( 0, 0, 0, 0 );
    cardsLayout->setSpacing( t->px( Home::CardGap ) );
    cardsLayout->addStretch( 1 );
    cardsArea->setWidget( row );
    cardsSectionLayout->addWidget( cardsArea );
    listsLayout->addWidget( cardsSection );

    recentSection = new QWidget( lists );
    QVBoxLayout *recentLayout = new QVBoxLayout( recentSection );
    recentLayout->setContentsMargins( 0, 0, 0, 0 );
    recentLayout->setSpacing( t->px( Home::TableTitleBottom ) );
    recentLayout->addWidget( sectionLabel( qtr( "Recent media" ), recentSection ) );
    table = new QTreeWidget( recentSection );
    table->setAccessibleName( qtr( "Recent media" ) );
    table->setProperty( "role", "homeRecent" );
    table->setColumnCount( ColumnCount );
    table->setHeaderHidden( true );
    table->setRootIsDecorated( false );
    table->setIndentation( 0 );
    table->setUniformRowHeights( true );
    table->setAllColumnsShowFocus( true );   /* repaints the whole row's ring */
    table->setSelectionMode( QAbstractItemView::SingleSelection );
    table->setFrameShape( QFrame::NoFrame );
    table->setFocusPolicy( Qt::TabFocus );
    table->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    table->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    table->setContextMenuPolicy( Qt::CustomContextMenu );
    table->setIconSize( QSize( t->px( Home::RowIcon ), t->px( Home::RowIcon ) ) );
    table->setItemDelegate( new RecentDelegate( table ) );
    table->header()->setStretchLastSection( false );
    table->header()->setSectionResizeMode( QHeaderView::Fixed );
    table->header()->setSectionResizeMode( TitleCol, QHeaderView::Stretch );
    table->setCursor( Qt::PointingHandCursor );
    recentLayout->addWidget( table );
    listsLayout->addWidget( recentSection );
    layout->addWidget( lists );

    /* inventory 1.1.1: nothing played yet; up to its token height, less
     * in a short window (LibraryEmptyState::sizeHint) */
    empty = new LibraryEmptyState(
        qtr( "Media you play shows up here, so you can pick up where you left off." ),
        qtr( "Recently played items are not being saved. Turn on "
             "\"Save recently played items\" in Preferences, Interface." ), content );
    layout->addWidget( empty );
    layout->addStretch( 1 );
    setWidget( content );

    connect( header->search(), &QLineEdit::textChanged, this, &HomePage::rebuild );

    connect( table, &QAbstractItemView::clicked, this, [this]( const QModelIndex &i ) {
        play( i.sibling( i.row(), TitleCol ).data( MrlRole ).toString(), false );
    } );
    /* Enter; a double click already played on its first click */
    connect( table, &QAbstractItemView::activated, this, [this]( const QModelIndex &i ) {
        if( QApplication::mouseButtons() == Qt::NoButton )
            play( i.sibling( i.row(), TitleCol ).data( MrlRole ).toString(), false );
    } );
    connect( table, &QWidget::customContextMenuRequested, this, [this]( const QPoint &pos ) {
        QTreeWidgetItem *item = table->itemAt( pos );
        QPoint at = table->viewport()->mapToGlobal( pos );
        if( !item )
        {
            /* the menu key: the current row */
            item = table->currentItem();
            if( item )
                at = table->viewport()->mapToGlobal( table->visualItemRect( item ).center() );
        }
        if( item )
            popup( item->data( TitleCol, MrlRole ).toString(), at );
    } );
    /* queued: the list changes under a card's or row's own click */
    connect( RecentsMRL::getInstance( p_intf ), &RecentsMRL::changed,
             this, &HomePage::rebuild, Qt::QueuedConnection );
    connect( t, &Theme::changed, this, &HomePage::rebuild );
    /* an entry gets its length when its input closes: show it while it plays */
    connect( THEMIM->getIM(), &InputManager::positionUpdated, this, [this]( float, int64_t, int len ) {
        input_thread_t *input = THEMIM->getInput();
        if( len <= 0 || !input )
            return;
        char *uri = input_item_GetURI( input_GetItem( input ) );
        const QString mrl = qfu( uri );
        free( uri );
        const int64_t lengthMs = int64_t( len ) * 1000;
        /* in place: a rebuild once a second would take the focus and the
         * screen reader's place */
        if( !RecentsMRL::getInstance( p_intf )->setLength( mrl, lengthMs ) )
            return;
        foreach( ResumeCard *card, cards )
            if( card->mrl == mrl )
                card->setLength( lengthMs );
        for( int i = 0; i < table->topLevelItemCount(); i++ )
        {
            QTreeWidgetItem *item = table->topLevelItem( i );
            if( item->data( TitleCol, MrlRole ).toString() == mrl )
                item->setText( DurationCol, timeString( lengthMs / 1000 ) );
        }
    } );

    rebuild();
}

void HomePage::rebuild()
{
    const Theme *t = Theme::instance();
    /* read each time: Preferences can turn it off while VLC runs */
    const bool enabled = var_InheritBool( p_intf, "qt-recentplay" );
    RecentsMRL *recents = RecentsMRL::getInstance( p_intf );
    const QStringList mrls = enabled ? recents->recentList() : QStringList();

    /* later: a card's own menu may still be open on the stack */
    foreach( ResumeCard *card, cards )
    {
        card->hide();
        card->deleteLater();
    }
    cards.clear();
    const int currentRow = table->currentIndex().row();
    table->clear();

    const QString search = header->search()->text().trimmed();
    foreach( const QString &mrl, mrls )
    {
        const Entry e = describe( mrl );
        if( !e.title.contains( search, Qt::CaseInsensitive )
         && !e.path.contains( search, Qt::CaseInsensitive ) )
            continue;
        const int64_t timeMs = recents->time( mrl );
        const int64_t lengthMs = recents->length( mrl );
        if( timeMs > 0 )
        {
            /* frames of videos only: not of a disc, a folder or a stream */
            const QString video = e.icon == RedesignIcons::IconVideo ? e.localPath : QString();
            ResumeCard *card = new ResumeCard( mrl, e.title, e.path, video, e.icon, timeMs, lengthMs,
                                               art, thumbs, cardsArea->widget() );
            connect( card, &QAbstractButton::clicked, this, [this, mrl]() { play( mrl, true ); } );
            connect( card, &QWidget::customContextMenuRequested, this,
                     [this, card]( const QPoint &pos ) { popup( card->mrl, card->mapToGlobal( pos ) ); } );
            cardsLayout->insertWidget( cards.count(), card );
            cards.append( card );
        }

        QTreeWidgetItem *item = new QTreeWidgetItem( table );
        item->setData( TitleCol, MrlRole, mrl );
        item->setData( TitleCol, LocalPathRole, e.localPath );
        item->setData( TitleCol, IconRole, QString( e.icon ) );
        item->setText( TitleCol, e.title );
        item->setText( PathCol, e.path );
        item->setFont( PathCol, t->font( Theme::Mono ) );
        item->setText( DurationCol, lengthMs > 0 ? timeString( lengthMs / 1000 ) : dash() );
        item->setText( WhenCol, whenText( recents->date( mrl ) ) );
        item->setTextAlignment( WhenCol, Qt::AlignRight | Qt::AlignVCenter );
    }
    /* new cards join the end of the focus chain: put them before the table */
    QWidget *previous = NULL;
    foreach( ResumeCard *card, cards )
    {
        if( previous )
            setTabOrder( previous, card );
        previous = card;
    }
    if( previous )
        setTabOrder( previous, table );

    const int count = table->topLevelItemCount();
    table->setFixedHeight( count * t->px( Home::RowH ) );
    if( table->hasFocus() && currentRow >= 0 && count > 0 )
        table->setCurrentItem( table->topLevelItem( qMin( currentRow, count - 1 ) ) );

    cardsSection->setVisible( !cards.isEmpty() );
    recentSection->setVisible( count > 0 );
    lists->setVisible( !mrls.isEmpty() );
    empty->setVisible( mrls.isEmpty() );
    empty->setNoteVisible( !enabled );
    header->setStatus( !mrls.isEmpty() && count == 0 ? qtr( "No results" ) : QString() );

    applyMissing();
    layoutForWidth();
    if( isVisible() )
        startCheck();
}

void HomePage::startCheck()
{
    if( checking )
    {
        recheck = true;
        return;
    }
    QStringList mrls, paths;
    for( int i = 0; i < table->topLevelItemCount(); i++ )
    {
        const QTreeWidgetItem *item = table->topLevelItem( i );
        const QString path = item->data( TitleCol, LocalPathRole ).toString();
        if( !path.isEmpty() )
        {
            mrls << item->data( TitleCol, MrlRole ).toString();
            paths << path;
        }
    }
    if( paths.isEmpty() )
        return;
    checking = true;
    ExistsCheck *check = new ExistsCheck( mrls, paths );
    /* queued: a result that comes after this page is gone is dropped */
    connect( check, &ExistsCheck::done, this, &HomePage::setMissing, Qt::QueuedConnection );
    LibraryData::workerPool()->start( check );
}

void HomePage::setMissing( const QStringList &mrls )
{
    checking = false;
    missing = QSet<QString>::fromList( mrls );
    applyMissing();
    if( recheck )
    {
        recheck = false;
        startCheck();
    }
}

void HomePage::applyMissing()
{
    Theme *t = Theme::instance();
    foreach( ResumeCard *card, cards )
        card->setMissing( missing.contains( card->mrl ) );

    const int iconPx = t->px( Home::RowIcon );
    for( int i = 0; i < table->topLevelItemCount(); i++ )
    {
        QTreeWidgetItem *item = table->topLevelItem( i );
        const bool gone = missing.contains( item->data( TitleCol, MrlRole ).toString() );
        item->setIcon( TitleCol, t->icon( item->data( TitleCol, IconRole ).toString(),
                                          gone ? Theme::Dim : Theme::Accent,
                                          QSize( iconPx, iconPx ) ) );
        item->setForeground( TitleCol, t->color( gone ? Theme::Dim : Theme::Text ) );
        item->setForeground( PathCol, t->color( Theme::Dim ) );
        item->setForeground( DurationCol, t->color( gone ? Theme::Dim : Theme::Mute ) );
        item->setForeground( WhenCol, t->color( gone ? Theme::Dim : Theme::Mute ) );
        const QString path = item->text( PathCol );
        item->setData( TitleCol, Qt::AccessibleTextRole, gone
                       ? qtr( "%1, not found" ).arg( item->text( TitleCol ) ) : QVariant() );
        for( int col = 0; col < ColumnCount; col++ )
            item->setToolTip( col, gone ? path + "\n" + missingText() : path );
    }
}

void HomePage::layoutForWidth()
{
    const Theme *t = Theme::instance();
    const BreakpointWatcher::Size bp = BreakpointWatcher::fromWidth( window()->width() );
    static const int colsAt[] = { Home::ColsXS, Home::ColsS, Home::ColsM, Home::ColsL };
    const int cols = colsAt[bp];
    const int gap = t->px( Home::CardGap );
    const int avail = viewport()->width() - 2 * t->px( Size::PagePadX );
    const int w = qMax( 1, ( avail - ( cols - 1 ) * gap ) / cols );
    const int h = ResumeCard::heightFor( w );
    foreach( ResumeCard *card, cards )
        card->setFixedSize( w, h );
    /* room for the scroll bar either way, so the page does not jump */
    cardsArea->setFixedHeight( h + cardsArea->horizontalScrollBar()->sizeHint().height() );

    /* the tokens are the text widths; the stylesheet pads each cell by
     * half the column gap, and the row's end by RowPadX */
    const bool small = bp <= BreakpointWatcher::S;
    const int cellPad = 2 * ( t->px( Home::ColumnGap ) / 2 );
    table->setColumnHidden( PathCol, bp == BreakpointWatcher::XS );
    const int duration = t->px( small ? Home::DurationColS : Home::DurationColL ) + cellPad;
    table->header()->resizeSection( DurationCol, duration );
    /* a 12-hour clock ("Today, 10:52 AM") is wider than the design's: fit
     * that in this locale, and every row's text, in the font it is drawn in
     * (the stylesheet's, so polished first) */
    table->ensurePolished();
    const QFontMetrics fm( table->font() );
    const QFontMetrics pathFm( t->font( Theme::Mono ) );
    int when = qMax( t->px( small ? Home::WhenColS : Home::WhenColL ),
                     fm.width( whenText( QDateTime( QDate::currentDate(), QTime( 10, 52 ) ).toUTC() ) ) );
    int titleW = 0, pathW = 0;
    for( int i = 0; i < table->topLevelItemCount(); i++ )
    {
        const QTreeWidgetItem *item = table->topLevelItem( i );
        when = qMax( when, fm.width( item->text( WhenCol ) ) );
        titleW = qMax( titleW, fm.width( item->text( TitleCol ) ) );
        pathW = qMax( pathW, pathFm.width( item->text( PathCol ) ) );
    }
    /* the delegate's own text margins, as QCommonStyle lays out item text */
    const int textMargin = table->style()->pixelMetric( QStyle::PM_FocusFrameHMargin, 0, table ) + 1;
    when += 2 * textMargin + t->px( Home::ColumnGap ) / 2 + t->px( Home::RowPadX );
    table->header()->resizeSection( WhenCol, when );
    /* the path takes the room the titles leave, never less than the design's
     * width: the title column stretches over the rest */
    const int title = t->px( Home::RowPadX ) + t->px( Home::IconCol ) + titleW
                    + 2 * textMargin + cellPad;
    const int path = qMin( pathW + 2 * textMargin + cellPad, avail - title - duration - when );
    table->header()->resizeSection( PathCol,
        qMax( t->px( small ? Home::PathColS : Home::PathColL ) + cellPad, path ) );
}

void HomePage::play( const QString &mrl, bool resume )
{
    /* the first click of a double click already played, and rebuilt the
     * page: the second must not play what is now under the mouse */
    if( sincePlay.isValid() && sincePlay.elapsed() < QApplication::doubleClickInterval() )
        return;
    sincePlay.start();

    RecentsMRL *recents = RecentsMRL::getInstance( p_intf );
    /* P4-4: a card always resumes, without the qt-continue question; not
     * an option on the item, which would start every replay there too */
    if( resume )
        recents->resumeOnce( mrl );
    recents->playMRL( mrl );
}

void HomePage::popup( const QString mrl, const QPoint &globalPos )
{
    const Entry e = describe( mrl );
    QMenu menu( this );
    QAction *playAction = menu.addAction(
        Theme::themedIcon( RedesignIcons::IconPlay, Theme::Text2 ), qtr( "Play" ) );
    QAction *showAction = menu.addAction(
        Theme::themedIcon( RedesignIcons::IconShowFolder, Theme::Text2 ),
        qtr( "Show containing folder" ) );
    showAction->setEnabled( !e.localPath.isEmpty() && !missing.contains( mrl ) );
    QAction *copyAction = menu.addAction(
        Theme::themedIcon( RedesignIcons::IconCopy, Theme::Text2 ), qtr( "Copy path" ) );
    menu.addSeparator();
    QAction *removeAction = menu.addAction(
        Theme::themedIcon( RedesignIcons::IconDelete, Theme::Text2 ),
        qtr( "Remove from recent media" ) );
    Theme::setDestructive( removeAction );

    QAction *chosen = menu.exec( globalPos );
    if( chosen == playAction )
        play( mrl, false );
    else if( chosen == showAction )
        QDesktopServices::openUrl(
            QUrl::fromLocalFile( QFileInfo( e.localPath ).absolutePath() ) );
    else if( chosen == copyAction )
        QApplication::clipboard()->setText( e.path );
    else if( chosen == removeAction )
        RecentsMRL::getInstance( p_intf )->removeRecent( mrl );
}

void HomePage::updateCards()
{
    foreach( ResumeCard *card, cards )
        card->update();
}

void HomePage::hideEvent( QHideEvent *e )
{
    thumbs->cancel();
    QScrollArea::hideEvent( e );
}

void HomePage::showEvent( QShowEvent *e )
{
    QScrollArea::showEvent( e );
    window()->installEventFilter( this );   /* once: a filter is not added twice */
    /* "Today" moves on, files come and go */
    rebuild();
}

bool HomePage::eventFilter( QObject *o, QEvent *e )
{
    /* back from Explorer, say, where a file may have been deleted */
    if( o == window() && e->type() == QEvent::WindowActivate && isVisible() )
        startCheck();
    return QScrollArea::eventFilter( o, e );
}

bool HomePage::viewportEvent( QEvent *e )
{
    if( e->type() == QEvent::Resize )
        layoutForWidth();
    return QScrollArea::viewportEvent( e );
}
