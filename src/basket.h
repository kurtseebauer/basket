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

#ifndef BASKET_H
#define BASKET_H

#include <qscrollview.h>
#include <qtooltip.h>
#include <qvaluelist.h>
#include <qtimer.h>
#include <qimage.h>
#include <qdatetime.h>
#include <qclipboard.h>
#include <kshortcut.h>
#include <kaction.h>
#include <kio/job.h>

#include "filter.h"
#include "note.h" // For Note::Zone

class QVBoxLayout;
class QDomDocument;
class QDomElement;

class Basket;
class Note;
class NoteEditor;
class Tag;


/** A list of flags to set how notes are inserted/plugged in the basket
  * Declare a varible with the type PlugOptions::Flags and assign a value like PlugOptions::DoSelection...
  * @author S�bastien Lao�t
  */
namespace PlugOptions
{
	enum Flags {
		SelectOnlyNewNotes = 0x01, /// << Unselect every notes in the basket and select the newly inserted ones
		DoTagsInheriting   = 0x02  /// << The new notes inherit the tags of the sibbling note
	};
	// TODO: FocusLastInsertedNote (last visible!), EnsureVisibleAddedNotes, PopupFeebackBaloon (if not called by hand), AnimateNewPosition, FeedbackUnmatched
	// TODO: moveNoteInTree(bool animate);
}

/** This represent a hierarchy of the selected classes.
  * If this is null, then there is no selected note.
  */
class NoteSelection
{
  public:
	NoteSelection()        : note(0), parent(0), firstChild(0), next(0), fullPath() {}
	NoteSelection(Note *n) : note(n), parent(0), firstChild(0), next(0), fullPath() {}

	Note          *note;
	NoteSelection *parent;
	NoteSelection *firstChild;
	NoteSelection *next;
	QString        fullPath; // Needeed for 'Cut' code to store temporary path of the cutted note.

	NoteSelection* firstStacked();
	NoteSelection* nextStacked();
	void append(NoteSelection *node);
	int count();

	QValueList<Note*> parentGroups();
};

/** This store all needed information when exporting to HTML
  */
class HtmlExportData
{
  public:
	QString iconsFolderPath;
	QString iconsFolderName;
	QString imagesFolderPath;
	QString imagesFolderName;
	QString dataFolderPath;
	QString dataFolderName;
	bool    formatForImpression;
	bool    embedLinkedFiles;
	bool    embedLinkedFolders;
};

/** This class handle Basket and add a FilterWidget on top of it.
  * @author S�bastien Lao�t
  */
class DecoratedBasket : public QWidget
{
  Q_OBJECT
  public:
	DecoratedBasket(QWidget *parent, const QString &folderName, const char *name = 0, WFlags fl = 0);
	~DecoratedBasket();
	void setFilterBarPosition(bool onTop);
	void resetFilter();
	void setFilterBarShown(bool show, bool switchFocus = true);
	bool isFilterBarShown()        { return m_filter->isShown();    }
	const FilterData& filterData() { return m_filter->filterData(); }
	FilterBar* filterBar()         { return m_filter;               }
	Basket*    basket()            { return m_basket;               }
  private:
	QVBoxLayout *m_layout;
	FilterBar   *m_filter;
	Basket      *m_basket;
};

class TransparentWidget : public QWidget
{
  Q_OBJECT
  public:
	TransparentWidget(Basket *basket);
	void setPosition(int x, int y);
	//void reparent(QWidget *parent, WFlags f, const QPoint &p, bool showIt = FALSE);
  protected:
	void paintEvent(QPaintEvent*);
	void mouseMoveEvent(QMouseEvent *event);
	bool eventFilter(QObject *object, QEvent *event);
  private:
	Basket *m_basket;
	int     m_x;
	int     m_y;
};

/**
  * @author S�bastien Lao�t
  */
