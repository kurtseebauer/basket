/***************************************************************************
 *   Copyright (C) 2005 by S�bastien Lao�t                                 *
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

#include <kapplication.h>
#include <kstyle.h>
#include <kiconloader.h>
#include <qpainter.h>
#include <qfont.h>
#include <qdom.h>
#include <qdir.h>
#include <kglobalsettings.h>
#include <klocale.h>

#include "tag.h"
#include "xmlwork.h"
#include "global.h"
#include "debugwindow.h"
#include "container.h"
#include "tools.h"
#include "basket.h"

#include <iostream>

/** class State: */

State::State(const QString &id, Tag *tag)
 : m_id(id), m_name(), m_emblem(), m_bold(false), m_italic(false), m_underline(false), m_strikeOut(false),
   m_textColor(), m_fontName(), m_fontSize(-1), m_backgroundColor(), m_textEquivalent(), m_onAllTextLines(false), m_parentTag(tag)
{
}

State::~State()
{
}

State* State::nextState(bool cycle /*= true*/)
{
	if (!parentTag())
		return 0;

	List states = parentTag()->states();
	// The tag contains only one state:
	if (states.count() == 1)
		return 0;
	// Find the next state:
	for (List::iterator it = states.begin(); it != states.end(); ++it)
		// Found the current state in the list:
		if (*it == this) {
			// Find the next state:
			State *next = *(++it);
			if (it == states.end())
				return (cycle ? states.first() : 0);
			return next;
		}
	// Should not happens:
	return 0;
}

QString State::fullName()
{
	if (!parentTag() || parentTag()->states().count() == 1)
		return name();
	return QString(i18n("%1: %2")).arg(parentTag()->name(), name());
}

QFont State::font(QFont base)
{
	if (bold())
		base.setBold(true);
	if (italic())
		base.setItalic(true);
	if (underline())
		base.setUnderline(true);
	if (strikeOut())
		base.setStrikeOut(true);
	if (!fontName().isEmpty())
		base.setFamily(fontName());
	if (fontSize() > 0)
		base.setPointSize(fontSize());
	return base;
}

QString State::toCSS(const QString &gradientFolderPath, const QString &gradientFolderName, const QFont &baseFont)
{
	QString css;
	if (bold())
		css += " font-weight: bold;";
	if (italic())
		css += " font-style: italic;";
	if (underline() && strikeOut())
		css += " text-decoration: underline line-through;";
	else if (underline())
		css += " text-decoration: underline;";
	else if (strikeOut())
		css += " text-decoration: line-through;";
	if (textColor().isValid())
		css += " color: " + textColor().name() + ";";
	if (!fontName().isEmpty()) {
		QString fontFamily = Tools::cssFontDefinition(fontName(), /*onlyFontFamily=*/true);
		css += " font-family: " + fontFamily + ";";
	}
	if (fontSize() > 0)
		css += " font-size: " + QString::number(fontSize()) + "px;";
	if (backgroundColor().isValid()) {
		// Get the colors of the gradient and the border:
		QColor topBgColor;
		QColor bottomBgColor;
		Note::getGradientColors(backgroundColor(), &topBgColor, &bottomBgColor);
		// Produce the CSS code:
		QString gradientFileName = Basket::saveGradientBackground(backgroundColor(), font(baseFont), gradientFolderPath);
		css += " background: " + bottomBgColor.name() + " url('" + gradientFolderName + gradientFileName + "') repeat-x;";
		css += " border-top: solid " + topBgColor.name() + " 1px;";
		css += " border-bottom: solid " + Tools::mixColor(topBgColor, bottomBgColor).name() + " 1px;";
	}

	if (css.isEmpty())
		return "";
	else
		return "   .tag_" + id() + " {" + css + " }\n";
}

