/***************************************************************************
 *   Copyright (c) 2015 Stefan Tröger <stefantroeger@gmx.net>              *
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


#include "PreCompiled.h"

#ifndef _PreComp_
# include <sstream>
# include <QAction>
# include <QRegExp>
# include <QTextStream>
# include <QMessageBox>
# include <QGenericReturnArgument>
# include <QMetaObject>
# include <QKeyEvent>
# include <Precision.hxx>
#endif

#include <boost/algorithm/string/predicate.hpp>

#include "ui_TaskPipeParameters.h"
#include "ui_TaskPipeOrientation.h"
#include "ui_TaskPipeScaling.h"
#include <ui_DlgReference.h>
#include "TaskPipeParameters.h"
#include <Base/Tools.h>
#include <App/Application.h>
#include <App/Document.h>
#include <App/Origin.h>
#include <App/Part.h>
#include <Gui/Application.h>
#include <Gui/Document.h>
#include <Gui/BitmapFactory.h>
#include <Gui/ViewProvider.h>
#include <Gui/WaitCursor.h>
#include <Gui/MetaTypes.h>
#include <Base/Console.h>
#include <Base/Tools.h>
#include <Gui/Selection.h>
#include <Gui/CommandT.h>
#include <Gui/MainWindow.h>
#include <Mod/PartDesign/App/FeaturePipe.h>
#include <Mod/Sketcher/App/SketchObject.h>
#include <Mod/PartDesign/App/Body.h>
#include "TaskSketchBasedParameters.h"
#include "ReferenceSelection.h"
#include "Utils.h"
#include "TaskFeaturePick.h"

FC_LOG_LEVEL_INIT("PartDesign", true, true)

using namespace PartDesignGui;
using namespace Gui;

static bool _Busy;

class SpineSelectionGate : public Gui::SelectionGate
{
public:
    SpineSelectionGate(TaskPipeParameters *master, const Gui::ViewProviderDocumentObject *vp)
        :parameter(master)
    {
        init(vp);
    }

    SpineSelectionGate(TaskPipeOrientation *master, const Gui::ViewProviderDocumentObject *vp)
        :orientation(master)
    {
        init(vp);
    }

    ~SpineSelectionGate()
    {
        if (parameter)
            parameter->exitSelectionMode();
        else
            orientation->exitSelectionMode();
    }

    void init(const Gui::ViewProviderDocumentObject *vp)
    {
        if (!vp)
            return;
        objT = App::DocumentObjectT(vp->getObject());
        auto prop = getProperty();
        if (prop && !prop->getValue()) {
            inList = vp->getObject()->getInListEx(true);
            inList.insert(vp->getObject());
        }
    }

    App::PropertyLinkSub *getProperty()
    {
        auto pipe = static_cast<PartDesign::Pipe*>(objT.getObject());
        if (!pipe)
            return nullptr;
        return parameter ? &pipe->Spine : &pipe->AuxillerySpine;
    }

    bool allow(App::Document*, App::DocumentObject* pObj, const char* sSubName)
    {
        auto prop = getProperty();
        if (!prop) {
            this->notAllowedReason = QT_TR_NOOP("Pipe feature not found.");
            return false;
        }
        if (inList.count(pObj)) {
            this->notAllowedReason = QT_TR_NOOP("Selecting this will cause circular dependency.");
            return false;
        }
        if (!boost::starts_with(sSubName, "Edge")
                && !boost::starts_with(sSubName, "Wire")
                && !boost::starts_with(sSubName, "Face"))
        {
            if (!Part::Feature::getTopoShape(pObj, sSubName, true).hasSubShape(TopAbs_EDGE)) {
                this->notAllowedReason = QT_TR_NOOP("No edge found in selection.");
                return false;
            }
        }
        return true;
    }

private:
    App::DocumentObjectT objT;
    std::set<App::DocumentObject*> inList;
    TaskPipeParameters *parameter = nullptr;
    TaskPipeOrientation *orientation = nullptr;
};

class PipeSectionSelectionGate : public Gui::SelectionGate
{
public:
    PipeSectionSelectionGate(TaskPipeScaling *master,
                         const Gui::ViewProviderDocumentObject *vp)
        :master(master)
    {
        if (vp) {
            objT = App::DocumentObjectT(vp->getObject());
            inList = vp->getObject()->getInListEx(true);
            inList.insert(vp->getObject());
        }
    }

    ~PipeSectionSelectionGate()
    {
        master->exitSelectionMode();
    }

    bool allow(App::Document*, App::DocumentObject* pObj, const char*)
    {
        auto pipe = static_cast<PartDesign::Pipe*>(objT.getObject());
        if (!pipe) {
            this->notAllowedReason = QT_TR_NOOP("Pipe feature not found.");
            return false;
        }
        if (inList.count(pObj)) {
            this->notAllowedReason = QT_TR_NOOP("Selecting this will cause circular dependency.");
            return false;
        }
        if (pipe->Profile.getValue() == pObj) {
            this->notAllowedReason = QT_TR_NOOP("Profile object cannot be used as section.");
            return false;
        }
        if (pipe->Sections.find(pObj->getNameInDocument())) {
            this->notAllowedReason = QT_TR_NOOP("Section object already selected.");
            return false;
        }
        if (pipe->Profile.getValue()) {
            if (!wireCount) {
                wireCount = Part::Feature::getTopoShape(
                        pipe->Profile.getValue()).countSubShapes(TopAbs_WIRE);
            }
            if (wireCount>0 && Part::Feature::getTopoShape(
                                    pObj).countSubShapes(TopAbs_WIRE) != wireCount)
            {
                this->notAllowedReason = QT_TR_NOOP("Section object must have the same number of wires.");
                return false;
            }
        } else
            wireCount = 0;
        return true;
    }

private:
    App::DocumentObjectT objT;
    std::set<App::DocumentObject*> inList;
    TaskPipeScaling *master;
    unsigned wireCount = 0;
};

/* TRANSLATOR PartDesignGui::TaskPipeParameters */


