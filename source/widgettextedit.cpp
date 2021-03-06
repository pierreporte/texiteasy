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


#include "hunspell/hunspell.hxx"
#include "widgettextedit.h"
#include "textaction.h"
#include "widgetinsertcommand.h"
#include "configmanager.h"
#include "file.h"
#include "viewer.h"
#include <QScrollBar>
#include <QDebug>
#include <QPainter>
#include "filestructure.h"
#include "blockdata.h"
#include <QListIterator>
#include <QMutableListIterator>
#include <QList>
#include <QFontMetrics>
#include <QFileDialog>
#include <QSettings>
#include <QMimeData>
#include <QPalette>
#include "syntaxhighlighter.h"
#include "completionengine.h"
#include <math.h>
#include <QtCore>
#include <QApplication>
#include <QMenu>
#include <QImage>
#include <QLayout>
#include <QMutexLocker>
#include "QTextEdit"
#include "filemanager.h"
#include "widgetlinenumber.h"
#include "widgetfile.h"
#include "textdocumentlayout.h"
#include "grammarchecker.h"
#include "textdocument.h"

#define max(a,b) ((a) < (b) ? (b) : (a))
#define min(a,b) ((a) > (b) ? (b) : (a))
#define abs(a) ((a) > 0 ? (a) : (-(a)))

WidgetTextEdit::WidgetTextEdit(WidgetFile * parent) :
    WIDGET_TEXT_EDIT_PARENT_CLASS(parent),
    _completionEngine(new CompletionEngine(this)),
    currentFile(new File(parent, this)),
    fileStructure(new FileStructure(this)),
    _textStruct(new TextStruct(this)),
    _indentationInited(false),
    _lineCount(0),
    _syntaxHighlighter(0),
    updatingIndentation(false),
    _widgetLineNumber(0),
    _macrosMenu(0),
    _scriptIsRunning(false),
    _lastBlockCount(0)

{
    _widgetFile = parent;
    _scriptEngine.setWidgetTextEdit(this);
    this->setContentsMargins(0,0,0,0);
    connect(this,SIGNAL(textChanged()),this,SLOT(updateIndentation()));
    connect(this,SIGNAL(cursorPositionChanged()), this, SLOT(onCursorPositionChange()));
    connect(this, SIGNAL(blockCountChanged(int)), this, SLOT(onBlockCountChanged(int)));
    //connect(this->verticalScrollBar(),SIGNAL(valueChanged(int)),this->viewport(),SLOT(update()));
#ifdef OS_MAC
    _wierdCircumflexCursor = false;
#endif
    this->setText(" ");
    this->currentFile->setModified(false);
    this->updateTabWidth();
    connect(&ConfigManager::Instance, SIGNAL(tabWidthChanged()), this, SLOT(updateTabWidth()));
    updateLineWrapMode();
    this->setMouseTracking(true);
}
WidgetTextEdit::~WidgetTextEdit()
{
    delete fileStructure;
    delete currentFile;
#ifdef DEBUG_DESTRUCTOR
    qDebug()<<"delete WidgetTextEdit";
#endif
}
void WidgetTextEdit::updateLineWrapMode()
{
    setLineWrapMode(ConfigManager::Instance.isLineWrapped() ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
}

void WidgetTextEdit::adjustScrollbar(QSizeF documentSize)
{
    QScrollBar * hbar = this->horizontalScrollBar();
    hbar->setRange(0, (int)documentSize.width() - viewport()->width());
    hbar->setPageStep(viewport()->width());
}

void WidgetTextEdit::scrollTo(int p)
{
    this->verticalScrollBar()->setSliderPosition(p);
}

void WidgetTextEdit::setText(const QString &text)
{
    this->_indentationInited = false;
    WIDGET_TEXT_EDIT_PARENT_CLASS::setPlainText(text);
/* TODO : Run initIndentation in a thread */
    //QtConcurrent::run(this,&WidgetTextEdit::initIndentation);
    this->initIndentation();
    this->updateIndentation();
    this->viewport()->update();
}
void WidgetTextEdit::insertText(const QString &text)
{
    WIDGET_TEXT_EDIT_PARENT_CLASS::insertPlainText(text);
}

typedef QPair<QString,QPair<int,int> > Argument;

void WidgetTextEdit::paintEvent(QPaintEvent *event)
{

    WIDGET_TEXT_EDIT_PARENT_CLASS::paintEvent(event);
    QPainter painter(viewport());
    painter.setFont(ConfigManager::Instance.getTextCharFormats("normal").font());

    painter.setPen(ConfigManager::Instance.getTextCharFormats("normal").foreground().color());
    foreach(QTextCursor cursor, _multipleEdit)
    {
        QTextLine line = cursor.block().layout()->lineForTextPosition(cursor.positionInBlock());
        if(line.isValid())
        {
            qreal left = line.cursorToX(cursor.positionInBlock());;
            qreal top = line.position().y() + line.height() + this->blockTop(cursor.block()) + this->contentOffsetTop();
            QPoint curPoint(left,top);
            QPoint diff(0,line.height());
            painter.drawLine(curPoint - diff, curPoint);
        }
    }
/*
    QBrush defaultBrush(ConfigManager::Instance.getTextCharFormats("normal").background().color().lighter(300));
    QBrush selectedBrush(ConfigManager::Instance.getTextCharFormats("normal").background().color().lighter());
    QPen borderSelectedPen = ConfigManager::Instance.getTextCharFormats("normal").foreground().color().darker();
    QPen borderPen = ConfigManager::Instance.getTextCharFormats("normal").foreground().color();*/
    QBrush defaultBrush(ConfigManager::Instance.getTextCharFormats("argument").background().color());
    QBrush selectedBrush(ConfigManager::Instance.getTextCharFormats("argument:selected").background().color());


    QPen textSelectedPen = ConfigManager::Instance.getTextCharFormats("argument:selected").foreground().color();
    QPen textPen = ConfigManager::Instance.getTextCharFormats("argument").foreground().color();

    QPen borderSelectedPen = ConfigManager::Instance.getTextCharFormats("argument-border:selected").foreground().color();
    QPen borderPen = ConfigManager::Instance.getTextCharFormats("argument-border").foreground().color();

    QTextBlock block = this->document()->firstBlock();

    while(block.isValid())
    {
        BlockData *data = static_cast<BlockData *>(block.userData());
        if(data && data->arguments.count())
        {
            foreach(const Argument &arg, data->arguments)
            {
                QTextLayout * layout = block.layout();
                if(layout)
                {
                    QTextLine line = layout->lineForTextPosition(arg.second.first);
                    QTextLine line2 = layout->lineForTextPosition(arg.second.second);
                    if(line.isValid() && line2.isValid())
                    {
                        bool selected = false;
                        bool extra = false;
                        //foreach(QTextEdit::ExtraSelection sel, extraSelections())
                        foreach(QTextCursor cur, _multipleEdit)
                        {
                            if((cur.position() >= block.position() + arg.second.first
                            && cur.position() <= block.position() + arg.second.second)
                                    || (    cur.hasSelection()
                                        &&  cur.selectionStart() <= block.position() + arg.second.second
                                        &&  cur.selectionEnd() >= block.position() + arg.second.first
                                        ))
                            {
                                painter.setBrush(selectedBrush);
                                painter.setPen(borderSelectedPen);
                                extra = true;
                                selected = true;
                            }
                        }
                        if(!extra)
                        {
                            if((textCursor().position() >= block.position() + arg.second.first
                                && textCursor().position() <= block.position() + arg.second.second)
                                || (    textCursor().hasSelection()
                                    &&  textCursor().selectionStart() <= block.position() + arg.second.second
                                    &&  textCursor().selectionEnd() >= block.position() + arg.second.first
                                    )
                                )
                            {
                                painter.setBrush(selectedBrush);
                                painter.setPen(borderSelectedPen);
                                selected = true;
                            }
                            else
                            {
                                painter.setBrush(defaultBrush);
                                painter.setPen(borderPen);
                            }
                        }

                        qreal xLeft = line.cursorToX(arg.second.first);
                        qreal width = line2.cursorToX(arg.second.second) - xLeft;
                        qreal top = line.position().y() + blockTop(block) + contentOffset().y();
                        qreal height = line2.rect().bottom() - line.position().y();
                        QRectF rF(xLeft, top, width, height);
                        QRect r(xLeft, top, width, height);

                        qreal invisibleMargin = painter.fontMetrics().width("{") / 4.0;

                        painter.drawRoundedRect(rF,5,5);
                        painter.setPen(selected ? textSelectedPen : textPen);

                        painter.drawText(rF.bottomLeft() + QPoint(1 + invisibleMargin, -5), arg.first);
                        //update the rect if it is upside for the arguments that are upside the current text cursor, or below for the arguments that are below the current text cursor.
                        if(block.position() + arg.second.second + 2 < textCursor().position() && r.top() < event->rect().top())
                        {
                            update(r);
                        }
                        if(block.position() + arg.second.first - 2 > textCursor().position() && r.top() > event->rect().top())
                        {
                            update(r);
                        }
                    }
                }
            }
        }
        block = block.next();
    }

    return;
}
void WidgetTextEdit::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu * defaultMenu = createStandardContextMenu();
    QMenu * menu = new QMenu(this);

    QTextCursor cursor = cursorForPosition(event->pos());
    BlockData *data = static_cast<BlockData *>(cursor.block().userData() );

    if(widgetFile()->spellChecker())
    {
        QTextCodec *codec = QTextCodec::codecForName(widgetFile()->spellCheckerEncoding().toLatin1());
        int blockPos = cursor.block().position();
        int colstart, colend;
        colend = colstart = cursor.positionInBlock();
        if (data && colstart < data->length() && data->characterData[colstart].misspelled==true)
        {
            while (colstart >= 0 && (data->characterData[colstart].misspelled==true))
            {
                --colstart;
            }
            ++colstart;
            while (colend < data->length() && (data->characterData[colend].misspelled==true))
            {
                colend++;
            }
            cursor.setPosition(blockPos+colstart,QTextCursor::MoveAnchor);
            cursor.setPosition(blockPos+colend,QTextCursor::KeepAnchor);
            QString    word          = cursor.selectedText();
            QByteArray encodedString = codec->fromUnicode(word);
            bool check = widgetFile()->spellChecker()->spell(encodedString.data());
            if (!check)
            {
                char ** wlst;
                int ns = widgetFile()->spellChecker()->suggest(&wlst, encodedString.data());
                if (ns > 0)
                {
                    QStringList suggWords;
                    for (int i=0; i < ns; i++)
                    {
                        suggWords.append(codec->toUnicode(wlst[i]));
                    }
                    widgetFile()->spellChecker()->free_list(&wlst, ns);
                    if(!suggWords.contains(word))
                    {
                        this->setTextCursor(cursor);

                        QAction * action;
                        QFont spellmenufont (qApp->font());
                        spellmenufont.setBold(true);
                        foreach (const QString &suggestion, suggWords)
                        {
                            action = new QAction(suggestion, menu);
                            menu->insertAction(menu->actionAt(QPoint(0,0)), action);
                            connect(action, SIGNAL(triggered()), this, SLOT(correctWord()));
                            action->setFont(spellmenufont);
                        }
                        spellmenufont.setBold(false);
                        spellmenufont.setItalic(true);
                        action = new QAction(trUtf8("Ajouter au dictionnaire"), menu);
                        menu->insertAction(menu->actionAt(QPoint(0,0)), action);
                        connect(action, SIGNAL(triggered()), this, SLOT(addToDictionnary()));
                        menu->addSeparator();
                    }

                 }
            }
        }// if the current file is misspelled
    }// if there is a dictionnary

    menu->addActions(defaultMenu->actions());
    menu->addSeparator();
    MacroEngine::Instance.createMacrosMenu(menu);



    menu->exec(event->globalPos());
    delete menu;
}
void WidgetTextEdit::addToDictionnary()
{
    QString newword = textCursor().selectedText();
    ConfigManager::Instance.addToDictionnary(this->widgetFile()->dictionary(), newword);
    QTextCodec *codec = QTextCodec::codecForName(widgetFile()->spellCheckerEncoding().toLatin1());
    this->widgetFile()->spellChecker()->add(codec->fromUnicode(newword).data());
    _syntaxHighlighter->rehighlight();
}

