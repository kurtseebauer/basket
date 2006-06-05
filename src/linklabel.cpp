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

#include <qlabel.h>
#include <kurl.h>
#include <qlayout.h>
#include <kiconloader.h>
#include <qcursor.h>
#include <klocale.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qhgroupbox.h>
#include <qpainter.h>
#include <kglobalsettings.h>
#include <qstyle.h>
#include <kapplication.h>
#include <kaboutdata.h>
#include <kdialogbase.h>

#include "linklabel.h"
#include "variouswidgets.h"
#include "tools.h"
#include "global.h"
#include "kcolorcombo2.h"
#include "basket.h" // For the class HtmlExportData

/** LinkLook */

LinkLook *LinkLook::soundLook       = new LinkLook(/*useLinkColor=*/false, /*canPreview=*/false);
LinkLook *LinkLook::fileLook        = new LinkLook(/*useLinkColor=*/false, /*canPreview=*/true);
LinkLook *LinkLook::localLinkLook   = new LinkLook(/*useLinkColor=*/true,  /*canPreview=*/true);
LinkLook *LinkLook::networkLinkLook = new LinkLook(/*useLinkColor=*/true,  /*canPreview=*/false);
LinkLook *LinkLook::launcherLook    = new LinkLook(/*useLinkColor=*/true,  /*canPreview=*/false);

LinkLook::LinkLook(bool useLinkColor, bool canPreview)
{
	m_useLinkColor = useLinkColor;
	m_canPreview   = canPreview;
}

LinkLook::LinkLook(const LinkLook &other)
{
	m_useLinkColor = other.useLinkColor();
	m_canPreview   = other.canPreview();
	setLook( other.italic(), other.bold(), other.underlining(),
	         other.color(), other.hoverColor(),
	         other.iconSize(), other.preview() );
}

void LinkLook::setLook(bool italic, bool bold, int underlining,
                       QColor color, QColor hoverColor,
                       int iconSize, int preview)
{
	m_italic      = italic;
	m_bold        = bold;
	m_underlining = underlining;
	m_color       = color;
	m_hoverColor  = hoverColor;
	m_iconSize    = iconSize;
	m_preview     = (canPreview() ? preview : None);
}

int LinkLook::previewSize() const
{
	if (previewEnabled()) {
		switch (preview()) {
			default:
			case None:          return 0;
			case IconSize:      return iconSize();
			case TwiceIconSize: return iconSize() * 2;
			case ThreeIconSize: return iconSize() * 3;
		}
	} else
		return 0;
}

QColor LinkLook::effectiveColor() const
{
	if (m_color.isValid())
		return m_color;
	else
		return defaultColor();
}

QColor LinkLook::effectiveHoverColor() const
{
	if (m_hoverColor.isValid())
		return m_hoverColor;
	else
		return defaultHoverColor();
}

QColor LinkLook::defaultColor() const
{
	if (m_useLinkColor)
		return KGlobalSettings::linkColor();
	else
		return KGlobalSettings::textColor();
}

QColor LinkLook::defaultHoverColor() const
{
	return Qt::red;
}

LinkLook* LinkLook::lookForURL(const KURL &url)
{
	return url.isLocalFile() ? localLinkLook : networkLinkLook;
}

QString LinkLook::toCSS(const QString &cssClass, const QColor &defaultTextColor) const
{
	// Set the link class:
	QString css = QString("   .%1 a { display: block; width: 100%;").arg(cssClass);
	if (underlineOutside())
		css += " text-decoration: underline;";
	else
		css += " text-decoration: none;";
	if (m_italic == true)
		css += " font-style: italic;";
	if (m_bold == true)
		css += " font-weight: bold;";
	QColor textColor = (color().isValid() || m_useLinkColor ? effectiveColor() : defaultTextColor);
	css += QString(" color: %1; }\n").arg(textColor.name());

	// Set the hover state class:
	QString hover;
	if (m_underlining == OnMouseHover)
		hover = "text-decoration: underline;";
	else if (m_underlining == OnMouseOutside)
		hover = "text-decoration: none;";
	if (effectiveHoverColor() != effectiveColor()) {
		if (!hover.isEmpty())
			hover += " ";
		hover += QString("color: %4;").arg(effectiveHoverColor().name());
	}

	// But include it only if it contain a different style than non-hover state:
	if (!hover.isEmpty())
		css += QString("   .%1 a:hover { %2 }\n").arg(cssClass, hover);

	return css;
}

