/***************************************************************************
 *   Copyright 2008 Evgeniy Ivanov <powerfox@kde.ru>                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef BRANCH_MANAGER_H
#define BRANCH_MANAGER_H

#include <KDE/KDialog>

class BranchesListModel;
class KJob;

namespace Ui { class BranchDialogBase; }

namespace KDevelop
{
class DistributedVersionControlPlugin;
}

class BranchManager : public KDialog
{
    Q_OBJECT
public:
    BranchManager(const QString& repository, KDevelop::DistributedVersionControlPlugin* executor, QWidget *parent = 0);
    ~BranchManager();

signals:
    void checkedOut(KJob*);

private slots:
    void createBranch();
    void deleteBranch();
    void renameBranch();
    void checkoutBranch();

private:
    QString m_repository;
    KDevelop::DistributedVersionControlPlugin* m_dvcPlugin;

    Ui::BranchDialogBase* m_ui;
    BranchesListModel* m_model;
};

#endif
