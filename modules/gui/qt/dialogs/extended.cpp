/*****************************************************************************
 * extended.cpp : Extended controls - Undocked
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#include <cassert>

#include "dialogs/extended.hpp"

#include "main_interface.hpp" /* the dock and the breakpoints */
#include "input_manager.hpp"
#include "dialogs_provider.hpp"
#include "actions_manager.hpp"   /* EXTENDED_ACTION, PLAYLIST_ACTION */
#include "components/overlay_host.hpp"
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include <QApplication>
#include <QBoxLayout>
#include <QGridLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVariantAnimation>
#include <vlc_modules.h>

using namespace RedesignTokens;

namespace
{

/* A page that scrolls vertically, with the sheet's body padding */
QScrollArea *scrolled( QWidget *page )
{
    const int pad = Theme::instance()->px( Effects::BodyPad );
    QWidget *inner = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout( inner );
    layout->setContentsMargins( pad, pad, pad, pad );
    layout->addWidget( page );
    QScrollArea *area = new QScrollArea;
    area->setWidgetResizable( true );
    area->setFrameShape( QFrame::NoFrame );
    area->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    area->setWidget( inner );
    return area;
}

/* Sub-pages are reached from "More": no tab bar, and only the page shown
 * counts for the size, or the Essential page would scroll as long as the
 * Colors one */
void asPages( QTabWidget *pages )
{
    QTabBar *bar = pages->findChild<QTabBar *>( QString(), Qt::FindDirectChildrenOnly );
    if( bar )
        bar->hide();
    const auto fit = [pages]( int current ) {
        for( int i = 0; i < pages->count(); i++ )
        {
            const QSizePolicy::Policy p = i == current ? QSizePolicy::Preferred
                                                       : QSizePolicy::Ignored;
            pages->widget( i )->setSizePolicy( p, p );
        }
    };
    QObject::connect( pages, &QTabWidget::currentChanged, pages, fit );
    fit( pages->currentIndex() );
}

}

