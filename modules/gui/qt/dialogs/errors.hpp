/*****************************************************************************
 * errors.hpp : Errors
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
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

#ifndef QVLC_ERRORS_DIALOG_H_
#define QVLC_ERRORS_DIALOG_H_ 1

#include "util/qvlcframe.hpp"
#include "util/singleton.hpp"

#include <QList>
#include <QPair>

class QCheckBox;
class QPushButton;
class QLabel;
class QScrollArea;

class ErrorsDialog : public QVLCDialog, public Singleton<ErrorsDialog>
{
    Q_OBJECT
public:

    void addError( const QString&, const QString&, bool fromCore = false );
    /*void addWarning( QString, QString );*/
private:
    virtual ~ErrorsDialog();
    ErrorsDialog( intf_thread_t * );
    void add( bool, const QString&, const QString& );
    void setFailedItem( input_item_t * );
    void showEmpty( bool );

    QLabel *icon;
    QLabel *title;
    QLabel *body;
    QLabel *empty;
    QScrollArea *scroll;
    QPushButton *retryButton, *closeButton, *clearButton;
    QCheckBox *stopShowing;
    QList<QPair<QString, QString> > entries; /* title, text */
    input_item_t *failedItem; /* held: the item whose open failed */
private slots:
    void close();
    void clear();
    void retry();
    void dontShow();

    friend class    Singleton<ErrorsDialog>;
};

#endif