class Basket : public QScrollView, public QToolTip
{
/// CONSTRUCTOR AND DESTRUCTOR:
  Q_OBJECT
  public:
	Basket(QWidget *parent, const QString &folderName);
	~Basket();

/// USER INTERACTION:
  private:
	bool   m_noActionOnMouseRelease;
	QPoint m_pressPos;
	bool   m_canDrag;
  public:
	void viewportResizeEvent(QResizeEvent *);
	void drawContents(QPainter *painter, int clipX, int clipY, int clipWidth, int clipHeight);
	void enterEvent(QEvent *);
	void leaveEvent(QEvent *);
	void contentsMouseMoveEvent(QMouseEvent *event);
	void contentsMousePressEvent(QMouseEvent *event);
	void contentsMouseReleaseEvent(QMouseEvent *event);
	void contentsMouseDoubleClickEvent(QMouseEvent *event);
	void contentsContextMenuEvent(QContextMenuEvent *event);
	void updateNote(Note *note);
	void clickedToInsert(QMouseEvent *event, Note *clicked = 0, int zone = 0);
  private slots:
	void setFocusIfNotInPopupMenu();

/// LAYOUT:
  private:
	Note   *m_firstNote;
	int     m_columnsCount;
	bool    m_mindMap;
	Note   *m_resizingNote;
	int     m_pickedResizer;
	Note   *m_movingNote;
	QPoint  m_pickedHandle;
  public:
	int tmpWidth;
	int tmpHeight;
  public:
	void unsetNotesWidth();
	void relayoutNotes(bool animate);
	Note* noteAt(int x, int y);
	inline Note* firstNote()       { return m_firstNote;                 }
	inline int   columnsCount()    { return m_columnsCount;              }
	inline bool  isColumnsLayout() { return m_columnsCount > 0;          }
	inline bool  isFreeLayout()    { return m_columnsCount <= 0;         }
	inline bool  isMindMap()       { return isFreeLayout() && m_mindMap; }
	Note* resizingNote()           { return m_resizingNote;              }
	Note* lastNote();
	void setDisposition(int disposition, int columnCount);
	void equalizeColumnSizes();

/// NOTES INSERTION AND REMOVAL:
  public:
	/// The following methods assume that the note(s) to insert already all have 'this' as the parent basket:
	void prependNoteIn(   Note *note, Note *in);     /// << Add @p note (and the next linked notes) as the first note(s) of the group @p in.
	void appendNoteIn(    Note *note, Note *in);     /// << Add @p note (and the next linked notes) as the last note(s) of the group @p in.
	void appendNoteAfter( Note *note, Note *after);  /// << Add @p note (and the next linked notes) just after (just below) the note @p after.
	void appendNoteBefore(Note *note, Note *before); /// << Add @p note (and the next linked notes) just before (just above) the note @p before.
	void groupNoteAfter(  Note *note, Note *with);   /// << Add a group at @p with place, move @p with in it, and add @p note (and the next linked notes) just after the group.
	void groupNoteBefore( Note *note, Note *with);   /// << Add a group at @p with place, move @p with in it, and add @p note (and the next linked notes) just before the group.
	void unplugNote(      Note *note);               /// << Unplug @p note (and its child notes) from the basket (and also decrease counts...).
	                                                 /// <<  After that, you should delete the notes yourself. Do not call prepend/append/group... functions two times: unplug and ok
	void ungroupNote(     Note *group);              /// << Unplug @p group but put child notes at its place.
	/// And this one do almost all the above methods depending on the context:
	void insertNote(Note *note, Note *clicked, int zone, const QPoint &pos = QPoint(), bool animateNewPosition = false);
	void insertCreatedNote(Note *note);
  private:
	void preparePlug(Note *note);
  private:
	Note  *m_clickedToInsert;
	int    m_zoneToInsert;
	QPoint m_posToInsert;
	Note  *m_savedClickedToInsert;
	int    m_savedZoneToInsert;
	QPoint m_savedPosToInsert;
	bool   m_isInsertPopupMenu;
  public:
	void saveInsertionData();
	void restoreInsertionData();
	void resetInsertionData();
  public slots:
	void insertEmptyNote(int type);
	void insertWizard(int type);
	void insertColor(const QColor &color);
	void insertImage(const QPixmap &image);
	void pasteNote(QClipboard::Mode mode = QClipboard::Clipboard);
	void delayedCancelInsertPopupMenu();
	void cancelInsertPopupMenu();
  private slots:
	void hideInsertPopupMenu();
	void timeoutHideInsertPopupMenu();

/// TOOL TIPS:
  protected:
	void maybeTip(const QPoint &pos);

/// ANIMATIONS:
  private:
	QValueList<Note*> m_animatedNotes;
	QTimer            m_animationTimer;
	int               m_deltaY;
	QTime             m_lastFrameTime;
	static const int FRAME_DELAY;
  private slots:
	void animateObjects();
  public slots:
	void animateLoad();
  public:
	void addAnimatedNote(Note *note);

/// LOAD AND SAVE:
  private:
	bool m_loaded;
	bool m_loadingLaunched;
  private slots:
	void loadNotes(const QDomElement &notes, Note *parent);
	void saveNotes(QDomDocument &document, QDomElement &element, Note *parent);
  public slots:
	void load();
	void loadProperties(const QDomElement &properties);
	void saveProperties(QDomDocument &document, QDomElement &properties);
	void save();
  public:
	bool isLoaded()        { return m_loaded;          }
	bool loadingLaunched() { return m_loadingLaunched; }

/// BACKGROUND:
  private:
	QColor   m_backgroundColorSetting;
	QString  m_backgroundImageName;
	QPixmap *m_backgroundPixmap;
	QPixmap *m_opaqueBackgroundPixmap;
	QPixmap *m_selectedBackgroundPixmap;
	bool     m_backgroundTiled;
	QColor   m_textColorSetting;
  public:
	inline bool           hasBackgroundImage()     { return m_backgroundPixmap != 0;  }
	inline const QPixmap* backgroundPixmap()       { return m_backgroundPixmap;       }
	inline bool           isTiledBackground()      { return m_backgroundTiled;        }
	inline QString        backgroundImageName()    { return m_backgroundImageName;    }
	inline QColor         backgroundColorSetting() { return m_backgroundColorSetting; }
	inline QColor         textColorSetting()       { return m_textColorSetting;       }
	QColor backgroundColor();
	QColor textColor();
	void setAppearance(const QString &icon, const QString &name, const QString &backgroundImage, const QColor &backgroundColor, const QColor &textColor);
	void blendBackground(QPainter &painter, const QRect &rect, int xPainter = -1, int yPainter = -1, bool opaque = false, QPixmap *bg = 0);
	void unbufferizeAll();
	void subscribeBackgroundImages();
	void unsubscribeBackgroundImages();

/// KEYBOARD SHORTCUT:
  private:
	KAction *m_action;
	int      m_shortcutAction;
  private slots:
	void activatedShortcut();
  public:
	KShortcut shortcut() { return m_action->shortcut(); }
	int shortcutAction() { return m_shortcutAction;     }
	void setShortcut(KShortcut shortcut, int action);

/// USER INTERACTION:
  private:
	Note  *m_hoveredNote;
	int    m_hoveredZone;
	bool   m_lockedHovering;
	bool   m_underMouse;
	QRect  m_inserterRect;
	bool   m_inserterShown;
	bool   m_inserterSplit;
	bool   m_inserterTop;
	bool   m_inserterGroup;
	void placeInserter(Note *note, int zone);
	void removeInserter();
  public:
//	bool inserterShown() { return m_inserterShown; }
	bool inserterSplit() { return m_inserterSplit; }
	bool inserterGroup() { return m_inserterGroup; }
  public slots:
	void doHoverEffects(Note *note, Note::Zone zone, const QPoint &pos = QPoint(0, 0)); /// << @p pos is optionnal and only used to show the link target in the statusbar
	void doHoverEffects(const QPoint &pos);
	void doHoverEffects(); // The same, but using the current cursor position
  public:
	void popupTagsMenu(Note *note);
	void popupEmblemMenu(Note *note, int emblemNumber);
	void addTagToSelectedNotes(Tag *tag);
	void removeTagFromSelectedNotes(Tag *tag);
	void removeAllTagsFromSelectedNotes();
	void addStateToSelectedNotes(State *state);
	void changeStateOfSelectedNotes(State *state);
	bool selectedNotesHaveTags();
	const QRect& inserterRect()  { return m_inserterRect;  }
	bool         inserterShown() { return m_inserterShown; }
	void drawInserter(QPainter &painter, int xPainter, int yPainter);
	DecoratedBasket* decoration();
	State *stateForTagFromSelectedNotes(Tag *tag);
  public slots:
	void activatedTagShortcut(Tag *tag);
  private slots:
	void toggledTagInMenu(int id);
	void toggledStateInMenu(int id);
	void unlockHovering();
	void disableNextClick();
	void recomputeAllStyles();
	void contentsMoved();
  private:
	Note  *m_tagPopupNote;
	Tag   *m_tagPopup;
	QTime  m_lastDisableClick;

/// SELECTION:
  private:
	bool   m_isSelecting;
	bool   m_selectionStarted;
	bool   m_selectionInvert;
	QPoint m_selectionBeginPoint;
	QPoint m_selectionEndPoint;
	QRect  m_selectionRect;
	QTimer m_autoScrollSelectionTimer;
	void stopAutoScrollSelection();
  private slots:
	void doAutoScrollSelection();
  public:
	inline bool isSelecting() { return m_isSelecting; }
	inline const QRect& selectionRect() { return m_selectionRect; }
	void selectNotesIn(const QRect &rect, bool invertSelection, bool unselectOthers = true);
	void resetWasInLastSelectionRect();
	void selectAll();
	void unselectAll();
	void invertSelection();
	void unselectAllBut(Note *toSelect);
	void invertSelectionOf(Note *toSelect);
	QColor selectionRectInsideColor();
	Note* theSelectedNote();
	NoteSelection* selectedNotes();

/// BLANK SPACES DRAWING:
  private:
	QValueList<QRect> m_blankAreas;
	void recomputeBlankRects();
	QWidget *m_cornerWidget;

/// COMMUNICATION WITH ITS CONTAINER:
  signals:
	void postMessage(const QString &message);      /// << Post a temporar message in the statusBar.
	void setStatusBarText(const QString &message); /// << Set the permanent statusBar text or reset it if message isEmpty().
	void resetStatusBarText();                     /// << Equivalent to setStatusBarText("").
	void propertiesChanged(Basket *basket);
	void countsChanged(Basket *basket);
  public slots:
	void linkLookChanged();
	void signalCountsChanged();
  private:
	QTimer m_timerCountsChanged;
  private slots:
	void countsChangedTimeOut();

/// NOTES COUNTING:
  public:
	void addSelectedNote()    { ++m_countSelecteds;   signalCountsChanged(); }
	void removeSelectedNote() { --m_countSelecteds;   signalCountsChanged(); }
	void resetSelectedNote()  { m_countSelecteds = 0; signalCountsChanged(); } // FIXME: Useful ???
	int count()               { return m_count;          }
	int countFounds()         { return m_countFounds;    }
	int countSelecteds()      { return m_countSelecteds; }
  private:
	int m_count;
	int m_countFounds;
	int m_countSelecteds;

/// PROPERTIES:
  public:
	QString basketName() { return m_basketName; }
	QString icon()       { return m_icon;       }
	QString folderName() { return m_folderName; }
	QString fullPath();
	QString fullPathForFileName(const QString &fileName); // Full path of an [existing or not] note in this basket
	static QString fullPathForFolderName(const QString &folderName);
  private:
	QString m_basketName;
	QString m_icon;
	QString m_folderName;

/// ACTIONS ON SELECTED NOTES FROM THE INTERFACE:
  public slots:
	void noteEdit(Note *note = 0L, bool justAdded = false, const QPoint &clickedPoint = QPoint());
	void noteDelete();
	void noteDeleteWithoutConfirmation(bool deleteFilesToo = true);
	void noteCopy();
	void noteCut();
	void noteOpen(Note *note = 0L);
	void noteOpenWith(Note *note = 0L);
	void noteSaveAs();
	void noteGroup();
	void noteUngroup();
	void noteMoveOnTop();
	void noteMoveOnBottom();
	void noteMoveNoteUp();
	void noteMoveNoteDown();
  public:
	enum CopyMode { CopyToClipboard, CopyToSelection, CutToClipboard };
	void doCopy(CopyMode copyMode);

/// NOTES EDITION:
  private:
	NoteEditor *m_editor;
	//QWidget    *m_rightEditorBorder;
	TransparentWidget *m_leftEditorBorder;
	TransparentWidget *m_rightEditorBorder;
	bool        m_redirectEditActions;
	int         m_editorWidth;
	int         m_editorHeight;
	QTimer      m_inactivityAutoSaveTimer;
  public:
	bool isDuringEdit()        { return m_editor;              }
	bool redirectEditActions() { return m_redirectEditActions; }
	bool hasTextInEditor();
	bool hasSelectedTextInEditor();
	bool selectedAllTextInEditor();
	Note* editedNote();
  protected slots:
	void selectionChangedInEditor();
	void contentChangedInEditor();
	void inactivityAutoSaveTimout();
  public slots:
	void placeEditor(bool andEnsureVisible = false);
	void placeEditorAndEnsureVisible();
	void closeEditor();
	void closeEditorDelayed();
	void updateEditorAppearance();

/// FILTERING:
  public slots:
	void newFilter(const FilterData &data);
	void cancelFilter();
	void validateFilter();
	void filterAgain();
	void filterAgainDelayed();
	bool isFiltering();

/// DRAG AND DROP:
  private:
	bool m_isDuringDrag;
	QValueList<Note*> m_draggedNotes;
  public:
	static void acceptDropEvent(QDropEvent *event, bool preCond = true);
//	void dropEvent(QDropEvent *event);
	void contentsDropEvent(QDropEvent *event);
	bool isDuringDrag() { return m_isDuringDrag; }
	QValueList<Note*> draggedNotes() { return m_draggedNotes; }
  protected:
	void contentsDragEnterEvent(QDragEnterEvent*);
	void contentsDragMoveEvent(QDragMoveEvent *event);
	void contentsDragLeaveEvent(QDragLeaveEvent*);
  public slots:
	void slotCopyingDone(KIO::Job *, const KURL &, const KURL &to, bool, bool);
	void slotCopyingDone2(KIO::Job *job);
  public:
	Note* noteForFullPath(const QString &path);

/// EXPORTATION:
  public:
	static QString copyIcon(const QString &iconName, int size, const QString &destFolder);
	static QString copyFile(const QString &srcPath, const QString &destFolder, bool createIt = false);
	QValueList<State*> usedStates();
	static QString saveGradientBackground(const QColor &color, const QFont &font, const QString &folder);
  public slots:
	void exportToHTML();

/// MANAGE FOCUS:
  private:
	Note *m_focusedNote;
  public:
	void setFocusedNote(Note *note);
	void focusANote();
	void focusANonSelectedNoteAbove();
	void focusANonSelectedNoteBelow();
	Note* focusedNote() { return m_focusedNote; }
	Note* firstNoteInStack();
	Note* lastNoteInStack();
	Note* firstNoteShownInStack();
	Note* lastNoteShownInStack();
	void selectRange(Note *start, Note *end, bool unselectOthers = true); /// FIXME: Not really a focus related method!
	void ensureNoteVisible(Note *note);
	virtual void keyPressEvent(QKeyEvent *event);
	virtual void focusInEvent(QFocusEvent*);
	virtual void focusOutEvent(QFocusEvent*);
	QRect noteVisibleRect(Note *note); // clipped global (desktop as origin) rectangle
	Note* firstNoteInGroup();
	Note *noteOnHome();
	Note *noteOnEnd();