ExtendedDialog::ExtendedDialog( intf_thread_t *_p_intf )
               : QWidget( NULL ), p_intf( _p_intf ), dock( NULL ), dockParent( NULL ),
                 host( NULL ), slide( NULL ), fullWidth( 0 ), shownWidth( 0 ), drawer( false )
{
    MainInterface *p_mi = p_intf->p_sys->p_mi;
    QWidget *over = p_mi ? p_mi->getCentralStack() : NULL;
    if( over )
    {
        dockParent = over->parentWidget();
        dock = qobject_cast<QBoxLayout *>( dockParent->layout() );
        assert( dock );
        setParent( dockParent );
    }
    setObjectName( "effectsSheet" );
    setAccessibleName( qtr( "Effects and filters" ) );
    setWindowTitle( qtr( "Adjustments and Effects" ) );
    setWindowRole( "vlc-extended" );
    hide();

    Theme *t = Theme::instance();
    content = new QWidget( this );
    QVBoxLayout *layout = new QVBoxLayout( content );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( t->px( Effects::TabsPadTop ) );

    /* Header: title, close */
    QHBoxLayout *head = new QHBoxLayout;
    head->setContentsMargins( t->px( Effects::HeadPadLeft ), t->px( Effects::HeadPadTop ),
                              t->px( Effects::HeadPadRight ), 0 );
    QLabel *title = new QLabel( qtr( "Effects and filters" ), content );
    title->setFont( t->font( Theme::PanelTitle ) );
    head->addWidget( title, 1 );
    QToolButton *close = new QToolButton( content );
    const int glyphPx = t->px( Effects::CloseGlyph );
    close->setIcon( Theme::themedIcon( RedesignIcons::IconClose, Theme::Text2 ) );
    close->setIconSize( QSize( glyphPx, glyphPx ) );
    close->setFixedSize( t->px( Effects::CloseButton ), t->px( Effects::CloseButton ) );
    close->setAccessibleName( qtr( "Close effects" ) );
    /* shortcut: Ctrl+E is copied from menus.cpp (Tools > Effects and
     * Filters), read it from that action once the menus expose it */
    close->setToolTip( QString( "%1 (%2)" ).arg( qtr( "Close effects" ),
        QKeySequence( QLatin1String( "Ctrl+E" ) ).toString( QKeySequence::NativeText ) ) );
    connect( close, &QToolButton::clicked, this, &QWidget::hide );
    head->addWidget( close );
    layout->addLayout( head );

    mainTabW = new QTabWidget( content );
    mainTabW->setObjectName( "effectsTabs" );
    mainTabW->setDocumentMode( true );

    /* AUDIO effects */
    audioTab = new QTabWidget;

    equal = new Equalizer( p_intf, audioTab );
    connect( equal, &Equalizer::configChanged, this, &ExtendedDialog::putAudioConfig );
    audioTab->addTab( equal, qtr( "Equalizer" ) );

    Compressor *compres = new Compressor( p_intf, audioTab );
    connect( compres, &Compressor::configChanged, this, &ExtendedDialog::putAudioConfig );
    audioTab->addTab( compres, qtr( "Compressor" ) );

    Spatializer *spatial = new Spatializer( p_intf, audioTab );
    connect( spatial, &Spatializer::configChanged, this, &ExtendedDialog::putAudioConfig );
    audioTab->addTab( spatial, qtr( "Spatializer" ) );

    StereoWidener *stereowiden = new StereoWidener( p_intf, audioTab );
    connect( stereowiden, &StereoWidener::configChanged, this, &ExtendedDialog::putAudioConfig );
    audioTab->addTab( stereowiden, qtr( "Stereo Widener" ) );

    QWidget *advancedTab = new QWidget;
    QGridLayout *advancedTabLayout = new QGridLayout;

    PitchShifter *pitchshifter = new PitchShifter( p_intf, audioTab );
    connect( pitchshifter, &PitchShifter::configChanged, this, &ExtendedDialog::putAudioConfig );

    advancedTabLayout->setColumnStretch( 1, 10 );
    advancedTabLayout->addWidget( pitchshifter );

    advancedTab->setLayout( advancedTabLayout );
    audioTab->addTab( advancedTab, qtr( "Advanced" ) );
    asPages( audioTab );

    mainTabW->insertTab( AUDIO_TAB, scrolled( audioTab ), qtr( "Equalizer" ) );

    /* Video Effects */
    videoTab = new QTabWidget;
    videoEffect = new ExtVideo( p_intf, videoTab );
    connect( videoEffect, &ExtVideo::configChanged, this, &ExtendedDialog::putVideoConfig );
    asPages( videoTab );

    mainTabW->insertTab( VIDEO_TAB, scrolled( videoTab ), qtr( "Video" ) );

    syncW = new SyncControls( p_intf, NULL );
    mainTabW->insertTab( SYNCHRO_TAB, scrolled( syncW ), qtr( "Synchronization" ) );

    if( module_exists( "v4l2" ) )
    {
        ExtV4l2 *v4l2 = new ExtV4l2( p_intf, NULL );
        mainTabW->insertTab( V4L2_TAB, scrolled( v4l2 ), qtr( "v4l2 controls" ) );
    }

    /* "More": every other existing panel */
    moreButton = new QToolButton( mainTabW );
    moreButton->setText( qtr( "More" ) );
    moreButton->setAccessibleName( qtr( "More effects" ) );
    moreButton->setToolTip( qtr( "More effects" ) );
    moreButton->setPopupMode( QToolButton::InstantPopup );
    moreButton->setCheckable( true );
    moreButton->setToolButtonStyle( Qt::ToolButtonTextOnly );
    QMenu *moreMenu = new QMenu( moreButton );
    const struct { QTabWidget *pages; QString title; } groups[] = {
        { audioTab, qtr( "Audio Effects" ) },
        { videoTab, qtr( "Video Effects" ) },
    };
    for( size_t g = 0; g < sizeof( groups ) / sizeof( groups[0] ); g++ )
    {
        QTabWidget *pages = groups[g].pages;
        moreMenu->addSection( groups[g].title );
        for( int i = 1; i < pages->count(); i++ )
            connect( moreMenu->addAction( pages->tabText( i ) ), &QAction::triggered,
                     this, [this, pages, i]() { showPage( pages, i ); } );
    }
    moreButton->setMenu( moreMenu );
    mainTabW->setCornerWidget( moreButton, Qt::TopRightCorner );

    /* the top tabs lead to each group's first page */
    connect( mainTabW, &QTabWidget::tabBarClicked, this, [this]( int i ) {
        if( i == AUDIO_TAB )
            audioTab->setCurrentIndex( 0 );
        else if( i == VIDEO_TAB )
            videoTab->setCurrentIndex( 0 );
    } );
    connect( audioTab, &QTabWidget::currentChanged, this, &ExtendedDialog::updateMore );
    connect( videoTab, &QTabWidget::currentChanged, this, &ExtendedDialog::updateMore );

    layout->addWidget( mainTabW, 1 );
    connect( mainTabW, &QTabWidget::currentChanged, this, &ExtendedDialog::currentTabChanged );

    /* Save, on the Audio and Video tabs */
    QHBoxLayout *foot = new QHBoxLayout;
    const int pad = t->px( Effects::BodyPad );
    foot->setContentsMargins( pad, 0, pad, pad );
    foot->addStretch( 1 );
    m_applyButton = new QPushButton( qtr("&Save"), content );
    m_applyButton->setEnabled( false );
    foot->addWidget( m_applyButton );
    layout->addLayout( foot );
    connect( m_applyButton, &QPushButton::clicked, this, &ExtendedDialog::saveConfig );

    /* UI Automation does not read a buddy label: every slider and box of
     * the panels gets its label's text as its name */
    foreach( QLabel *label, content->findChildren<QLabel *>() )
    {
        QWidget *buddy = label->buddy();
        if( buddy && buddy->accessibleName().isEmpty() )
            buddy->setAccessibleName( label->text().remove( QLatin1Char( '&' ) )
                                                   .remove( QLatin1Char( ':' ) ).trimmed() );
    }

    connect( THEMIM->getIM(), &InputManager::playingStatusChanged, this, &ExtendedDialog::changedItem );
    connect( t, &Theme::changed, this, static_cast<void (QWidget::*)()>( &QWidget::update ) );

    if( !dock )
    {
        /* a window of its own */
        QVBoxLayout *frame = new QVBoxLayout( this );
        frame->setContentsMargins( 0, 0, 0, 0 );
        frame->addWidget( content );
        if( !restoreGeometry( getSettings()->value("EPanel/geometry").toByteArray() ) )
            resize( t->px( Effects::SheetW ), sizeHint().height() );
        return;
    }

    /* the drawer's shadow is see-through; paintEvent fills the sheet */
    setAttribute( Qt::WA_TranslucentBackground );
    dock->addWidget( this );

    host = new OverlayHost( over, this );
    connect( host, &OverlayHost::changed, this, &ExtendedDialog::place );

    slide = new QVariantAnimation( this );
    slide->setEasingCurve( OverlayWindow::enterCurve() );
    connect( slide, &QVariantAnimation::valueChanged, this,
             [this]( const QVariant &v ) { setShownWidth( v.toInt() ); } );

    setBreakpoint( p_mi->getBreakpoints()->current() );
    connect( p_mi->getBreakpoints(), &BreakpointWatcher::changed,
             this, &ExtendedDialog::setBreakpoint );

    /* the docked playlist takes the right side back */
    connect( THEDP, &DialogsProvider::panelShown, this, [this]( int action, bool shown ) {
        if( action == PLAYLIST_ACTION && shown )
            hide();
    } );
}