/** LinkLabel */

LinkLabel::LinkLabel(int hAlign, int vAlign, QWidget *parent, const char *name, WFlags f)
 : QFrame(parent, name, f), m_isSelected(false), m_isHovered(false), m_look(0)
{
	initLabel(hAlign, vAlign);
}

LinkLabel::LinkLabel(const QString &title, const QString &icon, LinkLook *look, int hAlign, int vAlign,
                     QWidget *parent, const char *name, WFlags f)
 : QFrame(parent, name, f), m_isSelected(false), m_isHovered(false), m_look(0)
{
	initLabel(hAlign, vAlign);
	setLink(title, icon, look);
}

void LinkLabel::initLabel(int hAlign, int vAlign)
{
	m_layout  = new QBoxLayout(this, QBoxLayout::LeftToRight);
	m_icon    = new QLabel(this);
	m_title   = new QLabel(this);
	m_spacer1 = new QSpacerItem(0, 0, QSizePolicy::Preferred/*Expanding*/, QSizePolicy::Preferred/*Expanding*/);
	m_spacer2 = new QSpacerItem(0, 0, QSizePolicy::Preferred/*Expanding*/, QSizePolicy::Preferred/*Expanding*/);

	m_hAlign = hAlign;
	m_vAlign = vAlign;

	m_title->setTextFormat(Qt::PlainText);

	// DEGUB:
	//m_icon->setPaletteBackgroundColor("lightblue");
	//m_title->setPaletteBackgroundColor("lightyellow");
}

LinkLabel::~LinkLabel()
{
}

void LinkLabel::setLink(const QString &title, const QString &icon, LinkLook *look)
{
	if (look)
		m_look = look; // Needed for icon size

	m_title->setText(title);
	m_title->setShown( ! title.isEmpty() );

	if (icon.isEmpty())
		m_icon->clear();
	else
		m_icon->setPixmap( DesktopIcon(icon, m_look->iconSize()) );
	m_icon->setShown( ! icon.isEmpty() );

	if (look)
		setLook(look);
}

void LinkLabel::setLook(LinkLook *look) // FIXME: called externaly (so, without setLink()) it's buggy (icon not
{
	m_look = look;

	QFont font;
	font.setBold(look->bold());
	font.setUnderline(look->underlineOutside());
	font.setItalic(look->italic());
	m_title->setFont(font);
	m_title->setPaletteForegroundColor( m_isSelected ? KApplication::palette().active().highlightedText() : look->effectiveColor() );

	m_icon->setShown( m_icon->pixmap() && ! m_icon->pixmap()->isNull() );

	setAlign(m_hAlign, m_vAlign);
}

