/***************************************************************************
 *   Copyright (C) 2003 by S�bastien Lao�t                                 *
 *   slaout@linux62.org                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <qdragobject.h>
#include <qdom.h>
#include <qpainter.h>
#include <qstyle.h>
#include <kstyle.h>
#include <qtooltip.h>
#include <qlistview.h>
#include <qcursor.h>
#include <qsimplerichtext.h>
#include <ktextedit.h>
#include <qpoint.h>
#include <qstringlist.h>
#include <kapplication.h>
#include <kglobalsettings.h>
#include <kopenwith.h>
#include <kservice.h>
#include <klocale.h>
#include <kglobalaccel.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <kfiledialog.h>
#include <kaboutdata.h>
#include <klineedit.h>

#include <kpopupmenu.h>
#include <kiconloader.h>
#include <krun.h>

#include <qtoolbar.h>
#include <qclipboard.h>

#include <kmessagebox.h>
#include <qinputdialog.h>

#include <qlayout.h>

#include <stdlib.h>     // rand() function
#include <qdatetime.h>  // seed for rand()

#include "basket.h"
#include "note.h"
#include "notedrag.h"
#include "notefactory.h"
#include "noteedit.h"
#include "xmlwork.h"
#include "global.h"
#include "backgroundmanager.h"
#include "settings.h"
#include "tools.h"
#include "debugwindow.h"
#include "exporterdialog.h"
#include "config.h"
#include "popupmenu.h"

#include <iostream>

/** Class NoteSelection: */

NoteSelection* NoteSelection::nextStacked()
{
	// First, search in the childs:
	if (firstChild)
		if (firstChild->note && firstChild->note->content())
			return firstChild;
		else
			return firstChild->nextStacked();

	// Then, in the next:
	if (next)
		if (next->note && next->note->content())
			return next;
		else
			return next->nextStacked();

	// And finally, in the parent:
	NoteSelection *node = parent;
	while (node)
		if (node->next)
			if (node->next->note && node->next->note->content())
				return node->next;
			else
				return node->next->nextStacked();
		else
			node = node->parent;

	// Not found:
	return 0;
}

NoteSelection* NoteSelection::firstStacked()
{
	if (!this)
		return 0;

	if (note && note->content())
		return this;
	else
		return nextStacked();
}

void NoteSelection::append(NoteSelection *node)
{
	if (!this || !node)
		return;

	if (firstChild) {
		NoteSelection *last = firstChild;
		while (last->next)
			last = last->next;
		last->next = node;
	} else
		firstChild = node;

	while (node) {
		node->parent = this;
		node = node->next;
	}
}

int NoteSelection::count()
{
	if (!this)
		return 0;

	int count = 0;

	for (NoteSelection *node = this; node; node = node->next)
		if (node->note && node->note->content())
			++count;
		else
			count += node->firstChild->count();

	return count;
}

QValueList<Note*> NoteSelection::parentGroups()
{
	QValueList<Note*> groups;

	// For each note:
	for (NoteSelection *node = firstStacked(); node; node = node->nextStacked())
		// For each parent groups of the note:
		for (Note *note = node->note->parentNote(); note; note = note->parentNote())
			// Add it (if it was not already in the list):
			if (!note->isColumn() && !groups.contains(note))
				groups.append(note);

	return groups;
}

/** Class DecoratedBasket: */

DecoratedBasket::DecoratedBasket(QWidget *parent, const QString &folderName, const char *name, WFlags fl)
 : QWidget(parent, name, fl)
{
	m_layout = new QVBoxLayout(this);
	m_filter = new FilterBar(this);
	m_basket = new Basket(this, folderName);
	m_layout->addWidget(m_basket);
	setFilterBarPosition(Settings::filterOnTop());

	m_filter->setShown(true);
	m_basket->setFocus(); // To avoid the filter bar have focus on load

	connect( m_filter, SIGNAL(newFilter(const FilterData&)), m_basket, SLOT(newFilter(const FilterData&)) );
	connect( m_filter, SIGNAL(escapePressed()),              m_basket, SLOT(cancelFilter())               );
	connect( m_filter, SIGNAL(returnPressed()),              m_basket, SLOT(validateFilter())             );

	connect( m_basket, SIGNAL(postMessage(const QString&)),      Global::mainContainer, SLOT(postStatusbarMessage(const QString&)) );
	connect( m_basket, SIGNAL(setStatusBarText(const QString&)), Global::mainContainer, SLOT(setStatusBarHint(const QString&))     );
	connect( m_basket, SIGNAL(resetStatusBarText()),             Global::mainContainer, SLOT(updateStatusBarHint())                );
}

DecoratedBasket::~DecoratedBasket()
{
}

void DecoratedBasket::setFilterBarPosition(bool onTop)
{
	m_layout->remove(m_filter);
	if (onTop) {
		m_layout->insertWidget(0, m_filter);
		setTabOrder(this/*(QWidget*)parent()*/, m_filter);
		setTabOrder(m_filter, m_basket);
		setTabOrder(m_basket, (QWidget*)parent());
	} else {
		m_layout->addWidget(m_filter);
		setTabOrder(this/*(QWidget*)parent()*/, m_basket);
		setTabOrder(m_basket, m_filter);
		setTabOrder(m_filter, (QWidget*)parent());
	}
}

void DecoratedBasket::setFilterBarShown(bool show, bool switchFocus)
{
	m_basket->setShowFilterBar(true);//show);
//	m_basket->save();
	// In this order (m_basket and then m_filter) because setShown(false)
	//  will call resetFilter() that will update actions, and then check the
	//  Ctrl+F action whereas it should be unchecked
	//  FIXME: It's very uggly all those things
	m_filter->setShown(true);//show);
	if (show) {
		if (switchFocus)
			m_filter->setEditFocus();
	} else if (m_filter->hasEditFocus())
		m_basket->setFocus();
}

void DecoratedBasket::resetFilter()
{
	m_filter->reset();
}

/** Class TransparentWidget */

TransparentWidget::TransparentWidget(Basket *basket)
 : QWidget(basket->viewport(), "", Qt::WNoAutoErase), m_basket(basket)
{
	setFocusPolicy(QWidget::NoFocus);
	setWFlags(Qt::WNoAutoErase);
	setMouseTracking(true); // To receive mouseMoveEvents

	basket->viewport()->installEventFilter(this);
}

/*void TransparentWidget::reparent(QWidget *parent, WFlags f, const QPoint &p, bool showIt)
{
	QWidget::reparent(parent, Qt::WNoAutoErase, p, showIt);
}*/

void TransparentWidget::setPosition(int x, int y)
{
	m_x = x;
	m_y = y;
}

void TransparentWidget::paintEvent(QPaintEvent*event)
{
	QWidget::paintEvent(event);
	QPainter painter(this);

//	painter.save();

	painter.translate(-m_x, -m_y);
	m_basket->drawContents(&painter, m_x, m_y, width(), height());

//	painter.restore();
//	painter.setPen(Qt::blue);
//	painter.drawRect(0, 0, width(), height());
}

void TransparentWidget::mouseMoveEvent(QMouseEvent *event)
{
	QMouseEvent *translated = new QMouseEvent(QEvent::MouseMove, event->pos() + QPoint(m_x, m_y), event->button(), event->state());
	m_basket->contentsMouseMoveEvent(translated);
	delete translated;
}

bool TransparentWidget::eventFilter(QObject */*object*/, QEvent *event)
{
	// If the parent basket viewport has changed, we should change too:
	if (event->type() == QEvent::Paint)
		update();

	return false; // Event not consumed, in every cases (because it's only a notification)!
}

/** Class Basket: */

const int Basket::FRAME_DELAY = 50/*1500*/; // Delay between two animation "frames" in milliseconds

/*
 * Convenient function (defined in note.cpp !):
 */
void drawGradient( QPainter *p, const QColor &colorTop, const QColor & colorBottom,
                   int x, int y, int w, int h,
                   bool sunken, bool horz, bool flat  );

/*
 * Defined in note.cpp:
 */
extern void substractRectOnAreas(const QRect &rectToSubstract, QValueList<QRect> &areas, bool andRemove = true);

void debugZone(int zone)
{
	QString s;
	switch (zone) {
		case Note::Handle:        s = "Handle";              break;
		case Note::Group:         s = "Group";               break;
		case Note::TagsArrow:     s = "TagsArrow";           break;
		case Note::Custom0:       s = "Custom0";             break;
		case Note::GroupExpander: s = "GroupExpander";       break;
		case Note::Content:       s = "Content";             break;
		case Note::Link:          s = "Link";                break;
		case Note::TopInsert:     s = "TopInsert";           break;
		case Note::TopGroup:      s = "TopGroup";            break;
		case Note::BottomInsert:  s = "BottomInsert";        break;
		case Note::BottomGroup:   s = "BottomGroup";         break;
		case Note::BottomColumn:  s = "BottomColumn";        break;
		case Note::None:          s = "None";                break;
		default:
			if (zone == Note::Emblem0)
				s = "Emblem0";
			else
				s = "Emblem0+" + QString::number(zone - Note::Emblem0);
			break;
	}
	std::cout << s << std::endl;
}

#define FOR_EACH_NOTE(noteVar) \
	for (Note *noteVar = firstNote(); noteVar; noteVar = noteVar->next())

void Basket::prependNoteIn(Note *note, Note *in)
{
	if (!note)
		// No note to prepend:
		return;

	if (in) {
		// The normal case:
		preparePlug(note);

		Note *last = note->lastSibling();

		for (Note *n = note; n; n = n->next())
			n->setParentNote(in);
//		note->setPrev(0L);
		last->setNext(in->firstChild());

		if (in->firstChild())
			in->firstChild()->setPrev(last);

		in->setFirstChild(note);

		if (m_loaded)
			signalCountsChanged();
	} else
		// Prepend it directly in the basket:
		appendNoteBefore(note, firstNote());
}

void Basket::appendNoteIn(Note *note, Note *in)
{
	if (!note)
		// No note to append:
		return;

	if (in) {
		// The normal case:
		preparePlug(note);

//		Note *last = note->lastSibling();
		Note *lastChild = in->lastChild();

		for (Note *n = note; n; n = n->next())
			n->setParentNote(in);
		note->setPrev(lastChild);
//		last->setNext(0L);

		if (!in->firstChild())
			in->setFirstChild(note);

		if (lastChild)
			lastChild->setNext(note);

		if (m_loaded)
			signalCountsChanged();
	} else
		// Prepend it directly in the basket:
		appendNoteAfter(note, lastNote());
}

void Basket::appendNoteAfter(Note *note, Note *after)
{
	if (!note)
		// No note to append:
		return;

	if (!after)
		// By default, insert after the last note:
		after = lastNote();

	if (m_loaded && after && !after->isFree() && !after->isColumn())
		for (Note *n = note; n; n = n->next())
			n->inheritTagsOf(after);

//	if (!alreadyInBasket)
	preparePlug(note);

	Note *last = note->lastSibling();
	if (after) {
		// The normal case:
		for (Note *n = note; n; n = n->next())
			n->setParentNote(after->parentNote());
		note->setPrev(after);
		last->setNext(after->next());
		after->setNext(note);
		if (last->next())
			last->next()->setPrev(last);
	} else {
		// There is no note in the basket:
		for (Note *n = note; n; n = n->next())
			n->setParentNote(0);
		m_firstNote = note;
//		note->setPrev(0);
//		last->setNext(0);
	}

//	if (!alreadyInBasket)
	if (m_loaded)
		signalCountsChanged();
}

void Basket::appendNoteBefore(Note *note, Note *before)
{
	if (!note)
		// No note to append:
		return;

	if (!before)
		// By default, insert before the first note:
		before = firstNote();

	if (m_loaded && before && !before->isFree() && !before->isColumn())
		for (Note *n = note; n; n = n->next())
			n->inheritTagsOf(before);

	preparePlug(note);

	Note *last = note->lastSibling();
	if (before) {
		// The normal case:
		for (Note *n = note; n; n = n->next())
			n->setParentNote(before->parentNote());
		note->setPrev(before->prev());
		last->setNext(before);
		before->setPrev(last);
		if (note->prev())
			note->prev()->setNext(note);
		else {
			if (note->parentNote())
				note->parentNote()->setFirstChild(note);
			else
				m_firstNote = note;
		}
	} else {
		// There is no note in the basket:
		for (Note *n = note; n; n = n->next())
			n->setParentNote(0);
		m_firstNote = note;
//		note->setPrev(0);
//		last->setNext(0);
	}

	if (m_loaded)
		signalCountsChanged();
}

DecoratedBasket* Basket::decoration()
{
	return (DecoratedBasket*)parent();
}

void Basket::preparePlug(Note *note)
{
	// Select only the new notes, compute the new notes count and the new number of found notes:
	if (m_loaded)
		unselectAll();
	int count  = 0;
	int founds = 0;
	Note *last = 0;
	for (Note *n = note; n; n = n->next()) {
		if (m_loaded)
			n->setSelectedRecursivly(true); // Notes should have a parent basket (and they have, so that's OK).
		count  += n->count();
		founds += n->newFilter(decoration()->filterData());
		last = n;
	}
	m_count += count;
	m_countFounds += founds;

	// Focus the last inserted note:
	if (m_loaded && last) {
		setFocusedNote(last);
		m_startOfShiftSelectionNote = (last->isGroup() ? last->lastRealChild() : last);
	}

	// If some notes don't match (are hidden), tell it to the user:
	if (m_loaded && founds < count) {
		if      (count == 1)          postMessage( i18n("The new note does not match the filter and is hidden.")  );
		else if (founds == count - 1) postMessage( i18n("A new note does not match the filter and is hidden.")    );
		else if (founds > 0)          postMessage( i18n("Some new notes do not match the filter and are hidden.") );
		else                          postMessage( i18n("The new notes do not match the filter and are hidden.")  );
	}
}

void Basket::unplugNote(Note *note)
{
	// If there is nothing to do...
	if (!note)
		return;

//	if (!willBeReplugged) {
	note->setSelectedRecursivly(false); // To removeSelectedNote() and decrease the selectedsCount.
	m_count -= note->count();
	m_countFounds -= note->newFilter(decoration()->filterData());
	signalCountsChanged();
//	}

	// If it was the first note, change the first note:
	if (m_firstNote == note)
		m_firstNote = note->next();

	// Change previous and next notes:
	if (note->prev())
		note->prev()->setNext(note->next());
	if (note->next())
		note->next()->setPrev(note->prev());

	if (note->parentNote()) {
		// If it was the first note of a group, change the first note of the group:
		if (note->parentNote()->firstChild() == note)
			note->parentNote()->setFirstChild( note->next() );

		if (!note->parentNote()->isColumn()) {
			// Ungroup if still 0 note inside parent group:
			if ( ! note->parentNote()->firstChild() )
				unplugNote(note->parentNote()); // TODO delete

			// Ungroup if still 1 note inside parent group:
			else if ( ! note->parentNote()->firstChild()->next() )
				ungroupNote(note->parentNote());
		}
	}

	note->setParentNote(0);
	note->setPrev(0);
	note->setNext(0);

//	recomputeBlankRects(); // FIXME: called too much time. It's here because when dragging and moving a note to another basket and then go back to the original basket, the note is deleted but the note rect is not painter anymore.
}

void Basket::ungroupNote(Note *group)
{
	Note *note            = group->firstChild();
	Note *lastGroupedNote = group;
	Note *nextNote;

	// Move all notes after the group (not before, to avoid to change m_firstNote or group->m_firstChild):
	while (note) {
		nextNote = note->next();

		if (lastGroupedNote->next())
			lastGroupedNote->next()->setPrev(note);
		note->setNext(lastGroupedNote->next());
		lastGroupedNote->setNext(note);
		note->setParentNote(group->parentNote());
		note->setPrev(lastGroupedNote);

		note->setGroupWidth(group->groupWidth() - Note::GROUP_WIDTH);
		lastGroupedNote = note;
		note = nextNote;
	}

	// Unplug the group:
	group->setFirstChild(0);
	unplugNote(group); // TODO: delete

	relayoutNotes(true);
}

void Basket::groupNoteBefore(Note *note, Note *with)
{
	if (!note || !with)
		// No note to group or nowhere to group it:
		return;

//	if (m_loaded && before && !with->isFree() && !with->isColumn())
	for (Note *n = note; n; n = n->next())
		n->inheritTagsOf(with);

	preparePlug(note);

	Note *last = note->lastSibling();

	Note *group = new Note(this);
	group->setPrev(with->prev());
	group->setNext(with->next());
	group->setX(with->x());
	group->setY(with->y());
	if (with->parentNote() && with->parentNote()->firstChild() == with)
		with->parentNote()->setFirstChild(group);
	else if (m_firstNote == with)
		m_firstNote = group;
	group->setParentNote(with->parentNote());
	group->setFirstChild(note);
	group->setGroupWidth(with->groupWidth() + Note::GROUP_WIDTH);

	if (with->prev())
		with->prev()->setNext(group);
	if (with->next())
		with->next()->setPrev(group);
	with->setParentNote(group);
	with->setPrev(last);
	with->setNext(0L);

	for (Note *n = note; n; n = n->next())
		n->setParentNote(group);
//	note->setPrev(0L);
	last->setNext(with);

	if (m_loaded)
		signalCountsChanged();
}

void Basket::groupNoteAfter(Note *note, Note *with)
{
	if (!note || !with)
		// No note to group or nowhere to group it:
		return;

//	if (m_loaded && before && !with->isFree() && !with->isColumn())
	for (Note *n = note; n; n = n->next())
		n->inheritTagsOf(with);

	preparePlug(note);

//	Note *last = note->lastSibling();

	Note *group = new Note(this);
	group->setPrev(with->prev());
	group->setNext(with->next());
	group->setX(with->x());
	group->setY(with->y());
	if (with->parentNote() && with->parentNote()->firstChild() == with)
		with->parentNote()->setFirstChild(group);
	else if (m_firstNote == with)
		m_firstNote = group;
	group->setParentNote(with->parentNote());
	group->setFirstChild(with);
	group->setGroupWidth(with->groupWidth() + Note::GROUP_WIDTH);

	if (with->prev())
		with->prev()->setNext(group);
	if (with->next())
		with->next()->setPrev(group);
	with->setParentNote(group);
	with->setPrev(0L);
	with->setNext(note);

	for (Note *n = note; n; n = n->next())
		n->setParentNote(group);
	note->setPrev(with);
//	last->setNext(0L);

	if (m_loaded)
		signalCountsChanged();
}

void Basket::loadNotes(const QDomElement &notes, Note *parent)
{
	Note *note;
	for (QDomNode n = notes.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement e = n.toElement();
		if (e.isNull()) // Cannot handle that!
			continue;
		note = 0;
		// Load a Group:
		if (e.tagName() == "group") {
			note = new Note(this);      // 1. Create the group...
			loadNotes(e, note);         // 3. ... And populate it with child notes.
			int noteCount = note->count();
			if (noteCount > 0 || (parent == 0 && !isFreeLayout())) { // But don't remove columns!
				appendNoteIn(note, parent); // 2. ... Insert it... FIXME: Initially, the if() the insrtion was the step 2. Was it on purpose?
				// The notes in the group are counted two times (it's why appendNoteIn() was called before loadNotes):
				m_count       -= noteCount;// TODO: Recompute note count every time noteCount() is emitted!
				m_countFounds -= noteCount;
			}
		}
		// Load a Content-Based Note:
		if (e.tagName() == "note" || e.tagName() == "item") { // Keep compatible with 0.6.0 Alpha 1
			note = new Note(this);      // Create the note...
			NoteFactory__loadNode(XMLWork::getElement(e, "content"), e.attribute("type"), note); // ... Populate it with content...
			appendNoteIn(note, parent); // ... And insert it.
			// Load dates:
			if (e.hasAttribute("added"))
				note->setAddedDate(           QDateTime::fromString(e.attribute("added"),            Qt::ISODate));
			if (e.hasAttribute("lastModification"))
				note->setLastModificationDate(QDateTime::fromString(e.attribute("lastModification"), Qt::ISODate));
		}
		// If we successfully loaded a note:
		if (note) {
			// Free Note Properties:
			if (note->isFree()) {
				int x = e.attribute("x").toInt();
				int y = e.attribute("y").toInt();
				note->setX(x < 0 ? 0 : x);
				note->setY(y < 0 ? 0 : y);
			}
			// Resizeable Note Properties:
			if (note->hasResizer() || note->isColumn())
				note->setGroupWidth(e.attribute("width", "200").toInt());
			// Group Properties:
			if (note->isGroup() && !note->isColumn() && XMLWork::trueOrFalse(e.attribute("folded", "false")))
				note->toggleFolded(false);
			// Tags:
			if (note->content()) {
				QString tagsString = XMLWork::getElementText(e, "tags", "");
				QStringList tagsId = QStringList::split(";", tagsString);
				for (QStringList::iterator it = tagsId.begin(); it != tagsId.end(); ++it) {
					State *state = Tag::stateForId(*it);
					if (state)
						note->addState(state, /*orReplace=*/true);
				}
			}
		}
	}
}

void Basket::saveNotes(QDomDocument &document, QDomElement &element, Note *parent)
{
	Note *note = (parent ? parent->firstChild() : firstNote());
	while (note) {
		// Create Element:
		QDomElement noteElement = document.createElement(note->isGroup() ? "group" : "note");
		element.appendChild(noteElement);
		// Free Note Properties:
		if (note->isFree()) {
			noteElement.setAttribute("x", note->finalX());
			noteElement.setAttribute("y", note->finalY());
		}
		// Resizeable Note Properties:
		if (note->hasResizer())
			noteElement.setAttribute("width", note->groupWidth());
		// Group Properties:
		if (note->isGroup() && !note->isColumn())
			noteElement.setAttribute("folded", XMLWork::trueOrFalse(note->isFolded()));
		// Save Content:
		if (note->content()) {
			// Save Dates:
			noteElement.setAttribute("added",            note->addedDate().toString(Qt::ISODate)           );
			noteElement.setAttribute("lastModification", note->lastModificationDate().toString(Qt::ISODate));
			// Save Content:
			noteElement.setAttribute("type", note->content()->lowerTypeName());
			QDomElement content = document.createElement("content");
			noteElement.appendChild(content);
			note->content()->saveToNode(document, content);
			// Save Tags:
			if (note->states().count() > 0) {
				QString tags;
				for (State::List::iterator it = note->states().begin(); it != note->states().end(); ++it)
					tags += (tags.isEmpty() ? "" : ";") + (*it)->id();
				XMLWork::addElement(document, noteElement, "tags", tags);
			}
		} else
			// Save Child Notes:
			saveNotes(document, noteElement, note);
		// Go to the Next One:
		note = note->next();
	}
}

void Basket::loadProperties(const QDomElement &properties)
{
	// Compute Default Values for When Loading the Properties:
	QString defaultBackgroundColor = (backgroundColorSetting().isValid() ? backgroundColorSetting().name() : "");
	QString defaultTextColor       = (textColorSetting().isValid()       ? textColorSetting().name()       : "");

	// Load the Properties:
	QString icon = XMLWork::getElementText(properties, "icon", this->icon() );
	QString name = XMLWork::getElementText(properties, "name", basketName() );

	QDomElement appearance = XMLWork::getElement(properties, "appearance");
	// In 0.6.0-Alpha versions, there was a typo error: "backround" instead of "background"
	QString backgroundImage       = appearance.attribute( "backgroundImage", appearance.attribute( "backroundImage", backgroundImageName()  ) );
	QString backgroundColorString = appearance.attribute( "backgroundColor", appearance.attribute( "backroundColor", defaultBackgroundColor ) );
	QString textColorString       = appearance.attribute( "textColor",      defaultTextColor       );
	QColor  backgroundColor = (backgroundColorString.isEmpty() ? QColor() : QColor(backgroundColorString));
	QColor  textColor       = (textColorString.isEmpty()       ? QColor() : QColor(textColorString)      );

	QDomElement disposition = XMLWork::getElement(properties, "disposition");
	bool free        = XMLWork::trueOrFalse( disposition.attribute( "free",        XMLWork::trueOrFalse(isFreeLayout())   ) );
	int  columnCount =                       disposition.attribute( "columnCount", QString::number(this->columnsCount())  ).toInt();
	bool mindMap     = XMLWork::trueOrFalse( disposition.attribute( "mindMap",     XMLWork::trueOrFalse(isMindMap())      ) );

	QDomElement shortcut = XMLWork::getElement(properties, "shortcut");
	QString actionStrings[] = { "show", "globalShow", "globalSwitch" };
	KShortcut combination  = KShortcut( shortcut.attribute( "combination", m_action->shortcut().toStringInternal() ) );
	QString   actionString =            shortcut.attribute( "action" );
	int action = shortcutAction();
	if (actionString == actionStrings[0]) action = 0;
	if (actionString == actionStrings[1]) action = 1;
	if (actionString == actionStrings[2]) action = 2;

	// Apply the Properties:
	setDisposition((free ? (mindMap ? 2 : 1) : 0), columnCount);
	setShortcut(combination, action);
	setAppearance(icon, name, backgroundImage, backgroundColor, textColor); // Will emit propertiesChanged(this)
}

