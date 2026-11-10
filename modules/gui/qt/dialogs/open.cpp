/*****************************************************************************
 * open.cpp : Advanced open dialog
 *****************************************************************************
 * Copyright © 2006-2011 the VideoLAN team
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
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

#include "dialogs/open.hpp"
#include "dialogs_provider.hpp"
#include "recents.hpp"
#include "util/qt_dirs.hpp"
#include "util/redesign_icons.hpp"
#include "util/theme.hpp"

#include <QRegularExpression>
#include <QMenu>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QTabBar>
#include <QToolButton>
#include <QKeySequence>
#include <QKeyEvent>
#include <QAccessibleWidget>

#include <cstddef>

#ifndef NDEBUG
# define DEBUG_QT 1
#endif

OpenDialog *OpenDialog::instance = NULL;

namespace
{

/* Qt 5.6 tells assistive tools that a checkable tool button is a check
 * box and a split button a menu item: the tiles are the tabs of a tab
 * list, Play is a button (its menu is a second action) */
class OpenButtonAccessible : public QAccessibleWidget
{
public:
    OpenButtonAccessible( QToolButton *b, QAccessible::Role r ) : QAccessibleWidget( b, r ) {}

    QString text( QAccessible::Text t ) const Q_DECL_OVERRIDE
    {
        if( t == QAccessible::Name )
            return QString( button()->text() ).remove( QLatin1Char( '&' ) );
        if( t == QAccessible::Accelerator )
            return button()->shortcut().toString( QKeySequence::NativeText );
        return QAccessibleWidget::text( t );
    }
    QAccessible::State state() const Q_DECL_OVERRIDE
    {
        QAccessible::State s = QAccessibleWidget::state();
        if( role() == QAccessible::PageTab )
        {
            s.selectable = true;
            s.selected = button()->isChecked();
        }
        s.pressed = button()->isDown();
        return s;
    }
    QStringList actionNames() const Q_DECL_OVERRIDE
    {
        QStringList names;
        if( button()->isEnabled() )
        {
            names << pressAction();
            if( button()->menu() )
                names << showMenuAction();
        }
        return names + QAccessibleWidget::actionNames();
    }
    void doAction( const QString &action ) Q_DECL_OVERRIDE
    {
        if( !button()->isEnabled() )
            return;
        if( action == pressAction() )
            button()->click();
        else if( action == showMenuAction() )
            button()->showMenu();
        else
            QAccessibleWidget::doAction( action );
    }

private:
    QToolButton *button() const { return static_cast<QToolButton *>( widget() ); }
};

QAccessibleInterface *openButtonAccessible( const QString &, QObject *object )
{
    QToolButton *b = qobject_cast<QToolButton *>( object );
    const QString role = b ? b->property( "role" ).toString() : QString();
    if( role == QLatin1String( "openTile" ) )
        return new OpenButtonAccessible( b, QAccessible::PageTab );
    if( role == QLatin1String( "split" ) )
        return new OpenButtonAccessible( b, QAccessible::Button );
    return NULL;
}

}

OpenDialog* OpenDialog::getInstance( QWidget *parent, intf_thread_t *p_intf,
        bool b_rawInstance, int _action_flag, bool b_selectMode, bool _b_pl )
{
    /* Creation */
    if( !instance )
        instance = new OpenDialog( parent, p_intf, b_selectMode,
                                   _action_flag, _b_pl );
    else if( !b_rawInstance )
    {
        /* Request the instance but change small details:
           - Button menu */
        if( b_selectMode )
            _action_flag = SELECT; /* This should be useless, but we never know
                                      if the call is correct */
        instance->setWindowModality( Qt::WindowModal );
        instance->i_action_flag = _action_flag;
        instance->b_pl = _b_pl;
        instance->setMenuAction();
    }
    return instance;
}