	enum NoteOn { LEFT_SIDE = 1, RIGHT_SIDE, TOP_SIDE, BOTTOM_SIDE };
	Note* noteOn(NoteOn side);

/// REIMPLEMENTED:
  public:
	void deleteFiles();


  public:
	void wheelEvent(QWheelEvent *event);



  public:
	Note *m_startOfShiftSelectionNote;




/// FROM OLD ARCHITECTURE **********************

public slots:

	void showFrameInsertTo() {}
	void resetInsertTo() {}

	void  computeInsertPlace(const QPoint &/*cursorPosition*/)    { }
	void  processActionAsYouType(QKeyEvent */*event*/)    { };
	public slots:
		/** Notes manipulation */
		void changeNotePlace(Note */*note*/)    { };
		/** Selection of note(s) */
		void clicked(Note */*note*/, bool /*controlPressed*/, bool /*shiftPressed*/)    { };

	public:
		void    dontCareOfCreation(const QString &/*path*/)    { };
	protected slots:
		void slotModifiedFile(const QString &/*path*/)    { };
		void slotCreatedFile(const QString &/*path*/)    { };
		void slotDeletedFile(const QString &/*path*/)    { };
		void slotUpdateNotes()    { };
		void clipboardChanged(bool /*selectionMode*/ = false)    { };
		void selectionChanged()    { };
	public:
		bool importLauncher(const QString &/*type*/, const QDomElement &/*content*/, const QString &/*runCommand*/,
							const QString &/*annotations*//*, bool checked*/)    { return false; }