void WidgetTextEdit::correctWord()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (action)
    {
        QString newword = action->text();
        textCursor().removeSelectedText();
        textCursor().insertText(newword);
    }
}

void WidgetTextEdit::updateLineNumber(const QRect &rect, int /*dy*/)
{
    if(!_widgetLineNumber)
    {
        return;
    }
    _widgetLineNumber->setCurrentLine(textCursor().blockNumber());
    _widgetLineNumber->update(0, rect.y(), _widgetLineNumber->width(), rect.height());

}
bool WidgetTextEdit::isCursorVisible()
{
    bool down = this->blockBottom(this->textCursor().block()) + this->contentOffsetTop() > 0;
    bool up = this->blockTop(this->textCursor().block()) +this->contentOffsetTop() < this->height();
    return up && down;
}

void WidgetTextEdit::onCursorPositionChange()
{
    if(_widgetLineNumber)
    {
        _widgetLineNumber->removeHighlight();
    }

    this->removeExtraSelections(WidgetTextEdit::ArgumentSelection);
    this->removeExtraSelections(WidgetTextEdit::ParenthesesMatchingSelection);
    _textStruct->environmentPath(textCursor().position());
    this->highlightCurrentLine();

    //*
    if(!_scriptEngine.cursorsMutex()->tryLock())
    {
        QTimer::singleShot(10, this, SLOT(onCursorPositionChange()));
    }else
    {
        if(!textCursor().hasSelection())
        {
            // if the cursor is positionned on an argument => we select the whole argument
            BlockData *data = static_cast<BlockData *>( textCursor().block().userData() );
            if (data) {
                if((textCursor().positionInBlock() < data->characterData.count() && data->characterData[textCursor().positionInBlock()].state == SyntaxHighlighter::CompletionArgument)
                   || (textCursor().positionInBlock() > 0 && data->characterData[textCursor().positionInBlock()-1].state == SyntaxHighlighter::CompletionArgument))
                {
                    QString text = textCursor().block().text();
                    int selStart = textCursor().positionInBlock() - 1;
                    while(selStart > 0 && data->characterData[selStart].state == SyntaxHighlighter::CompletionArgument)
                    {
                        --selStart;
                    }
                    QTextCursor cursor = textCursor();
                    cursor.setPosition(textCursor().block().position() + selStart);
                    this->setTextCursor(cursor);
                    this->selectNextArgument();
                    _scriptEngine.cursorsMutex()->unlock();
                    return;
                }
            }
        }
        if(textCursor().hasSelection())
        {
            //if part of the selection include a part of an argument => enlarge the selection to include all the argument
            //TODO: clean this part
            QTextCursor cStart = textCursor();
            cStart.setPosition(qMin(textCursor().selectionStart(), textCursor().selectionEnd()));
            QTextCursor cEnd = textCursor();
            cEnd.setPosition(qMax(textCursor().selectionStart(), textCursor().selectionEnd()));
            BlockData *dataStart = static_cast<BlockData *>( cStart.block().userData() );
            BlockData *dataEnd = static_cast<BlockData *>( cEnd.block().userData() );
            bool change = false;
            if (dataStart) {
                while(cStart.positionInBlock() > 0 && cStart.positionInBlock() < dataStart->characterData.size()
                      && dataStart->characterData[cStart.positionInBlock()].state == SyntaxHighlighter::CompletionArgument
                      && dataStart->characterData[cStart.positionInBlock() - 1].state == SyntaxHighlighter::CompletionArgument)
                {
                    change = true;
                    cStart.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor);
                }
            }
            if (dataEnd) {
                while(cEnd.positionInBlock() > 0 && cEnd.positionInBlock() < cEnd.block().text().length()
                      && dataEnd->characterData[cEnd.positionInBlock() - 1].state == SyntaxHighlighter::CompletionArgument
                      && dataEnd->characterData[cEnd.positionInBlock()].state == SyntaxHighlighter::CompletionArgument)
                {
                    change = true;
                    cEnd.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor);
                }
            }
            if(change)
            {
                QTextCursor cursor = textCursor();
                if(cursor.selectionStart() < cursor.position())
                {
                    cursor.setPosition(cStart.position());
                    cursor.setPosition(cEnd.position(), QTextCursor::KeepAnchor);
                    this->setTextCursor(cursor);
                }
                else
                {
                    cursor.setPosition(cEnd.position());
                    cursor.setPosition(cStart.position(), QTextCursor::KeepAnchor);
                    this->setTextCursor(cursor);
                }
                _scriptEngine.cursorsMutex()->unlock();
                return;
            }
        }
        _scriptEngine.cursorsMutex()->unlock();
    }
    // */
    // else we do the usual match

    matchAll();
    this->currentFile->getViewer()->setLine(this->textCursor().blockNumber()+1);

    WidgetInsertCommand::instance()->setVisible(false);

    emit cursorPositionChanged(this->textCursor().blockNumber() + 1, this->textCursor().positionInBlock() + 1);
}

