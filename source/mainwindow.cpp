/***************************************************************************
 *   copyright       : (C) 2013 by Quentin BRAMAS                          *
 *   http://texiteasy.com                                                  *
 *                                                                         *
 *   This file is part of texiteasy.                                       *
 *                                                                         *
 *   texiteasy is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   texiteasy is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with texiteasy.  If not, see <http://www.gnu.org/licenses/>.    *
 *                                                                         *
 ***************************************************************************/

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "widgetlinenumber.h"
#include "widgetpdfviewer.h"
#include "widgettextedit.h"
#include "widgetscroller.h"
#include "syntaxhighlighter.h"
#include "file.h"
#include "builder.h"
#include "dialogwelcome.h"
#include "dialogconfig.h"
#include "viewer.h"
#include "widgetpdfdocument.h"
#include "dialogclose.h"
#include "widgetfindreplace.h"
#include "minisplitter.h"
#include "widgetsimpleoutput.h"
#include "widgetproject.h"
#include "macroengine.h"
#include "dialogsendfeedback.h"
#include "updatechecker.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QMenu>
#include <QAction>
#include <QScrollBar>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QSettings>
#include <QPushButton>
#include <QDebug>
#include <QUrl>
#include <QMimeData>
#include <QString>
#include <QPalette>
#include <QPixmap>
#include <QTableWidget>
#include <QMessageBox>
#include <QInputDialog>
#include <QDrag>
#include <QDir>
#include "configmanager.h"
#include "widgetconsole.h"
#include "widgetstatusbar.h"
#include "dialogabout.h"
#include "widgetfile.h"
#include "filemanager.h"
#include "widgettab.h"
#include "tools.h"

#include <QList>

