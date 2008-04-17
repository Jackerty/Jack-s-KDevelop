/* KDevelop CMake Support
 *
 * Copyright 2008 Aleix Pol <aleixpol@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef CMAKECODECOMPLETION_H
#define CMAKECODECOMPLETION_H

#include <ktexteditor/codecompletionmodel.h>
#include <duchainpointer.h>
#include <QStringList>

namespace KTextEditor { class Document; class Range; }

class CMakeCodeCompletionModel : public KTextEditor::CodeCompletionModel
{
    public:
        CMakeCodeCompletionModel (QObject *parent=0);

        void completionInvoked(KTextEditor::View* view, const KTextEditor::Range& range, InvocationType invocationType);
        QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
        void executeCompletionItem(KTextEditor::Document* document, const KTextEditor::Range& word, int row) const;
    private:
        enum Type { Command, VariableMacro };
        Type indexType(int row) const;
        QStringList m_commands;
        QList< KDevelop::DeclarationPointer > m_declarations;
};

#endif
