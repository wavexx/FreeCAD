/****************************************************************************
 *   Copyright (c) 2020 Zheng, Lei (realthunder) <realthunder.dev@gmail.com>*
 *                                                                          *
 *   This file is part of the FreeCAD CAx development system.               *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Library General Public            *
 *   License as published by the Free Software Foundation; either           *
 *   version 2 of the License, or (at your option) any later version.       *
 *                                                                          *
 *   This library  is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU Library General Public License for more details.                   *
 *                                                                          *
 *   You should have received a copy of the GNU Library General Public      *
 *   License along with this library; see the file COPYING.LIB. If not,     *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,          *
 *   Suite 330, Boston, MA  02111-1307, USA                                 *
 *                                                                          *
 ****************************************************************************/

#ifndef FC_OVERLAYWIDGETS_H
#define FC_OVERLAYWIDGETS_H 

#include <QTabWidget>
#include <QTimer>
#include <QTime>
#include <QAction>
#include <QToolButton>
#include <QGraphicsEffect>
#include <QImage>

#include <Base/Parameter.h>

class QDockWidget;
class QSplitter;
class QPropertyAnimation;

namespace Gui {

class GuiExport OverlayManager : public QObject {
    Q_OBJECT
public:
    OverlayManager();
    virtual ~OverlayManager();

    void restore();
    void save();
    void retranslate();

    void refresh(QWidget *widget=nullptr, bool refreshStyle=false);

    void setupTitleBar(QDockWidget *);

    enum OverlayMode {
        ToggleActive,
        ToggleAutoHide,
        ToggleTransparent,
        EnableActive,
        DisableActive,
        EnableAll,
        DisableAll,
        ToggleAll,
        AutoHideAll,
        AutoHideNone,
        ToggleAutoHideAll,
        TransparentAll,
        TransparentNone,
        ToggleTransparentAll,
    };
    void setOverlayMode(OverlayMode mode);

    void changeOverlaySize(int changes);

    void setMouseTransparent(bool enabled);
    bool isMouseTransparent() const;

    bool isUnderOverlay() const;

    void initDockWidget(QDockWidget *, QWidget *);
    void setupDockWidget(QDockWidget *);
    void unsetupDockWidget(QDockWidget *);

    static OverlayManager * instance();
    static void destruct();

    class Private;

protected:
    bool eventFilter(QObject *, QEvent *ev);

private Q_SLOTS:
    void onToggleDockWidget(bool checked);
    void onDockWidgetTitleChange(const QString &);
    void onTaskViewUpdate();
    void onTimer();
    void onFocusChanged(QWidget *, QWidget *);
    void onAction();

private:
    friend class Private;
    Private * d;
};

#if QT_VERSION  >= 0x050000
#   define FC_HAS_DOCK_OVERLAY
#endif

#ifdef FC_HAS_DOCK_OVERLAY

class OverlayProxyWidget;
class OverlayGraphicsEffect;

class OverlayTabWidget: public QTabWidget
{
    Q_OBJECT

    Q_PROPERTY(QColor effectColor READ effectColor WRITE setEffectColor DESIGNABLE true SCRIPTABLE true)
    Q_PROPERTY(int effectWidth READ effectWidth WRITE setEffectWidth DESIGNABLE true SCRIPTABLE true)
    Q_PROPERTY(int effectHeight READ effectHeight WRITE setEffectHeight DESIGNABLE true SCRIPTABLE true)
    Q_PROPERTY(qreal effectOffsetX READ effectOffsetX WRITE setEffectOffsetX DESIGNABLE true SCRIPTABLE true)
    Q_PROPERTY(qreal effectOffsetY READ effectOffsetY WRITE setEffectOffsetY DESIGNABLE true SCRIPTABLE true)
    Q_PROPERTY(qreal effectBlurRadius READ effectBlurRadius WRITE setEffectBlurRadius DESIGNABLE true SCRIPTABLE true)
    Q_PROPERTY(bool enableEffect READ effectEnabled WRITE setEffectEnabled DESIGNABLE true SCRIPTABLE true)
    Q_PROPERTY(qreal animation READ animation WRITE setAnimation DESIGNABLE true SCRIPTABLE true)

public:
    OverlayTabWidget(QWidget *parent, Qt::DockWidgetArea pos);