OpenDialog::OpenDialog( QWidget *parent,
                        intf_thread_t *_p_intf,
                        bool b_selectMode,
                        int _action_flag,
                        bool _b_pl)  :  QVLCDialog( parent, _p_intf )
{
    i_action_flag = _action_flag;
    b_pl =_b_pl;

    if( b_selectMode ) /* Select mode */
        i_action_flag = SELECT;

    /* Basic Creation of the Window */
    ui.setupUi( this );
    setWindowTitle( qtr( "Open Media" ) );
    setWindowRole( "vlc-open-media" );
    setWindowModality( Qt::WindowModal );
    /* a modal sheet in the design (1.7): Close only, no min/max */
    setWindowFlags( Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                    Qt::WindowCloseButtonHint );

    /* Tab definition and creation */
    fileOpenPanel    = new FileOpenPanel( this, p_intf );
    discOpenPanel    = new DiscOpenPanel( this, p_intf );
    netOpenPanel     = new NetOpenPanel( this, p_intf );
    captureOpenPanel = new CaptureOpenPanel( this, p_intf );

    /* Insert the tabs */
    ui.Tab->insertTab( OPEN_FILE_TAB, fileOpenPanel, qtr( "&File" ) );
    ui.Tab->insertTab( OPEN_DISC_TAB, discOpenPanel, qtr( "&Disc" ) );
    ui.Tab->insertTab( OPEN_NETWORK_TAB, netOpenPanel, qtr( "&Network" ) );
    ui.Tab->insertTab( OPEN_CAPTURE_TAB, captureOpenPanel, qtr( "Capture &Device" ) );

    /* DESIGN_SPEC 7: the tabs are tiles across the sheet, icon over text
     * (QSS role "openTile"); they drive the tab widget, whose bar is hidden */
    static const char * const tileIcons[OPEN_TAB_MAX] = {
        RedesignIcons::IconOpenFile, RedesignIcons::IconOpenDisc,
        RedesignIcons::IconOpenNetwork, RedesignIcons::IconOpenCapture };
    Theme *t = Theme::instance();
    const int glyph = t->px( RedesignTokens::Dialogs::TabTileGlyph );
    QHBoxLayout *tileRow = new QHBoxLayout;
    tileRow->setSpacing( t->px( RedesignTokens::Dialogs::TabTileGap ) );
    static bool accessibleInstalled = false;
    if( !accessibleInstalled )
    {
        QAccessible::installFactory( openButtonAccessible );
        accessibleInstalled = true;
    }
    tiles = new QButtonGroup( this );
    for( int i = 0; i < OPEN_TAB_MAX; i++ )
    {
        QToolButton *tile = new QToolButton;
        tile->setProperty( "role", "openTile" );
        /* no underlined mnemonic on a tile (1.6.2); its Alt key stays.
         * shortcut: Disc and Capture &Device share Alt+D (Qt focuses each in
         * turn), as before; give Capture its own letter in the strings */
        const QString label = ui.Tab->tabText( i );
        const QKeySequence key = QKeySequence::mnemonic( label );
        tile->setText( QString( label ).remove( QLatin1Char( '&' ) ) );
        tile->setShortcut( key );
        tile->setToolTip( qtr( "%1 (%2)" ).arg( tile->text(),
                                               key.toString( QKeySequence::NativeText ) ) );
        tile->setIcon( Theme::themedIcon( tileIcons[i], Theme::Text2 ) );
        tile->setIconSize( QSize( glyph, glyph ) );
        tile->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
        tile->setCheckable( true );
        tile->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
        tile->setFixedHeight( t->px( RedesignTokens::Dialogs::TabTileH ) );
        /* not a Tab stop until checked (toggled below); its Alt key still focuses it */
        tile->setFocusPolicy( Qt::ClickFocus );
        tiles->addButton( tile, i );
        tile->installEventFilter( this );
        tileRow->addWidget( tile );
    }
    ui.Tab->tabBar()->hide();
    ui.gridLayout->addLayout( tileRow, 0, 0, 1, 3 );
    /* toggled, not clicked: an assistive tool may check a tile without a click.
     * Only the checked tile is a Tab stop (a tab list): Qt's exclusive group
     * does not do this for tool buttons, and focus left on an unchecked tile
     * made the arrows select the tile beside it (Capture from the last one) */
    connect( tiles, QOverload<int, bool>::of( &QButtonGroup::buttonToggled ),
             ui.Tab, [this]( int i, bool on ) {
        tiles->button( i )->setFocusPolicy( on ? Qt::StrongFocus : Qt::ClickFocus );
        if( on ) ui.Tab->setCurrentIndex( i );
    } );
    tiles->button( ui.Tab->currentIndex() )->setChecked( true );
    connect( ui.Tab, &QTabWidget::currentChanged, tiles,
             [this]( int i ) { tiles->button( i )->setChecked( true ); } );

    /* Hide the Slave input widgets */
    ui.slaveLabel->hide();
    ui.slaveText->hide();
    ui.slaveBrowseButton->hide();

    /* Buttons Creation */
    /* Play Button */
    playButton = ui.playButton;

    /* Cancel Button */
    cancelButton = new QPushButton( qtr( "&Cancel" ) );

    /* Select Button */
    selectButton = new QPushButton( qtr( "&Select" ) );

    /* Menu for the Play button */
    QMenu * openButtonMenu = new QMenu( "Open", playButton );
    openButtonMenu->addAction( qtr( "&Enqueue" ), this, SLOT( enqueue() ),
                                    QKeySequence( "Alt+E" ) );
    openButtonMenu->addAction( qtr( "&Play" ), this, SLOT( play() ),
                                    QKeySequence( "Alt+P" ) );
    openButtonMenu->addAction( qtr( "&Stream" ), this, SLOT( stream() ) ,
                                    QKeySequence( "Alt+S" ) );
    openButtonMenu->addAction( qtr( "C&onvert" ), this, SLOT( transcode() ) ,
                                    QKeySequence( "Alt+O" ) );

    playButton->setMenu( openButtonMenu );

    /* Add the three Buttons */
    ui.buttonsBox->addButton( selectButton, QDialogButtonBox::AcceptRole );
    ui.buttonsBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    /* At creation time, modify the default buttons */
    setMenuAction();

    /* Force MRL update on tab change */
    connect( ui.Tab, &QTabWidget::currentChanged, this, &OpenDialog::signalCurrent );

    connect( fileOpenPanel, &FileOpenPanel::mrlUpdated,
             this, QOverload<const QStringList &, const QString &>::of(&OpenDialog::updateMRL) );
    connect( netOpenPanel, &NetOpenPanel::mrlUpdated,
             this, QOverload<const QStringList &, const QString &>::of(&OpenDialog::updateMRL) );
    connect( discOpenPanel, &DiscOpenPanel::mrlUpdated,
             this, QOverload<const QStringList &, const QString &>::of(&OpenDialog::updateMRL) );
    connect( captureOpenPanel, &CaptureOpenPanel::mrlUpdated,
             this, QOverload<const QStringList &, const QString &>::of(&OpenDialog::updateMRL) );

    connect( fileOpenPanel, &FileOpenPanel::methodChanged,
             this, &OpenDialog::newCachingMethod );
    connect( netOpenPanel, &NetOpenPanel::methodChanged,
             this, &OpenDialog::newCachingMethod );
    connect( discOpenPanel, &DiscOpenPanel::methodChanged,
             this, &OpenDialog::newCachingMethod );
    connect( captureOpenPanel, &CaptureOpenPanel::methodChanged,
             this, &OpenDialog::newCachingMethod );

    /* Advanced frame Connects */
    connect( ui.slaveCheckbox, &QCheckBox::toggled, this, QOverload<>::of(&OpenDialog::updateMRL) );
    connect( ui.slaveText, &QLineEdit::textChanged, this, QOverload<>::of(&OpenDialog::updateMRL) );
    connect( ui.cacheSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, QOverload<>::of(&OpenDialog::updateMRL) );
    connect( ui.startTimeTimeEdit, &QTimeEdit::timeChanged, this, QOverload<>::of(&OpenDialog::updateMRL) );
    connect( ui.stopTimeTimeEdit, &QTimeEdit::timeChanged, this, QOverload<>::of(&OpenDialog::updateMRL) );
    BUTTONACT( ui.advancedCheckBox, toggleAdvancedPanel );
    BUTTONACT( ui.slaveBrowseButton, browseInputSlave );

    /* Buttons action */
    BUTTONACT( playButton, selectSlots );
    BUTTONACT( selectButton, close );
    BUTTONACT( cancelButton, cancel );

    /* Hide the advancedPanel */
    if( !getSettings()->value( "OpenDialog/advanced", false ).toBool())
    {
        ui.advancedFrame->hide();
        ui.advancedFrame->setEnabled( false );
    }
    else
        ui.advancedCheckBox->setChecked( true );

    /* Initialize caching */
    storedMethod = "";
    newCachingMethod( "file-caching" );

    /* enforce section due to .ui bug */
    ui.startTimeTimeEdit->setCurrentSection( QDateTimeEdit::SecondSection );
    ui.stopTimeTimeEdit->setCurrentSection( QDateTimeEdit::SecondSection );

    setMinimumSize( sizeHint() );
    setMaximumWidth( 900 );
    resize( getSettings()->value( "OpenDialog/size", QSize( 500, 400 ) ).toSize() );
}

