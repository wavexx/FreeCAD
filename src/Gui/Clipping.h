/***************************************************************************
 *   Copyright (c) 2013 Werner Mayer <wmayer[at]users.sourceforge.net>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#ifndef GUI_DIALOG_CLIPPING_H
#define GUI_DIALOG_CLIPPING_H

#include <QDialog>
#include <Inventor/fields/SoSFColor.h>
#include <Inventor/fields/SoSFPlane.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Base/Placement.h>
#include "InventorBase.h"
#include "SoFCCSysDragger.h"

class QDockWidget;

class SoDrawStyle;
class SoClipPlane;
class SoBaseColor;
class SoCoordinate3;

class ClipDragger : public Gui::SoFCCSysDragger {
    typedef SoFCCSysDragger inherited;

    SO_NODE_HEADER(ClipDragger);

public:
    SoSFFloat planeSize;
    SoSFFloat lineWidth;
    SoSFColor planeColor;
    SoSFPlane plane;
    std::function<void(ClipDragger*)> dragDone;

    static void initClass(void);

    ClipDragger(SoClipPlane *clip = nullptr, bool custom = false);
    ~ClipDragger();

    virtual void notify(SoNotList * l) override;

    void init(SoClipPlane *clip, bool custom = false);

    SoClipPlane *getClipPlane() const {return clip;}

protected:
    void onDragStart();
    void onDragFinish();
    void onDragMotion();
    void updateSize();

private:
    Gui::CoinPtr<SoCoordinate3> coords;
    Gui::CoinPtr<SoClipPlane> clip;
    Gui::CoinPtr<SoBaseColor> color;
    Gui::CoinPtr<SoDrawStyle> drawStyle;
    bool busy = false;
};

namespace Gui {
class View3DInventor;
namespace Dialog {

/**
 * @author Werner Mayer
 */
class GuiExport Clipping : public QDialog
{
    Q_OBJECT

public:
    Clipping(Gui::View3DInventor* view, QWidget* parent = nullptr);
    ~Clipping();

    void setupClipPlanes(const Base::Placement &plaX, bool enableX,
                         const Base::Placement &plaY, bool enableY,
                         const Base::Placement &plaZ, bool enableZ,
                         const Base::Placement &plaCustom, bool enableCustom);

    void getClipPlanes(Base::Placement &plaX, bool &enableX,
                       Base::Placement &plaY, bool &enableY,
                       Base::Placement &plaZ, bool &enableZ,
                       Base::Placement &plaCustom, bool &enableCustom);

    static void toggle();

protected Q_SLOTS:
    void onViewDestroyed(QObject *);
    void on_groupBoxX_toggled(bool);
    void on_groupBoxY_toggled(bool);
    void on_groupBoxZ_toggled(bool);
    void on_clipX_valueChanged(double);
    void on_clipY_valueChanged(double);
    void on_clipZ_valueChanged(double);
    void on_flipClipX_clicked();
    void on_flipClipY_clicked();
    void on_flipClipZ_clicked();
    void on_groupBoxView_toggled(bool);
    void on_clipView_valueChanged(double);
    void on_fromView_clicked();
    void on_adjustViewdirection_toggled(bool);
    void on_dirX_valueChanged(double);
    void on_dirY_valueChanged(double);
    void on_dirZ_valueChanged(double);
    void on_angleX_valueChanged(double);
    void on_angleY_valueChanged(double);
    void on_checkBoxFill_toggled(bool);
    void on_checkBoxInvert_toggled(bool);
    void on_checkBoxConcave_toggled(bool);
    void on_checkBoxOnTop_toggled(bool);
    void on_checkBoxShowPlane_toggled(bool);
    void on_spinBoxHatchScale_valueChanged(double);
    void on_checkBoxHatch_toggled(bool on);
    void on_editHatchTexture_fileNameSelected(const QString &filename);

public:
    void done(int);

private:
    class Private;
    Private* d;
};

} // namespace Dialog
} // namespace Gui

#endif // GUI_DIALOG_CLIPPING_H