void WidgetTextEdit::resizeEvent(QResizeEvent *event)
{
    TextDocumentLayout* layout = dynamic_cast<TextDocumentLayout*>(this->document()->documentLayout());
    layout->setTextWidth(viewport()->width());
    WIDGET_TEXT_EDIT_PARENT_CLASS::resizeEvent(event);
}
void WidgetTextEdit::checkGrammar()
{
    GrammarChecker * gm = new GrammarChecker();
    QString text = this->toPlainRealText();
    qDebug()<<text;
    gm->check(text);
}

QString WidgetTextEdit::toPlainRealText()
{
    const StructItem * documentItem = this->_textStruct->documentItem();
    if(!documentItem)
    {
        return "";
    }
    QTextBlock block = this->document()->firstBlock();
    QString text;
    text.reserve(this->document()->characterCount());
    while(block.isValid() && documentItem->end > block.position())
    {
        if(documentItem->begin > block.position())
        {
            block = block.next();
            continue;
        }
        BlockData *data = static_cast<BlockData *>( block.userData() );
        if (data)
        {
            for(int i = 0; i < block.text().size() && i < data->characterData.size(); ++i)
            {
                QString c = block.text().at(i);
                //we remove special character and command and replace it with space (to keep track of positions)
                if(data->characterData.at(i).state == SyntaxHighlighter::Text && !c.contains(QRegExp("[\\{\\}\\[\\]\\\\]")))
                {
                    text += block.text().at(i);
                }
                else
                {
                    text += " ";
                }
            }
        }
        block = block.next();
    }
    return text;
}

void WidgetTextEdit::insertPlainText(const QString &text)
{
    if(_multipleEdit.count() && !text.contains(QRegExp("[^a-zA-Z0-9 ]")))
    {
        QTextCursor cur1 = this->textCursor();
        cur1.insertText(text);
        _multipleEdit.first().insertText(text);
        this->setTextCursor(cur1);
        return;
    }
    WIDGET_TEXT_EDIT_PARENT_CLASS::insertPlainText(text);
}

QList<QTextEdit::ExtraSelection> WidgetTextEdit::extraSelections(int kind)
{
    return _extraSelections.value(kind);
}

void WidgetTextEdit::removeExtraSelections(int kind)
{
    _extraSelections.remove(kind);
    this->applyExtraSelection();
}

void WidgetTextEdit::addExtraSelections(const QList<QTextEdit::ExtraSelection> &selections, int kind)
{
    _extraSelections[kind] = selections;
    this->applyExtraSelection();
}

void WidgetTextEdit::applyExtraSelection()
{
    QList<QTextEdit::ExtraSelection> newExtraSelections;
    foreach(const QList<QTextEdit::ExtraSelection> &selList, _extraSelections)
    {
        foreach(const QTextEdit::ExtraSelection &s, selList)
        {
            if(!s.cursor.isNull() && s.cursor.selectedText().length())
            {
                newExtraSelections.append(s);
            }
        }
    }
    WIDGET_TEXT_EDIT_PARENT_CLASS::setExtraSelections(newExtraSelections);
}
void WidgetTextEdit::setExtraSelections(const QList<QTextEdit::ExtraSelection> &)
{
    qDebug()<<"dont use this function, use addExtraSelections instead";
}


void WidgetTextEdit::mouseMoveEvent(QMouseEvent *e)
{
    if(e->modifiers() & Qt::ControlModifier)
    {
        QTextCursor clickCursor = textCursor();
        clickCursor.setPosition(this->hitTest(e->pos()), QTextCursor::MoveAnchor);
        QTextCursor match = TextActions::match(clickCursor, this->widgetFile());
        if(!match.isNull())
        {
            QTextEdit::ExtraSelection sel;
            sel.cursor = match;
            sel.format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
            sel.format.setUnderlineColor(ConfigManager::Instance.getTextCharFormats("normal").foreground().color());
            sel.format.setFontUnderline(true);
            this->addExtraSelections(QList<QTextEdit::ExtraSelection>() << sel, WidgetTextEdit::OtherSelection);
            this->viewport()->setCursor(Qt::PointingHandCursor);
            return;
        }
    }

    this->removeExtraSelections(WidgetTextEdit::OtherSelection);
    this->setBeamCursor();
    WIDGET_TEXT_EDIT_PARENT_CLASS::mouseMoveEvent(e);
}
void WidgetTextEdit::mousePressEvent(QMouseEvent *e)
{
    if(!hasArguments())
    {
        _scriptEngine.clear();
        _scriptIsRunning = false;
    }

    if(e->modifiers() == Qt::AltModifier)
    {
        _multipleEdit << textCursor();
    }
    else
    if(e->modifiers() & Qt::ControlModifier)
    {
        QTextCursor clickCursor = textCursor();
        clickCursor.setPosition(this->hitTest(e->pos()), QTextCursor::MoveAnchor);
        if(TextActions::execute(clickCursor, this->widgetFile(), e->modifiers()))
        {
            return;
        }
    }
    else
    {
        _multipleEdit.clear();
    }
    WIDGET_TEXT_EDIT_PARENT_CLASS::mousePressEvent(e);
}

