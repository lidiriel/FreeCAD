/***************************************************************************
 *   Copyright (c) 2013 Luke Parry <l.parry@warwick.ac.uk>                 *
 *   Copyright (c) 2019 Franck Jullien <franck.jullien@gmail.com>          *
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
  #include <BRep_Builder.hxx>
  #include <TopoDS_Compound.hxx>
  # include <TopoDS_Shape.hxx>
  # include <TopoDS_Edge.hxx>
  # include <TopoDS.hxx>
  # include <BRepAdaptor_Curve.hxx>
  # include <Precision.hxx>

  # include <QGraphicsScene>
  # include <QGraphicsSceneMouseEvent>
  # include <QPainter>
  # include <QPaintDevice>
  # include <QSvgGenerator>
//  # include <QApplication>

  # include <cmath>
#endif

#include <App/Application.h>
#include <App/Material.h>
#include <Base/Console.h>
#include <Base/Exception.h>
#include <Base/Parameter.h>
#include <Gui/Command.h>
#include <Gui/Control.h>
#include <string>

#include <Mod/TechDraw/App/DrawViewGDTReference.h>
#include <Mod/TechDraw/App/DrawViewPart.h>
#include <Mod/TechDraw/App/DrawUtil.h>
#include <Mod/TechDraw/App/Geometry.h>

#include "DrawGuiUtil.h"
#include "QGIArrow.h"
#include "QGIDimLines.h"
#include "QGIViewGDTReference.h"
#include "Rez.h"
#include "ViewProviderGDTReference.h"
#include "ZVALUE.h"


//TODO: hide the Qt coord system (+y down).  

using namespace TechDraw;
using namespace TechDrawGui;

QGIReferenceLabel::QGIReferenceLabel(QGIViewGDTReference * parent)
{
	this->parent = parent;

	posX = 0;
    posY = 0;

    m_marginHeight = 1.0;
    m_marginWidth = 2.0;

    setCacheMode(QGraphicsItem::NoCache);
    setFlag(ItemSendsGeometryChanges, true);
    setFlag(ItemIsMovable, true);
    setFlag(ItemIsSelectable, true);
    setAcceptHoverEvents(true);

    m_labelText = new QGCustomText();
    m_labelText->setParentItem(this);

    m_ctrl = false;
    hasHover = false;
}

QVariant QGIReferenceLabel::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemSelectedHasChanged && scene()) {
        if(isSelected()) {
            Q_EMIT selected(true);
            setPrettySel();
        } else {
            Q_EMIT selected(false);
            setPrettyNormal();
        }
        update();
    } else if(change == ItemPositionHasChanged && scene()) {
        setLabelCenter();
        Q_EMIT dragging(m_ctrl);
    }

    return QGraphicsItem::itemChange(change, value);
}

void QGIReferenceLabel::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
    if(event->modifiers() & Qt::ControlModifier) {
        m_ctrl = true;
    }

    if(scene() && this == scene()->mouseGrabberItem()) {
        Q_EMIT dragFinished();
    }
      QGraphicsItem::mousePressEvent(event);
}

void QGIReferenceLabel::mouseMoveEvent(QGraphicsSceneMouseEvent * event)
{
    QGraphicsItem::mouseMoveEvent(event);
}

void QGIReferenceLabel::mouseReleaseEvent(QGraphicsSceneMouseEvent * event)
{
    m_ctrl = false;
    if(scene() && this == scene()->mouseGrabberItem()) {
        Q_EMIT dragFinished();
    }

    QGraphicsItem::mouseReleaseEvent(event);
}

void QGIReferenceLabel::mouseDoubleClickEvent(QGraphicsSceneMouseEvent * event)
{
	// TODO add task managment
    //Gui::Control().showDialog(new TaskDlgReference(parent));
    //QGraphicsItem::mouseDoubleClickEvent(event);
}

void QGIReferenceLabel::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_EMIT hover(true);
    hasHover = true;
    if (!isSelected()) {
        setPrettyPre();
    } else {
        setPrettySel();
    }
    QGraphicsItem::hoverEnterEvent(event);
}

void QGIReferenceLabel::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    QGIView *view = dynamic_cast<QGIView *> (parentItem());
    assert(view != 0);
    Q_UNUSED(view);

    Q_EMIT hover(false);
    hasHover = false;
    if (!isSelected()) {
        setPrettyNormal();
    } else {
        setPrettySel();
    }
    QGraphicsItem::hoverLeaveEvent(event);
}

QRectF QGIReferenceLabel::boundingRect() const
{
    return childrenBoundingRect();
}

void QGIReferenceLabel::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
    Q_UNUSED(painter);
    QStyleOptionGraphicsItem myOption(*option);
    myOption.state &= ~QStyle::State_Selected;

    //QGraphicsObject/QGraphicsItem::paint gives link error.
}

void QGIReferenceLabel::setPosFromCenter(const double &xCenter, const double &yCenter)
{
    //set label's Qt position(top,left) given boundingRect center point
    setPos(xCenter - m_labelText->boundingRect().width() / 2., yCenter - m_labelText->boundingRect().height() / 2.);
}

void QGIReferenceLabel::setLabelCenter()
{
    //set label center (posX,posY) frome the given Qt position (top,left) == (x(),y())
    posX = x() + m_labelText->boundingRect().width() / 2.;
    posY = y() + m_labelText->boundingRect().height() / 2.;
}

void QGIReferenceLabel::setFont(QFont f)
{
    m_labelText->setFont(f);
}

void QGIReferenceLabel::setLabelString(QString t)
{
    prepareGeometryChange();
    m_labelText->setPlainText(t);
} 

void QGIReferenceLabel::setLabelString(QString t, qreal maxWidth)
{
    prepareGeometryChange();
    m_labelText->setPlainText(t);
    m_labelText->setTextWidth(maxWidth);
}

void QGIReferenceLabel::setPrettySel(void)
{
    m_labelText->setPrettySel();
}

void QGIReferenceLabel::setPrettyPre(void)
{
    m_labelText->setPrettyPre();
}

void QGIReferenceLabel::setPrettyNormal(void)
{
    m_labelText->setPrettyNormal();
}

void QGIReferenceLabel::setColor(QColor c)
{
    m_colNormal = c;
    m_labelText->setColor(m_colNormal);
}

//**************************************************************
// QGI VIEW REFERENCE
//**************************************************************
QGIViewGDTReference::QGIViewGDTReference() :
    hasHover(false),
    m_lineWidth(0.0)
{
    setHandlesChildEvents(false);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setCacheMode(QGraphicsItem::NoCache);

    referenceLabel = new QGIReferenceLabel(this);

    addToGroup(referenceLabel);
    referenceLabel->setColor(getNormalColor());
    referenceLabel->setPrettyNormal();

    referenceLines = new QGIDimLines();
    addToGroup(referenceLines);
    referenceLines->setNormalColor(getNormalColor());
    referenceLines->setPrettyNormal();

    referenceShape = new QGIDimLines();
    addToGroup(referenceShape);
    referenceShape->setNormalColor(getNormalColor());
    referenceShape->setPrettyNormal();
    
    referenceArrow = new QGIArrow();
    addToGroup(referenceArrow);
    referenceArrow->setNormalColor(getNormalColor());
    referenceArrow->setFillColor(getNormalColor());
    referenceArrow->setPrettyNormal();
    // TODO change style to PYRAMID
    //referenceArrow->setStyle(PYRAMID);

    referenceLabel->setZValue(ZVALUE::LABEL);
    referenceArrow->setZValue(ZVALUE::DIMENSION);
    referenceLines->setZValue(ZVALUE::DIMENSION);
    referenceLines->setStyle(Qt::SolidLine);

    referenceShape->setZValue(ZVALUE::DIMENSION);
    referenceShape->setStyle(Qt::SolidLine);

    referenceLabel->setPosFromCenter(0, 0);
    labelAngle = 0.;

    // connecting the needed slots and signals
    QObject::connect(
    		referenceLabel, SIGNAL(dragging(bool)),
        this  , SLOT  (referenceLabelDragged(bool)));

    QObject::connect(
    		referenceLabel, SIGNAL(dragFinished()),
        this  , SLOT  (referenceLabelDragFinished()));

    QObject::connect(
    		referenceLabel, SIGNAL(selected(bool)),
        this  , SLOT  (select(bool)));

    QObject::connect(
    		referenceLabel, SIGNAL(hover(bool)),
        this  , SLOT  (hover(bool)));

    setZValue(ZVALUE::DIMENSION);                    //note: this won't paint dimensions over another View if it stacks
                                                     //above this Dimension's parent view.   need Layers?

}

Base::Vector3d QGIViewGDTReference::calculateCenter(TechDraw::PointPair & segment){
	Base::Vector3d first = segment.first;
	Base::Vector3d second = segment.second;
	return (first+second)/2.0;
}

double QGIViewGDTReference::getIsoStandardLinePlacement(double labelAngle)
{
    // According to ISO 129-1 Standard Figure 23, the bordering angle is 2/3 PI, resp. -1/3 PI
    return labelAngle < -M_PI/3.0 || labelAngle > +2.0*M_PI/3.0
           ? +1.0 : -1.0;
}


Base::Vector3d QGIViewGDTReference::calculateLabelPlacement(TechDraw::PointPair & segment, Base::Vector3d & origin, double length){
	Base::Vector3d result;
	Base::Vector3d dir = segment.first-segment.second;
	Base::Vector3d base = segment.second;
	Base::Vector3d perp = origin.Perpendicular(base, dir);
	perp = perp.Normalize();
	// check orientation
	if(origin.Dot(perp) < 0)
		perp = -perp;
	result = origin + perp * length;

	double lineAngle = (fromQtApp(segment.second) - fromQtApp(segment.first)).Angle();
	double placementFactor = getIsoStandardLinePlacement(lineAngle);
	labelAngle = placementFactor > 0.0 ? DrawUtil::angleComposition(lineAngle, M_PI) : lineAngle;
	referenceLabel->linkDir = perp;

//	double angle = result.GetAngle(Base::Vector3d(0,1,0));
//	DrawUtil::angleNormalize(angle); // [-PI ; PI]
//	double angleLabel = 0;
//	if(-M_PI_4 <= angle && angle <= M_PI_4){
//		// linkDir south
//	}else if(M_PI_4 < angle && angle < 3*M_PI_4){
//		// linkDir east
//		//angle -= M_PI_2;
//	}else if(-M_PI_4 < angle && angle < -3*M_PI_4){
//		// linkDir west
//		//angle += M_PI_2;
//	}else{
//		// linkDir north
//		//angle -= M_PI;
//	}
	// referenceLabel->linkDir = Base::Vector2d(0., perp.y/abs(perp.y));
	// referenceLabel->linkDir = Base::Vector2d(perp.x/abs(perp.x), 0.);
	labelAngle = toQtDeg(labelAngle);
	lineAngle = toDeg(lineAngle);
	double angle = 0.;//toDeg(angle);
	Base::Console().Message("origin %.3f %.3f\n", origin.x, origin.y);
	Base::Console().Message("result %.3f %.3f  angle %.3f lineAngle %.3f labelAngle %3.f\n", result.x, result.y, angle, lineAngle, labelAngle);
	Base::Console().Message("linkDir %.3f %.3f\n",referenceLabel->linkDir.x, referenceLabel->linkDir.y);
	return result;
}


void QGIViewGDTReference::setViewPartFeature(TechDraw::DrawViewGDTReference *reference)
{
    if(reference == 0)
        return;

    setViewFeature(static_cast<TechDraw::DrawView *>(reference));

    double textWidth = referenceLabel->getLabelText()->boundingRect().width();
    double symbWidth = Rez::guiX(referenceLabel->marginWidth()) * 2 + textWidth;
    double distance = Rez::appX(2.5 * symbWidth);
    Base::Console().Message("symbWidth %.3f  line length %.3f\n", symbWidth, distance);
    PointPair edge = reference->getLinearPoints();
    Base::Vector3d origin = calculateCenter(edge);
    Base::Vector3d labelCenter = calculateLabelPlacement(edge, origin, distance);
    Base::Console().Message("labelcenter %.3f %.3f\n",labelCenter.x, labelCenter.y);

    reference->X.setValue(origin.x);
    reference->Y.setValue(origin.y);
    float x = Rez::guiX(labelCenter.x);
    float y = Rez::guiX(labelCenter.y);
    referenceLabel->setPosFromCenter(x, y);
    referenceLabel->setColor(getNormalColor());

    updateReference();
    draw();
}

void QGIViewGDTReference::select(bool state)
{
    setSelected(state);
    draw();
}

void QGIViewGDTReference::hover(bool state)
{
    hasHover = state;
    draw();
}

void QGIViewGDTReference::updateView(bool update)
{
    auto reference( dynamic_cast<TechDraw::DrawViewGDTReference*>(getViewObject()) );
    if( reference == nullptr )
        return;

    auto viewProvider = static_cast<ViewProviderGDTReference*>(getViewProvider(getViewObject()));
    if ( viewProvider == nullptr ) {
        return;
    }

    if(	update ||
    	reference->X.isTouched() || reference->Y.isTouched() ||
		reference->Text.isTouched() ||
		viewProvider->Font.isTouched() ||
		viewProvider->LineWidth.isTouched()){
			//float x = Rez::guiX(reference->X.getValue());
			//float y = Rez::guiX(reference->Y.getValue());
			//referenceLabel->setPosFromCenter(x,-y);
			updateReference();
    }
    draw();
}

void QGIViewGDTReference::updateReference()
{
    const auto reference( dynamic_cast<TechDraw::DrawViewGDTReference *>(getViewObject()) );
    if( reference == nullptr ) {
        return;
    }
    auto vp = static_cast<ViewProviderGDTReference*>(getViewProvider(getViewObject()));
    if ( vp == nullptr ) {
        return;
    }

    QFont font = referenceLabel->getFont();
    font.setPixelSize(calculateFontPixelSize(vp->Fontsize.getValue()));
    font.setFamily(QString::fromUtf8(vp->Font.getValue()));
    referenceLabel->setFont(font);

    prepareGeometryChange();
    QString labelText = QString::fromUtf8(reference->Text.getStrValue().data());
    referenceLabel->setLabelString(labelText, -1);
    referenceLabel->setPosFromCenter(referenceLabel->X(),referenceLabel->Y());
}

void QGIViewGDTReference::referenceLabelDragged(bool ctrl)
{
    draw_modifier(ctrl);
}

void QGIViewGDTReference::referenceLabelDragFinished()
{
    auto ref( dynamic_cast<TechDraw::DrawViewGDTReference *>(getViewObject()) );
    if( ref == nullptr ) {
        return;
    }

    //double x = Rez::appX(referenceLabel->X());
    //double y = Rez::appX(referenceLabel->Y());
    //Gui::Command::openCommand("Drag Reference");
    //Gui::Command::doCommand(Gui::Command::Doc,"App.ActiveDocument.%s.X = %f", ref->getNameInDocument(), x);
    //Gui::Command::doCommand(Gui::Command::Doc,"App.ActiveDocument.%s.Y = %f", ref->getNameInDocument(), -y);
    //Gui::Command::commitCommand();
}

void QGIViewGDTReference::draw()
{
    draw_modifier(false);
}

void QGIViewGDTReference::draw_modifier(bool modifier)
{
    if (!isVisible()) {		//should this be controlled by parent ViewPart?
        return;
    }

    TechDraw::DrawViewGDTReference *reference = dynamic_cast<TechDraw::DrawViewGDTReference *>(getViewObject());
    if((!reference) ||                                                       //nothing to draw, don't try
       (!reference->isDerivedFrom(TechDraw::DrawViewGDTReference::getClassTypeId()))) {
        referenceLabel->hide();
        hide();
        return;
    }

    referenceLabel->show();
    show();

    const TechDraw::DrawViewPart *refObj = reference->getViewPart();
    if(!refObj->hasGeometry()) {	//nothing to draw yet (restoring)
    	referenceLabel->hide();
        hide();
        return;
    }

    auto vp = static_cast<ViewProviderGDTReference*>(getViewProvider(getViewObject()));
    if ( vp == nullptr ) {
        return;
    }

    m_colNormal = getNormalColor();
    referenceLabel->setColor(m_colNormal);

    m_lineWidth = Rez::guiX(vp->LineWidth.getValue());

    double textWidth = referenceLabel->getLabelText()->boundingRect().width();
    double textHeight = referenceLabel->getLabelText()->boundingRect().height();

    Base::Console().Message("referenceLabel %.3f %.3f\n",referenceLabel->X(), referenceLabel->Y());

    if (reference->isLocked()) {
        referenceLabel->setFlag(QGraphicsItem::ItemIsMovable, false);
    } else
        referenceLabel->setFlag(QGraphicsItem::ItemIsMovable, true);

    QPointF centerPt = referenceLabel->boundingRect().center();
    referenceLabel->setTransformOriginPoint(centerPt);
    referenceLabel->setRotation(labelAngle);

    Base::Vector3d dLineStart;
    float scale = reference->SymbolScale.getValue();
    QPainterPath referencePath;

    // Square symbol
    textWidth = (textWidth * scale) + Rez::guiX(referenceLabel->marginWidth());
    textHeight = (textHeight * scale) + Rez::guiX(referenceLabel->marginHeight());
    double max = std::max(textWidth, textHeight);
    //referencePath.addRect(referenceLabel->X() -(max / 2.0), referenceLabel->Y() - (max / 2.0), max, max);
    double offset = (max / 2.0);
    QRectF squareSymbol(referenceLabel->X() -(max / 2.0), referenceLabel->Y() - (max / 2.0), max, max);
    referencePath.addRect(squareSymbol);

    Base::Vector2d origin, centerLabel;
    origin.x = Rez::guiX(reference->X.getValue());
    origin.y = Rez::guiX(reference->Y.getValue());
    centerLabel.x = referenceLabel->X();
    centerLabel.y = referenceLabel->Y();

    Base::Vector3d offsetVect = referenceLabel->linkDir * offset;
    dLineStart.x = referenceLabel->X() - offsetVect.x;
    dLineStart.y = referenceLabel->Y() - offsetVect.y;

    QPainterPath dLinePath;
    dLinePath.moveTo(dLineStart.x, dLineStart.y);
    dLinePath.lineTo(origin.x, origin.y);

    referenceLines->setPath(dLinePath);
    referenceShape->setPath(referencePath);
    referenceShape->setTransformOriginPoint(referenceShape->boundingRect().center());
    referenceShape->setRotation(labelAngle);
    referenceArrow->draw();

    Base::Vector3d origin3d(origin.x, origin.y, 0.0);
    Base::Vector3d dirreferenceLinesLine = (origin3d - dLineStart).Normalize();
    float arAngle = atan2(dirreferenceLinesLine.y, dirreferenceLinesLine.x) * 180 / M_PI;

    referenceArrow->setPos(origin.x, origin.y);
    referenceArrow->setRotation(arAngle);
    referenceArrow->show();

    // redraw the Reference and the parent View
    if (hasHover && !isSelected()) {
        referenceArrow->setPrettyPre();
        referenceLines->setPrettyPre();
        referenceShape->setPrettyPre();
    } else if (isSelected()) {
        referenceArrow->setPrettySel();
        referenceLines->setPrettySel();
        referenceShape->setPrettySel();
    } else {
        referenceArrow->setPrettyNormal();
        referenceLines->setPrettyNormal();
        referenceShape->setPrettyNormal();
    }

    update();
    if (parentItem()) {
        //TODO: parent redraw still required with new frame/label??
        parentItem()->update();
    } else {
        Base::Console().Log("INFO - QGIVD::draw - no parent to update\n");
    }

}

void QGIViewGDTReference::drawBorder(void)
{
	// Nothing doing !
}

QVariant QGIViewGDTReference::itemChange(GraphicsItemChange change, const QVariant &value)
{
   if (change == ItemSelectedHasChanged && scene()) {
        if(isSelected()) {
            referenceLabel->setSelected(true);
        } else {
            referenceLabel->setSelected(false);
        }
        draw();
    }
    return QGIView::itemChange(change, value);
}

void QGIViewGDTReference::paint ( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget) {
    QStyleOptionGraphicsItem myOption(*option);
    myOption.state &= ~QStyle::State_Selected;

    QPaintDevice* hw = painter->device();
    QSvgGenerator* svg = dynamic_cast<QSvgGenerator*>(hw);
    setPens();
    if (svg) {
        setSvgPens();
    } else {
        setPens();
    }
    QGIView::paint (painter, &myOption, widget);
    setPens();
}

void QGIViewGDTReference::setSvgPens(void)
{
    double svgLineFactor = 3.0;                     //magic number.  should be a setting somewhere.
    referenceLines->setWidth(m_lineWidth/svgLineFactor);
    referenceShape->setWidth(m_lineWidth/svgLineFactor);
    referenceArrow->setWidth(referenceArrow->getWidth()/svgLineFactor);
}

void QGIViewGDTReference::setPens(void)
{
	referenceLines->setWidth(m_lineWidth);
	referenceShape->setWidth(m_lineWidth);
    referenceArrow->setWidth(m_lineWidth);
}

QColor QGIViewGDTReference::getNormalColor()
{
    Base::Reference<ParameterGrp> hGrp = App::GetApplication().GetUserParameter()
                                        .GetGroup("BaseApp")->GetGroup("Preferences")->GetGroup("Mod/TechDraw/Dimensions");
    App::Color fcColor;
    fcColor.setPackedValue(hGrp->GetUnsigned("Color", 0x00000000));
    m_colNormal = fcColor.asValue<QColor>();

    auto reference( dynamic_cast<TechDraw::DrawViewGDTReference*>(getViewObject()) );
    if( reference == nullptr )
        return m_colNormal;

    auto viewProvider = static_cast<ViewProviderGDTReference*>(getViewProvider(getViewObject()));
    if ( viewProvider == nullptr ) {
        return m_colNormal;
    }

    m_colNormal = viewProvider->Color.getValue().asValue<QColor>();
    return m_colNormal;
}

#include <Mod/TechDraw/Gui/moc_QGIViewGDTReference.cpp>
