/*****************************************************************************
 * now_playing.cpp : now-playing bar, audio mode and mini player
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

#include "components/now_playing.hpp"
#include "components/spectrum.hpp"
#include "components/controller_widget.hpp"      /* SoundWidget */
#include "components/playlist/playlist_model.hpp"
#include "components/playlist/sorting.h"
#include "input_manager.hpp"
#include "util/customwidgets.hpp"                /* QElidingLabel */
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"
#ifdef _WIN32
# include "util/win_theme.hpp"
#endif

#include <vlc_input_item.h>

#include <QApplication>
#include <QDesktopWidget>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QPropertyAnimation>
#include <QSettings>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

using namespace RedesignTokens;

namespace {
/* "Play/Pause (Space)": the label and the first key bound to the hotkey */
QString withKey( intf_thread_t *p_intf, const QString &label, const char *hotkey )
{
    if( hotkey == NULL )
        return label;
    char *psz = var_InheritString( p_intf, hotkey );
    const QString key = qfu( psz ).section( '\t', 0, 0 );
    free( psz );
    return key.isEmpty() ? label : qtr( "%1 (%2)" ).arg( label, key );
}
}

/* Stripes at 135 degrees in the theme's two placeholder tints */
void paintStripes( QPainter &p, const QRectF &r, qreal period )
{
    const Theme *t = Theme::instance();
    QLinearGradient g( 0, 0, period / 2, period / 2 );
    g.setSpread( QGradient::RepeatSpread );
    g.setColorAt( 0, t->color( Theme::Stripe1 ) );
    g.setColorAt( 0.5, t->color( Theme::Stripe1 ) );
    g.setColorAt( 0.501, t->color( Theme::Stripe2 ) );
    g.setColorAt( 1, t->color( Theme::Stripe2 ) );
    p.fillRect( r, g );
}

QFont fontAt( Theme::TypeRole role, int tokenPx, bool semibold )
{
    QFont f = Theme::instance()->font( role );
    f.setPointSizeF( tokenPx * 72.0 / 96.0 );
    f.setWeight( semibold ? QFont::DemiBold : QFont::Normal );
    return f;
}

/* Short codec names for the format chip and the library cards; others
 * show their fourcc */
QString codecName( const QString &fourcc )
{
    static const struct { const char *fcc, *name; } names[] = {
        { "mpga", "MP3" }, { "mp3 ", "MP3" }, { "mp4a", "AAC" }, { "flac", "FLAC" },
        { "vorb", "Vorbis" }, { "opus", "Opus" }, { "alac", "ALAC" }, { "a52 ", "AC-3" },
        { "eac3", "E-AC-3" }, { "dts ", "DTS" }, { "wma2", "WMA" }, { "araw", "PCM" },
        { "s16l", "PCM" }, { "s24l", "PCM" }, { "s32l", "PCM" }, { "f32l", "PCM" },
        { "h264", "H.264" }, { "hevc", "H.265" }, { "VP90", "VP9" }, { "VP80", "VP8" },
        { "av01", "AV1" }, { "mp2v", "MPEG-2" }, { "mpgv", "MPEG-2" }, { "mp4v", "MPEG-4" },
        { "WVC1", "VC-1" }, { "WMV3", "WMV" }, { "theo", "Theora" },
    };
    for( size_t i = 0; i < sizeof( names ) / sizeof( names[0] ); i++ )
        if( fourcc == QLatin1String( names[i].fcc ) )
            return QLatin1String( names[i].name );
    return fourcc.trimmed().toUpper();
}

/**********************************************************************
 * ArtView
 **********************************************************************/
ArtView::ArtView( int _stripe, int _radius, QWidget *parent )
    : QWidget( parent ), stripe( _stripe ), radius( _radius )
{
}

void ArtView::setArt( const QString &url )
{
    art = url.isEmpty() ? QPixmap() : QPixmap( url );
    QWidget::update();
}

void ArtView::paintEvent( QPaintEvent * )
{
    const Theme *t = Theme::instance();
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );
    const QRectF r = QRectF( rect() ).adjusted( 0.5, 0.5, -0.5, -0.5 );
    const qreal rad = t->px( radius );
    QPainterPath clip;
    clip.addRoundedRect( r, rad, rad );
    p.setClipPath( clip );
    if( art.isNull() )
        paintStripes( p, r, t->px( stripe ) );
    else
    {
        /* fill and crop, like CSS object-fit: cover */
        const qreal dpr = devicePixelRatioF();
        QPixmap scaled = art.scaled( size() * dpr, Qt::KeepAspectRatioByExpanding,
                                     Qt::SmoothTransformation );
        scaled.setDevicePixelRatio( dpr );
        const QSizeF s = QSizeF( scaled.size() ) / dpr;
        p.drawPixmap( QPointF( ( width() - s.width() ) / 2, ( height() - s.height() ) / 2 ),
                      scaled );
    }
    p.setClipping( false );
    p.setPen( QPen( t->ink( InkAlpha::Border ), Size::Hairline ) );
    p.setBrush( Qt::NoBrush );
    p.drawRoundedRect( r, rad, rad );
}