//**************************************************************************
//**************************************************************************
// Task Parameter
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

TaskPipeParameters::TaskPipeParameters(ViewProviderPipe *PipeView, bool /*newObj*/, QWidget *parent)
    : TaskSketchBasedParameters(PipeView, parent, "PartDesign_AdditivePipe", tr("Pipe parameters"))
    , ui(new Ui_TaskPipeParameters)
{
    // we need a separate container widget to add all controls to
    proxy = new QWidget(this);
    ui->setupUi(proxy);
    QMetaObject::connectSlotsByName(this);

    QBoxLayout * boxLayout = qobject_cast<QBoxLayout*>(proxy->layout());
    if (boxLayout) {
        PartDesign::Pipe* pcPipe = static_cast<PartDesign::Pipe*>(PipeView->getObject());
        spineWidget = new LinkSubWidget(this, tr("Sweep Path"), pcPipe->Spine);
        spineWidget->setSelectionMode(SelectionMode::refSpine);
        boxLayout->addWidget(spineWidget);
    }

    connect(ui->comboBoxTransition, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onTransitionChanged(int)));

    this->initUI(proxy);
    this->groupLayout()->addWidget(proxy);

    refresh();

    QMetaObject::invokeMethod(this, "updateUI", Qt::QueuedConnection);
}

TaskPipeParameters::~TaskPipeParameters()
{
    exitSelectionMode();
}

void TaskPipeParameters::updateUI()
{
}

void TaskPipeParameters::refresh()
{
    if (!vp || !vp->getObject())
        return;

    QSignalBlocker guard(ui->comboBoxTransition);
    PartDesign::Pipe* pipe = static_cast<PartDesign::Pipe*>(vp->getObject());
    ui->comboBoxTransition->setCurrentIndex(pipe->Transition.getValue());

    spineWidget->refresh();
}

void TaskPipeParameters::onTransitionChanged(int idx) {

    try {
        setupTransaction();
        static_cast<PartDesign::Pipe*>(vp->getObject())->Transition.setValue(idx);
        recomputeFeature();
    } catch (Base::Exception &e) {
        e.ReportException();
    }
}

