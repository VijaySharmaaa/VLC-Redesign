/*****************************************************************************
 * extended_panels.cpp : Extended controls panels
 ****************************************************************************
 * Copyright (C) 2006-2013 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Antoine Cellerier <dionoea .t videolan d@t org>
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

#include <math.h>

#include <algorithm>

#include <QLabel>
#include <QVariant>
#include <QString>
#include <QFont>
#include <QGridLayout>
#include <QComboBox>
#include <QTimer>
#include <QFileDialog>
#include <QPainter>
#include <QRegularExpression>
#include <QApplication>
#include <QFrame>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>

#include "components/extended_panels.hpp"
#include "dialogs/preferences.hpp"
#include "qt.hpp"
#include "input_manager.hpp"
#include "util/qt_dirs.hpp"
#include "util/customwidgets.hpp"
#include "dialogs_provider.hpp"
#include "util/theme.hpp"
#include "util/redesign_icons.hpp"

#include "../../audio_filter/equalizer_presets.h"
#include <vlc_vout.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>

static bool filterIsPresent( const QString &filters, const QString &filter )
{
    QStringList list = filters.split( ':',
                                      #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                                        Qt::SkipEmptyParts
                                      #else
                                        QString::SkipEmptyParts
                                      #endif
                                    );

    foreach( const QString &filterCmp, list )
    {
        if( filterCmp.compare( filter ) == 0 )
            return true;
    }
    return false;
}

static const char* GetVFilterType( struct intf_thread_t *p_intf, const char *psz_name )
{
    module_t *p_obj = module_find( psz_name );
    if( !p_obj )
    {
        msg_Err( p_intf, "Unable to find filter module \"%s\".", psz_name );
        return NULL;
    }

    if( module_provides( p_obj, "video splitter" ) )
        return "video-splitter";
    else if( module_provides( p_obj, "video filter" ) )
        return "video-filter";
    else if( module_provides( p_obj, "sub source" ) )
        return "sub-source";
    else if( module_provides( p_obj, "sub filter" ) )
        return "sub-filter";
    else
    {
        msg_Err( p_intf, "Unknown video filter type." );
        return NULL;
    }
}

static const QString ModuleFromWidgetName( QObject *obj )
{
    return obj->objectName().replace( "Enable","" );
}

static QString OptionFromWidgetName( QObject *obj )
{
    /* Gruik ? ... nah */
    return obj->objectName()
        .remove( QRegularExpression( "Slider|Combo|Dial|Check|Spin|Text" ) )
        .replace( QRegularExpression( "([A-Z])" ), "-\\1" )
        .toLower();
}

static inline void setup_vfilter( intf_thread_t *p_intf, const char* psz_name, QWidget *widget )
{
    const char *psz_filter_type = GetVFilterType( p_intf, psz_name );
    if( psz_filter_type == NULL )
        return;

    char *psz_filters = var_InheritString( THEPL, psz_filter_type );
    if( psz_filters == NULL )
        return;

    QCheckBox *checkbox = qobject_cast<QCheckBox*>( widget );
    QGroupBox *groupbox = qobject_cast<QGroupBox*>( widget );
    if( filterIsPresent( qfu(psz_filters), qfu(psz_name) ) )
    {
        if( checkbox ) checkbox->setChecked( true ); \
        else if (groupbox) groupbox->setChecked( true ); \
    }
    else
    {
        if( checkbox ) checkbox->setChecked( false );
        else if (groupbox) groupbox->setChecked( false );
    }
    free( psz_filters );
}

#define SETUP_VFILTER( widget ) \
    setup_vfilter( p_intf, #widget, ui.widget##Enable ); \
    connect( ui.widget##Enable, &std::remove_pointer<decltype(ui.widget##Enable)>::type::clicked, this, &ExtVideo::updateFilters );

#define SETUP_VFILTER_OPTION( widget, signal ) \
    initComboBoxItems( ui.widget ); \
    setWidgetValue( ui.widget ); \
    connect( ui.widget, signal, this, &ExtVideo::updateFilterOptions );

ExtVideo::ExtVideo( intf_thread_t *_p_intf, QTabWidget *_parent ) :
            QObject( _parent ), p_intf( _p_intf )
{
    ui.setupUi( _parent );

    SETUP_VFILTER( adjust )
    SETUP_VFILTER_OPTION( hueSlider, &QSlider::valueChanged )
    SETUP_VFILTER_OPTION( contrastSlider, &QSlider::valueChanged )
    SETUP_VFILTER_OPTION( brightnessSlider, &QSlider::valueChanged )
    SETUP_VFILTER_OPTION( saturationSlider, &QSlider::valueChanged )
    SETUP_VFILTER_OPTION( gammaSlider, &QSlider::valueChanged )
    SETUP_VFILTER_OPTION( brightnessThresholdCheck, &QCheckBox::stateChanged )

    SETUP_VFILTER( extract )
    SETUP_VFILTER_OPTION( extractComponentText, &QLineEdit::textChanged )

    SETUP_VFILTER( posterize )

    SETUP_VFILTER( colorthres )
    SETUP_VFILTER_OPTION( colorthresColorText, &QLineEdit::textChanged )
    SETUP_VFILTER_OPTION( colorthresSaturationthresSlider, &QSlider::valueChanged )
    SETUP_VFILTER_OPTION( colorthresSimilaritythresSlider, &QSlider::valueChanged )

    SETUP_VFILTER( sepia )
    SETUP_VFILTER_OPTION( sepiaIntensitySpin, QOverload<int>::of(&QSpinBox::valueChanged) )

    SETUP_VFILTER( invert )

    SETUP_VFILTER( gradient )
    SETUP_VFILTER_OPTION( gradientModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged) )
    SETUP_VFILTER_OPTION( gradientTypeCheck, &QCheckBox::stateChanged )
    SETUP_VFILTER_OPTION( gradientCartoonCheck, &QCheckBox::stateChanged )

    SETUP_VFILTER( motionblur )
    SETUP_VFILTER_OPTION( blurFactorSlider, &QSlider::valueChanged )

    SETUP_VFILTER( motiondetect )

    SETUP_VFILTER( psychedelic )

    SETUP_VFILTER( sharpen )
    SETUP_VFILTER_OPTION( sharpenSigmaSlider, &QSlider::valueChanged )

    SETUP_VFILTER( ripple )

    SETUP_VFILTER( wave )

    SETUP_VFILTER( transform )
    SETUP_VFILTER_OPTION( transformTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged) )

    SETUP_VFILTER( rotate )
    SETUP_VFILTER_OPTION( rotateAngleDial, &VLCQDial::valueChanged )
    ui.rotateAngleDial->setWrapping( true );
    ui.rotateAngleDial->setNotchesVisible( true );

    SETUP_VFILTER( puzzle )
    SETUP_VFILTER_OPTION( puzzleRowsSpin, QOverload<int>::of(&QSpinBox::valueChanged) )
    SETUP_VFILTER_OPTION( puzzleColsSpin, QOverload<int>::of(&QSpinBox::valueChanged) )

    SETUP_VFILTER( magnify )

    SETUP_VFILTER( clone )
    SETUP_VFILTER_OPTION( cloneCountSpin, QOverload<int>::of(&QSpinBox::valueChanged) )

    SETUP_VFILTER( wall )
    SETUP_VFILTER_OPTION( wallRowsSpin, QOverload<int>::of(&QSpinBox::valueChanged) )
    SETUP_VFILTER_OPTION( wallColsSpin, QOverload<int>::of(&QSpinBox::valueChanged) )


    SETUP_VFILTER( erase )
    SETUP_VFILTER_OPTION( eraseMaskText, &QLineEdit::editingFinished )
    SETUP_VFILTER_OPTION( eraseYSpin, QOverload<int>::of(&QSpinBox::valueChanged) )
    SETUP_VFILTER_OPTION( eraseXSpin, QOverload<int>::of(&QSpinBox::valueChanged) )
    BUTTONACT( ui.eraseBrowseBtn, browseEraseFile );

    SETUP_VFILTER( marq )
    SETUP_VFILTER_OPTION( marqMarqueeText, &QLineEdit::textChanged )
    SETUP_VFILTER_OPTION( marqPositionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged) )

    SETUP_VFILTER( logo )
    SETUP_VFILTER_OPTION( logoFileText, &QLineEdit::editingFinished )
    SETUP_VFILTER_OPTION( logoYSpin, QOverload<int>::of(&QSpinBox::valueChanged) )
    SETUP_VFILTER_OPTION( logoXSpin, QOverload<int>::of(&QSpinBox::valueChanged) )
    SETUP_VFILTER_OPTION( logoOpacitySlider, &QSlider::valueChanged )
    BUTTONACT( ui.logoBrowseBtn, browseLogo );

    SETUP_VFILTER( gradfun )
    SETUP_VFILTER_OPTION( gradfunRadiusSlider, &QSlider::valueChanged )

    SETUP_VFILTER( grain )
    SETUP_VFILTER_OPTION( grainVarianceSlider, &QSlider::valueChanged )

    SETUP_VFILTER( mirror )

    SETUP_VFILTER( gaussianblur )
    SETUP_VFILTER_OPTION( gaussianblurSigmaSlider, &QSlider::valueChanged )

    SETUP_VFILTER( antiflicker )
    SETUP_VFILTER_OPTION( antiflickerSofteningSizeSlider, &QSlider::valueChanged )

    SETUP_VFILTER( hqdn3d )
    SETUP_VFILTER_OPTION( hqdn3dLumaSpatSlider, &QSlider::valueChanged )
    SETUP_VFILTER_OPTION( hqdn3dLumaTempSlider, &QSlider::valueChanged )
    SETUP_VFILTER_OPTION( hqdn3dChromaSpatSlider, &QSlider::valueChanged )
    SETUP_VFILTER_OPTION( hqdn3dChromaTempSlider, &QSlider::valueChanged )


    SETUP_VFILTER( anaglyph )