/**********************************************************************
 * ThinSeekBar
 **********************************************************************/
ThinSeekBar::ThinSeekBar( intf_thread_t *_p_i, QWidget *parent )
    : QSlider( Qt::Horizontal, parent ), p_intf( _p_i )
{
    enum { RANGE = 1000 };
    setRange( 0, RANGE );
    setSingleStep( RANGE / 100 );
    setPageStep( RANGE / 10 );
    /* Tab only: a click must not take the keys from the player */
    setFocusPolicy( Qt::TabFocus );
    setAccessibleName( qtr( "Seek" ) );
    setToolTip( qtr( "Seek" ) );
    /* the mini player's is made mid-track: start from the current input */
    input_thread_t *input = THEMIM->getInput();
    setEnabled( input != NULL && var_GetBool( input, "can-seek" ) );
    setCursor( Qt::PointingHandCursor );

    connect( THEMIM->getIM(), &InputManager::positionUpdated, this,
             [this]( float pos, int64_t, int ) {
                 if( !isSliderDown() )
                     setValue( pos < 0.f ? 0 : qRound( pos * maximum() ) );
             } );
    connect( THEMIM->getIM(), &InputManager::inputCanSeek, this, &QWidget::setEnabled );
    /* keys and wheel: sliderPosition holds the new value already */
    connect( this, &QAbstractSlider::actionTriggered, this, [this]( int action ) {
        if( action != SliderMove )
            seek();
    } );
    connect( this, &QAbstractSlider::sliderMoved, this, &ThinSeekBar::seek );
}

QSize ThinSeekBar::sizeHint() const
{
    return QSize( Theme::instance()->px( NowPlaying::CentreMinW ),
                  Theme::instance()->px( NowPlaying::ThinSeekHit ) );
}

QSize ThinSeekBar::minimumSizeHint() const
{
    return QSize( 0, sizeHint().height() );
}

int ThinSeekBar::valueAt( int x ) const
{
    return QStyle::sliderValueFromPosition( minimum(), maximum(), x, width(),
                                            layoutDirection() == Qt::RightToLeft );
}

void ThinSeekBar::seek()
{
    THEMIM->getIM()->sliderUpdate( float( sliderPosition() ) / maximum() );
}

void ThinSeekBar::mousePressEvent( QMouseEvent *e )
{
    if( e->button() != Qt::LeftButton )
        return QSlider::mousePressEvent( e );
    setSliderDown( true );
    setSliderPosition( valueAt( e->pos().x() ) );   /* emits sliderMoved: seeks */
    e->accept();
}

void ThinSeekBar::mouseMoveEvent( QMouseEvent *e )
{
    if( isSliderDown() )
        setSliderPosition( valueAt( e->pos().x() ) );
    e->accept();
}

void ThinSeekBar::mouseReleaseEvent( QMouseEvent *e )
{
    setSliderDown( false );
    e->accept();
}

void ThinSeekBar::paintEvent( QPaintEvent * )
{
    const Theme *t = Theme::instance();
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );
    const qreal h = t->px( NowPlaying::ThinSeek );
    const QRectF track( 0, ( height() - h ) / 2, width(), h );
    p.setPen( Qt::NoPen );
    p.setBrush( t->ink( InkAlpha::Track ) );
    p.drawRoundedRect( track, h / 2, h / 2 );
    if( isEnabled() && maximum() > 0 )
    {
        QRectF done = track;
        done.setWidth( track.width() * sliderPosition() / maximum() );
        if( layoutDirection() == Qt::RightToLeft )
            done.moveRight( track.right() );
        p.setBrush( t->color( Theme::Accent ) );
        p.drawRoundedRect( done, h / 2, h / 2 );
    }
    if( hasFocus() )
    {
        const qreal inset = Size::FocusRing / 2.;
        p.setPen( QPen( t->color( t->isDark() ? Theme::Accent : Theme::AccentText ),
                        Size::FocusRing ) );
        p.setBrush( Qt::NoBrush );
        p.drawRoundedRect( QRectF( rect() ).adjusted( inset, inset, -inset, -inset ),
                           t->metric( Theme::RadiusXs ), t->metric( Theme::RadiusXs ) );
    }
}

/**********************************************************************
 * NowPlayingInfo
 **********************************************************************/
NowPlayingInfo::NowPlayingInfo( intf_thread_t *_p_i, QObject *parent )
    : QObject( parent ), p_intf( _p_i )
{
    InputManager *im = THEMIM->getIM();
    /* a stop sends NULL: the views keep the last item */
    connect( im, QOverload<QString>::of( &InputManager::artChanged ), this,
             [this]( const QString &url ) { art = url; emit changed(); } );
    connect( im, &InputManager::currentMetaChanged, this, &NowPlayingInfo::readItem );
    connect( im, &InputManager::infoChanged, this, &NowPlayingInfo::readItem );
    connect( THEMIM, &MainInputManager::inputChanged, this, [this]( bool has ) {
        if( has && THEMIM->getInput() )
            readItem( input_GetItem( THEMIM->getInput() ) );
    } );
}