void Basket::saveProperties(QDomDocument &document, QDomElement &properties)
{
	XMLWork::addElement( document, properties, "name", basketName() );
	XMLWork::addElement( document, properties, "icon", icon()       );

	QDomElement appearance = document.createElement("appearance");
	properties.appendChild(appearance);
	appearance.setAttribute( "backgroundImage", backgroundImageName() );
	appearance.setAttribute( "backgroundColor", backgroundColorSetting().isValid() ? backgroundColorSetting().name() : "" );
	appearance.setAttribute( "textColor",       textColorSetting().isValid()       ? textColorSetting().name()       : "" );

	QDomElement disposition = document.createElement("disposition");
	properties.appendChild(disposition);
	disposition.setAttribute( "free",        XMLWork::trueOrFalse(isFreeLayout()) );
	disposition.setAttribute( "columnCount", QString::number(columnsCount())      );
	disposition.setAttribute( "mindMap",     XMLWork::trueOrFalse(isMindMap())    );

	QDomElement shortcut = document.createElement("shortcut");
	properties.appendChild(shortcut);
	QString actionStrings[] = { "show", "globalShow", "globalSwitch" };
	shortcut.setAttribute( "combination", m_action->shortcut().toStringInternal() );
	shortcut.setAttribute( "action",      actionStrings[shortcutAction()]         );
}

void Basket::subscribeBackgroundImages()
{
	if (!m_backgroundImageName.isEmpty()) {
		Global::backgroundManager->subscribe(m_backgroundImageName);
		Global::backgroundManager->subscribe(m_backgroundImageName, this->backgroundColor());
		Global::backgroundManager->subscribe(m_backgroundImageName, selectionRectInsideColor());
		m_backgroundPixmap         = Global::backgroundManager->pixmap(m_backgroundImageName);
		m_opaqueBackgroundPixmap   = Global::backgroundManager->opaquePixmap(m_backgroundImageName, this->backgroundColor());
		m_selectedBackgroundPixmap = Global::backgroundManager->opaquePixmap(m_backgroundImageName, selectionRectInsideColor());
		m_backgroundTiled          = Global::backgroundManager->tiled(m_backgroundImageName);
	}
}

void Basket::unsubscribeBackgroundImages()
{
	if (hasBackgroundImage()) {
		Global::backgroundManager->unsubscribe(m_backgroundImageName);
		Global::backgroundManager->unsubscribe(m_backgroundImageName, this->backgroundColor());
		Global::backgroundManager->unsubscribe(m_backgroundImageName, selectionRectInsideColor());
		m_backgroundPixmap         = 0;
		m_opaqueBackgroundPixmap   = 0;
		m_selectedBackgroundPixmap = 0;
	}
}

void Basket::setAppearance(const QString &icon, const QString &name, const QString &backgroundImage, const QColor &backgroundColor, const QColor &textColor)
{
	unsubscribeBackgroundImages();

	m_icon                   = icon;
	m_basketName             = name;
	m_backgroundImageName    = backgroundImage;
	m_backgroundColorSetting = backgroundColor;
	m_textColorSetting       = textColor;

	// Basket should ALWAYS have an icon (the "basket" icon by default):
	QPixmap iconTest = kapp->iconLoader()->loadIcon(m_icon, KIcon::NoGroup, 16, KIcon::DefaultState, 0L, /*canReturnNull=*/true);
	if (iconTest.isNull())
		m_icon = "basket";

	// We don't request the background images if it's not loaded yet (to make the application startup fast).
	// When the basket is loading (because requested by the user: he/she want to access it)
	// it load the properties, subscribe to (and then load) the images, update the "Loading..." message with the image,
	// load all the notes and it's done!
	if (m_loadingLaunched)
		subscribeBackgroundImages();

	recomputeAllStyles(); // If a note have a tag with the same background color as the basket one, then display a "..."
	recomputeBlankRects(); // See the drawing of blank areas in Basket::drawContents()
	unbufferizeAll();
	updateContents();

	if (isDuringEdit() && m_editor->widget()) {
		m_editor->widget()->setPaletteBackgroundColor( m_editor->note()->backgroundColor() );
		m_editor->widget()->setPaletteForegroundColor( m_editor->note()->textColor()       );
	}

	emit propertiesChanged(this);
}

void Basket::setDisposition(int disposition, int columnCount)
{
	static const int COLUMNS_LAYOUT  = 0;
	static const int FREE_LAYOUT     = 1;
	static const int MINDMAPS_LAYOUT = 2;

	int currentDisposition = (isFreeLayout() ? (isMindMap() ? MINDMAPS_LAYOUT : FREE_LAYOUT) : COLUMNS_LAYOUT);

	if (currentDisposition == COLUMNS_LAYOUT && disposition == COLUMNS_LAYOUT) {
		if (firstNote() && columnCount > m_columnsCount) {
			// Insert each new columns:
			for (int i = m_columnsCount; i < columnCount; ++i) {
				Note *newColumn = new Note(this);
				insertNote(newColumn, /*clicked=*/lastNote(), /*zone=*/Note::BottomInsert, QPoint(), /*animateNewPosition=*/false);
			}
		} else if (firstNote() && columnCount < m_columnsCount) {
			Note *column = firstNote();
			Note *cuttedNotes = 0;
			for (int i = 1; i <= m_columnsCount; ++i) {
				Note *columnToRemove = column;
				column = column->next();
				if (i > columnCount) {
					// Remove the columns that are too much:
					unplugNote(columnToRemove);
					// "Cut" the content in the columns to be deleted:
					if (columnToRemove->firstChild()) {
						for (Note *it = columnToRemove->firstChild(); it; it = it->next())
							it->setParentNote(0);
						if (!cuttedNotes)
							cuttedNotes = columnToRemove->firstChild();
						else {
							Note *lastCuttedNote = cuttedNotes;
							while (lastCuttedNote->next())
								lastCuttedNote = lastCuttedNote->next();
							lastCuttedNote->setNext(columnToRemove->firstChild());
							columnToRemove->firstChild()->setPrev(lastCuttedNote);
						}
						columnToRemove->setFirstChild(0);
					}
				}
			}
			// Paste the content in the last column:
			if (cuttedNotes)
				insertNote(cuttedNotes, /*clicked=*/lastNote(), /*zone=*/Note::BottomColumn, QPoint(), /*animateNewPosition=*/true);
			unselectAll();
		}
		if (columnCount != m_columnsCount) {
			m_columnsCount = (columnCount <= 0 ? 1 : columnCount);
			equalizeColumnSizes(); // Will relayoutNotes()
		}
	} else if (currentDisposition == COLUMNS_LAYOUT && (disposition == FREE_LAYOUT || disposition == MINDMAPS_LAYOUT)) {
		Note *column = firstNote();
		m_columnsCount = 0; // Now, so relayoutNotes() will not relayout the free notes as if they were columns!
		while (column) {
			// Move all childs on the first level:
			Note *nextColumn = column->next();
			ungroupNote(column);
			column = nextColumn;
		}
		unselectAll();
		m_mindMap = (disposition == MINDMAPS_LAYOUT);
		relayoutNotes(true);
	} else if ((currentDisposition == FREE_LAYOUT || currentDisposition == MINDMAPS_LAYOUT) && disposition == COLUMNS_LAYOUT) {
		if (firstNote()) {
			// TODO: Reorder notes!
			// Remove all notes (but keep a reference to them, we're not crazy ;-) ):
			Note *notes = m_firstNote;
			m_firstNote = 0;
			m_count = 0;
			m_countFounds = 0;
			// Insert the number of columns that is needed:
			Note *lastInsertedColumn = 0;
			for (int i = 0; i < columnCount; ++i) {
				Note *column = new Note(this);
				if (lastInsertedColumn)
					insertNote(column, /*clicked=*/lastInsertedColumn, /*zone=*/Note::BottomInsert, QPoint(), /*animateNewPosition=*/false);
				else
					m_firstNote = column;
				lastInsertedColumn = column;
			}
			// Reinsert the old notes in the first column:
			insertNote(notes, /*clicked=*/firstNote(), /*zone=*/Note::BottomColumn, QPoint(), /*animateNewPosition=*/true);
			unselectAll();
		} else {
			// Insert the number of columns that is needed:
			Note *lastInsertedColumn = 0;
			for (int i = 0; i < columnCount; ++i) {
				Note *column = new Note(this);
				if (lastInsertedColumn)
					insertNote(column, /*clicked=*/lastInsertedColumn, /*zone=*/Note::BottomInsert, QPoint(), /*animateNewPosition=*/false);
				else
					m_firstNote = column;
				lastInsertedColumn = column;
			}
		}
		m_columnsCount = (columnCount <= 0 ? 1 : columnCount);
		equalizeColumnSizes(); // Will relayoutNotes()
	}
}

void Basket::equalizeColumnSizes()
{
	if (!firstNote())
		return;

	// Necessary to know the available space;
	relayoutNotes(true);

	int availableSpace = visibleWidth();
	int columnWidth = (visibleWidth() - (columnsCount()-1)*Note::GROUP_WIDTH) / columnsCount();
	int columnCount = columnsCount();
	Note *column = firstNote();
	while (column) {
		int minGroupWidth = column->minRight() - column->x();
		if (minGroupWidth > columnWidth) {
			availableSpace -= minGroupWidth;
			--columnCount;
		}
		column = column->next();
	}
	columnWidth = (availableSpace - (columnsCount()-1)*Note::GROUP_WIDTH) / columnCount;

	column = firstNote();
	while (column) {
		int minGroupWidth = column->minRight() - column->x();
		if (minGroupWidth > columnWidth)
			column->setGroupWidth(minGroupWidth);
		else
			column->setGroupWidth(columnWidth);
		column = column->next();
	}

	relayoutNotes(true);
}

void Basket::save()
{
	if (!m_loaded)
		return;

	DEBUG_WIN << "Basket[" + folderName() + "]: Saving...";

	// Create Document:
	QDomDocument document(/*doctype=*/"basket");
	QDomElement root = document.createElement("basket");
	document.appendChild(root);

	// Create Properties Element and Populate It:
	QDomElement properties = document.createElement("properties");
	saveProperties(document, properties);
	root.appendChild(properties);

	// Create Notes Element and Populate It:
	QDomElement notes = document.createElement("notes");
	saveNotes(document, notes, 0);
	root.appendChild(notes);

	// Write to Disk:
	QFile file(fullPath() + "/.basket");
	if (file.open(IO_WriteOnly)) {
		QTextStream stream(&file);
		stream.setEncoding(QTextStream::UnicodeUTF8);
		QString xml = document.toString();
		stream << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";
		stream << xml;
		file.close();
	} else
		DEBUG_WIN << "Basket[" + folderName() + "]: <font color=red>FAILED</font>!";
}

void Basket::load()
{
	// Load only once:
	if (m_loadingLaunched)
		return;
	m_loadingLaunched = true;

	DEBUG_WIN << "Basket[" + folderName() + "]: Loading...";

	QDomDocument *doc = XMLWork::openFile("basket", fullPath() + "/.basket");
	if (doc == 0) {
		DEBUG_WIN << "Basket[" + folderName() + "]: <font color=red>FAILED</font>!";
		return;
	}

	QDomElement docElem = doc->documentElement();
	QDomElement properties = XMLWork::getElement(docElem, "properties");

	loadProperties(properties); // Since we are loading, this time the background image will also be loaded!
	// Now that the background image is loaded and subscribed, we display it during the load process:
	updateContents();
	kapp->processEvents();

	//BEGIN Compatibility with 0.6.0 Pre-Alpha versions:
	QDomElement notes = XMLWork::getElement(docElem, "notes");
	if (notes.isNull())
		notes = XMLWork::getElement(docElem, "items");
	loadNotes(notes, 0L);
	//loadNotes(XMLWork::getElement(docElem, "notes"), 0L);
	//END
	signalCountsChanged();
	if (isColumnsLayout()) {
		// Count the number of columns:
		int columnsCount = 0;
		Note *column = firstNote();
		while (column) {
			++columnsCount;
			column = column->next();
		}
		m_columnsCount = columnsCount;
	}

	relayoutNotes(false);

	// On application start, the current basket is not focused yet, so the focus rectangle is not shown when calling focusANote():
	if (Global::basketTree->currentBasket() == this)
		setFocus();
	focusANote();

	if (Settings::playAnimations() && !decoration()->filterBar()->filterData().isFiltering) // No animation when filtering all!
		animateLoad();//QTimer::singleShot( 0, this, SLOT(animateLoad()) );
	else
		m_loaded = true;
}

void Basket::filterAgain()
{
	newFilter(decoration()->filterData());
}

void Basket::filterAgainDelayed()
{
	QTimer::singleShot( 0, this, SLOT(filterAgain()) );
}

void Basket::newFilter(const FilterData &data)
{
	m_countFounds = 0;
	for (Note *note = firstNote(); note; note = note->next())
		m_countFounds += note->newFilter(data);

	relayoutNotes(true);
	signalCountsChanged();

	if (hasFocus())   // if (!hasFocus()), focusANote() will be called at focusInEvent()
		focusANote(); //  so, we avoid de-focus a note if it will be re-shown soon
	if (m_focusedNote != 0L)
		ensureNoteVisible(m_focusedNote);

	Global::mainContainer->setFiltering(data.isFiltering);
}

void Basket::cancelFilter()
{
	decoration()->filterBar()->reset();
	validateFilter();
}

void Basket::validateFilter()
{
	if (isDuringEdit())
		m_editor->widget()->setFocus();
	else
		setFocus();
}

bool Basket::isFiltering()
{
	return decoration()->filterBar()->filterData().isFiltering;
}



QString Basket::fullPath()
{
	return Global::basketsFolder() + folderName();
}

QString Basket::fullPathForFileName(const QString &fileName)
{
	return fullPath() + fileName;
}

/*static*/ QString Basket::fullPathForFolderName(const QString &folderName)
{
	return Global::basketsFolder() + folderName;
}


void Basket::setShortcut(KShortcut shortcut, int action)
{
	QString sAction = "global_basket_activate_" + folderName();
	Global::globalAccel->remove(sAction);
	Global::globalAccel->updateConnections();

	m_action->setShortcut(shortcut);
	m_shortcutAction = action;

	if (action > 0)
		Global::globalAccel->insert(sAction, m_action->text(), /*whatsThis=*/"", m_action->shortcut(), KShortcut(), this, SLOT(activatedShortcut()), /*configurable=*/false);
	Global::globalAccel->updateConnections();
}

void Basket::activatedShortcut()
{
	Global::mainContainer->setCurrentBasket(this);

	if (m_shortcutAction == 1)
		Global::mainContainer->setActive(true);
}

void Basket::signalCountsChanged()
{
	if (!m_timerCountsChanged.isActive())
		m_timerCountsChanged.start(0/*ms*/, /*singleShot=*/true);
}

void Basket::countsChangedTimeOut()
{
	emit countsChanged(this);
}


Basket::Basket(QWidget *parent, const QString &folderName)
 : QScrollView(parent),
   QToolTip(viewport()),
   m_noActionOnMouseRelease(false), m_pressPos(-100, -100), m_canDrag(false),
   m_firstNote(0), m_columnsCount(1), m_mindMap(false), m_resizingNote(0L), m_pickedResizer(0), m_movingNote(0L), m_pickedHandle(0, 0),
   m_clickedToInsert(0), m_zoneToInsert(0), m_posToInsert(-1, -1),
   m_isInsertPopupMenu(false),
   m_loaded(false), m_loadingLaunched(false),
   m_backgroundPixmap(0), m_opaqueBackgroundPixmap(0), m_selectedBackgroundPixmap(0),
   m_action(0), m_shortcutAction(0),
   m_hoveredNote(0), m_hoveredZone(Note::None), m_lockedHovering(false), m_underMouse(false),
   m_inserterRect(), m_inserterShown(false), m_inserterSplit(true), m_inserterTop(false), m_inserterGroup(false),
   m_isSelecting(false), m_selectionStarted(false),
   m_count(0), m_countFounds(0), m_countSelecteds(0),
   m_folderName(folderName),
   m_editor(0), m_leftEditorBorder(0), m_rightEditorBorder(0), m_redirectEditActions(false), m_editorWidth(-1), m_editorHeight(-1),
   m_isDuringDrag(false), m_draggedNotes(),
   m_focusedNote(0), m_startOfShiftSelectionNote(0)
{
	QString sAction = "local_basket_activate_" + folderName;
	m_action = new KAction("FAKE TEXT", "FAKE ICON", KShortcut(), this, SLOT(activatedShortcut()), Global::mainContainer->actionCollection(), sAction);
	m_action->setShortcutConfigurable(false); // We do it in the basket properties dialog (and keep it in sync with the global one)

	if (!m_folderName.endsWith("/"))
		m_folderName += "/";

	setFocusPolicy(QWidget::StrongFocus);
	setWFlags(Qt::WNoAutoErase);
	setDragAutoScroll(true);

	// By default, there is no corner widget: we set one for the corner area to be painted!
	// If we don't set one and there are two scrollbars present, slowly resizing up the window show graphical glitches in that area!
	m_cornerWidget = new QWidget(this);
	setCornerWidget(m_cornerWidget);

	viewport()->setAcceptDrops(true);
	viewport()->setMouseTracking(true);
	viewport()->setBackgroundMode(NoBackground); // Do not clear the widget before paintEvent() because we always draw every pixels (faster and flicker-free)

	connect( &m_animationTimer,           SIGNAL(timeout()),   this, SLOT(animateObjects())           );
	connect( &m_autoScrollSelectionTimer, SIGNAL(timeout()),   this, SLOT(doAutoScrollSelection())    );
	connect( &m_timerCountsChanged,       SIGNAL(timeout()),   this, SLOT(countsChangedTimeOut())     );
	connect( &m_inactivityAutoSaveTimer,  SIGNAL(timeout()),   this, SLOT(inactivityAutoSaveTimout()) );
	connect( this, SIGNAL(contentsMoving(int, int)), this, SLOT(contentsMoved()) );

//	m_transparentWidget = new TransparentWidget(this);
//	m_transparentWidget->setPosition(100, 100);
//	addChild(m_transparentWidget, 100, 100);
}

void Basket::contentsMoved()
{
	// This slot is called BEFORE the content move, so we delay the hover effects:
	QTimer::singleShot(0, this, SLOT(doHoverEffects()));
}

void Basket::enterEvent(QEvent *)
{
	m_underMouse = true;
	doHoverEffects();
}

void Basket::leaveEvent(QEvent *)
{
	m_underMouse = false;
	doHoverEffects();

	if (m_lockedHovering)
		return;

	removeInserter();
	if (m_hoveredNote) {
		m_hoveredNote->setHovered(false);
		m_hoveredNote->setHoveredZone(Note::None);
		updateNote(m_hoveredNote);
	}
	m_hoveredNote = 0;
}

void Basket::setFocusIfNotInPopupMenu()
{
	if (!kapp->activePopupWidget())
		setFocus();
}

void Basket::contentsMousePressEvent(QMouseEvent *event)
{
	// If user click the basket, focus it!
	// The focus is delayed because if the click results in showing a popup menu,
	// the interface flicker by showing the focused rectangle (as the basket gets focus)
	// and immediatly removing it (because the popup menu now have focus).
	if (!isDuringEdit())
		QTimer::singleShot(0, this, SLOT(setFocusIfNotInPopupMenu()));

	// Convenient variables:
	bool controlPressed = event->stateAfter() & Qt::ControlButton;
	bool shiftPressed   = event->stateAfter() & Qt::ShiftButton;

	// Do nothing if we disabled the click some milliseconds sooner.
	// For instance when a popup menu has been closed with click, we should not do action:
	if (event->button() == Qt::LeftButton && (kapp->activePopupWidget() || m_lastDisableClick.msecsTo(QTime::currentTime()) <= 80)) {
		doHoverEffects();
		m_noActionOnMouseRelease = true;
		// But we allow to select:
		// The code is the same as at the bottom of this method:
		if (event->button() == Qt::LeftButton) {
			m_selectionStarted = true;
			m_selectionBeginPoint = event->pos();
			m_selectionInvert = controlPressed || shiftPressed;
		}
		return;
	}

	// Figure out what is the clicked note and zone:
	Note *clicked = noteAt(event->pos().x(), event->pos().y());
	Note::Zone zone = (clicked ? clicked->zoneAt( event->pos() - QPoint(clicked->x(), clicked->y()) ) : Note::None);

	// Popup Tags menu:
	if (zone == Note::TagsArrow && !controlPressed && !shiftPressed && event->button() != Qt::MidButton) {
		if (!clicked->isSelected())
			unselectAllBut(clicked);
		setFocusedNote(clicked); /// /// ///
		m_startOfShiftSelectionNote = clicked;
		m_noActionOnMouseRelease = true;
		popupTagsMenu(clicked);
		return;
	}

	if (event->button() == Qt::LeftButton) {
		// Prepare to allow drag and drop when moving mouse further:
		if ( (zone == Note::Handle || zone == Note::Group) ||
		     (clicked && clicked->isSelected() &&
		       (zone == Note::TagsArrow || zone == Note::Custom0 || zone == Note::Content || zone == Note::Link /**/ || zone >= Note::Emblem0 /**/)) ) {
			m_pressPos = event->pos(); // TODO: Allow to drag emblems to assign them to other notes. Then don't allow drag at Emblem0!!
			m_canDrag  = true;
		}

		// Initializing Resizer move:
		if (zone == Note::Resizer) {
			m_resizingNote  = clicked;
			m_pickedResizer = event->pos().x() - clicked->rightLimit();
			m_noActionOnMouseRelease = true;
			m_lockedHovering = true;
			return;
		}

		// Select note(s):
		if (zone == Note::Handle || zone == Note::Group || (zone == Note::GroupExpander && (controlPressed || shiftPressed))) {
			Note *end = clicked;
			if (clicked->isGroup() && shiftPressed) {
				if (clicked->contains(m_startOfShiftSelectionNote)) {
					m_startOfShiftSelectionNote = clicked->firstRealChild();
					end = clicked->lastRealChild();
				} else if (clicked->firstRealChild()->isAfter(m_startOfShiftSelectionNote))
					end = clicked->lastRealChild();
				else
					end = clicked->firstRealChild();
			}
			if (controlPressed && shiftPressed)
				selectRange(m_startOfShiftSelectionNote, end, /*unselectOthers=*/false);
			else if (shiftPressed)
				selectRange(m_startOfShiftSelectionNote, end);
			else if (controlPressed)
				clicked->setSelectedRecursivly(!clicked->allSelected());
			else if (!clicked->allSelected())
				unselectAllBut(clicked);
			setFocusedNote(end); /// /// ///
			m_startOfShiftSelectionNote = (end->isGroup() ? end->firstRealChild() : end);
			m_noActionOnMouseRelease = false;
			return;
		} else if (clicked && zone != Note::None && zone != Note::BottomColumn && zone != Note::Resizer && (controlPressed || shiftPressed)) {
			if (controlPressed && shiftPressed)
				selectRange(m_startOfShiftSelectionNote, clicked, /*unselectOthers=*/false);
			else if (shiftPressed)
				selectRange(m_startOfShiftSelectionNote, clicked);
			else if (controlPressed)
				clicked->setSelectedRecursivly(!clicked->allSelected());
			setFocusedNote(clicked); /// /// ///
			m_startOfShiftSelectionNote = (clicked->isGroup() ? clicked->firstRealChild() : clicked);
			m_noActionOnMouseRelease = true;
			return;
		}

		// Initializing Note move:
/*		if ((zone == Note::Group || zone == Note::Handle) && clicked->isFree()) {
			m_movingNote   = clicked;
			m_pickedHandle = QPoint(event->pos().x() - clicked->x(), event->pos().y() - clicked->y());
			m_noActionOnMouseRelease = true;
			m_lockedHovering = true;
			return;
		}
*/

		// Folding/Unfolding group:
		if (zone == Note::GroupExpander) {
			clicked->toggleFolded(Settings::playAnimations());
			relayoutNotes(true);
			m_noActionOnMouseRelease = true;
			return;
		}
	}

	// Popup menu for tag emblems:
	if (event->button() == Qt::RightButton && zone >= Note::Emblem0) {
		if (!clicked->isSelected())
			unselectAllBut(clicked);
		setFocusedNote(clicked); /// /// ///
		m_startOfShiftSelectionNote = clicked;
		popupEmblemMenu(clicked, zone - Note::Emblem0);
		m_noActionOnMouseRelease = true;
		return;
	}

	// Insertion Popup Menu:
	if ( (event->button() == Qt::RightButton) &&
	     ((!clicked && isFreeLayout()) ||
	      (clicked && (zone == Note::TopInsert || zone == Note::TopGroup || zone == Note::BottomInsert || zone == Note::BottomGroup || zone == Note::BottomColumn))) ) {
		unselectAll();
		m_clickedToInsert = clicked;
		m_zoneToInsert    = zone;
		m_posToInsert     = event->pos();
		KPopupMenu* menu = (KPopupMenu*)(Global::mainContainer->popupMenu("insert_popup"));
		if (!menu->title(/*id=*/120).isEmpty()) // If we already added a title, remove it because it would be kept and then added several times:
			menu->removeItem(/*id=*/120);
		menu->insertTitle((zone == Note::TopGroup || zone == Note::BottomGroup ? i18n("The verb (Group New Note)", "Group") : i18n("The verb (Insert New Note)", "Insert")), /*id=*/120, /*index=*/0);
		m_isInsertPopupMenu = true;
		connect( menu, SIGNAL(aboutToHide()),  this, SLOT(delayedCancelInsertPopupMenu()) );
		connect( menu, SIGNAL(aboutToHide()),  this, SLOT(unlockHovering())               );
		connect( menu, SIGNAL(aboutToHide()),  this, SLOT(disableNextClick())             );
		connect( menu, SIGNAL(aboutToHide()),  this, SLOT(hideInsertPopupMenu())          );
		doHoverEffects(clicked, zone); // In the case where another popup menu was open, we should do that manually!
		m_lockedHovering = true;
		menu->exec(QCursor::pos());
		m_noActionOnMouseRelease = true;
		return;
	}

	// Note Context Menu:
	if (event->button() == Qt::RightButton && clicked && !clicked->isColumn() && zone != Note::Resizer) {
		if (!clicked->isSelected())
			unselectAllBut(clicked);
		setFocusedNote(clicked); /// /// ///
		if (editedNote() == clicked) {
			closeEditor();
			clicked->setSelected(true);
		}
		m_startOfShiftSelectionNote = (clicked->isGroup() ? clicked->firstRealChild() : clicked);
		QPopupMenu* menu = Global::mainContainer->popupMenu("note_popup");
		connect( menu, SIGNAL(aboutToHide()),  this, SLOT(unlockHovering())   );
		connect( menu, SIGNAL(aboutToHide()),  this, SLOT(disableNextClick()) );
		doHoverEffects(clicked, zone); // In the case where another popup menu was open, we should do that manually!
		m_lockedHovering = true;
		menu->exec(QCursor::pos());
		m_noActionOnMouseRelease = true;
		return;
	}

	// Paste selection under cursor (but not "create new primary note under cursor" because this is on moveRelease):
	if (event->button() == Qt::MidButton && zone != Note::Resizer) {
		if (clicked)
			zone = clicked->zoneAt( event->pos() - QPoint(clicked->x(), clicked->y()), true );
		closeEditor();
		clickedToInsert(event, clicked, zone);
		m_noActionOnMouseRelease = true;
		save();
		return;
	}

	// Finally, no action has been done durint pressEvent, so an action can be done on releaseEvent:
	m_noActionOnMouseRelease = false;

	/* Selection scenario:
	 * On contentsMousePressEvent, put m_selectionStarted to true and init Begin and End selection point.
	 * On contentsMouseMoveEvent, if m_selectionStarted, update End selection point, update selection rect,
	 * and if it's larger, switching to m_isSelecting mode: we can draw the selection rectangle.
	 */
	// Prepare selection:
	if (event->button() == Qt::LeftButton) {
		m_selectionStarted = true;
		m_selectionBeginPoint = event->pos();
		// We usualy invert the selection with the Ctrl key, but some environements (like GNOME or The Gimp) do it with the Shift key.
		// Since the Shift key has no specific usage, we allow to invert selection ALSO with Shift for Gimp people
		m_selectionInvert = controlPressed || shiftPressed;
	}
}