/* The tiles are a tab list: only the checked tile is a Tab stop (its focus
 * policy, set on toggle), the arrows, Home and End move to a tile and open it */
bool OpenDialog::eventFilter( QObject *obj, QEvent *e )
{
    QAbstractButton *tile = qobject_cast<QAbstractButton *>( obj );
    const int current = tile ? tiles->id( tile ) : -1;
    if( e->type() != QEvent::KeyPress || current < 0 )
        return QVLCDialog::eventFilter( obj, e );
    const int count = tiles->buttons().count();
    const int forward = tile->isRightToLeft() ? -1 : 1;
    int next;
    switch( static_cast<QKeyEvent *>( e )->key() )
    {
    case Qt::Key_Left:  next = current - forward; break;
    case Qt::Key_Right: next = current + forward; break;
    case Qt::Key_Up:    next = current - 1; break;
    case Qt::Key_Down:  next = current + 1; break;
    case Qt::Key_Home:  next = 0; break;
    case Qt::Key_End:   next = count - 1; break;
    default:
        return QVLCDialog::eventFilter( obj, e );
    }
    /* no wrap, as a tab bar: Left on File doesn't land on Capture */
    QAbstractButton *target = tiles->button( qBound( 0, next, count - 1 ) );
    target->setChecked( true );
    /* after the panel's onFocus(): the keyboard stays on the tiles */
    target->setFocus( Qt::TabFocusReason );
    return true;
}