void NowPlayingInfo::readItem( input_item_t *p_item )
{
    if( p_item == NULL )
        return;
    char *psz;
    psz = input_item_GetTitleFbName( p_item ); title = qfu( psz ).trimmed(); free( psz );
    psz = input_item_GetArtist( p_item ); artist = qfu( psz ).trimmed(); free( psz );
    psz = input_item_GetAlbum( p_item );  album = qfu( psz ).trimmed();  free( psz );
    psz = input_item_GetDate( p_item );   year = qfu( psz ).left( 4 );   free( psz );

    /* "FLAC · 24/96" from the first audio stream's info (es_out.c keys) */
    QString codec, rate, bits;
    vlc_mutex_lock( &p_item->lock );
    for( int i = 0; i < p_item->i_categories && codec.isEmpty(); i++ )
    {
        const info_category_t *cat = p_item->pp_categories[i];
        QString type, c, r, b;
        for( int j = 0; j < cat->i_infos; j++ )
        {
            const QString name = qfu( cat->pp_infos[j]->psz_name );
            const QString value = qfu( cat->pp_infos[j]->psz_value );
            if( name == qtr( "Type" ) )
                type = value;
            else if( name == qtr( "Codec" ) )
                c = value.contains( '(' ) ? value.section( '(', -1 ).section( ')', 0, 0 ) : value;
            else if( name == qtr( "Sample rate" ) )
                r = value.section( ' ', 0, 0 );
            else if( name == qtr( "Bits per sample" ) )
                b = value;
        }
        if( type == qtr( "Audio" ) )
        {
            codec = c; rate = r; bits = b;
        }
    }
    vlc_mutex_unlock( &p_item->lock );

    format.clear();
    if( !codec.isEmpty() )
    {
        format = codecName( codec );
        const double khz = rate.toDouble() / 1000.;
        const QString k = QString::number( khz, 'g', 3 );
        if( khz > 0 && bits.toInt() > 0 )
            format += qtr( " · %1/%2" ).arg( bits, k );
        else if( khz > 0 )
            format += qtr( " · %1 kHz" ).arg( k );
    }
    emit changed();
}

QString NowPlayingInfo::metaLine() const
{
    QStringList parts;
    if( !artist.isEmpty() )
        parts << artist;
    if( !album.isEmpty() )
        parts << album;
    return parts.isEmpty() ? format : parts.join( qtr( " · " ) );
}

/**********************************************************************
 * Buttons
 **********************************************************************/
QToolButton *makeIconButton( intf_thread_t *p_intf, const char *icon, const QString &name,
                             const char *hotkey, int size, QWidget *parent )
{
    const Theme *t = Theme::instance();
    QToolButton *b = new QToolButton( parent );
    b->setAutoRaise( true );
    b->setIcon( Theme::themedIcon( icon, Theme::Text2 ) );
    b->setIconSize( QSize( t->metric( Theme::IconGlyph ), t->metric( Theme::IconGlyph ) ) );
    b->setFixedSize( t->px( size ), t->px( size ) );
    b->setAccessibleName( name );
    b->setToolTip( withKey( p_intf, name, hotkey ) );
    b->setFocusPolicy( Qt::TabFocus );
    return b;
}

/* The accent circle; its icon and name follow the playing state */
QToolButton *makePlayButton( intf_thread_t *p_intf, int size, QWidget *parent )
{
    QToolButton *b = makeIconButton( p_intf, RedesignIcons::IconPlay, qtr( "Play" ),
                                     "key-play-pause", size, parent );
    b->setProperty( "np", "play" );
    QObject::connect( b, &QToolButton::clicked, THEMIM, &MainInputManager::togglePlayPause );
    auto sync = [b, p_intf]( int state ) {
        const bool playing = state == PLAYING_S;
        b->setIcon( Theme::themedIcon( playing ? RedesignIcons::IconPause
                                               : RedesignIcons::IconPlay, Theme::OnAccent ) );
        const QString name = playing ? qtr( "Pause" ) : qtr( "Play" );
        b->setAccessibleName( name );
        b->setToolTip( withKey( p_intf, name, "key-play-pause" ) );
    };
    QObject::connect( THEMIM->getIM(), &InputManager::playingStatusChanged, b, sync );
    sync( THEMIM->getIM()->playingStatus() );
    return b;
}