void TaskPipeParameters::onTangentChanged(bool checked) {
    try {
        setupTransaction();
        static_cast<PartDesign::Pipe*>(vp->getObject())->SpineTangent.setValue(checked);
        recomputeFeature();
    } catch (Base::Exception &e) {
        e.ReportException();
    }
}

//**************************************************************************
//**************************************************************************
// Task Orientation
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

TaskPipeOrientation::TaskPipeOrientation(ViewProviderPipe* PipeView, bool /*newObj*/, QWidget* parent)
    : TaskSketchBasedParameters(PipeView, parent, "PartDesign_AdditivePipe", tr("Section orientation")),
    ui(new Ui_TaskPipeOrientation)
{
    // we need a separate container widget to add all controls to
    proxy = new QWidget(this);
    ui->setupUi(proxy);
    QMetaObject::connectSlotsByName(this);

    QBoxLayout * boxLayout = qobject_cast<QBoxLayout*>(ui->auxiliary->layout());
    if (boxLayout) {
        PartDesign::Pipe* pcPipe = static_cast<PartDesign::Pipe*>(PipeView->getObject());
        auxSpineWidget = new LinkSubWidget(this, tr("Aux Path"), pcPipe->AuxillerySpine);
        auxSpineWidget->setSelectionMode(SelectionMode::refAuxSpine);
        boxLayout->addWidget(auxSpineWidget);
    }

    connect(ui->comboBoxMode, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onOrientationChanged(int)));
    connect(ui->stackedWidget, SIGNAL(currentChanged(int)),
            this, SLOT(updateUI(int)));
    connect(ui->curvelinear, SIGNAL(toggled(bool)),
            this, SLOT(onCurvelinearChanged(bool)));
    connect(ui->doubleSpinBoxX, SIGNAL(valueChanged(double)),
            this, SLOT(onBinormalChanged(double)));
    connect(ui->doubleSpinBoxY, SIGNAL(valueChanged(double)),
            this, SLOT(onBinormalChanged(double)));
    connect(ui->doubleSpinBoxZ, SIGNAL(valueChanged(double)),
            this, SLOT(onBinormalChanged(double)));

    this->groupLayout()->addWidget(proxy);
    PartDesign::Pipe* pipe = static_cast<PartDesign::Pipe*>(vp->getObject());
    // should be called after panel has become visible
    QMetaObject::invokeMethod(this, "updateUI", Qt::QueuedConnection,
        QGenericReturnArgument(), Q_ARG(int,pipe->Mode.getValue()));

    refresh();
}

TaskPipeOrientation::~TaskPipeOrientation()
{
    exitSelectionMode();
}

void TaskPipeOrientation::refresh()
{
    if (!vp || !vp->getObject())
        return;

    // Temporarily prevent unnecessary feature recomputes
    for (QWidget* child : proxy->findChildren<QWidget*>())
        child->blockSignals(true);

    PartDesign::Pipe* pipe = static_cast<PartDesign::Pipe*>(vp->getObject());

    ui->curvelinear->setChecked(pipe->AuxilleryCurvelinear.getValue());

    int idx = pipe->Mode.getValue();
    ui->comboBoxMode->setCurrentIndex(idx);
    ui->stackedWidget->setCurrentIndex(idx);
    updateUI(idx);

    if (auxSpineWidget)
        auxSpineWidget->refresh();

    for (QWidget* child : proxy->findChildren<QWidget*>())
        child->blockSignals(false);

    TaskSketchBasedParameters::refresh();
}

void TaskPipeOrientation::onOrientationChanged(int idx) {
    if (!vp)
        return;
    ui->stackedWidget->setCurrentIndex(idx);
    try {
        setupTransaction();
        auto pipe = static_cast<PartDesign::Pipe*>(vp->getObject());
        pipe->Mode.setValue(idx);
        if (idx != 3 || pipe->AuxillerySpine.getValue())
            recomputeFeature();
    } catch (Base::Exception &e) {
        e.ReportException();
    }
    if (idx == 3) {
        if (getSelectionMode() != SelectionMode::refAuxSpine)
            auxSpineWidget->onButton();
    } else
        exitSelectionMode();
}

