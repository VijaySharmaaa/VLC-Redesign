/*****************************************************************************
 * network_page.hpp : the Network library page
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

#ifndef VLC_QT_NETWORK_PAGE_HPP_
#define VLC_QT_NETWORK_PAGE_HPP_

#include "qt.hpp"
#include "components/library/library_widgets.hpp"

#include <QAbstractButton>
#include <QMap>
#include <QPersistentModelIndex>

class QGridLayout;
class QLabel;
class QScrollArea;
class QStackedWidget;
class QTimer;

/* DESIGN_SPEC 4: a 40 px tinted icon square, a name and a mono address */
class NetworkTile : public QAbstractButton
{
    Q_OBJECT
public:
    NetworkTile( const char *icon, const QString &name, const QString &address, QWidget *parent );
    QSize sizeHint() const Q_DECL_OVERRIDE;

protected:
    void paintEvent( QPaintEvent * ) Q_DECL_OVERRIDE;
    void keyPressEvent( QKeyEvent * ) Q_DECL_OVERRIDE;

private:
    const char *icon;
    QString address;
};

/* DESIGN_SPEC 4 (P4-5, P4-6): "On your network" has a tile per server or
 * share the LAN discoveries (upnp, microdns, sap: those in the package)
 * find, live; they load when the page is first shown. "Streams" opens the
 * network dialog, the Icecast directory and the podcasts. A tile opens the
 * node's items in a list on its own PLModel. */
class NetworkPage : public QWidget
{
    Q_OBJECT
public:
    explicit NetworkPage( intf_thread_t *, QWidget *parent = NULL );

protected:
    void showEvent( QShowEvent * ) Q_DECL_OVERRIDE;
    void resizeEvent( QResizeEvent * ) Q_DECL_OVERRIDE;

private:
    struct Lan
    {
        PLModel *model;
        const char *icon;
    };
    void load();
    QLabel *addGroup( const QString &title, QGridLayout ** );
    void rebuildLanTiles();
    void layoutTiles();
    void openSd( const LibraryData::Sd &, const QString &title );
    void open( PLModel *, const QModelIndex &source, const QString &title );
    void activate( const QModelIndex & );
    void browse( const QModelIndex &source );
    void goBack();
    void showTiles();
    void updateState();

    intf_thread_t *p_intf;
    QList<Lan> lans;
    QMap<QString, PLModel *> internet;   /* by module name, made when opened */
    LibraryHeader *header;
    QStackedWidget *body;
    QScrollArea *tilesPage;
    QGridLayout *lanGrid, *streamGrid;
    QLabel *lanHint;
    QList<NetworkTile *> lanTiles, streamTiles;
    QTimer *rebuild, *settle;
    CardView *list;
    LibraryFilter *filter;
    PLModel *browsed;
    QPersistentModelIndex top;   /* the opened tile; invalid: a whole discovery */
    QString topTitle;
    bool topIsItem;              /* a server or share, which can go away */
    int expandingId;
    bool loaded;
};

#endif