void WidgetTextEdit::keyPressEvent(QKeyEvent *e)
{
#ifdef OS_MAC
    _wierdCircumflexCursor = false;
#endif
    if(e == QKeySequence::Undo || e == QKeySequence::Redo || e == QKeySequence::Copy || e == QKeySequence::Cut)
    {
        _multipleEdit.clear();
        WIDGET_TEXT_EDIT_PARENT_CLASS::keyPressEvent(e);
        return;
    }
    if(e->key() == Qt::Key_Space && (e->modifiers() & (Qt::MetaModifier | Qt::ControlModifier)))
    {
        //this->matchCommand();
        _multipleEdit.clear();
        this->displayWidgetInsertCommand();
        return;
    }
    if(this->_completionEngine->isVisible() && e->key() == Qt::Key_Down)
    {
        _multipleEdit.clear();
        this->_completionEngine->setFocus();
        return;
    }
    if(this->_completionEngine->isVisible() && e->key() == Qt::Key_Up)
    {
        _multipleEdit.clear();
        this->_completionEngine->setFocus();
        return;
    }
    if(this->focusWidget() != this || (_completionEngine->isVisible() && e->key() == Qt::Key_Tab))
    {
        _multipleEdit.clear();
        QString insertWord = this->_completionEngine->acceptedWord();
        QRegExp command("\\\\[a-zA-Z\\{\\-_]+$");
        int pos = this->textCursor().positionInBlock();
        QString possibleCommand = this->textCursor().block().text().left(pos);


        if(possibleCommand.indexOf(command) != -1) // the possibleCommand is a command
        {
            QTextCursor cur(this->textCursor());
            cur.setPosition(cur.position() - command.matchedLength(), QTextCursor::KeepAnchor);
            cur.removeSelectedText();
            this->insertPlainText(insertWord);
            this->setFocus();
             // place the cursor at the begin of the command in order to search some argumuments
            cur = this->textCursor();
            cur.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, insertWord.length());
            this->setTextCursor(cur);
            if(!this->selectNextArgument()) // if no argument are found, place the cursor at the end of the command
            {
                cur = this->textCursor();
                cur.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, insertWord.length());
                this->setTextCursor(cur);
            }
        }
        return;
    }
    if(e->key() == Qt::Key_Tab)
    {
        _multipleEdit.clear();
        QTextCursor cursor = this->textCursor();
        if(-1 == cursor.block().text().left(cursor.positionInBlock()).indexOf(QRegExp("^[ \t]*$")))
        {
            if(this->selectNextArgument())
            {
                return;
            }
            if(triggerTabMacros())
            {
                return;
            }
            if(cursor.positionInBlock() < cursor.block().text().size())
            {
                QChar c = cursor.block().text().at(cursor.positionInBlock());
                if(c == '}' || c == ']')
                {
                    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor);
                    this->setTextCursor(cursor);
                    return;
                }
            }
        }
        if(cursor.hasSelection())
        {
            indentSelectedText();
            return;
        }
        cursor.insertText(ConfigManager::Instance.tabToString());
        return;
    }
    if(e->key() == Qt::Key_Backtab)
    {
        QTextCursor cursor = this->textCursor();
        if(cursor.hasSelection())
        {
            desindentSelectedText();
            return;
        }
    }
    if(e->text() == "$")
    {
        QTextCursor cur = this->textCursor();
        int start = cur.selectionStart();
        int end = cur.selectionEnd();
        BlockData * bd = dynamic_cast<BlockData *>(this->textCursor().block().userData());
        if(!bd)
        {
            cur.insertText(QString::fromUtf8("$"));
            this->setTextCursor(cur);
            return;
        }
        if(start == end && ConfigManager::Instance.isDollarAuto() && this->nextChar(cur) == '$' && bd->isAClosingDollar(start - this->textCursor().block().position() - 1))
        {
              cur.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor);
              this->setTextCursor(cur);
              return;
        }
        if(start == end && (!ConfigManager::Instance.isDollarAuto() || bd->isAClosingDollar(start - this->textCursor().block().position()) || (bd->characterData.size() && bd->characterData.last().state == SyntaxHighlighter::Math)))
        {
            cur.insertText(QString::fromUtf8("$"));
            this->setTextCursor(cur);
            return;
        }
        cur.beginEditBlock();
        cur.setPosition(start);
        cur.insertText(QString::fromUtf8("$"));
        cur.setPosition(end+1);
        cur.insertText(QString::fromUtf8("$"));

        if(end == start)
        {
            cur.movePosition(QTextCursor::Left);
        }
        cur.endEditBlock();
        this->setTextCursor(cur);
        _multipleEdit.clear();
        return;
    } else
    if(e->text() == "{" && (this->textCursor().hasSelection() || nextChar(textCursor()).isNull() || !QString(nextChar(textCursor())).contains(QRegExp("[^ \\t]"))))
    {
        QTextCursor cur = this->textCursor();
        cur.beginEditBlock();
        int start = cur.selectionStart();
        int end = cur.selectionEnd();
        cur.setPosition(start);
        cur.insertText(QString::fromUtf8("{"));
        cur.setPosition(end+1);
        cur.insertText(QString::fromUtf8("}"));
        if(end == start)
        {
            cur.movePosition(QTextCursor::Left);
        }
        cur.endEditBlock();
        this->setTextCursor(cur);
        _multipleEdit.clear();
        return;
    }
    else if ((e->key()==Qt::Key_Enter)||(e->key()==Qt::Key_Return))
    {
        _multipleEdit.clear();
        QPlainTextEdit::keyPressEvent(e);
        // add exactly the same  space and tabulation as the previous line.
        newLine();
        return;
    }
    if(_scriptIsRunning)
    {
        if(e->key() != Qt::Key_Backspace && (e->text().isEmpty() || e->text().contains(QRegExp(QString::fromUtf8("[^a-zA-Z0-9èéàëêïîùüû&()\"'\\$§,;\\.+=\\-_*\\/\\\\!?%#@° ]")))))
        {
            _scriptEngine.clear();
            _scriptIsRunning = false;
        }
    }
    if(_multipleEdit.count() //&& e->modifiers() == Qt::NoModifier
            && !e->text().isEmpty() && !e->text().contains(QRegExp(QString::fromUtf8("[^a-zA-Z0-9èéàëêïîùüû&()\"'\\$§,;\\.+=\\-_*\\/\\\\!?%#@° ]")))
            )
    {
        QTextCursor cur1 = this->textCursor();
        cur1.beginEditBlock();
        cur1.insertText(e->text());
        cur1.endEditBlock();
        foreach(QTextCursor cursor, _multipleEdit)
        {
            cursor.joinPreviousEditBlock();
            cursor.insertText(e->text());
            cursor.endEditBlock();
        }
        this->setTextCursor(cur1);
        this->onCursorPositionChange();
        if(_scriptIsRunning)
        {
            _scriptEngine.evaluate();
        }
        return;
    }
    if(_multipleEdit.count() && e->modifiers() == Qt::NoModifier && (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace))
    {
        QTextCursor cur1 = this->textCursor();
        cur1.beginEditBlock();
        if(e->key() == Qt::Key_Delete)
        {
            cur1.deleteChar();
            cur1.endEditBlock();
            foreach(QTextCursor cursor, _multipleEdit)
            {
                cursor.joinPreviousEditBlock();
                cursor.deleteChar();
                cursor.endEditBlock();
            }
        }
        else
        {
            cur1.deletePreviousChar();
            cur1.endEditBlock();
            foreach(QTextCursor cursor, _multipleEdit)
            {
                cursor.joinPreviousEditBlock();
                cursor.deletePreviousChar();
                cursor.endEditBlock();
            }
        }
        this->setTextCursor(cur1);
        this->onCursorPositionChange();
        if(_scriptIsRunning)
        {
            _scriptEngine.evaluate();
        }
        return;
    }
    if(e->key() != Qt::Key_Control && e->key() != Qt::Key_Shift && e->key() != Qt::Key_Alt && e->key() != Qt::Key_AltGr && e->key() != Qt::Key_ApplicationLeft && e->key() != Qt::Key_ApplicationRight)
    {
        _multipleEdit.clear();
        _scriptIsRunning = false;
    }

    if (e->key() ==  Qt::Key_Backspace && !textCursor().hasSelection())
    {
        if(ConfigManager::Instance.isUsingSpaceIndentation())
        {
            QTextCursor cursor = textCursor();
            if(cursor.block().text().left(cursor.positionInBlock()).contains(QRegExp("^[ ]+$")))
            {
                deletePreviousTab();
                return;
            }
        }
    }
    WIDGET_TEXT_EDIT_PARENT_CLASS::keyPressEvent(e);

    if(!e->text().isEmpty() && _scriptIsRunning)
    {
        _scriptEngine.evaluate();
    }
    if(e->modifiers() == Qt::NoModifier && !e->text().isEmpty())
    {
        this->matchCommand();
    }
    if(e->key() == Qt::Key_Right)
    {
        onCursorPositionChange();
    }

}

int WidgetTextEdit::hitTest(const QPoint & pos) const
{
    QTextBlock block = this->firstVisibleBlock();
    while(block.isValid())
    {
        if(block.isVisible())
        {
            if(pos.y() - this->contentOffsetTop() < this->blockBottom(block))
            {
                int lineBottom = this->blockTop(block);
                QTextLine line = block.layout()->lineAt(0);
                for(int line_idx = 0; line_idx < block.layout()->lineCount(); line_idx++)
                {
                    lineBottom += block.layout()->lineAt(line_idx).height();
                    if(pos.y() - this->contentOffsetTop() < lineBottom)
                    {
                        return block.position() + block.layout()->lineAt(line_idx).xToCursor(pos.x());
                    }
                }
                return -1;
            }

        }
        block = block.next();
    }
    return -1;
}