typedef QList<int> IntegerList;
Q_DECLARE_METATYPE(IntegerList)

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    _confirmCloseMessageBox(0),
    dialogConfig(new DialogConfig(this)),
    dialogWelcome(new DialogWelcome(this)),
    _emptyWidget(new WidgetEmpty(0)),
    _menuMacrosAction(0)
{

    Tools::Log("MainWindow: setupUi");
    ui->setupUi(this);
    // remove grammar checking (not ready yet)
    ui->actionCheckGrammar->setVisible(false);
    Tools::Log("MainWindow: ConfigManager::Instance.setMainWindow(this)");
    ConfigManager::Instance.setMainWindow(this);
    Tools::Log("MainWindow: FileManager::Instance.setMainWindow(this)");
    FileManager::Instance.setMainWindow(this);
    _tabWidget = new WidgetTab();
    connect(_tabWidget, SIGNAL(currentChanged(WidgetFile*)), this, SLOT(onCurrentFileChanged(WidgetFile*)));
    connect(_tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));
    connect(_tabWidget, SIGNAL(newTabRequested()), this, SLOT(newFile()));
    ui->verticalLayout->setMargin(0);
    ui->verticalLayout->setSpacing(0);
    ui->verticalLayout->setContentsMargins(0,0,0,0);
    ui->verticalLayout->addWidget(_tabWidget);
    ui->verticalLayout->addWidget(_emptyWidget);
    connect(_emptyWidget, SIGNAL(mouseDoubleClick()), this, SLOT(newFile()));


    _widgetStatusBar = new WidgetStatusBar(this);
    ui->actionLinkSync->setChecked(ConfigManager::Instance.isPdfSynchronized());
    _widgetStatusBar->setLinkSyncAction(ui->actionLinkSync);

    ui->actionPdfViewerInItsOwnWidget->setChecked(ConfigManager::Instance.pdfViewerInItsOwnWidget());
    _widgetStatusBar->setPdfViewerWidgetAction(ui->actionPdfViewerInItsOwnWidget);

    ui->actionSplitEditor->setChecked(ConfigManager::Instance.splitEditor());
    _widgetStatusBar->setSplitEditorAction(ui->actionSplitEditor);
    this->setStatusBar(_widgetStatusBar);

    connect(&FileManager::Instance, SIGNAL(currentFileModified(bool)), this, SLOT(setWindowModified(bool)));
    connect(&FileManager::Instance, SIGNAL(cursorPositionChanged(int,int)), _widgetStatusBar, SLOT(cursorPositionChanged(int,int)));
    connect(&FileManager::Instance, SIGNAL(messageFromCurrentFile(QString)), _widgetStatusBar, SLOT(showTemporaryMessage(QString)));
    connect(&FileManager::Instance, SIGNAL(requestOpenFile(QString)), this, SLOT(open(QString)));
    QSettings settings;
    settings.beginGroup("mainwindow");
    if(settings.contains("geometry"))
    {
        this->setGeometry(settings.value("geometry").toRect());
    }

    //define background
    this->initTheme();


    Tools::Log("MainWindow: connect actions");
    // Connect menubar Actions
    connect(this->ui->actionDisplayHelp, SIGNAL(triggered()), this, SLOT(displayHelp()));
    connect(this->ui->actionLinkSync, SIGNAL(toggled(bool)), &FileManager::Instance, SLOT(setPdfSynchronized(bool)));
    connect(this->ui->actionLinkSync, SIGNAL(toggled(bool)), &ConfigManager::Instance, SLOT(setPdfSynchronized(bool)));
    connect(this->ui->actionPdfViewerInItsOwnWidget, SIGNAL(toggled(bool)), &FileManager::Instance, SLOT(setPdfViewerInItsOwnWidget(bool)));
    connect(this->ui->actionPdfViewerInItsOwnWidget, SIGNAL(toggled(bool)), &ConfigManager::Instance, SLOT(setPdfViewerInItsOwnWidget(bool)));
    connect(this->ui->actionDeleteLastOpenFiles,SIGNAL(triggered()),this,SLOT(clearLastOpened()));
    connect(this->ui->actionNouveau,SIGNAL(triggered()),this,SLOT(newFile()));
    connect(this->ui->actionExit, SIGNAL(triggered()), this, SLOT(close()));
    connect(this->ui->actionAbout, SIGNAL(triggered()), new DialogAbout(this), SLOT(show()));
    connect(this->ui->actionOpen,SIGNAL(triggered()),this,SLOT(open()));
    connect(this->ui->actionOpenLastSession,SIGNAL(triggered()),this,SLOT(openLastSession()));
    connect(this->ui->actionSave,SIGNAL(triggered()),&FileManager::Instance,SLOT(save()));
    connect(this->ui->actionSaveAs,SIGNAL(triggered()),&FileManager::Instance,SLOT(saveAs()));
    connect(this->ui->actionOpenConfigFolder, SIGNAL(triggered()), &ConfigManager::Instance, SLOT(openThemeFolder()));
    connect(this->ui->actionSettings,SIGNAL(triggered()),this->dialogConfig,SLOT(show()));

    connect(this->dialogConfig,SIGNAL(accepted()), &FileManager::Instance,SLOT(rehighlight()));
    connect(this->ui->actionUndo, SIGNAL(triggered()), &FileManager::Instance, SLOT(undo()));
    connect(this->ui->actionRedo, SIGNAL(triggered()), &FileManager::Instance, SLOT(redo()));
    connect(this->ui->actionCopy, SIGNAL(triggered()), &FileManager::Instance, SLOT(copy()));
    connect(this->ui->actionCut, SIGNAL(triggered()), &FileManager::Instance, SLOT(cut()));
    connect(this->ui->actionPaste, SIGNAL(triggered()), &FileManager::Instance, SLOT(paste()));
    connect(this->ui->actionFindReplace, SIGNAL(triggered()), &FileManager::Instance, SLOT(openFindReplaceWidget()));
    connect(this->ui->actionDefaultCommandLatex,SIGNAL(triggered()), &FileManager::Instance,SLOT(builTex()));
    connect(this->ui->actionBibtex,SIGNAL(triggered()), &FileManager::Instance,SLOT(bibtex()));
    connect(this->ui->actionClean,SIGNAL(triggered()), &FileManager::Instance,SLOT(clean()));
    connect(this->ui->actionView, SIGNAL(triggered()), &FileManager::Instance,SLOT(jumpToPdfFromSource()));
    this->ui->actionSplitEditor->setProperty("role", "splitEditor");
    connect(this->ui->actionSplitEditor, SIGNAL(toggled(bool)), &FileManager::Instance,SLOT(splitEditor(bool)));
    connect(this->ui->actionOpenPdf, SIGNAL(triggered()), &FileManager::Instance,SLOT(openCurrentPdf()));
    connect(this->ui->actionComment, SIGNAL(triggered()), &FileManager::Instance,SLOT(comment()));
    connect(this->ui->actionUncomment, SIGNAL(triggered()), &FileManager::Instance,SLOT(uncomment()));
    connect(this->ui->actionToggleComment, SIGNAL(triggered()), &FileManager::Instance,SLOT(toggleComment()));
    connect(this->ui->actionCheckGrammar, SIGNAL(triggered()), &FileManager::Instance,SLOT(checkGrammar()));
    connect(this->ui->actionSaveWithUTF8, SIGNAL(triggered()), this,SLOT(setUtf8()));
    connect(this->ui->actionSaveWithOtherEncoding, SIGNAL(triggered()), this,SLOT(setOtherEncoding()));
    connect(this->ui->actionTexDirEncoding, SIGNAL(triggered()), this, SLOT(insertTexDirEncoding()));
    connect(this->ui->actionTexDirProgram, SIGNAL(triggered()), this, SLOT(insertTexDirProgram()));
    connect(this->ui->actionTexDirRoot, SIGNAL(triggered()), this, SLOT(insertTexDirRoot()));
    connect(this->ui->actionTexDirSpellChecker, SIGNAL(triggered()), this, SLOT(insertTexDirSpellCheck()));
    connect(this->ui->actionSendFeedback, SIGNAL(triggered()), this, SLOT(openSendFeedbackDialog()));
    connect(&FileManager::Instance, SIGNAL(filenameChanged(QString)), this, SLOT(onFilenameChanged(QString)));

    connect(&ConfigManager::Instance, SIGNAL(versionIsOutdated()), this, SLOT(addUpdateMenu()));

    Tools::Log("MainWindow: create reopen");
    QMenu * parentMenu = this->ui->menuReopenWithEncoding;
    foreach(const QString& codec, ConfigManager::CodecsAvailable)
    {
        QAction * action;
        if(codec.contains('/'))
        {
            QStringList l = codec.split('/');
            if(l.first().compare("-"))
            {
                parentMenu = new QMenu(l.first());
                this->ui->menuReopenWithEncoding->addMenu(parentMenu);
            }
            action = new QAction(l.at(1), parentMenu);
        }
        else
        {
            parentMenu =this->ui->menuReopenWithEncoding;
            action = new QAction(codec, parentMenu);
        }
        if(action->text().isEmpty())
        {
            delete action;
            parentMenu->addSeparator();
        }
        else
        {
            action->setPriority(QAction::LowPriority);
            parentMenu->addAction(action);
            connect(action, SIGNAL(triggered()), this, SLOT(reopenWithEncoding()));
        }
    }


    Tools::Log("MainWindow: create themes menu");
    QAction * lastAction = this->ui->menuTh_me->actions().last();
    foreach(const QString& theme, ConfigManager::Instance.themesList())
    {
        QAction * action = new QAction(theme, this->ui->menuTh_me);
        action->setPriority(QAction::LowPriority);
        action->setCheckable(true);
        if(!theme.compare(ConfigManager::Instance.theme()))
        {
            action->setChecked(true);
        }
        this->ui->menuTh_me->insertAction(lastAction,action);
        connect(action, SIGNAL(triggered()), this, SLOT(changeTheme()));
    }
    this->ui->menuTh_me->insertSeparator(lastAction);

    settings.endGroup();

    Tools::Log("MainWindow: createLastOpenedFilesMenu()");
    createLastOpenedFilesMenu();

    Tools::Log("MainWindow: init shortcuts");
    {
        QList<QAction *> actionsList = this->findChildren<QAction *>();
        QSettings settings;
        settings.beginGroup("shortcuts");
        foreach(QAction * action, actionsList) {
            if(!action->text().isEmpty() && settings.contains(action->objectName()))
            {
                QList<QKeySequence> l;
                foreach(const QString &s, settings.value(action->objectName()).toStringList())
                {
                    l << QKeySequence(s, QKeySequence::NativeText);
                }
                action->setShortcuts(l);
            }
        }
    }

    Tools::Log("MainWindow: initMacrosMenu()");
    initMacrosMenu();
    connect(&MacroEngine::Instance, SIGNAL(changed()), this, SLOT(initMacrosMenu()));

    Tools::Log("MainWindow: initBuildMenu()");
    initBuildMenu();
    connect(&ConfigManager::Instance, SIGNAL(changed()), this, SLOT(initBuildMenu()));

    setAcceptDrops(true);

    if(ConfigManager::Instance.openLastSessionAtStartup())
    {
        Tools::Log("MainWindow: openLastSession()");
        this->openLastSession();
    }
}

