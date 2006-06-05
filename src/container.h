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

#ifndef CONTAINER_H
#define CONTAINER_H

#include <kmainwindow.h>
#include <qtabbar.h>
#include <qtabwidget.h>
#include <qlabel.h>
#include <ksystemtray.h>
#include <qptrlist.h>
#include <qpoint.h>
#include <qclipboard.h>
#include <kaction.h>
#include <qpixmap.h>
#include <qdesktopwidget.h>
#include <qtimer.h>
#include <qsplitter.h>
#include <klistview.h>

class QWidget;
class QPoint;
class KAction;
class KToggleAction;
class QPopupMenu;
class QSignalMapper;
class QStringList;
class QToolTipGroup;
class KPassivePopup;

class DecoratedBasket;
class Basket;
class Container;
class RegionGrabber;


/// NEW:

class QWidgetStack;

class BasketListViewItem : public QListViewItem
{
  public:
	/// CONSTRUCTOR AND DESTRUCTOR:
	BasketListViewItem(QListView     *parent, Basket *basket);
	BasketListViewItem(QListViewItem *parent, Basket *basket);
	BasketListViewItem(QListView     *parent, QListViewItem *after, Basket *basket);
	BasketListViewItem(QListViewItem *parent, QListViewItem *after, Basket *basket);
	~BasketListViewItem();
	///
	bool acceptDrop(const QMimeSource *mime) const;
	void dropped(QDropEvent *event);
	Basket *basket() { return m_basket; }
	void setup();
	int width(const QFontMetrics &fontMetrics, const QListView *listView, int column) const;
	BasketListViewItem* lastChild();
	BasketListViewItem* prevSibling();
	BasketListViewItem* shownItemAbove();
	BasketListViewItem* shownItemBelow();
	QStringList childNamesTree(int deep = 0);
	void moveChildsBaskets();
	void ensureVisible();
	bool isShown();
	bool isCurrentBasket();
	void paintCell(QPainter *painter, const QColorGroup &colorGroup, int column, int width, int align);
	QString escapedName(const QString &string);
	///
	QPixmap circledTextPixmap(const QString &text, int height, const QFont &font, const QColor &color);
	QPixmap foundCountPixmap(bool isLoading, int countFound, bool childsAreLoading, int countChildsFound, const QFont &font, int height);
	bool haveChildsLoading();
	bool haveHiddenChildsLoading();
	int countChildsFound();
	int countHiddenChildsFound();
	///
//	QDragObject* dragObject();
//	bool acceptDrop ( const QMimeSource * mime ) const;
  private:
	Basket *m_basket;
	int     m_width;
};

class BasketTreeListView : public KListView
{
  Q_OBJECT
  public:
	BasketTreeListView(QWidget *parent = 0, const char *name = 0);
	void contentsDragEnterEvent(QDragEnterEvent *event);
	void removeExpands();
	void contentsDragLeaveEvent(QDragLeaveEvent *event);
	void contentsDragMoveEvent(QDragMoveEvent *event);
	void contentsDropEvent(QDropEvent *event);
	void resizeEvent(QResizeEvent *event);
	void paintEmptyArea(QPainter *painter, const QRect &rect);
  protected:
	void focusInEvent(QFocusEvent*);
  private:
	QTimer         m_autoOpenTimer;
	QListViewItem *m_autoOpenItem;
  private slots:
	void autoOpen();
};

class BasketTree : public QSplitter
{
  Q_OBJECT
  public:
	/// CONSTRUCTOR AND DESTRUCTOR:
	BasketTree(QWidget *parent = 0);
	~BasketTree();
	/// MANAGE CONFIGURATION EVENTS!:
	void setTreePlacement(bool onLeft);
	void relayoutAllBaskets();
	void linkLookChanged();
	void filterPlacementChanged(bool onTop);
	/// MANAGE BASKETS:
	BasketListViewItem* listViewItemForBasket(Basket *basket);
	Basket* currentBasket();
	Basket* parentBasketOf(Basket *basket);
	void setCurrentBasket(Basket *basket);
	void removeBasket(Basket *basket);
	/// For NewBasketDialog (and later some other classes):
	QListViewItem* firstListViewItem();
	///
	BasketListViewItem* lastListViewItem();
	int basketCount(QListViewItem *parent = 0);
	bool canFold();
	bool canExpand();
  public slots:
	void updateBasketListViewItem(Basket *basket);
	void save();
	void save(QListViewItem *firstItem, QDomDocument &document, QDomElement &parentElement);
	void load();
	void load(KListView *listView, QListViewItem *item, const QDomElement &baskets);
	void loadNewBasket(const QString &folderName, const QDomElement &properties, Basket *parent);
	void goToPreviousBasket();
	void goToNextBasket();
	void foldBasket();
	void expandBasket();
	void closeAllEditors();
	///
	void toggleFilterAllBaskets(bool doFilter);
	void newFilter();
	void newFilterFromFilterBar();
	bool isFilteringAllBaskets();
  private:
	Basket* loadBasket(const QString &folderName);
	BasketListViewItem* appendBasket(Basket *basket, QListViewItem *parentItem);
  private slots:
	void slotPressed(QListViewItem *item, const QPoint &/*pos*/ = QPoint(), int /*column*/ = 0);
	void needSave(QListViewItem*);
	void slotContextMenu(KListView *listView, QListViewItem *item, const QPoint &pos);
	void slotMouseButtonPressed(int button, QListViewItem *item, const QPoint &pos, int column);
	void slotShowProperties(QListViewItem *item, const QPoint&, int);
  signals:
	void basketNumberChanged(int number);
	void basketChanged();
  private:
	KListView    *m_tree;
	QWidgetStack *m_stack;
	bool          m_loading;
};