#undef SETUP_VFILTER
#undef SETUP_VFILTER_OPTION

    connect( ui.cropTopPx, QOverload<int>::of(&QSpinBox::valueChanged), this, &ExtVideo::cropChange );
    connect( ui.cropBotPx, QOverload<int>::of(&QSpinBox::valueChanged), this, &ExtVideo::cropChange );
    connect( ui.cropLeftPx, QOverload<int>::of(&QSpinBox::valueChanged), this, &ExtVideo::cropChange );
    connect( ui.cropRightPx, QOverload<int>::of(&QSpinBox::valueChanged), this, &ExtVideo::cropChange );
    connect( ui.leftRightCropSync, &QCheckBox::toggled, this, &ExtVideo::cropChange );
    connect( ui.topBotCropSync, &QCheckBox::toggled, this, &ExtVideo::cropChange );
    connect( ui.topBotCropSync, &QCheckBox::toggled,
             ui.cropBotPx, &QSpinBox::setDisabled );
    connect( ui.leftRightCropSync, &QCheckBox::toggled,
             ui.cropRightPx, &QSpinBox::setDisabled );

    /* The Video tab (DESIGN_SPEC 7): each adjust value beside its slider,
     * in the design's order, and their defaults back */
    QSlider *const adjust[] = { ui.brightnessSlider, ui.contrastSlider, ui.saturationSlider,
                                ui.hueSlider, ui.gammaSlider };
    const int digits = QFontMetrics( ui.adjustEnable->font() ).width( "-180.00" );
    QWidget *previous = ui.adjustEnable;
    for( size_t i = 0; i < sizeof( adjust ) / sizeof( adjust[0] ); i++ )
    {
        QSlider *slider = adjust[i];
        slider->setTickPosition( QSlider::NoTicks );
        QLabel *value = new QLabel( ui.adjustEnable );
        value->setProperty( "role", "hint" );
        value->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
        value->setMinimumWidth( digits );
        const auto show = [slider, value]( int v ) {
            const int ticks = slider->tickInterval();
            value->setText( ticks > 1 ? QString::number( v / double( ticks ), 'f', 2 )
                                      : QString::number( v ) );
        };
        show( slider->value() );
        connect( slider, &QSlider::valueChanged, value, show );
        int row, column, rowSpan, columnSpan;
        ui.gridLayout_6->getItemPosition( ui.gridLayout_6->indexOf( slider ),
                                          &row, &column, &rowSpan, &columnSpan );
        ui.gridLayout_6->addWidget( value, row, column + 1 );
        if( slider == ui.contrastSlider )
        {
            QWidget::setTabOrder( previous, ui.brightnessThresholdCheck );
            previous = ui.brightnessThresholdCheck;
        }
        QWidget::setTabOrder( previous, slider );
        previous = slider;
    }
    QPushButton *reset = new QPushButton( qtr( "Reset to defaults" ), ui.adjustEnable );
    reset->setProperty( "role", "outline" );
    ui.gridLayout_6->addWidget( reset, ui.gridLayout_6->rowCount(), 0, 1, -1, Qt::AlignLeft );
    QWidget::setTabOrder( previous, reset );
    connect( reset, &QPushButton::clicked, this, &ExtVideo::resetAdjust );
}

/* The adjust filter's options back to their defaults: each slider applies
 * its own as if moved (updateFilterOptions) */
void ExtVideo::resetAdjust()
{
    QObject *const widgets[] = { ui.brightnessSlider, ui.brightnessThresholdCheck,
                                 ui.contrastSlider, ui.saturationSlider,
                                 ui.hueSlider, ui.gammaSlider };
    for( size_t i = 0; i < sizeof( widgets ) / sizeof( widgets[0] ); i++ )
    {
        const module_config_t *p_item =
            config_FindConfig( qtu( OptionFromWidgetName( widgets[i] ) ) );
        if( p_item == NULL )
            continue;
        QSlider *slider = qobject_cast<QSlider *>( widgets[i] );
        QCheckBox *check = qobject_cast<QCheckBox *>( widgets[i] );
        if( slider )
            /* the conversion of setWidgetValue() */
            slider->setValue( p_item->i_type == CONFIG_ITEM_FLOAT
                ? ( int )( p_item->orig.f * ( double )slider->tickInterval() )
                : ( int )p_item->orig.i );
        else if( check )
            check->setChecked( p_item->orig.i != 0 );
    }
}

void ExtVideo::cropChange()
{
    if( ui.topBotCropSync->isChecked() )
        ui.cropBotPx->setValue( ui.cropTopPx->value() );
    if( ui.leftRightCropSync->isChecked() )
        ui.cropRightPx->setValue( ui.cropLeftPx->value() );

    QVector<vout_thread_t*> p_vouts = THEMIM->getVouts();
    foreach( vout_thread_t *p_vout, p_vouts )
    {
        var_SetInteger( p_vout, "crop-top", ui.cropTopPx->value() );
        var_SetInteger( p_vout, "crop-bottom", ui.cropBotPx->value() );
        var_SetInteger( p_vout, "crop-left", ui.cropLeftPx->value() );
        var_SetInteger( p_vout, "crop-right", ui.cropRightPx->value() );
        vlc_object_release( p_vout );
    }
}

void ExtVideo::clean()
{
    ui.cropTopPx->setValue( 0 );
    ui.cropBotPx->setValue( 0 );
    ui.cropLeftPx->setValue( 0 );
    ui.cropRightPx->setValue( 0 );
}

static QString ChangeFiltersString( struct intf_thread_t *p_intf, const char *psz_filter_type, const char *psz_name, bool b_add )
{
    char* psz_chain = var_GetString( THEPL, psz_filter_type );

    QString const chain = QString( psz_chain ? psz_chain : "" );
    QStringList list = chain.split( ':',
                                    #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                                        Qt::SkipEmptyParts
                                    #else
                                        QString::SkipEmptyParts
                                    #endif
                                  );

    /* playlist_EnableAudioFilter() copies the audio output's list, spectrum
     * tap included (SpectrumWidget::setTap), into the playlist variable read
     * above: keep it out of the string the Effects dialog saves */
    list.removeAll( "qt_spectrum" );

    if( b_add && std::find(list.begin(), list.end(), psz_name) == list.end() )
        list << psz_name;
    else if (!b_add)
        list.removeAll( psz_name );

#ifdef _WIN32
    /* VLC 3.x HACK: "adjust" d3d* filters can't work with other SW filters.
     * There is not way to fix it until VLC 4.0. As a workaround, force the
     * adjust filter to be added at the end of the list. Therefore the SW
     * "adjust" filter will be used since the previous filter will be SW. */
    if( b_add && strcmp( psz_filter_type, "video-filter" ) == 0
     && strcmp( psz_name, "adjust" ) != 0 )
    {
        QList<QString>::iterator it = std::find(list.begin(), list.end(), "adjust");
        if( it != list.end() )
        {
            list.erase(it);
            list << "adjust";
        }
    }
#endif

    free( psz_chain );

    return list.join( ":" );
}

static void UpdateVFiltersString( struct intf_thread_t *p_intf,
                                  const char *psz_filter_type, const char *value )
{
    var_SetString( THEPL, psz_filter_type, value );

    /* Try to set non splitter filters on the fly */
    if( strcmp( psz_filter_type, "video-splitter" ) )
    {
        QVector<vout_thread_t*> p_vouts = THEMIM->getVouts();
        foreach( vout_thread_t *p_vout, p_vouts )
        {
            var_SetString( p_vout, psz_filter_type, value );
            vlc_object_release( p_vout );
        }
    }
}

void ExtVideo::changeVFiltersString( const char *psz_name, bool b_add )
{
    const char *psz_filter_type = GetVFilterType( p_intf, psz_name );
    if( psz_filter_type == NULL )
        return;

    QString result = ChangeFiltersString( p_intf, psz_filter_type, psz_name, b_add );

    emit configChanged( qfu( psz_filter_type ), result );

    UpdateVFiltersString( p_intf, psz_filter_type, qtu( result ) );
}

void ExtVideo::updateFilters()
{
    QString module = ModuleFromWidgetName( sender() );

    QCheckBox *checkbox = qobject_cast<QCheckBox*>( sender() );
    QGroupBox *groupbox = qobject_cast<QGroupBox*>( sender() );

    changeVFiltersString( qtu( module ),
                          checkbox ? checkbox->isChecked()
                                   : groupbox->isChecked() );
}

#define UPDATE_AND_APPLY_TEXT( widget, file ) \
    QMetaObject::Connection connection = connect( ui.widget, &QLineEdit::textChanged, \
             this, &ExtVideo::updateFilterOptions ); \
    ui.widget->setText( toNativeSeparators( file ) ); \
    disconnect( connection );

void ExtVideo::browseLogo()
{
    const QStringList schemes = QStringList(QStringLiteral("file"));
    QString filter = QString( "%1 (*.png *.jpg);;%2 (*)" )
                        .arg( qtr("Image Files") )
                        .arg( TITLE_EXTENSIONS_ALL );
    QString file = QFileDialog::getOpenFileUrl( NULL, qtr( "Logo filenames" ),
                   p_intf->p_sys->filepath, filter,
                   NULL, QFileDialog::Options(), schemes ).toLocalFile();

    UPDATE_AND_APPLY_TEXT( logoFileText, file );
}

void ExtVideo::browseEraseFile()
{
    const QStringList schemes = QStringList(QStringLiteral("file"));
    QString filter = QString( "%1 (*.png *.jpg);;%2 (*)" )
                        .arg( qtr("Image Files") )
                        .arg( TITLE_EXTENSIONS_ALL );
    QString file = QFileDialog::getOpenFileUrl( NULL, qtr( "Image mask" ),
                   p_intf->p_sys->filepath, filter,
                   NULL, QFileDialog::Options(), schemes ).toLocalFile();

    UPDATE_AND_APPLY_TEXT( eraseMaskText, file );
}