void Basket::delayedCancelInsertPopupMenu()
{
	QTimer::singleShot( 0, this, SLOT(cancelInsertPopupMenu()) );
}

void Basket::cancelInsertPopupMenu()
{
	m_isInsertPopupMenu = false;
}

void Basket::contentsContextMenuEvent(QContextMenuEvent *event)
{
	if (event->reason() == QContextMenuEvent::Keyboard) {
		if (countFounds/*countShown*/() == 0) { // TODO: Count shown!!
			QRect basketRect( mapToGlobal(QPoint(0,0)), size() );
			QPopupMenu *menu = Global::mainContainer->popupMenu("insert_popup");
			m_isInsertPopupMenu = true;
			connect( menu, SIGNAL(aboutToHide()),  this, SLOT(delayedCancelInsertPopupMenu()) );
			connect( menu, SIGNAL(aboutToHide()),  this, SLOT(unlockHovering())               );
			connect( menu, SIGNAL(aboutToHide()),  this, SLOT(disableNextClick())             );
			removeInserter();
			m_lockedHovering = true;
			PopupMenu::execAtRectCenter(*menu, basketRect); // Popup at center or the basket
		} else {
			if ( ! m_focusedNote->isSelected() )
				unselectAllBut(m_focusedNote);
			setFocusedNote(m_focusedNote); /// /// ///
			m_startOfShiftSelectionNote = (m_focusedNote->isGroup() ? m_focusedNote->firstRealChild() : m_focusedNote);
			// Popup at bottom (or top) of the focused note, if visible :
			QPopupMenu *menu = Global::mainContainer->popupMenu("note_popup");
			connect( menu, SIGNAL(aboutToHide()),  this, SLOT(unlockHovering())   );
			connect( menu, SIGNAL(aboutToHide()),  this, SLOT(disableNextClick()) );
			doHoverEffects(m_focusedNote, Note::Content); // In the case where another popup menu was open, we should do that manually!
			m_lockedHovering = true;
			PopupMenu::execAtRectBottom(*menu, noteVisibleRect(m_focusedNote), true);
		}
	}
}

QRect Basket::noteVisibleRect(Note *note)
{
	QRect rect( contentsToViewport(QPoint(note->x(), note->y())), QSize(note->width(),note->height()) );
	QPoint basketPoint = mapToGlobal(QPoint(0,0));
	rect.moveTopLeft( rect.topLeft() + basketPoint + QPoint(frameWidth(), frameWidth()) );

	// Now, rect contain the global note rectangle on the screen.
	// We have to clip it by the basket widget :
	if (rect.bottom() > basketPoint.y() + visibleHeight() + 1) { // Bottom too... bottom
		rect.setBottom( basketPoint.y() + visibleHeight() + 1);
		if (rect.height() <= 0) // Have at least one visible pixel of height
			rect.setTop(rect.bottom());
	}
	if (rect.top() < basketPoint.y() + frameWidth()) { // Top too... top
		rect.setTop( basketPoint.y() + frameWidth());
		if (rect.height() <= 0)
			rect.setBottom(rect.top());
	}
	if (rect.right() > basketPoint.x() + visibleWidth() + 1) { // Right too... right
		rect.setRight( basketPoint.x() + visibleWidth() + 1);
		if (rect.width() <= 0) // Have at least one visible pixel of width
			rect.setLeft(rect.right());
	}
	if (rect.left() < basketPoint.x() + frameWidth()) { // Left too... left
		rect.setLeft( basketPoint.x() + frameWidth());
		if (rect.width() <= 0)
			rect.setRight(rect.left());
	}

	return rect;
}

void Basket::disableNextClick()
{
	m_lastDisableClick = QTime::currentTime();
}

void Basket::recomputeAllStyles()
{
	FOR_EACH_NOTE (note)
		note->recomputeAllStyles();
}

void Basket::insertNote(Note *note, Note *clicked, int zone, const QPoint &pos, bool animateNewPosition)
{
	if (!note) {
		std::cout << "Wanted to insert NO note" << std::endl;
		return;
	}

	if (clicked && zone == Note::BottomColumn) {
		// When inserting at the bottom of a column, it's obvious the new note SHOULD inherit tags.
		// We ensure that by changing the insertion point after the last note of the column:
		Note *last = clicked->lastChild();
		if (last) {
			clicked = last;
			zone = Note::BottomInsert;
		}
	}

	/// Insertion at the bottom of a column:
	if (clicked && zone == Note::BottomColumn) {
		note->setWidth(clicked->rightLimit() - clicked->x());
		Note *lastChild = clicked->lastChild();
		if (!animateNewPosition || !Settings::playAnimations())
			for (Note *n = note; n; n = n->next()) {
				n->setXRecursivly(clicked->x());
				n->setYRecursivly((lastChild ? lastChild : clicked)->bottom() + 1);
			}
		appendNoteIn(note, clicked);

	/// Insertion relative to a note (top/bottom, insert/group):
	} else if (clicked) {
		note->setWidth(clicked->width());
		if (!animateNewPosition || !Settings::playAnimations())
			for (Note *n = note; n; n = n->next()) {
				if (zone == Note::TopGroup || zone == Note::BottomGroup)
					n->setXRecursivly(clicked->x() + Note::GROUP_WIDTH);
				else
					n->setXRecursivly(clicked->x());
				if (zone == Note::TopInsert || zone == Note::TopGroup)
					n->setYRecursivly(clicked->y());
				else
					n->setYRecursivly(clicked->bottom() + 1);
			}

		if      (zone == Note::TopInsert)    { appendNoteBefore(note, clicked); }
		else if (zone == Note::BottomInsert) { appendNoteAfter(note,  clicked); }
		else if (zone == Note::TopGroup)     { groupNoteBefore(note,  clicked); }
		else if (zone == Note::BottomGroup)  { groupNoteAfter(note,   clicked); }

	/// Free insertion:
	} else if (isFreeLayout()) {
		// Group if note have siblings:
		if (note->next()) {
			Note *group = new Note(this);
			for (Note *n = note; n; n = n->next())
				n->setParentNote(group);
			group->setFirstChild(note);
			note = group;
		}
		// Insert at cursor position:
		const int initialWidth = 250;
		note->setWidth(note->isGroup() ? Note::GROUP_WIDTH : initialWidth);
		if (note->isGroup() && note->firstChild())
			note->setInitialHeight(note->firstChild()->height());
		//note->setGroupWidth(initialWidth);
		if (animateNewPosition && Settings::playAnimations())
			note->setFinalPosition(pos.x(), pos.y());
		else {
			note->setXRecursivly(pos.x());
			note->setYRecursivly(pos.y());
		}
		appendNoteAfter(note, lastNote());
	}

	relayoutNotes(true);
}

void Basket::clickedToInsert(QMouseEvent *event, Note *clicked, /*Note::Zone*/int zone)
{
	Note *note;
	if (event->button() == Qt::MidButton)
		note = NoteFactory::dropNote(KApplication::clipboard()->data(QClipboard::Selection), this);
	else
		note = NoteFactory::createNoteText("", this);

	if (!note)
		return;

	insertNote(note, clicked, zone, event->pos(), /*animateNewPosition=*/false);

//	ensureNoteVisible(lastInsertedNote()); // TODO: in insertNote()

	if (event->button() != Qt::MidButton) {
		removeInserter(); // Case: user clicked below a column to insert, the note is inserted and doHoverEffects() put a new inserter below. We don't want it.
		closeEditor();
		noteEdit(note, /*justAdded=*/true);
	}
}

void Basket::contentsDragEnterEvent(QDragEnterEvent *event)
{
	m_isDuringDrag = true;
	Global::mainContainer->updateStatusBarHint();
	if (NoteDrag::basketOf(event) == this)
		m_draggedNotes = NoteDrag::notesOf(event);
}

void Basket::contentsDragMoveEvent(QDragMoveEvent *event)
{
//	m_isDuringDrag = true;

//	if (isLocked())
//		return;

//	FIXME: viewportToContents does NOT work !!!
//	QPoint pos = viewportToContents(event->pos());
//	QPoint pos( event->pos().x() + contentsX(), event->pos().y() + contentsY() );

//	if (insertAtCursorPos())
//		computeInsertPlace(pos);
	doHoverEffects(event->pos());

//	showFrameInsertTo();
	if (isFreeLayout() || noteAt(event->pos().x(), event->pos().y())) // Cursor before rightLimit() or hovering the dragged source notes
		acceptDropEvent(event);
	else {
		event->acceptAction(false);
		event->accept(false);
	}

/*	Note *hoveredNote = noteAt(event->pos().x(), event->pos().y());
	if ( (isColumnsLayout() && !hoveredNote) || (draggedNotes().contains(hoveredNote)) ) {
		event->acceptAction(false);
		event->accept(false);
	} else
		acceptDropEvent(event);*/

	// A workarround since QScrollView::dragAutoScroll seem to have no effect :
//	ensureVisible(event->pos().x() + contentsX(), event->pos().y() + contentsY(), 30, 30);
//	QScrollView::dragMoveEvent(event);
}

void Basket::contentsDragLeaveEvent(QDragLeaveEvent*)
{
//	resetInsertTo();
	m_isDuringDrag = false;
	m_draggedNotes.clear();
	m_noActionOnMouseRelease = true;
	emit resetStatusBarText();
	doHoverEffects();
}

/*void Basket::dropEvent(QDropEvent *event)
{
	QScrollView::dropEvent(event);
}*/

void Basket::contentsDropEvent(QDropEvent *event)
{
	m_isDuringDrag = false;
	emit resetStatusBarText();

//	if (isLocked())
//		return;

	// Do NOT check the bottom&right borders.
	// Because imagine someone drag&drop a big note from the top to the bottom of a big basket (with big vertical scrollbars),
	// the note is first removed, and relayoutNotes() compute the new height that is smaller
	// Then noteAt() is called for the mouse pointer position, because the basket is now smaller, the cursor is out of boundaries!!!
	// Should, of course, not return 0:
	Note *clicked = noteAt(event->pos().x(), event->pos().y());

	Note *note = NoteFactory::dropNote( event, this, true, event->action(), dynamic_cast<Note*>(event->source()) );

	if (note) {
		Note::Zone zone = (clicked ? clicked->zoneAt( event->pos() - QPoint(clicked->x(), clicked->y()), /*toAdd=*/true ) : Note::None);
		bool animateNewPosition = NoteFactory::movingNotesInTheSameBasket(event, this, event->action());
		if (animateNewPosition) {
			FOR_EACH_NOTE (n)
				n->setOnTop(false);
			// FOR_EACH_NOTE_IN_CHUNK(note)
			for (Note *n = note; n; n = n->next())
				n->setOnTop(true);
		}

		insertNote(note, clicked, zone, event->pos(), animateNewPosition);

		// If moved a note on bottom, contentsHeight has been disminished, then view scrolled up, and we should re-scroll the view down:
		ensureNoteVisible(note);

//		if (event->button() != Qt::MidButton) {
//			removeInserter(); // Case: user clicked below a column to insert, the note is inserted and doHoverEffects() put a new inserter below. We don't want it.
//		}

//		resetInsertTo();
//		doHoverEffects(); called by insertNote()
		save();
	}

	m_draggedNotes.clear();
}

void Basket::insertEmptyNote(int type)
{
	if (isDuringEdit())
		closeEditor();
	Note *note = NoteFactory::createEmptyNote((NoteType::Id)type, this);
	insertCreatedNote(note/*, / *edit=* /true*/);
	noteEdit(note, /*justAdded=*/true);
}

void Basket::insertWizard(int type)
{
	saveInsertionData();
	Note *note = 0;
	switch (type) {
		default:
		case 1: note = NoteFactory::importKMenuLauncher(this); break;
		case 2: note = NoteFactory::importIcon(this);          break;
		case 3: note = NoteFactory::importFileContent(this);   break;
	}
	if (!note)
		return;
	restoreInsertionData();
	insertCreatedNote(note);
	unselectAllBut(note);
	resetInsertionData();
}

void Basket::insertColor(const QColor &color)
{
	Note *note = NoteFactory::createNoteColor(color, this);
	restoreInsertionData();
	insertCreatedNote(note);
	unselectAllBut(note);
	resetInsertionData();
}

void Basket::insertImage(const QPixmap &image)
{
	Note *note = NoteFactory::createNoteImage(image, this);
	restoreInsertionData();
	insertCreatedNote(note);
	unselectAllBut(note);
	resetInsertionData();
}

void Basket::pasteNote(QClipboard::Mode mode)
{
	if (!m_isInsertPopupMenu && redirectEditActions()) {
		if (m_editor->textEdit())
			m_editor->textEdit()->paste();
		else if (m_editor->lineEdit())
			m_editor->lineEdit()->paste();
	} else {
		Note *note = NoteFactory::dropNote(KApplication::clipboard()->data(mode), this);
		if (note) {
			insertCreatedNote(note);
			unselectAllBut(note);
		}
	}
}

void Basket::insertCreatedNote(Note *note)
{
	// Get the insertion data if the user clicked inside the basket:
	Note *clicked = m_clickedToInsert;
	int zone      = m_zoneToInsert;
	QPoint pos    = m_posToInsert;

	// If it isn't the case, use the default position:
	if (!clicked && (pos.x() < 0 || pos.y() < 0)) {
		// Insert right after the focused note:
		focusANote();
		if (m_focusedNote) {
			clicked = m_focusedNote;
			zone    = (m_focusedNote->isFree() ? Note::BottomGroup : Note::BottomInsert);
			pos     = QPoint(m_focusedNote->x(), m_focusedNote->finalBottom());
		// Insert at the end of the last column:
		} else if (isColumnsLayout()) {
			Note *column = /*(Settings::newNotesPlace == 0 ?*/ firstNote() /*: lastNote())*/;
			/*if (Settings::newNotesPlace == 0 && column->firstChild()) { // On Top, if at least one child in the column
				clicked = column->firstChild();
				zone    = Note::TopInsert;
			} else { // On Bottom*/
				clicked = column;
				zone    = Note::BottomColumn;
			/*}*/
		// Insert at free position:
		} else {
			pos = QPoint(0, 0);
		}
	}

	insertNote(note, clicked, zone, pos);
//	ensureNoteVisible(lastInsertedNote());
	removeInserter(); // Case: user clicked below a column to insert, the note is inserted and doHoverEffects() put a new inserter below. We don't want it.
//	resetInsertTo();
	save();
}

void Basket::saveInsertionData()
{
	m_savedClickedToInsert = m_clickedToInsert;
	m_savedZoneToInsert    = m_zoneToInsert;
	m_savedPosToInsert     = m_posToInsert;
}

void Basket::restoreInsertionData()
{
	m_clickedToInsert = m_savedClickedToInsert;
	m_zoneToInsert    = m_savedZoneToInsert;
	m_posToInsert     = m_savedPosToInsert;
}

void Basket::resetInsertionData()
{
	m_clickedToInsert = 0;
	m_zoneToInsert    = 0;
	m_posToInsert     = QPoint(-1, -1);
}

void Basket::hideInsertPopupMenu()
{
	QTimer::singleShot( 50/*ms*/, this, SLOT(timeoutHideInsertPopupMenu()) );
}

void Basket::timeoutHideInsertPopupMenu()
{
	resetInsertionData();
}

void Basket::acceptDropEvent(QDropEvent *event, bool preCond)
{
	// FIXME: Should not accept all actions! Or not all actions (link not supported?!)
	event->acceptAction(preCond && 1);
	event->accept(preCond);
}

void Basket::contentsMouseReleaseEvent(QMouseEvent *event)
{
	// Now disallow drag:
	m_canDrag = false;

	// Cancel Resizer move:
	if (m_resizingNote) {
		m_resizingNote  = 0;
		m_pickedResizer = 0;
		m_lockedHovering = false;
		doHoverEffects();
		save();
	}

	// Cancel Note move:
/*	if (m_movingNote) {
		m_movingNote   = 0;
		m_pickedHandle = QPoint(0, 0);
		m_lockedHovering = false;
		//doHoverEffects();
		save();
	}
*/

	// Cancel Selection rectangle:
	if (m_isSelecting) {
		m_isSelecting = false;
		stopAutoScrollSelection();
		resetWasInLastSelectionRect();
		doHoverEffects();
		updateContents(m_selectionRect);
	}
	m_selectionStarted = false;

	// Do nothing if an action has already been made during mousePressEvent,
	// or if user made a selection and canceled it by regressing to a very small rectangle.
	if (m_noActionOnMouseRelease)
		return;
	// We immediatly set it to true, to avoid actions set on mouseRelease if NO mousePress event has been triggered.
	// This is the case when a popup menu is shown, and user click to the basket area to close it:
	// the menu then receive the mousePress event and the basket area ONLY receive the mouseRelease event.
	// Obviously, nothing should be done in this case:
	m_noActionOnMouseRelease = true;

	Note *clicked = noteAt(event->pos().x(), event->pos().y());
	if ( ! clicked ) {
		if (isFreeLayout() && event->button() == Qt::LeftButton) {
			clickedToInsert(event);
			save();
		}
		return;
	}
	Note::Zone zone = clicked->zoneAt( event->pos() - QPoint(clicked->x(), clicked->y()) );

	// Switch tag states:
	if (zone >= Note::Emblem0) {
		if (event->button() == Qt::LeftButton) {
			int icons = -1;
			for (State::List::iterator it = clicked->states().begin(); it != clicked->states().end(); ++it) {
				if ( ! (*it)->emblem().isEmpty() )
					icons++;
				if (icons == zone - Note::Emblem0) {
					State *state = (*it)->nextState();
					if (!state)
						return;
					it = clicked->states().insert(it, state);
					++it;
					clicked->states().remove(it);
					clicked->recomputeStyle();
					clicked->unbufferize();
					updateNote(clicked);
					updateEditorAppearance();
					filterAgain();
					save();
					break;
				}
			}
			return;
		}/* else if (event->button() == Qt::RightButton) {
		popupEmblemMenu(clicked, zone - Note::Emblem0);
		return;
	}*/
	}

	// Insert note or past clipboard:
	QString  text;
//	Note *note;
	QString  link;
	//int zone = zone;
	if (event->button() == Qt::MidButton && zone == Note::Resizer)
		return; //zone = clicked->zoneAt( event->pos() - QPoint(clicked->x(), clicked->y()), true );
	if (event->button() == Qt::RightButton && (clicked->isColumn() || zone == Note::Resizer))
		return;
	if (clicked->isGroup() && zone == Note::None)
		return;
	switch (zone) {
		case Note::Handle:
		case Note::Group:
			// We select note on mousePress if it was unselected or Ctrl is pressed.
			// But the user can want to drag select_s_ notes, so it the note is selected, we only select it alone on mouseRelease:
			if (event->stateAfter() == 0) {
				if ( !(event->stateAfter() & Qt::ControlButton) && clicked->allSelected())
					unselectAllBut(clicked);
				if (zone == Note::Handle && isDuringEdit() && editedNote() == clicked) {
					closeEditor();
					clicked->setSelected(true);
				}
			}
			break;

		case Note::Custom0:
			//unselectAllBut(clicked);
			setFocusedNote(clicked);
			noteOpen(clicked);
			break;

		case Note::GroupExpander:
		case Note::TagsArrow:
			break;

		case Note::Link:
			link = clicked->linkAt(event->pos() - QPoint(clicked->x(), clicked->y()));
			if ( ! link.isEmpty() ) {
				KRun *run = new KRun(link); //  open the URL.
				run->setAutoDelete(true);
				break;
			} // If there is no link, edit note content
		case Note::Content:
			closeEditor();
			unselectAllBut(clicked);
			noteEdit(clicked, /*justAdded=*/false, event->pos());
			break;

		case Note::TopInsert:
		case Note::TopGroup:
		case Note::BottomInsert:
		case Note::BottomGroup:
		case Note::BottomColumn:
			clickedToInsert(event, clicked, zone);
			save();
			break;

		case Note::None:
		default:
			KMessageBox::information(viewport(),
				i18n("This message should never appear. If it's the case, this program is buggy! "
				"Please report the bug to the developer."));
			break;
	}
}

void Basket::contentsMouseDoubleClickEvent(QMouseEvent *event)
{
	Note *clicked = noteAt(event->pos().x(), event->pos().y());
	Note::Zone zone = (clicked ? clicked->zoneAt( event->pos() - QPoint(clicked->x(), clicked->y()) ) : Note::None);

	if (event->button() == Qt::LeftButton && (zone == Note::Group || zone == Note::Handle)) {
		doCopy(CopyToSelection);
		m_noActionOnMouseRelease = true;
	} else
		contentsMousePressEvent(event);
}

