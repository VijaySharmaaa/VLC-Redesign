/*****************************************************************************
 * home_page.hpp : the library's Home page (continue watching, recent media)
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

#ifndef VLC_QT_HOME_PAGE_HPP_
#define VLC_QT_HOME_PAGE_HPP_

#include "qt.hpp"

#include <QElapsedTimer>
#include <QRunnable>
#include <QScrollArea>
#include <QSet>
#include <QStringList>

class ArtLoader;
class LibraryEmptyState;
class LibraryNotice;
class LibraryHeader;
class QHBoxLayout;
class QTreeWidget;
class ResumeCard;
class Thumbnailer;

/* DESIGN_SPEC 4 "Home", from the recents (RecentsMRL): a row of cards for
 * what can resume, then a table of every recent item. Without recents (or
 * with qt-recentplay off) it shows the empty state of inventory 1.1.1. */
class HomePage : public QScrollArea
{
    Q_OBJECT
public:
    explicit HomePage( intf_thread_t *, QWidget *parent = NULL );

protected:
    void showEvent( QShowEvent * ) Q_DECL_OVERRIDE;
    void hideEvent( QHideEvent * ) Q_DECL_OVERRIDE;
    bool eventFilter( QObject *, QEvent * ) Q_DECL_OVERRIDE;
    bool viewportEvent( QEvent * ) Q_DECL_OVERRIDE;

private slots:
    void rebuild();
    void setMissing( const QStringList &checkedMrls, const QStringList &mrls );
    void updateCards();

private:
    void layoutForWidth();
    void applyMissing();
    void startCheck();
    void play( const QString &mrl, bool resume );
    /* by value: the card that owns mrl can go while the menu is open */
    void popup( const QString mrl, const QPoint &globalPos );

    intf_thread_t *p_intf;
    ArtLoader *art;
    Thumbnailer *thumbs;
    LibraryHeader *header;
    QWidget *lists;
    QWidget *cardsSection, *recentSection;
    QScrollArea *cardsArea;
    QHBoxLayout *cardsLayout;
    QList<ResumeCard *> cards;
    QTreeWidget *table;
    LibraryEmptyState *empty;
    LibraryNotice *noResults;
    QSet<QString> missing;  /* mrls of local files that are not there */
    QSet<QString> checked;  /* mrls the last check covered */
    /* one check at a time: a blocked one must not pile up others */
    bool checking, recheck;
    QElapsedTimer sincePlay;
};

/* Whether local files exist, off the GUI thread: an offline share can
 * block a stat for many seconds */
class ExistsCheck : public QObject, public QRunnable
{
    Q_OBJECT
public:
    ExistsCheck( const QStringList &_mrls, const QStringList &_paths )
        : mrls( _mrls ), paths( _paths ) {}
    void run() Q_DECL_OVERRIDE;

signals:
    void done( const QStringList &checkedMrls, const QStringList &missingMrls );

private:
    QStringList mrls, paths;
};

#endif