#undef UPDATE_AND_APPLY_TEXT

void ExtVideo::initComboBoxItems( QObject *widget )
{
    QComboBox *combobox = qobject_cast<QComboBox*>( widget );
    if( !combobox ) return;

    QString option = OptionFromWidgetName( widget );
    module_config_t *p_item = config_FindConfig( qtu( option ) );
    if( p_item == NULL )
    {
        msg_Err( p_intf, "Couldn't find option \"%s\".", qtu( option ) );
        return;
    }

    if( p_item->i_type == CONFIG_ITEM_INTEGER
     || p_item->i_type == CONFIG_ITEM_BOOL )
    {
        int64_t *values;
        char **texts;
        ssize_t count = config_GetIntChoices( VLC_OBJECT( p_intf ),
                                              qtu( option ), &values, &texts );
        for( ssize_t i = 0; i < count; i++ )
        {
            combobox->addItem( qtr( texts[i] ), qlonglong(values[i]) );
            free( texts[i] );
        }
        free( texts );
        free( values );
    }
    else if( p_item->i_type == CONFIG_ITEM_STRING )
    {
        char **values;
        char **texts;
        ssize_t count = config_GetPszChoices( VLC_OBJECT( p_intf ),
                                              qtu( option ), &values, &texts );
        for( ssize_t i = 0; i < count; i++ )
        {
            combobox->addItem( qtr( texts[i] ), qfu(values[i]) );
            free( texts[i] );
            free( values[i] );
        }
        free( texts );
        free( values );
    }
}

void ExtVideo::setWidgetValue( QObject *widget )
{
    QString module = ModuleFromWidgetName( widget->parent() );
    //std::cout << "Module name: " << module.toStdString() << std::endl;
    QString option = OptionFromWidgetName( widget );
    //std::cout << "Option name: " << option.toStdString() << std::endl;

    vlc_value_t val;
    int i_type = config_GetType( qtu( option ) ) & VLC_VAR_CLASS;
    switch( i_type )
    {
        case VLC_VAR_INTEGER:
        case VLC_VAR_BOOL:
        case VLC_VAR_FLOAT:
        case VLC_VAR_STRING:
            break;
        default:
            msg_Err( p_intf,
                     "Module %s's %s variable is of an unsupported type ( %d )",
                     qtu( module ), qtu( option ), i_type );
            return;
    }
    if( var_Create( THEPL, qtu( option ), i_type | VLC_VAR_DOINHERIT ) )
        return;
    if( var_GetChecked( THEPL, qtu( option ), i_type, &val ) )
        return;

    /* Try to cast to all the widgets we're likely to encounter. Only
     * one of the casts is expected to work. */
    QSlider        *slider        = qobject_cast<QSlider*>       ( widget );
    QCheckBox      *checkbox      = qobject_cast<QCheckBox*>     ( widget );
    QSpinBox       *spinbox       = qobject_cast<QSpinBox*>      ( widget );
    QDoubleSpinBox *doublespinbox = qobject_cast<QDoubleSpinBox*>( widget );
    VLCQDial       *dial          = qobject_cast<VLCQDial*>      ( widget );
    QLineEdit      *lineedit      = qobject_cast<QLineEdit*>     ( widget );
    QComboBox      *combobox      = qobject_cast<QComboBox*>     ( widget );

    if( i_type == VLC_VAR_INTEGER || i_type == VLC_VAR_BOOL )
    {
        if( slider )        slider->setValue( val.i_int );
        else if( checkbox ) checkbox->setCheckState( val.i_int? Qt::Checked
                                                              : Qt::Unchecked );
        else if( spinbox )  spinbox->setValue( val.i_int );
        else if( dial )     dial->setValue( (360 - val.i_int) % 360 );
        else if( lineedit )
        {
            char str[30];
            snprintf( str, sizeof(str), "%06" PRIX64, val.i_int );
            lineedit->setText( str );
        }
        else if( combobox ) combobox->setCurrentIndex(
                            combobox->findData( qlonglong(val.i_int) ) );
        else msg_Warn( p_intf, "Could not find the correct Integer widget" );
    }
    else if( i_type == VLC_VAR_FLOAT )
    {
        if( slider ) slider->setValue( ( int )( val.f_float*( double )slider->tickInterval() ) ); /* hack alert! */
        else if( doublespinbox ) doublespinbox->setValue( val.f_float );
        else if( dial ) dial->setValue( (360 - lroundf(val.f_float)) % 360 );
        else msg_Warn( p_intf, "Could not find the correct Float widget" );
    }
    else if( i_type == VLC_VAR_STRING )
    {
        if( lineedit ) lineedit->setText( qfu( val.psz_string ) );
        else if( combobox ) combobox->setCurrentIndex(
                            combobox->findData( qfu( val.psz_string ) ) );
        else msg_Warn( p_intf, "Could not find the correct String widget" );
        free( val.psz_string );
    }
}

void ExtVideo::setFilterOption( const char *psz_module, const char *psz_option,
        int i_int, double f_float, const char *psz_string )
{
    QVector<vout_thread_t*> p_vouts = THEMIM->getVouts();
    int i_type = 0;
    bool b_is_command = false;

    if( !p_vouts.isEmpty() )
    {
        i_type = var_Type( p_vouts.at(0), psz_option );
        b_is_command = ( i_type & VLC_VAR_ISCOMMAND );
    }
    if( i_type == 0 )
        i_type = config_GetType( psz_option );

    vlc_value_t val;
    i_type &= VLC_VAR_CLASS;
    if( i_type == VLC_VAR_INTEGER || i_type == VLC_VAR_BOOL )
    {
        emit configChanged( qfu( psz_option ), QVariant( i_int ) );
        if( i_type == VLC_VAR_INTEGER )
        {
            val.i_int = i_int;
            var_SetInteger( THEPL, psz_option, i_int );
        }
        else
        {
            var_SetBool( THEPL, psz_option, i_int );
            val.b_bool = i_int;
        }
    }
    else if( i_type == VLC_VAR_FLOAT )
    {
        emit configChanged( qfu( psz_option ), QVariant( f_float ) );
        var_SetFloat( THEPL, psz_option, f_float );
        val.f_float = f_float;
    }
    else if( i_type == VLC_VAR_STRING )
    {
        if( psz_string == NULL )
            psz_string = "";
        emit configChanged( qfu( psz_option ), QVariant( psz_string ) );
        var_SetString( THEPL, psz_option, psz_string );
        val.psz_string = (char *) psz_string;
    }
    else
    {
        msg_Err( p_intf,
                 "Module %s's %s variable is of an unsupported type ( %d )",
                 psz_module,
                 psz_option,
                 i_type );
        b_is_command = false;
    }

    if( b_is_command )
    {
        foreach( vout_thread_t *p_vout, p_vouts )
        {
            var_SetChecked( p_vout, psz_option, i_type, val );
#ifndef NDEBUG
            int i_cur_type = var_Type( p_vout, psz_option );
            assert( ( i_cur_type & VLC_VAR_CLASS ) == i_type );
            assert( !!( i_cur_type & VLC_VAR_ISCOMMAND ) == b_is_command );
#endif
        }
    }

    foreach( vout_thread_t *p_vout, p_vouts )
        vlc_object_release( p_vout );
}

void ExtVideo::updateFilterOptions()
{
    QString module = ModuleFromWidgetName( sender()->parent() );
    //msg_Dbg( p_intf, "Module name: %s", qtu( module ) );
    QString option = OptionFromWidgetName( sender() );
    //msg_Dbg( p_intf, "Option name: %s", qtu( option ) );

    /* Try to cast to all the widgets we're likely to encounter. Only
     * one of the casts is expected to work. */
    QSlider        *slider        = qobject_cast<QSlider*>       ( sender() );
    QCheckBox      *checkbox      = qobject_cast<QCheckBox*>     ( sender() );
    QSpinBox       *spinbox       = qobject_cast<QSpinBox*>      ( sender() );
    QDoubleSpinBox *doublespinbox = qobject_cast<QDoubleSpinBox*>( sender() );
    VLCQDial       *dial          = qobject_cast<VLCQDial*>      ( sender() );
    QLineEdit      *lineedit      = qobject_cast<QLineEdit*>     ( sender() );
    QComboBox      *combobox      = qobject_cast<QComboBox*>     ( sender() );

    int i_int = -1;
    double f_float = -1.;
    QString val;

    if( slider ) {
        i_int = slider->value();
        f_float = ( double )slider->value() / ( double )slider->tickInterval(); /* hack alert! */
    }
    else if( checkbox ) i_int = checkbox->checkState() == Qt::Checked;
    else if( spinbox ) i_int = spinbox->value();
    else if( doublespinbox ) f_float = doublespinbox->value();
    else if( dial ) {
        i_int = (360 - dial->value()) % 360;
        f_float = i_int;
    }
    else if( lineedit ) {
        i_int = lineedit->text().toInt( NULL,16 );
        f_float = lineedit->text().toDouble();
        val = lineedit->text();
    }
    else if( combobox ) {
        i_int = combobox->itemData( combobox->currentIndex() ).toInt();
        val = combobox->itemData( combobox->currentIndex() ).toString();
    }

    setFilterOption( qtu( module ), qtu( option ), i_int, f_float, qtu( val ) );
}

/**********************************************************************
 * v4l2 controls
 **********************************************************************/

ExtV4l2::ExtV4l2( intf_thread_t *_p_intf, QWidget *_parent )
    : QWidget( _parent ), p_intf( _p_intf ), box( NULL )
{
    QVBoxLayout *layout = new QVBoxLayout( this );
    help = new QLabel( qtr("No v4l2 instance found.\n"
      "Please check that the device has been opened with VLC and is playing.\n\n"
      "Controls will automatically appear here.")
      , this );
    help->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
    help->setWordWrap( true );
    layout->addWidget( help );
    setLayout( layout );
}