void Basket::contentsMouseMoveEvent(QMouseEvent *event)
{
	// Drag the notes:
	if (m_canDrag && (m_pressPos - event->pos()).manhattanLength() > KApplication::startDragDistance()) {
		m_canDrag          = false;
		m_isSelecting      = false; // Don't draw selection rectangle ater drag!
		m_selectionStarted = false;
		NoteSelection *selection = selectedNotes();
		if (selection->firstStacked()) {
			QDragObject *d = NoteDrag::dragObject(selection, /*cutting=*/false, /*source=*/this); // d will be deleted by QT
		/*bool shouldRemove = */d->drag();
//		delete selection;

		// Never delete because URL is dragged and the file must be available for the extern appliation
//		if (shouldRemove && d->target() == 0) // If target is another application that request to remove the note
//			emit wantDelete(this);
		}
		return;
	}

	// Moving a Resizer:
	if (m_resizingNote) {
		int groupWidth = event->pos().x() - m_resizingNote->x() - m_pickedResizer;
		int minRight   = m_resizingNote->minRight();
		int maxRight   = 100 * contentsWidth(); // A big enough value (+infinity) for free layouts.
		Note *nextColumn = m_resizingNote->next();
		if (m_resizingNote->isColumn()) {
			if (nextColumn)
				maxRight = nextColumn->x() + nextColumn->rightLimit() - nextColumn->minRight() - Note::RESIZER_WIDTH;
			else
				maxRight = contentsWidth();
		}
		if (groupWidth > maxRight - m_resizingNote->x())
			groupWidth = maxRight - m_resizingNote->x();
		if (groupWidth < minRight - m_resizingNote->x())
			groupWidth = minRight - m_resizingNote->x();
		int delta = groupWidth - m_resizingNote->groupWidth();
		m_resizingNote->setGroupWidth(groupWidth);
		// If resizing columns:
		if (m_resizingNote->isColumn()) {
			Note *column = m_resizingNote;
			if ( (column = column->next()) ) {
				// Next columns should not have them X coordinate animated, because it would flicker:
				column->setXRecursivly(column->x() + delta);
				// And the resizer should resize the TWO sibling columns, and not push the other columns on th right:
				column->setGroupWidth(column->groupWidth() - delta);
			}
		}
		relayoutNotes(true);
	}

	// Moving a Note:
/*	if (m_movingNote) {
		int x = event->pos().x() - m_pickedHandle.x();
		int y = event->pos().y() - m_pickedHandle.y();
		if (x < 0) x = 0;
		if (y < 0) y = 0;
		m_movingNote->setX(x);
		m_movingNote->setY(y);
		m_movingNote->relayoutAt(x, y, / *animate=* /false);
		relayoutNotes(true);
	}
*/

	// Dragging the selection rectangle:
	if (m_selectionStarted)
		doAutoScrollSelection();

	doHoverEffects(event->pos());
}

void Basket::doAutoScrollSelection()
{
	static const int AUTO_SCROLL_MARGIN = 50;  // pixels
	static const int AUTO_SCROLL_DELAY  = 100; // milliseconds

	QPoint pos = viewport()->mapFromGlobal(QCursor::pos());

	// Do the selection:

	if (m_isSelecting)
		updateContents(m_selectionRect);

	m_selectionEndPoint = viewportToContents(pos);
	m_selectionRect = QRect(m_selectionBeginPoint, m_selectionEndPoint).normalize();
	if (m_selectionRect.left() < 0)                    m_selectionRect.setLeft(0);
	if (m_selectionRect.top() < 0)                     m_selectionRect.setTop(0);
	if (m_selectionRect.right() >= contentsWidth())    m_selectionRect.setRight(contentsWidth() - 1);
	if (m_selectionRect.bottom() >= contentsHeight())  m_selectionRect.setBottom(contentsHeight() - 1);

	if ( (m_selectionBeginPoint - m_selectionEndPoint).manhattanLength() > QApplication::startDragDistance() ) {
		m_isSelecting = true;
		selectNotesIn(m_selectionRect, m_selectionInvert);
		updateContents(m_selectionRect);
		m_noActionOnMouseRelease = true;
	} else {
			// If the user was selecting but cancel by making the rectangle too small, cancel it really!!!
		if (m_isSelecting) {
			if (m_selectionInvert)
				selectNotesIn(QRect(), m_selectionInvert);
			else
				unselectAllBut(0); // TODO: unselectAll();
		}
		if (m_isSelecting)
			resetWasInLastSelectionRect();
		m_isSelecting = false;
		stopAutoScrollSelection();
		return;
	}

	// Do the auto-scrolling:
	// FIXME: It's still flickering

	QRect insideRect(AUTO_SCROLL_MARGIN, AUTO_SCROLL_MARGIN, visibleWidth() - 2*AUTO_SCROLL_MARGIN, visibleHeight() - 2*AUTO_SCROLL_MARGIN);

	int dx = 0;
	int dy = 0;

	if (pos.y() < AUTO_SCROLL_MARGIN)
		dy = pos.y() - AUTO_SCROLL_MARGIN;
	else if (pos.y() > visibleHeight() - AUTO_SCROLL_MARGIN)
		dy = pos.y() - visibleHeight() + AUTO_SCROLL_MARGIN;

	if (pos.x() < AUTO_SCROLL_MARGIN)
		dx = pos.x() - AUTO_SCROLL_MARGIN;
	else if (pos.x() > visibleWidth() - AUTO_SCROLL_MARGIN)
		dx = pos.x() - visibleWidth() + AUTO_SCROLL_MARGIN;

	if (dx || dy) {
		kapp->sendPostedEvents(); // Do the repaints, because the scrolling will make the area to repaint to be wrong
		scrollBy(dx, dy);
		if (!m_autoScrollSelectionTimer.isActive())
			m_autoScrollSelectionTimer.start(AUTO_SCROLL_DELAY);
	} else
		stopAutoScrollSelection();
}

void Basket::stopAutoScrollSelection()
{
	m_autoScrollSelectionTimer.stop();
}

void Basket::resetWasInLastSelectionRect()
{
	Note *note = m_firstNote;
	while (note) {
		note->resetWasInLastSelectionRect();
		note = note->next();
	}
}

void Basket::selectAll()
{
	if (redirectEditActions()) {
		if (m_editor->textEdit())
			m_editor->textEdit()->selectAll();
		else if (m_editor->lineEdit())
			m_editor->lineEdit()->selectAll();
	} else {
		// First select all in the group, then in the parent group...
		Note *child  = m_focusedNote;
		Note *parent = (m_focusedNote ? m_focusedNote->parentNote() : 0);
		while (parent) {
			if (!parent->allSelected()) {
				parent->setSelectedRecursivly(true);
				return;
			}
			child  = parent;
			parent = parent->parentNote();
		}
		// Then, select all:
		FOR_EACH_NOTE (note)
			note->setSelectedRecursivly(true);
	}
}

void Basket::unselectAll()
{
	if (redirectEditActions()) {
		if (m_editor->textEdit()) {
			m_editor->textEdit()->removeSelection();
			selectionChangedInEditor(); // THIS IS NOT EMITED BY Qt!!!
		} else if (m_editor->lineEdit())
			m_editor->lineEdit()->deselect();
	} else {
		if (countSelecteds() > 0) // Optimisation
			FOR_EACH_NOTE (note)
				note->setSelectedRecursivly(false);
	}
}

void Basket::invertSelection()
{
	FOR_EACH_NOTE (note)
		note->invertSelectionRecursivly();
}

void Basket::unselectAllBut(Note *toSelect)
{
	FOR_EACH_NOTE (note)
		note->unselectAllBut(toSelect);
}

void Basket::invertSelectionOf(Note *toSelect)
{
	FOR_EACH_NOTE (note)
		note->invertSelectionOf(toSelect);
}

void Basket::selectNotesIn(const QRect &rect, bool invertSelection, bool unselectOthers /*= true*/)
{
	FOR_EACH_NOTE (note)
		note->selectIn(rect, invertSelection, unselectOthers);
}

void Basket::doHoverEffects()
{
	doHoverEffects(  viewportToContents( viewport()->mapFromGlobal(QCursor::pos()) )  );
}

void Basket::doHoverEffects(Note *note, Note::Zone zone, const QPoint &pos)
{
	// Inform the old and new hovered note (if any):
	Note *oldHoveredNote = m_hoveredNote;
	if (note != m_hoveredNote) {
		if (m_hoveredNote) {
			m_hoveredNote->setHovered(false);
			m_hoveredNote->setHoveredZone(Note::None);
			updateNote(m_hoveredNote);
		}
		m_hoveredNote = note;
		if (note)
			note->setHovered(true);
	}

	// If we are hovering a note, compute which zone is hovered and inform the note:
	if (m_hoveredNote) {
		if (zone != m_hoveredZone || oldHoveredNote != m_hoveredNote) {
			m_hoveredZone = zone;
			m_hoveredNote->setCursor(zone);
			updateNote(m_hoveredNote);
		}
		m_hoveredNote->setHoveredZone(zone);
		// If we are hovering an insert line zone, update this thing:
		if (zone == Note::TopInsert || zone == Note::TopGroup || zone == Note::BottomInsert || zone == Note::BottomGroup || zone == Note::BottomColumn)
			placeInserter(m_hoveredNote, zone);
		else
			removeInserter();
		// If we are hovering an embedded link in a rich text element, show the destination in the statusbar:
		if (zone == Note::Link)
			emit setStatusBarText(m_hoveredNote->linkAt( pos - QPoint(m_hoveredNote->x(), m_hoveredNote->y()) ));
		else if (m_hoveredNote->content())
			emit setStatusBarText(m_hoveredNote->content()->statusBarMessage(m_hoveredZone));//resetStatusBarText();
	// If we aren't hovering a note, reset all:
	} else {
		if (isFreeLayout() && !isSelecting())
			viewport()->setCursor(Qt::CrossCursor);
		else
			viewport()->unsetCursor();
		m_hoveredZone = Note::None;
		removeInserter();
		emit resetStatusBarText();
	}
}

void Basket::doHoverEffects(const QPoint &pos)
{
//	if (isDuringEdit())
//		viewport()->unsetCursor();

	// Do we have the right to do hover effects?
	if ( ! m_loaded || m_lockedHovering)
		return;

	// enterEvent() (mouse enter in the widget) set m_underMouse to true, and leaveEvent() make it false.
	// But some times the enterEvent() is not trigerred: eg. when dragging the scrollbar:
	// Ending the drag INSIDE the basket area will make NO hoverEffects() because m_underMouse is false.
	// User need to leave the area and re-enter it to get effects.
	// This hack solve that by dismissing the m_underMouse variable:
	bool underMouse = Global::mainContainer->currentBasket() == this && QRect(contentsX(), contentsY(), visibleWidth(), visibleHeight()).contains(pos);

	// Don't do hover effects when a popup menu is opened.
	// Primarily because the basket area will only receive mouseEnterEvent and mouveLeaveEvent.
	// It willn't be noticed of mouseMoveEvent, which would result in a apparently broken application state:
	if (kapp->activePopupWidget())
		underMouse = false;

	// Compute which note is hovered:
	Note       *note = (m_isSelecting || !underMouse ? 0 : noteAt(pos.x(), pos.y()));
	Note::Zone  zone = (note ? note->zoneAt( pos - QPoint(note->x(), note->y()), isDuringDrag() ) : Note::None);

	// Inform the old and new hovered note (if any) and update the areas:
	doHoverEffects(note, zone, pos);
}

void Basket::removeInserter()
{
	if (m_inserterShown) { // Do not hide (and then update/repaint the view) if it is already hidden!
		m_inserterShown = false;
		updateContents(m_inserterRect);
	}
}

void Basket::placeInserter(Note *note, int zone)
{
	// Remove the inserter:
	if (!note) {
		removeInserter();
		return;
	}

	// Update the old position:
	if (inserterShown())
		updateContents(m_inserterRect);
	// Some comodities:
	m_inserterShown = true;
	m_inserterTop   = (zone == Note::TopGroup || zone == Note::TopInsert);
	m_inserterGroup = (zone == Note::TopGroup || zone == Note::BottomGroup);
	// X and width:
	int groupIndent = (note->isGroup() ? note->width() : Note::HANDLE_WIDTH);
	int x     = note->x();
	int width = (note->isGroup() ? note->rightLimit() - note->x() : note->width());
	if (m_inserterGroup) {
		x     += groupIndent;
		width -= groupIndent;
	}
	m_inserterSplit = (Settings::groupOnInsertionLine() && note && !note->isGroup() && !note->isFree() && !note->isColumn());
//	if (note->isGroup())
//		width = note->rightLimit() - note->x() - (m_inserterGroup ? groupIndent : 0);
	// Y:
	int y = note->y() - (m_inserterGroup && m_inserterTop ? 1 : 3);
	if (!m_inserterTop)
		y += (note->isColumn() ? note->finalHeight() : note->height());
	// Assigning result:
	m_inserterRect = QRect(x, y, width, 6 - (m_inserterGroup ? 2 : 0));
	// Update the new position:
	updateContents(m_inserterRect);
}

inline void drawLineByRect(QPainter &painter, int x, int y, int width, int height)
{
	painter.drawLine(x, y, x + width - 1, y + height - 1);
}

void Basket::drawInserter(QPainter &painter, int xPainter, int yPainter)
{
	if (!m_inserterShown)
		return;

	QRect rect = m_inserterRect; // For shorter code-lines when drawing!
	rect.moveBy(-xPainter, -yPainter);
	int lineY  = (m_inserterGroup && m_inserterTop ? 0 : 2);
	int roundY = (m_inserterGroup && m_inserterTop ? 0 : 1);

	QColor dark  = KApplication::palette().active().dark();
	QColor light = dark.light().light();
	if (m_inserterGroup && Settings::groupOnInsertionLine())
		light = Tools::mixColor(light, KGlobalSettings::highlightColor());
	painter.setPen(dark);
	// The horizontal line:
	//painter.drawRect(       rect.x(),                    rect.y() + lineY,  rect.width(), 2);
	int width = rect.width() - 4;
	drawGradient(&painter, dark,  light, rect.x() + 2,           rect.y() + lineY, width/2,         2, /*sunken=*/false, /*horz=*/false, /*flat=*/false);
	drawGradient(&painter, light, dark,  rect.x() + 2 + width/2, rect.y() + lineY, width - width/2, 2, /*sunken=*/false, /*horz=*/false, /*flat=*/false);
	// The left-most and right-most edges (biggest vertical lines):
	drawLineByRect(painter, rect.x(),                    rect.y(),          1,            (m_inserterGroup ? 4 : 6));
	drawLineByRect(painter, rect.x() + rect.width() - 1, rect.y(),          1,            (m_inserterGroup ? 4 : 6));
	// The left and right mid vertical lines:
	drawLineByRect(painter, rect.x() + 1,                rect.y() + roundY, 1,            (m_inserterGroup ? 3 : 4));
	drawLineByRect(painter, rect.x() + rect.width() - 2, rect.y() + roundY, 1,            (m_inserterGroup ? 3 : 4));
	// Draw the split as a feedback to know where is the limit between insert and group:
	if (m_inserterSplit) {
		int noteWidth = rect.width() + (m_inserterGroup ? Note::HANDLE_WIDTH : 0);
		int xSplit = rect.x() - (m_inserterGroup ? Note::HANDLE_WIDTH : 0) + noteWidth / 2;
		painter.setPen(Tools::mixColor(dark, light));
		painter.drawRect(xSplit - 2, rect.y() + lineY, 4, 2);
		painter.setPen(dark);
		painter.drawRect(xSplit - 1, rect.y() + lineY, 2, 2);
	}
}

void Basket::maybeTip(const QPoint &pos)
{
	if ( !m_loaded || !Settings::showNotesToolTip() )
		return;

	QString message;
	QRect   rect;

	QPoint contentPos = viewportToContents(pos);
	Note *note = noteAt(contentPos.x(), contentPos.y());

	/*if (isDuringEdit()) {
		message = i18n("Confirm note changes"); // TODO: i18n("Cancel note addition");    i18n("Confirm note addition");   ""
		QValueList<QRect> areas;
		areas.clear();
		areas.append( QRect(0, 0, contentsWidth(), contentsHeight()) );
		QRect editorRect(m_editor->widget()->x(), m_editor->widget()->y(), m_editor->widget()->width(), m_editor->widget()->height());
		substractRectOnAreas(editorRect, areas, / *andRemove=* /true);
		for (QValueList<QRect>::iterator it = areas.begin(); it != areas.end(); ++it)
			if ((*it).contains(pos))
				rect = *it;
	} else */if (!note && isFreeLayout()) {
		message = i18n("Insert note here\nRight click for more options");
		QRect itRect;
		for (QValueList<QRect>::iterator it = m_blankAreas.begin(); it != m_blankAreas.end(); ++it) {
			itRect = QRect(0, 0, visibleWidth(), visibleHeight()).intersect(*it);
			if (itRect.contains(contentPos)) {
				rect = itRect;
				rect.moveLeft(rect.left() - contentsX());
				rect.moveTop( rect.top()  - contentsY());
				break;
			}
		}
	} else {
		if (!note)
			return;

		Note::Zone zone = note->zoneAt( contentPos - QPoint(note->x(), note->y()) );
		switch (zone) {
			case Note::Resizer:       message = (note->isColumn() ?
			                                    i18n("Resize those columns") :
			                                    (note->isGroup() ?
			                                    i18n("Resize this group") :
			                                    i18n("Resize this note")));                 break;
			case Note::Handle:        message = i18n("Select or move this note");           break;
			case Note::Group:         message = i18n("Select or move this group");          break;
			case Note::TagsArrow:     message = i18n("Assign or remove tags from this note");
			                          if (note->states().count() > 0) {
			                          	message = "<qt><nobr>" + message + "</nobr><br>" + i18n("<b>Assigned Tags</b>: %1");
			                          	QString tagsString = "";
			                          	for (State::List::iterator it = note->states().begin(); it != note->states().end(); ++it) {
			                          		QString tagName = "<nobr>" + Tools::textToHTMLWithoutP((*it)->fullName()) + "</nobr>";
			                          		if (tagsString.isEmpty())
			                          			tagsString = tagName;
			                          		else
			                          			tagsString = i18n("%1, %2").arg(tagsString, tagName);
			                          	}
			                          	message = message.arg(tagsString);
			                          }
			                          break;
			case Note::Custom0:       message = note->content()->zoneTip(zone);             break; //"Open this link/Open this file/Open this sound file/Launch this application"
			case Note::GroupExpander: message = (note->isFolded() ?
			                                    i18n("Expand this group") :
			                                    i18n("Collapse this group"));               break;
			case Note::Link:
			case Note::Content:       message = note->content()->editToolTipText();         break;
			case Note::TopInsert:
			case Note::BottomInsert:  message = i18n("Insert note here\nRight click for more options");              break;
			case Note::TopGroup:      message = i18n("Group note with the one below\nRight click for more options"); break;
			case Note::BottomGroup:   message = i18n("Group note with the one above\nRight click for more options"); break;
			case Note::BottomColumn:  message = i18n("Insert note here\nRight click for more options");              break;
			case Note::None:          message = "** Zone NONE: internal error **";                                   break;
			default:
				if (zone >= Note::Emblem0)
					message = note->stateForEmblemNumber(zone - Note::Emblem0)->fullName();
				else
					message = "";
				break;
		}

		if (zone == Note::Content || zone == Note::Link) {
			QStringList keys;
			QStringList values;

			note->content()->toolTipInfos(&keys, &values);
			keys.append(i18n("Added"));
			keys.append(i18n("Last Modification"));
			values.append(note->addedStringDate());
			values.append(note->lastModificationStringDate());

			message = "<qt><nobr>" + message;
			QStringList::iterator key;
			QStringList::iterator value;
			for (key = keys.begin(), value = values.begin(); key != keys.end() && value != values.end(); ++key, ++value)
				message += "<br>" + i18n("of the form 'key: value'", "<b>%1</b>: %2").arg(*key, *value);
			message += "</nobr></qt>";
		} else if (m_inserterSplit && (zone == Note::TopInsert || zone == Note::BottomInsert))
			message += "\n" + i18n("Click on the right to group instead of insert");
		else if (m_inserterSplit && (zone == Note::TopGroup || zone == Note::BottomGroup))
			message += "\n" + i18n("Click on the left to insert instead of group");

		rect = note->zoneRect( zone, contentPos - QPoint(note->x(), note->y()) );

		rect.moveLeft(rect.left() - contentsX());
		rect.moveTop( rect.top()  - contentsY());

		rect.moveLeft(rect.left() + note->x());
		rect.moveTop( rect.top()  + note->y());
	}

	tip(rect, message);
}

Note* Basket::lastNote()
{
	Note *note = firstNote();
	while (note && note->next())
		note = note->next();
	return note;
}

Note* Basket::noteAt(int x, int y)
{
//NO:
// 	// Do NOT check the bottom&right borders.
// 	// Because imagine someone drag&drop a big note from the top to the bottom of a big basket (with big vertical scrollbars),
// 	// the note is first removed, and relayoutNotes() compute the new height that is smaller
// 	// Then noteAt() is called for the mouse pointer position, because the basket is now smaller, the cursor is out of boundaries!!!
// 	// Should, of course, not return 0:
	if (x < 0 || x > contentsWidth() || y < 0 || y > contentsHeight())
		return 0;

	// When resizing a note/group, keep it highlighted:
	if (m_resizingNote)
		return m_resizingNote;

	// Search and return the hovered note:
	Note *note = m_firstNote;
	Note *possibleNote;
	while (note) {
		possibleNote = note->noteAt(x, y);
		if (possibleNote) {
			if (draggedNotes().contains(possibleNote))
				return 0;
			else
				return possibleNote;
		}
		note = note->next();
	}

	// If the basket is layouted in columns, return one of the columns to be able to add notes in them:
	if (isColumnsLayout()) {
		Note *column = m_firstNote;
		while (column) {
			if (x >= column->x() && x < column->rightLimit())
				return column;
			column = column->next();
		}
	}

	// Nothing found, no note is hovered:
	return NULL;
}

Basket::~Basket()
{
	delete m_action;
	// TODO: Delete Notes (and then NoteContents)!
}

void Basket::viewportResizeEvent(QResizeEvent *event)
{
	relayoutNotes(true);
	//cornerWidget()->setShown(horizontalScrollBar()->isShown() && verticalScrollBar()->isShown());
	if (horizontalScrollBar()->isShown() && verticalScrollBar()->isShown()) {
		if (!cornerWidget())
			setCornerWidget(m_cornerWidget);
	} else {
		if (cornerWidget())
			setCornerWidget(0);
	}
//	if (isDuringEdit())
//		ensureNoteVisible(editedNote());
	QScrollView::viewportResizeEvent(event);
}

void Basket::animateLoad()
{
	const int viewHeight = contentsY() + visibleHeight();

	QTime t = QTime::currentTime(); // Set random seed
	srand(t.hour()*12 + t.minute()*60 + t.second()*60);

	Note *note = firstNote();
	while (note) {
		if ((note->finalY() < viewHeight) && note->matching())
			note->initAnimationLoad();
		note = note->next();
	}

	m_loaded = true;
}

QColor Basket::selectionRectInsideColor()
{
	return Tools::mixColor(Tools::mixColor(backgroundColor(), KGlobalSettings::highlightColor()), backgroundColor());
}

QColor alphaBlendColors(const QColor &bgColor, const QColor &fgColor, const int a)
{
	// normal button...
	QRgb rgb = bgColor.rgb();
	QRgb rgb_b = fgColor.rgb();
	int alpha = a;
	if (alpha>255) alpha = 255;
	if (alpha<0) alpha = 0;
	int inv_alpha = 255 - alpha;
	QColor result = QColor( qRgb(qRed(rgb_b)*inv_alpha/255 + qRed(rgb)*alpha/255,
	                        qGreen(rgb_b)*inv_alpha/255 + qGreen(rgb)*alpha/255,
	                        qBlue(rgb_b)*inv_alpha/255 + qBlue(rgb)*alpha/255) );

	return result;
}