ExtendedDialog::~ExtendedDialog()
{
    if( !dock )
        getSettings()->setValue("EPanel/geometry", saveGeometry());
}

void ExtendedDialog::showEvent( QShowEvent *e )
{
    QWidget::showEvent( e );
    THEDP->setPanelShown( EXTENDED_ACTION, true );
    /* back from a minimised window: no slide, the focus stays */
    if( !dock || e->spontaneous() )
        return;
    slide->stop();
    const int ms = Theme::instance()->duration( Theme::PanelSlide );
    if( ms )
    {
        slide->setDuration( ms );
        slide->setStartValue( 0 );
        slide->setEndValue( fullWidth );
        slide->start();
    }
    else
        setShownWidth( fullWidth );
    if( drawer )
    {
        /* a window of its own: take the keys, Esc gives them back */
        raise();
        activateWindow();
    }
    mainTabW->setFocus();
}

void ExtendedDialog::hideEvent( QHideEvent *e )
{
    QWidget::hideEvent( e );
    /* a minimised main window hides the sheet too: it stays open */
    if( !e->spontaneous() )
        THEDP->setPanelShown( EXTENDED_ACTION, false );
}

void ExtendedDialog::setBreakpoint( BreakpointWatcher::Size bp )
{
    fullWidth = Theme::instance()->px( bp == BreakpointWatcher::XS ? Effects::SheetWXS
                                                                   : Effects::SheetW );
    const bool shown = isVisible();
    if( ( bp == BreakpointWatcher::XS ) != drawer )
    {
        /* the sheet moves, never the video: its native window stays put */
        drawer = bp == BreakpointWatcher::XS;
        if( drawer )
        {
            dock->removeWidget( this );
            setParent( dockParent->window(),
                       Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint );
            /* the docked fixed width would clamp the drawer's geometry */
            setMinimumWidth( 0 );
            setMaximumWidth( QWIDGETSIZE_MAX );
        }
        else
        {
            /* Qt::Widget drops the drawer's FramelessWindowHint: with it, a
             * native child of an alpha (translucent) format is flushed as a
             * layered window, which a child cannot be: it stays blank */
            setParent( dockParent, Qt::Widget );
            dock->addWidget( this );
        }
        if( shown )
            show();   /* setParent() hides */
    }
    slide->stop();
    setShownWidth( fullWidth );
}