/* Finish the dialog and decide if you open another one after */
void OpenDialog::setMenuAction()
{
    if( i_action_flag == SELECT )
    {
        playButton->hide();
        selectButton->show();
        selectButton->setDefault( true );
    }
    else
    {
        switch ( i_action_flag )
        {
        case OPEN_AND_STREAM:
            playButton->setText( qtr( "&Stream" ) );
            break;
        case OPEN_AND_SAVE:
            playButton->setText( qtr( "C&onvert / Save" ) );
            break;
        case OPEN_AND_ENQUEUE:
            playButton->setText( qtr( "&Enqueue" ) );
            break;
        case OPEN_AND_PLAY:
        default:
            playButton->setText( qtr( "&Play" ) );
        }
        playButton->show();
        selectButton->hide();
    }
}

OpenDialog::~OpenDialog()
{
    getSettings()->setValue( "OpenDialog/size", size() -
                 ( ui.advancedFrame->isEnabled() ?
                   QSize(0, ui.advancedFrame->height()) : QSize(0, 0) ) );
    /* the checkbox: the frame is no longer visible once the dialog is hidden */
    getSettings()->setValue( "OpenDialog/advanced", ui.advancedCheckBox->isChecked() );
}

/* Used by VLM dialog and inputSlave selection */
QString OpenDialog::getMRL( bool b_all )
{
    if( itemsMRL.count() == 0 ) return "";
    return b_all ? itemsMRL[0] + getOptions()
                 : itemsMRL[0];
}

QStringList OpenDialog::getMRLs( bool b_with_options )
{
    if ( !b_with_options ) return itemsMRL;
    QStringList postfixedMRLs;
    foreach( const QString &mrl, itemsMRL )
        postfixedMRLs << QString( mrl ).append( getOptions() );
    return postfixedMRLs;
}

QString OpenDialog::getOptions()
{
    return ui.advancedLineInput->text();
}