bool WidgetTextEdit::hasArguments()
{
    QTextCursor curStrArg = this->document()->find(QRegExp("\\\\verb\\#\\{\\{([^\\}]*)\\}\\}\\#"));
    return !curStrArg.isNull();
}

bool WidgetTextEdit::selectNextArgument()
{
    //QTextCursor curIntArg = this->document()->find(QRegExp("%[0-9]"),this->textCursor().position());
    QTextCursor curStrArg = this->document()->find(QRegExp("\\\\verb\\#\\{\\{([^\\}]*)\\}\\}\\#"),this->textCursor().position());

  /*  if(!curIntArg.isNull() && (curStrArg.isNull() || curIntArg.selectionStart() < curStrArg.selectionStart()))
    {
        this->setTextCursor(curIntArg);
        return true;
    }*/
    if(!curStrArg.isNull())
    {
        QTextCursor curStrArg2 = this->document()->find(curStrArg.selectedText(), curStrArg.position() + 1);
        if(curStrArg2.isNull())
        {
            curStrArg2 = this->document()->find(curStrArg.selectedText(), 0);
            if(curStrArg2.position() == curStrArg.position())
            {
                curStrArg2 = QTextCursor();
            }
        }
        this->setTextCursor(curStrArg);
        if(!curStrArg2.isNull())
        {
            QList<QTextEdit::ExtraSelection> selections = extraSelections(WidgetTextEdit::ArgumentSelection);
            QTextEdit::ExtraSelection selection;
            QTextCharFormat format = selection.format;
            format.setBackground( QColor("#DDDDDD") );
            format.setForeground( QColor("#333333") );
            selection.format = format;
            selection.cursor = curStrArg2;
            selections.append( selection );
            addExtraSelections(selections, WidgetTextEdit::ArgumentSelection);

            _multipleEdit.clear();
            _multipleEdit.append(curStrArg2);
        }
        return true;
    }
    return false;

}

void WidgetTextEdit::wheelEvent(QWheelEvent * event)
{

    if(event->modifiers() & (Qt::ControlModifier))
    {
        int delta =  event->delta() > 0 ? 1 : -1 ;

        ConfigManager::Instance.changePointSizeBy(delta);

        int pos = this->textCursor().position();
        this->selectAll();
        this->textCursor().setBlockCharFormat(ConfigManager::Instance.getTextCharFormats("normal"));

        QTextCursor cur(this->textCursor());
        cur.setPosition(pos);
        cur.setCharFormat(ConfigManager::Instance.getTextCharFormats("normal"));
        this->setTextCursor(cur);

        this->setCurrentCharFormat(ConfigManager::Instance.getTextCharFormats("normal"));


        if(this->_syntaxHighlighter)
        {
            this->_syntaxHighlighter->rehighlight();
        }
        _widgetLineNumber->updateWidth(this->document()->blockCount());

    }
    else
    {
        WIDGET_TEXT_EDIT_PARENT_CLASS::wheelEvent(event);
    }
    //update();
}
void WidgetTextEdit::setBlockLeftMargin(const QTextBlock &/*textBlock*/, int leftMargin)
{
    QTextBlockFormat format;
    format = textCursor().blockFormat();
    qDebug()<<"oldMargin "<<format.leftMargin()<<" new "<<leftMargin;
    format.setLeftMargin(leftMargin);
    textCursor().setBlockFormat(format);
}

void WidgetTextEdit::initIndentation(void)
{
    this->currentFile->refreshLineNumber();
}

void WidgetTextEdit::updateIndentation(void)
{
    _textStruct->reload();
    //_textStruct->debug();


    if(this->document()->blockCount() != _lineCount)
    {
        this->currentFile->insertLine(this->textCursor().blockNumber(), this->document()->blockCount() - _lineCount);
        emit lineCountChanged(this->document()->blockCount());
    }
    _lineCount = this->document()->blockCount();

}
void WidgetTextEdit::insertFile(QString filename)
{
    // if it is an image, compute the boundingbox option (very usefull for those who use latex + ...
    QImage image(filename);
    QString options = "width=10cm";
    if(!image.isNull())
    {
        options  +=   ", bb=0 0 "+QString::number(image.width())
                    +" "+QString::number(image.height());
    }

    if(!this->getCurrentFile()->getFilename().isEmpty())
    {
        QDir dir(this->getCurrentFile()->getPath());
        filename = dir.relativeFilePath(filename);
    }
    QTextCursor cur = textCursor();
    cur.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
    cur.insertText("\n\\includegraphics["+options+"]{"+filename+"}");
    this->setTextCursor(cur);
}

void WidgetTextEdit::insertFromMimeData(const QMimeData *source)
{
    if(source->hasUrls())
    {
        if(FileManager::Instance.handleMimeData(source))
        {
            return;
        }
    }
    WIDGET_TEXT_EDIT_PARENT_CLASS::insertFromMimeData(source);
}

void WidgetTextEdit::matchAll()
{
    this->_completionEngine->setVisible(false);
    this->matchPar();
    this->matchLat();
    int pos;
    removeExtraSelections(WidgetTextEdit::UnmatchedBrace);
    if(-1 != (pos = matchRightPar(document()->lastBlock(), ParenthesisInfo::RIGHT_BRACE, document()->lastBlock().length() - 1, 0 )))
    {
        QTextEdit::ExtraSelection selection;
        QTextCharFormat format;
        format.setForeground(QBrush(QColor(255,255,255)));
        format.setBackground(QBrush(QColor(255,0,0)));
        selection.format = format;

        QTextCursor cursor = textCursor();
        cursor.setPosition( pos );
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        selection.cursor = cursor;

        QList<QTextEdit::ExtraSelection> selections;
        selections.append( selection );
        addExtraSelections(selections, WidgetTextEdit::UnmatchedBrace);
    }
    if(-1 != (pos = matchLeftPar(document()->firstBlock(), ParenthesisInfo::LEFT_BRACE, 0, 0 )))
    {
        QTextEdit::ExtraSelection selection;
        QTextCharFormat format;
        format.setForeground(QBrush(QColor(255,255,255)));
        format.setBackground(QBrush(QColor(255,0,0)));
        selection.format = format;

        QTextCursor cursor = textCursor();
        cursor.setPosition(pos);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        selection.cursor = cursor;

        QList<QTextEdit::ExtraSelection> selections;
        selections.append( selection );
        addExtraSelections(selections, WidgetTextEdit::UnmatchedBrace);
    }
}

void WidgetTextEdit::displayWidgetInsertCommand()
{
    QTextLine line = this->textCursor().block().layout()->lineForTextPosition(this->textCursor().positionInBlock());
    if(!line.isValid())
    {
        return;
    }
    qreal top = line.position().y() + line.height() + 5  + this->blockTop(this->textCursor().block()) + this->contentOffsetTop();
    QRect geo = WidgetInsertCommand::instance()->geometry();
    geo.moveTo(QPoint(0, top));
    if(geo.bottom() > this->height())
    {
        geo.translate(QPoint(0,-geo.height()-line.height()));
    }
    WidgetInsertCommand::instance()->setParent(this);
    WidgetInsertCommand::instance()->setGeometry(geo);
    WidgetInsertCommand::instance()->setVisible(true);
    WidgetInsertCommand::instance()->show();

}

void WidgetTextEdit::matchCommand()
{
#ifdef OS_MAC
    if(_wierdCircumflexCursor)
    {
        return;
    }
#endif
    QRegExp command("\\\\[a-zA-Z\\{\\-_]+$");
    QRegExp beginCommand("\\\\begin\\{([^\\}]+)\\}$");
    int pos = this->textCursor().positionInBlock();
    QString possibleCommand = this->textCursor().block().text().left(pos);
    if(possibleCommand.indexOf(command) != -1) // the possibleCommand is a command
    {
        //QFontMetrics fm(ConfigManager::Instance.getTextCharFormats("normal").font());
        int length = command.matchedLength();
        possibleCommand = possibleCommand.right(length);

        QTextLine line = this->textCursor().block().layout()->lineForTextPosition(this->textCursor().positionInBlock());
        if(!line.isValid())
        {
            return;
        }
        qreal left = line.cursorToX(this->textCursor().positionInBlock());
        qreal top = line.position().y() + line.height() + 5;
        this->_completionEngine->proposeCommand(left,top + this->blockTop(this->textCursor().block()) + this->contentOffsetTop(), line.height(),possibleCommand);
        if(this->_completionEngine->isVisible())// && e->key() == Qt::Key_Down)
        {
            this->_completionEngine->setFocus();
        }
    }

}