/* shortcut: the dock and drawer code repeats PlaylistPanel's; share one
 * side panel class once a third panel needs it */
void ExtendedDialog::setShownWidth( int w )
{
    shownWidth = w;
    if( drawer )
        place();
    else
        setFixedWidth( w );
}

int ExtendedDialog::shadowWidth() const
{
    return drawer ? Theme::instance()->px( Playlist::DrawerShadow ) : 0;
}

/* The drawer: over the right of the stack, the shadow to its left */
void ExtendedDialog::place()
{
    if( !drawer )
        return;
    const QRect target = host->targetRect();
    const int w = shownWidth + shadowWidth();
    /* shortcut: always on the right, as in a left-to-right layout; mirror
     * it with the dock once right-to-left layouts are checked */
    setGeometry( target.right() + 1 - w, target.top(), w, target.height() );
}

void ExtendedDialog::resizeEvent( QResizeEvent * )
{
    if( dock )
        content->setGeometry( shadowWidth(), 0, fullWidth, height() );
}

void ExtendedDialog::paintEvent( QPaintEvent * )
{
    Theme *t = Theme::instance();
    QPainter p( this );
    const int s = shadowWidth();
    if( s )
    {
        /* shortcut: a linear ramp for the prototype's 50 px blur */
        QColor shadow( Qt::black );
        shadow.setAlpha( Playlist::DrawerShadowAlpha );
        QLinearGradient ramp( 0, 0, s, 0 );
        ramp.setColorAt( 0, Qt::transparent );
        ramp.setColorAt( 1, shadow );
        p.fillRect( 0, 0, s, height(), ramp );
    }
    /* FxBg is 0.97 opaque: over the panel colour, never over nothing */
    p.fillRect( s, 0, width() - s, height(), t->color( Theme::Panel ) );
    p.fillRect( s, 0, width() - s, height(), t->color( Theme::FxBg ) );
    if( dock )
        p.fillRect( s, 0, Size::Hairline, height(), t->ink( InkAlpha::Divider ) );
}

