#include "ShapeIcon.h"
#include "Backend/Commons/LogCategories.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>

namespace CargoNetSim
{
namespace GUI
{

ShapeIcon::ShapeIcon(const QString &shapeType,
                     QWidget       *parent)
    : QWidget(parent)
    , m_shapeType(shapeType)
    , m_fillColor(Qt::lightGray)
    , m_borderColor(Qt::black)
    , m_borderWidth(1)
{
    qCInfo(lcGuiScene)
        << "ShapeIcon::ShapeIcon:"
        << "shapeType=" << shapeType;

    // Set transparent background
    setStyleSheet("background-color: transparent;");

    // Default size policy to fit content
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Set focus policy to not receive focus
    setFocusPolicy(Qt::NoFocus);
}

QString ShapeIcon::shapeType() const
{
    return m_shapeType;
}

void ShapeIcon::setShapeType(const QString &type)
{
    if (m_shapeType != type)
    {
        qCDebug(lcGuiScene)
            << "ShapeIcon::setShapeType:"
            << "old=" << m_shapeType
            << "new=" << type;
        m_shapeType = type;
        update();
        emit shapeTypeChanged(m_shapeType);
    }
}

QColor ShapeIcon::fillColor() const
{
    return m_fillColor;
}

void ShapeIcon::setFillColor(const QColor &color)
{
    if (m_fillColor != color)
    {
        qCDebug(lcGuiScene)
            << "ShapeIcon::setFillColor:"
            << "color=" << color.name();
        m_fillColor = color;
        update();
        emit fillColorChanged(m_fillColor);
    }
}

QColor ShapeIcon::borderColor() const
{
    return m_borderColor;
}

void ShapeIcon::setBorderColor(const QColor &color)
{
    if (m_borderColor != color)
    {
        qCDebug(lcGuiScene)
            << "ShapeIcon::setBorderColor:"
            << "color=" << color.name();
        m_borderColor = color;
        update();
        emit borderColorChanged(m_borderColor);
    }
}

int ShapeIcon::borderWidth() const
{
    return m_borderWidth;
}

void ShapeIcon::setBorderWidth(int width)
{
    if (m_borderWidth != width)
    {
        qCDebug(lcGuiScene)
            << "ShapeIcon::setBorderWidth:"
            << "width=" << width;
        m_borderWidth = width;
        update();
        emit borderWidthChanged(m_borderWidth);
    }
}

QSize ShapeIcon::sizeHint() const
{
    return QSize(24, 24);
}

QSize ShapeIcon::minimumSizeHint() const
{
    return QSize(12, 12);
}

void ShapeIcon::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    qCDebug(lcGuiScene)
        << "ShapeIcon::paintEvent:"
        << "shapeType=" << m_shapeType;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Configure painter
    painter.setPen(QPen(m_borderColor, m_borderWidth));
    painter.setBrush(QBrush(m_fillColor));

    // Calculate the available drawing area with margin
    QRect rect = this->rect().adjusted(
        m_borderWidth, m_borderWidth, -m_borderWidth,
        -m_borderWidth);

    // Draw the appropriate shape
    if (m_shapeType == "circle")
    {
        painter.drawEllipse(rect);
    }
    else if (m_shapeType == "rectangle")
    {
        painter.drawRect(rect);
    }
    else if (m_shapeType == "triangle")
    {
        QPainterPath path;
        path.moveTo(rect.center().x(), rect.top());
        path.lineTo(rect.left(), rect.bottom());
        path.lineTo(rect.right(), rect.bottom());
        path.lineTo(rect.center().x(), rect.top());
        painter.drawPath(path);
    }
    else if (m_shapeType == "diamond")
    {
        QPainterPath path;
        path.moveTo(rect.center().x(), rect.top());
        path.lineTo(rect.right(), rect.center().y());
        path.lineTo(rect.center().x(), rect.bottom());
        path.lineTo(rect.left(), rect.center().y());
        path.lineTo(rect.center().x(), rect.top());
        painter.drawPath(path);
    }
    else
    {
        // Default to circle if shape type is unknown
        painter.drawEllipse(rect);
    }
}

} // namespace GUI
} // namespace CargoNetSim