void Basket::drawContents(QPainter *painter, int clipX, int clipY, int clipWidth, int clipHeight)
{
	// Start the load the first time the basket is shown:
	if (!m_loadingLaunched)
		QTimer::singleShot( 0, this, SLOT(load()) );

	QBrush brush(backgroundColor()); // FIXME: share it for all the basket?
	QRect clipRect(clipX, clipY, clipWidth, clipHeight);

	// Draw notes (but not when it's not loaded yet):
	Note *note = (m_loaded ? m_firstNote : 0);
	while (note) {
		note->draw(painter, clipRect);
		note = note->next();
	}

	// Draw loading message:
	if (!m_loaded) {
		QPixmap pixmap(visibleWidth(), visibleHeight()); // TODO: Clip it to asked size only!
		QPainter painter2(&pixmap);
		QSimpleRichText rt(QString("<center>%1</center>").arg(i18n("Loading...")), QScrollView::font());
		rt.setWidth(visibleWidth());
		int hrt = rt.height();
		painter2.fillRect(0, 0, visibleWidth(), visibleHeight(), brush);
		blendBackground(painter2, QRect(0, 0, visibleWidth(), visibleHeight()), -1, -1, /*opaque=*/true);
		QColorGroup cg = colorGroup();
		cg.setColor(QColorGroup::Text, textColor());
		rt.draw(&painter2, 0, (visibleHeight() - hrt) / 2, QRect(), cg);
		painter2.end();
		painter->drawPixmap(0, 0, pixmap);
		return; // TODO: Clip to the wanted rectangle
	}

	// We will draw blank areas below.
	// For each rectangle to be draw there is three ways to do so:
	// - The rectangle is full of the background COLOR  => we fill a rect directly on screen
	// - The rectangle is full of the background PIXMAP => we draw it directly on screen (we draw m_opaqueBackgroundPixmap that is not transparent)
	// - The rectangle contains the resizer             => We draw it on an offscreen buffer and then paint the buffer on screen
	// If the background image is not tiled, we know that recomputeBlankRects() broken rects so that they are full of either background pixmap or color, but not a mix.

	// Draw blank areas (see the last preparation above):
	QPixmap  pixmap;
	QPainter painter2;
	QRect    rect;
	for (QValueList<QRect>::iterator it = m_blankAreas.begin(); it != m_blankAreas.end(); ++it) {
		rect = clipRect.intersect(*it);
		if (rect.width() > 0 && rect.height() > 0) {
			// If there is an inserter to draw, draw the image off screen,
			// apply the inserter and then draw the image on screen:
			if (  (inserterShown() && rect.intersects(inserterRect()))  ||  (m_isSelecting && rect.intersects(m_selectionRect))  ) {
				pixmap.resize(rect.width(), rect.height());
				painter2.begin(&pixmap);
				painter2.fillRect(0, 0, rect.width(), rect.height(), backgroundColor());
				blendBackground(painter2, rect, -1, -1, /*opaque=*/true);
				// Draw inserter:
				if (inserterShown() && rect.intersects(inserterRect()))
					drawInserter(painter2, rect.x(), rect.y());
				// Draw selection rect:
				if (m_isSelecting && rect.intersects(m_selectionRect)) {
					QRect selectionRect = m_selectionRect;
					selectionRect.moveBy(-rect.x(), -rect.y());
					QRect selectionRectInside(selectionRect.x() + 1, selectionRect.y() + 1, selectionRect.width() - 2, selectionRect.height() - 2);
					if (selectionRectInside.width() > 0 && selectionRectInside.height() > 0) {
						QColor insideColor = selectionRectInsideColor();
						painter2.fillRect(selectionRectInside, insideColor);
						selectionRectInside.moveBy(rect.x(), rect.y());
						blendBackground(painter2, selectionRectInside, rect.x(), rect.y(), true, /*&*/m_selectedBackgroundPixmap);
					}
					painter2.setPen(KGlobalSettings::highlightColor().dark());
					painter2.drawRect(selectionRect);
					painter2.setPen(Tools::mixColor(KGlobalSettings::highlightColor().dark(), backgroundColor()));
					painter2.drawPoint(selectionRect.topLeft());
					painter2.drawPoint(selectionRect.topRight());
					painter2.drawPoint(selectionRect.bottomLeft());
					painter2.drawPoint(selectionRect.bottomRight());
				}
				painter2.end();
				painter->drawPixmap(rect.x(), rect.y(), pixmap);
			// If it's only a blank rectangle to draw, draw it directly on screen (faster!!!):
			} else if ( ! hasBackgroundImage() ) {
				painter->fillRect(rect, backgroundColor());
			// It's either a background pixmap to draw or a background color to fill:
			} else {
				if (isTiledBackground() || (rect.x() < backgroundPixmap()->width() && rect.y() < backgroundPixmap()->height()))
					blendBackground(*painter, rect, 0, 0, /*opaque=*/true);
				else
					painter->fillRect(rect, backgroundColor());
			}
		}
	}
}

/*  rect(x,y,width,height)==(xBackgroundToDraw,yBackgroundToDraw,widthToDraw,heightToDraw)
 */
void Basket::blendBackground(QPainter &painter, const QRect &rect, int xPainter, int yPainter, bool opaque, QPixmap *bg)
{
	if (xPainter == -1 && yPainter == -1) {
		xPainter = rect.x();
		yPainter = rect.y();
	}

	if (hasBackgroundImage()) {
		const QPixmap *bgPixmap = (bg ? /* * */bg : (opaque ? m_opaqueBackgroundPixmap : m_backgroundPixmap));
		if (isTiledBackground())
			painter.drawTiledPixmap(rect.x() - xPainter, rect.y() - yPainter, rect.width(), rect.height(), *bgPixmap, rect.x(), rect.y());
		else
			painter.drawPixmap(rect.x() - xPainter, rect.y() - yPainter, *bgPixmap, rect.x(), rect.y(), rect.width(), rect.height());
	}
}

void Basket::recomputeBlankRects()
{
	m_blankAreas.clear();
	m_blankAreas.append( QRect(0, 0, contentsWidth(), contentsHeight()) );

	FOR_EACH_NOTE (note)
		note->recomputeBlankRects(m_blankAreas);

	// See the drawing of blank areas in Basket::drawContents()
	if (hasBackgroundImage() && ! isTiledBackground())
		substractRectOnAreas( QRect(0, 0, backgroundPixmap()->width(), backgroundPixmap()->height()), m_blankAreas, false );
}

void Basket::addAnimatedNote(Note *note)
{
	if (m_animatedNotes.isEmpty()) {
		m_animationTimer.start(FRAME_DELAY);
		m_lastFrameTime = QTime::currentTime();
	}

	m_animatedNotes.append(note);
}

void Basket::unsetNotesWidth()
{
	Note *note = m_firstNote;
	while (note) {
		note->unsetWidth();
		note = note->next();
	}
}

void Basket::relayoutNotes(bool animate)
{
	if (!Settings::playAnimations())
		animate = false;

	if (!animate) {
		m_animatedNotes.clear();
		m_animationTimer.stop();
	}

	int h     = 0;
	tmpWidth  = 0;
	tmpHeight = 0;
	Note *note = m_firstNote;
	while (note) {
		if (note->matching()) {
			note->relayoutAt(0, h, animate);
			if (note->isColumn() && note->hasResizer()) {
				int minGroupWidth = note->minRight() - note->x();
				if (note->groupWidth() < minGroupWidth) {
					note->setGroupWidth(minGroupWidth);
					relayoutNotes(animate); // Redo the thing, but this time it should not recurse
					return;
				}
			}
			h += note->finalHeight();
		}
		note = note->next();
	}

	if (isFreeLayout())
		tmpHeight += 100;

	resizeContents( QMAX(tmpWidth, visibleWidth()), QMAX(tmpHeight, visibleHeight()) );
	recomputeBlankRects();
	placeEditor();
	doHoverEffects();
	updateContents();
}

void Basket::updateNote(Note *note)
{
	updateContents(note->rect());
	if (note->hasResizer())
		updateContents(note->resizerRect());
}

void Basket::animateObjects()
{
	QValueList<Note*>::iterator it;
	for (it = m_animatedNotes.begin(); it != m_animatedNotes.end(); )
//		if ((*it)->y() >= contentsY() && (*it)->bottom() <= contentsY() + contentsWidth())
//			updateNote(*it);
		if ((*it)->advance())
			it = m_animatedNotes.remove(it);
	else {
//			if ((*it)->y() >= contentsY() && (*it)->bottom() <= contentsY() + contentsWidth())
//				updateNote(*it);
		++it;
	}

	if (m_animatedNotes.isEmpty()) {
		// Stop animation timer:
		m_animationTimer.stop();
		// Reset all onTop notes:
		Note* note = m_firstNote;
		while (note) {
			note->setOnTop(false);
			note = note->next();
		}
	}

	if (m_focusedNote)
		ensureNoteVisible(m_focusedNote);

	// We refresh content if it was the last frame,
	// or if the drawing of the last frame was not too long.
	if (!m_animationTimer.isActive() || (m_lastFrameTime.msecsTo(QTime::currentTime()) < FRAME_DELAY*11/10)) { // *11/10 == *1.1 : We keep a 0.1 security margin
		m_lastFrameTime = m_lastFrameTime.addMSecs(FRAME_DELAY);                                               // because timers are not accurate and can trigger late
		//m_lastFrameTime = QTime::currentTime();
//std::cout << ">>" << m_lastFrameTime.toString("hh:mm:ss.zzz") << std::endl;
		if (m_underMouse)
			doHoverEffects();
		recomputeBlankRects();
		//relayoutNotes(true); // In case an animated note was to the contents view boundaries, resize the view!
		updateContents();
	// If the drawing of the last frame was too long, we skip the drawing of the current and do the next one:
	} else {
		m_lastFrameTime = m_lastFrameTime.addMSecs(FRAME_DELAY);
//std::cout << "+=" << m_lastFrameTime.toString("hh:mm:ss.zzz") << std::endl;
		animateObjects();
	}

	doHoverEffects();
	placeEditor();

/*	int delta = m_deltaY / 3;
	if (delta == 0       && m_deltaY != 0)
	delta = (m_deltaY > 0 ? 1 : -1);
	m_deltaY -= delta;
	resizeContents(contentsWidth(), contentsHeight() + delta); //m_lastNote->y() + m_lastNote->height()
*/
}

void Basket::popupEmblemMenu(Note *note, int emblemNumber)
{
	m_tagPopupNote = note;
	State *state = note->stateForEmblemNumber(emblemNumber);
	State *nextState = state->nextState(/*cycle=*/false);
	Tag *tag = state->parentTag();
	m_tagPopup = tag;

	QKeySequence sequence = tag->shortcut().operator QKeySequence();
	bool sequenceOnDelete = (nextState == 0 && !tag->shortcut().isNull());

	KPopupMenu menu(this);
	if (tag->countStates() == 1) {
		menu.insertTitle(/*SmallIcon(state->icon()), */tag->name());
		menu.insertItem( SmallIconSet("editdelete"), i18n("&Remove"),             1 );
		menu.insertItem( SmallIconSet("configure"),  i18n("&Customize..."),       2 );
		menu.insertSeparator();
		menu.insertItem( SmallIconSet("filter"),     i18n("&Filter by this Tag"), 3 );
	} else {
		menu.insertTitle(tag->name());
		QValueList<State*>::iterator it;
		State *currentState;

		int i = 10;
		for (it = tag->states().begin(); it != tag->states().end(); ++it) {
			currentState = *it;
			QKeySequence sequence;
			if (currentState == nextState && !tag->shortcut().isNull())
				sequence = tag->shortcut().operator QKeySequence();
			menu.insertItem(StateMenuItem::radioButtonIconSet(state == currentState, menu.colorGroup()), new StateMenuItem(currentState, sequence, false), i );
			if (currentState == nextState && !tag->shortcut().isNull())
				menu.setAccel(sequence, i);
			++i;
		}
		menu.insertSeparator();
		menu.insertItem( new IndentedMenuItem(i18n("&Remove"),               "editdelete", (sequenceOnDelete ? sequence : QKeySequence())), 1 );
		menu.insertItem( new IndentedMenuItem(i18n("&Customize..."),         "configure"),  2 );
		menu.insertSeparator();
		menu.insertItem( new IndentedMenuItem(i18n("&Filter by this Tag"),   "filter"),     3 );
		menu.insertItem( new IndentedMenuItem(i18n("Filter by this &State"), "filter"),     4 );
	}
	if (sequenceOnDelete)
		menu.setAccel(sequence, 1);

	connect( &menu, SIGNAL(activated(int)), this, SLOT(toggledStateInMenu(int)) );
	connect( &menu, SIGNAL(aboutToHide()),  this, SLOT(unlockHovering())        );
	connect( &menu, SIGNAL(aboutToHide()),  this, SLOT(disableNextClick())      );

	m_lockedHovering = true;
	menu.exec(QCursor::pos());
}

void Basket::toggledStateInMenu(int id)
{
	if (id == 1) {
		removeTagFromSelectedNotes(m_tagPopup);
		//m_tagPopupNote->removeTag(m_tagPopup);
		//m_tagPopupNote->setWidth(0); // To force a new layout computation
		updateEditorAppearance();
		filterAgain();
		save();
		return;
	}
	if (id == 2) {
		KMessageBox::information(viewport(),
			"<qt>This is not implemented yet.<br>In a future version, you will be able to add, remove or modify tags "
			"by setting icon, states, background color, font, font size, bold, italic, underline, text color...<br>"
			"But for now, I'm afraid you have to wait ;-)");
		return;
	}
	if (id == 3) { // Filter by this Tag
		decoration()->filterBar()->filterTag(m_tagPopup);
		return;
	}
	if (id == 4) { // Filter by this State
		decoration()->filterBar()->filterState(m_tagPopupNote->stateOfTag(m_tagPopup));
		return;
	}

	/*addStateToSelectedNotes*/changeStateOfSelectedNotes(m_tagPopup->states()[id - 10] /*, orReplace=true*/);
	//m_tagPopupNote->addState(m_tagPopup->states()[id - 10], true);
	filterAgain();
	save();
}

State* Basket::stateForTagFromSelectedNotes(Tag *tag)
{
	State *state = 0;

	FOR_EACH_NOTE (note)
		if (note->stateForTagFromSelectedNotes(tag, &state) && state == 0)
			return 0;
	return state;
}

void Basket::activatedTagShortcut(Tag *tag)
{
	// Compute the next state to set:
	State *state = stateForTagFromSelectedNotes(tag);
	if (state)
		state = state->nextState(/*cycle=*/false);
	else
		state = tag->states().first();

	// Set or unset it:
	if (state) {
		FOR_EACH_NOTE (note)
			note->addStateToSelectedNotes(state, /*orReplace=*/true);
		updateEditorAppearance();
	} else
		removeTagFromSelectedNotes(tag);

	filterAgain();
	save();
}

void Basket::popupTagsMenu(Note *note)
{
	m_tagPopupNote = note;

	KPopupMenu menu(this);
	menu.insertTitle(i18n("Tags"));
	QValueList<Tag*>::iterator it;
	Tag *currentTag;
	State *currentState;
	int i = 10;
	for (it = Tag::all.begin(); it != Tag::all.end(); ++it) {
		// Current tag and first state of it:
		currentTag = *it;
		currentState = currentTag->states().first();
		QKeySequence sequence;
		if (!currentTag->shortcut().isNull())
			sequence = currentTag->shortcut().operator QKeySequence();
		menu.insertItem(StateMenuItem::checkBoxIconSet(note->hasTag(currentTag), menu.colorGroup()), new StateMenuItem(currentState, sequence, true), i );
		if (!currentTag->shortcut().isNull())
			menu.setAccel(sequence, i);
		++i;
	}

	menu.insertSeparator();
//	menu.insertItem( /*SmallIconSet("editdelete"),*/ "&Assign new Tag...", 1 );
	//id = menu.insertItem( SmallIconSet("editdelete"), "&Remove All", -2 );
	//if (note->states().isEmpty())
	//	menu.setItemEnabled(id, false);
//	menu.insertItem( SmallIconSet("configure"),  "&Customize...", 3 );
	menu.insertItem(      new IndentedMenuItem(i18n("&Assign new Tag...")),          1 );
	menu.insertItem( new IndentedMenuItem(i18n("&Remove All"),   "editdelete"), 2 );
	menu.insertItem(      new IndentedMenuItem(i18n("&Customize..."), "configure"),  3 );

	if (!selectedNotesHaveTags())//note->states().isEmpty())
		menu.setItemEnabled(2, false);

	connect( &menu, SIGNAL(activated(int)), this, SLOT(toggledTagInMenu(int)) );
	connect( &menu, SIGNAL(aboutToHide()),  this, SLOT(unlockHovering())      );
	connect( &menu, SIGNAL(aboutToHide()),  this, SLOT(disableNextClick())    );

	m_lockedHovering = true;
	menu.exec(QCursor::pos());
}

void Basket::unlockHovering()
{
	m_lockedHovering = false;
	doHoverEffects();
}

void Basket::toggledTagInMenu(int id)
{
	if (id == 1) {
		KMessageBox::information(viewport(),
			"<qt>Sorry... Not implemented yet! Please come back later :-)<br>"
			"This thing will allow to add a new tag and assign it to the note in one go.");
		return;
	}
	if (id == 2) {
		removeAllTagsFromSelectedNotes();//m_tagPopupNote->removeAllTags();
		//m_tagPopupNote->setWidth(0); // To force a new layout computation
		filterAgain();
		save();
		return;
	}
	if (id == 3) {
		KMessageBox::information(viewport(),
			"<qt>This is not implemented yet.<br>In a future version, you will be able to add, remove or modify tags "
			"by setting icon, states, background color, font, font size, bold, italic, underline, text color...<br>"
			"But for now, I'm afraid you have to wait ;-)");
		return;
	}

	Tag *tag = Tag::all[id - 10];
	if (!tag)
		return;

	if (m_tagPopupNote->hasTag(tag))
		removeTagFromSelectedNotes(tag);//m_tagPopupNote->removeTag(tag);
	else
		addTagToSelectedNotes(tag);//m_tagPopupNote->addTag(tag);
	m_tagPopupNote->setWidth(0); // To force a new layout computation
	filterAgain();
	save();
}

void Basket::addTagToSelectedNotes(Tag *tag)
{
	FOR_EACH_NOTE (note)
		note->addTagToSelectedNotes(tag);
	updateEditorAppearance();
}

void Basket::removeTagFromSelectedNotes(Tag *tag)
{
	FOR_EACH_NOTE (note)
		note->removeTagFromSelectedNotes(tag);
	updateEditorAppearance();
}

void Basket::addStateToSelectedNotes(State *state)
{
	FOR_EACH_NOTE (note)
		note->addStateToSelectedNotes(state);
	updateEditorAppearance();
}

void Basket::updateEditorAppearance()
{
	if (isDuringEdit() && m_editor->widget()) {
		m_editor->widget()->setFont(m_editor->note()->font());
		m_editor->widget()->setPaletteBackgroundColor(m_editor->note()->backgroundColor());
		m_editor->widget()->setPaletteForegroundColor(m_editor->note()->textColor());

		// Uggly Hack arround Qt bugs: placeCursor() don't call any signal:
		HtmlEditor *htmlEditor = dynamic_cast<HtmlEditor*>(m_editor);
		if (htmlEditor) {
			int para, index;
			m_editor->textEdit()->getCursorPosition(&para, &index);
			if (para == 0 && index == 0) {
				m_editor->textEdit()->moveCursor(QTextEdit::MoveForward,  /*select=*/false);
				m_editor->textEdit()->moveCursor(QTextEdit::MoveBackward, /*select=*/false);
			} else {
				m_editor->textEdit()->moveCursor(QTextEdit::MoveBackward, /*select=*/false);
				m_editor->textEdit()->moveCursor(QTextEdit::MoveForward,  /*select=*/false);
			}
			htmlEditor->cursorPositionChanged(); // Does not work anyway :-( (when clicking on a red bold text, the toolbar still show black normal text)
		}
	}
}

void Basket::changeStateOfSelectedNotes(State *state)
{
	FOR_EACH_NOTE (note)
		note->changeStateOfSelectedNotes(state);
	updateEditorAppearance();
}

void Basket::removeAllTagsFromSelectedNotes()
{
	FOR_EACH_NOTE (note)
		note->removeAllTagsFromSelectedNotes();
	updateEditorAppearance();
}

bool Basket::selectedNotesHaveTags()
{
	FOR_EACH_NOTE (note)
		if (note->selectedNotesHaveTags())
			return true;
	return false;
}

QColor Basket::backgroundColor()
{
	if (m_backgroundColorSetting.isValid())
		return m_backgroundColorSetting;
	else
		return KGlobalSettings::baseColor();
}

QColor Basket::textColor()
{
	if (m_textColorSetting.isValid())
		return m_textColorSetting;
	else
		return KGlobalSettings::textColor();
}

void Basket::unbufferizeAll()
{
	FOR_EACH_NOTE (note)
		note->unbufferizeAll();
}

Note* Basket::editedNote()
{
	if (m_editor)
		return m_editor->note();
	else
		return 0;
}

bool Basket::hasTextInEditor()
{
	if (!isDuringEdit() || !redirectEditActions())
		return false;

	if (m_editor->textEdit())
		return ! m_editor->textEdit()->text().isEmpty();
	else if (m_editor->lineEdit())
		return ! m_editor->lineEdit()->text().isEmpty();
	else
		return false;
}

bool Basket::hasSelectedTextInEditor()
{
	if (!isDuringEdit() || !redirectEditActions())
		return false;

	if (m_editor->textEdit()) {
		// The following line does NOT work if one letter is selected and the user press Shift+Left or Shift+Right to unselect than letter:
		// Qt misteriously tell us there is an invisible selection!!
		//return m_editor->textEdit()->hasSelectedText();
		return !m_editor->textEdit()->selectedText().isEmpty();
	} else if (m_editor->lineEdit())
		return m_editor->lineEdit()->hasSelectedText();
	else
		return false;
}

bool Basket::selectedAllTextInEditor()
{
	if (!isDuringEdit() || !redirectEditActions())
		return false;

	if (m_editor->textEdit())
		return m_editor->textEdit()->text().isEmpty() || m_editor->textEdit()->text() == m_editor->textEdit()->selectedText();
	else if (m_editor->lineEdit())
		return m_editor->lineEdit()->text().isEmpty() || m_editor->lineEdit()->text() == m_editor->lineEdit()->selectedText();
	else
		return false;
}

void Basket::selectionChangedInEditor()
{
	Global::mainContainer->countSelectedsChanged();
}

void Basket::contentChangedInEditor()
{
//	std::cout << "contentChangedInEditor()" << std::endl;

	if (m_inactivityAutoSaveTimer.isActive())
		m_inactivityAutoSaveTimer.stop();

	m_inactivityAutoSaveTimer.start(3 * 1000, /*singleShot=*/true);
}

void Basket::inactivityAutoSaveTimout()
{
	if (m_editor) {
//		std::cout << "Auto Saving" << std::endl;
		m_editor->autoSave();
	}
}

void Basket::placeEditorAndEnsureVisible()
{
	placeEditor(/*andEnsureVisible=*/true);
}

void Basket::placeEditor(bool andEnsureVisible /*= false*/)
{
	if (!isDuringEdit())
		return;

	QFrame    *editorQFrame = dynamic_cast<QFrame*>(m_editor->widget());
	KTextEdit *textEdit     = m_editor->textEdit();
//	QLineEdit *lineEdit     = m_editor->lineEdit();
	Note      *note         = m_editor->note();

	int frameWidth = (editorQFrame ? editorQFrame->frameWidth() : 0);
	int x          = note->x() + note->contentX() + note->content()->xEditorIndent() - frameWidth;
	int y;
	int maxHeight  = QMAX(visibleHeight(), contentsHeight());
	int height, width;

	if (textEdit) {
		x -= 4;
		// Need to do it 2 times, because it's wrong overwise:
		for (int i = 0; i < 2; i++) {
			// FIXME: CRASH: Select all text, press Del or [<--] and editor->removeSelectedText() is called:
			//        editor->sync() CRASH!!
	//		editor->sync();
			y = note->y() + Note::NOTE_MARGIN - frameWidth;
			height = textEdit->contentsHeight() + 2*frameWidth;
			height = /*QMAX(*/height/*, note->height())*/;
			height = QMIN(height, visibleHeight());
			width  = /*note->x() + note->width()*/note->rightLimit() - x + 2*frameWidth + 1;
			if (y + height > maxHeight)
				y = maxHeight - height;
			textEdit->setFixedSize(width, height);
		}
	} else {
		height = note->height() - 2*Note::NOTE_MARGIN + 2*frameWidth;
		width  = note->rightLimit() - x + 2*frameWidth;
		m_editor->widget()->setFixedSize(width, height);
		x -= 1;
		y = note->y() + Note::NOTE_MARGIN - frameWidth;
	}
//	std::cout << m_editorWidth << ":" << m_editorHeight << "       " << width << "," << height <<std::endl;
	if ((m_editorWidth > 0 && m_editorWidth != width) || (m_editorHeight > 0 && m_editorHeight != height)) {
		m_editorWidth  = width; // Avoid infinite recursion!!!
		m_editorHeight = height;
//		std::cout << "autoSaving" << std::endl;
		m_editor->autoSave();
	}
	m_editorWidth  = width;
	m_editorHeight = height;
	addChild(m_editor->widget(), x, y);

	m_leftEditorBorder->setFixedSize( (m_editor->textEdit() ? 3 : 0), height);
	m_leftEditorBorder->raise();
	addChild(m_leftEditorBorder,     x, y );
	m_leftEditorBorder->setPosition( x, y );

	m_rightEditorBorder->setFixedSize(3, height);
	m_rightEditorBorder->raise();
	addChild(m_rightEditorBorder,     note->rightLimit() - Note::NOTE_MARGIN, note->y() + Note::NOTE_MARGIN );
	m_rightEditorBorder->setPosition( note->rightLimit() - Note::NOTE_MARGIN, note->y() + Note::NOTE_MARGIN );

	// ensureWidgetVisible(m_editor->widget()):
	//int visibleX = (contentsX() + visibleWidth()) / 2; // Take middle of view, to not scroll in X
//	ensureVisible( note->rightLimit(), y + height, 0,0 ); // Bottom-right corner
//	ensureVisible( x,                  y,          0,0 ); // Top-left corner
	if (andEnsureVisible)
		ensureNoteVisible(note);
}