	inline bool    isLocked()               { return 0;}//m_isLocked;               }
	inline bool    showFilterBar()          { return 0;}//m_showFilterBar;          } // Others... ::
	/** Basket properties SET */
	void setLocked(bool /*lock*/) {};
	void setShowFilterBar(bool /*show*/)         { }

	friend class ContainerSystemTray;
};









#if 0

#include <qwidget.h>
#include <qscrollview.h>
#include <qclipboard.h>
#include <qptrlist.h>
#include <qtimer.h>
#include <kio/job.h>
#include <qcolor.h>

#include "filter.h"

class QFrame;
class QVBoxLayout;
class QCheckBox;
class QString;
class QColor;
class QPixmap;
class QAction;
class QStringList;
class QRect;

class QDomElement;

class KDirWatch;

class Basket;
class Note;
class NoteEditorBase;

/** Used to enqueue a file path when the Basket receive a file modification / creation / deletion
  * It associate the file name with an event.
  * All this queue will be treated later.
  * TODO: rename to class WatcherEvent ?
  * @author S�bastien Lao�t
  */
class FileEvent
{
  public:
	enum Event { Modified = 1, Created, Deleted, Renamed };
	FileEvent(Event evt, const QString &path)
	 : event(evt), filePath(path)
	{ }
  public: // Because it must be fast and theire is no need to be private
	Event   event;
	QString filePath;
};