void TaskPipeOrientation::onCurvelinearChanged(bool checked)
{
    try {
        setupTransaction();
        static_cast<PartDesign::Pipe*>(vp->getObject())->AuxilleryCurvelinear.setValue(checked);
        recomputeFeature();
    } catch (Base::Exception &e) {
        e.ReportException();
    }
}

void TaskPipeOrientation::onBinormalChanged(double)
{
    Base::Vector3d vec(ui->doubleSpinBoxX->value(),
                       ui->doubleSpinBoxY->value(),
                       ui->doubleSpinBoxZ->value());

    try {
        setupTransaction();
        static_cast<PartDesign::Pipe*>(vp->getObject())->Binormal.setValue(vec);
        recomputeFeature();
    } catch (Base::Exception &e) {
        e.ReportException();
    }
}

void TaskPipeOrientation::updateUI(int idx) {
    //make sure we resize to the size of the current page
    for (int i=0; i<ui->stackedWidget->count(); ++i)
        ui->stackedWidget->widget(i)->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    if (idx < ui->stackedWidget->count())
        ui->stackedWidget->widget(idx)->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}


//**************************************************************************
//**************************************************************************
// Task Scaling
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
TaskPipeScaling::TaskPipeScaling(ViewProviderPipe* PipeView, bool /*newObj*/, QWidget* parent)
    : TaskSketchBasedParameters(PipeView, parent, "PartDesign_AdditivePipe", tr("Section transformation")),
    ui(new Ui_TaskPipeScaling)
{
    // we need a separate container widget to add all controls to
    proxy = new QWidget(this);
    ui->setupUi(proxy);
    QMetaObject::connectSlotsByName(this);

    ui->listWidgetReferences->installEventFilter(this);
    ui->listWidgetReferences->setMouseTracking(true);

    connect(ui->comboBoxScaling, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onScalingChanged(int)));
    connect(ui->buttonRefAdd, SIGNAL(clicked(bool)),
            this, SLOT(onButtonRefAdd(bool)));
    connect(ui->stackedWidget, SIGNAL(currentChanged(int)),
            this, SLOT(updateUI(int)));
    connect(ui->listWidgetReferences, SIGNAL(itemEntered(QListWidgetItem*)),
            this, SLOT(onItemEntered(QListWidgetItem*)));
    connect(ui->listWidgetReferences, SIGNAL(itemSelectionChanged()),
            this, SLOT(onItemSelectionChanged()));

    // Create context menu
    QAction* remove = new QAction(tr("Remove"), this);
    remove->setShortcut(QKeySequence::Delete);
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    // display shortcut behind the context menu entry
    remove->setShortcutVisibleInContextMenu(true);
#endif
    ui->listWidgetReferences->addAction(remove);
    ui->listWidgetReferences->setContextMenuPolicy(Qt::ActionsContextMenu);
    connect(remove, SIGNAL(triggered()), this, SLOT(onDeleteSection()));

    this->groupLayout()->addWidget(proxy);

    PartDesign::Pipe* pipe = static_cast<PartDesign::Pipe*>(PipeView->getObject());

    // should be called after panel has become visible
    QMetaObject::invokeMethod(this, "updateUI", Qt::QueuedConnection,
        QGenericReturnArgument(), Q_ARG(int,pipe->Transformation.getValue()));

    refresh();

    connSections = pipe->Sections.signalChanged.connect(
        [this](const App::Property &) {toggleShowOnTop(vp, lastSections, "Sections");});
}

TaskPipeScaling::~TaskPipeScaling()
{
    exitSelectionMode();
}

void TaskPipeScaling::refresh()
{
    if (!vp || !vp->getObject())
        return;

    // Temporarily prevent unnecessary feature recomputes
    for (QWidget* child : proxy->findChildren<QWidget*>())
        child->blockSignals(true);

    PartDesign::Pipe* pipe = static_cast<PartDesign::Pipe*>(vp->getObject());
    ui->listWidgetReferences->clear();
    for (auto obj : pipe->Sections.getValues())
        addItem(obj);

    int idx = pipe->Transformation.getValue();
    ui->comboBoxScaling->setCurrentIndex(idx);
    ui->stackedWidget->setCurrentIndex(idx);
    updateUI(idx);

    for (QWidget* child : proxy->findChildren<QWidget*>())
        child->blockSignals(false);

    TaskSketchBasedParameters::refresh();
}

