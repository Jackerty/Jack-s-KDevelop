/* KDevelop QMake Support
 *
 * Copyright 2007 Andreas Pakulat <apaku@gmx.de>
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

#include "qmake_parser.h"
class QString;

namespace QMake
{

class Lexer {
public:
    Lexer(parser* _parser, const QString& contents);

    int getNextTokenKind() const;
    std::size_t getTokenBegin() const;
    std::size_t getTokenEnd() const;

private:
    QString mContent;
    parser* mParser;
    int state() const;
    void setState(int state);

    int mState;
    enum State
    {
        DefaultState,
        CommentState,
        QuoteState
    };

};

}

// kate: space-indent on; indent-width 4; tab-width: 4; replace-tabs on; auto-insert-doxygen on