/** Basket that contain some Notes.
  * @author S�bastien Lao�t
  */
clas   s Bas    ket : public QScrollView
{
  Q_OBJECT
  public:
	/** Construtor and destructor */
	Bask      et(QWidget *parent, const QString &folderName, const char *name = "", WFlags fl = 0);
  public:
  protected:
	virtual void contentsContextMenuEvent(QContextMenuEvent *event);
	virtual void contentsMousePressEvent(QMouseEvent *event); // For redirected event !!
	virtual void showEvent(QShowEvent *);
	/** Drag and drop functions */
	virtual void dragEnterEvent(QDragEnterEvent*);
	virtual void dragMoveEvent(QDragMoveEvent* event);
	virtual void dragLeaveEvent(QDragLeaveEvent*);
  public:
	virtual void dropEvent(QDropEvent *event);
	static  void acceptDropEvent(QDropEvent *event, bool preCond = true);
	bool  canDragNote()       { return !isEmpty(); }
	void  computeInsertPlace(const QPoint &cursorPosition);
	Note* noteAtPosition(const QPoint &pos);
	Note* duplicatedOf(Note *note);
	void  checkClipboard();
	void  processActionAsYouType(QKeyEvent *event);
	void  exportToHTML();
  signals:
	void nameChanged(Basket *basket, const QString &name);
	void iconChanged(Basket *basket, const QString &icon);
	void notesNumberChanged(Basket *basket);
  public slots:
	void linkLookChanged();
	void showNotesToolTipChanged();
	/** Notes manipulation */
	void insertNote(Note *note);
	void delNote(Note *note, bool askForMirroredFile = true);
	void changeNotePlace(Note *note);
	void pasteNote(QClipboard::Mode mode = QClipboard::Clipboard);
	void recolorizeNotes();
	void reloadMirroredFolder();
	void showMirrorOnlyOnceInfo();
	/** Selection of note(s) */
	void selectAll();
	void unselectAll();
	void unselectAllBut(Note *toSelect);
	void invertSelection();
	void selectRange(Note *start, Note *end);
	void clicked(Note *note, bool controlPressed, bool shiftPressed);
	void setFocusedNote(Note *note);
	void focusANote();
	void ensureVisibleNote(Note *note);
	QRect noteRect(Note *note); // clipped global (desktop as origin) rectangle
	/** Travel the list to find the next shown note, or the previous if step == -1, or the next after 10 if step == 10... */
	Note* nextShownNoteFrom(Note *note, int step);
	/** Actions on (selected) notes */
	void editNote(Note *note, bool editAnnotations = false);
	void editNote();
	void delNote();
	void copyNote();
	void cutNote();
	void openNote();
	void openNoteWith();
	void saveNoteAs();
	void moveOnTop();
	void moveOnBottom();
	void moveNoteUp();
	void moveNoteDown();
  public:
	void    dontCareOfCreation(const QString &path);
	QString copyIcon(const QString &iconName, int size, const QString &destFolder);
	QString copyFile(const QString &srcPath, const QString &destFolder, bool createIt = false);
  protected slots:
	void slotModifiedFile(const QString &path);
	void slotCreatedFile(const QString &path);
	void slotDeletedFile(const QString &path);
	void slotUpdateNotes();
	void placeEditor();
	void closeEditor(bool save = true);
	void clipboardChanged(bool selectionMode = false);
	void selectionChanged();
  private:
	QTimer              m_updateTimer;
	QPtrList<FileEvent> m_updateQueue;
	QStringList         m_dontCare;
	static const int    c_updateTime;
  private:
	void load(); // Load is performed only once, during contructor
	void loadNotes(const QDomElement &notes);
	bool importLauncher(const QString &type, const QDomElement &content, const QString &runCommand,
	                    const QString &annotations/*, bool checked*/);
	void computeShownNotes();

  private:
	KDirWatch      *m_watcher;
	NoteEditorBase *m_editor;
	QKeyEvent      *m_stackedKeyEvent;
};

#endif // #if 0

#endif // BASKET_H