void LinkLabel::setAlign(int hAlign, int vAlign)
{
	m_hAlign = hAlign;
	m_vAlign = vAlign;

	if (!m_look)
		return;

	// Define alignment flags :
	//FIXME TODO: Use directly flags !
	int hFlag, vFlag, wBreak;
	switch (hAlign) {
		default:
		case 0: hFlag = Qt::AlignLeft;    break;
		case 1: hFlag = Qt::AlignHCenter; break;
		case 2: hFlag = Qt::AlignRight;   break;
	}
	switch (vAlign) {
		case 0: vFlag = Qt::AlignTop;     break;
		default:
		case 1: vFlag = Qt::AlignVCenter; break;
		case 2: vFlag = Qt::AlignBottom;  break;
	}
	wBreak = Qt::WordBreak * (hAlign != 1);

	// Clear the widget :
	m_layout->removeItem(m_spacer1);
	m_layout->remove(m_icon);
	m_layout->remove(m_title);
	m_layout->removeItem(m_spacer2);

	// Otherwise, minimumSize will be incoherent (last size ? )
	m_layout->setResizeMode(QLayout::Minimum);

	// And re-populate the widget with the appropriates things and order
	bool addSpacers = hAlign == 1;
	m_layout->setDirection(QBoxLayout::LeftToRight);
	//m_title->setSizePolicy( QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum/*Expanding*/, 0, 0, false) );
	m_icon->setSizePolicy( QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred/*Expanding*/, 0, 0, false) );
	m_spacer1->changeSize( 0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred/*Expanding*/ );
	m_spacer2->changeSize( 0, 0, QSizePolicy::Expanding, QSizePolicy::Preferred/*Expanding*/ );

	m_icon->setAlignment( hFlag | vFlag );
	m_title->setAlignment( hFlag | vFlag | wBreak );
	if ( addSpacers && (vAlign != 0) ||
	   (m_title->text().isEmpty() && hAlign == 2) )
		m_layout->addItem(m_spacer1);
	if (hAlign == 2) { // If align at right, icon is at right
		m_layout->addWidget(m_title);
		m_layout->addWidget(m_icon);
	} else {
		m_layout->addWidget(m_icon);
		m_layout->addWidget(m_title);
	}
	if ( addSpacers && (vAlign != 2) ||
	   (m_title->text().isEmpty() && hAlign == 0) )
		m_layout->addItem(m_spacer2);
}

void LinkLabel::enterEvent(QEvent*)
{
	m_isHovered = true;
	if ( ! m_isSelected )
		m_title->setPaletteForegroundColor(m_look->effectiveHoverColor());

	QFont font = m_title->font();
	font.setUnderline(m_look->underlineInside());
	m_title->setFont(font);
}

void LinkLabel::leaveEvent(QEvent*)
{
	m_isHovered = false;
	if ( ! m_isSelected )
		m_title->setPaletteForegroundColor(m_look->effectiveColor());

	QFont font = m_title->font();
	font.setUnderline(m_look->underlineOutside());
	m_title->setFont(font);
}

void LinkLabel::setSelected(bool selected)
{
	m_isSelected = selected;
	if (selected)
		m_title->setPaletteForegroundColor(KApplication::palette().active().highlightedText());
	else if (m_isHovered)
		m_title->setPaletteForegroundColor(m_look->effectiveHoverColor());
	else
		m_title->setPaletteForegroundColor(m_look->effectiveColor());
}

void LinkLabel::setPaletteBackgroundColor(const QColor &color)
{
	QFrame::setPaletteBackgroundColor(color);
	m_title->setPaletteBackgroundColor(color);
}

int LinkLabel::heightForWidth(int w) const
{
	int iconS  = (m_icon->isShown())   ? m_look->iconSize()                 : 0; // Icon size
	int iconW  = iconS;                                                          // Icon width to remove to w
	int titleH = (m_title->isShown())  ? m_title->heightForWidth(w - iconW) : 0; // Title height

	return (titleH >= iconS) ? titleH : iconS; // No margin for the moment !
}

QString LinkLabel::toHtml(const QString &imageName)
{
	QString begin = "<font color=" + m_look->effectiveColor().name() + ">";
	QString end   = "</font>";
	if (m_look->italic()) {
		begin += "<i>";
		end.prepend("</i>");
	}
	if (m_look->bold()) {
		begin += "<b>";
		end.prepend("</b>");
	}
	if (m_look->underlineOutside()) {
		begin += "<u>";
		end.prepend("</u>");
	}
	if (m_icon->pixmap()) {
		QPixmap icon(*m_icon->pixmap());
		begin.prepend("<img src=" + imageName + " style=\"vertical-align: middle\"> ");
		QMimeSourceFactory::defaultFactory()->setPixmap(imageName, icon);
	} else
		QMimeSourceFactory::defaultFactory()->setData(imageName, 0L);
	return begin + Tools::textToHTMLWithoutP(m_title->text()) + end;
}

/** class LinkDisplay
 */