static QWidget *makeTransport( intf_thread_t *p_intf, int gap, QWidget *parent )
{
    QWidget *w = new QWidget( parent );
    QHBoxLayout *l = new QHBoxLayout( w );
    l->setContentsMargins( 0, 0, 0, 0 );
    l->setSpacing( Theme::instance()->px( gap ) );
    QToolButton *prev = makeIconButton( p_intf, RedesignIcons::IconPrevious, qtr( "Previous" ),
                                        "key-prev", NowPlaying::SmallButton, w );
    QToolButton *next = makeIconButton( p_intf, RedesignIcons::IconNext, qtr( "Next" ),
                                        "key-next", NowPlaying::SmallButton, w );
    QObject::connect( prev, &QToolButton::clicked, THEMIM, &MainInputManager::prev );
    QObject::connect( next, &QToolButton::clicked, THEMIM, &MainInputManager::next );
    l->addWidget( prev );
    l->addWidget( makePlayButton( p_intf, Size::PlayNowBar, w ) );
    l->addWidget( next );
    return w;
}

/**********************************************************************
 * NowPlayingBar
 **********************************************************************/
NowPlayingBar::NowPlayingBar( intf_thread_t *_p_i, NowPlayingInfo *_info, QWidget *parent )
    : QWidget( parent ), p_intf( _p_i ), info( _info )
{
    const Theme *t = Theme::instance();
    setProperty( "np", "bar" );
    setFixedHeight( t->metric( Theme::NowPlayingBarH ) );

    QHBoxLayout *l = new QHBoxLayout( this );
    l->setContentsMargins( t->px( NowPlaying::PadX ), 0, t->px( NowPlaying::PadX ), 0 );
    l->setSpacing( t->px( NowPlaying::ColumnGap ) );

    /* left: thumb, title and meta; a click opens the player */
    left = new QWidget( this );
    left->setCursor( Qt::PointingHandCursor );
    left->setToolTip( qtr( "Open player" ) );
    left->installEventFilter( this );
    QHBoxLayout *ll = new QHBoxLayout( left );
    ll->setContentsMargins( 0, 0, 0, 0 );
    ll->setSpacing( t->px( NowPlaying::InfoGap ) );
    thumb = new ArtView( NowPlaying::ThumbStripe, Size::RadiusMd, left );
    thumb->setFixedSize( t->px( NowPlaying::ThumbW ), t->px( NowPlaying::ThumbH ) );
    ll->addWidget( thumb );
    QVBoxLayout *text = new QVBoxLayout;
    text->setSpacing( t->px( NowPlaying::TextGap ) );
    title = new QElidingLabel( QString(), Qt::ElideRight, left );
    title->setFont( fontAt( Theme::Body, Type::Body, true ) );
    meta = new QElidingLabel( QString(), Qt::ElideRight, left );
    meta->setFont( t->font( Theme::Meta ) );
    meta->setProperty( "np", "mute" );
    text->addStretch();
    text->addWidget( title );
    text->addWidget( meta );
    text->addStretch();
    ll->addLayout( text, 1 );

    /* centre: transport above the seek line, times on its sides */
    QWidget *centre = new QWidget( this );
    centre->setMinimumWidth( t->px( NowPlaying::CentreMinW ) );
    centre->setMaximumWidth( t->px( NowPlaying::CentreMaxW ) );
    QVBoxLayout *cl = new QVBoxLayout( centre );
    cl->setContentsMargins( 0, 0, 0, 0 );
    cl->setSpacing( t->px( NowPlaying::RowGap ) );
    cl->addStretch();
    cl->addWidget( makeTransport( p_intf, NowPlaying::TransportGap, centre ), 0, Qt::AlignHCenter );
    QHBoxLayout *seekRow = new QHBoxLayout;
    seekRow->setSpacing( t->px( NowPlaying::TimeGap ) );
    elapsed = new QLabel( centre );
    length = new QLabel( centre );
    foreach( QLabel *l, QList<QLabel *>() << elapsed << length )
    {
        l->setFont( fontAt( Theme::Overline, Type::Overline, false ) );
        l->setProperty( "np", "mute" );
        l->setFixedWidth( t->px( NowPlaying::TimeW ) );
    }
    elapsed->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
    seekRow->addWidget( elapsed );
    seekRow->addWidget( new ThinSeekBar( p_intf, centre ), 1 );
    seekRow->addWidget( length );
    cl->addLayout( seekRow );
    cl->addStretch();
    connect( THEMIM->getIM(), &InputManager::positionUpdated, this,
             [this]( float pos, int64_t time, int len ) {
                 const bool known = pos >= 0.f && len > 0;
                 elapsed->setText( pos < 0.f ? QString() : timeString( time / CLOCK_FREQ ) );
                 length->setText( known ? timeString( len ) : QString() );
             } );

    /* right: volume and expand */
    QWidget *right = new QWidget( this );
    QHBoxLayout *rl = new QHBoxLayout( right );
    rl->setContentsMargins( 0, 0, 0, 0 );
    rl->addStretch();
    rl->addWidget( new SoundWidget( right, p_intf, true ) );
    QToolButton *expand = makeIconButton( p_intf, RedesignIcons::IconExpand, qtr( "Open player" ),
                                          NULL, NowPlaying::SmallButton, right );
    connect( expand, &QToolButton::clicked, this, &NowPlayingBar::expandRequested );
    rl->addWidget( expand );

    /* the sides share what the centre leaves, so it stays centred */
    left->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
    right->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
    l->addWidget( left, 1 );
    l->addWidget( centre, 1 );
    l->addWidget( right, 1 );

    connect( info, &NowPlayingInfo::changed, this, &NowPlayingBar::refresh );
    refresh();
}