void ExtV4l2::showEvent( QShowEvent *event )
{
    QWidget::showEvent( event );
    Refresh();
}

void ExtV4l2::Refresh( void )
{
    vlc_object_t *p_obj = (vlc_object_t*)vlc_object_find_name( THEPL, "v4l2" );
    help->hide();
    if( box )
    {
        layout()->removeWidget( box );
        delete box;
        box = NULL;
    }
    if( p_obj )
    {
        vlc_value_t val, text;
        int i_ret = var_Change( p_obj, "controls", VLC_VAR_GETCHOICES,
                                &val, &text );
        if( i_ret < 0 )
        {
            msg_Err( p_intf, "Oops, v4l2 object doesn't have a 'controls' variable." );
            help->show();
            vlc_object_release( p_obj );
            return;
        }

        box = new QGroupBox( this );
        layout()->addWidget( box );
        QVBoxLayout *layout = new QVBoxLayout( box );
        box->setLayout( layout );

        for( int i = 0; i < val.p_list->i_count; i++ )
        {
            vlc_value_t vartext;
            const char *psz_var = text.p_list->p_values[i].psz_string;

            if( var_Change( p_obj, psz_var, VLC_VAR_GETTEXT, &vartext, NULL ) )
                continue;

            QString name = qtr( vartext.psz_string );
            free( vartext.psz_string );
            msg_Dbg( p_intf, "v4l2 control \"%" PRIx64 "\": %s (%s)",
                     val.p_list->p_values[i].i_int, psz_var, qtu( name ) );

            int i_type = var_Type( p_obj, psz_var );
            switch( i_type & VLC_VAR_TYPE )
            {
                case VLC_VAR_INTEGER:
                {
                    QLabel *label = new QLabel( name, box );
                    QHBoxLayout *hlayout = new QHBoxLayout();
                    hlayout->addWidget( label );
                    int i_val = var_GetInteger( p_obj, psz_var );
                    if( i_type & VLC_VAR_HASCHOICE )
                    {
                        QComboBox *combobox = new QComboBox( box );
                        combobox->setObjectName( qfu( psz_var ) );

                        vlc_value_t val2, text2;
                        var_Change( p_obj, psz_var, VLC_VAR_GETCHOICES,
                                    &val2, &text2 );
                        for( int j = 0; j < val2.p_list->i_count; j++ )
                        {
                            combobox->addItem(
                                       text2.p_list->p_values[j].psz_string,
                                       qlonglong( val2.p_list->p_values[j].i_int) );
                            if( i_val == val2.p_list->p_values[j].i_int )
                                combobox->setCurrentIndex( j );
                        }
                        var_FreeList( &val2, &text2 );

                        connect( combobox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                                 QOverload<int>::of(&ExtV4l2::ValueChange) );
                        hlayout->addWidget( combobox );
                    }
                    else
                    {
                        QSlider *slider = new QSlider( box );
                        slider->setObjectName( qfu( psz_var ) );
                        slider->setOrientation( Qt::Horizontal );
                        vlc_value_t val2;
                        var_Change( p_obj, psz_var, VLC_VAR_GETMIN,
                                    &val2, NULL );
                        if( val2.i_int < INT_MIN )
                            val2.i_int = INT_MIN; /* FIXME */
                        slider->setMinimum( val2.i_int );
                        var_Change( p_obj, psz_var, VLC_VAR_GETMAX,
                                    &val2, NULL );
                        if( val2.i_int > INT_MAX )
                            val2.i_int = INT_MAX; /* FIXME */
                        slider->setMaximum( val2.i_int );
                        if( !var_Change( p_obj, psz_var, VLC_VAR_GETSTEP,
                                         &val2, NULL ) )
                            slider->setSingleStep( val2.i_int );
                        slider->setValue( i_val );
                        connect( slider, &QSlider::valueChanged, this,
                                 QOverload<int>::of(&ExtV4l2::ValueChange) );
                        hlayout->addWidget( slider );
                    }
                    layout->addLayout( hlayout );
                    break;
                }
                case VLC_VAR_BOOL:
                {
                    QCheckBox *button = new QCheckBox( name, box );
                    button->setObjectName( qfu( psz_var ) );
                    button->setChecked( var_GetBool( p_obj, psz_var ) );

                    connect( button, &QCheckBox::clicked, this,
                             QOverload<bool>::of(&ExtV4l2::ValueChange) );
                    layout->addWidget( button );
                    break;
                }
                case VLC_VAR_VOID:
                {
                    if( i_type & VLC_VAR_ISCOMMAND )
                    {
                        QPushButton *button = new QPushButton( name, box );
                        button->setObjectName( qfu( psz_var ) );

                        connect( button, &QPushButton::clicked, this,
                                 QOverload<bool>::of(&ExtV4l2::ValueChange) );
                        layout->addWidget( button );
                    }
                    else
                    {
                        QLabel *label = new QLabel( name, box );
                        layout->addWidget( label );
                    }
                    break;
                }
                default:
                    msg_Warn( p_intf, "Unhandled var type for %s", psz_var );
                    break;
            }
        }
        var_FreeList( &val, &text );
        vlc_object_release( p_obj );
    }
    else
    {
        msg_Dbg( p_intf, "Couldn't find v4l2 instance" );
        help->show();
        if ( isVisible() )
            QTimer::singleShot( 2000, this, SLOT(Refresh()) );
    }
}

void ExtV4l2::ValueChange( bool value )
{
    ValueChange( (int)value );
}

void ExtV4l2::ValueChange( int value )
{
    QObject *s = sender();
    vlc_object_t *p_obj = (vlc_object_t*)vlc_object_find_name( THEPL, "v4l2" );
    if( p_obj )
    {
        QString var = s->objectName();
        int i_type = var_Type( p_obj, qtu( var ) );
        switch( i_type & VLC_VAR_TYPE )
        {
            case VLC_VAR_INTEGER:
                if( i_type & VLC_VAR_HASCHOICE )
                {
                    QComboBox *combobox = qobject_cast<QComboBox*>( s );
                    value = combobox->itemData( value ).toInt();
                }
                var_SetInteger( p_obj, qtu( var ), value );
                break;
            case VLC_VAR_BOOL:
                var_SetBool( p_obj, qtu( var ), value );
                break;
            case VLC_VAR_VOID:
                var_TriggerCallback( p_obj, qtu( var ) );
                break;
        }
        vlc_object_release( p_obj );
    }
    else
    {
        msg_Warn( p_intf, "Oops, v4l2 object isn't available anymore" );
        Refresh();
    }
}

/**********************************************************************
 * Sliders
 **********************************************************************/

FilterSliderData::FilterSliderData( QObject *parent, QSlider *_slider ) :
    QObject( parent ), slider( _slider )
{
}

FilterSliderData::FilterSliderData( QObject *parent,
                                    intf_thread_t *_p_intf,
                                    QSlider *_slider,
                                    QLabel *_label, QLabel *_nameLabel,
                                    const slider_data_t *_p_data ):
    QObject( parent ), slider( _slider ), valueLabel( _label ),
    nameLabel( _nameLabel ), p_data( _p_data ), p_intf( _p_intf )
{
    slider->setMinimum( p_data->f_min / p_data->f_resolution );
    slider->setMaximum( p_data->f_max / p_data->f_resolution );
    nameLabel->setText( p_data->descs );
    slider->setAccessibleName( QString( p_data->descs ).replace( QLatin1Char( '\n' ), QLatin1Char( ' ' ) ) );
    connect( slider, &QSlider::valueChanged, this, &FilterSliderData::updateText );
    setValue( initialValue() );
    /* In case current == min|max text would not be first updated */
    if ( slider->value() == slider->maximum() ||
         slider->value() == slider->minimum() )
        updateText( slider->value() );
    connect( slider, &QSlider::valueChanged, this, &FilterSliderData::onValueChanged );
}

void FilterSliderData::setValue( float f )
{
    slider->setValue( f / p_data->f_resolution );
}

void FilterSliderData::updateText( int i )
{
    float f = ((float) i) * p_data->f_resolution * p_data->f_visual_multiplier;
    valueLabel->setText( QString( p_data->units )
                    .prepend( "%1 " )
                    .arg( QString::number( f, 'f', 1 ) ) );
}

float FilterSliderData::initialValue()
{
    vlc_object_t *p_aout = (vlc_object_t *) THEMIM->getAout();
    float f = p_data->f_value;
    if( p_aout )
    {
        if ( var_Type( p_aout, qtu(p_data->name) ) == 0 )
        {
            vlc_object_release( p_aout );
            /* Not found, will try in config */
        }
        else
        {
            f = var_GetFloat( p_aout, qtu(p_data->name) );
            vlc_object_release( p_aout );
            return f;
        }
    }

    if ( ! config_FindConfig( qtu(p_data->name) ) )
        return f;

    f = config_GetFloat( p_intf, qtu(p_data->name) );
    return f;
}

void FilterSliderData::onValueChanged( int i )
{
    float f = ((float) i) * p_data->f_resolution;
    vlc_object_t *p_aout = (vlc_object_t *) THEMIM->getAout();
    if ( p_aout )
    {
        var_SetFloat( p_aout, qtu(p_data->name), f );
        vlc_object_release( p_aout );
    }
    writeToConfig();
}

void FilterSliderData::writeToConfig()
{
    float f = ((float) slider->value()) * p_data->f_resolution;
    emit configChanged( p_data->name, QVariant( f ) );
}

AudioFilterControlWidget::AudioFilterControlWidget
( intf_thread_t *_p_intf, QWidget *parent, const char *_name ) :
    QWidget( parent ), slidersBox( NULL ), p_intf( _p_intf ), name( _name ),
    i_smallfont(0)
{}

void AudioFilterControlWidget::connectConfigChanged( FilterSliderData *slider )
{
    connect( slider, &FilterSliderData::configChanged,
             this, &AudioFilterControlWidget::configChanged );
}