/* QVLCDialog's keys: Esc and Enter close */
void ExtendedDialog::keyPressEvent( QKeyEvent *e )
{
    switch( e->key() )
    {
    case Qt::Key_Escape:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        hide();
        return;
    }
    if( drawer && dockParent->window() )
        /* a window of its own: the player's hotkeys still apply */
        QApplication::sendEvent( dockParent->window(), e );
    else
        QWidget::keyPressEvent( e );
}

void ExtendedDialog::showTab( int i )
{
    mainTabW->setCurrentIndex( i );
    show();
    if( !dock )
    {
        raise();
        activateWindow();
    }
}

int ExtendedDialog::currentTab()
{
    return mainTabW->currentIndex();
}

void ExtendedDialog::showPage( QTabWidget *pages, int page )
{
    mainTabW->setCurrentIndex( pages == audioTab ? AUDIO_TAB : VIDEO_TAB );
    pages->setCurrentIndex( page );
    pages->currentWidget()->setFocus();
}

/* "More" names the sub-page shown, if not the group's first */
void ExtendedDialog::updateMore()
{
    const int tab = currentTab();
    QTabWidget *pages = tab == AUDIO_TAB ? audioTab : tab == VIDEO_TAB ? videoTab : NULL;
    const bool sub = pages && pages->currentIndex() > 0;
    moreButton->setText( sub ? pages->tabText( pages->currentIndex() ) : qtr( "More" ) );
    moreButton->setChecked( sub );
}

void ExtendedDialog::changedItem( int i_status )
{
    if( i_status != END_S ) return;
    syncW->clean();
    videoEffect->clean();
}

void ExtendedDialog::currentTabChanged( int i )
{
    if( i == AUDIO_TAB || i == VIDEO_TAB )
    {
        m_applyButton->setVisible( true );
        m_applyButton->setEnabled( !m_hashConfigs[i].isEmpty() );
    }
    else
        m_applyButton->setVisible( false );
    updateMore();
}

void ExtendedDialog::putAudioConfig( const QString& name, const QVariant value )
{
    m_hashConfigs[AUDIO_TAB].insert( name, value );
    m_applyButton->setEnabled( true );
}

void ExtendedDialog::putVideoConfig( const QString& name, const QVariant value )
{
    m_hashConfigs[VIDEO_TAB].insert( name, value );
    m_applyButton->setEnabled( true );
}

void ExtendedDialog::saveConfig()
{
    assert( currentTab() == AUDIO_TAB || currentTab() == VIDEO_TAB );
    QHash<QString, QVariant> *hashConfig = &m_hashConfigs[currentTab()];

    for( QHash<QString, QVariant>::iterator i = hashConfig->begin();
         i != hashConfig->end(); ++i )
    {
        QVariant &value = i.value();
        switch( static_cast<QMetaType::Type>(value.type()) )
        {
            case QMetaType::QString:
                config_PutPsz( p_intf, qtu(i.key()), qtu(value.toString()) );
                break;
            case QMetaType::Int:
            case QMetaType::Bool:
                config_PutInt( p_intf, qtu(i.key()), value.toInt() ) ;
                break;
            case QMetaType::Double:
            case QMetaType::Float:
                config_PutFloat( p_intf, qtu(i.key()), value.toFloat() ) ;
                break;
            default:
                vlc_assert_unreachable();
        }
    }
    config_SaveConfigFile( p_intf );
    hashConfig->clear();
    m_applyButton->setEnabled( false );
}
