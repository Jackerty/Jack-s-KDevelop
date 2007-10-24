/* This file is part of KDevelop
    Copyright 2006 Hamish Rodda <rodda@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef PARSESESSION_H
#define PARSESESSION_H

#include <cstdlib>

#include <QtCore/QString>
#include <QtCore/QByteArray>

#include <ktexteditor/cursor.h>

#include <cppparserexport.h>
#include <ksharedptr.h>
#include <kurl.h>

namespace Cpp {
  class EnvironmentFile;
}

class pool;
class TokenStream;
class LocationTable;

namespace rpp { class MacroBlock; }

/// Contains everything needed to keep an AST useful once the rest of the parser
/// has gone away.
class KDEVCPPPARSER_EXPORT ParseSession
{
public:
  ParseSession();
  ~ParseSession();

  /**
   * Return the position (\a line%, \a column%) of the \a offset in \a filename
   *
   * \note the line starts from 0.
   */
  void positionAt(std::size_t offset, int *line, int *column,
                  QString *filename) const;

  void setContents(const QByteArray& contents, const KTextEditor::Cursor& offset = KTextEditor::Cursor());

  void setUrl(const KUrl& url);
  
  const char *contents() const;
  std::size_t size() const;
  pool* mempool;
  TokenStream* token_stream;
  LocationTable* location_table; //Maps line-numbers to positions in the document
  LocationTable* line_table;
  rpp::MacroBlock* macros;
  KTextEditor::Cursor m_contentOffset;
  KUrl m_url; //Should contain the url from which the content was extracted, can also be empty.

private:
  void extract_line(int offset, int *line, QString *filename) const;
  QByteArray m_contents;
};

#endif