void NowPlayingBar::refresh()
{
    title->setText( info->title );
    meta->setText( info->metaLine() );
    thumb->setArt( info->art );
    setAccessibleName( qtr( "Now playing: %1" ).arg( info->title ) );
}

bool NowPlayingBar::eventFilter( QObject *o, QEvent *e )
{
    if( o == left && e->type() == QEvent::MouseButtonRelease
     && static_cast<QMouseEvent *>( e )->button() == Qt::LeftButton )
        emit expandRequested();
    return QWidget::eventFilter( o, e );
}

void NowPlayingBar::paintEvent( QPaintEvent * )
{
    const Theme *t = Theme::instance();
    QPainter p( this );
    p.fillRect( rect(), t->color( Theme::Chrome ) );
    p.fillRect( QRect( 0, 0, width(), Size::Hairline ), t->ink( InkAlpha::Divider ) );
}

/**********************************************************************
 * AudioModePage
 **********************************************************************/
namespace {
/* 40 px rows: number (or the playing glyph), title, duration */
class TrackDelegate : public QStyledItemDelegate
{
public:
    TrackDelegate( intf_thread_t *_p_i, QObject *parent )
        : QStyledItemDelegate( parent ), p_intf( _p_i ) {}

    QSize sizeHint( const QStyleOptionViewItem &, const QModelIndex & ) const Q_DECL_OVERRIDE
    {
        return QSize( 0, Theme::instance()->metric( Theme::PlaylistRowH ) );
    }

    void paint( QPainter *p, const QStyleOptionViewItem &opt,
                const QModelIndex &index ) const Q_DECL_OVERRIDE
    {
        Theme *t = Theme::instance();
        const bool current = index.data( VLCModel::CURRENT_ITEM_ROLE ).toBool();
        const QRect r = opt.rect;
        const int radius = t->metric( Theme::RadiusMd );
        p->save();
        p->setRenderHint( QPainter::Antialiasing );
        p->setPen( Qt::NoPen );
        if( current )
            p->setBrush( t->color( Theme::AccentTint12 ) );
        else if( opt.state & QStyle::State_MouseOver )
            p->setBrush( t->ink( InkAlpha::Hairline ) );
        else
            p->setBrush( Qt::NoBrush );
        p->drawRoundedRect( r, radius, radius );
        if( opt.state & QStyle::State_HasFocus )
        {
            const qreal inset = Size::FocusRing / 2.;
            p->setPen( QPen( t->color( t->isDark() ? Theme::Accent : Theme::AccentText ),
                             Size::FocusRing ) );
            p->setBrush( Qt::NoBrush );
            p->drawRoundedRect( QRectF( r ).adjusted( inset, inset, -inset, -inset ),
                                radius, radius );
        }

        const int padX = t->px( NowPlaying::TrackPadX ), gap = t->px( NowPlaying::TrackGap );
        const QRect num( r.x() + padX, r.y(), t->px( NowPlaying::TrackNumW ), r.height() );
        if( current )
        {
            const int icon = t->px( NowPlaying::TrackIcon );
            const bool playing = THEMIM->getIM()->playingStatus() == PLAYING_S;
            Theme::themedIcon( playing ? RedesignIcons::IconNowPlaying : RedesignIcons::IconPause,
                               Theme::Accent )
                .paint( p, QRect( num.center().x() - icon / 2, num.center().y() - icon / 2,
                                  icon, icon ) );
        }
        else
        {
            p->setFont( fontAt( Theme::Meta, Type::Meta, true ) );
            p->setPen( t->color( Theme::Dim ) );
            p->drawText( num, Qt::AlignCenter, QString::number( index.row() + 1 ) );
        }

        const QString duration = VLCModel::getMeta( index, COLUMN_DURATION );
        p->setFont( t->font( Theme::Body ) );
        const int durW = p->fontMetrics().width( duration );
        const QRect durRect( r.right() - padX - durW, r.y(), durW, r.height() );
        p->setPen( t->color( Theme::Mute ) );
        p->drawText( durRect, Qt::AlignVCenter | Qt::AlignRight, duration );

        const QRect titleRect( num.right() + 1 + gap, r.y(),
                               durRect.left() - gap - num.right() - 1 - gap, r.height() );
        p->setFont( fontAt( Theme::Body, NowPlaying::TrackTitle, false ) );
        p->setPen( t->color( current ? Theme::AccentText : Theme::Text ) );
        p->drawText( titleRect, Qt::AlignVCenter | Qt::AlignLeft,
                     p->fontMetrics().elidedText( index.data( Qt::DisplayRole ).toString(),
                                                  Qt::ElideRight, titleRect.width() ) );
        p->restore();
    }

private:
    intf_thread_t *p_intf;
};
}