/** Class to pick a color on the screen
  * @author S�bastien Lao�t
  */
class DesktopColorPicker : public QDesktopWidget
{
  Q_OBJECT
  public:
	/** Construtor, initializer and destructor */
	DesktopColorPicker();
	~DesktopColorPicker();
  public slots:
	/** Begin color picking.
	  * This function returns immediatly, and pickedColor() is emitted if user has
	  * choosen a color, and not canceled the process (by pressing Escape).
	  */
	void pickColor();
  signals:
	/** When user picked a color, this signal is emitted.
	  */
	void pickedColor(const QColor &color);
	/** When user cancel a picking (by pressing Escape), this signal is emitted.
	  */
	void canceledPick();
  protected slots:
	void slotDelayedPick();
  protected:
	void mouseReleaseEvent(QMouseEvent *event);
	void keyPressEvent(QKeyEvent *event);
	bool m_gettingColorFromScreen;
};

/** This class is a QLabel that can emit a clicked() signal when clicked !
  * @author S�bastien Lao�t
  */
class ClickableLabel : public QLabel
{
  Q_OBJECT
  public:
	/** Construtor, initializer and destructor */
	ClickableLabel(QWidget *parent = 0, const char *name = 0)
	 : QLabel(parent, name) {}
	~ClickableLabel()       {}
  signals:
	void clicked();
  protected:
	virtual void mousePressEvent(QMouseEvent *event)
	{
		if (event->button() & Qt::LeftButton)
			emit clicked();
	}
};

/** Convenient class to develop the displayCloseMessage() dialog
  * hopefuly integrated in KDE 3.4
  * @author S�bastien Lao�t
  */
class KSystemTray2 : public KSystemTray
{
  Q_OBJECT
  public:
	KSystemTray2(QWidget *parent = 0, const char *name = 0);
	~KSystemTray2();
	/**
	  * Call this method when the user clicked the close button of the window
	  * (the [x]) to inform him that the application sit in the system tray
	  * and willn't be closed (as he is used to).
	  *
	  * You usualy call it from reimplemented KMainWindow::queryClose()
	  *
	  * @since 3.4
	  */
	void displayCloseMessage(QString fileMenu = "");
};

/** This class provide a personalized system tray icon.
  * @author S�bastien Lao�t
  */
class ContainerSystemTray : public KSystemTray2
{
  Q_OBJECT
  public:
	ContainerSystemTray(QWidget *parent = 0, const char *name = 0);
	~ContainerSystemTray();
  protected:
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual void dragMoveEvent(QDragMoveEvent* event);
	virtual void dragLeaveEvent(QDragLeaveEvent*);
	virtual void dropEvent(QDropEvent *event);
	void wheelEvent(QWheelEvent *event);
	void enterEvent(QEvent*);
	void leaveEvent(QEvent*);
  public slots:
	void updateToolTip();
  protected slots:
	void updateToolTipDelayed();
	void showBasket(int index);
  private:
	QTimer    *m_showTimer;
	QTimer    *m_autoShowTimer;
	bool       m_canDrag;
	QPoint     m_pressPos;
	Container *m_parentContainer;
	QPixmap    m_iconPixmap;
	QPixmap    m_lockedIconPixmap;
};

/** This class provide a QTabBar with a popup menu for the baskets
  * and drag and drop possibilities.
  * @author S�bastien Lao�t
  */