void AudioFilterControlWidget::build()
{
    QFont smallFont = QApplication::font();
    smallFont.setPointSize( smallFont.pointSize() + i_smallfont );

    QVBoxLayout *layout = new QVBoxLayout( this );
    slidersBox = new QGroupBox( qtr( "Enable" ) );
    slidersBox->setCheckable( true );
    layout->addWidget( slidersBox );

    QGridLayout *ctrlLayout = new QGridLayout( slidersBox );

    int i = 0;
    foreach( const FilterSliderData::slider_data_t &data, controls )
    {
        QSlider *slider = new QSlider( Qt::Vertical );
        QLabel *valueLabel = new QLabel();
        valueLabel->setFont( smallFont );
        valueLabel->setAlignment( Qt::AlignHCenter );
        QLabel *nameLabel = new QLabel();
        nameLabel->setFont( smallFont );
        nameLabel->setAlignment( Qt::AlignHCenter );
        FilterSliderData *filter =
            new FilterSliderData( this, p_intf,
                                  slider, valueLabel, nameLabel, & data );
        ctrlLayout->addWidget( slider, 0, i, Qt::AlignHCenter );
        ctrlLayout->addWidget( valueLabel, 1, i, Qt::AlignHCenter );
        ctrlLayout->addWidget( nameLabel, 2, i, Qt::AlignHCenter );
        i++;
        sliderDatas << filter;
        connectConfigChanged( filter );
    }

    char *psz_af = var_InheritString( THEPL, "audio-filter" );

    if( psz_af && filterIsPresent( qfu(psz_af), name ) )
        slidersBox->setChecked( true );
    else
        slidersBox->setChecked( false );
    connect( slidersBox, &QGroupBox::toggled, this, &AudioFilterControlWidget::enable );

    free( psz_af );
}

void AudioFilterControlWidget::enable( bool b_enable )
{
    module_t *p_obj = module_find( qtu(name) );
    if( !p_obj )
    {
        msg_Err( p_intf, "Unable to find filter module \"%s\".", qtu(name) );
        return;
    }

    QString result = ChangeFiltersString( p_intf, "audio-filter", qtu(name),
                                          b_enable );
    emit configChanged( qfu("audio-filter"), result );
    playlist_EnableAudioFilter( THEPL, qtu(name), b_enable );
}

/**********************************************************************
 * Equalizer
 **********************************************************************/

EqualizerSliderData::EqualizerSliderData( QObject *parent, intf_thread_t *_p_intf,
                                          QSlider *slider, QLabel *_label,
                                          QLabel *_nameLabel, const slider_data_t *_p_data,
                                          int _index )
    : FilterSliderData( parent, slider ), index( _index )
{
    p_intf = _p_intf;
    valueLabel = _label;
    nameLabel = _nameLabel;
    p_data = _p_data;

    slider->setMinimum( p_data->f_min / p_data->f_resolution );
    slider->setMaximum( p_data->f_max / p_data->f_resolution );
    nameLabel->setText( p_data->descs );
    connect( slider, &QSlider::valueChanged, this, &EqualizerSliderData::updateText );
    setValue( initialValue() );
    updateText( slider->value() );
    connect( slider, &QSlider::valueChanged, this, &EqualizerSliderData::onValueChanged );
}

QStringList EqualizerSliderData::getBandsFromAout() const
{
    vlc_object_t *p_aout = (vlc_object_t *) THEMIM->getAout();
    QStringList bands;
    if( p_aout )
    {
        if ( var_Type( p_aout, qtu(p_data->name) ) == VLC_VAR_STRING )
        {
            char *psz_bands = var_GetString( p_aout, qtu(p_data->name) );
            if ( psz_bands )
            {
                bands = QString( psz_bands ).split( " ",
                                                    #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                                                        Qt::SkipEmptyParts
                                                    #else
                                                        QString::SkipEmptyParts
                                                    #endif
                                                  );
                free( psz_bands );
            }
        }
        vlc_object_release( p_aout );
    }

    if ( bands.count() ) return bands;
    /* Or try config then */

    if ( ! config_FindConfig( qtu(p_data->name) ) )
        return bands;

    char *psz_bands = config_GetPsz( p_intf, qtu(p_data->name) );
    if ( psz_bands )
    {
        bands = QString( psz_bands ).split( " ",
                                            #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                                                Qt::SkipEmptyParts
                                            #else
                                                QString::SkipEmptyParts
                                            #endif
                                          );
        free( psz_bands );
    }

    return bands;
}

float EqualizerSliderData::initialValue()
{
    float f = p_data->f_value;
    QStringList bands = getBandsFromAout();

    if ( bands.count() > index )
        f = QLocale( QLocale::C ).toFloat( bands[ index ] );

    return f;
}

void EqualizerSliderData::onValueChanged( int i )
{
    QStringList bands = getBandsFromAout();
    if ( bands.count() > index )
    {
        float f = ((float) i) * p_data->f_resolution;
        bands[ index ] = QLocale( QLocale::C ).toString( f );
        vlc_object_t *p_aout = (vlc_object_t *) THEMIM->getAout();
        if ( p_aout )
        {
            var_SetString( p_aout, qtu(p_data->name), qtu(bands.join( " " )) );
            vlc_object_release( p_aout );
        }
        writeToConfig();
    }
}

void EqualizerSliderData::writeToConfig()
{
    QStringList bands = getBandsFromAout();
    if ( bands.count() > index )
    {
        float f = (float) slider->value() * p_data->f_resolution;
        bands[ index ] = QLocale( QLocale::C ).toString( f );
        emit configChanged( p_data->name, QVariant( bands.join( " " ) ) );
    }
}