MainWindow::~MainWindow()
{
    QSettings settings;
    settings.setValue("mainwindow/geometry", this->geometry());
    if(FileManager::Instance.widgetPdfViewerWrapper())
    {
        settings.setValue("pdfViewerWrapper/geometry", FileManager::Instance.widgetPdfViewerWrapper()->geometry());
    }
    delete ui;
}

void MainWindow::focus()
{
    this->activateWindow();
}

void MainWindow::openSendFeedbackDialog()
{
    DialogSendFeedback *d = new DialogSendFeedback(this);
    d->exec();
}

void MainWindow::closeEvent(QCloseEvent * event)
{
    QStringList files;
    QStringList fileCursorPositions;
    QList<QVariant> pdfPosition;
    QList<QVariant> pdfZoom;
    int tabIndex = _tabWidget->currentIndex();
    while(_tabWidget->count())
    {
        int tabId = 0;
        if(_tabWidget->count() > 1 && _tabWidget->currentIndex() == 0)
        {
            //tabId = 1;
        }
        QString * filename = 0;

        fileCursorPositions<< QString::number(_tabWidget->widget(tabId)->widgetTextEdit()->textCursor().position());
        pdfPosition << _tabWidget->widget(tabId)->widgetPdfViewer()->widgetPdfDocument()->pdfOffset();
        pdfZoom << _tabWidget->widget(tabId)->widgetPdfViewer()->widgetPdfDocument()->zoom();
        if(!this->closeTab(tabId, &filename))
        {
            event->ignore();
            return;
        }
        if(filename)
        {
            files << *filename;
            delete filename;
        }
        else
        {
            fileCursorPositions.pop_back();
        }
    }
    ConfigManager::Instance.setOpenFilesWhenClosing(files, fileCursorPositions, tabIndex);
    ConfigManager::Instance.setOpenFilesWhenClosingPdfPosition(pdfPosition, pdfZoom);

    if(FileManager::Instance.widgetPdfViewerWrapper())
    {
        FileManager::Instance.widgetPdfViewerWrapper()->close();
    }

    event->accept();
}
void MainWindow::dragEnterEvent(QDragEnterEvent * event)
{
    if(!event->mimeData()->hasUrls())
    {
        event->ignore();
        return;
    }
    QList<QUrl> urlList = event->mimeData()->urls();
    foreach(const QUrl & url, urlList)
    {
        if(canBeOpened(url.toLocalFile()) || canBeInserted(url.toLocalFile()))
        {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}
void MainWindow::dragMoveEvent(QDragMoveEvent * event)
{
    event->acceptProposedAction();
}
void MainWindow::dragLeaveEvent(QDragLeaveEvent * event)
{
    event->accept();
}

void MainWindow::displayHelp()
{
    this->closeCurrentWidgetFile();
    FileManager::Instance.setCurrent(0);
    _tabWidget->setCurrentIndex(-1, false);
    ui->verticalLayout->addWidget(new HelpWidget(this));
    _widgetStatusBar->updateButtons();
}

bool MainWindow::handleMimeData(const QMimeData* mimeData)
{
    // check for our needed mime type, here a file or a list of files
    if (mimeData->hasUrls())
    {
        QStringList openableFiles;
        QStringList insertableFiles;
        // extract the local paths of the files
        foreach (const QUrl & url, mimeData->urls())
        {
            if(canBeOpened(url.toLocalFile()))
            {
                openableFiles.append(url.toLocalFile());
            }
            else if(canBeInserted(url.toLocalFile()))
            {
                insertableFiles.append(url.toLocalFile());
            }
        }
        if(!openableFiles.isEmpty())
        {
            foreach(const QString& file, openableFiles)
            {
                open(file);
            }
            return true;
        }
        if(!insertableFiles.isEmpty() && FileManager::Instance.currentWidgetFile())
        {
            foreach(const QString& file, insertableFiles)
            {
                FileManager::Instance.currentWidgetFile()->widgetTextEdit()->insertFile(file);
            }
            return true;
        }
    }
    return false;
}

void MainWindow::dropEvent(QDropEvent * event)
{
    if(handleMimeData(event->mimeData()))
    {
        event->acceptProposedAction();
    }
    else
    {
        event->ignore();
    }
}

bool MainWindow::event(QEvent *event)
{
    bool ok = QMainWindow::event(event);
    if(event->type() == QEvent::LanguageChange)
    {
        QList<QAction *> actionsList = this->findChildren<QAction *>();
        QSettings settings;
        settings.beginGroup("shortcuts");
        foreach(QAction * action, actionsList) {
            if(!action->text().isEmpty() && settings.contains(action->objectName()))
            {
                QList<QKeySequence> l;
                foreach(const QString &s, settings.value(action->objectName()).toStringList())
                {
                    l << QKeySequence(s, QKeySequence::NativeText);
                }
                action->setShortcuts(l);
            }
        }
    }
    return ok;
}


void MainWindow::changeEvent(QEvent *event)
{

    if (!isActiveWindow())
        return QMainWindow::changeEvent(event);

    if (event->type() == QEvent::ActivationChange) {
        FileManager::Instance.checkCurrentFileSystemChanges();
    }
    else
    if (event->type() == QEvent::LanguageChange) {
        this->ui->retranslateUi(this);
    }
    else
        QMainWindow::changeEvent(event);
}
bool MainWindow::canBeOpened(QString filename)
{
    QString ext = QFileInfo(filename).suffix();
    if(!ext.compare("tex")
            || !ext.compare("bib"))
    {
        return true;
    }
    return false;
}
bool MainWindow::canBeInserted(QString filename)
{
    QString ext = QFileInfo(filename).suffix();
    if(!ext.compare("jpeg",Qt::CaseInsensitive)
            || !ext.compare("jpg",Qt::CaseInsensitive)
            || !ext.compare("png",Qt::CaseInsensitive))
    {
        return true;
    }
    return false;
}

void MainWindow::initMacrosMenu()
{
    foreach(QAction * a, this->actions())
    {
        removeAction(a);
    }

    if(_menuMacrosAction)
    {
        this->ui->menuBar->removeAction(_menuMacrosAction);
    }
    QMenu * menu = new QMenu(tr("&Macros"));
    _menuMacrosAction = ui->menuBar->insertMenu(ui->menuView->menuAction(), menu);
    MacroEngine::Instance.createMacrosMenu(menu);
    MacroEngine::Instance.createShortCuts(this);
    this->addActions(menu->actions());
}

void MainWindow::initBuildMenu()
{

    QSettings settings;
    settings.beginGroup("shortcuts");

    this->ui->menuOtherBuilder->clear();
    this->ui->actionDefaultCommandLatex->setText(ConfigManager::Instance.defaultLatex());
    QStringList commandNameList = ConfigManager::Instance.latexCommandNames();
    foreach(const QString & name, commandNameList)
    {
        QAction * action = new QAction(name, this->ui->menuOtherBuilder);
        action->setObjectName(name);
        QList<QKeySequence> l;
        foreach(const QString &s, settings.value(name, QStringList()).toStringList())
        {
            l << QKeySequence(s, QKeySequence::NativeText);
        }
        action->setShortcuts(l);
        //action->setPriority(QAction::LowPriority);
        /*if(!name.compare(ConfigManager::Instance.defaultLatex()))
        {
            delete action;
            continue;
        }*/
        this->ui->menuOtherBuilder->addAction(action);
        connect(action, SIGNAL(triggered()), &FileManager::Instance,SLOT(builTex()));
    }

    if(WidgetFile * widget = FileManager::Instance.currentWidgetFile())
    if(widget->file()->texDirectives().contains("program"))
    {
        QString engine = widget->file()->texDirectives().value("program");
        if(ConfigManager::Instance.latexCommandNames().contains(engine, Qt::CaseInsensitive))
        {
            ui->actionDefaultCommandLatex->setText(engine);
        }
    }

}

WidgetFile * MainWindow::newFile()
{
    if(FileManager::Instance.newFile(this))
    {
        _tabWidget->addTab(FileManager::Instance.currentWidgetFile(), "untitled");
        _tabWidget->setCurrentIndex(_tabWidget->count()-1);
    }
    return FileManager::Instance.currentWidgetFile();
}
void MainWindow::onFilenameChanged(QString filename)
{
    addFilenameToLastOpened(filename);
    this->setWindowTitle(_tabWidget->currentText()+" - texiteasy");

#ifdef OS_MAC
    //setWindowIcon(style()->standardIcon(QStyle::SP_FileIcon, 0, this));
    setWindowFilePath(FileManager::Instance.currentWidgetFile()->file()->getFilename());
#endif
}

void MainWindow::createLastOpenedFilesMenu()
{
    this->ui->menuOuvrir_R_cent->clear();
    this->ui->menuOuvrir_R_cent->insertAction(0,this->ui->actionDeleteLastOpenFiles);
    QAction* lastAction = this->ui->menuOuvrir_R_cent->actions().last();
    QStringList lastFiles = QSettings().value("lastFiles").toStringList();
    foreach(const QString& file, lastFiles)
    {
        if(!QFile(file).exists())
        {
            continue;
        }
        QAction * action = new QAction(file, this->ui->menuOuvrir_R_cent);
        action->setPriority(QAction::LowPriority);
        this->ui->menuOuvrir_R_cent->insertAction(lastAction,action);
        connect(action, SIGNAL(triggered()), this, SLOT(openLast()));
    }
    this->ui->menuOuvrir_R_cent->insertSeparator(lastAction);

}

void MainWindow::addFilenameToLastOpened(QString filename)
{
    filename.replace("\\","/");
    QSettings settings;
    QFileInfo info(filename);
    settings.setValue("lastFolder",info.path());
    QString basename = info.baseName();
    //udpate the settings
    {
        QSettings settings;
        QStringList lastFiles = settings.value("lastFiles",QStringList()).toStringList();
        lastFiles.prepend(filename);
        lastFiles.removeDuplicates();
        while(lastFiles.count()>10) { lastFiles.pop_back(); }
        settings.setValue("lastFiles", lastFiles);
    }
    createLastOpenedFilesMenu();
}

void MainWindow::openLastSession()
{
    Tools::Log("MainWindow::openLastSession: load files");
    QStringList files = ConfigManager::Instance.openFilesWhenClosing();
    Tools::Log("MainWindow::openLastSession: load cursor position");
    QStringList fileCursorPositions = ConfigManager::Instance.openFileCursorPositionsWhenClosing();
    QList<QVariant> pdfZoom = ConfigManager::Instance.openFilesWhenClosingPdfZoom();
    QList<QVariant> pdfPosition = ConfigManager::Instance.openFilesWhenClosingPdfPosition();
    int index = 0;
    foreach(const QString & file, files)
    {
        if(index < fileCursorPositions.count())
        {
            Tools::Log("MainWindow::openLastSession: open "+file);
            QPoint p;
            if(index < pdfPosition.count())
            {
                p = pdfPosition.at(index).toPoint();
            }
            qreal z = 1;
            if(index < pdfZoom.count())
            {
                z = pdfZoom.at(index).toReal();
            }
            open(file, fileCursorPositions.at(index).toInt(), p, z);
        }
        ++index;
    }
    Tools::Log("MainWindow::openLastSession: create tabs");
    int tabIndex = ConfigManager::Instance.openTabIndexWhenClosing();
    if(tabIndex < _tabWidget->count())
    {
        _tabWidget->setCurrentIndex(tabIndex);
    }
}

void MainWindow::openLast()
{
    QString filename = dynamic_cast<QAction*>(sender())->text();
    open(filename);
}
void MainWindow::open()
{
    QSettings settings;
    //get the filname

    QString filename = QFileDialog::getOpenFileName(this,tr("Ouvrir un fichier"), ConfigManager::Instance.lastFolder(), ConfigManager::Extensions);
    if(filename.isEmpty())
    {
        return;
    }
    open(filename);
}
void MainWindow::open(QString filename, int cursorPosition, QPoint pdfPosition, qreal pdfZoom)
{

    this->activateWindow();
    this->raise();

    filename.replace("\\", "/");
    QSettings settings;

    //check the filename
    if(filename.isEmpty())
    {
        return;
    }
    //check the filename
    if(!QFileInfo(filename).exists())
    {
        return;
    }
    Tools::Log("MainWindow::open: addFilenameToLastOpened()");
    this->addFilenameToLastOpened(filename);

    //check if it is already open
    int index = _tabWidget->indexOf(filename);
    if(index != -1)
    {
        _tabWidget->setCurrentIndex(index);
        return;
    }

    //open
    Tools::Log("MainWindow::open: FileManager::Instance.open()");
    if(FileManager::Instance.open(filename, this))
    {
        Tools::Log("MainWindow::open:get current widget");
        WidgetFile * current = FileManager::Instance.currentWidgetFile();
        QString tabName = FileManager::Instance.currentWidgetFile()->file()->fileInfo().fileName();
        _tabWidget->addTab(current, tabName);
        _tabWidget->setCurrentIndex(_tabWidget->count()-1);
        if(current->file()->texDirectives().contains("program"))
        {
            QString engine = current->file()->texDirectives().value("program");
            if(!ConfigManager::Instance.latexCommandNames().contains(engine, Qt::CaseInsensitive))
            {
                QMessageBox::warning(this, trUtf8("Attention"), trUtf8("Le compilateur %1 n'est pas définie, veuillez le créer dans les options.").arg(engine));
            }
        }

        current->widgetTextEdit()->setTextCursorPosition(cursorPosition);
        current->widgetPdfViewer()->widgetPdfDocument()->setZoom(pdfZoom);
        current->widgetPdfViewer()->widgetPdfDocument()->setPdfOffset(pdfPosition);
        QTimer::singleShot(1,current->widgetTextEdit(), SLOT(setFocus()));

    }
    else
    {
        _tabWidget->setTabText(_tabWidget->currentIndex(), FileManager::Instance.currentWidgetFile()->file()->fileInfo().fileName());
    }

    this->statusBar()->showMessage(filename,4000);
    this->_widgetStatusBar->setEncoding(FileManager::Instance.currentWidgetFile()->widgetTextEdit()->getCurrentFile()->codec());
}
void MainWindow::onOtherInstanceMessage(const QString & msg)
{
    QStringList argv = msg.split("#!#");
    if(!argv.size())
    {
        return;
    }
    argv.pop_front();
    foreach(const QString &arg, argv)
    {
        this->open(arg);
    }
}

void MainWindow::onCurrentFileChanged(WidgetFile * widget)
{
    this->closeCurrentWidgetFile();

    FileManager::Instance.setCurrent(widget);
    if(!widget)
    {
        ui->verticalLayout->addWidget(_emptyWidget);
        _widgetStatusBar->updateButtons();
#ifdef OS_MAC
        //setWindowIcon(QIcon());
#endif
        return;
    }
    ui->verticalLayout->addWidget(widget);
    widget->widgetTextEdit()->setFocus();
    _widgetStatusBar->updateButtons();


// change the default builder if the tex directive "program" exists
    ui->actionDefaultCommandLatex->setText(ConfigManager::Instance.defaultLatex());
    if(widget->file()->texDirectives().contains("program"))
    {
        QString engine = widget->file()->texDirectives().value("program");
        if(ConfigManager::Instance.latexCommandNames().contains(engine, Qt::CaseInsensitive))
        {
            ui->actionDefaultCommandLatex->setText(engine);
        }
    }

    //window title
    this->setWindowModified(widget->file()->isModified());
#ifdef OS_MAC
    this->setWindowTitle(_tabWidget->currentText()+" - texiteasy");
    if(widget->file()->isUntitled())
    {
        //setWindowIcon(QIcon());
    }
    else
    {
        //setWindowIcon(style()->standardIcon(QStyle::SP_FileIcon, 0, this));
    }
#endif
    setWindowFilePath(widget->file()->getFilename());

    FileManager::Instance.checkCurrentFileSystemChanges();
}
bool MainWindow::widgetFileCanBeClosed(WidgetFile * widget)
{
    if(!widget->widgetTextEdit()->getCurrentFile()->isModified())
    {
        return true;
    }

    QString insertedFilename = "";
    if(!widget->file()->isUntitled())
    {
        insertedFilename = " \""+widget->file()->fileInfo().baseName()+"\"";
    }
    if(!_confirmCloseMessageBox)
    {
        _confirmCloseMessageBox = new QMessageBox(trUtf8("Continuer?"),
                                              trUtf8("Le fichier%1 a été modifié.").arg(insertedFilename) + "\n" +
          trUtf8("Voullez-vous sauvegarder les changements?"),//Do you want to save your changes?"),
            QMessageBox::Warning,
            QMessageBox::Yes | QMessageBox::Default,
            QMessageBox::No,
            QMessageBox::Cancel | QMessageBox::Escape,
            this, Qt::Sheet);
        _confirmCloseMessageBox->setButtonText(QMessageBox::Yes,
              widget->file()->isUntitled() ? trUtf8("Sauvegarder")+"..." : trUtf8("Sauvegarder"));
        _confirmCloseMessageBox->setButtonText(QMessageBox::No,
            trUtf8("Ne pas sauvegarder"));
        _confirmCloseMessageBox->setButtonText(QMessageBox::Cancel,
            trUtf8("Annuler"));
    }

    int i = _confirmCloseMessageBox->exec();

    if(i == QMessageBox::Cancel)
    {
        return false;
    }
    else
    {
        if(i == QMessageBox::Yes)
        {
            widget->save();
            if(widget->file()->isModified())
            {
                return false;
            }
        }
    }
    return true;
}

bool MainWindow::closeTab(int index, QString ** filename)
{
    WidgetFile * widget = _tabWidget->widget(index);

    if(!widgetFileCanBeClosed(widget))
    {
        return false;
    }

    if(filename && !widget->file()->isUntitled())
    {
        *filename = new QString(widget->file()->getFilename());
    }

    if(widget == FileManager::Instance.currentWidgetFile())
    {
        this->closeCurrentWidgetFile();
        FileManager::Instance.close(widget);
        _tabWidget->removeTab(index);
    }
    else
    {
        FileManager::Instance.close(widget);
        _tabWidget->removeTab(index);
    }
    if(!_tabWidget->count())
    {
        // The following is also call by the tabWidget but maybe after so we
        // make sure that it is call before closing the widget
        this->onCurrentFileChanged(0);
    }
    return true;
}
void MainWindow::clearLastOpened()
{
    QSettings settings;
    settings.setValue("lastFiles", QStringList());
    this->ui->menuOuvrir_R_cent->clear();
    this->ui->menuOuvrir_R_cent->insertAction(0,this->ui->actionDeleteLastOpenFiles);
}
void MainWindow::changeTheme()
{
    QString text = dynamic_cast<QAction*>(this->sender())->text();
    this->setTheme(text);

}
void MainWindow::setUtf8()
{
    FileManager::Instance.setEncoding("UTF-8");
    FileManager::Instance.save();
    _widgetStatusBar->updateButtons();
}
void MainWindow::setOtherEncoding()
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Nom de l'encodage"),
                                              tr("Nom de l'encodage: "), QLineEdit::Normal,
                                         "", &ok);
    if (!ok || text.isEmpty())
    {
        return;
    }
    FileManager::Instance.setEncoding(text);
    FileManager::Instance.save();
    _widgetStatusBar->updateButtons();
}
void MainWindow::reopenWithEncoding()
{
    QAction * action = qobject_cast<QAction*>(sender());
    if(!action || !FileManager::Instance.currentWidgetFile())
    {
        return;
    }
    if(!widgetFileCanBeClosed(FileManager::Instance.currentWidgetFile()))
    {
        return;
    }
    FileManager::Instance.reopenWithEncoding(action->text());
    _widgetStatusBar->updateButtons();
}