LinkDisplay::LinkDisplay()
 : m_title(), m_icon(), m_preview(), m_look(0), m_font(), m_minWidth(0), m_width(0), m_height(0)
{
}

void LinkDisplay::setLink(const QString &title, const QString &icon, LinkLook *look, const QFont &font)
{
	setLink(title, icon, m_preview, look, font);
}

void LinkDisplay::setLink(const QString &title, const QString &icon, const QPixmap &preview, LinkLook *look, const QFont &font)
{
	m_title   = title;
	m_icon    = icon;
	m_preview = preview;
	m_look    = look;
	m_font    = font;

	// "Constants":
	int BUTTON_MARGIN = kapp->style().pixelMetric(QStyle::PM_ButtonMargin);
	int LINK_MARGIN   = BUTTON_MARGIN + 2;

	// Recompute m_minWidth:
	QRect textRect = QFontMetrics(labelFont(font, false)).boundingRect(0, 0, /*width=*/1, 500000, Qt::AlignAuto | Qt::AlignTop | Qt::WordBreak, m_title);
	int iconPreviewWidth = QMAX(m_look->iconSize(), (m_look->previewEnabled() ? m_preview.width() : 0));
	m_minWidth = BUTTON_MARGIN - 1 + iconPreviewWidth + LINK_MARGIN + textRect.width();
	// Recompute m_maxWidth:
	textRect = QFontMetrics(labelFont(font, false)).boundingRect(0, 0, /*width=*/50000000, 500000, Qt::AlignAuto | Qt::AlignTop | Qt::WordBreak, m_title);
	m_maxWidth = BUTTON_MARGIN - 1 + iconPreviewWidth + LINK_MARGIN + textRect.width();
	// Adjust m_width:
	if (m_width < m_minWidth)
		setWidth(m_minWidth);
	// Recompute m_height:
	m_height = heightForWidth(m_width);
}

void LinkDisplay::setWidth(int width)
{
	if (width < m_minWidth)
		width = m_minWidth;

	if (width != m_width) {
		m_width  = width;
		m_height = heightForWidth(m_width);
	}
}

/** Paint on @p painter
  *       in (@p x, @p y, @p width, @p height)
  *       using @p colorGroup for the button drawing (if @p isHovered)
  *       and the LinkLook color() for the text,
  *       unless [the LinkLook !color.isValid() and it does not useLinkColor()] or [@p isDefaultColor is false]: in this case it will use @p colorGroup.text().
  *       It will draw the button if @p isIconButtonHovered.
  */
void LinkDisplay::paint(QPainter *painter, int x, int y, int width, int height, const QColorGroup &colorGroup,
                        bool isDefaultColor, bool isSelected, bool isHovered, bool isIconButtonHovered) const
{
	int BUTTON_MARGIN = kapp->style().pixelMetric(QStyle::PM_ButtonMargin);
	int LINK_MARGIN   = BUTTON_MARGIN + 2;

	QPixmap pixmap;
	// Load the preview...:
	if (!isHovered && m_look->previewEnabled() && !m_preview.isNull())
		pixmap  = m_preview;
	// ... Or the icon (if no preview or if the "Open" icon should be shown):
	else {
		int           iconSize   = m_look->iconSize();
		QString       iconName   = (isHovered ? Global::openNoteIcon() : m_icon);
		KIcon::States iconState  = (isIconButtonHovered ? KIcon::ActiveState : KIcon::DefaultState);
		pixmap = kapp->iconLoader()->loadIcon(iconName, KIcon::Desktop, iconSize, iconState, 0L, /*canReturnNull=*/false);
	}
	int iconPreviewWidth  = QMAX(m_look->iconSize(), (m_look->previewEnabled() ? m_preview.width()  : 0));
	int pixmapX = (iconPreviewWidth - pixmap.width()) / 2;
	int pixmapY = (height - pixmap.height()) / 2;
	// Draw the button (if any) and the icon:
	if (isHovered)
		kapp->style().drawPrimitive(QStyle::PE_ButtonCommand, painter, QRect(-1, -1, iconPreviewWidth + 2*BUTTON_MARGIN, height + 2),
		                            colorGroup, QStyle::Style_Enabled | (isIconButtonHovered ? QStyle::Style_MouseOver : 0));
	painter->drawPixmap(x + BUTTON_MARGIN - 1 + pixmapX, y + pixmapY, pixmap);

	// Figure out the text color:
	if (isSelected)
		painter->setPen(KGlobalSettings::highlightedTextColor());
	else if (isIconButtonHovered)
		painter->setPen(m_look->effectiveHoverColor());
	else if (!isDefaultColor || (!m_look->color().isValid() && !m_look->useLinkColor())) // If the color is FORCED or if the link color default to the text color:
		painter->setPen(colorGroup.text());
	else
		painter->setPen(m_look->effectiveColor());
	// Draw the text:
	painter->setFont(labelFont(m_font, isIconButtonHovered));
	painter->drawText(x + BUTTON_MARGIN - 1 + iconPreviewWidth + LINK_MARGIN, y, width - BUTTON_MARGIN + 1 - iconPreviewWidth - LINK_MARGIN, height,
	                  Qt::AlignAuto | Qt::AlignVCenter | Qt::WordBreak, m_title);
}

