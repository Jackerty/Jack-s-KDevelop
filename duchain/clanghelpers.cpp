/*
 * Copyright 2014  Olivier de Gaalon <olivier.jg@gmail.com>
 * Copyright 2014  Milian Wolff <mail@milianw.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "clanghelpers.h"

#include <language/duchain/duchainlock.h>
#include <language/duchain/declaration.h>
#include <language/duchain/parsingenvironment.h>
#include <language/backgroundparser/urlparselock.h>

#include "tuduchain.h"
#include "parsesession.h"
#include "clangparsingenvironmentfile.h"

#include "util/clangtypes.h"

#include <algorithm>

using namespace KDevelop;

namespace {

// TODO: Use it once clang_getInclusions _does_ returns imports on reparse with CXTranslationUnit_PrecompiledPreamble flag.
// void visitInclusions(CXFile file, CXSourceLocation* stack, unsigned stackDepth, CXClientData d)
// {
//     if (stackDepth) {
//         auto imports = static_cast<Imports*>(d);
//         CXFile parentFile;
//         uint line, column;
//         clang_getFileLocation(stack[0], &parentFile, &line, &column, nullptr);
//         imports->insert(parentFile, {file, CursorInRevision(line-1, column-1)});
//     }
// }

CXChildVisitResult visitCursor(CXCursor cursor, CXCursor, CXClientData data)
{
    if (cursor.kind != CXCursor_InclusionDirective) {
        return CXChildVisit_Continue;
    }

    auto imports = static_cast<Imports*>(data);
    CXFile file = clang_getIncludedFile(cursor);
    if(!file){
        return CXChildVisit_Continue;
    }

    CXSourceLocation location = clang_getCursorLocation(cursor);
    CXFile parentFile;
    uint line, column;
    clang_getFileLocation(location, &parentFile, &line, &column, nullptr);

    foreach (auto import, *imports) {
        // clang_getInclusions doesn't include the same import twice, so we shouldn't do it too.
        if (import.file == file) {
            return CXChildVisit_Continue;
        }
    }

    imports->insert(parentFile, {file, CursorInRevision(line-1, column-1)});

    return CXChildVisit_Recurse;
}

ReferencedTopDUContext createTopContext(const IndexedString& path, const ClangParsingEnvironment& environment,
                                        const bool isSystemHeader)
{
    ClangParsingEnvironmentFile* file = new ClangParsingEnvironmentFile(path, environment, isSystemHeader);
    ReferencedTopDUContext context = new ClangTopDUContext(path, RangeInRevision(0, 0, INT_MAX, INT_MAX), file);
    DUChain::self()->addDocumentChain(context);
    return context;
}

}

Imports ClangHelpers::tuImports(CXTranslationUnit tu)
{
    Imports imports;
    // TODO: Use it once clang_getInclusions _does_ returns imports on reparse with CXTranslationUnit_PrecompiledPreamble flag.
    //clang_getInclusions(tu, &::visitInclusions, &imports);

    CXCursor tuCursor = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(tuCursor, &visitCursor, &imports);

    return imports;
}

ReferencedTopDUContext ClangHelpers::buildDUChain(CXFile file, const Imports& imports, const ParseSession& session,
                                                  TopDUContext::Features features, IncludeFileContexts& includedFiles)
{
    if (includedFiles.contains(file)) {
        return {};
    }

    // prevent recursion
    includedFiles.insert(file, {});

    // ensure DUChain for imports are build properly
    foreach(const auto& import, imports.values(file)) {
        buildDUChain(import.file, imports, session, features, includedFiles);
    }

    const IndexedString path(QDir::cleanPath(QString::fromUtf8(ClangString(clang_getFileName(file)))));

    const auto& environment = session.environment();

    bool update = false;
    UrlParseLock urlLock(path);
    ReferencedTopDUContext context;
    {
        const auto problems = session.problemsForFile(file);

        auto fileLocation = clang_getLocation(session.unit(), file, 1, 1);
        const bool isSystemHeader = clang_Location_isInSystemHeader(fileLocation);

        DUChainWriteLocker lock;
        context = DUChain::self()->chainForDocument(path, &environment);
        if (!context) {
            context = ::createTopContext(path, environment, isSystemHeader);
        } else {
            update = true;
        }

        context->setProblems(problems);

        includedFiles.insert(file, context);
        if (update) {
            auto envFile = ClangParsingEnvironmentFile::Ptr(dynamic_cast<ClangParsingEnvironmentFile*>(context->parsingEnvironmentFile().data()));
            Q_ASSERT(envFile);
            if (!envFile->needsUpdate(&environment) && envFile->featuresSatisfied(features)) {
                return context;
            } else {
                envFile->setEnvironment(environment);
                envFile->setIsSystemHeader(isSystemHeader);
                envFile->setModificationRevision(ModificationRevision::revisionForFile(context->url()));
            }

            context->clearImportedParentContexts();
        }
        context->setFeatures(features);

        foreach(const auto& import, imports.values(file)) {
            Q_ASSERT(includedFiles.contains(import.file));
            auto ctx = includedFiles.value(import.file);
            if (!ctx) {
                // happens for cyclic imports
                continue;
            }
            context->addImportedParentContext(ctx, import.location);
        }
    }

    TUDUChain tuduchain(session.unit(), file, includedFiles, update);

    return context;
}

DeclarationPointer ClangHelpers::findDeclaration(CXCursor cursor, const IncludeFileContexts& includes)
{
    auto refLoc = clang_getCursorLocation(cursor);
    CXFile file = nullptr;
    clang_getFileLocation(refLoc, &file, nullptr, nullptr, nullptr);
    if (!file) {
        return {};
    }
    auto refCursor = CursorInRevision(ClangLocation(refLoc));

    const auto& top = includes.value(file);
    if (!top) {
        // may happen for cyclic includes
        return {};
    }

    DUChainReadLocker lock;
    Q_ASSERT(top);
    if (DUContext *local = top->findContextAt(refCursor)) {
        if (local->owner() && local->owner()->range().contains(refCursor)) {
           return DeclarationPointer(local->owner());
        }
        return DeclarationPointer(local->findDeclarationAt(refCursor));
    }
    return {};
}

DeclarationPointer ClangHelpers::findDeclaration(CXType type, const IncludeFileContexts& includes)
{
    CXCursor cursor = clang_getTypeDeclaration(type);
    return findDeclaration(cursor, includes);
}

DeclarationPointer ClangHelpers::findForwardDeclaration(CXType type, DUContext* context, CXCursor cursor)
{
    auto qualifiedIdentifier = QualifiedIdentifier(QString::fromUtf8(ClangString(clang_getTypeSpelling(type))));

    DUChainReadLocker lock;
    auto decls = context->findDeclarations(qualifiedIdentifier,
        CursorInRevision(ClangLocation(clang_getCursorLocation(cursor)))
    );

    foreach (auto decl, decls) {
        if (decl->isForwardDeclaration()) {
            return DeclarationPointer(decl);
        }
    }
    return {};
}

RangeInRevision ClangHelpers::cursorSpellingNameRange(CXCursor cursor, const Identifier& id)
{
    auto range = ClangRange(clang_Cursor_getSpellingNameRange(cursor, 0, 0)).toRangeInRevision();
    auto kind = clang_getCursorKind(cursor);
    // TODO: Upstream issue: Check why clang reports invalid ranges for destructors and methods like 'operator='
    // Current issues:
    // - CXCursor_Destructor: Only returns the range of '~'
    // - CXCursor_CXXMethod: For operator overloads, only returns the range of 'operator'
    if (kind == CXCursor_Destructor || kind == CXCursor_CXXMethod) {
        range.end.column = range.start.column + id.toString().length();
    }
    return range;
}

QStringList ClangHelpers::headerExtensions()
{
    static const QStringList headerExtensions = {
            QStringLiteral("h"),
            QStringLiteral("H"),
            QStringLiteral("hh"),
            QStringLiteral("hxx"),
            QStringLiteral("hpp"),
            QStringLiteral("tlh"),
            QStringLiteral("h++"),
    };
    return headerExtensions;
}

QStringList ClangHelpers::sourceExtensions()
{
    static const QStringList sourceExtensions = {
        QStringLiteral("c"),
        QStringLiteral("cc"),
        QStringLiteral("cpp"),
        QStringLiteral("c++"),
        QStringLiteral("cxx"),
        QStringLiteral("C"),
        QStringLiteral("m"),
        QStringLiteral("mm"),
        QStringLiteral("M"),
        QStringLiteral("inl"),
        QStringLiteral("_impl.h"),
    };
    return sourceExtensions;
}

bool ClangHelpers::isSource(const QString& path)
{
    auto extensions = sourceExtensions();
    return std::any_of(extensions.constBegin(), extensions.constEnd(),
                       [&](const QString& ext) { return path.endsWith(ext); });
}

bool ClangHelpers::isHeader(const QString& path)
{
    auto extensions = headerExtensions();
    return std::any_of(extensions.constBegin(), extensions.constEnd(),
                       [&](const QString& ext) { return path.endsWith(ext); });
}