void Basket::closeEditorDelayed()
{
	setFocus();
	QTimer::singleShot( 0, this, SLOT(closeEditor()) );
}

void Basket::closeEditor()
{
	if (!isDuringEdit())
		return;

	if (m_redirectEditActions) {
		disconnect( m_editor->widget(), SIGNAL(selectionChanged()), this, SLOT(selectionChangedInEditor()) );
		if (m_editor->textEdit()) {
			disconnect( m_editor->textEdit(), SIGNAL(textChanged()),               this, SLOT(selectionChangedInEditor()) );
			disconnect( m_editor->textEdit(), SIGNAL(textChanged()),               this, SLOT(contentChangedInEditor())   );
		} else if (m_editor->lineEdit()) {
			disconnect( m_editor->lineEdit(), SIGNAL(textChanged(const QString&)), this, SLOT(selectionChangedInEditor()) );
			disconnect( m_editor->lineEdit(), SIGNAL(textChanged(const QString&)), this, SLOT(contentChangedInEditor())   );
		}
	}
	m_editor->widget()->disconnect();
	m_editor->widget()->hide();
	m_editor->validate();

	delete m_leftEditorBorder;
	delete m_rightEditorBorder;
	m_leftEditorBorder  = 0;
	m_rightEditorBorder = 0;

	Note *note = m_editor->note();
	note->setWidth(0); // For relayoutNotes() to succeed to take care of the change

	// Delete the editor BEFORE unselecting the note because unselecting the note would trigger closeEditor() recursivly:
	bool isEmpty = m_editor->isEmpty();
	delete m_editor;
	m_editor = 0;
	m_redirectEditActions = false;
	m_editorWidth  = -1;
	m_editorHeight = -1;
	m_inactivityAutoSaveTimer.stop();

	// Delete the note if it is now empty:
	if (isEmpty) {
		focusANonSelectedNoteAbove();
		focusANonSelectedNoteBelow();
		note->setSelected(true);
		note->deleteSelectedNotes();
		save();
		note = 0;
	}

	unlockHovering();
	filterAgain();

// Does not work:
//	if (Settings::playAnimations())
//		note->setOnTop(true); // So if it grew, do not obscure it temporarily while the notes below it are moving

	if (note)
		note->setSelected(false);//unselectAll();
	doHoverEffects();
//	save();

	Global::mainContainer->m_actEditNote->setEnabled( !isLocked() && countSelecteds() == 1 /*&& !isDuringEdit()*/ );

	emit resetStatusBarText(); // Remove the "Editing. ... to validate." text.

	//if (kapp->activeWindow() == Global::mainContainer)

	// Set focus to the basket, unless the user pressed a letter key in the filter bar and the currently edited note came hidden, then editing closed:
	if (!decoration()->filterBar()->lineEdit()->hasFocus())
		setFocus();
}

Note* Basket::theSelectedNote()
{
	if (countSelecteds() != 1) {
		std::cout << "NO SELECTED NOTE !!!!" << std::endl;
		return 0;
	}

	Note *selectedOne;
	FOR_EACH_NOTE (note) {
		selectedOne = note->theSelectedNote();
		if (selectedOne)
			return selectedOne;
	}

	std::cout << "One selected note, BUT NOT FOUND !!!!" << std::endl;

	return 0;
}

void debugSel(NoteSelection* sel, int n = 0)
{
	for (NoteSelection *node = sel; node; node = node->next) {
		for(int i = 0; i < n; i++)
			std::cout << "-";
		std::cout << (node->firstChild ? "Group" : node->note->content()->toText("")) << std::endl;
		if (node->firstChild)
			debugSel(node->firstChild, n+1);
	}
}

NoteSelection* Basket::selectedNotes()
{
	NoteSelection selection;

	FOR_EACH_NOTE (note)
		selection.append(note->selectedNotes());

	if (!selection.firstChild)
		return 0;

	for (NoteSelection *node = selection.firstChild; node; node = node->next)
		node->parent = 0;

	// If the top-most groups are columns, export only childs of those groups
	// (because user is not consciencious that columns are groups, and don't care: it's not what she want):
	if (selection.firstChild->note->isColumn()) {
		NoteSelection tmpSelection;
		NoteSelection *nextNode;
		for (NoteSelection *node = selection.firstChild; node; node = node->next) {
			for (NoteSelection *subNode = node->firstChild; subNode; subNode = nextNode) {
				nextNode = subNode->next;
				tmpSelection.append(subNode);
				subNode->parent = 0;
				subNode->next = 0;
			}
		}
//		debugSel(tmpSelection.firstChild);
		return tmpSelection.firstChild;
	} else {
//		debugSel(selection.firstChild);
		return selection.firstChild;
	}
}

void Basket::noteEdit(Note *note, bool justAdded, const QPoint &clickedPoint) // TODO: Remove the first parameter!!!
{
	if (!note)
		note = theSelectedNote(); // TODO: Or pick the focused note!
	if (!note)
		return;

	if (isDuringEdit()) {
		closeEditor(); // Validate the noteeditors in KLineEdit that does not intercept Enter key press (and edit is triggered with Enter too... Can conflict)
		return;
	}

	if (note != m_focusedNote) {
		setFocusedNote(note);
		m_startOfShiftSelectionNote = note;
	}

	doHoverEffects(note, Note::Content); // Be sure (in the case Edit was triggered by menu or Enter key...): better feedback!
	//m_lockedHovering = true;

	//m_editorWidget = note->content()->launchEdit(this);
	NoteEditor *editor = NoteEditor::editNoteContent(note->content(), this);

	if (editor->widget()) {
		m_editor = editor;
		m_leftEditorBorder  = new TransparentWidget(this);
		m_rightEditorBorder = new TransparentWidget(this);
		m_editor->widget()->reparent(viewport(), QPoint(0,0), true);
		m_leftEditorBorder->reparent(viewport(), QPoint(0,0), true);
		m_rightEditorBorder->reparent(viewport(), QPoint(0,0), true);
		addChild(m_editor->widget(), 0, 0);
		placeEditorAndEnsureVisible(); //       placeEditor(); // FIXME: After?
		m_redirectEditActions = m_editor->lineEdit() || m_editor->textEdit();
		if (m_redirectEditActions) {
			connect( m_editor->widget(), SIGNAL(selectionChanged()), this, SLOT(selectionChangedInEditor()) );
			// In case there is NO text, "Select All" is disabled. But if the user press a key the there is now a text:
			// selection has not changed but "Select All" should be re-enabled:
			if (m_editor->textEdit()) {
				connect( m_editor->textEdit(), SIGNAL(textChanged()),               this, SLOT(selectionChangedInEditor()) );
				connect( m_editor->textEdit(), SIGNAL(textChanged()),               this, SLOT(contentChangedInEditor())   );
			} else if (m_editor->lineEdit()) {
				connect( m_editor->lineEdit(), SIGNAL(textChanged(const QString&)), this, SLOT(selectionChangedInEditor()) );
				connect( m_editor->lineEdit(), SIGNAL(textChanged(const QString&)), this, SLOT(contentChangedInEditor())   );
			}
		}
		m_editor->widget()->show();
		//m_editor->widget()->raise();
		m_editor->widget()->setFocus();
		connect( m_editor, SIGNAL(askValidation()), this, SLOT(closeEditorDelayed()) );
		if (m_editor->textEdit()) {
			connect( m_editor->textEdit(), SIGNAL(textChanged()), this, SLOT(placeEditorAndEnsureVisible()) );
			if (clickedPoint != QPoint()) {
				QPoint pos(clickedPoint.x() - note->x() - note->contentX() + m_editor->textEdit()->frameWidth() + 4   - m_editor->textEdit()->frameWidth(),
				           clickedPoint.y() - note->y()   - m_editor->textEdit()->frameWidth());
				// Do it right before the kapp->processEvents() to not have the cursor to quickly flicker at end (and sometimes stay at end AND where clicked):
				m_editor->textEdit()->moveCursor(KTextEdit::MoveHome, false);
				m_editor->textEdit()->ensureCursorVisible();
				m_editor->textEdit()->placeCursor(pos);
				updateEditorAppearance();
			}
		}
		kapp->processEvents();     // Show the editor toolbar before ensuring the note is visible
		ensureNoteVisible(note);   //  because toolbar can create a new line and then partially hide the note
		m_editor->widget()->setFocus(); // When clicking in the basket, a QTimer::singleShot(0, ...) focus the basket! So we focus the the widget after kapp->processEvents()
		emit resetStatusBarText(); // Display "Editing. ... to validate."
	} else {
		// Delete the note user have canceled the addition:
		if ((justAdded && editor->canceled()) || editor->isEmpty() /*) && editor->note()->states().count() <= 0*/) {
			focusANonSelectedNoteAbove();
			focusANonSelectedNoteBelow();
			editor->note()->setSelected(true);
			editor->note()->deleteSelectedNotes();
			save();
		}
		delete editor;
		unlockHovering();
		filterAgain();
		unselectAll();
	}
	Global::mainContainer->m_actEditNote->setEnabled(false);
}

void Basket::noteDelete()
{
	if (redirectEditActions()) {
		if (m_editor->textEdit())
			m_editor->textEdit()->del();
		else if (m_editor->lineEdit())
			m_editor->lineEdit()->del();
		return;
	}

	if (countSelecteds() <= 0)
		return;
	int really = KMessageBox::questionYesNo( this,
		i18n("<qt>Do you really want to delete this note?</qt>",
		     "<qt>Do you really want to delete those <b>%n</b> notes?</qt>",
		     countSelecteds()),
		i18n("Delete Note", "Delete Notes", countSelecteds())
#if KDE_IS_VERSION( 3, 2, 90 )   // KDE 3.3.x
		, KStdGuiItem::del(), KStdGuiItem::cancel());
#else
		                    );
#endif
	if (really == KMessageBox::No)
		return;

	noteDeleteWithoutConfirmation();
}

void Basket::focusANonSelectedNoteBelow()
{
	// First focus another unselected one below it...:
	if (m_focusedNote && m_focusedNote->isSelected()) {
		Note *next = m_focusedNote->nextShownInStack();
		while (next && next->isSelected())
			next = next->nextShownInStack();
		if (next) {
			setFocusedNote(next);
			m_startOfShiftSelectionNote = next;
		}
	}
}

void Basket::focusANonSelectedNoteAbove()
{
	// ... Or above it:
	if (m_focusedNote && m_focusedNote->isSelected()) {
		Note *prev = m_focusedNote->prevShownInStack();
		while (prev && prev->isSelected())
			prev = prev->prevShownInStack();
		if (prev) {
			setFocusedNote(prev);
			m_startOfShiftSelectionNote = prev;
		}
	}
}

void Basket::noteDeleteWithoutConfirmation(bool deleteFilesToo)
{
	// If the currently focused note is selected, it will be deleted.
	focusANonSelectedNoteBelow();
	focusANonSelectedNoteAbove();

	// Do the deletion:
	Note *note = firstNote();
	Note *next;
	while (note) {
		next = note->next(); // If we delete 'note' on the next line, note->next() will be 0!
		note->deleteSelectedNotes(deleteFilesToo);
		note = next;
	}

	relayoutNotes(true); // FIXME: filterAgain()?
	save();
}

void Basket::doCopy(CopyMode copyMode)
{
	QClipboard *cb = KApplication::clipboard();
	QClipboard::Mode mode = (copyMode == CopyToSelection ? QClipboard::Selection : QClipboard::Clipboard);

	NoteSelection *selection = selectedNotes();
	int countCopied = countSelecteds();
	if (selection->firstStacked()) {
		QDragObject *d = NoteDrag::dragObject(selection, copyMode == CutToClipboard, /*source=*/0); // d will be deleted by QT
//		/*bool shouldRemove = */d->drag();
//		delete selection;
		cb->setData(d, mode); // NoteMultipleDrag will be deleted by QT
//		if (copyMode == CutToClipboard && !note->useFile()) // If useFile(), NoteDrag::dragObject() will delete it TODO
//			note->slotDelete();

		if (copyMode == CutToClipboard)
			noteDeleteWithoutConfirmation(/*deleteFilesToo=*/false);

		switch (copyMode) {
			default:
			case CopyToClipboard: emit postMessage(i18n("Copied note to clipboard.", "Copied notes to clipboard.", countCopied)); break;
			case CutToClipboard:  emit postMessage(i18n("Cut note to clipboard.",    "Cut notes to clipboard.",    countCopied)); break;
			case CopyToSelection: emit postMessage(i18n("Copied note to selection.", "Copied notes to selection.", countCopied)); break;
		}
	}
}

void Basket::noteCopy()
{
	if (redirectEditActions()) {
		if (m_editor->textEdit())
			m_editor->textEdit()->copy();
		else if (m_editor->lineEdit())
			m_editor->lineEdit()->copy();
	} else
		doCopy(CopyToClipboard);
}

void Basket::noteCut()
{
	if (redirectEditActions()) {
		if (m_editor->textEdit())
			m_editor->textEdit()->cut();
		else if (m_editor->lineEdit())
			m_editor->lineEdit()->cut();
	} else
		doCopy(CutToClipboard);
}

void Basket::noteOpen(Note *note)
{
	/*
	GetSelectedNotes
	NoSelectedNote || Count == 0 ? return
	AllTheSameType ?
	Get { url, message(count) }
	*/

	// TODO: Open ALL selected notes!
	if (!note)
		note = theSelectedNote();
	if (!note)
		return;

	KURL    url     = note->content()->urlToOpen(/*with=*/false);
	QString message = note->content()->messageWhenOpenning(NoteContent::OpenOne /*NoteContent::OpenSeveral*/);
	if (url.isEmpty()) {
		if (message.isEmpty())
			emit postMessage(i18n("Unable to open this note.") /*"Unable to open those notes."*/);
		else {
			int result = KMessageBox::warningContinueCancel(this, message, /*caption=*/QString::null, KGuiItem(i18n("&Edit"), "edit"));
			if (result == KMessageBox::Continue)
				noteEdit(note);
		}
	} else {
		emit postMessage(message); // "Openning link target..." / "Launching application..." / "Openning note file..."
		// Finally do the opening job:
		QString customCommand = note->content()->customOpenCommand();
		if (customCommand.isEmpty()) {
			KRun *run = new KRun(url);
			run->setAutoDelete(true);
		} else
			KRun::run(customCommand, url);
	}
}

/** Code from bool KRun::displayOpenWithDialog(const KURL::List& lst, bool tempFiles)
  * It does not allow to set a text, so I ripped it to do that:
  */
bool KRun__displayOpenWithDialog(const KURL::List& lst, bool tempFiles, const QString &text)
{
	if (kapp && !kapp->authorizeKAction("openwith")) {
		KMessageBox::sorry(0L, i18n("You are not authorized to open this file.")); // TODO: Better message, i18n freeze :-(
		return false;
	}
	KOpenWithDlg l(lst, text, QString::null, 0L);
	if (l.exec()) {
		KService::Ptr service = l.service();
		if (!!service)
			return KRun::run(*service, lst, tempFiles);
		//kdDebug(250) << "No service set, running " << l.text() << endl;
		return KRun::run(l.text(), lst); // TODO handle tempFiles
	}
	return false;
}

void Basket::noteOpenWith(Note *note)
{
	if (!note)
		note = theSelectedNote();
	if (!note)
		return;

	KURL    url     = note->content()->urlToOpen(/*with=*/true);
	QString message = note->content()->messageWhenOpenning(NoteContent::OpenOneWith /*NoteContent::OpenSeveralWith*/);
	QString text    = note->content()->messageWhenOpenning(NoteContent::OpenOneWithDialog /*NoteContent::OpenSeveralWithDialog*/);
	if (url.isEmpty())
		emit postMessage(i18n("Unable to open this note.") /*"Unable to open those notes."*/);
	else if (KRun__displayOpenWithDialog(url, false, text))
		emit postMessage(message); // "Openning link target with..." / "Openning note file with..."
}

void Basket::noteSaveAs()
{
//	if (!note)
//		note = theSelectedNote();
//	if (!note)
//		return;
	Note *note = theSelectedNote();

	KURL url = note->content()->urlToOpen(/*with=*/false);
	if (url.isEmpty())
		return;

	QString fileName = KFileDialog::getSaveFileName(url.fileName(), note->content()->saveAsFilters(), this, i18n("Save to File"));
	// TODO: Ask to overwrite !
	if (fileName.isEmpty())
		return;

	// TODO: Convert format, etc. (use NoteContent::saveAs(fileName))
	KIO::copy(url, KURL(fileName));
}

void Basket::noteGroup()
{
	// Nothing to do?
	if (countSelecteds() <= 1)
		return;

	// Get the first selected note: we will group selected items just before:
	Note *first = 0;
	FOR_EACH_NOTE (note) {
		first = note->firstSelected();
		if (first)
			break;
	}

	m_loaded = false; // Hack to avoid notes to be unselected and new notes to be selected:

	// Create and insert the receiving group:
	Note *group = new Note(this);
	insertNote(group, first, Note::TopInsert, QPoint(), /*animateNewPosition=*/false);

	// Put a FAKE UNSELECTED note in the new group, so if the new group is inside an allSelected() group, the parent group is not moved inside the new group!
	Note *fakeNote = NoteFactory::createNoteColor(Qt::red, this);
	insertNote(fakeNote, group, Note::BottomColumn, QPoint(), /*animateNewPosition=*/false);

	// Group the notes:
	FOR_EACH_NOTE (note)
		note->groupIn(group);

	m_loaded = true; // Part 2 / 2 of the workarround!

	// Do cleanup:
	unplugNote(fakeNote);
	unselectAll();
	group->setSelectedRecursivly(true); // Notes were unselected by unplugging

	relayoutNotes(true);
	save();
}

void Basket::noteUngroup()
{
}

void Basket::noteMoveOnTop()
{
}

void Basket::noteMoveOnBottom()
{
}

void Basket::noteMoveNoteUp()
{
}

void Basket::noteMoveNoteDown()
{
}

void Basket::wheelEvent(QWheelEvent *event)
{
	QScrollView::wheelEvent(event);
}

void Basket::linkLookChanged()
{
	Note *note = m_firstNote;
	while (note) {
		note->linkLookChanged();
		note = note->next();
	}
	relayoutNotes(true);
}


void Basket::slotCopyingDone(KIO::Job *, const KURL &, const KURL &to, bool, bool)
{
	Note *note = noteForFullPath(to.path());
	DEBUG_WIN << "Copy finished, load note: " + to.path() + (note ? "" : " --- NO CORRESPONDING NOTE");
	if (note != 0L) {
		note->content()->loadFromFile();
		if (m_focusedNote == note)   // When inserting a new note we ensure it visble
			ensureNoteVisible(note); //  But after loading it has certainly grown and if it was
	}                                //  on bottom of the basket it's not visible entirly anymore
}

void Basket::slotCopyingDone2(KIO::Job *job)
{
	if (job->error()) {
		DEBUG_WIN << "Copy finished, ERROR";
		return;
	}
	KIO::FileCopyJob *fileCopyJob = (KIO::FileCopyJob*)job;
	Note *note = noteForFullPath(fileCopyJob->destURL().path());
	DEBUG_WIN << "Copy finished, load note: " + fileCopyJob->destURL().path() + (note ? "" : " --- NO CORRESPONDING NOTE");
	if (note != 0L) {
		note->content()->loadFromFile();
		if (m_focusedNote == note)   // When inserting a new note we ensure it visble
			ensureNoteVisible(note); //  But after loading it has certainly grown and if it was
	}                                //  on bottom of the basket it's not visible entirly anymore
}

Note* Basket::noteForFullPath(const QString &path)
{
	Note *note = firstNote();
	Note *found;
	while (note) {
		found = note->noteForFullPath(path);
		if (found)
			return found;
		note = note->next();
	}
	return 0;
}

void Basket::deleteFiles()
{
//	m_watcher->stopScan();
	Tools::deleteRecursively(fullPath());
}

/** Save an icon to a folder.
  * If an icon with the same name already exist in the destination,
  * it is assumed the icon is already copied, so no action is took.
  * It is optimized so that you can have an empty folder receiving the icons
  * and call copyIcon() each time you encounter one during export process.
  */
QString Basket::copyIcon(const QString &iconName, int size, const QString &destFolder)
{
	if (iconName.isEmpty())
		return "";

	// Sometimes icon can be "favicons/www.kde.org", we replace the '/' with a '_'
	QString fileName = iconName; // QString::replace() isn't const, so I must copy the string before
	fileName = "ico" + QString::number(size) + "_" + fileName.replace("/", "_") + ".png";
	QString fullPath = destFolder + fileName;
	if (!QFile::exists(fullPath))
		DesktopIcon(iconName, size).save(fullPath, "PNG");
	return fileName;
}

/** Done: Sometimes we can call two times copyFile() with the same srcPath and destFolder
  *       (eg. when exporting basket to HTML with two links to same filename
  *            (but not necesary same path, as in "/home/foo.txt" and "/foo.txt") )
  *       The first copy isn't yet started, so the dest file isn't created and this method
  *       returns the same filename !!!!!!!!!!!!!!!!!!!!
  */
QString Basket::copyFile(const QString &srcPath, const QString &destFolder, bool createIt)
{
	QString fileName = Tools::fileNameForNewFile(KURL(srcPath).fileName(), destFolder);
	QString fullPath = destFolder + fileName;

	if (createIt) {
		// We create the file to be sure another very near call to copyFile() willn't choose the same name:
		QFile file(KURL(fullPath).path());
		if (file.open(IO_WriteOnly))
			file.close();
		// And then we copy the file AND overwriting the file we juste created:
		new KIO::FileCopyJob(
			KURL(srcPath), KURL(fullPath), 0666, /*move=*/false,
			/*overwrite=*/true, /*resume=*/true, /*showProgress=*/true );
	} else
		/*KIO::CopyJob *copyJob = */KIO::copy(KURL(srcPath), KURL(fullPath)); // Do it as before

	return fileName;
}

QValueList<State*> Basket::usedStates()
{
	QValueList<State*> states;
	FOR_EACH_NOTE (note)
		note->usedStates(states);
	return states;
}

QString Basket::saveGradientBackground(const QColor &color, const QFont &font, const QString &folder)
{
	// Construct file name and return if the file already exists:
	QString fileName = "note_background_" + color.name().lower().mid(1) + ".png";
	QString fullPath = folder + fileName;
	if (QFile::exists(fullPath))
		return fileName;

	// Get the gradient top and bottom colors:
	QColor topBgColor;
	QColor bottomBgColor;
	Note::getGradientColors(color, &topBgColor, &bottomBgColor);

	// Draw and save the gradient image:
	int sampleTextHeight = QFontMetrics(font)
	                       .boundingRect(0, 0, /*width=*/10000, /*height=*/0, Qt::AlignAuto | Qt::AlignTop | Qt::WordBreak, "Test text")
	                       .height();
	QPixmap noteGradient(100, sampleTextHeight + Note::NOTE_MARGIN);
	QPainter painter(&noteGradient);
	drawGradient(&painter, topBgColor, bottomBgColor, 0, 0, noteGradient.width(), noteGradient.height(), /*sunken=*/false, /*horz=*/true, /*flat=*/false);
	painter.end();
	noteGradient.save(fullPath, "PNG");

	// Return the name of the created file:
	return fileName;
}

