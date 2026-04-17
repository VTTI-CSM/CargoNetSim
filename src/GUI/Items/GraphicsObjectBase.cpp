#include "GraphicsObjectBase.h"
#include "AnimationObject.h"
#include "Backend/Commons/LogCategories.h"
#include <QBrush>
#include <QGraphicsRectItem>
#include <QPen>
#include <QPropertyAnimation>
#include <QUuid>

using namespace CargoNetSim::GUI;

GraphicsObjectBase::GraphicsObjectBase(
    QGraphicsItem *parent)
    : QGraphicsObject{parent}
    , m_id{QUuid::createUuid().toString(
          QUuid::WithoutBraces)}
    , m_animObject{new AnimationObject(this)}
    , m_animation{new QPropertyAnimation(m_animObject,
                                         "opacity", this)}
{
    qCDebug(lcGuiScene)
        << "GraphicsObjectBase::GraphicsObjectBase:"
        << "id=" << m_id;

    // 4. Auto-delete when finished
    m_animation->setDuration(1000);
    m_animation->setLoopCount(3);
    m_animation->setStartValue(1.0);
    m_animation->setKeyValueAt(0.5, 0.0);
    m_animation->setEndValue(1.0);

    connect(m_animation, &QPropertyAnimation::finished,
            this, &GraphicsObjectBase::onAnimationFinished);
}

GraphicsObjectBase::~GraphicsObjectBase() = default;

QString GraphicsObjectBase::getID() const
{
    return m_id;
}

void GraphicsObjectBase::flash(bool          evenIfHidden,
                               const QColor &color)
{
    qCDebug(lcGuiScene)
        << "GraphicsObjectBase::flash:"
        << "id=" << m_id
        << "evenIfHidden=" << evenIfHidden
        << "color=" << color.name();

    bool wasHidden = !isVisible();
    if (evenIfHidden && wasHidden)
    {
        setVisible(true);
    }

    // Store state to restore after animation
    m_animObject->setWasHidden(wasHidden);
    m_animObject->setRestoreVisibility(evenIfHidden
                                       && wasHidden);

    // Stop any running animation
    if (m_animation->state() != QAbstractAnimation::Stopped)
    {
        m_animation->stop();
    }

    // Clear any existing animation visuals
    clearAnimationVisuals();

    // Create new visual based on derived class type
    createAnimationVisual(color);

    // Start animation
    m_animation->start();
}

void GraphicsObjectBase::clearAnimationVisuals()
{
    m_animObject->clearVisuals();
}

void GraphicsObjectBase::createAnimationVisual(
    const QColor &color)
{
    auto *rect =
        new QGraphicsRectItem(boundingRect(), this);
    rect->setBrush(QBrush(color));
    rect->setPen(QPen(Qt::NoPen));
    rect->setZValue(100);
    m_animObject->setRect(rect);
}

void GraphicsObjectBase::onAnimationFinished()
{
    qCDebug(lcGuiScene)
        << "GraphicsObjectBase::onAnimationFinished:"
        << "id=" << m_id;
    clearAnimationVisuals();
    if (m_animObject->shouldRestoreVisibility())
        setVisible(false);
}