class ContainerTabBar : public QTabBar
{
  Q_OBJECT
  public:
	ContainerTabBar(QWidget *parent = 0, const char *name = 0);
	~ContainerTabBar();
	int tabIndexForCursorPosition(const QPoint &curPos);
	void updateToolTips(const QStringList &tips);
  protected:
	virtual void dragEnterEvent(QDragEnterEvent*);
	virtual void dragMoveEvent(QDragMoveEvent *event);
	virtual void dragLeaveEvent(QDragLeaveEvent*);
	virtual void dropEvent(QDropEvent *event);
	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent*);
	virtual void mouseDoubleClickEvent(QMouseEvent *event);
	virtual void contextMenuEvent(QContextMenuEvent *event);
  signals:
	void doubleClicked(int tabIndex);
	void wantContextMenu(int tabIndex, const QPoint &globalPos);
	void wantContextMenu(int tabIndex, const QRect  &globalRect);
	void wantShowPage(int tabIndex);
	void wantPaste(int tabIndex, QClipboard::Mode mode);
  private slots:
	void timeout();

  private:
	QTimer        *m_showTimer;
	int            m_basketIndexToMove;
	Basket        *m_pointedBasket;
	int            m_pointedBasketIndex;
	QToolTipGroup *m_ttGroup;
};

/** This class provide a QTabWidget with a popup menu for the tabs.
  * @author S�bastien Lao�t
  */
class ContainerTabWidget : public QTabWidget
{
  Q_OBJECT
  public:
	ContainerTabWidget(QWidget *parent = 0, const char *name = 0);
	~ContainerTabWidget();
	void updateToolTips(const QStringList &tips);
  signals:
	void doubleClicked(int tabIndex);
	void contextMenu(int tabIndex, const QPoint &globalPos);
	void contextMenu(int tabIndex, const QRect  &globalRect);
	void wantPaste(int tabIndex, QClipboard::Mode mode);
  public slots:
	void setCurrentPage(int index); // We overload it to not try to show a page that is already shown
  private slots:
	void slotWantPaste(int tabIndex, QClipboard::Mode mode);
	void slotDoubleClicked(int tabIndex);
	void slotContextMenu(int tabIndex, const QPoint &globalPos);
	void slotContextMenu(int tabIndex, const QRect  &globalRect);
  protected:
	virtual void contextMenuEvent(QContextMenuEvent *event);
};

/** The window that contain baskets, organized by tabs.
  * @author S�bastien Lao�t
  */
