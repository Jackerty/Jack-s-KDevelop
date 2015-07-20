/* This file is part of the KDE project
Copyright 2002 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
Copyright 2002 Bernd Gehrmann <bernd@kdevelop.org>
Copyright 2003 Roberto Raggi <roberto@kdevelop.org>
Copyright 2003-2008 Hamish Rodda <rodda@kde.org>
Copyright 2003 Harald Fernengel <harry@kdevelop.org>
Copyright 2003 Jens Dagerbo <jens.dagerbo@swipnet.se>
Copyright 2005 Adam Treat <treat@kde.org>
Copyright 2004-2007 Alexander Dymo <adymo@kdevelop.org>
Copyright 2007 Andreas Pakulat <apaku@gmx.de>

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
#include "documentcontroller.h"

#include <QApplication>
#include <QDBusConnection>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QTemporaryFile>

#include <KActionCollection>
#include <KEncodingFileDialog>
#include <KIO/StatJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KProtocolInfo>
#include <KRecentFilesAction>
#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <KTextEditor/AnnotationInterface>

#include <sublime/area.h>
#include <sublime/view.h>
#include <interfaces/iplugincontroller.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/ibuddydocumentfinder.h>
#include <interfaces/iproject.h>
#include <interfaces/iselectioncontroller.h>
#include <interfaces/context.h>
#include <project/projectmodel.h>
#include <util/path.h>

#include "core.h"
#include "mainwindow.h"
#include "textdocument.h"
#include "uicontroller.h"
#include "partcontroller.h"
#include "savedialog.h"
#include "debug.h"

#include <vcs/interfaces/ibasicversioncontrol.h>
#include <vcs/models/vcsannotationmodel.h>
#include <vcs/vcsjob.h>
#include <vcs/vcspluginhelper.h>

#define EMPTY_DOCUMENT_URL i18n("Untitled")

namespace KDevelop
{


struct DocumentControllerPrivate {
    DocumentControllerPrivate(DocumentController* c)
        : controller(c)
        , fileOpenRecent(0)
        , globalTextEditorInstance(0)
    {
    }

    ~DocumentControllerPrivate()
    {
        //delete temporary files so they are removed from disk
        foreach (QTemporaryFile *temp, tempFiles)
            delete temp;
    }

    // used to map urls to open docs
    QHash< QUrl, IDocument* > documents;

    QHash< QString, IDocumentFactory* > factories;
    QList<QTemporaryFile*> tempFiles;

    struct HistoryEntry
    {
        HistoryEntry() {}
        HistoryEntry( const QUrl & u, const KTextEditor::Cursor& cursor );

        QUrl url;
        KTextEditor::Cursor cursor;
        int id;
    };

    void removeDocument(Sublime::Document *doc)
    {
        QList<QUrl> urlsForDoc = documents.keys(dynamic_cast<KDevelop::IDocument*>(doc));
        foreach (const QUrl &url, urlsForDoc)
        {
            qCDebug(SHELL) << "destroying document" << doc;
            documents.remove(url);
        }
    }

    KEncodingFileDialog::Result showOpenFile() const
    {
        QUrl dir;
        if ( controller->activeDocument() ) {
            dir = controller->activeDocument()->url().adjusted(QUrl::RemoveFilename);
        } else  {
            const auto cfg = KSharedConfig::openConfig()->group("Open File");
            dir = cfg.readEntry( "Last Open File Directory", Core::self()->projectController()->projectsBaseDirectory() );
        }

        return KEncodingFileDialog::getOpenUrlsAndEncoding( QString(), dir, i18n( "*|Text File\n" ),
                                    Core::self()->uiControllerInternal()->defaultMainWindow(),
                                    i18n( "Open File" ) );
    }

    void chooseDocument()
    {
        const auto res = showOpenFile();
        if( !res.URLs.isEmpty() ) {
            QString encoding = res.encoding;
            foreach( const QUrl& u, res.URLs ) {
                openDocumentInternal(u, QString(), KTextEditor::Range::invalid(), encoding  );
            }
        }

    }

    void changeDocumentUrl(KDevelop::IDocument* document)
    {
        QMutableHashIterator<QUrl, IDocument*> it = documents;
        while (it.hasNext()) {
            if (it.next().value() == document) {
                if (documents.contains(document->url())) {
                    // Weird situation (saving as a file that is aready open)
                    IDocument* origDoc = documents[document->url()];
                    if (origDoc->state() & IDocument::Modified) {
                        // given that the file has been saved, close the saved file as the other instance will become conflicted on disk
                        document->close();
                        controller->activateDocument( origDoc );
                        break;
                    }
                    // Otherwise close the original document
                    origDoc->close();
                } else {
                    // Remove the original document
                    it.remove();
                }

                documents.insert(document->url(), document);

                if (!controller->isEmptyDocumentUrl(document->url()))
                {
                    fileOpenRecent->addUrl(document->url());
                }

                break;
            }
        }
    }

    KDevelop::IDocument* findBuddyDocument(const QUrl &url, IBuddyDocumentFinder* finder)
    {
        QList<KDevelop::IDocument*> allDocs = controller->openDocuments();
        foreach( KDevelop::IDocument* doc, allDocs ) {
            if(finder->areBuddies(url, doc->url())) {
                return doc;
            }
        }
        return 0;
    }

    static bool fileExists(const QUrl& url)
    {
        if (url.isLocalFile()) {
            return QFile::exists(url.toLocalFile());
        } else {
            auto job = KIO::stat(url, KIO::StatJob::SourceSide, 0);
            KJobWidgets::setWindow(job, ICore::self()->uiController()->activeMainWindow());
            return job->exec();
        }
    };

    IDocument* openDocumentInternal( const QUrl & inputUrl, const QString& prefName = QString(),
        const KTextEditor::Range& range = KTextEditor::Range::invalid(), const QString& encoding = QString(),
        DocumentController::DocumentActivationParams activationParams = 0,
        IDocument* buddy = 0)
    {
        Q_ASSERT(!inputUrl.isRelative());
        Q_ASSERT(!inputUrl.fileName().isEmpty());
        QString _encoding = encoding;

        QUrl url = inputUrl;

        if ( url.isEmpty() && (!activationParams.testFlag(IDocumentController::DoNotCreateView)) )
        {
            const auto res = showOpenFile();
            if( !res.URLs.isEmpty() )
                url = res.URLs.first();
            _encoding = res.encoding;
            if ( url.isEmpty() )
                //still no url
                return 0;
        }

        KSharedConfig::openConfig()->group("Open File").writeEntry( "Last Open File Directory", url.adjusted(QUrl::RemoveFilename) );

        // clean it and resolve possible symlink
        url = url.adjusted( QUrl::NormalizePathSegments );
        if ( url.isLocalFile() )
        {
            QString path = QFileInfo( url.toLocalFile() ).canonicalFilePath();
            if ( !path.isEmpty() )
                url = QUrl::fromLocalFile( path );
        }

        //get a part document
        IDocument* doc = documents.value(url);
        if (!doc)
        {
            QMimeType mimeType;

            if (DocumentController::isEmptyDocumentUrl(url))
            {
                mimeType = QMimeDatabase().mimeTypeForName("text/plain");
            }
            else if (!url.isValid())
            {
                // Exit if the url is invalid (should not happen)
                // If the url is valid and the file does not already exist,
                // kate creates the file and gives a message saying so
                qCDebug(SHELL) << "invalid URL:" << url.url();
                return 0;
            }
            else if (KProtocolInfo::isKnownProtocol(url.scheme()) && !fileExists(url))
            {
                //Don't create a new file if we are not in the code mode.
                if (ICore::self()->uiController()->activeArea()->objectName() != "code") {
                    return 0;
                }
                // enfore text mime type in order to create a kate part editor which then can be used to create the file
                // otherwise we could end up opening e.g. okteta which then crashes, see: https://bugs.kde.org/id=326434
                mimeType = QMimeDatabase().mimeTypeForName("text/plain");
            }
            else
            {
                mimeType = QMimeDatabase().mimeTypeForUrl(url);

                if(!url.isLocalFile() && mimeType.isDefault())
                {
                    // fall back to text/plain, for remote files without extension, i.e. COPYING, LICENSE, ...
                    // using a synchronous KIO::MimetypeJob is hazardous and may lead to repeated calls to
                    // this function without it having returned in the first place
                    // and this function is *not* reentrant, see assert below:
                    // Q_ASSERT(!documents.contains(url) || documents[url]==doc);
                    mimeType = QMimeDatabase().mimeTypeForName("text/plain");
                }
            }

            // is the URL pointing to a directory?
            if (mimeType.inherits(QStringLiteral("inode/directory")))
            {
                qCDebug(SHELL) << "cannot open directory:" << url.url();
                return 0;
            }

            if( prefName.isEmpty() )
            {
                // Try to find a plugin that handles this mimetype
                QVariantMap constraints;
                constraints.insert("X-KDevelop-SupportedMimeTypes", mimeType.name());
                Core::self()->pluginController()->pluginForExtension(QString(), QString(), constraints);
            }

            if( IDocumentFactory* factory = factories.value(mimeType.name()))
            {
                doc = factory->create(url, Core::self());
            }

            if(!doc) {
                if( !prefName.isEmpty() )
                {
                    doc = new PartDocument(url, Core::self(), prefName);
                } else  if ( Core::self()->partControllerInternal()->isTextType(mimeType))
                {
                    doc = new TextDocument(url, Core::self(), _encoding);
                } else if( Core::self()->partControllerInternal()->canCreatePart(url) )
                {
                    doc = new PartDocument(url, Core::self());
                } else
                {
                    int openAsText = KMessageBox::questionYesNo(0, i18n("KDevelop could not find the editor for file '%1' of type %2.\nDo you want to open it as plain text?", url.fileName(), mimeType.name()), i18nc("@title:window", "Could Not Find Editor"),
                                                                KStandardGuiItem::yes(), KStandardGuiItem::no(), "AskOpenWithTextEditor");
                    if (openAsText == KMessageBox::Yes)
                        doc = new TextDocument(url, Core::self(), _encoding);
                    else
                        return 0;
                }
            }
        }

        // The url in the document must equal the current url, else the housekeeping will get broken
        Q_ASSERT(!doc || doc->url() == url);

        if(doc && openDocumentInternal(doc, range, activationParams, buddy))
            return doc;
        else
            return 0;

    }

    bool openDocumentInternal(IDocument* doc,
                                const KTextEditor::Range& range,
                                DocumentController::DocumentActivationParams activationParams,
                                IDocument* buddy = 0)
    {
        IDocument* previousActiveDocument = controller->activeDocument();
        KTextEditor::View* previousActiveTextView = ICore::self()->documentController()->activeTextDocumentView();
        KTextEditor::Cursor previousActivePosition;
        if(previousActiveTextView)
            previousActivePosition = previousActiveTextView->cursorPosition();

        QUrl url=doc->url();
        UiController *uiController = Core::self()->uiControllerInternal();
        Sublime::Area *area = uiController->activeArea();

        //We can't have the same url in many documents
        //so we check it's already the same if it exists
        //contains=>it's the same
        Q_ASSERT(!documents.contains(url) || documents[url]==doc);

        Sublime::Document *sdoc = dynamic_cast<Sublime::Document*>(doc);
        if( !sdoc )
        {
            documents.remove(url);
            delete doc;
            return false;
        }
        //react on document deletion - we need to cleanup controller structures

        QObject::connect(sdoc, &Sublime::Document::aboutToDelete, controller, &DocumentController::notifyDocumentClosed);
        //We check if it was already opened before
        bool emitOpened = !documents.contains(url);
        if(emitOpened)
            documents[url]=doc;

        if (!activationParams.testFlag(IDocumentController::DoNotCreateView))
        {
            //find a view if there's one already opened in this area
            Sublime::View *partView = 0;
            Sublime::AreaIndex* activeViewIdx = area->indexOf(uiController->activeSublimeWindow()->activeView());
            foreach (Sublime::View *view, sdoc->views())
            {
                Sublime::AreaIndex* areaIdx = area->indexOf(view);
                if (areaIdx && areaIdx == activeViewIdx)
                {
                    partView = view;
                    break;
                }
            }
            bool addView = false;
            if (!partView)
            {
                //no view currently shown for this url
                partView = sdoc->createView();
                addView = true;
            }

            if(addView) {
                // This code is never executed when restoring session on startup,
                // only when opening a file manually

                Sublime::View* buddyView = 0;
                bool placeAfterBuddy = true;
                if(Core::self()->uiControllerInternal()->arrangeBuddies()) {
                    // If buddy is not set, look for a (usually) plugin which handles this URL's mimetype
                    // and use its IBuddyDocumentFinder, if available, to find a buddy document
                    if(!buddy && doc->mimeType().isValid()) {
                        QString mime = doc->mimeType().name();
                        IBuddyDocumentFinder* buddyFinder = IBuddyDocumentFinder::finderForMimeType(mime);
                        if(buddyFinder) {
                            buddy = findBuddyDocument(url, buddyFinder);
                            if(buddy) {
                                placeAfterBuddy = buddyFinder->buddyOrder(buddy->url(), doc->url());
                            }
                        }
                    }

                    if(buddy) {
                        Sublime::Document* sublimeDocBuddy = dynamic_cast<Sublime::Document*>(buddy);

                        if(sublimeDocBuddy) {
                            Sublime::AreaIndex *pActiveViewIndex = area->indexOf(uiController->activeSublimeWindow()->activeView());
                            if(pActiveViewIndex) {
                                // try to find existing View of buddy document in current active view's tab
                                foreach (Sublime::View *pView, pActiveViewIndex->views()) {
                                    if(sublimeDocBuddy->views().contains(pView)) {
                                        buddyView = pView;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                // add view to the area
                if(buddyView && area->indexOf(buddyView)) {
                    if(placeAfterBuddy) {
                        // Adding new view after buddy view, simple case
                        area->addView(partView, area->indexOf(buddyView), buddyView);
                    }
                    else {
                        // First new view, then buddy view
                        area->addView(partView, area->indexOf(buddyView), buddyView);
                        // move buddyView tab after the new document
                        area->removeView(buddyView);
                        area->addView(buddyView, area->indexOf(partView), partView);
                    }
                }
                else {
                    // no buddy found for new document / plugin does not support buddies / buddy feature disabled
                    Sublime::View *activeView = uiController->activeSublimeWindow()->activeView();
                    Sublime::UrlDocument *activeDoc = 0;
                    IBuddyDocumentFinder *buddyFinder = 0;
                    if(activeView)
                        activeDoc = dynamic_cast<Sublime::UrlDocument *>(activeView->document());
                    if(activeDoc && Core::self()->uiControllerInternal()->arrangeBuddies()) {
                        QString mime = QMimeDatabase().mimeTypeForUrl(activeDoc->url()).name();
                        buddyFinder = IBuddyDocumentFinder::finderForMimeType(mime);
                    }

                    if(Core::self()->uiControllerInternal()->openAfterCurrent() &&
                       Core::self()->uiControllerInternal()->arrangeBuddies() &&
                       buddyFinder)
                    {
                        // Check if active document's buddy is directly next to it.
                        // For example, we have the already-open tabs | *foo.h* | foo.cpp | , foo.h is active.
                        // When we open a new document here (and the buddy feature is enabled),
                        // we do not want to separate foo.h and foo.cpp, so we take care and avoid this.
                        Sublime::AreaIndex *activeAreaIndex = area->indexOf(activeView);
                        int pos = activeAreaIndex->views().indexOf(activeView);
                        Sublime::View *afterActiveView = activeAreaIndex->views().value(pos+1, 0);

                        Sublime::UrlDocument *activeDoc = 0, *afterActiveDoc = 0;
                        if(activeView && afterActiveView) {
                            activeDoc = dynamic_cast<Sublime::UrlDocument *>(activeView->document());
                            afterActiveDoc = dynamic_cast<Sublime::UrlDocument *>(afterActiveView->document());
                        }
                        if(activeDoc && afterActiveDoc &&
                           buddyFinder->areBuddies(activeDoc->url(), afterActiveDoc->url()))
                        {
                            // don't insert in between of two buddies, but after them
                            area->addView(partView, activeAreaIndex, afterActiveView);
                        }
                        else {
                            // The active document's buddy is not directly after it
                            // => no ploblem, insert after active document
                            area->addView(partView, activeView);
                        }
                    }
                    else {
                        // Opening as last tab won't disturb our buddies
                        // Same, if buddies are disabled, we needn't care about them.

                        // this method places the tab according to openAfterCurrent()
                        area->addView(partView, activeView);
                    }
                }
            }

            if (!activationParams.testFlag(IDocumentController::DoNotActivate))
            {
                uiController->activeSublimeWindow()->activateView(
                    partView, !activationParams.testFlag(IDocumentController::DoNotFocus));
            }
            if (!controller->isEmptyDocumentUrl(url))
            {
                fileOpenRecent->addUrl( url );
            }

            if( range.isValid() )
            {
                if (range.isEmpty())
                    doc->setCursorPosition( range.start() );
                else
                    doc->setTextSelection( range );
            }
        }

        // Deferred signals, wait until it's all ready first
        if( emitOpened ) {
            emit controller->documentOpened( doc );
        }

        if (!activationParams.testFlag(IDocumentController::DoNotActivate) && doc != controller->activeDocument())
            emit controller->documentActivated( doc );

        saveAll->setEnabled(true);
        revertAll->setEnabled(true);
        close->setEnabled(true);
        closeAll->setEnabled(true);
        closeAllOthers->setEnabled(true);

        KTextEditor::Cursor activePosition;
        if(range.isValid())
            activePosition = range.start();
        else if(KTextEditor::View* v = doc->activeTextView())
            activePosition = v->cursorPosition();

        if (doc != previousActiveDocument || activePosition != previousActivePosition)
            emit controller->documentJumpPerformed(doc, activePosition, previousActiveDocument, previousActivePosition);

        return true;
    }

    DocumentController* controller;

    QList<HistoryEntry> backHistory;
    QList<HistoryEntry> forwardHistory;
    bool isJumping;

    QPointer<QAction> saveAll;
    QPointer<QAction> revertAll;
    QPointer<QAction> close;
    QPointer<QAction> closeAll;
    QPointer<QAction> closeAllOthers;
    KRecentFilesAction* fileOpenRecent;
    KTextEditor::Document* globalTextEditorInstance;
};

DocumentController::DocumentController( QObject *parent )
        : IDocumentController( parent )
{
    setObjectName("DocumentController");
    d = new DocumentControllerPrivate(this);
    QDBusConnection::sessionBus().registerObject( "/org/kdevelop/DocumentController",
        this, QDBusConnection::ExportScriptableSlots );

    connect(this, &DocumentController::documentUrlChanged, this, [&] (IDocument* document) { d->changeDocumentUrl(document); });

    if(!(Core::self()->setupFlags() & Core::NoUi)) setupActions();
}

void DocumentController::initialize()
{
}

void DocumentController::cleanup()
{
    if (d->fileOpenRecent)
        d->fileOpenRecent->saveEntries( KConfigGroup(KSharedConfig::openConfig(), "Recent Files" ) );

    // Close all documents without checking if they should be saved.
    // This is because the user gets a chance to save them during MainWindow::queryClose.
    foreach (IDocument* doc, openDocuments())
        doc->close(IDocument::Discard);
}

DocumentController::~DocumentController()
{
    delete d;
}

void DocumentController::setupActions()
{
    KActionCollection* ac = Core::self()->uiControllerInternal()->defaultMainWindow()->actionCollection();

    QAction* action;

    action = ac->addAction( "file_open" );
    action->setIcon(QIcon::fromTheme("document-open"));
    ac->setDefaultShortcut(action, Qt::CTRL + Qt::Key_O );
    action->setText(i18n( "&Open..." ) );
    connect( action, &QAction::triggered, this, [&] { d->chooseDocument(); } );
    action->setToolTip( i18n( "Open file" ) );
    action->setWhatsThis( i18n( "Opens a file for editing." ) );

    d->fileOpenRecent = KStandardAction::openRecent(this,
                    SLOT(slotOpenDocument(QUrl)), ac);
    d->fileOpenRecent->setWhatsThis(i18n("This lists files which you have opened recently, and allows you to easily open them again."));
    d->fileOpenRecent->loadEntries( KConfigGroup(KSharedConfig::openConfig(), "Recent Files" ) );

    action = d->saveAll = ac->addAction( "file_save_all" );
    action->setIcon(QIcon::fromTheme("document-save"));
    action->setText(i18n( "Save Al&l" ) );
    connect( action, &QAction::triggered, this, &DocumentController::slotSaveAllDocuments );
    action->setToolTip( i18n( "Save all open documents" ) );
    action->setWhatsThis( i18n( "Save all open documents, prompting for additional information when necessary." ) );
    ac->setDefaultShortcut(action, QKeySequence(Qt::CTRL + Qt::Key_L) );
    action->setEnabled(false);

    action = d->revertAll = ac->addAction( "file_revert_all" );
    action->setIcon(QIcon::fromTheme("document-revert"));
    action->setText(i18n( "Reload All" ) );
    connect( action, &QAction::triggered, this, &DocumentController::reloadAllDocuments );
    action->setToolTip( i18n( "Revert all open documents" ) );
    action->setWhatsThis( i18n( "Revert all open documents, returning to the previously saved state." ) );
    action->setEnabled(false);

    action = d->close = ac->addAction( "file_close" );
    action->setIcon(QIcon::fromTheme("window-close"));
    ac->setDefaultShortcut(action, Qt::CTRL + Qt::Key_W );
    action->setText( i18n( "&Close" ) );
    connect( action, &QAction::triggered, this, &DocumentController::fileClose );
    action->setToolTip( i18n( "Close file" ) );
    action->setWhatsThis( i18n( "Closes current file." ) );
    action->setEnabled(false);

    action = d->closeAll = ac->addAction( "file_close_all" );
    action->setIcon(QIcon::fromTheme("window-close"));
    action->setText(i18n( "Clos&e All" ) );
    connect( action, &QAction::triggered, this, &DocumentController::closeAllDocuments );
    action->setToolTip( i18n( "Close all open documents" ) );
    action->setWhatsThis( i18n( "Close all open documents, prompting for additional information when necessary." ) );
    action->setEnabled(false);

    action = d->closeAllOthers = ac->addAction( "file_closeother" );
    action->setIcon(QIcon::fromTheme("window-close"));
    ac->setDefaultShortcut(action, Qt::CTRL + Qt::SHIFT + Qt::Key_W );
    action->setText(i18n( "Close All Ot&hers" ) );
    connect( action, &QAction::triggered, this, &DocumentController::closeAllOtherDocuments );
    action->setToolTip( i18n( "Close all other documents" ) );
    action->setWhatsThis( i18n( "Close all open documents, with the exception of the currently active document." ) );
    action->setEnabled(false);

    action = ac->addAction( "vcsannotate_current_document" );
    connect( action, &QAction::triggered, this, &DocumentController::vcsAnnotateCurrentDocument );
    action->setText( i18n( "Show Annotate on current document") );
    action->setIconText( i18n( "Annotate" ) );
    action->setIcon( QIcon::fromTheme("user-properties") );
}

void DocumentController::slotOpenDocument(const QUrl &url)
{
    openDocument(url);
}

IDocument* DocumentController::openDocumentFromText( const QString& data )
{
    IDocument* d = openDocument(nextEmptyDocumentUrl());
    Q_ASSERT(d->textDocument());
    d->textDocument()->setText( data );
    return d;
}

bool DocumentController::openDocumentFromTextSimple( QString text )
{
    return (bool)openDocumentFromText( text );
}

bool DocumentController::openDocumentSimple( QString url, int line, int column )
{
    return (bool)openDocument( QUrl::fromUserInput(url), KTextEditor::Cursor( line, column ) );
}

IDocument* DocumentController::openDocument( const QUrl& inputUrl, const QString& prefName )
{
    return d->openDocumentInternal( inputUrl, prefName );
}

IDocument* DocumentController::openDocument( const QUrl & inputUrl,
        const KTextEditor::Range& range,
        DocumentActivationParams activationParams,
        const QString& encoding, IDocument* buddy)
{
    return d->openDocumentInternal( inputUrl, "", range, encoding, activationParams, buddy);
}


bool DocumentController::openDocument(IDocument* doc,
                                      const KTextEditor::Range& range,
                                      DocumentActivationParams activationParams,
                                      IDocument* buddy)
{
    return d->openDocumentInternal( doc, range, activationParams, buddy);
}


void DocumentController::fileClose()
{
    IDocument *activeDoc = activeDocument();
    if (activeDoc)
    {
        UiController *uiController = Core::self()->uiControllerInternal();
        Sublime::View *activeView = uiController->activeSublimeWindow()->activeView();

        uiController->activeArea()->closeView(activeView);
    }
}

void DocumentController::closeDocument( const QUrl &url )
{
    if( !d->documents.contains(url) )
        return;

    //this will remove all views and after the last view is removed, the
    //document will be self-destructed and removeDocument() slot will catch that
    //and clean up internal data structures
    d->documents[url]->close();
}

void DocumentController::notifyDocumentClosed(Sublime::Document* doc_)
{
    IDocument* doc = dynamic_cast<IDocument*>(doc_);
    Q_ASSERT(doc);

    d->removeDocument(doc_);

    if (d->documents.isEmpty()) {
        if (d->saveAll)
            d->saveAll->setEnabled(false);
        if (d->revertAll)
            d->revertAll->setEnabled(false);
        if (d->close)
            d->close->setEnabled(false);
        if (d->closeAll)
            d->closeAll->setEnabled(false);
        if (d->closeAllOthers)
            d->closeAllOthers->setEnabled(false);
    }

    emit documentClosed(doc);
}

IDocument * DocumentController::documentForUrl( const QUrl & dirtyUrl ) const
{
    if (dirtyUrl.isEmpty()) {
        return nullptr;
    }
    Q_ASSERT(!dirtyUrl.isRelative());
    Q_ASSERT(!dirtyUrl.fileName().isEmpty());
    //Fix urls that might not be normalized
    return d->documents.value( dirtyUrl.adjusted( QUrl::NormalizePathSegments ), 0 );
}

QList<IDocument*> DocumentController::openDocuments() const
{
    QList<IDocument*> opened;
    foreach (IDocument *doc, d->documents)
    {
        Sublime::Document *sdoc = dynamic_cast<Sublime::Document*>(doc);
        if( !sdoc )
        {
            continue;
        }
        if (!sdoc->views().isEmpty())
            opened << doc;
    }
    return opened;
}

void DocumentController::activateDocument( IDocument * document, const KTextEditor::Range& range )
{
    // TODO avoid some code in openDocument?
    Q_ASSERT(document);
    openDocument(document->url(), range);
}

void DocumentController::slotSaveAllDocuments()
{
    saveAllDocuments(IDocument::Silent);
}

bool DocumentController::saveAllDocuments(IDocument::DocumentSaveMode mode)
{
    return saveSomeDocuments(openDocuments(), mode);
}

bool KDevelop::DocumentController::saveSomeDocuments(const QList< IDocument * > & list, IDocument::DocumentSaveMode mode)
{
    if (mode & IDocument::Silent) {
        foreach (IDocument* doc, modifiedDocuments(list)) {
            if( !DocumentController::isEmptyDocumentUrl(doc->url()) && !doc->save(mode) )
            {
                if( doc )
                    qWarning() << "!! Could not save document:" << doc->url();
                else
                    qWarning() << "!! Could not save document as its NULL";
            }
            // TODO if (!ret) showErrorDialog() ?
        }

    } else {
        // Ask the user which documents to save
        QList<IDocument*> checkSave = modifiedDocuments(list);

        if (!checkSave.isEmpty()) {
            KSaveSelectDialog dialog(checkSave, qApp->activeWindow());
            if (dialog.exec() == QDialog::Rejected)
                return false;
        }
    }

    return true;
}

QList< IDocument * > KDevelop::DocumentController::visibleDocumentsInWindow(MainWindow * mw) const
{
    // Gather a list of all documents which do have a view in the given main window
    // Does not find documents which are open in inactive areas
    QList<IDocument*> list;
    foreach (IDocument* doc, openDocuments()) {
        if (Sublime::Document* sdoc = dynamic_cast<Sublime::Document*>(doc)) {
            foreach (Sublime::View* view, sdoc->views()) {
                if (view->hasWidget() && view->widget()->window() == mw) {
                    list.append(doc);
                    break;
                }
            }
        }
    }
    return list;
}

QList< IDocument * > KDevelop::DocumentController::documentsExclusivelyInWindow(MainWindow * mw, bool currentAreaOnly) const
{
    // Gather a list of all documents which have views only in the given main window
    QList<IDocument*> checkSave;

    foreach (IDocument* doc, openDocuments()) {
        if (Sublime::Document* sdoc = dynamic_cast<Sublime::Document*>(doc)) {
            bool inOtherWindow = false;

            foreach (Sublime::View* view, sdoc->views()) {
                foreach(Sublime::MainWindow* window, Core::self()->uiControllerInternal()->mainWindows())
                    if(window->containsView(view) && (window != mw || (currentAreaOnly && window == mw && !mw->area()->views().contains(view))))
                        inOtherWindow = true;
            }

            if (!inOtherWindow)
                checkSave.append(doc);
        }
    }
    return checkSave;
}

QList< IDocument * > KDevelop::DocumentController::modifiedDocuments(const QList< IDocument * > & list) const
{
    QList< IDocument * > ret;
    foreach (IDocument* doc, list)
        if (doc->state() == IDocument::Modified || doc->state() == IDocument::DirtyAndModified)
            ret.append(doc);
    return ret;
}

bool DocumentController::saveAllDocumentsForWindow(KParts::MainWindow* mw, KDevelop::IDocument::DocumentSaveMode mode, bool currentAreaOnly)
{
    QList<IDocument*> checkSave = documentsExclusivelyInWindow(dynamic_cast<KDevelop::MainWindow*>(mw), currentAreaOnly);

    return saveSomeDocuments(checkSave, mode);
}

void DocumentController::reloadAllDocuments()
{
    if (Sublime::MainWindow* mw = Core::self()->uiControllerInternal()->activeSublimeWindow()) {
        QList<IDocument*> views = visibleDocumentsInWindow(dynamic_cast<KDevelop::MainWindow*>(mw));

        if (!saveSomeDocuments(views, IDocument::Default))
            // User cancelled or other error
            return;

        foreach (IDocument* doc, views)
            if(!isEmptyDocumentUrl(doc->url()))
                doc->reload();
    }
}

bool DocumentController::closeAllDocuments()
{
    if (Sublime::MainWindow* mw = Core::self()->uiControllerInternal()->activeSublimeWindow()) {
        QList<IDocument*> views = visibleDocumentsInWindow(dynamic_cast<KDevelop::MainWindow*>(mw));

        if (!saveSomeDocuments(views, IDocument::Default))
            // User cancelled or other error
            return false;

        foreach (IDocument* doc, views)
            doc->close(IDocument::Discard);
    }
    return true;
}

void DocumentController::closeAllOtherDocuments()
{
    if (Sublime::MainWindow* mw = Core::self()->uiControllerInternal()->activeSublimeWindow()) {
        Sublime::View* activeView = mw->activeView();

        if (!activeView) {
            qWarning() << "Shouldn't there always be an active view when this function is called?";
            return;
        }

        // Deal with saving unsaved solo views
        QList<IDocument*> soloViews = documentsExclusivelyInWindow(dynamic_cast<KDevelop::MainWindow*>(mw));
        soloViews.removeAll(dynamic_cast<IDocument*>(activeView->document()));

        if (!saveSomeDocuments(soloViews, IDocument::Default))
            // User cancelled or other error
            return;

        foreach (Sublime::View* view, mw->area()->views()) {
            if (view != activeView)
                mw->area()->closeView(view);
        }
        activeView->widget()->setFocus();
    }
}

IDocument* DocumentController::activeDocument() const
{
    UiController *uiController = Core::self()->uiControllerInternal();
    Sublime::MainWindow* mw = uiController->activeSublimeWindow();
    if( !mw || !mw->activeView() ) return 0;
    return dynamic_cast<IDocument*>(mw->activeView()->document());
}

KTextEditor::View* DocumentController::activeTextDocumentView() const
{
    UiController *uiController = Core::self()->uiControllerInternal();
    Sublime::MainWindow* mw = uiController->activeSublimeWindow();
    if( !mw || !mw->activeView() )
        return 0;

    TextView* view = qobject_cast<TextView*>(mw->activeView());
    if(!view)
        return 0;
    return view->textView();
}

QString DocumentController::activeDocumentPath( QString target ) const
{
    if(target.size()) {
        foreach(IProject* project, Core::self()->projectController()->projects()) {
            if(project->name().startsWith(target, Qt::CaseInsensitive)) {
                return project->path().pathOrUrl() + "/.";
            }
        }
    }
    IDocument* doc = activeDocument();
    if(!doc || target == "[selection]")
    {
        Context* selection = ICore::self()->selectionController()->currentSelection();
        if(selection && selection->type() == Context::ProjectItemContext && static_cast<ProjectItemContext*>(selection)->items().size())
        {
            QString ret = static_cast<ProjectItemContext*>(selection)->items()[0]->path().pathOrUrl();
            if(static_cast<ProjectItemContext*>(selection)->items()[0]->folder())
                ret += "/.";
            return  ret;
        }
        return QString();
    }
    return doc->url().toString();
}

QStringList DocumentController::activeDocumentPaths() const
{
    UiController *uiController = Core::self()->uiControllerInternal();
    if( !uiController->activeSublimeWindow() ) return QStringList();

    QSet<QString> documents;
    foreach(Sublime::View* view, uiController->activeSublimeWindow()->area()->views())
        documents.insert(view->document()->documentSpecifier());

    return documents.toList();
}

void DocumentController::registerDocumentForMimetype( const QString& mimetype,
                                        KDevelop::IDocumentFactory* factory )
{
    if( !d->factories.contains( mimetype ) )
        d->factories[mimetype] = factory;
}

QStringList DocumentController::documentTypes() const
{
    return QStringList() << "Text";
}

static const QRegularExpression& emptyDocumentPattern()
{
    static const QRegularExpression pattern(QStringLiteral("^/%1(?:\\s\\((\\d+)\\))?$").arg(EMPTY_DOCUMENT_URL));
    return pattern;
}

bool DocumentController::isEmptyDocumentUrl(const QUrl &url)
{
    return emptyDocumentPattern().match(url.toDisplayString(QUrl::PreferLocalFile)).hasMatch();
}

QUrl DocumentController::nextEmptyDocumentUrl()
{
    int nextEmptyDocNumber = 0;
    const auto& pattern = emptyDocumentPattern();
    foreach (IDocument *doc, Core::self()->documentControllerInternal()->openDocuments()) {
        if (DocumentController::isEmptyDocumentUrl(doc->url())) {
            const auto match = pattern.match(doc->url().toDisplayString(QUrl::PreferLocalFile));
            if (match.hasMatch()) {
                const int num = match.captured(1).toInt();
                nextEmptyDocNumber = qMax(nextEmptyDocNumber, num + 1);
            } else {
                nextEmptyDocNumber = qMax(nextEmptyDocNumber, 1);
            }
        }
    }

    QUrl url;
    if (nextEmptyDocNumber > 0)
        url = QUrl::fromLocalFile(QStringLiteral("/%1 (%2)").arg(EMPTY_DOCUMENT_URL).arg(nextEmptyDocNumber));
    else
        url = QUrl::fromLocalFile('/' + EMPTY_DOCUMENT_URL);
    return url;
}

IDocumentFactory* DocumentController::factory(const QString& mime) const
{
    return d->factories.value(mime);
}

KTextEditor::Document* DocumentController::globalTextEditorInstance()
{
    if(!d->globalTextEditorInstance)
        d->globalTextEditorInstance = Core::self()->partControllerInternal()->createTextPart();
    return d->globalTextEditorInstance;
}

bool DocumentController::openDocumentsSimple( QStringList urls )
{
    Sublime::Area* area = Core::self()->uiControllerInternal()->activeArea();
    Sublime::AreaIndex* areaIndex = area->rootIndex();

    QList<Sublime::View*> topViews = static_cast<Sublime::MainWindow*>(Core::self()->uiControllerInternal()->activeMainWindow())->getTopViews();

    if(Sublime::View* activeView = Core::self()->uiControllerInternal()->activeSublimeWindow()->activeView())
        areaIndex = area->indexOf(activeView);

    qCDebug(SHELL) << "opening " << urls << " to area " << area << " index " << areaIndex << " with children " << areaIndex->first() << " " << areaIndex->second();

    bool isFirstView = true;

    bool ret = openDocumentsWithSplitSeparators( areaIndex, urls, isFirstView );

    qCDebug(SHELL) << "area arch. after opening: " << areaIndex->print();

    // Required because sublime sometimes doesn't update correctly when the area-index contents has been changed
    // (especially when views have been moved to other indices, through unsplit, split, etc.)
    static_cast<Sublime::MainWindow*>(Core::self()->uiControllerInternal()->activeMainWindow())->reconstructViews(topViews);

    return ret;
}

bool DocumentController::openDocumentsWithSplitSeparators( Sublime::AreaIndex* index, QStringList urlsWithSeparators, bool& isFirstView )
{
    qCDebug(SHELL) << "opening " << urlsWithSeparators << " index " << index << " with children " << index->first() << " " << index->second() << " view-count " << index->viewCount();
    if(urlsWithSeparators.isEmpty())
        return true;

    Sublime::Area* area = Core::self()->uiControllerInternal()->activeArea();

    QList<int> topLevelSeparators; // Indices of the top-level separators (with groups skipped)
    QStringList separators = QStringList() << "/" << "-";
    QList<QStringList> groups;

    bool ret = true;

    {
        int parenDepth = 0;
        int groupStart = 0;
        for(int pos = 0; pos < urlsWithSeparators.size(); ++pos)
        {
            QString item = urlsWithSeparators[pos];
            if(separators.contains(item))
            {
                if(parenDepth == 0)
                    topLevelSeparators << pos;
            }else if(item == "[")
            {
                if(parenDepth == 0)
                    groupStart = pos+1;
                ++parenDepth;
            }
            else if(item == "]")
            {
                if(parenDepth > 0)
                {
                    --parenDepth;

                    if(parenDepth == 0)
                        groups << urlsWithSeparators.mid(groupStart, pos-groupStart);
                }
                else{
                    qCDebug(SHELL) << "syntax error in " << urlsWithSeparators << ": parens do not match";
                    ret = false;
                }
            }else if(parenDepth == 0)
            {
                groups << (QStringList() << item);
            }
        }
    }

    if(topLevelSeparators.isEmpty())
    {
        if(urlsWithSeparators.size() > 1)
        {
            foreach(const QStringList& group, groups)
                ret &= openDocumentsWithSplitSeparators( index, group, isFirstView );
        }else{
            while(index->isSplit())
                index = index->first();
            // Simply open the document into the area index
            IDocument* doc = Core::self()->documentControllerInternal()->openDocument(QUrl::fromUserInput(urlsWithSeparators.front()),
                        KTextEditor::Cursor::invalid(),
                        IDocumentController::DoNotActivate | IDocumentController::DoNotCreateView);
            Sublime::Document *sublimeDoc = dynamic_cast<Sublime::Document*>(doc);
            if (sublimeDoc) {
                Sublime::View* view = sublimeDoc->createView();
                area->addView(view, index);
                if(isFirstView)
                {
                    static_cast<Sublime::MainWindow*>(Core::self()->uiControllerInternal()->activeMainWindow())->activateView(view);
                    isFirstView = false;
                }
            }else{
                ret = false;
            }
        }
        return ret;
    }

    // Pick a separator in the middle

    int pickSeparator = topLevelSeparators[topLevelSeparators.size()/2];

    bool activeViewToSecondChild = false;
    if(pickSeparator == urlsWithSeparators.size()-1)
    {
        // There is no right child group, so the right side should be filled with the currently active views
        activeViewToSecondChild = true;
    }else{
        QStringList separatorsAndParens = separators;
        separatorsAndParens << "[" << "]";
        // Check if the second child-set contains an unterminated separator, which means that the active views should end up there
        for(int pos = pickSeparator+1; pos < urlsWithSeparators.size(); ++pos)
            if( separators.contains(urlsWithSeparators[pos]) && (pos == urlsWithSeparators.size()-1 ||
                separatorsAndParens.contains(urlsWithSeparators[pos-1]) ||
                separatorsAndParens.contains(urlsWithSeparators[pos-1])) )
                    activeViewToSecondChild = true;
    }

    Qt::Orientation orientation = urlsWithSeparators[pickSeparator] == "/" ? Qt::Horizontal : Qt::Vertical;

    if(!index->isSplit())
    {
        qCDebug(SHELL) << "splitting " << index << "orientation" << orientation << "to second" << activeViewToSecondChild;
        index->split(orientation, activeViewToSecondChild);
    }else{
        index->setOrientation(orientation);
        qCDebug(SHELL) << "WARNING: Area is already split (shouldn't be)" << urlsWithSeparators;
    }

    openDocumentsWithSplitSeparators( index->first(), urlsWithSeparators.mid(0, pickSeparator) , isFirstView );
    if(pickSeparator != urlsWithSeparators.size() - 1)
        openDocumentsWithSplitSeparators( index->second(), urlsWithSeparators.mid(pickSeparator+1, urlsWithSeparators.size() - (pickSeparator+1) ), isFirstView );

    // Clean up the child-indices, because document-loading may fail

    if(!index->first()->viewCount() && !index->first()->isSplit())
    {
        qCDebug(SHELL) << "unsplitting first";
        index->unsplit(index->first());
    }
    else if(!index->second()->viewCount() && !index->second()->isSplit())
    {
        qCDebug(SHELL) << "unsplitting second";
        index->unsplit(index->second());
    }

    return ret;
}

void DocumentController::vcsAnnotateCurrentDocument()
{
    IDocument* doc = activeDocument();
    QUrl url = doc->url();
    IProject* project = KDevelop::ICore::self()->projectController()->findProjectForUrl(url);
    if(project && project->versionControlPlugin()) {
        IBasicVersionControl* iface = 0;
        iface = project->versionControlPlugin()->extension<IBasicVersionControl>();
        auto helper = new VcsPluginHelper(project->versionControlPlugin(), iface);
        connect(doc->textDocument(), &KTextEditor::Document::aboutToClose,
                helper, static_cast<void(VcsPluginHelper::*)(KTextEditor::Document*)>(&VcsPluginHelper::disposeEventually));
        Q_ASSERT(qobject_cast<KTextEditor::AnnotationViewInterface*>(doc->activeTextView()));
        // can't use new signal slot syntax here, AnnotationViewInterface is not a QObject
        connect(doc->activeTextView(), SIGNAL(annotationBorderVisibilityChanged(View*,bool)),
                helper, SLOT(disposeEventually(View*, bool)));
        helper->addContextDocument(url);
        helper->annotation();
    }
    else {
        KMessageBox::error(0, i18n("Could not annotate the document because it is not "
                                   "part of a version-controlled project."));
    }
}

}

#include "moc_documentcontroller.cpp"