void State::merge(const List &states, State *result, int *emblemsCount, bool *haveInvisibleTags, const QColor &backgroundColor)
{
	*result            = State(); // Reset to default values.
	*emblemsCount      = 0;
	*haveInvisibleTags = false;

	for (List::const_iterator it = states.begin(); it != states.end(); ++it) {
		State *state = *it;
		bool isVisible = false;
		// For each propertie, if that properties have a value (is not default) is the current state of the list,
		// and if it haven't been set to the result state by a previous state, then it's visible and we assign the propertie to the result state.
		if (!state->emblem().isEmpty()) {
			++*emblemsCount;
			isVisible = true;
		}
		if (state->bold() && !result->bold()) {
			result->setBold(true);
			isVisible = true;
		}
		if (state->italic() && !result->italic()) {
			result->setItalic(true);
			isVisible = true;
		}
		if (state->underline() && !result->underline()) {
			result->setUnderline(true);
			isVisible = true;
		}
		if (state->strikeOut() && !result->strikeOut()) {
			result->setStrikeOut(true);
			isVisible = true;
		}
		if (state->textColor().isValid() && !result->textColor().isValid()) {
			result->setTextColor(state->textColor());
			isVisible = true;
		}
		if (!state->fontName().isEmpty() && result->fontName().isEmpty()) {
			result->setFontName(state->fontName());
			isVisible = true;
		}
		if (state->fontSize() > 0 && result->fontSize() <= 0) {
			result->setFontSize(state->fontSize());
			isVisible = true;
		}
		if (state->backgroundColor().isValid() && !result->backgroundColor().isValid() && state->backgroundColor() != backgroundColor) { // vv
			result->setBackgroundColor(state->backgroundColor()); // This is particular: if the note background color is the same as the basket one, don't use that.
			isVisible = true;
		}
		// If it's not visible, well, at least one tag is not visible: the note will display "..." at the tags arrow place to show that:
		if (!isVisible)
			*haveInvisibleTags = true;
	}
}


/** class Tag: */

Tag::List Tag::all = Tag::List();

Tag::Tag()
{
	static int tagNumber = 0;
	++tagNumber;
	QString sAction = "tag_shortcut_number_" + QString::number(tagNumber);
	m_action = new KAction("FAKE TEXT", "FAKE ICON", KShortcut(), Global::mainContainer, SLOT(activatedTagShortcut()), Global::mainContainer->actionCollection(), sAction);
	m_action->setShortcutConfigurable(false); // We do it in the tag properties dialog

	m_inheritedBySiblings = false;
}

Tag::~Tag()
{
}

State* Tag::stateForId(const QString &id)
{
	for (List::iterator it = all.begin(); it != all.end(); ++it)
		for (State::List::iterator it2 = (*it)->states().begin(); it2 != (*it)->states().end(); ++it2)
			if ((*it2)->id() == id)
				return *it2;
	return 0;
}

Tag* Tag::tagForKAction(KAction *action)
{
	for (List::iterator it = all.begin(); it != all.end(); ++it)
		if ((*it)->m_action == action)
			return *it;
	return 0;
}