    void setOverlayMode(QWidget *widget, int enable);
    void setOverlayMode(bool enable);
    void addWidget(QDockWidget *widget, const QString &title);
    void removeWidget(QDockWidget *widget, QDockWidget *last=nullptr);
    void setCurrent(QDockWidget *widget);

    void setAutoHide(bool enable);
    bool isAutoHide() const {return actAutoHide.isChecked();}

    void setTransparent(bool enable);
    bool isTransparent() const {return actTransparent.isChecked();}

    void setEditHide(bool enable);
    bool isEditHide() const {return actEditHide.isChecked();}

    void setEditShow(bool enable);
    bool isEditShow() const {return actEditShow.isChecked();}

    void touch() {touched = true;}
    bool isTouched() const {return touched;}

    void setRect(QRect rect);

    const QRect &getRect();
    bool isOverlayed() const {return overlayed;}
    bool checkAutoHide() const;
    bool getAutoHideRect(QRect &rect) const;
    void changeSize(int changes, bool checkModifier=true);
    void onAction(QAction *);

    void setOffset(const QSize &ofs);
    const QSize &getOffset() const {return offset;}

    void setSizeDelta(int delta);
    int getSizeDelta() const {return sizeDelta;}

    OverlayProxyWidget *getProxyWidget() {return proxyWidget;}

    QDockWidget *currentDockWidget() const;
    QDockWidget *dockWidget(int index) const;
    int dockWidgetIndex(QDockWidget *) const;

    void setTitleBar(QWidget *);

    QSplitter *getSplitter() const {return splitter;}
    QWidget *getTitleBar() const {return titleBar;}

    Qt::DockWidgetArea getDockArea() const {return dockArea;}

    const QTime &getRevealTime() const {return revealTime;}
    void setRevealTime(const QTime &time);

    void restore(ParameterGrp::handle handle);
    void saveTabs();

    QColor effectColor() const;
    void setEffectColor(const QColor&);
    int effectWidth() const;
    void setEffectWidth(int);
    int effectHeight() const;
    void setEffectHeight(int);
    qreal effectOffsetX() const;
    void setEffectOffsetX(qreal);
    qreal effectOffsetY() const;
    void setEffectOffsetY(qreal);
    qreal effectBlurRadius() const;
    void setEffectBlurRadius(qreal);
    bool effectEnabled() const;
    void setEffectEnabled(bool);
    qreal animation() const {return _animation;}
    void setAnimation(qreal);

    void scheduleRepaint();

    int testAlpha(const QPoint &);

    void startShow();
    void startHide();

    enum State {
        State_Normal,
        State_Hint,
        State_HintHidden,
    };
    void setState(State);
    State getState() const {return _state;}

protected:
    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);
    void changeEvent(QEvent*);
    void resizeEvent(QResizeEvent*);
    void paintEvent(QPaintEvent *);
    bool event(QEvent *ev);
    bool eventFilter(QObject *, QEvent *ev);

    static void _setOverlayMode(QWidget *widget, int enable);
    void retranslate();

protected Q_SLOTS:
    void onCurrentChanged(int index);
    void onTabMoved(int from, int to);
    void onSplitterMoved();
    void onRepaint();
    void onAnimationStateChanged();
    void setupLayout();
    void onSizeGripMove(const QPoint &);

private:
    friend class OverlayProxyWidget;
    friend class OverlayManager::Private;