void MainWindow::setTheme(QString theme)
{
    foreach(QAction * action, this->ui->menuTh_me->actions())
    {
        if(action->text().compare(theme))
            action->setChecked(false);
        else
            action->setChecked(true);

    }
    ConfigManager::Instance.load(theme);
    this->initTheme();
    FileManager::Instance.rehighlight();
}


void MainWindow::insertTexDirEncoding()
{
    if(!FileManager::Instance.currentWidgetFile())
    {
        return;
    }
    WidgetFile * widget = FileManager::Instance.currentWidgetFile();
    QString t = "%!TEX encoding = ";
    t += widget->file()->codec();
    t += "\n";
    t += widget->widgetTextEdit()->toPlainText();
    widget->widgetTextEdit()->setPlainText(t);
}
void MainWindow::insertTexDirProgram()
{
    if(!FileManager::Instance.currentWidgetFile())
    {
        return;
    }
    bool ok;
    QStringList commandNameList = ConfigManager::Instance.latexCommandNames();
    QString item = QInputDialog::getItem(this, tr("Compilateur pour ce fichier"),
                                             tr("Compilateur:"), commandNameList, 0, false, &ok);


    if(!ok)
    {
        return;
    }

    WidgetFile * widget = FileManager::Instance.currentWidgetFile();
    QString t = "%!TEX program = ";
    t += item;
    t += "\n";
    t += widget->widgetTextEdit()->toPlainText();
    widget->widgetTextEdit()->setPlainText(t);

    widget->file()->addTexDirective("program", item);
    ui->actionDefaultCommandLatex->setText(item);

}
void MainWindow::insertTexDirSpellCheck()
{
    if(!FileManager::Instance.currentWidgetFile())
    {
        return;
    }
    WidgetFile * widget = FileManager::Instance.currentWidgetFile();
    QString t = "%!TEX spellcheck = ";
    t += widget->dictionary();
    t += "\n";
    t += widget->widgetTextEdit()->toPlainText();
    widget->widgetTextEdit()->setPlainText(t);
}
void MainWindow::insertTexDirRoot()
{
    if(!FileManager::Instance.currentWidgetFile())
    {
        return;
    }
    WidgetFile * widget = FileManager::Instance.currentWidgetFile();
    QDir baseDir;
    if(!widget->file()->getFilename().isEmpty())
    {
        baseDir = QFileInfo(widget->file()->getFilename()).dir();
    }
    QString t = "%!TEX root = ";
    if(widget->masterFile())
    {
        t += baseDir.relativeFilePath(widget->masterFile()->file()->getFilename());
    }
    t += "\n";
    t += widget->widgetTextEdit()->toPlainText();
    widget->widgetTextEdit()->setPlainText(t);
}