void Tag::loadTags()
{
	QString fullPath = Global::savesFolder() + "tags.xml";
	QString doctype  = "basketTags";

	QDir dir;
	if (!dir.exists(fullPath)) {
		DEBUG_WIN << "Tags file does not exists: Creating it...";
		createDefaultTagsSet(fullPath);
	}

	QDomDocument *document = XMLWork::openFile(doctype, fullPath);
	if (!document) {
		DEBUG_WIN << "<font color=red>FAILED to read the tags file</font>";
		return;
	}

	QDomElement docElem = document->documentElement();
	QDomNode node = docElem.firstChild();
	while (!node.isNull()) {
		QDomElement element = node.toElement();
		if ( (!element.isNull()) && element.tagName() == "tag" ) {
			Tag *tag = new Tag();
			// Load properties:
			QString name      = XMLWork::getElementText(element, "name");
			QString shortcut  = XMLWork::getElementText(element, "shortcut");
			QString inherited = XMLWork::getElementText(element, "inherited", "false");
			tag->setName(name);
			tag->setShortcut(KShortcut(shortcut));
			tag->setInheritedBySiblings(XMLWork::trueOrFalse(inherited));
			// Load states:
			QDomNode subNode = element.firstChild();
			while (!subNode.isNull()) {
				QDomElement subElement = subNode.toElement();
				if ( (!subElement.isNull()) && subElement.tagName() == "state" ) {
					State *state = new State(subElement.attribute("id"), tag);
					state->setName(   XMLWork::getElementText(subElement, "name")   );
					state->setEmblem( XMLWork::getElementText(subElement, "emblem") );
					QDomElement textElement = XMLWork::getElement(subElement, "text");
					state->setBold(      XMLWork::trueOrFalse(textElement.attribute("bold",      "false")) );
					state->setItalic(    XMLWork::trueOrFalse(textElement.attribute("italic",    "false")) );
					state->setUnderline( XMLWork::trueOrFalse(textElement.attribute("underline", "false")) );
					state->setStrikeOut( XMLWork::trueOrFalse(textElement.attribute("strikeOut", "false")) );
					QString textColor = textElement.attribute("color", "");
					state->setTextColor(textColor.isEmpty() ? QColor() : QColor(textColor));
					QDomElement fontElement = XMLWork::getElement(subElement, "font");
					state->setFontName(fontElement.attribute("name", ""));
					QString fontSize = fontElement.attribute("size", "");
					state->setFontSize(fontSize.isEmpty() ? -1 : fontSize.toInt());
					QString backgroundColor = XMLWork::getElementText(subElement, "backgroundColor", "");
					state->setBackgroundColor(backgroundColor.isEmpty() ? QColor() : QColor(backgroundColor));
					QDomElement textEquivalentElement = XMLWork::getElement(subElement, "textEquivalent");
					state->setTextEquivalent( textEquivalentElement.attribute("string", "") );
					state->setOnAllTextLines( XMLWork::trueOrFalse(textEquivalentElement.attribute("onAllTextLines", "false")) );
					tag->appendState(state);
				}
				subNode = subNode.nextSibling();
			}
			// Append it
			if (tag->countStates() > 0) {
				State *firstState = tag->states().first();
				if (tag->countStates() == 1 && firstState->name().isEmpty())
					firstState->setName(tag->name());
				if (tag->name().isEmpty())
					tag->setName(firstState->name());
				all.append(tag);
			}
		}
		node = node.nextSibling();
	}
}

void Tag::saveTags()
{
}