void OpenDialog::showTab( int i_tab )
{
    if( i_tab == OPEN_CAPTURE_TAB ) captureOpenPanel->initialize();
    ui.Tab->setCurrentIndex( i_tab );
    show();
    if( ui.Tab->currentWidget() != NULL )
    {
        OpenPanel *panel = qobject_cast<OpenPanel *>( ui.Tab->currentWidget() );
        assert( panel );
        panel->onFocus();
        /* start in the panel (its first control), not on "Show more options" */
        for( QWidget *w = panel->nextInFocusChain();
             w != panel && !panel->isAncestorOf( focusWidget() );
             w = w->nextInFocusChain() )
            if( panel->isAncestorOf( w ) && w->isVisibleTo( panel ) && w->isEnabled()
             && ( w->focusPolicy() & Qt::TabFocus ) )
                w->setFocus( Qt::TabFocusReason );
    }
}

void OpenDialog::toggleAdvancedPanel()
{
    if( ui.advancedFrame->isVisible() )
    {
        ui.advancedFrame->hide();
        ui.advancedFrame->setEnabled( false );
        if( size().isValid() )
            resize( size().width(), size().height()
                    - ui.advancedFrame->height() );
    }
    else
    {
        ui.advancedFrame->show();
        ui.advancedFrame->setEnabled( true );
        if( size().isValid() )
            resize( size().width(), size().height()
                    + ui.advancedFrame->height() );
    }
}

void OpenDialog::browseInputSlave()
{
    OpenDialog *od = new OpenDialog( this, p_intf, true, SELECT );
    od->exec();
    ui.slaveText->setText( od->getMRL( false ) );
    delete od;
}

/* Function called on signal currentChanged triggered */
void OpenDialog::signalCurrent( int i_tab )
{
    if( i_tab == OPEN_CAPTURE_TAB ) captureOpenPanel->initialize();
    if( ui.Tab->currentWidget() != NULL )
    {
        OpenPanel *panel = qobject_cast<OpenPanel *>( ui.Tab->currentWidget() );
        assert( panel );
        panel->onFocus();
        panel->updateMRL();
        panel->updateContext( i_action_flag == OPEN_AND_PLAY ?
                              OpenPanel::CONTEXT_INTERACTIVE :
                              OpenPanel::CONTEXT_BATCH );
    }
}

/***********
 * Actions *
 ***********/
/* If Cancel is pressed or escaped */
void OpenDialog::cancel()
{
    /* Clear the panels */
    for( int i = 0; i < OPEN_TAB_MAX; i++ )
        qobject_cast<OpenPanel*>( ui.Tab->widget( i ) )->clear();

    /* Clear the variables */
    itemsMRL.clear();
    optionsMRL.clear();

    /* If in Select Mode, reject instead of hiding */
    if( i_action_flag == SELECT ) reject();
    else hide();
}

/* If EnterKey is pressed */
void OpenDialog::close()
{
    /* If in Select Mode, accept instead of selecting a Slot */
    if( i_action_flag == SELECT )
        accept();
    else
        selectSlots();
}

/* Play button */
void OpenDialog::selectSlots()
{
    switch ( i_action_flag )
    {
    case OPEN_AND_STREAM:
        stream();
        break;
    case OPEN_AND_SAVE:
        transcode();
        break;
    case OPEN_AND_ENQUEUE:
        enqueue();
        break;
    case OPEN_AND_PLAY:
    default:
        play();
    }
}

/* Play Action, called from selectSlots or play Menu */
void OpenDialog::play()
{
    enqueue( false );
}

/* Enqueue Action, called from selectSlots or enqueue Menu */
void OpenDialog::enqueue( bool b_enqueue )
{
    toggleVisible();

    if( i_action_flag == SELECT )
    {
        accept();
        return;
    }

    for( int i = 0; i < OPEN_TAB_MAX; i++ )
        qobject_cast<OpenPanel*>( ui.Tab->widget( i ) )->onAccept();

    /* Sort alphabetically */
    itemsMRL.sort();

    /* Go through the item list */
    for( int i = 0; i < itemsMRL.count(); i++ )
    {
        bool b_start = !i && !b_enqueue;

        /* Take options from the UI, not from what we stored */
        QStringList optionsList = getOptions().split( " :" );

        /* Switch between enqueuing and starting the item */
        Open::openMRLwithOptions( p_intf, itemsMRL[i], &optionsList, b_start, b_pl );
    }
}

void OpenDialog::transcode()
{
    stream( true );
}