void WidgetTextEdit::matchPar()
{
    QTextBlock textBlock = textCursor().block();
    BlockData *data = static_cast<BlockData *>( textBlock.userData() );
    if ( data ) {
        QVector<ParenthesisInfo *> infos = data->parentheses();
        int pos = textCursor().block().position();

        for (int i=0; i < infos.size(); ++i) {
            ParenthesisInfo *info = infos.at(i);
            int curPos = textCursor().position() - textBlock.position();
            // Clicked on a left parenthesis?
            if ( info->position <= curPos-1 && info->position + info->length > curPos-1 && !(info->type & ParenthesisInfo::RIGHT) ) {
                if ( -1 != matchLeftPar(textBlock, info->type, i+1, 0 ) )
                    createParSelection( pos + info->position, info->length );
            }

            // Clicked on a right parenthesis?
            if ( info->position <= curPos-1 && info->position + info->length > curPos-1 && (info->type & ParenthesisInfo::RIGHT)) {
                if (-1 != matchRightPar( textBlock, info->type, i-1, 0 ) )
                    createParSelection( pos + info->position, info->length );
            }
        }
    }
}
int WidgetTextEdit::matchLeftPar(	QTextBlock currentBlock, int type, int index, int numLeftPar )
{
    BlockData *data = static_cast<BlockData *>( currentBlock.userData() );
    if(data)
    {
        QVector<ParenthesisInfo *> infos = data->parentheses();
        int docPos = currentBlock.position();

        // Match in same line?
        for ( ; index<infos.size(); ++index ) {
            ParenthesisInfo *info = infos.at(index);

            if ( info->type == type ) {
                ++numLeftPar;
                continue;
            }

            if ( info->type == type + ParenthesisInfo::RIGHT )
            {
                if(numLeftPar == 0) {
                    createParSelection( docPos + info->position, info->length );
                    return docPos + info->position;
                }
                else
                {
                    --numLeftPar;
                }
            }

        }
    }

    // No match yet? Then try next block
    currentBlock = currentBlock.next();
    if ( currentBlock.isValid() )
        return matchLeftPar( currentBlock, type, 0, numLeftPar );

    // No match at all
    return -1;
}

int WidgetTextEdit::matchRightPar(QTextBlock currentBlock, int type, int index, int numRightPar)
{
    BlockData *data = static_cast<BlockData *>( currentBlock.userData() );
    if(data)
    {
        QVector<ParenthesisInfo *> infos = data->parentheses();
        int docPos = currentBlock.position();

        // Match in same line?
        for (int j=index; j>=0 && j < infos.size(); --j ) {
            ParenthesisInfo *info = infos.at(j);

            if ( info->type == type ) {
                ++numRightPar;
                continue;
            }

            if ( info->type == type - ParenthesisInfo::RIGHT)
            {
                if( numRightPar == 0 ) {
                    createParSelection( docPos + info->position, info->length );
                    return  docPos + info->position;
                }
                else
                {
                    --numRightPar;
                }
            }
        }
    }

    // No match yet? Then try previous block
    currentBlock = currentBlock.previous();
    if ( currentBlock.isValid() ) {

        // Recalculate correct index first
        BlockData *data = static_cast<BlockData *>( currentBlock.userData() );
        if(data)
        {
            QVector<ParenthesisInfo *> infos = data->parentheses();
            return matchRightPar( currentBlock, type, infos.size()-1, numRightPar );
        }
    }

    // No match at all
    return -1;
}

void WidgetTextEdit::createParSelection( int pos, int length )
{
    QList<QTextEdit::ExtraSelection> selections = extraSelections(WidgetTextEdit::ParenthesesMatchingSelection);
    QTextEdit::ExtraSelection selection;
    QTextCharFormat format = ConfigManager::Instance.getTextCharFormats("matched");
    selection.format = format;

    QTextCursor cursor = textCursor();
    cursor.setPosition( pos );
    cursor.movePosition( QTextCursor::NextCharacter, QTextCursor::KeepAnchor, length);
    selection.cursor = cursor;
    selections.append( selection );
    addExtraSelections(selections, WidgetTextEdit::ParenthesesMatchingSelection);
}
void WidgetTextEdit::matchLat()
{

    QTextCursor cursor = textCursor();

    BlockData * data = dynamic_cast<BlockData*>(cursor.block().userData());
    if(!data)
    {
        return;
    }
    foreach(const LatexBlockInfo* latexBlockInfo, data->latexblocks())
    {
        int position = latexBlockInfo->position + cursor.block().position();
        if(cursor.selectedText() == latexBlockInfo->name)
        {
            if(position == cursor.selectionStart() - 7)
            {
                const StructItem * item = _textStruct->environmentPath().top();
                int endPos = item->end;
                QTextCursor endCursor = textCursor();
                endCursor.setPosition(endPos - 1);
                endCursor.setPosition(endPos - 3 - item->name.length(), QTextCursor::KeepAnchor);
                if(endCursor.selectedText() == "{"+cursor.selectedText()+"}")
                {
                    endCursor.setPosition(endPos - 2);
                    endCursor.setPosition(endPos - 2 - item->name.length(), QTextCursor::KeepAnchor);
                    _multipleEdit << endCursor;

                    QList<QTextEdit::ExtraSelection> selections = extraSelections(WidgetTextEdit::ParenthesesMatchingSelection);
                    QTextEdit::ExtraSelection selection;
                    QTextCharFormat format = selection.format;
                    format.setBackground( QColor("#DDDDDD") );
                    format.setForeground( QColor("#333333") );
                    selection.format = format;
                    selection.cursor = endCursor;
                    selections.append(selection);
                    addExtraSelections(selections, WidgetTextEdit::ParenthesesMatchingSelection);
                }
            }
            if(position == cursor.selectionEnd() + 2)
            {
                const StructItem * item = _textStruct->environmentPath().top();
                int startPos = item->begin;
                QTextCursor startCursor = textCursor();
                startCursor.setPosition(startPos + 6);
                startCursor.setPosition(startPos + 8 + item->name.length(), QTextCursor::KeepAnchor);
                if(startCursor.selectedText() == "{"+cursor.selectedText()+"}")
                {
                    startCursor.setPosition(startPos + 7);
                    startCursor.setPosition(startPos + 7 + item->name.length(), QTextCursor::KeepAnchor);
                    _multipleEdit << startCursor;

                    QList<QTextEdit::ExtraSelection> selections = extraSelections(WidgetTextEdit::ParenthesesMatchingSelection);
                    QTextEdit::ExtraSelection selection;
                    QTextCharFormat format = selection.format;
                    format.setBackground( QColor("#DDDDDD") );
                    format.setForeground( QColor("#333333") );
                    selection.format = format;
                    selection.cursor = startCursor;
                    selections.append(selection);
                    addExtraSelections(selections, WidgetTextEdit::ParenthesesMatchingSelection);
                }
            }
        }
    }
}

int WidgetTextEdit::matchLeftLat(	QTextBlock currentBlock, int index, int numLeftLat, int bpos )
{
    BlockData *data = static_cast<BlockData *>( currentBlock.userData() );
    QVector<LatexBlockInfo *> infos = data->latexblocks();

    // Match in same line?
    for ( ; index<infos.size(); ++index ) {
        LatexBlockInfo *info = infos.at(index);

        if ( info->type == LatexBlockInfo::ENVIRONEMENT_BEGIN ) {
            ++numLeftLat;
            continue;
        }

        if ( info->type == LatexBlockInfo::ENVIRONEMENT_END && numLeftLat == 0 ) {
            createLatSelection( bpos,currentBlock.blockNumber() );
            return info->position + currentBlock.position();
        } else
            --numLeftLat;
    }

    // No match yet? Then try next block
    currentBlock = currentBlock.next();
    if ( currentBlock.isValid() )
        return matchLeftLat( currentBlock, 0, numLeftLat, bpos );

    // No match at all
    return -1;
}