void Tag::createDefaultTagsSet(const QString &fullPath)
{
	QString xml = QString(
		"<!DOCTYPE basketTags>\n"
		"<basketTags>\n"
		"  <tag>\n"
		"    <name>%1</name>\n" // "To Do"
		"    <shortcut>Ctrl+1</shortcut>\n"
		"    <inherited>true</inherited>\n"
		"    <state id=\"todo_unchecked\">\n"
		"      <name>%2</name>\n" // "Unchecked"
		"      <emblem>tag_checkbox</emblem>\n"
		"      <text bold=\"false\" italic=\"false\" underline=\"false\" strikeOut=\"false\" color=\"\" />\n"
		"      <font name=\"\" size=\"\" />\n"
		"      <backgroundColor></backgroundColor>\n"
		"      <textEquivalent string=\"[ ]\" onAllTextLines=\"false\" />\n"
		"    </state>\n"
		"    <state id=\"todo_done\">\n"
		"      <name>%3</name>\n" // "Done"
		"      <emblem>tag_checkbox_checked</emblem>\n"
		"      <text bold=\"false\" italic=\"false\" underline=\"false\" strikeOut=\"true\" color=\"\" />\n"
		"      <font name=\"\" size=\"\" />\n"
		"      <backgroundColor></backgroundColor>\n"
		"      <textEquivalent string=\"[x]\" onAllTextLines=\"false\" />\n"
		"    </state>\n"
		"  </tag>\n"
		"\n"
		"  <tag>\n"
		"    <name>%4</name>\n" // "Progress"
		"    <shortcut>Ctrl+2</shortcut>\n"
		"    <inherited>true</inherited>\n"
		"    <state id=\"progress_000\">\n"
		"      <name>%5</name>\n" // "0 %"
		"      <emblem>tag_progress_000</emblem>\n"
		"      <textEquivalent string=\"[    ]\" />\n"
		"    </state>\n"
		"    <state id=\"progress_025\">\n"
		"      <name>%6</name>\n" // "25 %"
		"      <emblem>tag_progress_025</emblem>\n"
		"      <textEquivalent string=\"[=   ]\" />\n"
		"    </state>\n"
		"    <state id=\"progress_050\">\n"
		"      <name>%7</name>\n" // "50 %"
		"      <emblem>tag_progress_050</emblem>\n"
		"      <textEquivalent string=\"[==  ]\" />\n"
		"    </state>\n"
		"    <state id=\"progress_075\">\n"
		"      <name>%8</name>\n" // "75 %"
		"      <emblem>tag_progress_075</emblem>\n"
		"      <textEquivalent string=\"[=== ]\" />\n"
		"    </state>\n"
		"    <state id=\"progress_100\">\n"
		"      <name>%9</name>\n" // "100 %"
		"      <emblem>tag_progress_100</emblem>\n"
		"      <textEquivalent string=\"[====]\" />\n"
		"    </state>\n"
		"  </tag>\n"
		"\n")
			.arg( i18n("To Do"),     i18n("Unchecked"),      i18n("Done")        )  // %1 %2 %3
			.arg( i18n("Progress"),  i18n("0 %"),            i18n("25 %")        )  // %4 %5 %6
			.arg( i18n("50 %"),      i18n("75 %"),           i18n("100 %")       )  // %7 %8 %9
	+ QString(
		"  <tag>\n"
		"    <name>%1</name>\n" // "Priority"
		"    <shortcut>Ctrl+3</shortcut>\n"
		"    <inherited>true</inherited>\n"
		"    <state id=\"priority_low\">\n"
		"      <name>%2</name>\n" // "Low"
		"      <emblem>tag_priority_low</emblem>\n"
		"      <textEquivalent string=\"{1}\" />\n"
		"    </state>\n"
		"    <state id=\"priority_medium\">\n"
		"      <name>%3</name>\n" // "Medium
		"      <emblem>tag_priority_medium</emblem>\n"
		"      <textEquivalent string=\"{2}\" />\n"
		"    </state>\n"
		"    <state id=\"priority_high\">\n"
		"      <name>%4</name>\n" // "High"
		"      <emblem>tag_priority_high</emblem>\n"
		"      <textEquivalent string=\"{3}\" />\n"
		"    </state>\n"
		"  </tag>\n"
		"\n"
		"  <tag>\n"
		"    <name>%5</name>\n" // "Preference"
		"    <shortcut>Ctrl+4</shortcut>\n"
		"    <inherited>true</inherited>\n"
		"    <state id=\"preference_bad\">\n"
		"      <name>%6</name>\n" // "Bad"
		"      <emblem>tag_preference_bad</emblem>\n"
		"      <textEquivalent string=\"(*  )\" />\n"
		"    </state>\n"
		"    <state id=\"preference_good\">\n"
		"      <name>%7</name>\n" // "Good"
		"      <emblem>tag_preference_good</emblem>\n"
		"      <textEquivalent string=\"(** )\" />\n"
		"    </state>\n"
		"    <state id=\"preference_excelent\">\n"
		"      <name>%8</name>\n" // "Excelent"
		"      <emblem>tag_preference_excelent</emblem>\n"
		"      <textEquivalent string=\"(***)\" />\n"
		"    </state>\n"
		"  </tag>\n"
		"\n"
		"  <tag>\n"
		"    <name>%9</name>\n" // "Highlight"
		"    <shortcut>Ctrl+5</shortcut>\n"
		"    <state id=\"highlight\">\n"
		"      <backgroundColor>#ffffcc</backgroundColor>\n"
		"      <textEquivalent string=\"=>\" />\n"
		"    </state>\n"
		"  </tag>\n"
		"\n")
			.arg( i18n("Priority"),  i18n("Low"),            i18n("Medium")      )  // %1 %2 %3
			.arg( i18n("High"),      i18n("Preference"),     i18n("Bad")         )  // %4 %5 %6
			.arg( i18n("Good"),      i18n("Excelent"),       i18n("Highlight")   )  // %7 %8 %9
	+ QString(
		"  <tag>\n"
		"    <name>%1</name>\n" // "Important"
		"    <shortcut>Ctrl+6</shortcut>\n"
		"    <state id=\"important\">\n"
		"      <emblem>tag_important</emblem>\n"
		"      <backgroundColor>#ffcccc</backgroundColor>\n"
		"      <textEquivalent string=\"!!\" />\n"
		"    </state>\n"
		"  </tag>\n"
		"\n"
		"  <tag>\n"
		"    <name>%2</name>\n" // "Very Important"
		"    <shortcut>Ctrl+7</shortcut>\n"
		"    <state id=\"very_important\">\n"
		"      <emblem>tag_important</emblem>\n"
		"      <text color=\"#ffffff\" />\n"
		"      <backgroundColor>#ff0000</backgroundColor>\n"
		"      <textEquivalent string=\"/!\\\" />\n"
		"    </state>\n"
		"  </tag>\n"
		"\n"
		"  <tag>\n"
		"    <name>%3</name>\n" // "Information"
		"    <shortcut>Ctrl+8</shortcut>\n"
		"    <state id=\"information\">\n"
		"      <emblem>messagebox_info</emblem>\n"
		"      <textEquivalent string=\"(i)\" />\n"
		"    </state>\n"
		"  </tag>\n"
		"\n"
		"  <tag>\n"
		"    <name>%4</name>\n" // "Idea"
		"    <shortcut>Ctrl+9</shortcut>\n"
		"    <state id=\"idea\">\n"
		"      <emblem>ktip</emblem>\n"
		"      <textEquivalent string=\"%5\" />\n" // I.
		"    </state>\n"
		"  </tag>""\n"
		"\n"
		"  <tag>\n"
		"    <name>%6</name>\n" // "Title"
		"    <shortcut>Ctrl+0</shortcut>\n"
		"    <state id=\"title\">\n"
		"      <text bold=\"true\" />\n"
		"      <textEquivalent string=\"##\" />\n"
		"    </state>\n"
		"  </tag>\n"
		"\n"
		"  <tag>\n"
		"    <name>%7</name>\n" // "Code"
		"    <state id=\"code\">\n"
		"      <font name=\"monospace\" />\n"
		"      <textEquivalent string=\"|\" onAllTextLines=\"true\" />\n"
		"    </state>\n"
		"  </tag>\n"
		"\n"
		"  <tag>\n"
		"    <state id=\"work\">\n"
		"      <name>%8</name>\n" // "Work"
		"      <text color=\"#ff8000\" />\n"
		"      <textEquivalent string=\"%9\" />\n" // W.
		"    </state>\n"
		"  </tag>""\n"
		"\n")
			.arg( i18n("Important"), i18n("Very Important"),              i18n("Information")                 ) // %1 %2 %3
			.arg( i18n("Idea"),      i18n("The initial of 'Idea'", "I."), i18n("Title")                       ) // %4 %5 %6
			.arg( i18n("Code"),      i18n("Work"),                        i18n("The initial of 'Work'", "W.") ) // %7 %8 %9
	+ QString(
		"  <tag>\n"
		"    <state id=\"personal\">\n"
		"      <name>%1</name>\n" // "Personal"
		"      <text color=\"#008000\" />\n"
		"      <textEquivalent string=\"%2\" />\n" // P.
		"    </state>\n"
		"  </tag>\n"
		"\n"
		"  <tag>\n"
		"    <state id=\"funny\">\n"
		"      <name>%3</name>\n" // "Funny"
		"      <emblem>tag_fun</emblem>\n"
		"    </state>\n"
		"  </tag>\n"
		"</basketTags>\n"
		"")
			.arg( i18n("Personal"), i18n("The initial of 'Personal'", "P."), i18n("Funny") ); // %1 %2 %3

	// Write to Disk:
	QFile file(fullPath);
	if (file.open(IO_WriteOnly)) {
		QTextStream stream(&file);
		stream.setEncoding(QTextStream::UnicodeUTF8);
		stream << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";
		stream << xml;
		file.close();
	} else
		DEBUG_WIN << "<font color=red>FAILED to create the tags file</font>!";
}