void TaskPipeScaling::onSelectionModeChanged(SelectionMode) {
    if (getSelectionMode() == SelectionMode::refAdd) {
        ui->buttonRefAdd->setChecked(true);
        addBlinkWidget(ui->buttonRefAdd);
        toggleShowOnTop(vp, lastSections, "Sections", true);
    } else {
        removeBlinkWidget(ui->buttonRefAdd);
        ui->buttonRefAdd->setChecked(false);
        toggleShowOnTop(vp, lastSections, nullptr);
    }
}

void TaskPipeScaling::onButtonRefAdd(bool checked) {

    if (checked)
        setSelectionMode(SelectionMode::refAdd, new PipeSectionSelectionGate(this, vp));
    else
        exitSelectionMode();
}

void TaskPipeScaling::onScalingChanged(int idx) {
    if (!vp)
        return;
    ui->stackedWidget->setCurrentIndex(idx);
    updateUI(idx);
    try {
        setupTransaction();
        static_cast<PartDesign::Pipe*>(vp->getObject())->Transformation.setValue(idx);
        recomputeFeature();
    } catch (Base::Exception &e) {
        e.ReportException();
    }
    if (idx == 1) {
        onButtonRefAdd(true);
    } else
        exitSelectionMode();
}

void TaskPipeScaling::addItem(App::DocumentObject *obj, bool select)
{
    QString label = QString::fromUtf8(obj->Label.getValue());
    QListWidgetItem* item = new QListWidgetItem();
    auto vp = Gui::Application::Instance->getViewProvider(obj);
    if (vp)
        item->setIcon(vp->getIcon());
    item->setText(label);
    item->setData(Qt::UserRole, QVariant::fromValue(App::SubObjectT(obj)));
    ui->listWidgetReferences->addItem(item);
    if (select) {
        QSignalBlocker blocker(ui->listWidgetReferences);
        item->setSelected(true);
        ui->listWidgetReferences->scrollToItem(item);
    }
}

void TaskPipeScaling::_onSelectionChanged(const Gui::SelectionChanges& msg)
{
    if (!vp || _Busy)
        return;

    if (msg.Type == Gui::SelectionChanges::ClrSelection) {
        QSignalBlocker blocker(ui->listWidgetReferences);
        ui->listWidgetReferences->selectionModel()->clearSelection();
        return;
    }

    if (msg.Type != Gui::SelectionChanges::AddSelection)
        return;

    auto obj = msg.Object.getObject();
    if (!obj)
        return;

    PartDesign::Pipe* pipe = static_cast<PartDesign::Pipe*>(vp->getObject());

    switch(getSelectionMode()) {
    case SelectionMode::refAdd: {
        App::SubObjectT ref(msg.pOriginalMsg ? msg.pOriginalMsg->Object : msg.Object);
        ref = PartDesignGui::importExternalElement(ref);
        auto refObj = ref.getSubObject();
        if (refObj) {
            auto sections = pipe->Sections.getValues();
            sections.push_back(refObj);
            addItem(refObj, true);
            try {
                setupTransaction();
                pipe->Sections.setValues(sections);
                recomputeFeature();
            } catch (Base::Exception &e) {
                e.ReportException();
            }
        }
        break;
    }
    default:
        break;
    }
}

void TaskPipeScaling::onDeleteSection()
{
    if (!vp)
        return;
    PartDesign::Pipe* pipe = static_cast<PartDesign::Pipe*>(vp->getObject());
    std::vector<App::DocumentObject*> refs = pipe->Sections.getValues();

    // Delete the selected profile
    for(auto item : ui->listWidgetReferences->selectedItems()) {
        auto objT = qvariant_cast<App::SubObjectT>(item->data(Qt::UserRole));
        refs.erase(std::remove(refs.begin(), refs.end(), objT.getSubObject()), refs.end());
        delete item;
    }

    if (refs.size() != pipe->Sections.getValues().size()) {
        try {
            setupTransaction();
            pipe->Sections.setValues(refs);
            recomputeFeature();
        } catch (Base::Exception &e) {
            e.ReportException();
        }
    }
}