int WidgetTextEdit::matchRightLat(QTextBlock currentBlock, int index, int numRightLat, int epos)
{

    BlockData *data = static_cast<BlockData *>( currentBlock.userData() );
    QVector<LatexBlockInfo *> infos = data->latexblocks();

    // Match in same line?
    for (int j=index; j>=0; --j ) {
        LatexBlockInfo *info = infos.at(j);

        if ( info->type == LatexBlockInfo::ENVIRONEMENT_END ) {
            ++numRightLat;
            continue;
        }

        if ( info->type == LatexBlockInfo::ENVIRONEMENT_BEGIN && numRightLat == 0 ) {
            createLatSelection( epos, currentBlock.blockNumber() );
            return info->position + currentBlock.position();
        } else
            --numRightLat;
    }

    // No match yet? Then try previous block
    currentBlock = currentBlock.previous();
    if ( currentBlock.isValid() ) {

        // Recalculate correct index first
        BlockData *data = static_cast<BlockData *>( currentBlock.userData() );
        QVector<LatexBlockInfo *> infos = data->latexblocks();

        return matchRightLat( currentBlock, infos.size()-1, numRightLat, epos );
    }

    // No match at all
    return -1;
}

QChar WidgetTextEdit::nextChar(const QTextCursor cursor) const
{
    QTextBlock block = cursor.block();
    int position = cursor.positionInBlock();
    if(block.text().length() > position)
    {
        return block.text().at(position);
    }
    return QChar::Null;
}

void WidgetTextEdit::createLatSelection( int start, int end )
{
    int s=qMin(start,end);
    int e=qMax(start,end);
    emit setBlockRange(s,e);
    //endBlock=e;
}

void WidgetTextEdit::updateTabWidth()
{
    QFontMetrics fm(ConfigManager::Instance.getTextCharFormats("normal").font());
    WIDGET_TEXT_EDIT_PARENT_CLASS::setTabStopWidth(ConfigManager::Instance.tabWidth() * fm.width(" "));
}

void WidgetTextEdit::goToLine(int line, QString stringSelected)
{
    QTextCursor cursor(this->textCursor());
    cursor.setPosition(this->document()->findBlockByNumber(line - 1).position());
    this->setTextCursor(cursor);
    ensureCursorVisible();
    this->highlightSyncedLine();
    if(!stringSelected.isEmpty() && !stringSelected.isEmpty())
    {
        int index;
        if((index = this->document()->findBlockByNumber(line - 1).text().indexOf(stringSelected)) != -1)
        {
            cursor.movePosition(QTextCursor::Right,QTextCursor::MoveAnchor,index);
            cursor.movePosition(QTextCursor::Right,QTextCursor::KeepAnchor,stringSelected.length());
            setTextCursor(cursor);
        }
    }
}

void WidgetTextEdit::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;
    QTextCursor cursor = textCursor();
    int blockNumber = cursor.blockNumber();
    cursor.movePosition(QTextCursor::StartOfBlock);
    while(cursor.blockNumber() == blockNumber)
    {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(ConfigManager::Instance.getTextCharFormats("selected-line").background());
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = QTextCursor(cursor);
        selection.cursor.clearSelection();
        extraSelections.append(selection);
        if(!cursor.movePosition(QTextCursor::Down))
        {
            break;
        }
    }
    addExtraSelections(extraSelections, WidgetTextEdit::CurrentLineSelection);
}
void WidgetTextEdit::highlightSyncedLine(int line)
{
    if( line == -1)
    {
        line = textCursor().blockNumber();
    }
    QList<QTextEdit::ExtraSelection> extraSelections;// = this->extraSelections();


    QTextCursor cursor = textCursor();
    cursor.setPosition(this->document()->findBlockByNumber(line).position());
    while(cursor.blockNumber() == line)
    {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(ConfigManager::Instance.getTextCharFormats("synced-line").background());
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = QTextCursor(cursor);
        selection.cursor.clearSelection();
        extraSelections.append(selection);
        if(!cursor.movePosition(QTextCursor::Down))
        {
            break;
        }
    }

    addExtraSelections(extraSelections, WidgetTextEdit::CurrentLineSelection);
    matchPar();
}

int WidgetTextEdit::centerBlockNumber()
{
    int centerBlockNumber = this->firstVisibleBlock().blockNumber();
    while(centerBlockNumber < this->document()->blockCount())
    {
        if(this->contentOffset().y() + this->blockTop(centerBlockNumber) > this->height() / 2)
        {
            break;
        }
        ++centerBlockNumber;
    }
    return centerBlockNumber - 1;

}

QString WidgetTextEdit::wordOnLeft()
{
    QTextCursor cursor = textCursor();
    if(cursor.hasSelection())
    {
        return cursor.selectedText();
    }

    BlockData *data = static_cast<BlockData *>(cursor.block().userData() );
    if(data && cursor.positionInBlock() > 0 && data->characterData[cursor.positionInBlock() - 1].state == SyntaxHighlighter::Command)
    {
        return "";
    }
    QString lineBegining = cursor.block().text().left(cursor.positionInBlock());
    QRegExp lastWordPatter("([a-zA-Z0-9èéàëêïîùüû\\-_*]+)$");
    if(lineBegining.indexOf(lastWordPatter) != -1)
    {
        return lastWordPatter.capturedTexts().last();
    }
    return QString();
}
void WidgetTextEdit::deletePreviousTab()
{
    QTextCursor cursor = textCursor();
    if(cursor.block().text().left(cursor.positionInBlock()).contains(QRegExp("^[ ]*$")))
    {
        cursor.joinPreviousEditBlock();
        cursor.deletePreviousChar(); //delete at least one char
        while(cursor.positionInBlock() % ConfigManager::Instance.tabWidth())
        {
            cursor.deletePreviousChar();
        }
        cursor.endEditBlock();
    }
}

void WidgetTextEdit::newLine()
{
    QTextCursor cursor = textCursor();
    cursor.joinPreviousEditBlock();
    QTextBlock block=cursor.block();
    QTextBlock blockprev=block.previous();
    if(blockprev.isValid())
    {
        QString txt=blockprev.text();
        int j=0;
        while ( (j<txt.count()) && ((txt[j]==' ') || txt[j]=='\t') )
        {
            cursor.insertText(QString(txt[j]));
            j++;
        }

    }
    cursor.endEditBlock();
}

void WidgetTextEdit::setTextCursorPosition(int pos)
{
    QTextCursor cursor = this->textCursor();
    cursor.setPosition(pos);
    this->setTextCursor(cursor);
}

bool WidgetTextEdit::triggerTabMacros()
{
    QList<Macro> list = MacroEngine::Instance.tabMacros();
    foreach(const Macro &macro, list)
    {
        if(onMacroTriggered(macro))
        {
            return true;
        }
    }
    return false;
}

bool WidgetTextEdit::onMacroTriggered(Macro macro, bool force)
{
    QTextCursor cursor = textCursor();
    QString word;
    if(cursor.hasSelection())
    {
        word = cursor.selectedText();
    }
    else
    {
        word = this->wordOnLeft();
    }
    word = word.replace("\u2029", "\n");

    QRegExp pattern("^"+macro.leftWord+"$");
    bool patternExists = word.contains(pattern);
    if(!patternExists && !force)
    {
        return false;
    }

    cursor = textCursor();
    cursor.beginEditBlock();

    QString content = macro.content;

    if(patternExists && !cursor.hasSelection() && pattern.capturedTexts().count() > 1)
    {
         cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, word.length());
         this->setTextCursor(cursor);
    }

    int pos = textCursor().position();
    _scriptIsRunning = true;
    _scriptEngine.parse(content, this, pattern.capturedTexts().toVector());

    cursor = textCursor();
    cursor.setPosition(pos);
    this->setTextCursor(cursor);
    cursor.endEditBlock();

    _multipleEdit.clear();
    selectNextArgument();
    //_scriptEngine.evaluate();
    return true;

}