#include <kapplication.h>
#include <qrect.h>
#include <qstyle.h>
#include <qcheckbox.h>
#include <qbitmap.h>
#include <kglobalsettings.h>
#include <qimage.h>
#include <qradiobutton.h>
#include <kiconeffect.h>

/** class IndentedMenuItem: */

IndentedMenuItem::IndentedMenuItem(const QString &text, const QString &icon, const QString &shortcut)
 : m_text(text), m_icon(icon), m_shortcut(shortcut)
{
}

IndentedMenuItem::~IndentedMenuItem()
{
}

void IndentedMenuItem::paint(QPainter *painter, const QColorGroup &cg, bool active, bool enabled, int x, int y, int w, int h)
{
	QPen  pen  = painter->pen();
	QFont font = painter->font();

	int iconSize   = KIcon::SizeSmall;
	int iconMargin = StateMenuItem::iconMargin();

	/* When an item is disabled, it often have a 3D sunken look.
	 * This is done by calling this paint routine two times, with different pen color and offset.
	 * A disabled item is first painted in the rect (x+1, y+1, w, h) and with pen of cg.light() color,
	 * It is then drawn a second time in the rect (x, y, w, h).
	 * But we don't want to draw the icon two times! So, we try to detect if we are in the "etched-text draw" state and then don't draw the icon.
	 * This doesn't work for every styles but it's already better than nothing (styles when it doesn't work are seldomly used, if used).
	 */
	bool drawingEtchedText = !enabled && !active && painter->pen().color() != cg.mid()/*== cg.foreground()*/;
	if (drawingEtchedText) {
		QString styleName = kapp->style().name();
		if (styleName == "plastik" || styleName == "lipstik")
			painter->setPen(cg.light());
		drawingEtchedText = !enabled && !active && painter->pen().color() != cg.foreground();
	} else
		drawingEtchedText = !enabled && !active && painter->pen().color() == cg.light();
	if (!m_icon.isEmpty() && !drawingEtchedText) {
		QPixmap icon = kapp->iconLoader()->loadIcon(m_icon, KIcon::Small, iconSize,
		                                            (enabled ? (active ? KIcon::ActiveState : KIcon::DefaultState) : KIcon::DisabledState),
		                                            /*path_store=*/0L, /*canReturnNull=*/true);
		painter->drawPixmap(x, y + (h-iconSize)/2, icon);
	}
	/* Pen and font are already set to the good ones, so we can directly draw the text.
	 * BUT, for the half of styles provided with KDE, the pen is not set for the Active state (when hovered by mouse of selected by keyboard).
	 * So, I set the pen myself.
	 * But it's certainly a bug in those styles because some other styles eg. just draw a 3D sunken rect when an item is selected
	 * and keep the background to white, drawing a white text over it is... very bad. But I can't see what can be done.
	 */
	if (active && enabled)
		painter->setPen(KGlobalSettings::highlightedTextColor());
	painter->drawText(x + iconSize + iconMargin, y, w - iconSize - iconMargin, h, AlignLeft | AlignVCenter | DontClip | ShowPrefix, m_text/*painter->pen().color().name()*/);

	if (!m_shortcut.isEmpty()) {
		painter->setPen(pen);
		if (active && enabled)
			painter->setPen(KGlobalSettings::highlightedTextColor());
		painter->setFont(font);
		painter->setClipping(false);
		painter->drawText(x + 5 + w, y, 3000, h, AlignLeft | AlignVCenter | DontClip | ShowPrefix, m_shortcut);
	}
}