void TaskPipeScaling::updateUI(int idx) {

    //make sure we resize to the size of the current page
    for(int i=0; i<ui->stackedWidget->count(); ++i)
        ui->stackedWidget->widget(i)->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    if (idx < ui->stackedWidget->count())
        ui->stackedWidget->widget(idx)->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

}

void TaskPipeScaling::onItemEntered(QListWidgetItem *item)
{
    if (!vp)
        return;
    auto objT = qvariant_cast<App::SubObjectT>(item->data(Qt::UserRole));
    PartDesignGui::highlightObjectOnTop(objT);
}

bool TaskPipeScaling::eventFilter(QObject *, QEvent *ev)
{
    switch(ev->type()) {
    case QEvent::Leave:
        Gui::Selection().rmvPreselect();
        break;
    default:
        break;
    }
    return false;
}

void TaskPipeScaling::onItemSelectionChanged()
{
    if (!vp)
        return;
    auto items = ui->listWidgetReferences->selectedItems();
    Base::StateLocker lock(_Busy);
    if (items.isEmpty() || items.size() == 1) {
        Gui::Selection().selStackPush();
        Gui::Selection().clearSelection();
    }
    for (auto item : items) {
        PartDesignGui::selectObjectOnTop(
                qvariant_cast<App::SubObjectT>(item->data(Qt::UserRole)), true);
    }
}

//**************************************************************************
//**************************************************************************
// TaskDialog
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

TaskDlgPipeParameters::TaskDlgPipeParameters(ViewProviderPipe *PipeView,bool newObj)
   : TaskDlgSketchBasedParameters(PipeView)
{
    assert(PipeView);
    parameter    = new TaskPipeParameters(PipeView,newObj);
    orientation  = new TaskPipeOrientation(PipeView,newObj);
    scaling      = new TaskPipeScaling(PipeView,newObj);

    parameter->signalSelectionModeChanged.connect(
            [this](){onSelectionModeChanged(this->parameter);});
    orientation->signalSelectionModeChanged.connect(
            [this](){onSelectionModeChanged(this->orientation);});
    scaling->signalSelectionModeChanged.connect(
            [this](){onSelectionModeChanged(this->scaling);});

    Content.push_back(parameter);
    Content.push_back(orientation);
    Content.push_back(scaling);

    if (PipeView->getObject()->isTouched() || PipeView->getObject()->mustExecute())
        parameter->recomputeFeature();
}

TaskDlgPipeParameters::~TaskDlgPipeParameters()
{
}

void TaskDlgPipeParameters::onSelectionModeChanged(TaskSketchBasedParameters *source)
{
    if (_busy)
        return;
    Base::StateLocker guard(_busy);
    parameter->setSelectionMode(source->getSelectionMode(), nullptr);
    orientation->setSelectionMode(source->getSelectionMode(), nullptr);
    scaling->setSelectionMode(source->getSelectionMode(), nullptr);
}

//==== calls from the TaskView ===============================================================


bool TaskDlgPipeParameters::accept()
{
    PartDesign::Pipe* pcPipe = static_cast<PartDesign::Pipe*>(getPipeView()->getObject());
    try {
        parameter->setupTransaction();
        Gui::cmdGuiDocument(pcPipe, "resetEdit()");
        Gui::cmdAppDocument(pcPipe, "recompute()");
        if (!vp->getObject()->isValid())
            throw Base::RuntimeError(vp->getObject()->getStatusString());
        Gui::Command::commitCommand();
    }
    catch (const Base::Exception& e) {
        QMessageBox::warning(parameter, tr("Input error"), QString::fromUtf8(e.what()));
        return false;
    }

    return true;
}

#include "moc_TaskPipeParameters.cpp"