QPixmap LinkDisplay::feedbackPixmap(int width, int height, const QColorGroup &colorGroup, bool isDefaultColor)
{
	int theWidth  = QMIN(width, maxWidth());
	int theHeight = QMIN(height, heightForWidth(theWidth));
	QPixmap pixmap(theWidth, theHeight);
	pixmap.fill(colorGroup.background());
	QPainter painter(&pixmap);
	paint(&painter, 0, 0, theWidth, theHeight, colorGroup, isDefaultColor,
	      /*isSelected=*/false, /*isHovered=*/false, /*isIconButtonHovered=*/false);
	painter.end();
	return pixmap;
}

bool LinkDisplay::iconButtonAt(const QPoint &pos) const
{
	int BUTTON_MARGIN    = kapp->style().pixelMetric(QStyle::PM_ButtonMargin);
//	int LINK_MARGIN      = BUTTON_MARGIN + 2;
	int iconPreviewWidth = QMAX(m_look->iconSize(), (m_look->previewEnabled() ? m_preview.width()  : 0));

	return pos.x() <= BUTTON_MARGIN - 1 + iconPreviewWidth + BUTTON_MARGIN;
}

QRect LinkDisplay::iconButtonRect() const
{
	int BUTTON_MARGIN    = kapp->style().pixelMetric(QStyle::PM_ButtonMargin);
//	int LINK_MARGIN      = BUTTON_MARGIN + 2;
	int iconPreviewWidth = QMAX(m_look->iconSize(), (m_look->previewEnabled() ? m_preview.width()  : 0));

	return QRect(0, 0, BUTTON_MARGIN - 1 + iconPreviewWidth + BUTTON_MARGIN, m_height);
}

QFont LinkDisplay::labelFont(QFont font, bool isIconButtonHovered) const
{
	if (m_look->italic())
		font.setItalic(true);
	if (m_look->bold())
		font.setBold(true);
	if (isIconButtonHovered) {
		if (m_look->underlineInside())
			font.setUnderline(true);
	} else {
		if (m_look->underlineOutside())
			font.setUnderline(true);
	}
	return font;
}

int LinkDisplay::heightForWidth(int width) const
{
	int BUTTON_MARGIN     = kapp->style().pixelMetric(QStyle::PM_ButtonMargin);
	int LINK_MARGIN       = BUTTON_MARGIN + 2;
	int iconPreviewWidth  = QMAX(m_look->iconSize(), (m_look->previewEnabled() ? m_preview.width()  : 0));
	int iconPreviewHeight = QMAX(m_look->iconSize(), (m_look->previewEnabled() ? m_preview.height() : 0));

	QRect textRect = QFontMetrics(labelFont(m_font, false)).boundingRect(0, 0, width - BUTTON_MARGIN + 1 - iconPreviewWidth - LINK_MARGIN, 500000, Qt::AlignAuto | Qt::AlignTop | Qt::WordBreak, m_title);
	return QMAX(textRect.height(), iconPreviewHeight + 2*BUTTON_MARGIN - 2);
}