void MainWindow::initTheme()
{
    QPalette Pal(palette());
    Pal.setColor(QPalette::Background, ConfigManager::Instance.getTextCharFormats("line-number").background().color());
    this->setAutoFillBackground(true);
    this->setPalette(Pal);
    FileManager::Instance.initTheme();
    _widgetStatusBar->initTheme();
    _tabWidget->initTheme();
}
void MainWindow::closeCurrentWidgetFile()
{
    if(ui->verticalLayout->count() > 1)
    {
        QWidget * w = ui->verticalLayout->itemAt(1)->widget();
        ui->verticalLayout->removeWidget(w);
        w->setParent(0);
    }
}
void MainWindow::addUpdateMenu()
{
    QAction * openWebsiteAction = new QAction(trUtf8("Mettre à jour TexitEasy"), this->ui->menuBar);

#ifdef OS_MAC
    /* I don't know why but on mac we cannot add an action directly in the menu Bar
     * So lets add a menu with the action
     */
    QMenu * m = new QMenu(trUtf8("Mettre à jour TexitEasy"), this->ui->menuBar);
    m->addAction(openWebsiteAction);
    this->ui->menuBar->addMenu(m);
#else
    /* On the other platform we can add an action directly in the menu Bar
     */
    this->ui->menuBar->addAction(openWebsiteAction);
#endif
    connect(openWebsiteAction, SIGNAL(triggered()), this, SLOT(proposeUpdateDialog()));

}
void MainWindow::proposeUpdateDialog()
{
    UpdateChecker::proposeUpdateDialog(this);
}

QAction * MainWindow::actionByRole(QString actionRole)
{
    foreach(QAction * action, this->findChildren<QAction*>())
    {
        if(action->property("role").toString() == actionRole)
        {
            return action;
        }
    }
    return 0;
}

void MainWindow::setWindowModified(bool b)
{

#ifdef OS_MAC
    QMainWindow::setWindowModified(b);
#else
    QString star("");
    if(FileManager::Instance.currentWidgetFile() && FileManager::Instance.currentWidgetFile()->file()->isModified())
    {
        star = "*";
    }
    this->setWindowTitle(_tabWidget->currentText()+star+" - texiteasy");
#endif
}
