/*  This file is part of KDevelop
    Copyright 2010 Aleix Pol Gonzalez <aleixpol@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "projecttargetscombobox.h"
#include <project/projectmodel.h>
#include <interfaces/iproject.h>
#include <util/kdevstringhandler.h>
#include <KIcon>
#include <interfaces/icore.h>
#include <interfaces/iprojectcontroller.h>

using namespace KDevelop;

ProjectTargetsComboBox::ProjectTargetsComboBox(QWidget* parent)
    : QComboBox(parent)
{

}
    
class ExecutablePathsVisitor
    : public ProjectVisitor
{
    public:
        virtual void visit(ProjectExecutableTargetItem* eit) {
            m_paths += KDevelop::joinWithEscaping(eit->model()->pathFromIndex(eit->index()), '/', '\\');
        }
        virtual void visit(IProject* p) { ProjectVisitor::visit(p); }
        virtual void visit(ProjectBuildFolderItem* it) { ProjectVisitor::visit(it); }
        virtual void visit(ProjectFolderItem* it) { ProjectVisitor::visit(it); }
        virtual void visit(ProjectFileItem* it) { ProjectVisitor::visit(it); }
        virtual void visit(ProjectLibraryTargetItem* it) { ProjectVisitor::visit(it); }
        
        QStringList paths() const { return m_paths; }
        
    private:
        QStringList m_paths;
};


void ProjectTargetsComboBox::setBaseItem(ProjectFolderItem* item)
{
    clear();

    QList<ProjectFolderItem*> items;
    if(item) {
        items += item;
    } else {
        foreach(IProject* p, ICore::self()->projectController()->projects()) {
            items += p->projectItem();
        }
    }
    
    ExecutablePathsVisitor walker;
    foreach(ProjectFolderItem* item, items) {
        walker.visit(item);
    }
    
    foreach(const QString& item, walker.paths())
        addItem(KIcon("system-run"), item);
    
}

QStringList ProjectTargetsComboBox::currentItemPath() const
{
    return KDevelop::splitWithEscaping(currentText(), '/', '\\');
}

void ProjectTargetsComboBox::setCurrentItemPath(const QStringList& str)
{
    setCurrentIndex(findText(KDevelop::joinWithEscaping(str, '/', '\\')));
}