QString LinkDisplay::toHtml(const QString &/*imageName*/) const
{
	// TODO
	return "";
}

QString LinkDisplay::toHtml(const HtmlExportData &exportData, const KURL &url, const QString &title)
{
	QString linkIcon;
	if (m_look->previewEnabled() && !m_preview.isNull()) {
		QString fileName = Tools::fileNameForNewFile("preview_" + url.fileName() + ".png", exportData.iconsFolderPath);
		QString fullPath = exportData.iconsFolderPath + fileName;
		m_preview.save(fullPath, "PNG");
		linkIcon = QString("<img src=\"%1\" width=\"%2\" height=\"%3\" alt=\"\">")
		           .arg(exportData.iconsFolderName + fileName, QString::number(m_preview.width()), QString::number(m_preview.height()));
	} else {
		linkIcon = exportData.iconsFolderName + Basket::copyIcon(m_icon, m_look->iconSize(), exportData.iconsFolderPath);
		linkIcon = QString("<img src=\"%1\" width=\"%2\" height=\"%3\" alt=\"\">")
		           .arg(linkIcon, QString::number(m_look->iconSize()), QString::number(m_look->iconSize()));
	}

	QString linkTitle = Tools::textToHTMLWithoutP(title.isEmpty() ? m_title : title);

	return QString("<a href=\"%1\">%2 %3</a>").arg(url.prettyURL(), linkIcon, linkTitle);
}

/** LinkLookEditWidget **/