QSize IndentedMenuItem::sizeHint()
{
	int iconSize   = KIcon::SizeSmall;
	int iconMargin = StateMenuItem::iconMargin();
	QSize textSize = QFontMetrics(KGlobalSettings::menuFont()).size( AlignLeft | AlignVCenter | ShowPrefix | DontClip,  m_text );
	return QSize(iconSize + iconMargin + textSize.width(), textSize.height());
}

/** class StateMenuItem: */

StateMenuItem::StateMenuItem(State *state, const QString &shortcut, bool withTagName)
 : m_state(state), m_shortcut(shortcut)
{
	m_name = (withTagName && m_state->parentTag() ? m_state->parentTag()->name() : m_state->name());
}

StateMenuItem::~StateMenuItem()
{
}

void StateMenuItem::paint(QPainter *painter, const QColorGroup &cg, bool active, bool enabled, int x, int y, int w, int h)
{
	QPen  pen  = painter->pen();
	QFont font = painter->font();

	int iconSize   = 16; // We use 16 instead of KIcon::SizeSmall (the size of icons in menus) because tags will always be 16*16 icons

	if (!active && m_state->backgroundColor().isValid())
		painter->fillRect(x/*-1*/, y/*-1*/, w/*+2*/, h/*+2*/, m_state->backgroundColor());
	/* When an item is disabled, it often have a 3D sunken look.
	 * This is done by calling this paint routine two times, with different pen color and offset.
	 * A disabled item is first painted in the rect (x+1, y+1, w, h) and with pen of cg.light() color,
	 * It is then drawn a second time in the rect (x, y, w, h).
	 * But we don't want to draw the icon two times! So, we try to detect if we are in the "etched-text draw" state and then don't draw the icon.
	 * This doesn't work for every styles but it's already better than nothing (styles when it doesn't work are seldomly used, if used).
	 */
	bool drawingEtchedText = !enabled && !active && painter->pen().color() != cg.mid()/*== cg.foreground()*/;
	if (drawingEtchedText) {
		QString styleName = kapp->style().name();
		if (styleName == "plastik" || styleName == "lipstik")
			painter->setPen(cg.light());
		drawingEtchedText = !enabled && !active && painter->pen().color() != cg.foreground();
	} else
		drawingEtchedText = !enabled && !active && painter->pen().color() == cg.light();
	if (!m_state->emblem().isEmpty() && !drawingEtchedText) {
		QPixmap icon = kapp->iconLoader()->loadIcon(m_state->emblem(), KIcon::Small, iconSize,
		                                            (enabled ? (active ? KIcon::ActiveState : KIcon::DefaultState) : KIcon::DisabledState),
		                                            /*path_store=*/0L, /*canReturnNull=*/true);
		painter->drawPixmap(x, y + (h-iconSize)/2, icon);
	}
	if (enabled && !active && m_state->textColor().isValid())
		painter->setPen(m_state->textColor());
	/* Pen and font are already set to the good ones, so we can directly draw the text.
	 * BUT, for the half of styles provided with KDE, the pen is not set for the Active state (when hovered by mouse of selected by keyboard).
	 * So, I set the pen myself.
	 * But it's certainly a bug in those styles because some other styles eg. just draw a 3D sunken rect when an item is selected
	 * and keep the background to white, drawing a white text over it is... very bad. But I can't see what can be done.
	 */
	if (active && enabled)
		painter->setPen(KGlobalSettings::highlightedTextColor());
	painter->setFont( m_state->font(painter->font()) );
	painter->drawText(x + iconSize + iconMargin(), y, w - iconSize - iconMargin(), h, AlignLeft | AlignVCenter | DontClip | ShowPrefix, m_name);

	if (!m_shortcut.isEmpty()) {
		painter->setPen(pen);
		if (active && enabled)
			painter->setPen(KGlobalSettings::highlightedTextColor());
		painter->setFont(font);
		painter->setClipping(false);
		painter->drawText(x + 5 + w, y, 3000, h, AlignLeft | AlignVCenter | DontClip | ShowPrefix, m_shortcut);
	}
}

