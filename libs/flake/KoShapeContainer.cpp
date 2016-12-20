/* This file is part of the KDE project
 * Copyright (C) 2006-2010 Thomas Zander <zander@kde.org>
 * Copyright (C) 2007 Jan Hambrecht <jaham@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "KoShapeContainer.h"
#include "KoShapeContainer_p.h"
#include "KoShapeContainerModel.h"
#include "KoShapeStrokeModel.h"
#include "SimpleShapeContainerModel.h"
#include "KoShapeSavingContext.h"
#include "KoViewConverter.h"

#include <QPointF>
#include <QPainter>
#include <QPainterPath>

#include "kis_painting_tweaks.h"
#include "kis_assert.h"

KoShapeContainerPrivate::KoShapeContainerPrivate(KoShapeContainer *q)
    : KoShapePrivate(q),
      shapeInterface(q),
      model(0)
{
}

KoShapeContainerPrivate::~KoShapeContainerPrivate()
{
    delete model;
}

KoShapeContainerPrivate::KoShapeContainerPrivate(const KoShapeContainerPrivate &rhs, KoShapeContainer *q)
    : KoShapePrivate(rhs, q),
      shapeInterface(q),
      model(0)
{
}

KoShapeContainer::KoShapeContainer(KoShapeContainerModel *model)
        : KoShape(new KoShapeContainerPrivate(this))
{
    Q_D(KoShapeContainer);
    d->model = model;
}

KoShapeContainer::KoShapeContainer(KoShapeContainerPrivate *dd)
    : KoShape(dd)
{
    Q_D(KoShapeContainer);

    // HACK ALERT: the shapes are copied inside the model,
    //             but we still need to connect the to the
    //             hierarchy here!
    if (d->model) {
        Q_FOREACH (KoShape *shape, d->model->shapes()) {
            shape->setParent(this);
        }
    }
}

KoShapeContainer::~KoShapeContainer()
{
    Q_D(KoShapeContainer);
    if (d->model) {
        d->model->deleteOwnedShapes();
    }
}

void KoShapeContainer::addShape(KoShape *shape)
{
    shape->setParent(this);
}

void KoShapeContainer::removeShape(KoShape *shape)
{
    shape->setParent(0);
}

int  KoShapeContainer::shapeCount() const
{
    Q_D(const KoShapeContainer);
    if (d->model == 0)
        return 0;
    return d->model->count();
}

bool KoShapeContainer::isChildLocked(const KoShape *child) const
{
    Q_D(const KoShapeContainer);
    if (d->model == 0)
        return false;
    return d->model->isChildLocked(child);
}

void KoShapeContainer::setClipped(const KoShape *child, bool clipping)
{
    Q_D(KoShapeContainer);
    if (d->model == 0)
        return;
    d->model->setClipped(child, clipping);
}

void KoShapeContainer::setInheritsTransform(const KoShape *shape, bool inherit)
{
    Q_D(KoShapeContainer);
    if (d->model == 0)
        return;
    d->model->setInheritsTransform(shape, inherit);
}

bool KoShapeContainer::inheritsTransform(const KoShape *shape) const
{
    Q_D(const KoShapeContainer);
    if (d->model == 0)
        return false;
    return d->model->inheritsTransform(shape);
}

void KoShapeContainer::paint(QPainter &painter, const KoViewConverter &converter, KoShapePaintingContext &paintcontext)
{
    Q_D(KoShapeContainer);
    painter.save();
    paintComponent(painter, converter, paintcontext);
    painter.restore();
    if (d->model == 0 || d->model->count() == 0)
        return;

    QList<KoShape*> sortedObjects = d->model->shapes();
    qSort(sortedObjects.begin(), sortedObjects.end(), KoShape::compareShapeZIndex);

    // Do the following to revert the absolute transformation of the container
    // that is re-applied in shape->absoluteTransformation() later on. The transformation matrix
    // of the container has already been applied once before this function is called.
    QTransform baseMatrix = absoluteTransformation(&converter).inverted() * painter.transform();

    // clip the children to the parent outline.
    QTransform m;
    qreal zoomX, zoomY;
    converter.zoom(&zoomX, &zoomY);
    m.scale(zoomX, zoomY);
    painter.setClipPath(m.map(outline()), Qt::IntersectClip);

    QRectF toPaintRect = converter.viewToDocument(KisPaintingTweaks::safeClipBoundingRect(painter));
    toPaintRect = transform().mapRect(toPaintRect);
    // We'll use this clipRect to see if our child shapes lie within it.
    // Because shape->boundingRect() uses absoluteTransformation(0) we'll
    // use that as well to have the same (absolute) reference transformation
    // of our and the child's bounding boxes.
    QTransform absTrans = absoluteTransformation(0);
    QRectF clipRect = absTrans.map(outline()).boundingRect();


    Q_FOREACH (KoShape *shape, sortedObjects) {
        //debugFlake <<"KoShapeContainer::painting shape:" << shape->shapeId() <<"," << shape->boundingRect();
        if (!shape->isVisible())
            continue;

        // FIXME:this line breaks painting of the grouped shapes (probably deprecate clipping?)
        //if (!isClipped(shape))  // the shapeManager will have to draw those, or else we can't do clipRects
        //    continue;

        // don't try to draw a child shape that is not in the clipping rect of the painter.
        if (!clipRect.intersects(shape->boundingRect()))

            continue;

        painter.save();
        painter.setTransform(shape->absoluteTransformation(&converter) * baseMatrix);
        shape->paint(painter, converter, paintcontext);
        painter.restore();
        if (shape->stroke()) {
            painter.save();
            painter.setTransform(shape->absoluteTransformation(&converter) * baseMatrix);
            shape->stroke()->paint(shape, painter, converter);
            painter.restore();
        }
    }
}

void KoShapeContainer::shapeChanged(ChangeType type, KoShape* shape)
{
    Q_UNUSED(shape);
    Q_D(KoShapeContainer);
    if (d->model == 0)
        return;
    if (!(type == RotationChanged || type == ScaleChanged || type == ShearChanged
            || type == SizeChanged || type == PositionChanged || type == GenericMatrixChange))
        return;
    d->model->containerChanged(this, type);
    Q_FOREACH (KoShape *shape, d->model->shapes())
        shape->notifyChanged();
}

bool KoShapeContainer::isClipped(const KoShape *child) const
{
    Q_D(const KoShapeContainer);
    if (d->model == 0) // throw exception??
        return false;
    return d->model->isClipped(child);
}

void KoShapeContainer::update() const
{
    Q_D(const KoShapeContainer);
    KoShape::update();
    if (d->model)
        Q_FOREACH (KoShape *shape, d->model->shapes())
            shape->update();
}

QList<KoShape*> KoShapeContainer::shapes() const
{
    Q_D(const KoShapeContainer);
    if (d->model == 0)
        return QList<KoShape*>();

    return d->model->shapes();
}

KoShapeContainerModel *KoShapeContainer::model() const
{
    Q_D(const KoShapeContainer);
    return d->model;
}

KoShapeContainer::ShapeInterface *KoShapeContainer::shapeInterface()
{
    Q_D(KoShapeContainer);
    return &d->shapeInterface;
}

KoShapeContainer::ShapeInterface::ShapeInterface(KoShapeContainer *_q)
    : q(_q)
{
}

void KoShapeContainer::ShapeInterface::addShape(KoShape *shape)
{
    KoShapeContainerPrivate * const d = q->d_func();

    Q_ASSERT(shape);
    if (shape->parent() == q && q->shapes().contains(shape)) {
        return;
    }

    // TODO add a method to create a default model depending on the shape container
    if (!d->model) {
        d->model = new SimpleShapeContainerModel();
    }

    if (shape->parent() && shape->parent() != q) {
        shape->parent()->shapeInterface()->removeShape(shape);
    }

    d->model->add(shape);
}

void KoShapeContainer::ShapeInterface::removeShape(KoShape *shape)
{
    KoShapeContainerPrivate * const d = q->d_func();

    KIS_SAFE_ASSERT_RECOVER_RETURN(shape);
    KIS_SAFE_ASSERT_RECOVER_RETURN(d->model);
    KIS_SAFE_ASSERT_RECOVER_RETURN(d->model->shapes().contains(shape));

    d->model->remove(shape);

    KoShapeContainer *grandparent = q->parent();
    if (grandparent) {
        grandparent->model()->childChanged(q, KoShape::ChildChanged);
    }
}