AudioModePage::AudioModePage( intf_thread_t *_p_i, NowPlayingInfo *_info, QWidget *parent )
    : QWidget( parent ), p_intf( _p_i ), info( _info ),
      breakpoint( BreakpointWatcher::L ), artMax( 0 )
{
    const Theme *t = Theme::instance();
    QVBoxLayout *outer = new QVBoxLayout( this );
    outer->setContentsMargins( 0, 0, 0, 0 );
    outer->setSpacing( 0 );
    columns = new QHBoxLayout;
    outer->addLayout( columns, 1 );
    spectrum = new SpectrumWidget( p_intf, this );
    outer->addWidget( spectrum );

    /* left: back to the library, art, chips */
    leftColumn = new QWidget( this );
    QVBoxLayout *ll = new QVBoxLayout( leftColumn );
    ll->setContentsMargins( 0, 0, 0, 0 );
    ll->setSpacing( t->px( NowPlaying::ColumnInnerGap ) );
    QToolButton *back = new QToolButton( leftColumn );
    back->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
    back->setIcon( Theme::themedIcon( RedesignIcons::IconBack, Theme::Text2 ) );
    back->setIconSize( QSize( t->px( NowPlaying::TrackIcon ), t->px( NowPlaying::TrackIcon ) ) );
    back->setText( qtr( "Library" ) );
    back->setAccessibleName( qtr( "Go to library" ) );
    back->setToolTip( qtr( "Go to library" ) );
    back->setFixedHeight( t->px( NowPlaying::PillH ) );
    back->setProperty( "np", "pill" );
    back->setFocusPolicy( Qt::TabFocus );
    connect( back, &QToolButton::clicked, this, &AudioModePage::libraryRequested );
    ll->addWidget( back, 0, Qt::AlignLeft );
    art = new ArtView( NowPlaying::ArtStripe, NowPlaying::ArtRadius, leftColumn );
    art->setAccessibleName( qtr( "Album art" ) );
    ll->addWidget( art );
    QHBoxLayout *chips = new QHBoxLayout;
    chips->setSpacing( t->px( NowPlaying::ChipGap ) );
    formatChip = new QLabel( leftColumn );
    formatChip->setProperty( "np", "chip" );
    yearChip = new QLabel( leftColumn );
    yearChip->setProperty( "np", "chip2" );
    foreach( QLabel *chip, QList<QLabel *>() << formatChip << yearChip )
    {
        chip->setFont( fontAt( Theme::Overline, Type::Overline, chip == formatChip ) );
        chips->addWidget( chip );
    }
    chips->addStretch();
    ll->addLayout( chips );
    ll->addStretch();
    columns->addWidget( leftColumn );

    /* right: header and track list */
    QVBoxLayout *rl = new QVBoxLayout;
    rl->setSpacing( 0 );
    rl->addSpacing( t->px( NowPlaying::HeaderTop ) );
    QLabel *overline = new QLabel( qtr( "Now playing" ).toUpper(), this );
    overline->setFont( t->font( Theme::Overline ) );
    overline->setProperty( "np", "accent" );
    rl->addWidget( overline );
    rl->addSpacing( t->px( NowPlaying::TitleTop ) );
    title = new QElidingLabel( QString(), Qt::ElideRight, this );
    rl->addWidget( title );
    rl->addSpacing( t->px( NowPlaying::MetaTop ) );
    meta = new QElidingLabel( QString(), Qt::ElideRight, this );
    meta->setFont( fontAt( Theme::PanelTitle, Type::PanelTitle, false ) );
    meta->setProperty( "np", "mute" );
    rl->addWidget( meta );
    rl->addSpacing( t->px( NowPlaying::ListTop ) );

    tracks = new QListView( this );
    tracks->setProperty( "np", "tracks" );
    tracks->setAccessibleName( qtr( "Tracks" ) );
    tracks->setModel( PLModel::getPLModel( p_intf ) );
    tracks->setItemDelegate( new TrackDelegate( p_intf, tracks ) );
    tracks->setUniformItemSizes( true );        /* 10,000 rows: no per-row size query */
    tracks->setEditTriggers( QAbstractItemView::NoEditTriggers );
    tracks->setSelectionMode( QAbstractItemView::NoSelection );
    tracks->setMouseTracking( true );
    tracks->setFrameShape( QFrame::NoFrame );
    connect( tracks, &QListView::activated, this, [this]( const QModelIndex &index ) {
        PLModel::getPLModel( p_intf )->activateItem( index );
    } );
    connect( PLModel::getPLModel( p_intf ), &VLCModel::currentIndexChanged, tracks,
             [this]( const QModelIndex &index ) { tracks->scrollTo( index ); } );
    connect( THEMIM->getIM(), &InputManager::playingStatusChanged, tracks->viewport(),
             QOverload<>::of( &QWidget::update ) );
    rl->addWidget( tracks, 1 );
    columns->addLayout( rl, 1 );

    connect( info, &NowPlayingInfo::changed, this, &AudioModePage::refresh );
    setBreakpoint( breakpoint );
    refresh();
}