void OpenDialog::stream( bool b_transcode_only )
{
//    QString soutMRL = getMRL( false );
//    if( soutMRL.isEmpty() ) return;

    for( int i = 0; i < OPEN_TAB_MAX; i++ )
        qobject_cast<OpenPanel*>( ui.Tab->widget( i ) )->onAccept();

    QStringList soutMRLS = getMRLs(false);
    if(soutMRLS.empty())
    {
        return;
    }

    toggleVisible();

    /* Dbg and send :D */
    msg_Dbg( p_intf, "MRL(s) passed to the Sout: %zu", static_cast<size_t>( soutMRLS.length() ) );
    for(int i = 0; i < soutMRLS.length(); i++)
    {
        msg_Dbg( p_intf, "MRL(s) passed to the Sout: %s", qtu( soutMRLS[i] ) );
    }
    THEDP->streamingDialog( this, soutMRLS, b_transcode_only,
                            getOptions().split( " :" ) );
}

/* Update the MRL items from the panels */
void OpenDialog::updateMRL( const QStringList& item, const QString& tempMRL )
{
    optionsMRL = tempMRL;
    itemsMRL = item;
    updateMRL();
}

/* Format the time to milliseconds */
QString TimeToMilliseconds( const QTimeEdit *t ) {
    return QString::number( ( t->minimumTime().msecsTo( t->time() ) ) / 1000.0, 'f', 3);
}

/* Update the complete MRL */
void OpenDialog::updateMRL() {
    QString mrl = optionsMRL;
    if( ui.slaveCheckbox->isChecked() ) {
        mrl += " :input-slave=" + ui.slaveText->text();
    }
    mrl += QString( " :%1=%2" ).arg( storedMethod ).
                                arg( ui.cacheSpinBox->value() );
    if( ui.startTimeTimeEdit->time() != ui.startTimeTimeEdit->minimumTime() ) {
        mrl += " :start-time=" + TimeToMilliseconds( ui.startTimeTimeEdit );
    }
    if( ui.stopTimeTimeEdit->time() > ui.startTimeTimeEdit->time() ) {
        mrl += " :stop-time=" + TimeToMilliseconds( ui.stopTimeTimeEdit );
    }
    ui.advancedLineInput->setText( mrl );
    ui.mrlLine->setText( itemsMRL.join( " " ) );
    /* Only allow action without valid items */
    playButton->setEnabled( !itemsMRL.isEmpty() );
    selectButton->setEnabled( !itemsMRL.isEmpty() );
}

/* Change the caching combobox */
void OpenDialog::newCachingMethod( const QString& method )
{
    if( method != storedMethod ) {
        storedMethod = method;
        int i_value = var_InheritInteger( p_intf, qtu( storedMethod ) );
        ui.cacheSpinBox->setValue( i_value );
    }
}

/* Split the entries
 * FIXME! */
QStringList OpenDialog::SeparateEntries( const QString& entries )
{
    bool b_quotes_mode = false;

    QStringList entries_array;
    QString entry;

    int index = 0;
    while( index < entries.count() )
    {
        int delim_pos = entries.indexOf( QRegularExpression( "\\s+|\"" ), index );
        if( delim_pos < 0 ) delim_pos = entries.count() - 1;
        entry += entries.mid( index, delim_pos - index + 1 );
        index = delim_pos + 1;

        if( entry.isEmpty() ) continue;

        if( !b_quotes_mode && entry.endsWith( "\"" ) )
        {
            /* Enters quotes mode */
            entry.truncate( entry.count() - 1 );
            b_quotes_mode = true;
        }
        else if( b_quotes_mode && entry.endsWith( "\"" ) )
        {
            /* Finished the quotes mode */
            entry.truncate( entry.count() - 1 );
            b_quotes_mode = false;
        }
        else if( !b_quotes_mode && !entry.endsWith( "\"" ) )
        {
            /* we found a non-quoted standalone string */
            if( index < entries.count() ||
                entry.endsWith( " " ) || entry.endsWith( "\t" ) ||
                entry.endsWith( "\r" ) || entry.endsWith( "\n" ) )
                entry.truncate( entry.count() - 1 );
            if( !entry.isEmpty() ) entries_array.append( entry );
            entry.clear();
        }
        else
        {;}
    }

    if( !entry.isEmpty() ) entries_array.append( entry );

    return entries_array;
}