LinkLookEditWidget::LinkLookEditWidget(LinkLook *look, const QString exTitle, const QString exIcon,
                                       QWidget *parent, const char *name, WFlags fl)
 : QWidget(parent, name, fl)
{
	QLabel      *label;
	QVBoxLayout *layout = new QVBoxLayout(this, KDialogBase::marginHint(), KDialogBase::spacingHint());

	m_look = look;

	m_italic = new QCheckBox(i18n("I&talic"), this);
	m_italic->setChecked(look->italic());
	layout->addWidget(m_italic);

	m_bold = new QCheckBox(i18n("&Bold"), this);
	m_bold->setChecked(look->bold());
	layout->addWidget(m_bold);

	QGridLayout *gl = new QGridLayout(layout, /*rows=*//*(look->canPreview() ? 5 : 4)*/5, /*columns=*//*3*/4);
	gl->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding), 1, /*2*/3);

	m_underlining = new QComboBox(false, this);
	m_underlining->insertItem(i18n("Always"));
	m_underlining->insertItem(i18n("Never"));
	m_underlining->insertItem(i18n("On mouse hovering"));
	m_underlining->insertItem(i18n("When mouse is outside"));
	m_underlining->setCurrentItem(look->underlining());
	label = new QLabel(m_underlining, i18n("&Underline:"), this);
	gl->addWidget(label,         0, 0);
	gl->addWidget(m_underlining, 0, 1);

	m_color = new KColorCombo2(m_look->color(), m_look->defaultColor(), this);
	label = new QLabel(m_color, i18n("Colo&r:"), this);
	gl->addWidget(label,   1, 0);
	gl->addWidget(m_color, 1, 1);

	m_hoverColor = new KColorCombo2(m_look->hoverColor(), m_look->defaultHoverColor(), this);
	label = new QLabel(m_hoverColor, i18n("&Mouse hover color:"), this);
	gl->addWidget(label,        2, 0);
	gl->addWidget(m_hoverColor, 2, 1);

	QHBoxLayout *icoLay = new QHBoxLayout(/*parent=*/0L, /*margin=*/0, KDialogBase::spacingHint());
	m_iconSize = new IconSizeCombo(false, this);
	m_iconSize->setSize(look->iconSize());
	icoLay->addWidget(m_iconSize);
	label = new QLabel(m_iconSize, i18n("&Icon size:"), this);
	gl->addWidget(label,  3, 0);
	gl->addItem(  icoLay, 3, 1);

	m_preview = new QComboBox(false, this);
	m_preview->insertItem(i18n("None"));
	m_preview->insertItem(i18n("Icon size"));
	m_preview->insertItem(i18n("Twice the icon size"));
	m_preview->insertItem(i18n("Three times the icon size"));
	m_preview->setCurrentItem(look->preview());
	label = new QLabel(m_preview, i18n("&Preview:"), this);
	HelpLabel *hLabel = new HelpLabel(
		i18n("You disabled preview but still see images?"),
		i18n("<p>This is normal because there are several type of notes.<br>"
		     "This setting applies only to file and local link notes.<br>"
		     "And the images you see are image notes, not file notes.<br>"
		     "File notes are generic documents, whereas image notes are pictures you can draw in.</p>"
		     "<p>When dropping files to baskets, %1 detects theire type and show you the content of the files.<br>"
		     "For instance, when dropping image or text files, image and text notes are created for them.<br>"
		     "For type of files %2 does not understand, they are shown as generic file notes with just an icon or file preview and a filename.</p>"
		     "<p>If you do not want the application to create notes depending on the content of the files you drop, "
		     "go to the \"General\" page and uncheck \"Image or animation\" in the \"View Content of Added Files for the Following Types\" group.</p>")
		// TODO: Note: you can resize down maximum size of images...
			.arg(kapp->aboutData()->programName(), kapp->aboutData()->programName()),
		this);
	gl->addWidget(label,     4, 0);
	gl->addWidget(m_preview, 4, 1);
	gl->addMultiCellWidget(hLabel, /*fromRow=*/5, /*toRow=*/5, /*fromCol=*/1, /*toCol*/2);
	connect( m_preview, SIGNAL(activated(int)), this, SLOT(slotChangeLook(int)) );
	if (!look->canPreview()) {
		label->setEnabled(false);
		hLabel->setEnabled(false);
		m_preview->setEnabled(false);
	}

	QGroupBox *gb = new QHGroupBox(i18n("Example"), this);
	m_exLook = new LinkLook(*look);
	m_example = new LinkLabel(exTitle, exIcon, m_exLook, 1, 1, gb);
	m_example->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_example->setCursor(QCursor(Qt::PointingHandCursor));
	layout->addWidget(gb);
	m_exTitle = exTitle;
	m_exIcon  = exIcon;

	slotChangeLook();

	connect( m_italic,      SIGNAL(clicked()),              this, SLOT(slotChangeLook())              );
	connect( m_bold,        SIGNAL(clicked()),              this, SLOT(slotChangeLook())              );
	connect( m_underlining, SIGNAL(activated(int)),         this, SLOT(slotChangeLook(int))           );
	connect( m_color,       SIGNAL(changed(const QColor&)), this, SLOT(slotChangeLook(const QColor&)) );
	connect( m_hoverColor,  SIGNAL(changed(const QColor&)), this, SLOT(slotChangeLook(const QColor&)) );
	connect( m_iconSize,    SIGNAL(activated(int)),         this, SLOT(slotChangeLook(int))           );
}


void LinkLookEditWidget::slotChangeLook(const QColor&)
{
	slotChangeLook();
}

void LinkLookEditWidget::slotChangeLook()
{
	saveToLook(m_exLook);
	m_example->setLink(m_exTitle, m_exIcon, m_exLook); // and can't reload it at another size
}

void LinkLookEditWidget::slotChangeLook(int)
{
	slotChangeLook();
}

LinkLookEditWidget::~LinkLookEditWidget()
{
}

void LinkLookEditWidget::saveChanges()
{
	saveToLook(m_look);
}

void LinkLookEditWidget::saveToLook(LinkLook *look)
{
	look->setLook( m_italic->isOn(), m_bold->isOn(), m_underlining->currentItem(),
	               m_color->color(), m_hoverColor->color(),
	               m_iconSize->iconSize(), (look->canPreview() ? m_preview->currentItem() : LinkLook::None) );
}

#include "linklabel.moc"