QSize StateMenuItem::sizeHint()
{
	int iconSize   = 16; // We use 16 instead of KIcon::SizeSmall (the size of icons in menus) because tags will always be 16*16 icons
	QFont theFont = m_state->font(KGlobalSettings::menuFont());
	QSize textSize = QFontMetrics(theFont).size( AlignLeft | AlignVCenter | ShowPrefix | DontClip,  m_name );
	return QSize(iconSize + iconMargin() + textSize.width(), textSize.height());
}

QIconSet StateMenuItem::checkBoxIconSet(bool checked, QColorGroup cg)
{
	int width  = kapp->style().pixelMetric(QStyle::PM_IndicatorWidth,  0);
	int height = kapp->style().pixelMetric(QStyle::PM_IndicatorHeight, 0);
	QRect rect(0, 0, width, height);

	QColor menuBackgroundColor = (dynamic_cast<KStyle*>(&(kapp->style())) == NULL ? cg.background() : cg.background().light(103));

	QPixmap  pixmap(width, height);
	pixmap.fill(menuBackgroundColor); // In case the pixelMetric() haven't returned a bigger rectangle than what drawPrimitive() draws
	QPainter painter(&pixmap);
	int style = QStyle::Style_Enabled | QStyle::Style_Active | (checked ? QStyle::Style_On : QStyle::Style_Off);
	kapp->style().drawPrimitive(QStyle::PE_Indicator, &painter, rect, cg, style);
	painter.end();

	QPixmap  pixmapHover(width, height);
	pixmapHover.fill(menuBackgroundColor); // In case the pixelMetric() haven't returned a bigger rectangle than what drawPrimitive() draws
	painter.begin(&pixmapHover);
	style |= QStyle::Style_MouseOver;
	cg.setColor(QColorGroup::Background, KGlobalSettings::highlightColor());
	kapp->style().drawPrimitive(QStyle::PE_Indicator, &painter, rect, cg, style);
	painter.end();

	QIconSet iconSet(pixmap);
	iconSet.setPixmap(pixmapHover, QIconSet::Automatic, QIconSet::Active);
	return iconSet;
}