    QSize offset;
    int sizeDelta = 0;
    QRect rectOverlay;
    OverlayProxyWidget *proxyWidget;
    QSplitter *splitter = nullptr;
    QWidget *titleBar = nullptr;
    QAction actAutoHide;
    QAction actEditHide;
    QAction actEditShow;
    QAction actTransparent;
    QAction actIncrease;
    QAction actDecrease;
    QAction actOverlay;
    QTimer timer;
    QTimer repaintTimer;
    bool repainting = false;
    bool overlayed = false;
    bool touched = false;
    bool busy = false;
    Qt::DockWidgetArea dockArea;
    int tabSize = 0;
    QTime revealTime;
    ParameterGrp::handle hGrp;

    OverlayGraphicsEffect *_graphicsEffect = nullptr;
    OverlayGraphicsEffect *_graphicsEffectTab = nullptr;
    bool _effectEnabled = false;

    QImage _image;
    qreal _imageScale;

    qreal _animation = 0;
    QPropertyAnimation *_animator = nullptr;

    State _state = State_Normal;
};

#if 0
class OverlayDragFrame: public QWidget
{
    Q_OBJECT
protected:
    void paintEvent(QPaintEvent*);
};

class OverlayTitleBar: public QWidget
{
    Q_OBJECT
protected:
    void paintEvent(QPaintEvent*);
    void mouseMoveEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
};
#endif

class OverlaySizeGrip: public QWidget
{
    Q_OBJECT
public:
    OverlaySizeGrip(QWidget *parent, bool vertical);

Q_SIGNALS:
    void dragMove(const QPoint &globalPos);

protected:
    void paintEvent(QPaintEvent*);
    void mouseMoveEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
};

class OverlayToolButton: public QToolButton
{
    Q_OBJECT
public:
    OverlayToolButton(QWidget *parent);
};

class OverlayProxyWidget: public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QBrush hintColor READ hintColor WRITE setHintColor DESIGNABLE true SCRIPTABLE true)

public:
    OverlayProxyWidget(OverlayTabWidget *);

    OverlayTabWidget *getOwner() const {return owner;}
    bool hitTest(QPoint, bool delay=true);
    bool isActivated() const;

    QBrush hintColor() const;
    void setHintColor(const QBrush &);

protected:
    void enterEvent(QEvent*);
    void leaveEvent(QEvent*);
    void hideEvent(QHideEvent*);
    void paintEvent(QPaintEvent*);
    void mousePressEvent(QMouseEvent *);

protected Q_SLOTS:
    void onTimer();

private:
    OverlayTabWidget* owner;
    int drawLine = false;
    int dockArea;
    QTimer timer;
    QBrush _hintColor;
};

// modified from https://stackoverflow.com/a/23752747
class OverlayGraphicsEffect: public QGraphicsEffect
{
    Q_OBJECT
public:
    OverlayGraphicsEffect(QObject *parent);

    virtual void draw(QPainter* painter);
    virtual QRectF boundingRectFor(const QRectF& rect) const;

    inline void setSize(const QSize &size)
        { if(_size!=size){_size = size; updateBoundingRect(); } }

    inline QSize size() const { return _size; }

    inline void setOffset(const QPointF &offset)
        { if(_offset!=offset) {_offset = offset; updateBoundingRect(); } }

    inline QPointF offset() const { return _offset; }

    inline void setBlurRadius(qreal blurRadius)
        { if(_blurRadius!=blurRadius) {_blurRadius = blurRadius; updateBoundingRect();} }

    inline qreal blurRadius() const { return _blurRadius; }

    inline void setColor(const QColor& color) { _color = color; }
    inline QColor color() const { return _color; }

    inline bool enabled() const {return _enabled;}
    inline void setEnabled(bool enabled) 
        { if(_enabled!=enabled) {_enabled = enabled; updateBoundingRect();} }

private:
    bool _enabled;
    QSize  _size;
    qreal  _blurRadius;
    QColor _color;
    QPointF _offset;
};

#endif // FC_HAS_DOCK_OVERLAY

} // namespace Gui

#endif // FC_OVERLAYWIDGETS_H