void AudioModePage::setBreakpoint( BreakpointWatcher::Size s )
{
    using namespace NowPlaying;
    const Theme *t = Theme::instance();
    breakpoint = s;
    const int artPx = s == BreakpointWatcher::L ? ArtL : s == BreakpointWatcher::M ? ArtM
                    : s == BreakpointWatcher::S ? ArtS : ArtXS;
    const int titlePx = s == BreakpointWatcher::L ? TitleL : s == BreakpointWatcher::M ? TitleM
                      : s == BreakpointWatcher::S ? TitleS : TitleXS;
    const int gap = s == BreakpointWatcher::L ? GapL : s == BreakpointWatcher::M ? GapM
                  : s == BreakpointWatcher::S ? GapS : GapXS;
    const int padTop = s == BreakpointWatcher::L ? PadTopL : s == BreakpointWatcher::XS ? PadXS : PadTopM;
    const int padX = s == BreakpointWatcher::L ? PadXL : s == BreakpointWatcher::XS ? PadXS : PadXM;
    artMax = t->px( artPx );
    fitArt();
    title->setFont( fontAt( Theme::Display, titlePx, true ) );
    columns->setSpacing( t->px( gap ) );
    columns->setContentsMargins( t->px( padX ), t->px( padTop ), t->px( padX ), 0 );
}

/* The art is square, at most artMax, and shrinks to what the page's height
 * leaves under the pill and above the chips and the spectrum */
void AudioModePage::fitArt()
{
    using namespace NowPlaying;
    const Theme *t = Theme::instance();
    /* short pages: the track list keeps its rows, the spectrum goes first */
    const int listMin = TrackMinRows * t->metric( Theme::PlaylistRowH );
    spectrum->setVisible( height() - tracks->y() - t->px( SpectrumH ) >= listMin );
    const int top = columns->contentsMargins().top();
    const int room = height() - top - ( spectrum->isHidden() ? 0 : t->px( SpectrumH ) ) - t->px( PillH )
                   - formatChip->sizeHint().height() - 2 * t->px( ColumnInnerGap );
    const int side = qMax( 0, qMin( artMax, room ) );
    art->setFixedSize( side, side );
    /* never narrower than the chips under the art */
    int chips = formatChip->isHidden() ? 0 : formatChip->sizeHint().width();
    if( !yearChip->isHidden() )
        chips += yearChip->sizeHint().width() + t->px( ChipGap );
    leftColumn->setFixedWidth( qMax( side, chips ) );
}

void AudioModePage::resizeEvent( QResizeEvent *e )
{
    QWidget::resizeEvent( e );
    fitArt();
}

void AudioModePage::refresh()
{
    title->setText( info->title );
    QStringList parts;
    if( !info->artist.isEmpty() )
        parts << info->artist;
    if( !info->album.isEmpty() )
        parts << info->album;
    meta->setText( parts.join( qtr( " · " ) ) );
    meta->setVisible( !parts.isEmpty() );
    formatChip->setText( info->format );
    formatChip->setVisible( !info->format.isEmpty() );
    yearChip->setText( info->year );
    yearChip->setVisible( !info->year.isEmpty() );
    art->setArt( info->art );
    fitArt();
}

/**********************************************************************
 * MiniPlayer
 **********************************************************************/
