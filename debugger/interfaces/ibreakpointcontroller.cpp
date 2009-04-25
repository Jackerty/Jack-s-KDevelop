/* This file is part of the KDE project
   Copyright (C) 2002 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
   Copyright (C) 2002 John Firebaugh <jfirebaugh@kde.org>
   Copyright (C) 2006, 2008 Vladimir Prus <ghost@cs.msu.su>
   Copyright (C) 2007 Hamish Rodda <rodda@kde.org>
   Copyright (C) 2009 Niko Sams <niko.sams@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <KDebug>

#include "ibreakpointcontroller.h"
#include "idebugsession.h"
#include "../../interfaces/icore.h"
#include "../breakpoint/breakpointmodel.h"
#include "../../interfaces/idebugcontroller.h"

namespace KDevelop {

IBreakpointController::IBreakpointController(KDevelop::IDebugSession* parent)
    : QObject(parent), m_dontSendChanges(false)
{

    connect(breakpointModel(),
            SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(dataChanged(QModelIndex,QModelIndex)));
    connect(breakpointModel(), SIGNAL(breakpointDeleted(KDevelop::Breakpoint*)), SLOT(breakpointDeleted(KDevelop::Breakpoint*)));

}

IDebugSession* IBreakpointController::debugSession() const
{
    return static_cast<IDebugSession*>(const_cast<QObject*>(QObject::parent()));
}

BreakpointModel* IBreakpointController::breakpointModel() const
{
    return ICore::self()->debugController()->breakpointModel();
}


void IBreakpointController::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (m_dontSendChanges) return;

    kDebug() << debugSession()->state();

    Q_ASSERT(topLeft.parent() == bottomRight.parent());
    Q_ASSERT(topLeft.row() == bottomRight.row());
    Q_ASSERT(topLeft.column() == bottomRight.column());
    KDevelop::Breakpoint *b = breakpointModel()->breakpointsItem()->breakpoint(topLeft.row());
    m_dirty[b].insert(topLeft.column());
    kDebug() << topLeft.column() << m_dirty;
    if (debugSession()->isRunning()) {
        sendMaybe(b);
    }
}

void IBreakpointController::breakpointDeleted(KDevelop::Breakpoint* breakpoint)
{
    kDebug() << breakpoint;
    sendMaybe(breakpoint);
}
}

#include "ibreakpointcontroller.moc"