void WidgetTextEdit::desindentSelectedText()
{
    QTextCursor cursor = textCursor();
    int startPos = cursor.selectionStart();
    int endPos = cursor.selectionEnd();

    cursor.setPosition(startPos);
    cursor.beginEditBlock();
    QTextBlock block = cursor.block();
    while(block.isValid() && block.position() <= endPos)
    {
        cursor.setPosition(block.position());
        for(int idx = 0; idx < ConfigManager::Instance.tabWidth(); ++idx)
        {
            if(block.text().contains(QRegExp("^[ \\t]")))
            {
                cursor.deleteChar();
                --endPos;
            }
        }
        block = block.next();
    }
    cursor.endEditBlock();
}
void WidgetTextEdit::indentSelectedText()
{
    QTextCursor cursor = textCursor();
    int startPos = cursor.selectionStart();
    int endPos = cursor.selectionEnd();
    QString tabString = ConfigManager::Instance.tabToString();

    cursor.beginEditBlock();
    cursor.setPosition(startPos);
    QTextBlock block = cursor.block();
    while(block.isValid() && block.position() <= endPos)
    {
        cursor.setPosition(block.position());
        cursor.insertText(tabString);
        endPos += tabString.size();
        block = block.next();
    }
    cursor.endEditBlock();
}
void WidgetTextEdit::initTheme()
{

    this->setStyleSheet(QString("QPlainTextEdit { border: 0px solid ")+
        ConfigManager::Instance.colorToString(ConfigManager::Instance.getTextCharFormats("textedit-border").foreground().color())+"; "+
        QString("border-right: 1px solid ")+
        ConfigManager::Instance.colorToString(ConfigManager::Instance.getTextCharFormats("textedit-border").foreground().color())+"; "+
        QString("color: ")+
        ConfigManager::Instance.colorToString(ConfigManager::Instance.getTextCharFormats("normal").foreground().color())+"; "+
        QString("background-color: ")+
        ConfigManager::Instance.colorToString(ConfigManager::Instance.getTextCharFormats("normal").background().color())+"; "+
                        QString("selection-color: ")+
                        ConfigManager::Instance.colorToString(ConfigManager::Instance.getTextCharFormats("selection").foreground().color())+"; "+
                        QString("selection-background-color: ")+
                        ConfigManager::Instance.colorToString(ConfigManager::Instance.getTextCharFormats("selection").background().color())+
"; }");

    QPalette p = this->palette();
    p.setBrush(QPalette::HighlightedText, QBrush(QColor(0, 0, 0, 0), Qt::NoBrush));
    this->setPalette(p);

    this->setCurrentCharFormat(ConfigManager::Instance.getTextCharFormats("normal"));
    QTextCursor cur = this->textCursor();
    cur.setCharFormat(ConfigManager::Instance.getTextCharFormats("normal"));
    this->setTextCursor(cur);
    //this->widgetTextEdit->setCurrentFont(ConfigManager::Instance.getTextCharFormats("normal").font());




}


void WidgetTextEdit::setBeamCursor()
{
    this->viewport()->setCursor(Qt::IBeamCursor);
#ifdef OS_MAC
    if(ConfigManager::Instance.getTextCharFormats("normal").background().color().value()<100) // if it's a dark color
    {
        QPixmap whiteBeamPixmap(":/data/cursor/whiteBeam.png");
        QCursor whiteBeam(whiteBeamPixmap);
        this->viewport()->setCursor(whiteBeam);
    }
#endif
}

void WidgetTextEdit::fold(int start, int end)
{
    _foldedLines.insert(start, end);
    for (int i = start + 1; i <= end; i++)
    {
        document()->findBlockByNumber(i).setVisible(false);
    }
    update();
    resizeEvent(new QResizeEvent(QSize(0, 0), size()));
    viewport()->update();
    _widgetLineNumber->update();
    ensureCursorVisible();
}
void WidgetTextEdit::unfold(int start)
{
    if (!_foldedLines.keys().contains(start)) return;
    int end = _foldedLines.value(start);
    _foldedLines.remove(start);
    int i=start+1;
    while (i<=end)
    {
        if (_foldedLines.keys().contains(i))
        {
            document()->findBlockByNumber(i).setVisible(true);
            i=_foldedLines.value(i);
        }
        else document()->findBlockByNumber(i).setVisible(true);
        i++;
    }
    update();
    resizeEvent(new QResizeEvent(QSize(0, 0), size()));
    viewport()->update();
    _widgetLineNumber->update();
    //ensureCursorVisible();
}

void WidgetTextEdit::highlightSearchResult(const QTextCursor &searchResult)
{
    // TODO: add Overlay to have a catchy selection

    /*QList<QTextEdit::ExtraSelection> extraSelections;
    QTextEdit::ExtraSelection selection;
    selection.format.setUnderlineColor(ConfigManager::Instance.getTextCharFormats("search-result").background().color());
    selection.format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    selection.format.setFontWeight(100);
    selection.cursor = QTextCursor(searchResult);
    extraSelections.append(selection);
    addExtraSelections(extraSelections, WidgetTextEdit::SearchResultSelection);*/
}

void WidgetTextEdit::goToSection(QString sectionName)
{
    int line = _textStruct->sectionNameToLine(sectionName);
    if(line != -1)
    {
        goToLine(line + 1);
    }
}

void WidgetTextEdit::onBlockCountChanged(int newBlockCount)
{
    int delta = newBlockCount - _lastBlockCount;
    QTextBlock block = textCursor().block();
    int currentline = block.blockNumber();
    int i = currentline-1;
    QList<int> start,end;

    while (block.isValid())
    {
        if (_foldedLines.keys().contains(i))
        {
            start.append(i+delta);
            end.append(_foldedLines[i]+delta);
            _foldedLines.remove(i);
        }
        i++;
        block = block.next();
    }
    for (int i = 0; i < start.count(); ++i)
    {
        _foldedLines.insert(start[i],end[i]);
    }
    _lastBlockCount = newBlockCount;
}

void WidgetTextEdit::addTextCursor(QTextCursor cursor)
{
    _multipleEdit << cursor;
}

void WidgetTextEdit::comment()
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    cursor.setPosition(cursor.selectionStart());
    cursor.movePosition(QTextCursor::StartOfBlock);
    while(cursor.position() <= textCursor().selectionEnd())
    {
        if(!cursor.block().text().isEmpty() && !cursor.block().text().contains(QRegExp("^[ \\t]*$")))
        {
            cursor.insertText("%");
        }
        if(!cursor.movePosition(QTextCursor::NextBlock))
        {
            //there is no next block
            break;
        }
    }
    cursor.endEditBlock();
}

void WidgetTextEdit::toggleComment()
{
    QTextCursor cursor = textCursor();
    cursor.setPosition(cursor.selectionStart());
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.setPosition(textCursor().selectionEnd(), QTextCursor::KeepAnchor);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);

    if(cursor.selectedText().contains(QRegExp("\u2029[ \\t]*%")) || cursor.selectedText().contains(QRegExp("^[ \\t]*%")))
    {
        uncomment();
    }
    else
    {
        comment();
    }
}

void WidgetTextEdit::uncomment()
{
    QTextCursor cursor = textCursor();
    cursor.beginEditBlock();
    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();
    cursor.setPosition(start);
    cursor.movePosition(QTextCursor::StartOfBlock);

    // if the selection does not include the first % the start of the selection must be move on the left by one.
    if(cursor.block().text().left(start - cursor.block().position()).contains(QRegExp("^([ \\t]*)%")))
    {
        --start;
    }

    while(cursor.position() < end)
    {
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString t = cursor.selectedText();
        t.replace(QRegExp("^([ \\t]*)%"), "\\1");
        --end;
        cursor.insertText(t);
        cursor.movePosition(QTextCursor::NextBlock);
    }
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.endEditBlock();
    setTextCursor(cursor);
}

void WidgetTextEdit::updateCompletionCustomWords()
 {
    _completionEngine->addCustomWordFromSource();
}