Equalizer::Equalizer( intf_thread_t *p_intf, QWidget *parent )
    : AudioFilterControlWidget( p_intf, parent, "equalizer" )
{
    i_smallfont = -3;
    bool b_vlcBands = var_InheritBool( p_intf, "equalizer-vlcfreqs" );

    const FilterSliderData::slider_data_t vlc_bands[10] =
    {
        { "equalizer-bands", qtr("60 Hz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("170 Hz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("310 Hz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("600 Hz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("1 KHz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("3 KHz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("6 KHz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("12 KHz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("14 KHz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("16 KHz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
    };
    const FilterSliderData::slider_data_t iso_bands[10] =
    {
        { "equalizer-bands", qtr("31 Hz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("63 Hz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("125 Hz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("250 Hz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("500 Hz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("1 KHz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("2 KHz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("4 KHz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("8 KHz"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
        { "equalizer-bands", qtr("16 KHz"), qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 },
    };
    const FilterSliderData::slider_data_t preamp_vals =
        { "equalizer-preamp", qtr("Preamp"),  qtr("dB"), -20.0f, 20.0f, 0.0f, 0.1f, 1.0 };

    for( int i=0; i<10 ;i++ ) controls.append( (b_vlcBands) ? vlc_bands[i] : iso_bands[i] );
    preamp_values = preamp_vals;
    build();
}

namespace
{

/* An equalizer band (DESIGN_SPEC 7): the track, the 0 line, the accent from
 * the 0 line to the knob. It paints itself: `.QSlider` rules match only the
 * exact class. A press jumps to the pointer, like the prototype's bands. */
class EqBandSlider : public QSlider
{
public:
    EqBandSlider() : QSlider( Qt::Vertical ) {}

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    void mousePressEvent( QMouseEvent *e ) Q_DECL_OVERRIDE
    {
        if( e->button() != Qt::LeftButton )
        {
            QSlider::mousePressEvent( e );
            return;
        }
        setSliderDown( true );
        seek( e->pos().y() );
    }
    void mouseMoveEvent( QMouseEvent *e ) Q_DECL_OVERRIDE
    {
        if( isSliderDown() )
            seek( e->pos().y() );
    }
    void mouseReleaseEvent( QMouseEvent *e ) Q_DECL_OVERRIDE
    {
        if( isSliderDown() )
            setSliderDown( false );
        else
            QSlider::mouseReleaseEvent( e );
    }

private:
    int knob() const { return Theme::instance()->px( RedesignTokens::Effects::BandKnob ); }
    int span() const { return height() - knob(); }
    qreal yOf( int v ) const
    {
        return knob() / 2. + QStyle::sliderPositionFromValue( minimum(), maximum(), v, span(), true );
    }
    void seek( int y )
    {
        setValue( QStyle::sliderValueFromPosition( minimum(), maximum(), y - knob() / 2,
                                                    span(), true ) );
    }
};

void EqBandSlider::paintEvent( QPaintEvent * )
{
    using namespace RedesignTokens;
    Theme *t = Theme::instance();
    QPainter p( this );
    p.setRenderHint( QPainter::Antialiasing );
    if( !isEnabled() )
        p.setOpacity( Effects::DisabledAlpha / 255. );
    const qreal track = t->px( Effects::BandTrack );
    const qreal radius = knob() / 2.;
    const qreal cx = width() / 2.;
    const QRectF groove( cx - track / 2, radius, track, span() );
    p.setPen( Qt::NoPen );
    p.setBrush( t->ink( InkAlpha::Track ) );
    p.drawRoundedRect( groove, track / 2, track / 2 );
    const qreal zero = yOf( qBound( minimum(), 0, maximum() ) );
    p.fillRect( QRectF( 0, zero, width(), Size::Hairline ), t->ink( InkAlpha::Track ) );
    const qreal y = yOf( value() );
    p.setBrush( t->color( Theme::Accent ) );
    p.drawRoundedRect( QRectF( groove.x(), qMin( y, zero ), track, qAbs( y - zero ) ),
                       track / 2, track / 2 );
    if( hasFocus() )
    {
        /* the accent is 2.4:1 on light surfaces */
        p.setPen( QPen( t->color( t->isDark() ? Theme::Accent : Theme::AccentText ),
                        Size::FocusRing ) );
        p.setBrush( Qt::NoBrush );
        p.drawEllipse( QPointF( cx, y ), radius + Size::FocusRing, radius + Size::FocusRing );
    }
    p.setPen( Qt::NoPen );
    p.setBrush( t->color( Theme::Text ) );
    p.drawEllipse( QPointF( cx, y ), radius, radius );
}

/* The preset chips, wrapped in rows as wide as the sheet. The height
 * follows the width through the minimum height, set on each resize. */
class ChipFlow : public QWidget
{
public:
    explicit ChipFlow( QWidget *parent ) : QWidget( parent ) {}
    QSize minimumSizeHint() const Q_DECL_OVERRIDE
    {
        int w = 0;
        foreach( const QWidget *chip, chips() )
            w = qMax( w, chip->sizeHint().width() );
        return QSize( w, minimumHeight() );
    }

protected:
    void resizeEvent( QResizeEvent * ) Q_DECL_OVERRIDE
    {
        setMinimumHeight( arrange( width() ) );
    }

private:
    QList<QWidget *> chips() const
    {
        return findChildren<QWidget *>( QString(), Qt::FindDirectChildrenOnly );
    }
    /* places the chips, returns the height they take */
    int arrange( int w ) const
    {
        const int gap = Theme::instance()->px( RedesignTokens::Effects::ChipGap );
        int x = 0, y = 0, rowH = 0;
        foreach( QWidget *chip, chips() )
        {
            /* sizeHint() leaves out the fixed ChipH height */
            const QSize s = chip->sizeHint().expandedTo( chip->minimumSize() );
            if( x > 0 && x + s.width() > w )
            {
                x = 0;
                y += rowH + gap;
                rowH = 0;
            }
            chip->setGeometry( QStyle::visualRect( layoutDirection(), rect(),
                                                   QRect( QPoint( x, y ), s ) ) );
            x += s.width() + gap;
            rowH = qMax( rowH, s.height() );
        }
        return y + rowH;
    }
};

}

void Equalizer::build()
{
    using namespace RedesignTokens;
    Theme *t = Theme::instance();

    Ui::EqualizerWidget ui;
    ui.setupUi( this );
    ui.mainLayout->setContentsMargins( 0, 0, 0, 0 );
    ui.mainLayout->setSpacing( t->px( Effects::SectionGap ) );
    ui.presetLabel->setProperty( "role", "hint" );
    presetName = ui.presetName;
    /* drawn as a switch by ThemeStyle */
    ui.enableCheck->setProperty( "role", "toggle" );

    /* set up preamp control */
    ui.preampLabel->setProperty( "role", "hint" );
    preampSlider = ui.preampSlider;
    preamp = new FilterSliderData( this, p_intf,
        ui.preampSlider, ui.preampValue, ui.preampLabel, & preamp_values );
    connectConfigChanged( preamp );

    /* the presets VLC knows, as chips */
    QVBoxLayout *chipsLayout = new QVBoxLayout( ui.presetsPlaceholder );
    chipsLayout->setContentsMargins( 0, 0, 0, 0 );
    ChipFlow *flow = new ChipFlow( ui.presetsPlaceholder );
    flow->setAccessibleName( qtr( "Preset" ) );
    chipsLayout->addWidget( flow );
    QWidget *previous = ui.enableCheck;
    for( int i = 0 ; i < NB_PRESETS ; i ++ )
    {
        QPushButton *chip = new QPushButton( qtr( preset_list_text[i] ), flow );
        chip->setProperty( "role", "chip" );
        chip->setCheckable( true );
        chip->setFixedHeight( t->metric( Theme::ChipH ) );
        connect( chip, &QPushButton::clicked, this, [this, i]() {
            setCorePreset( i + 1 );   /* its 1st entry was "custom" */
            updatePreset();           /* the bands may already match */
        } );
        setTabOrder( previous, chip );
        previous = chip;
        chips << chip;
    }

    /* the bands, with a +20 / 0 / -20 scale */
    QGridLayout *ctrlLayout = new QGridLayout( ui.slidersPlaceholder );
    ctrlLayout->setContentsMargins( 0, 0, 0, 0 );
    ctrlLayout->setHorizontalSpacing( t->px( Effects::BandGap ) );
    if( !controls.isEmpty() )
    {
        QVBoxLayout *scale = new QVBoxLayout;
        scale->setSpacing( 0 );
        const float marks[] = { controls[0].f_max, 0.f, controls[0].f_min };
        for( size_t m = 0; m < sizeof( marks ) / sizeof( marks[0] ); m++ )
        {
            if( m > 0 )
                scale->addStretch( 1 );
            QLabel *mark = new QLabel( marks[m] > 0 ? QString( "+%1" ).arg( marks[m] )
                                                    : QString::number( marks[m] ) );
            mark->setFont( t->font( Theme::Meta ) );
            mark->setProperty( "role", "libraryDim" );
            mark->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
            mark->setMinimumWidth( t->px( Effects::BandScaleW ) );
            scale->addWidget( mark );
        }
        QWidget *scaleBox = new QWidget( ui.slidersPlaceholder );
        scaleBox->setLayout( scale );
        scaleBox->setFixedHeight( t->px( Effects::BandH ) );
        scale->setContentsMargins( 0, 0, 0, 0 );
        ctrlLayout->addWidget( scaleBox, 0, 0 );
    }
    int i = 0;
    QString baseUnit;
    foreach( const FilterSliderData::slider_data_t &data, controls )
    {
        EqBandSlider *slider = new EqBandSlider;
        slider->setFixedHeight( t->px( Effects::BandH ) );
        slider->setMinimumWidth( t->px( Effects::BandKnob ) + 2 * Size::FocusRing );
        slider->setAccessibleName( data.descs );
        /* the value stays in the slider's accessible value and tooltip */
        QLabel *valueLabel = new QLabel( ui.slidersPlaceholder );
        valueLabel->hide();
        QLabel *nameLabel = new QLabel();
        nameLabel->setFont( t->font( Theme::Meta ) );
        nameLabel->setProperty( "role", "hint" );
        nameLabel->setAlignment( Qt::AlignHCenter );
        EqualizerSliderData *filter =
            new EqualizerSliderData( this, p_intf,
                                     slider, valueLabel, nameLabel, & data, i );
        /* one line per band (1.5.1): "60", "1K", the unit once under the
         * scale; the slider's name keeps the full "1 KHz" */
        const QString number = data.descs.section( QLatin1Char( ' ' ), 0, 0 );
        const QString unit = data.descs.section( QLatin1Char( ' ' ), 1 );
        if( !unit.isEmpty() )
        {
            if( baseUnit.isEmpty() )
                baseUnit = unit;
            nameLabel->setText( unit == baseUnit ? number : number + unit.left( 1 ) );
        }
        slider->setToolTip( valueLabel->text() );
        connect( slider, &QSlider::valueChanged, slider,
                 [slider, valueLabel]() { slider->setToolTip( valueLabel->text() ); } );
        ctrlLayout->addWidget( slider, 0, i + 1, Qt::AlignHCenter );
        ctrlLayout->addWidget( nameLabel, 1, i + 1, Qt::AlignHCenter );
        setTabOrder( previous, slider );
        previous = slider;
        sliderDatas << filter; /* keep track for applying presets */
        bandSliders << slider;
        connect( slider, &QSlider::valueChanged, this, &Equalizer::updatePreset );
        i++;
        connectConfigChanged( filter );
    }
    if( !baseUnit.isEmpty() )
    {
        QLabel *unitLabel = new QLabel( baseUnit, ui.slidersPlaceholder );
        unitLabel->setFont( t->font( Theme::Meta ) );
        unitLabel->setProperty( "role", "libraryDim" );
        unitLabel->setAlignment( Qt::AlignRight );
        ctrlLayout->addWidget( unitLabel, 1, 0 );
    }
    setTabOrder( previous, ui.preampSlider );
    setTabOrder( ui.preampSlider, ui.eq2PassCheck );
    connect( ui.preampSlider, &QSlider::valueChanged, this, &Equalizer::updatePreset );
    updatePreset();

    /* Set enable checkbox */
    vlc_object_t *p_aout = (vlc_object_t *)THEMIM->getAout();
    char *psz_af;
    if( p_aout )
        psz_af = var_GetNonEmptyString( p_aout, "audio-filter" );
    else
        psz_af = var_InheritString( THEPL, "audio-filter" );

    /* To enable or disable subwidgets: off dims them, values kept */
    QWidget *const dependents[] = {
        ui.presetLabel, ui.presetName, ui.presetsPlaceholder, ui.eq2PassCheck,
        ui.slidersPlaceholder, ui.preampSlider, ui.preampValue, ui.preampLabel,
    };
    for( size_t d = 0; d < sizeof( dependents ) / sizeof( dependents[0] ); d++ )
        connect( ui.enableCheck, &QCheckBox::toggled, dependents[d], &QWidget::setEnabled );

    if( psz_af && filterIsPresent( qfu(psz_af), name ) )
        ui.enableCheck->setChecked( true );
    else
        ui.enableCheck->setChecked( false );

    /* workaround for non emitted toggle() signal */
    ui.enableCheck->toggle(); ui.enableCheck->toggle();

    free( psz_af );
    connect( ui.enableCheck, &QCheckBox::toggled, this, &Equalizer::enable );

    /* Connect and set 2 Pass checkbox */
    ui.eq2PassCheck->setChecked( var_InheritBool( p_aout, "equalizer-2pass" ) );
    connect( ui.eq2PassCheck, &QCheckBox::toggled, this, &Equalizer::enable2Pass );
    if( p_aout )
        vlc_object_release( p_aout );
}

/* Moving a band shows "Custom", a preset its chip: compared the way
 * FilterSliderData::setValue() converts, so an applied preset matches */
void Equalizer::updatePreset()
{
    int match = -1;
    for( int i = 0; i < NB_PRESETS && match < 0; i++ )
    {
        const eqz_preset_t &preset = eqz_preset_10b[i];
        const auto same = []( QSlider *slider, float f, float resolution ) {
            return qBound( slider->minimum(), static_cast<int>( f / resolution ),
                           slider->maximum() ) == slider->value();
        };
        bool all = same( preampSlider, preset.f_preamp, preamp_values.f_resolution );
        for( int j = 0; all && j < qMin( preset.i_band, bandSliders.count() ); j++ )
            all = same( bandSliders[j], preset.f_amp[j], controls[j].f_resolution );
        if( all )
            match = i;
    }
    for( int i = 0; i < chips.count(); i++ )
        chips[i]->setChecked( i == match );
    presetName->setText( match < 0 ? qtr( "Custom" ) : qtr( preset_list_text[match] ) );
}

void Equalizer::setCorePreset( int i_preset )
{
    if( i_preset < 1 )
        return;

    i_preset--;/* 1st in index was an empty entry */

    preamp->setValue( eqz_preset_10b[i_preset].f_preamp );
    for ( int i=0; i< qMin( eqz_preset_10b[i_preset].i_band,
                            sliderDatas.count() ) ; i++ )
        sliderDatas[i]->setValue( eqz_preset_10b[i_preset].f_amp[i] );

    vlc_object_t *p_aout = (vlc_object_t *)THEMIM->getAout();
    if( p_aout )
    {
        var_SetString( p_aout , "equalizer-preset" , preset_list[i_preset] );
        vlc_object_release( p_aout );
    }
    emit configChanged( qfu( "equalizer-preset" ), QVariant( qfu( preset_list[i_preset] ) ) );
}

/* Function called when the set2Pass button is activated */
void Equalizer::enable2Pass( bool b_enable )
{
    vlc_object_t *p_aout= (vlc_object_t *)THEMIM->getAout();

    if( p_aout )
    {
        var_SetBool( p_aout, "equalizer-2pass", b_enable );
        vlc_object_release( p_aout );
    }
    emit configChanged( qfu( "equalizer-2pass" ), QVariant( b_enable ) );
}

/**********************************************************************
 * Audio filters
 **********************************************************************/

/**********************************************************************
 * Dynamic range compressor
 **********************************************************************/

Compressor::Compressor( intf_thread_t *p_intf, QWidget *parent )
    : AudioFilterControlWidget( p_intf, parent, "compressor" )
{
    i_smallfont = -2;
    const FilterSliderData::slider_data_t a[7] =
    {
        { "compressor-rms-peak",    qtr("RMS/peak"),         "",       0.0f,   1.0f,   0.00f, 0.001f, 1.0 },
        { "compressor-attack",      qtr("Attack"),       qtr("ms"),   1.5f, 400.0f,  25.00f, 0.100f, 1.0 },
        { "compressor-release",     qtr("Release"),      qtr("ms"),   2.0f, 800.0f, 100.00f, 0.100f, 1.0 },
        { "compressor-threshold",   qtr("Threshold"),    qtr("dB"), -30.0f,   0.0f, -11.00f, 0.010f, 1.0 },
        { "compressor-ratio",       qtr("Ratio"),            ":1",     1.0f,  20.0f,   8.00f, 0.010f, 1.0 },
        { "compressor-knee",        qtr("Knee\nradius"), qtr("dB"),   1.0f,  10.0f,   2.50f, 0.010f, 1.0 },
        { "compressor-makeup-gain", qtr("Makeup\ngain"), qtr("dB"),   0.0f,  24.0f,   7.00f, 0.010f, 1.0 },
    };
    for( int i=0; i<7 ;i++ ) controls.append( a[i] );
    build();
}

/**********************************************************************
 * Spatializer
 **********************************************************************/

Spatializer::Spatializer( intf_thread_t *p_intf, QWidget *parent )
    : AudioFilterControlWidget( p_intf, parent, "spatializer" )
{
    i_smallfont = -1;
    const FilterSliderData::slider_data_t a[5] =
    {
        { "spatializer-roomsize",   qtr("Size"),    "", 0.0f, 1.1f, 0.85f, 0.1f, 10.0 },
        { "spatializer-width",      qtr("Width"),   "", 0.0f, 1.0f, 1.0f, 0.1f, 10.0 },
        { "spatializer-wet",        qtr("Wet"),     "", 0.0f, 1.0f, 0.4f, 0.1f, 10.0 },
        { "spatializer-dry",        qtr("Dry"),     "", 0.0f, 1.0f, 0.5f, 0.1f, 10.0 },
        { "spatializer-damp",       qtr("Damp"),    "", 0.0f, 1.0f, 0.5f, 0.1f, 10.0 },
    };
    for( int i=0; i<5 ;i++ ) controls.append( a[i] );
    build();
}

/**********************************************************************
 * Spatializer
 **********************************************************************/

StereoWidener::StereoWidener( intf_thread_t *p_intf, QWidget *parent )
    : AudioFilterControlWidget( p_intf, parent, "stereo_widen" )
{
    i_smallfont = -1;
    const FilterSliderData::slider_data_t a[4] =
    {
        { "stereowiden-delay",     qtr("Delay time"),    "ms", 1.0, 100,  20, 1.0, 1.0 },
        { "stereowiden-feedback",  qtr("Feedback gain"), "%",  0.0, 0.9, 0.3, 0.1, 1.0 },
        { "stereowiden-crossfeed", qtr("Crossfeed"),     "%",  0.0, 0.8, 0.3, 0.1, 1.0 },
        { "stereowiden-dry-mix",   qtr("Dry mix"),       "%",  0.0, 1.0, 0.8, 0.1, 1.0 },
    };
    for( int i=0; i<4 ;i++ ) controls.append( a[i] );
    build();
}

/**********************************************************************
 * Advanced
 **********************************************************************/

PitchShifter::PitchShifter( intf_thread_t *p_intf, QWidget *parent )
    : AudioFilterControlWidget( p_intf, parent, "scaletempo_pitch" )
{
    i_smallfont = -1;
    controls.append( { "pitch-shift", qtr("Adjust pitch"), "semitones",
                        -12.0, 12.0, 0.0, 0.25, 1.0 } );
    build();
}

#include <QToolButton>
#include <QGridLayout>
#include <QFocusEvent>
#include <QLineEdit>

#define SUBSDELAY_CFG_MODE                     "subsdelay-mode"
#define SUBSDELAY_CFG_FACTOR                   "subsdelay-factor"
#define SUBSDELAY_MODE_ABSOLUTE                0
#define SUBSDELAY_MODE_RELATIVE_SOURCE_DELAY   1
#define SUBSDELAY_MODE_RELATIVE_SOURCE_CONTENT 2

namespace
{

/* "+0.150 s": the sign shows on delays too */
class SignedSpinBox : public QDoubleSpinBox
{
protected:
    QString textFromValue( double value ) const Q_DECL_OVERRIDE
    {
        const QString text = QDoubleSpinBox::textFromValue( value );
        return value > 0 ? QLatin1Char( '+' ) + text : text;
    }
    /* the sheet opens with a caret, not the value selected in accent;
     * Tab still selects it all, to type over */
    void focusInEvent( QFocusEvent *e ) Q_DECL_OVERRIDE
    {
        QDoubleSpinBox::focusInEvent( e );
        if( e->reason() != Qt::TabFocusReason && e->reason() != Qt::BacktabFocusReason )
            lineEdit()->deselect();
    }
};

/* "Decrease (G)", or the bare label when the action has no key */
QString withHotkey( const QString &label, const QString &key )
{
    return key.isEmpty() ? label : QString( "%1 (%2)" ).arg( label, key );
}

/* One card of the Synchronization tab: title and description, the control */
QFrame *syncRow( const QString &title, const QString &desc, QWidget *control )
{
    using namespace RedesignTokens;
    Theme *t = Theme::instance();
    QFrame *row = new QFrame;
    row->setProperty( "role", "fxRow" );
    QHBoxLayout *layout = new QHBoxLayout( row );
    const int pad = t->px( Effects::SyncRowPad );
    layout->setContentsMargins( pad, pad, pad, pad );
    layout->setSpacing( t->px( Effects::SyncRowGap ) );
    QVBoxLayout *texts = new QVBoxLayout;
    texts->setSpacing( 0 );
    QLabel *titleLabel = new QLabel( title );
    QFont titleFont = t->font( Theme::Body );
    titleFont.setWeight( QFont::Medium );
    titleLabel->setFont( titleFont );
    titleLabel->setBuddy( control );
    QLabel *descLabel = new QLabel( desc );
    descLabel->setFont( t->font( Theme::Meta ) );
    descLabel->setProperty( "role", "hint" );
    descLabel->setWordWrap( true );
    texts->addWidget( titleLabel );
    texts->addWidget( descLabel );
    layout->addLayout( texts, 1 );
    layout->addWidget( control );
    control->setAccessibleName( title );
    control->setAccessibleDescription( desc );
    return row;
}

}

SyncWidget::SyncWidget( QWidget *_parent, const QString &name,
                        const QString &downKey, const QString &upKey )
    : QWidget( _parent )
{
    using namespace RedesignTokens;
    Theme *t = Theme::instance();
    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( t->px( Effects::BandGap ) );

    spinBox = new SignedSpinBox;
    spinBox->setAlignment( Qt::AlignCenter );
    spinBox->setDecimals( 3 );
    spinBox->setMinimum( -600.0 );
    spinBox->setMaximum( 600.0 );
    spinBox->setSingleStep( 0.05 );     /* P5-2: 50 ms, the hotkeys keep theirs */
    spinBox->setSuffix( " s" );
    spinBox->setButtonSymbols( QDoubleSpinBox::NoButtons );
    spinBox->setFont( t->font( Theme::Mono ) );
    spinBox->setAccessibleName( name );
    connect( spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
             this, &SyncWidget::valueChanged );

    const int buttonPx = t->px( Effects::StepperButton );
    const int glyphPx = t->px( Effects::StepperGlyph );
    const struct { const char *icon; QString label, key; int step; } steps[] = {
        { RedesignIcons::IconRemove, qtr( "Decrease %1" ).arg( name ), downKey, -1 },
        { RedesignIcons::IconAdd, qtr( "Increase %1" ).arg( name ), upKey, 1 },
    };
    for( size_t i = 0; i < sizeof( steps ) / sizeof( steps[0] ); i++ )
    {
        QToolButton *b = new QToolButton( this );
        b->setProperty( "role", "stepper" );
        b->setIcon( Theme::themedIcon( steps[i].icon, Theme::Text ) );
        b->setIconSize( QSize( glyphPx, glyphPx ) );
        b->setFixedSize( buttonPx, buttonPx );
        b->setAutoRepeat( true );
        b->setAccessibleName( steps[i].label );
        b->setToolTip( withHotkey( steps[i].label, steps[i].key ) );
        const int step = steps[i].step;
        connect( b, &QToolButton::clicked, spinBox, [this, step]() { spinBox->stepBy( step ); } );
        if( i == 0 )
            layout->addWidget( b );
        else
        {
            layout->addWidget( spinBox );
            layout->addWidget( b );
        }
    }
    /* the widest value fits: the sign, 3 digits, the decimals and the unit */
    spinBox->setMinimumWidth( QFontMetrics( spinBox->font() ).width( "+000.000 s" )
                              + 2 * t->px( Size::MenuPanelPadding ) );
    setFocusProxy( spinBox );
}

void SyncWidget::setValue( double d )
{
    spinBox->setValue( d );
}

SyncControls::SyncControls( intf_thread_t *_p_intf, QWidget *_parent ) :
                            QWidget( _parent ) , p_intf( _p_intf )
{
    using namespace RedesignTokens;
    Theme *t = Theme::instance();
    QToolButton *updateButton;

    b_userAction = true;

    QVBoxLayout *mainLayout = new QVBoxLayout( this );
    mainLayout->setContentsMargins( 0, 0, 0, 0 );
    mainLayout->setSpacing( t->px( Effects::SyncRowGap ) );

    /* the hotkeys' hints: P5-2 keeps their own steps */
    const QString audioDown = hotkeyText( p_intf, "key-audiodelay-down" );
    const QString audioUp = hotkeyText( p_intf, "key-audiodelay-up" );
    const QString subsDown = hotkeyText( p_intf, "key-subdelay-down" );
    const QString subsUp = hotkeyText( p_intf, "key-subdelay-up" );
    const auto keysHint = []( const QString &desc, const QString &down, const QString &up ) {
        return down.isEmpty() || up.isEmpty() ? desc
             : qtr( "%1. Keys: %2 / %3" ).arg( desc, down, up );
    };

    /* AV sync */
    AVSpin = new SyncWidget( this, qtr( "Audio track" ), audioDown, audioUp );
    mainLayout->addWidget( syncRow( qtr( "Audio track" ),
        keysHint( qtr( "Positive delays audio behind video" ), audioDown, audioUp ), AVSpin ) );

    /* Subs */
    subsSpin = new SyncWidget( this, qtr( "Subtitle track" ), subsDown, subsUp );
    mainLayout->addWidget( syncRow( qtr( "Subtitle track" ),
        keysHint( qtr( "Positive shows subtitles later" ), subsDown, subsUp ), subsSpin ) );

    subSpeedSpin = new QDoubleSpinBox;
    subSpeedSpin->setAlignment( Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter );
    subSpeedSpin->setDecimals( 3 );
    subSpeedSpin->setMinimum( 1 );
    subSpeedSpin->setMaximum( 100 );
    subSpeedSpin->setSingleStep( 0.2 );
    subSpeedSpin->setSuffix( " fps" );
    subSpeedSpin->setButtonSymbols( QDoubleSpinBox::PlusMinus );
    subSpeedSpin->setFont( t->font( Theme::Mono ) );
    mainLayout->addWidget( syncRow( qtr( "Subtitle speed" ),
        qtr( "Stretch timing for mismatched frame rates" ), subSpeedSpin ) );

    subDurationSpin = new QDoubleSpinBox;
    subDurationSpin->setAlignment( Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter );
    subDurationSpin->setDecimals( 3 );
    subDurationSpin->setMinimum( 0 );
    subDurationSpin->setMaximum( 20 );
    subDurationSpin->setSingleStep( 0.2 );
    subDurationSpin->setButtonSymbols( QDoubleSpinBox::PlusMinus );
    subDurationSpin->setFont( t->font( Theme::Mono ) );
    /* the row reads the mode's explanation initSubsDuration() puts in the tooltip */
    initSubsDuration();
    mainLayout->addWidget( syncRow( qtr( "Subtitle duration factor" ),
        subDurationSpin->toolTip().replace( QLatin1Char( '\n' ), QLatin1Char( ' ' ) ),
        subDurationSpin ) );

    updateButton = new QToolButton;
    updateButton->setAutoRaise( true );
    QHBoxLayout *updateRow = new QHBoxLayout;
    updateRow->addStretch( 1 );
    updateRow->addWidget( updateButton );
    mainLayout->addLayout( updateRow );
    mainLayout->addStretch( 1 );

    /* Various Connects */
    connect( AVSpin, &SyncWidget::valueChanged, this, &SyncControls::advanceAudio ) ;
    connect( subsSpin, &SyncWidget::valueChanged, this, &SyncControls::advanceSubs ) ;
    connect( subSpeedSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SyncControls::adjustSubsSpeed );
    connect( subDurationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SyncControls::adjustSubsDuration );

    connect( THEMIM->getIM(), &InputManager::synchroChanged, this, &SyncControls::update );
    BUTTON_SET_ACT_I( updateButton, "", update,
            qtr( "Force update of this dialog's values" ), update );
    updateButton->setAccessibleName( qtr( "Force update of this dialog's values" ) );

    /* Set it */
    update();
}

SyncControls::~SyncControls()
{
    subsdelayClean();
}

void SyncControls::clean()
{
    b_userAction = false;
    AVSpin->setValue( 0.0 );
    subsSpin->setValue( 0.0 );
    subSpeedSpin->setValue( 1.0 );
    subsdelayClean();
    b_userAction = true;
}

void SyncControls::update()
{
    b_userAction = false;

    int64_t i_delay;
    if( THEMIM->getInput() )
    {
        i_delay = var_GetInteger( THEMIM->getInput(), "audio-delay" );
        AVSpin->setValue( ( (double)i_delay ) / CLOCK_FREQ );
        i_delay = var_GetInteger( THEMIM->getInput(), "spu-delay" );
        subsSpin->setValue( ( (double)i_delay ) / CLOCK_FREQ );
        subSpeedSpin->setValue( var_GetFloat( THEMIM->getInput(), "sub-fps" ) );
        subDurationSpin->setValue( var_InheritFloat( p_intf, SUBSDELAY_CFG_FACTOR ) );
    }
    b_userAction = true;
}

void SyncControls::advanceAudio( double f_advance )
{
    if( THEMIM->getInput() && b_userAction )
    {
        int64_t i_delay = f_advance * CLOCK_FREQ;
        var_SetInteger( THEMIM->getInput(), "audio-delay", i_delay );
    }
}

void SyncControls::advanceSubs( double f_advance )
{
    if( THEMIM->getInput() && b_userAction )
    {
        int64_t i_delay = f_advance * CLOCK_FREQ;
        var_SetInteger( THEMIM->getInput(), "spu-delay", i_delay );
    }
}

void SyncControls::adjustSubsSpeed( double f_fps )
{
    if( THEMIM->getInput() && b_userAction )
    {
        var_SetFloat( THEMIM->getInput(), "sub-fps", f_fps );
    }
}

void SyncControls::adjustSubsDuration( double f_factor )
{
    if( THEMIM->getInput() && b_userAction )
    {
        subsdelaySetFactor( f_factor );
        changeVFiltersString( "subsdelay", f_factor > 0 );
    }
}

void SyncControls::initSubsDuration()
{
    int i_mode = var_InheritInteger( p_intf, SUBSDELAY_CFG_MODE );

    switch (i_mode)
    {
    default:
    case SUBSDELAY_MODE_ABSOLUTE:
        subDurationSpin->setToolTip( qtr( "Extend subtitle duration by this value.\n"
                                          "Set 0 to disable." ) );
        subDurationSpin->setSuffix( " s" );
        break;
    case SUBSDELAY_MODE_RELATIVE_SOURCE_DELAY:
        subDurationSpin->setToolTip( qtr( "Multiply subtitle duration by this value.\n"
                                          "Set 0 to disable." ) );
        subDurationSpin->setSuffix( "" );
        break;
    case SUBSDELAY_MODE_RELATIVE_SOURCE_CONTENT:
        subDurationSpin->setToolTip( qtr( "Recalculate subtitle duration according\n"
                                          "to their content and this value.\n"
                                          "Set 0 to disable." ) );
        subDurationSpin->setSuffix( "" );
        break;
    }
}

void SyncControls::subsdelayClean()
{
    /* Remove subsdelay filter */
    changeVFiltersString( "subsdelay", false );
}

void SyncControls::subsdelaySetFactor( double f_factor )
{
    QVector<vout_thread_t*> p_vouts = THEMIM->getVouts();
    foreach( vout_thread_t *p_vout, p_vouts )
    {
        var_SetFloat( p_vout, SUBSDELAY_CFG_FACTOR, f_factor );
        vlc_object_release( p_vout );
    }
}

void SyncControls::changeVFiltersString( const char *psz_name, bool b_add )
{
    const char *psz_filter_type = GetVFilterType( p_intf, psz_name );
    if( psz_filter_type == NULL )
        return;

    QString result = ChangeFiltersString( p_intf, psz_filter_type, psz_name, b_add );

    UpdateVFiltersString( p_intf, psz_filter_type, qtu( result ) );
}


/**********************************************************************
 * Video filters / Adjust
 **********************************************************************/

/**********************************************************************
 * Extended playbak controls
 **********************************************************************/