QIconSet StateMenuItem::radioButtonIconSet(bool checked, QColorGroup cg)
{
	int width  = kapp->style().pixelMetric(QStyle::PM_ExclusiveIndicatorWidth,  0);
	int height = kapp->style().pixelMetric(QStyle::PM_ExclusiveIndicatorHeight, 0);
	QRect rect(0, 0, width, height);

	int style = QStyle::Style_Default | QStyle::Style_Enabled | (checked ? QStyle::Style_On : QStyle::Style_Off);

	QPixmap pixmap(width, height);
	pixmap.fill(Qt::red);
	QPainter painter(&pixmap);
	/* We can't use that line of code (like for checkboxes):
	 * //kapp->style().drawPrimitive(QStyle::PE_ExclusiveIndicator, &painter, rect, cg, style);
	 * because Plastik (and derived styles) don't care of the QStyle::Style_On flag and will ALWAYS draw an unchecked radiobutton.
	 * So, we use another method:
	 */
	QRadioButton rb(0);
	rb.setChecked(checked);
	kapp->style().drawControl(QStyle::CE_RadioButton, &painter, &rb, rect, cg, style);
	painter.end();
	/* Some styles like Plastik (and derived ones) have QStyle::PE_ExclusiveIndicator drawing a radiobutton disc, as wanted,
	 * and leave pixels ouside it untouched, BUT QStyle::PE_ExclusiveIndicatorMask is a fully black square.
	 * So, we can't apply the mask to make the radiobutton circle transparent outside.
	 * We're using an hack by filling the pixmap in Qt::red, drawing the radiobutton and then creating an heuristic mask.
	 * The heuristic mask is created using the 4 edge pixels (that are red) and by making transparent every pixels that are of this color:
	 */
	pixmap.setMask(pixmap.createHeuristicMask());

	QPixmap pixmapHover(width, height);
	pixmapHover.fill(Qt::red);
	painter.begin(&pixmapHover);
	//kapp->style().drawPrimitive(QStyle::PE_ExclusiveIndicator, &painter, rect, cg, style);
	style |= QStyle::Style_MouseOver;
	cg.setColor(QColorGroup::Background, KGlobalSettings::highlightColor());
	kapp->style().drawControl(QStyle::CE_RadioButton, &painter, &rb, rect, cg, style);
	painter.end();
	pixmapHover.setMask(pixmapHover.createHeuristicMask());

	QIconSet iconSet(pixmap);
	iconSet.setPixmap(pixmapHover, QIconSet::Automatic, QIconSet::Active);
	return iconSet;
}