class Container : public KMainWindow
{
  Q_OBJECT
  public:
	/** Construtor, initializer and destructor */
	Container(QWidget *parent = 0, const char *name = 0);
	~Container();
  private:
	void setupActions();
	void setupStatusBar();
  public slots:
	/** GUI Main Window actions **/
	void setStatusBarHint(const QString &hint); /// << Set a specific message or update if hint is empty
	void updateStatusBarHint();                 /// << Display the current state message (dragging, editing) or reset the startsbar message
	void postStatusbarMessage(const QString &text);
	/** User actions */
	void setActive(bool active = true);
	void changeActive();
	void showAppPurpose(); // TODO: Remove
	/** Basket */
	void askNewBasket();
	void askNewSubBasket();
	void aboutToHideNewBasketPopup();
	void setNewBasketPopup();
	void cancelNewBasketPopup();
	void propBasket();
	void delBasket();
	void doBasketDeletion(Basket *basket);
	void lockBasket();
	void exportToHTML();
	void importKNotes();
	void importKJots();
	void importStickyNotes();
	void importTomboy();
	void importKnowIt();
	void hideOnEscape();
	bool askForQuit();
	/** Edit */
	void undo();
	void redo();
	void cutNote();
	void copyNote();
	void pasteInCurrentBasket();
	void pasteSelInCurrentBasket();
	void pasteToBasket(int index, QClipboard::Mode mode = QClipboard::Clipboard);
	void delNote();
	void slotSelectAll();
	void slotUnselectAll();
	void slotInvertSelection();
	void showHideFilterBar(bool show, bool switchFocus = true);
	void slotResetFilter();
	/** Note */
	void editNote();
	void openNote();
	void openNoteWith();
	void saveNoteAs();
	void noteGroup();
	void noteUngroup();
	void moveOnTop();
	void moveOnBottom();
	void moveNoteUp();
	void moveNoteDown();
	void activatedTagShortcut();
	/** Insert **/
	void insertEmpty(int type);
	void insertWizard(int type);
	void slotColorFromScreen(bool global = false);
	void slotColorFromScreenGlobal();
	void colorPicked(const QColor &color);
	void colorPickingCanceled();
	void grabScreenshot(bool global = false);
	void grabScreenshotGlobal();
	void screenshotGrabbed(const QPixmap &pixmap);
	/** Settings **/
	void toggleToolBar();
	void toggleStatusBar();
	void showShortcutsSettingsDialog();
	void showGlobalShortcutsSettingsDialog();
	void configureToolbars();
	void configureNotifications();
	void showSettingsDialog();
  public:
	/** Necessary to the System tray popup menu, and others... */
	Basket* currentBasket();
	DecoratedBasket* currentDecoratedBasket();
	Basket* basketForFolderName(const QString &folderName);
	QPopupMenu* popupMenu(const QString &menuName);
  private:
	QString m_passiveDroppedTitle;
	bool    m_passiveDroppedShortTypeName;
	KPassivePopup *m_passivePopup;
  public:
	static const int c_delayTooltipTime;
  public slots:
	void setCurrentBasket(int index);
	void setCurrentBasket(Basket *basket);
	void countsChanged(Basket *basket);
	/** Global shortcuts */
	void showPassiveDropped(const QString &title, bool showTypeName = true);
		// %1 will be replaced by typeName(), unless showTypeName is false (it will then remove %1)
		// %2 with currentBasket()->name()
	void showPassiveDroppedDelayed(); // Do showPassiveDropped(), but delayed
	void showPassiveContent();
	void showPassiveImpossible(const QString &message);
	// For GUI :
	void setFiltering(bool filtering);
  protected:
	bool queryExit();
	bool queryClose();
	void enterEvent(QEvent*);
	void leaveEvent(QEvent*);
	virtual void resizeEvent(QResizeEvent*);
	virtual void moveEvent(QMoveEvent*);
  public:
	void polish();
  private slots:
	/** User actions */
	void contextMenu(int tabIndex, const QPoint &globalPos);
	void contextMenu(int tabIndex, const QRect  &globalRect);
	void changedSelectedNotes();
	void timeoutTryHide();
	void timeoutHide();
	void basketNumberChanged(int number);
	void basketChanged();
	void currentBasketChanged();
	void isLockedChanged();
	void updateNotesActions();
	void slotNewToolbarConfig();
  public slots:
	void countSelectedsChanged();
  public: // TODO: Migrate to private
	KAction       *actNewBasket;
	KAction       *actNewSubBasket;
	KAction       *m_actHideWindow;
	KAction       *actQuit;
	KToggleAction *m_actFilterAllBaskets;
	KAction       *m_actEditNote;
	KAction       *m_actOpenNote;
	KAction       *m_actGrabScreenshot;
	KAction       *m_actColorPicker;
	KAction       *actConfigGlobalShortcuts;
	KAction       *actAppConfig;
	QPtrList<KAction> actBasketsList;
  private:
	bool m_newBasketPopup;
	// Basket actions :
	KAction       *m_actPropBasket;
	KAction       *m_actDelBasket;
	KToggleAction *m_actLockBasket;
	// Edit actions :
	KAction       *m_actUndo;
	KAction       *m_actRedo;
	KAction       *m_actCutNote;
	KAction       *m_actCopyNote;
	KAction       *m_actPaste;
	KAction       *m_actDelNote;
	KAction       *m_actSelectAll;
	KAction       *m_actUnselectAll;
	KAction       *m_actInvertSelection;
	KToggleAction *m_actShowFilter;
	KAction       *m_actResetFilter;
	// Go actions :
	KAction       *m_actPreviousBasket;
	KAction       *m_actNextBasket;
	KAction       *m_actFoldBasket;
	KAction       *m_actExpandBasket;
	// Notes actions :
	KAction       *m_actOpenNoteWith;
	KAction       *m_actSaveNoteAs;
	KAction       *m_actGroup;
	KAction       *m_actUngroup;
	KAction       *m_actMoveOnTop;
	KAction       *m_actMoveNoteUp;
	KAction       *m_actMoveNoteDown;
	KAction       *m_actMoveOnBottom;
	// Insert actions :
	KAction       *m_actInsertText;
	KAction       *m_actInsertHtml;
	KAction       *m_actInsertLink;
	KAction       *m_actInsertImage;
	KAction       *m_actInsertColor;
	KAction       *m_actImportKMenu;
	KAction       *m_actInsertLauncher;
	KAction       *m_actImportIcon;
	KAction       *m_actLoadFile;
//	KAction       *m_actColorPicker;
//	KAction       *m_actGrabScreenshot;
	// Settings actions :
	KToggleAction *m_actShowToolbar;
	KToggleAction *m_actShowStatusbar;
	//
	QPtrList<KAction> m_insertActions;
  private:
	QVBoxLayout        *m_layout;
	BasketTree         *m_baskets;
	QLabel             *m_basketStatus;
	QLabel             *m_selectionStatus;
	ClickableLabel     *m_lockStatus;
	QTimer             *m_tryHideTimer;
	QTimer             *m_hideTimer;
	bool                m_startDocked;
	DesktopColorPicker *m_colorPicker;
	RegionGrabber      *m_regionGrabber;
	bool                m_colorPickWasShown;
	bool                m_colorPickWasGlobal;
};

#endif // CONTAINER_H