void Basket::exportToHTML()
{
	/*KMessageBox::information(this, "Sorry, this action isn't re-available yet. It need to be adapted to the new BasKet Note Pads capabilities.");*/

	// Get the HTML filename:
	ExporterDialog *options = new ExporterDialog(this);
	if (options->exec() == QDialog::Rejected)
		return;

	// TODO: options->erasePreviousFiles() is not used: on pressing OK, a messagebox should show:
	//       "A folder named foobar_files already exist. This folder will contains the data files and images.
	//        Should it be erased?
	//        [Erase] [Cancel]"
	// TODO: options->formatForImpression() was used to put a line between notes.
	//       It's now only used to put URL in front of links. TODO: Use @media print { } for that!

	// Open the file to write:
	QString filePath = options->filePath();
	QFile file(filePath);
	if ( ! file.open(IO_WriteOnly) )
		return;
	QTextStream stream(&file);
	stream.setEncoding(QTextStream::UnicodeUTF8);

	// Create and empty the folder:
	QString filesFolderPath = i18n("HTML export folder (files)", "%1_files").arg(filePath) + "/";                  // eg.: "/home/seb/foo.html_files/"
	QString filesFolderName = i18n("HTML export folder (files)", "%1_files").arg(KURL(filePath).fileName()) + "/"; // eg.: "foo.html_files/"
	Tools::deleteRecursively(filesFolderPath);
	QDir dir;
	dir.mkdir(filesFolderPath);

	// Create sub-folders:
	QString iconsFolderPath  = filesFolderPath + i18n("HTML export folder (icons)",  "icons")  + "/"; // eg.: "/home/seb/foo.html_files/icons/"
	QString iconsFolderName  = filesFolderName + i18n("HTML export folder (icons)",  "icons")  + "/"; // eg.: "foo.html_files/icons/"
	QString imagesFolderPath = filesFolderPath + i18n("HTML export folder (images)", "images") + "/"; // eg.: "/home/seb/foo.html_files/images/"
	QString imagesFolderName = filesFolderName + i18n("HTML export folder (images)", "images") + "/"; // eg.: "foo.html_files/images/"
	QString dataFolderPath   = filesFolderPath + i18n("HTML export folder (data)",   "data")   + "/"; // eg.: "/home/seb/foo.html_files/data/"
	QString dataFolderName   = filesFolderName + i18n("HTML export folder (data)",   "data")   + "/"; // eg.: "foo.html_files/data/"
	dir.mkdir(iconsFolderPath);
	dir.mkdir(imagesFolderPath);
	dir.mkdir(dataFolderPath);

	// Generate basket icons:
	QString basketIcon16 = iconsFolderName + copyIcon(icon(), 16, iconsFolderPath);
	QString basketIcon32 = iconsFolderName + copyIcon(icon(), 32, iconsFolderPath);
	// Generate the [+] image for groups:
	QPixmap expandGroup(Note::EXPANDER_WIDTH, Note::EXPANDER_HEIGHT);
	expandGroup.fill(backgroundColor());
	QPainter painter(&expandGroup);
	Note::drawExpander(&painter, 0, 0, backgroundColor(), /*expand=*/true, this);
	painter.end();
	expandGroup.save(imagesFolderPath + "expand_group.png", "PNG");
	// Generate the [-] image for groups:
	QPixmap foldGroup(Note::EXPANDER_WIDTH, Note::EXPANDER_HEIGHT);
	foldGroup.fill(backgroundColor());
	painter.begin(&foldGroup);
	Note::drawExpander(&painter, 0, 0, backgroundColor(), /*expand=*/false, this);
	painter.end();
	foldGroup.save(imagesFolderPath + "fold_group.png", "PNG");
	// Compute the colors to draw dragient for notes:
	QColor topBgColor;
	QColor bottomBgColor;
	Note::getGradientColors(backgroundColor(), &topBgColor, &bottomBgColor);
	// Compute the gradient image for notes:
	QString gradientImageFileName = saveGradientBackground(backgroundColor(), QScrollView::font(), imagesFolderPath);

	// Output the header:
	QString borderColor = Tools::mixColor(backgroundColor(), textColor()).name();
	stream <<
		"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\n"
		"         \"http://www.w3.org/TR/html4/strict.dtd\">\n"
		"<html>\n"
		" <head>\n"
		"  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n"
		"  <meta name=\"Generator\" content=\"" << kapp->aboutData()->programName() << " " << VERSION << " http://basket.kde.org/\">\n"
		"  <style type=\"text/css\">\n"
		"   h1 { text-align: center; }\n"
		"   img { border: none; vertical-align: middle; }\n"
		"   .basket { background-color: " << backgroundColor().name() << "; border: solid " << borderColor << " 1px; "
		             "font: " << Tools::cssFontDefinition(QScrollView::font()) << "; color: " << textColor().name() << "; padding: 1px; width: 100%; }\n"
		"   table.basket { border-collapse: collapse; }\n"
		"   .basket * { padding: 0; margin: 0; }\n"
		"   .basket table { width: 100%; border-spacing: 0; _border-collapse: collapse; }\n"
		"   .column { vertical-align: top; }\n"
		"   .columnHandle { width: " << Note::RESIZER_WIDTH << "px; background: transparent url('" << imagesFolderName << "column_handle.png') repeat-y; }\n"
		"   .group { margin: 0; padding: 0; border-collapse: collapse; width: 100% }\n"
		"   .groupHandle { margin: 0; width: " << Note::GROUP_WIDTH << "px; text-align: center; }\n"
		"   .note { padding: 1px 2px; background: " << bottomBgColor.name() << " url('" << imagesFolderName << gradientImageFileName << "')"
		          " repeat-x; border-top: solid " << topBgColor.name() <<
		          " 1px; border-bottom: solid " << Tools::mixColor(topBgColor, bottomBgColor).name() <<
		          " 1px; width: 100%; }\n"
		"   .tags { width: 1px; white-space: nowrap; }\n"
		"   .tags img { padding-right: 2px; }\n"
		<< LinkLook::soundLook->toCSS("sound", textColor())
		<< LinkLook::fileLook->toCSS("file", textColor())
		<< LinkLook::localLinkLook->toCSS("local", textColor())
		<< LinkLook::networkLinkLook->toCSS("network", textColor())
		<< LinkLook::launcherLook->toCSS("launcher", textColor())
		<<
		"   .unknown { margin: 1px 2px; border: 1px solid " << borderColor << "; -moz-border-radius: 4px; }\n";
	QValueList<State*> states = usedStates();
	QString statesCss;
	for (State::List::Iterator it = states.begin(); it != states.end(); ++it)
		statesCss += (*it)->toCSS(imagesFolderPath, imagesFolderName, QScrollView::font());
	stream <<
		statesCss <<
		"   .credits { margin: 3px 0 0 0; font-size: 80%; color: " << borderColor << "; }\n"
		"  </style>\n"
		"  <title>" << Tools::textToHTMLWithoutP(basketName()) << "</title>\n"
		"  <link rel=\"shortcut icon\" type=\"image/png\" href=\"" << basketIcon16 << "\">\n";
	// Create the column handle image:
	QPixmap columnHandle(Note::RESIZER_WIDTH, 50);
	painter.begin(&columnHandle);
	Note::drawInactiveResizer(&painter, 0, 0, columnHandle.height(), backgroundColor(), /*column=*/true);
	painter.end();
	columnHandle.save(imagesFolderPath + "column_handle.png", "PNG");
	//
	// Copy a transparent GIF image in the folder, needed for the JavaScript hack:
	QString gifFileName = "spacer.gif";
	QFile transGIF(imagesFolderPath + gifFileName);
	if (transGIF.open(IO_WriteOnly)) {
		QDataStream streamGIF(&transGIF);
		// This is a 1px*1px transparent GIF image:
		const uchar blankGIF[] = {
			0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x0a, 0x00, 0x0a, 0x00,
			0x80, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x21,
			0xfe, 0x15, 0x43, 0x72, 0x65, 0x61, 0x74, 0x65, 0x64, 0x20,
			0x77, 0x69, 0x74, 0x68, 0x20, 0x54, 0x68, 0x65, 0x20, 0x47,
			0x49, 0x4d, 0x50, 0x00, 0x21, 0xf9, 0x04, 0x01, 0x0a, 0x00,
			0x01, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x0a,
			0x00, 0x00, 0x02, 0x08, 0x8c, 0x8f, 0xa9, 0xcb, 0xed, 0x0f,
			0x63, 0x2b, 0x00, 0x3b };
		streamGIF.writeRawBytes((const char*)blankGIF, (unsigned int)74);
		transGIF.close();
		stream <<
			"  <!--[if gte IE 5.5000]>\n"
			"  <script>\n"
			"   window.attachEvent(\"onload\", pngFix);\n"
			"   function pngFix() {\n"
			"    for (i = document.images.length - 1; i >= 0; i--) {\n"
			"     x = document.images[i];\n"
			"     if (x.src.substr(x.src.length - 4) == \".png\") {\n"
			"      x.style.filter = \"progid:DXImageTransform.Microsoft.AlphaImageLoader(src=\'\"+x.src+\"')\"\n"
			"      x.src = \"" << imagesFolderName << gifFileName << "\";\n"
			"     }\n"
			"    }\n"
			"   }\n"
			"  </script>\n"
			"  <![endif]-->\n";
	} // else: do not provide IE compatibility
	stream <<
		" </head>\n"
		" <body>\n"
		"  <h1><img src=\"" << basketIcon32 << "\" width=\"32\" height=\"32\" alt=\"\"> " << Tools::textToHTMLWithoutP(basketName()) << "</h1>\n";

	// If filtering, only export filtered notes, inform to the user:
	// TODO: Filtering tags too!!
	// TODO: Make sure only filtered notes are exported!
	if (decoration()->filterData().isFiltering)
		stream <<
			"  <p>" << i18n("Notes matching the filter &quot;%1&quot;:").arg(Tools::textToHTMLWithoutP(decoration()->filterData().string)) << "</p>\n";

	if (isColumnsLayout())
		stream <<
			"  <table class=\"basket\">\n"
			"   <tr>\n";
	else
		stream <<
			"  <div class=\"basket\" style=\"position: relative; height: " << contentsHeight() << "px; width: " << contentsWidth() << "px; min-width: 100%;\">\n";

	// output the body:
	HtmlExportData exportData;
	exportData.iconsFolderPath     = iconsFolderPath;
	exportData.iconsFolderName     = iconsFolderName;
	exportData.imagesFolderPath    = imagesFolderPath;
	exportData.imagesFolderName    = imagesFolderName;
	exportData.dataFolderPath      = dataFolderPath;
	exportData.dataFolderName      = dataFolderName;
	exportData.formatForImpression = options->formatForImpression();
	exportData.embedLinkedFiles    = options->embedLinkedFiles();
	exportData.embedLinkedFolders  = options->embedLinkedFolders();

	FOR_EACH_NOTE (note)
		note->exportToHTML(stream, /*indent=*/(isFreeLayout() ? 3 : 4), exportData);

	// Output the footer:
	if (isColumnsLayout())
		stream <<
			"   </tr>\n"
			"  </table>\n";
	else
		stream <<
			"  </div>\n";
	stream << QString(
		"  <p class=\"credits\">%1</p>\n"
		" </body>\n"
		"</html>\n").arg(
			i18n("Made with %1, a KDE tool to take notes and keep a full range of data on hand.")
				.arg("<a href=\"http://basket.kde.org/\">%1</a> %2")
				.arg(kapp->aboutData()->programName(), VERSION));
	file.close();
}







/** Unfocus the previously focused note (unless it was null)
  * and focus the new @param note (unless it is null) if hasFocus()
  * Update m_focusedNote to the new one
  */
void Basket::setFocusedNote(Note *note) // void Basket::changeFocusTo(Note *note)
{
	// Don't focus an hidden note:
	if (note != 0L && !note->isShown())
		return;
	// When clicking a group, this group gets focused. But only content-based notes should be focused:
	if (note && note->isGroup())
		note = note->firstRealChild();
	// The first time a note is focused, it becomes the start of the Shift selection:
	if (m_startOfShiftSelectionNote == 0)
		m_startOfShiftSelectionNote = note;
	// Unfocus the old focused note:
	if (m_focusedNote != 0L)
		m_focusedNote->setFocused(false);
	// Notify the new one to draw a focus rectangle... only if the basket is focused:
	if (hasFocus() && note != 0L)
		note->setFocused(true);
	// Save the new focused note:
	m_focusedNote = note;
}

/** If no shown note is currently focused, try to find a shown note and focus it
  * Also update m_focusedNote to the new one (or null if there isn't)
  */
void Basket::focusANote()
{
	if (countFounds() == 0) { // No note to focus
		setFocusedNote(0L);
//		m_startOfShiftSelectionNote = 0;
		return;
	}

	if (m_focusedNote == 0L) { // No focused note yet : focus the first shown
		Note *toFocus = (isFreeLayout() ? noteOnHome() : firstNoteShownInStack());
		setFocusedNote(toFocus);
//		m_startOfShiftSelectionNote = m_focusedNote;
		return;
	}

	// Search a visible note to focus if the focused one isn't shown :
	Note *toFocus = m_focusedNote;
	if (toFocus && !toFocus->isShown())
		toFocus = toFocus->nextShownInStack();
	if (!toFocus && m_focusedNote)
		toFocus = m_focusedNote->prevShownInStack();
	setFocusedNote(toFocus);
//	m_startOfShiftSelectionNote = toFocus;
}

Note* Basket::firstNoteInStack()
{
	if (!firstNote())
		return 0;

	if (firstNote()->content())
		return firstNote();
	else
		return firstNote()->nextInStack();
}

Note* Basket::lastNoteInStack()
{
	Note *note = lastNote();
	while (note) {
		if (note->content())
			return note;
		Note *possibleNote = note->lastRealChild();
		if (possibleNote && possibleNote->content())
			return possibleNote;
		note = note->prev();
	}
	return 0;
}

Note* Basket::firstNoteShownInStack()
{
	Note *first = firstNoteInStack();
	while (first && !first->isShown())
		first = first->nextInStack();
	return first;
}

Note* Basket::lastNoteShownInStack()
{
	Note *last = lastNoteInStack();
	while (last && !last->isShown())
		last = last->prevInStack();
	return last;
}

inline int abs(int n)
{
	return (n < 0 ? -n : n);
}

Note* Basket::noteOn(NoteOn side)
{
	Note *bestNote = 0;
	int   distance = -1;
	int   bestDistance = contentsWidth() * contentsHeight() * 10;

	Note *note    = firstNoteShownInStack();
	Note *primary = m_focusedNote->parentPrimaryNote();
	while (note) {
		switch (side) {
			case LEFT_SIDE:   distance = m_focusedNote->distanceOnLeftRight(note, LEFT_SIDE);   break;
			case RIGHT_SIDE:  distance = m_focusedNote->distanceOnLeftRight(note, RIGHT_SIDE);  break;
			case TOP_SIDE:    distance = m_focusedNote->distanceOnTopBottom(note, TOP_SIDE);    break;
			case BOTTOM_SIDE: distance = m_focusedNote->distanceOnTopBottom(note, BOTTOM_SIDE); break;
		}
		if ((side == TOP_SIDE || side == BOTTOM_SIDE || primary != note->parentPrimaryNote()) && note != m_focusedNote && distance > 0 && distance < bestDistance) {
			bestNote     = note;
			bestDistance = distance;
		}
		note = note ->nextShownInStack();
	}

	return bestNote;
}

Note* Basket::firstNoteInGroup()
{
	Note *child  = m_focusedNote;
	Note *parent = (m_focusedNote ? m_focusedNote->parentNote() : 0);
	while (parent) {
		if (parent->firstChild() != child && !parent->isColumn())
			return parent->firstRealChild();
		child  = parent;
		parent = parent->parentNote();
	}
	return 0;
}

Note* Basket::noteOnHome()
{
	// First try to find the first note of the group containing the focused note:
	Note *child  = m_focusedNote;
	Note *parent = (m_focusedNote ? m_focusedNote->parentNote() : 0);
	while (parent) {
		if (parent->nextShownInStack() != m_focusedNote)
			return parent->nextShownInStack();
		child  = parent;
		parent = parent->parentNote();
	}

	// If it was not found, then focus the very first note in the basket:
	if (isFreeLayout()) {
		Note *first = firstNoteShownInStack(); // The effective first note found
		Note *note  = first; // The current note, to conpare with the previous first note, if this new note is more on top
		if (note)
			note = note->nextShownInStack();
		while (note) {
			if (note->finalY() < first->finalY() || (note->finalY() == first->finalY() && note->finalX() < first->finalX()))
				first = note;
			note = note->nextShownInStack();
		}
		return first;
	} else
		return firstNoteShownInStack();
}

Note* Basket::noteOnEnd()
{
	Note *child     = m_focusedNote;
	Note *parent    = (m_focusedNote ? m_focusedNote->parentNote() : 0);
	Note *lastChild;
	while (parent) {
		lastChild = parent->lastRealChild();
		if (lastChild && lastChild != m_focusedNote) {
			if (lastChild->isShown())
				return lastChild;
			lastChild = lastChild->prevShownInStack();
			if (lastChild && lastChild->isShown() && lastChild != m_focusedNote)
				return lastChild;
		}
		child  = parent;
		parent = parent->parentNote();
	}
	if (isFreeLayout()) {
		Note *last;
		Note *note;
		last = note = firstNoteShownInStack();
		note = note->nextShownInStack();
		while (note) {
			if (note->finalBottom() > last->finalBottom() || (note->finalBottom() == last->finalBottom() && note->finalX() > last->finalX()))
				last = note;
			note = note->nextShownInStack();
		}
		return last;
	} else
		return lastNoteShownInStack();
}


void Basket::keyPressEvent(QKeyEvent *event)
{
	if (isDuringEdit() && event->key() == Qt::Key_Return) {
		//if (m_editor->lineEdit())
		//	closeEditor();
		//else
		m_editor->widget()->setFocus();
	} else if (event->key() == Qt::Key_Escape) {
		if (isDuringEdit())
			closeEditor();
		else if (decoration()->filterData().isFiltering)
			cancelFilter();
	}

	if (countFounds() == 0) {
//		processActionAsYouType(event);
		//event->ignore(); // Important !!
		return;
	}

	if (!m_focusedNote)
		return;

	Note *toFocus = 0L;

	switch (event->key()) {
		case Qt::Key_Down:
			toFocus = (isFreeLayout() ? noteOn(BOTTOM_SIDE) : m_focusedNote->nextShownInStack());
			if (toFocus)
				break;
			scrollBy(0, 30); // This cases do not move focus to another note...
			return;
		case Qt::Key_Up:
			toFocus = (isFreeLayout() ? noteOn(TOP_SIDE) : m_focusedNote->prevShownInStack());
			if (toFocus)
				break;
			scrollBy(0, -30); // This cases do not move focus to another note...
			return;
		case Qt::Key_PageDown:
		//	toFocus = m_focusedNote;
		//	for (int i = 0; i < 10 && toFocus; ++i)
		//		toFocus = (isFreeLayout() ? toFocus->noteOn(BOTTOM_SIDE) : toFocus->nextShownInStack());
			if (isFreeLayout()) {
				Note *lastFocused = m_focusedNote;
				for (int i = 0; i < 10 && m_focusedNote; ++i)
					m_focusedNote = noteOn(BOTTOM_SIDE);
				toFocus = m_focusedNote;
				m_focusedNote = lastFocused;
			} else {
				toFocus = m_focusedNote;
				for (int i = 0; i < 10 && toFocus; ++i)
					toFocus = toFocus->nextShownInStack();
			}
			if (toFocus == 0L)
				toFocus = (isFreeLayout() ? noteOnEnd() : lastNoteShownInStack());
			if (toFocus && toFocus != m_focusedNote)
				break;
			scrollBy(0, visibleHeight() / 2); // This cases do not move focus to another note...
			return;
		case Qt::Key_PageUp:
		//	toFocus = m_focusedNote;
		//	for (int i = 0; i < 10 && toFocus; ++i)
		//		toFocus = (isFreeLayout() ? toFocus->noteOn(TOP_SIDE) : toFocus->prevShownInStack());
			if (isFreeLayout()) {
				Note *lastFocused = m_focusedNote;
				for (int i = 0; i < 10 && m_focusedNote; ++i)
					m_focusedNote = noteOn(TOP_SIDE);
				toFocus = m_focusedNote;
				m_focusedNote = lastFocused;
			} else {
				toFocus = m_focusedNote;
				for (int i = 0; i < 10 && toFocus; ++i)
					toFocus = toFocus->prevShownInStack();
			}
			if (toFocus == 0L)
				toFocus = (isFreeLayout() ? noteOnHome() : firstNoteShownInStack());
			if (toFocus && toFocus != m_focusedNote)
				break;
			scrollBy(0, - visibleHeight() / 2); // This cases do not move focus to another note...
			return;
		case Qt::Key_Home:
			toFocus = noteOnHome();
			break;
		case Qt::Key_End:
			toFocus = noteOnEnd();
			break;
		case Qt::Key_Left:
			if (m_focusedNote->tryFoldParent())
				return;
			if ( (toFocus = noteOn(LEFT_SIDE)) )
				break;
			if ( (toFocus = firstNoteInGroup()) )
				break;
			scrollBy(-30, 0); // This cases do not move focus to another note...
			return;
		case Qt::Key_Right:
			if (m_focusedNote->tryExpandParent())
				return;
			if ( (toFocus = noteOn(RIGHT_SIDE)) )
				break;
			scrollBy(30, 0); // This cases do not move focus to another note...
			return;
		case Qt::Key_Space:  // This case do not move focus to another note...
			if (m_focusedNote) {
				m_focusedNote->setSelected( ! m_focusedNote->isSelected() );
				event->accept();
			} else
				event->ignore();
			return;          // ... so we return after the process
		default:
//			processActionAsYouType(event);
			return;
	}

	if (toFocus == 0L) { // If no direction keys have been pressed OR reached out the begin or end
		event->ignore(); // Important !!
		return;
	}

	if (event->state() & Qt::ShiftButton) { // Shift+arrowKeys selection
		if (m_startOfShiftSelectionNote == 0L)
			m_startOfShiftSelectionNote = toFocus;
		ensureNoteVisible(toFocus); // Important: this line should be before the other ones because else repaint would be done on the wrong part!
		selectRange(m_startOfShiftSelectionNote, toFocus);
		setFocusedNote(toFocus);
		event->accept();
		return;
	} else /*if (toFocus != m_focusedNote)*/ {  // Move focus to ANOTHER note...
		ensureNoteVisible(toFocus); // Important: this line should be before the other ones because else repaint would be done on the wrong part!
		setFocusedNote(toFocus);
		m_startOfShiftSelectionNote = toFocus;
		if ( ! (event->state() & Qt::ControlButton) )       // ... select only current note if Control
			unselectAllBut(m_focusedNote);
		event->accept();
		return;
	}

	event->ignore(); // Important !!
}

/** Select a range of notes and deselect the others.
  * The order between start and end has no importance (end could be before start)
  */
void Basket::selectRange(Note *start, Note *end, bool unselectOthers /*= true*/)
{
	Note *cur;
	Note *realEnd = 0L;

	// Avoid crash when start (or end) is null
	if (start == 0L)
		start = end;
	else if (end == 0L)
		end = start;
	// And if *both* are null
	if (start == 0L) {
		if (unselectOthers)
			unselectAll();
		return;
	}
	// In case there is only one note to select
	if (start == end) {
		if (unselectOthers)
			unselectAllBut(start);
		else
			start->setSelected(true);
		return;
	}

	// Free layout baskets should select range as if we were drawing a rectangle between start and end:
	if (isFreeLayout()) {
		QRect startRect( start->finalX(), start->finalY(), start->width(), start->finalHeight() );
		QRect endRect(     end->finalX(),   end->finalY(),   end->width(),   end->finalHeight() );
		QRect toSelect = startRect.unite(endRect);
		selectNotesIn(toSelect, /*invertSelection=*/false, unselectOthers);
		return;
	}

	// Search the REAL first (and deselect the others before it) :
	for (cur = firstNoteInStack(); cur != 0L; cur = cur->nextInStack()) {
		if (cur == start || cur == end)
			break;
		if (unselectOthers)
			cur->setSelected(false);
	}

	// Select the notes after REAL start, until REAL end :
	if (cur == start)
		realEnd = end;
	else if (cur == end)
		realEnd = start;

	for (/*cur = cur*/; cur != 0L; cur = cur->nextInStack()) {
		cur->setSelected(cur->isShown()); // Select all notes in the range, but only if they are shown
		if (cur == realEnd)
			break;
	}

	if (!unselectOthers)
		return;

	// Deselect the remaining notes :
	if (cur)
		cur = cur->nextInStack();
	for (/*cur = cur*/; cur != 0L; cur = cur->nextInStack())
		cur->setSelected(false);
}