MiniPlayer::MiniPlayer( intf_thread_t *_p_i, NowPlayingInfo *_info, QWidget *parent )
    : QWidget( parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint ),
      p_intf( _p_i ), info( _info )
{
    using namespace NowPlaying;
    const Theme *t = Theme::instance();
    setObjectName( "miniPlayer" );
    setWindowTitle( qtr( "Mini player" ) );
    setWindowIcon( QApplication::windowIcon() );
    setFixedSize( t->px( MiniW ), t->px( MiniH ) );

    QVBoxLayout *l = new QVBoxLayout( this );
    l->setContentsMargins( Size::Hairline, Size::Hairline, Size::Hairline, Size::Hairline );
    l->setSpacing( 0 );

    art = new ArtView( ThumbStripe, 0, this );
    QGridLayout *overlay = new QGridLayout( art );
    const int inset = t->px( MiniRestoreInset );
    overlay->setContentsMargins( inset, inset, inset, inset );
    QToolButton *restore = makeIconButton( p_intf, RedesignIcons::IconExpand, qtr( "Restore" ),
                                           NULL, MiniRestore, art );
    /* white on the dark scrim, in both themes */
    restore->setIcon( Theme::themedIcon( RedesignIcons::IconExpand, Theme::WindowCloseGlyph ) );
    restore->setToolTip( qtr( "Restore (Esc)" ) );
    restore->setProperty( "np", "overlay" );
    connect( restore, &QToolButton::clicked, this, &MiniPlayer::restoreRequested );
    overlay->addWidget( restore, 0, 0, Qt::AlignTop | Qt::AlignRight );
    l->addWidget( art, 1 );

    l->addWidget( new ThinSeekBar( p_intf, this ) );

    QHBoxLayout *row = new QHBoxLayout;
    row->setContentsMargins( t->px( MiniPadX ), t->px( MiniPadY ), t->px( MiniPadX ), t->px( MiniPadY ) );
    row->setSpacing( t->px( MiniGap ) );
    QVBoxLayout *text = new QVBoxLayout;
    text->setSpacing( 0 );
    title = new QElidingLabel( QString(), Qt::ElideRight, this );
    title->setFont( fontAt( Theme::Body, Type::Body, true ) );
    time = new QLabel( this );
    time->setFont( fontAt( Theme::Overline, Type::Overline, false ) );
    time->setProperty( "np", "mute" );
    text->addWidget( title );
    text->addWidget( time );
    row->addLayout( text, 1 );
    row->addWidget( makeTransport( p_intf, MiniGap, this ) );
    l->addLayout( row );

    connect( THEMIM->getIM(), &InputManager::positionUpdated, this,
             [this]( float pos, int64_t us, int len ) {
                 time->setText( pos < 0.f ? QString()
                              : len > 0 ? qtr( "%1 / %2" ).arg( timeString( us / CLOCK_FREQ ),
                                                                timeString( len ) )
                              : timeString( us / CLOCK_FREQ ) );
             } );
    connect( info, &NowPlayingInfo::changed, this, &MiniPlayer::refresh );
    refresh();

    fade = new QPropertyAnimation( this, "windowOpacity", this );
    fade->setStartValue( 0.0 );
    fade->setEndValue( 1.0 );
    fade->setEasingCurve( QEasingCurve::OutCubic );

    /* where it was last time, or the bottom-right corner of the work area */
    if( !restoreGeometry( getSettings()->value( "MiniPlayer/geometry" ).toByteArray() ) )
    {
        const QRect work = QApplication::desktop()->availableGeometry( parent ? parent : this );
        const int margin = t->px( MiniMargin );
        move( work.right() - width() - margin, work.bottom() - height() - margin );
    }
}

void MiniPlayer::popIn()
{
    const int ms = Theme::instance()->duration( Theme::MiniPlayer );
    setWindowOpacity( ms > 0 ? 0.0 : 1.0 );
    show();
    raise();
    activateWindow();
    if( ms > 0 )
    {
        fade->setDuration( ms );
        fade->start();
    }
}

void MiniPlayer::refresh()
{
    title->setText( info->title );
    art->setArt( info->art );
    setWindowTitle( info->title.isEmpty() ? qtr( "Mini player" )
                                          : qtr( "%1 - Mini player" ).arg( info->title ) );
}

void MiniPlayer::keyPressEvent( QKeyEvent *e )
{
    if( e->key() == Qt::Key_Escape && e->modifiers() == Qt::NoModifier )
    {
        emit restoreRequested();
        return;
    }
    emit keyPressed( e );   /* the player's hotkeys */
}

void MiniPlayer::mousePressEvent( QMouseEvent *e )
{
    /* frameless: drag it by any spot that is not a control */
    if( e->button() == Qt::LeftButton )
        dragOffset = e->globalPos() - frameGeometry().topLeft();
}

void MiniPlayer::mouseMoveEvent( QMouseEvent *e )
{
    if( e->buttons() & Qt::LeftButton )
        move( e->globalPos() - dragOffset );
}

void MiniPlayer::paintEvent( QPaintEvent * )
{
    const Theme *t = Theme::instance();
    QPainter p( this );
    p.fillRect( rect(), t->color( Theme::Surface ) );
    p.setPen( QPen( t->ink( InkAlpha::OpenFill ), Size::Hairline ) );
    p.drawRect( QRectF( rect() ).adjusted( 0.5, 0.5, -0.5, -0.5 ) );
}

void MiniPlayer::showEvent( QShowEvent *e )
{
#ifdef _WIN32
    /* Windows 11 rounds it like the design; older versions keep it square */
    WinTheme::setRoundCorners( reinterpret_cast<HWND>( winId() ) );
#endif
    QWidget::showEvent( e );
}

void MiniPlayer::hideEvent( QHideEvent *e )
{
    getSettings()->setValue( "MiniPlayer/geometry", saveGeometry() );
    QWidget::hideEvent( e );
}

void MiniPlayer::closeEvent( QCloseEvent *e )
{
    /* Alt+F4 goes back to the main window, it does not quit */
    e->ignore();
    emit restoreRequested();
}