void Basket::focusInEvent(QFocusEvent*)
{
//	if (isDuringEdit()) // It arrive toolbar of another editor stay shown...
//		closeEditor();

	focusANote();      // hasFocus() is true at this stage, note will be focused
}

void Basket::focusOutEvent(QFocusEvent*)
{
	if (m_focusedNote != 0L)
		m_focusedNote->setFocused(false);
}

void Basket::ensureNoteVisible(Note *note)
{
	if (!note->isShown()) // Logical!
		return;

	int finalBottom = note->finalY() + QMIN(note->finalHeight(),                                             visibleHeight());
	int finalRight  = note->finalX() + QMIN(note->width() + (note->hasResizer() ? Note::RESIZER_WIDTH : 0),  visibleWidth());
	ensureVisible( finalRight,     finalBottom,    0,0 );
	ensureVisible( note->finalX(), note->finalY(), 0,0 );
}






#if 0

#include <qlayout.h>
#include <qvbox.h>
#include <qstring.h>
#include <qpixmap.h>
#include <qcolor.h>
#include <kpopupmenu.h>
#include <kurllabel.h>
#include <qcheckbox.h>
#include <qpalette.h>
#include <qcursor.h>
#include <qaction.h>
#include <kstdaccel.h>
#include <kglobalsettings.h>
#include <qevent.h>

#include <kapplication.h>
#include <kaboutdata.h>
#include <qinputdialog.h>
#include <qdragobject.h>
#include <kurldrag.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kmimetype.h>
#include <kfiledialog.h>
#include <qdir.h>
#include <kiconloader.h>
#include <qregexp.h>
#include <qfileinfo.h>

#include <qstringlist.h>
#include <qdir.h>
#include <kurl.h>
#include <krun.h>
#include <kmessagebox.h>
#include <kdeversion.h>

#include "kdirwatch.h"
#include <qstringlist.h>
#include <klineedit.h>

#include <config.h>
#include <qtextcodec.h>

#include "basket.h"
#include "note.h"
#include "notefactory.h"
#include "variouswidgets.h"
#include "linklabel.h"
#include "global.h"
#include "container.h"
#include "xmlwork.h"
#include "settings.h"
#include "popupmenu.h"
#include "debugwindow.h"
#include "exporterdialog.h"


/** Basket */

const int Basket::c_updateTime = 200;

Basket::Basket(QWidget *parent, const QString &folderName, const char *name, WFlags fl)
 : QScrollView(parent, name, fl)
{
//...
	// Create m_watcher before load() because mirrored files should be added to it
	m_watcher = new KDirWatch(this);
	m_watcher->addDir(fullPath(), true); // Watch all files modifications
	connect( m_watcher,     SIGNAL(dirty(const QString&)),   this, SLOT(slotModifiedFile(const QString&)) );
	connect( m_watcher,     SIGNAL(created(const QString&)), this, SLOT(slotCreatedFile(const QString&))  );
	connect( m_watcher,     SIGNAL(deleted(const QString&)), this, SLOT(slotDeletedFile(const QString&))  );
	connect(&m_updateTimer, SIGNAL(timeout()),               this, SLOT(slotUpdateNotes())                );

	m_watcher->stopScan();
	load(); // We disable Dir scan during load, in case files are created (when importing launchers)
	m_watcher->startScan();
	resetFilter();
}





// Remove the note from the basket and delete the associated file
// If the note mirror a file, it will ask before deleting or not the file
// But if askForMirroredFile is false, it willn't ask NOR delete the MIRRORED file
//  (it will anyway delete the file if it is not a mirror)
void Basket::delNote(Note *note, bool askForMirroredFile)
{
//...
	if (hasFocus())
		focusANote();       // We need note->next() and note->previous() here [BUT deleted note should be hidden]
	if (note->isSelected())
		note->setSelected(false); //removeSelectedNote();

	relayoutNotes();
	recolorizeNotes();
	resetInsertTo();         // If we delete the first or the last, pointer to it is invalid
	save();

	if (note == m_startOfShiftSelectionNote)
		m_startOfShiftSelectionNote = 0L;

	if (isDuringEdit() && m_editor->editedNote() == note)
		closeEditor(false);
//...
}


// Calculate where to paste or drop
void Basket::computeInsertPlace(const QPoint &cursorPosition)
{
	int y = cursorPosition.y();

	if (countShown() == 0)
		return;

	// TODO: Memorize the last hovered note to avoid a new computation on dragMoveEvent !!
	//       If the mouse is not over the last note, compute which new is :
	// TODO: Optimization : start from m_insertAtNote and compare y position to search before or after (or the same)
	for (Note *it = firstNote(); it != 0L; it = it->next())
		if ( (it->isShown()) && (it->y() + it->height() >= y) && (it->y() < y) ) {
			int center = it->y() + (it->height() / 2);
			m_insertAtNote = it;
			m_insertAfter  = y > center;
			return;
		}
	// Else, there is at least one shown note but cursor hover NO note, so we are after the last shown note
	m_insertAtNote = lastShownNote();
	m_insertAfter  = true;

	// Code for rectangular notes :
	/*QRect globalRect = it->rect();
	globalRect.moveTopLeft(it->pos() + contentsY());
	if ( globalRect.contains(curPos) ) {
		it->doInterestingThing();
	}*/
}

void Basket::dragMoveEvent(QDragMoveEvent* event)
{
//	m_isDuringDrag = true;

	if (isLocked())
		return;

//	FIXME: viewportToContents does NOT work !!!
//	QPoint pos = viewportToContents(event->pos());
	QPoint pos( event->pos().x() + contentsX(), event->pos().y() + contentsY() );

//	if (insertAtCursorPos())
		computeInsertPlace(pos);

	showFrameInsertTo();
	acceptDropEvent(event);

	// A workarround since QScrollView::dragAutoScroll seem to have no effect :
	ensureVisible(event->pos().x() + contentsX(), event->pos().y() + contentsY(), 30, 30);
//	QScrollView::dragMoveEvent(event);
}

void Basket::dropEvent(QDropEvent *event)
{
	m_isDuringDrag = false;
	emit resetStatusBarText();

	if (isLocked())
		return;

	NoteFactory::dropNote( event, this, true, event->action(), dynamic_cast<Note*>(event->source()) );
	// TODO: need to know if we really inserted an (or several!!!!) note !!!
	ensureNoteVisible(lastInsertedNote());
	unselectAllBut(lastInsertedNote());
	setFocusedNote(lastInsertedNote());

	resetInsertTo();
}


void Basket::contentsMousePressEvent(QMouseEvent *event)
{
	if (event->button() & Qt::LeftButton) { // Clicked an empty area of the basket
		if ( ! isDuringEdit() ) // Do nothing when clicked in edit mode
			unselectAll();
		setFocus();
	}
	// if MidButton BUT NOT ALT PRESSED to allow Alt+middleClick to launch actions
	//              Because KWin handle Alt+click : it's allow an alternative to alt+click !
	if ( (event->button() & Qt::MidButton) && (event->state() == 0) ) {
//		if (insertAtCursorPos())
			computeInsertPlace(event->pos());
		pasteNote(QClipboard::Selection);
		event->accept();
	} else if ( (Settings::middleAction() != 0) &&
	            (event->button() & Qt::MidButton) && (event->state() == Qt::ShiftButton) ) {
//		if (insertAtCursorPos())
			computeInsertPlace(event->pos());
		// O:Nothing ; 1:Paste ; 2:Text ; 3:Html ; 4:Image ; 5:Link ; 6:Launcher ; 7:Color
		// TODO: 8:Ask (with a popup menu)
		Note::Type type = (Note::Type)0;
		switch (Settings::middleAction()) {
			case 1: pasteNote();           break;
			case 2: type = Note::Text;     break;
			case 3: type = Note::Html;     break;
			case 4: type = Note::Image;    break;
			case 5: type = Note::Link;     break;
			case 6: type = Note::Launcher; break;
			case 7: type = Note::Color;    break;
		}
		if (type != 0)
			Global::mainContainer->insertEmpty(type);
		event->accept();
	}
}



void Basket::processActionAsYouType(QKeyEvent *event)
{
	if (Global::debugWindow)
		*Global::debugWindow << QString("Key press (%1)").arg(event->text());

	if ( (kapp->focusWidget() != this) ||    // Do not receive child keyPress events
	     (event->key() == Qt::Key_Escape) || // This key generate a text() !
	     (event->text().isEmpty()) ||        // It should be a text (not a modifier...)
	     (event->state() & AltButton) ||     // If user pressed an unexisting shortcut
	     (event->state() & MetaButton) ||    //  or accelerator key, do nothing
	     ((event->state() & ControlButton) && (event->key() != Qt::Key_Backspace)) ) // But allow Ctrl+BreakSpace
		return;                                                                        //  for filter bar

	DecoratedBasket *decoration = this->decoration();
	FilterBar       *filterBar  = decoration->filterBar();

	int  action         = Settings::writingAction();
	bool takeCareOfText = true;
	if ( event->text().startsWith(",") &&
	     (Settings::writingCommaAction() != 0) && // Do the same, so don't process the ',' as a special case!
		 (Settings::writingCommaAction() != Settings::writingAction()) ) { // Do the same too!
		action = Settings::writingCommaAction();
		takeCareOfText = false;
	}
	if (decoration->isFilterBarShown()) {
		action = 1;
		takeCareOfText = true;
	}

	// O:Nothing ; 1:Filter ; 2:Text ; 3:Html ; 4:Link
	if (action == 1) {
		if ((event->key() == Qt::Key_Backspace) && !decoration->isFilterBarShown())
			return; // Do not show and then hide bar (avoid two basket savings)
		if ( ! decoration->isFilterBarShown() )
			Global::mainContainer->showHideFilterBar(true, false);
		if (takeCareOfText)
			kapp->sendEvent(filterBar->lineEdit(), event);
		if (filterBar->lineEdit()->text().isEmpty() && takeCareOfText) // takeCareOfText: comma just show the (empty) bar
			Global::mainContainer->showHideFilterBar(false);
	} else if (action > 1) {
		if ( (isLocked()) ||                        // Do not insert note if locked
		     (event->key() == Qt::Key_Backspace) || // Or if key is backspace !
		     (m_stackedKeyEvent != 0L) )            // Sometimes, insert link dialog can take time to appear
			return;                                 //  and lot of links would be created otherwise.
		Note::Type type = Note::Text;
		if (action == 3)
			type = Note::Html;
		if (action == 4)
			type = Note::Link;
		if (takeCareOfText)
			m_stackedKeyEvent = new QKeyEvent(*event);
		else
			m_stackedKeyEvent = 0L;
		Global::mainContainer->insertEmpty(type);
	}
}

Note* Basket::nextShownNoteFrom(Note *note, int step)
{
	if (step > 0) {
		while (step && note) {
			note = note->next();
			if (note && note->isShown())
				step--;
		}
	} else {
		while (step && note) {
			note = note->previous();
			if (note && note->isShown())
				step++;
		}
	}
	return note;
}

void Basket::clicked(Note *note, bool controlPressed, bool shiftPressed)
{
	if (shiftPressed) {
		if (m_startOfShiftSelectionNote == 0L)
			m_startOfShiftSelectionNote = note;
		selectRange(m_startOfShiftSelectionNote, note);
	} else {
		m_startOfShiftSelectionNote = note;
		if (controlPressed) {
			note->setSelected( ! note->isSelected() );
			recolorizeNotes();
		} else
			unselectAllBut(note);
	}

	setFocusedNote(note);
}



void Basket::editNote(Note *note, bool editAnnotations)
{
	if (isLocked())
		return;

	if (m_editor != 0L) // It arrive toolbar of another editor stay shown...
		closeEditor();

	setFocusedNote(note); // Focus and select note (in case of new inserted note)
	unselectAllBut(note);

	// If possible and if wanted, use inline editor :
	if ( true/*Settings::useInlineEditors()*/ && !editAnnotations && Global::mainContainer->isShown() &&
	     (note->type() == Note::Text || note->type() == Note::Html) ) {
		if (note->type() == Note::Text)
			m_editor = new NoteTextEditor(note, Global::mainContainer, viewport(), m_stackedKeyEvent);
		else
			m_editor = new NoteHtmlEditor(note, Global::mainContainer, viewport(), m_stackedKeyEvent);
		Global::mainContainer->addDockWindow(m_editor->toolbar());
/*		// DISABLE all SHOWN notes:
		for (Note *it = firstShownNote(); it != 0L; it = it->next())
			if (it->isShown()) {
				it->setEnabled(false);
				if (it == lastShownNote())
					break;
			}*/
		// The editor :
		m_editor->editorWidget()->setPaletteBackgroundColor(note->isAlternate() ? altColor() : color());
		m_editor->editorWidget()->reparent( viewport(), QPoint(0,0), true );
		addChild(m_editor->editorWidget(), 0, 0);
		placeEditor();
//		((QFrame*)m_editor->editorWidget())->setFrameShape(QFrame::NoFrame);
//		((QFrame*)m_editor->editorWidget())->setLineWidth(1);
		// To avoid a one line text with an hozirontal scrollBar that take all the widget :
		if (note->type() == Note::Text || note->type() == Note::Html)
			((QScrollView*)m_editor->editorWidget())->setHScrollBarMode(QScrollView::AlwaysOff);
		m_editor->editorWidget()->show();
		m_editor->editorWidget()->raise();
		m_editor->goFirstFocus();
		connect( m_editor, SIGNAL(focusOut()), this, SLOT(closeEditor()) );
		connect( (QTextEdit*)(m_editor->editorWidget()), SIGNAL(textChanged()), this, SLOT(placeEditor()) );
//		if (m_stackedKeyEvent) {
//			kapp->postEvent(m_editor->editorWidget(), m_stackedKeyEvent);
		m_stackedKeyEvent = 0L;
//		}
		kapp->processEvents();   // Show the editor toolbar before ensuring the note is visible
		ensureNoteVisible(note); //  because toolbar can create a new line and then partially hide the note
		Global::mainContainer->updateStatusBarHint(); // Display the edition hint
	} else {
		NoteEditDialog *dialog = new NoteEditDialog(note, editAnnotations, note, m_stackedKeyEvent);
		dialog->exec();
		m_stackedKeyEvent = 0L;
		delete dialog;
	}
}




void Basket::openNote()
{
	for (Note *it = firstNote(); it != 0L; it = it->next())
		if (it->isSelected())
			it->slotOpen();
}

void Basket::openNoteWith()
{
	if (m_countSelecteds != 1) // TODO: Do better for multiple open with...
		return;

	// Take THE selected note
	for (Note *it = firstNote(); it != 0L; it = it->next())
		if (it->isSelected()) {
			it->slotOpenWith();
			return;
		}
}

void Basket::saveNoteAs()
{
	if (m_countSelecteds != 1) // TODO: Do better for multiple save as...
		return;

	// Take THE selected note
	for (Note *it = firstNote(); it != 0L; it = it->next())
		if (it->isSelected()) {
			it->slotSaveAs();
			return;
		}
}

void Basket::moveOnTop()
{
	if (m_countSelecteds == 0)
		return;

	Note *endOfBrowse = firstShownNote();
	Note *topNote     = firstNote();
	Note *prev;
	for (Note *it = lastShownNote(); it != 0L; ) {
		prev = it->previous();
		if (it->isSelected()) {
			m_insertAtNote = topNote;
			m_insertAfter  = false;
			changeNotePlace(it);
			topNote = it;
		}
		if (it == endOfBrowse)
			break;
		it = prev;
	}
	ensureNoteVisible(firstShownNote());
	ensureNoteVisible(m_focusedNote);
}

void Basket::moveOnBottom()
{
	if (m_countSelecteds == 0)
		return;

	Note *endOfBrowse = lastShownNote();
	Note *bottomNote  = lastNote();
	Note *next;
	for (Note *it = firstShownNote(); it != 0L; ) {
		next = it->next();
		if (it->isSelected()) {
			m_insertAtNote = bottomNote;
			m_insertAfter  = true;
			changeNotePlace(it);
			bottomNote = it;
		}
		if (it == endOfBrowse)
			break;
		it = next;
	}
	ensureNoteVisible(lastShownNote());
	ensureNoteVisible(m_focusedNote);
}

void Basket::moveNoteUp()
{
	if (m_countSelecteds == 0)
		return;

	// Begin from the top (important move all selected notes one note up
	// AND to quit early if a selected note is the first shown one
	for (Note *it = firstShownNote(); it != 0L; it = it->next()) {
		if (it->isSelected() && it->isShown()) { // it->isShown() not necessary, but in case...
			if (it == firstShownNote())
				return; // No way...
			m_insertAtNote = nextShownNoteFrom(it, -1); // Previous shown note
			if (m_insertAtNote == 0L) { // Should not appends, since it's not the first shown note,
				resetInsertTo();        // there SHOULD be one before
				return;
			}
			m_insertAfter  = false;
			changeNotePlace(it);
		}
		if (it == lastShownNote())
			break;
	}
	ensureNoteVisible(m_focusedNote);
}

void Basket::moveNoteDown()
{
	if (m_countSelecteds == 0)
		return;

	// Begin from the bottom (important move all selected notes one note down
	// AND to quit early if a selected note is the last shown one
	for (Note *it = lastShownNote(); it != 0L; it = it->previous()) {
		if (it->isSelected() && it->isShown()) { // it->isShown() not necessary, but in case...
			if (it == lastShownNote())
				return; // No way...
			m_insertAtNote = nextShownNoteFrom(it, 1); // Next shown note
			if (m_insertAtNote == 0L) { // Should not appends, since it's not the last shown note,
				resetInsertTo();        // there SHOULD be one before
				return;
			}
			m_insertAfter  = true;
			changeNotePlace(it);
		}
		if (it == firstShownNote())
			break;
	}
	ensureNoteVisible(m_focusedNote);
}


/*********** Work to re-load notes when theire files have changed */
// TODO: class BasketUpdater
// FIXME: When a rename (de, new, del), if cutted by two timer intervals ????? Restart with 50 ms ? Yes.
// FIXME: If a timer can't be launched ? Is this happens often ?

// Created :         In :                  Deleted in :
// FileEvent(...)    slot*edFile           event
//   QString(...)

void Basket::dontCareOfCreation(const QString &path) /////////// FIXME: TODO: URGENT: notYetInserted() ???,
{
	m_dontCare.append(path);
	if (Global::debugWindow)
		*Global::debugWindow << "Watcher>Add a <i>don't care creation</i> of file : <font color=violet>" + path + "</font>";
}

void Basket::slotModifiedFile(const QString &path)
{
	m_updateQueue.append( new FileEvent(FileEvent::Modified, path) );
	if ( ! m_updateTimer.isActive() )
		m_updateTimer.start(c_updateTime, true); // 200 ms, only once
	if (Global::debugWindow)
		*Global::debugWindow << "Watcher>Modified : <font color=blue>" + path + "</font>";

}
void Basket::slotCreatedFile(const QString &path)
{
	bool dontCare = false;
	if (m_dontCare.contains(path)) { // Don't care of creation of files we know we have created ourself
		m_dontCare.remove(path);
		dontCare = true;
	} else {
		m_updateQueue.append( new FileEvent(FileEvent::Created, path) );
		if ( ! m_updateTimer.isActive() )
			m_updateTimer.start(c_updateTime, true); // 200 ms, only once
	}
	if (Global::debugWindow) {
		*Global::debugWindow << "Watcher>Created : <font color=green>" + path + "</font>";
		if (dontCare)
			*Global::debugWindow << "\tBut don't care (created by the application and we know this) : ignore it";
	}
}
void Basket::slotDeletedFile(const QString &path)
{
	m_updateQueue.append( new FileEvent(FileEvent::Deleted, path) );
	if ( ! m_updateTimer.isActive() )
		m_updateTimer.start(c_updateTime, true); // 200 ms, only once
	if (Global::debugWindow)
		*Global::debugWindow << "Watcher>Deleted : <font color=red>" + path + "</font>";
}

void Basket::slotUpdateNotes()
{
	/* This functions is called after changes on the disk
	 *
	 */

//	m_dontCare.clear();

	if (Global::debugWindow)
		*Global::debugWindow << "Updater : Begin";

	/* First, browse the queue and keep only one instance of each files
	    (to reload/change only once), placed in lists per actions to do.
	    Note : Events was added in m_updateQueue by chronogical order */
	FileEvent   *event;
	QStringList  toCreate;
	QStringList  toModify;
	QStringList  toDelete;
	QStringList  toRename;
	QStringList  renameTo;
	while (m_updateQueue.count() > 0) {
		event = m_updateQueue.take(0);

		int     eventType =   event->event;
		QString eventPath(event->filePath);
		/* The .basket file isn't a note */
		if (eventPath.endsWith("/.basket")) {
			;                                             // Do nothing
		/* Note must be created ? */
		} else if (eventType == FileEvent::Created) {
			if (toCreate.findIndex(eventPath) == -1)      // Create only once (return first index or -1 if not)
				toCreate.append(eventPath);
		/* Note must be updated ? */
		} else if (eventType == FileEvent::Modified) {
			if ( toCreate.findIndex(eventPath) == -1 &&   // If created and then modified, do not add
			     toModify.findIndex(eventPath) == -1    ) // If modified several times,    do not add
				toModify.append(eventPath);
		/* Note must be deleted ? */
		} else if (eventType == FileEvent::Deleted) {
			// First try to see if it is a file to rename
			//  In this case, events are : [delete foo], [create bar], [delete foo]
			if (m_updateQueue.count() >= 2) { // If theire is at least two next events
				FileEvent *createEvt  = m_updateQueue.take(0);
				FileEvent *delete2Evt = m_updateQueue.take(0);
				if (createEvt->event     == FileEvent::Created &&
				    delete2Evt->event    == FileEvent::Deleted &&
				    delete2Evt->filePath == event->filePath       ) {
					if (toRename.findIndex(eventPath) == -1 ) { // Rename only once
						toRename.append(eventPath);
						renameTo.append(createEvt->filePath);
					}
					delete event;
					delete createEvt;
					delete delete2Evt;
					continue; // Continue on top of the while(){}
				} else { // It isn't a rename : re-place the events in the stack to parse them the next time
					m_updateQueue.prepend(delete2Evt); // In the inverse order !
					m_updateQueue.prepend(createEvt);
					// It's a delete : perform the corresponding action :
				}
			}
			if (toCreate.findIndex(eventPath) != -1)  // Created and then deleted
				toCreate.remove(eventPath);           //  => No need to create
			if (toModify.findIndex(eventPath) != -1)  // Modified and then deleted
				toModify.remove(eventPath);           //  => No need to modify
			if (toDelete.findIndex(eventPath) == -1 ) // Delete only once
				toDelete.append(eventPath);
		}
		delete event;

	}

	if (Global::debugWindow) {
		QValueList<QString>::iterator path;
		QValueList<QString>::iterator name2 = renameTo.begin();
		*Global::debugWindow << "    Create :";
		for (path = toCreate.begin(); path != toCreate.end(); ++path)
			*Global::debugWindow << "\t" + (*path);
		*Global::debugWindow << "    Modify :";
		for (path = toModify.begin(); path != toModify.end(); ++path)
			*Global::debugWindow << "\t" + (*path);
		*Global::debugWindow << "    Delete :";
		for (path = toDelete.begin(); path != toDelete.end(); ++path)
			*Global::debugWindow << "\t" + (*path);
		*Global::debugWindow << "    Rename :";
		for (path = toRename.begin(); path != toRename.end(); ++path) {
			*Global::debugWindow << "\t" + (*path) + " to " + (*name2);
			++name2;
		}
	}

	Note *note;
	QValueList<QString>::iterator path;
	QValueList<QString>::iterator name2 = renameTo.begin();
	for (path = toCreate.begin(); path != toCreate.end(); ++path) {
		// fileNameForFullPath() :
		QString fileName = (*path).mid(fullPath().length()); // fileName is never a mirror
		NoteFactory::loadFile(fileName, this);
	}
	for (path = toModify.begin(); path != toModify.end(); ++path)
		if ( (note = noteForFullPath(*path)) != 0 ) {
			note->loadContent();
			noteSizeChanged(note);
		}
	for (path = toDelete.begin(); path != toDelete.end(); ++path)
		if ( (note = noteForFullPath(*path)) != 0 )
			delNote(note);
	for (path = toRename.begin(); path != toRename.end(); ++path) {
		if ( (note = noteForFullPath(*path)) != 0 )
			note->fileNameChanged( (*name2).mid(fullPath().length()) ); // Same
		++name2;
	} // We view renamed files ^^ and save the change:
	if (!toRename.isEmpty())
		save();

	if (Global::debugWindow)
		*Global::debugWindow << "Updater : End";
}

/******************************************************************/

#endif // #if 0

#include "basket.moc"