#include "IconCreator.h"

#include "Backend/Commons/LogCategories.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <QRadialGradient>
#include <QRect>
#include <QRectF>
#include <cmath>

namespace CargoNetSim
{
namespace GUI
{
namespace IconFactory
{

//------------------------------------------------------------------------------
// Terminal Icons
//------------------------------------------------------------------------------
QMap<QString, QPixmap> createTerminalIcons()
{
    qCDebug(lcGuiUtil) << "IconFactory::createTerminalIcons()";
    QMap<QString, QPixmap> icons;

    // Define modern colors
    QColor origin_red("#E53935");        // Material Red
    QColor dest_blue("#1E88E5");         // Material Blue
    QColor port_blue("#0D47A1");         // Dark Blue
    QColor intermodal_orange("#FB8C00"); // Material Orange
    QColor train_gray("#424242");  // Material Dark Gray
    QColor truck_green("#2E7D32"); // Material Green

    // --- Sea Port Terminal (Ship and container design) ---
    {
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        // Draw water
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#BBDEFB"));
        painter.drawRect(0, 24, 32, 8);
        // Draw ship hull
        QPainterPath path;
        path.moveTo(4, 20);
        path.lineTo(28, 20);
        path.lineTo(24, 28);
        path.lineTo(8, 28);
        path.closeSubpath();
        QLinearGradient gradient(4, 20, 28, 28);
        gradient.setColorAt(0, port_blue);
        gradient.setColorAt(1, port_blue.darker(120));
        painter.setBrush(gradient);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawPath(path);
        // Draw container
        painter.setBrush(QColor("#FFA000"));
        painter.drawRect(10, 12, 12, 8);
        painter.end();
        icons.insert("Sea Port Terminal", pixmap);
    }

    // --- Intermodal Terminal (Modern container transfer
    // design) ---
    {
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        // Draw crane structure
        painter.setPen(QPen(Qt::black, 2));
        painter.setBrush(intermodal_orange);
        painter.drawRect(14, 6, 4, 20);
        painter.drawRect(4, 8, 24, 4);
        // Draw containers
        painter.setBrush(QColor("#1976D2"));
        painter.drawRect(6, 16, 8, 6);
        painter.setBrush(QColor("#388E3C"));
        painter.drawRect(18, 16, 8, 6);
        painter.end();
        icons.insert("Intermodal Land Terminal", pixmap);
    }

    // --- Train Terminal (Modern railway design) ---
    {
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        QColor platform_gray("#BDBDBD"); // Light gray
        painter.setBrush(platform_gray);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawRect(4, 20, 24, 6);
        QColor rails_gray("#E0E0E0"); // Even lighter gray
        painter.setPen(QPen(rails_gray, 2));
        painter.drawLine(2, 28, 30, 28);
        painter.drawLine(2, 30, 30, 30);
        for (int x = 4; x < 29; x += 6)
        {
            painter.drawLine(x, 27, x, 31);
        }
        painter.end();
        icons.insert("Train Stop/Depot", pixmap);
    }

    // --- Truck Parking (Modern parking design) ---
    {
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(truck_green, 1));
        painter.setBrush(truck_green.lighter(150));
        for (int x = 4; x < 28; x += 8)
        {
            painter.drawRect(x, 8, 6, 16);
        }
        QFont font("Arial", 14, QFont::Bold);
        painter.setFont(font);
        painter.setPen(truck_green);
        painter.drawText(QRect(11, 24, 12, 12),
                         Qt::AlignCenter, "P");
        painter.end();
        icons.insert("Truck Parking", pixmap);
    }

    // --- Facility (Factory / Warehouse) ---
    {
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        QColor brown(0x5D, 0x40, 0x37);
        painter.setBrush(brown);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawRect(6, 8, 20, 20);
        QColor lightBrown(0x79, 0x55, 0x48);
        painter.setBrush(lightBrown);
        painter.drawRect(13, 16, 6, 12);
        // Roof
        QPolygon roof;
        roof << QPoint(4, 8) << QPoint(16, 2)
             << QPoint(28, 8);
        painter.setBrush(QColor(0x3E, 0x27, 0x23));
        painter.drawPolygon(roof);
        painter.end();
        icons.insert(QStringLiteral("Facility"), pixmap);
        qCDebug(lcGuiUtil)
            << "IconCreator::createTerminalIcons: Facility"
            << "size=" << pixmap.size();
    }

    return icons;
}

//------------------------------------------------------------------------------
// Connect Terminals Icon
//------------------------------------------------------------------------------
QPixmap createConnectTerminalsPixmap(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createConnectTerminalsPixmap() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor("#2E86C1"), 2);
    painter.setPen(pen);
    int    radius = 4;
    QPoint left_center(8, size / 2);
    QPoint right_center(size - 8, size / 2);
    painter.drawEllipse(left_center, radius, radius);
    painter.drawEllipse(right_center, radius, radius);
    painter.drawLine(left_center, right_center);
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Assign Selected to Current Region Icon
//------------------------------------------------------------------------------
QPixmap createAssignSelectedToCurrentRegionPixmap(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createAssignSelectedToCurrentRegionPixmap() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QRect selection_rect(4, 4, size / 2 - 4, size / 2 - 4);
    QPen  pen(QColor("#3498DB"), 2, Qt::DashLine);
    painter.setPen(pen);
    painter.drawRect(selection_rect);
    QRect region_rect(size / 2, size / 2, size / 2 - 4,
                      size / 2 - 4);
    pen = QPen(QColor("#27AE60"), 2);
    painter.setPen(pen);
    painter.drawRect(region_rect);
    QPoint selection_center = selection_rect.center();
    QPoint region_center    = region_rect.center();
    pen                     = QPen(QColor("#2C3E50"), 2);
    painter.setPen(pen);
    painter.drawLine(selection_center, region_center);
    double arrow_size = 4;
    double angle      = std::atan2(
        region_center.y() - selection_center.y(),
        region_center.x() - selection_center.x());
    QPointF arrow_p1(
        region_center.x()
            - arrow_size * std::cos(angle - M_PI / 6),
        region_center.y()
            - arrow_size * std::sin(angle - M_PI / 6));
    QPointF arrow_p2(
        region_center.x()
            - arrow_size * std::cos(angle + M_PI / 6),
        region_center.y()
            - arrow_size * std::sin(angle + M_PI / 6));
    QPolygonF arrow_head;
    arrow_head << QPointF(region_center) << arrow_p1
               << arrow_p2;
    painter.setBrush(QBrush(QColor("#2C3E50")));
    painter.drawPolygon(arrow_head);
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Set Background Color Icon
//------------------------------------------------------------------------------
QPixmap createSetBackgroundColorPixmap(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createSetBackgroundColorPixmap() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QRect           rect(4, 4, size - 8, size - 8);
    QLinearGradient gradient(QPointF(rect.topLeft()),
                             QPointF(rect.bottomRight()));
    gradient.setColorAt(0, QColor("#D6EAF8"));
    gradient.setColorAt(1, QColor("#AED6F1"));
    painter.setBrush(QBrush(gradient));
    QPen pen(QColor("#1F618D"), 2);
    painter.setPen(pen);
    painter.drawRect(rect);
    painter.drawLine(rect.topLeft(), rect.bottomRight());
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Measure Distance Icon
//------------------------------------------------------------------------------
QPixmap createMeasureDistancePixmap(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createMeasureDistancePixmap() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor("#2E86C1"), 2);
    painter.setPen(pen);
    painter.drawLine(4, size / 2, size - 4, size / 2);
    for (int x = 6; x < size - 4; x += 6)
    {
        if (x % 12 == 0)
            painter.drawLine(x, size / 2 - 6, x,
                             size / 2 + 6);
        else
            painter.drawLine(x, size / 2 - 4, x,
                             size / 2 + 4);
    }
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Clear Measurements Icon
//------------------------------------------------------------------------------
QPixmap createClearMeasurementsPixmap(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createClearMeasurementsPixmap() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor("#2E86C1"), 2);
    painter.setPen(pen);
    int ruler_y = size * 3 / 4;
    painter.drawLine(8, ruler_y, size - 8, ruler_y);
    for (int x = 10; x < size - 8; x += 5)
    {
        if (x % 10 == 0)
            painter.drawLine(x, ruler_y - 4, x,
                             ruler_y + 4);
        else
            painter.drawLine(x, ruler_y - 2, x,
                             ruler_y + 2);
    }
    pen = QPen(QColor("#E74C3C"), 2);
    painter.setPen(pen);
    int margin = 6;
    painter.drawLine(margin, margin, size - margin,
                     size - margin);
    painter.drawLine(size - margin, margin, margin,
                     size - margin);
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Properties Icon
//------------------------------------------------------------------------------
QPixmap createPropertiesIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createPropertiesIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor("#4CAF50"),
             std::max(1, int(size * 0.08)));
    painter.setPen(pen);
    int rect_height = int(size * 0.15);
    for (int i = 0; i < 3; ++i)
    {
        int y_offset =
            int((size * 0.2) + (i * rect_height * 1.5));
        painter.drawRect(int(size * 0.2), y_offset,
                         int(size * 0.6), rect_height);
    }
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Freight Terminal Library Icon
//------------------------------------------------------------------------------
QPixmap createFreightTerminalLibraryIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createFreightTerminalLibraryIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    // Use a QStringList to hold the container colors.
    const QStringList colors = {"#FF9800", "#F57C00",
                                "#FFA726"};
    for (int i = 0; i < colors.size(); ++i)
    {
        painter.setBrush(QBrush(QColor(colors[i])));
        painter.setPen(
            QPen(Qt::black, std::max(1, int(size * 0.03))));
        painter.drawRect(int(size * 0.2),
                         int(size * 0.2 + i * size * 0.2),
                         int(size * 0.6), int(size * 0.15));
    }
    painter.setPen(QPen(QColor("#000000"),
                        std::max(1, int(size * 0.04))));
    int crane_x = int(size * 0.5);
    painter.drawLine(crane_x, int(size * 0.05), crane_x,
                     int(size * 0.2));
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Region Manager Icon
//------------------------------------------------------------------------------
QPixmap createRegionManagerIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createRegionManagerIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor hub_color("#C5E1A5");  // Lighter green
    QColor node_color("#81D4FA"); // Lighter blue
    QColor link_color("#AED581"); // Soft green

    painter.setBrush(QBrush(hub_color));
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.04))));
    int hub_x = int(size * 0.45);
    int hub_y = int(size * 0.1);
    painter.drawEllipse(hub_x, hub_y, int(size * 0.12),
                        int(size * 0.12));

    // Terminal nodes
    QVector<QPoint> nodes;
    nodes.append(QPoint(int(size * 0.2), int(size * 0.6)));
    nodes.append(QPoint(int(size * 0.5), int(size * 0.5)));
    nodes.append(QPoint(int(size * 0.8), int(size * 0.6)));
    for (const QPoint &pt : nodes)
    {
        painter.setBrush(QBrush(node_color));
        // Draw each node as an ellipse (using half the
        // intended diameter)
        painter.drawEllipse(pt, int(size * 0.06),
                            int(size * 0.06));
    }

    QPen pen(link_color, std::max(1, int(size * 0.03)));
    painter.setPen(pen);
    for (const QPoint &pt : nodes)
    {
        painter.drawLine(hub_x + int(size * 0.06),
                         hub_y + int(size * 0.06),
                         pt.x() + int(size * 0.06),
                         pt.y() + int(size * 0.06));
    }

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Simulation Settings Icon
//------------------------------------------------------------------------------
QPixmap createSimulationSettingsIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createSimulationSettingsIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    int             bar_width   = int(size * 0.15);
    QVector<double> heights     = {0.7, 0.5, 0.3};
    QVector<int>    x_positions = {
        int(size * 0.2), int(size * 0.45), int(size * 0.7)};

    for (int i = 0; i < 3; ++i)
    {
        painter.setBrush(
            QBrush(QColor("#9C27B0"))); // Purple color
        painter.setPen(Qt::NoPen);
        int rect_x      = x_positions[i];
        int rect_y      = int(size * (1 - heights[i]));
        int rect_height = int(size * heights[i]);
        painter.drawRect(rect_x, rect_y, bar_width,
                         rect_height);
    }

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Show/Hide Grid Icon
//------------------------------------------------------------------------------
QPixmap createShowHideGridIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createShowHideGridIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen pen(QColor("#B0BEC5"),
             std::max(1, int(size * 0.03)));
    painter.setPen(pen);

    int num_lines = 4;
    int spacing   = size / (num_lines + 1);
    for (int i = 1; i <= num_lines; ++i)
    {
        int x = i * spacing;
        painter.drawLine(x, int(size * 0.1), x,
                         int(size * 0.9));
        int y = i * spacing;
        painter.drawLine(int(size * 0.1), y,
                         int(size * 0.9), y);
    }

    // Toggle eye symbol
    int eye_x      = int(size * 0.65);
    int eye_y      = int(size * 0.75);
    int eye_width  = int(size * 0.3);
    int eye_height = int(size * 0.15);
    painter.setBrush(QBrush(QColor("#EF9A9A")));
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.04))));
    painter.drawEllipse(eye_x, eye_y, eye_width,
                        eye_height);
    painter.setBrush(QBrush(Qt::black));
    painter.drawEllipse(eye_x + eye_width / 3,
                        eye_y + eye_height / 3,
                        eye_width / 3, eye_height / 3);

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Freight Train Icon
//------------------------------------------------------------------------------
QPixmap createFreightTrainIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createFreightTrainIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    int train_body_height = int(size * 0.35);
    int train_body_width  = int(size * 0.7);
    int wheel_radius      = int(size * 0.08);
    int track_y           = int(size * 0.85);

    QLinearGradient gradient(
        size * 0.1, size * 0.3, size * 0.1,
        size * 0.3 + train_body_height);
    gradient.setColorAt(0,
                        QColor("#F44336")); // Lighter red
    gradient.setColorAt(1, QColor("#B71C1C")); // Darker red
    painter.setBrush(gradient);
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.drawRoundedRect(
        int(size * 0.1), int(size * 0.3), int(size * 0.25),
        train_body_height, 8, 8);

    QLinearGradient window_gradient(
        size * 0.15, size * 0.35, size * 0.15, size * 0.45);
    window_gradient.setColorAt(0, QColor("#FFFFFF"));
    window_gradient.setColorAt(1, QColor("#B0BEC5"));
    painter.setBrush(window_gradient);
    painter.drawRoundedRect(
        int(size * 0.15), int(size * 0.35), int(size * 0.1),
        int(size * 0.1), 5, 5);

    QRadialGradient radial_gradient(
        size * 0.18, size * 0.45, size * 0.04);
    radial_gradient.setColorAt(0, QColor("#FFF176"));
    radial_gradient.setColorAt(1, QColor("#FDD835"));
    painter.setBrush(radial_gradient);
    painter.drawEllipse(int(size * 0.16), int(size * 0.43),
                        int(size * 0.06), int(size * 0.06));

    QList<QPair<QString, QString>> container_colors = {
        {"#2196F3", "#1565C0"}, {"#4CAF50", "#2E7D32"}};
    for (int i = 0; i < container_colors.size(); ++i)
    {
        int x_offset = int(size * (0.38 + i * 0.28));
        QLinearGradient container_gradient(
            x_offset, size * 0.3, x_offset,
            size * 0.3 + train_body_height);
        container_gradient.setColorAt(
            0, QColor(container_colors[i].first));
        container_gradient.setColorAt(
            1, QColor(container_colors[i].second));
        painter.setBrush(container_gradient);
        painter.drawRoundedRect(
            x_offset, int(size * 0.32), int(size * 0.25),
            int(train_body_height * 0.8), 6, 6);
        painter.setPen(
            QPen(QColor(container_colors[i].second),
                 std::max(1, int(size * 0.01))));
        for (int y = int(size * 0.34); y < int(size * 0.6);
             y += int(size * 0.06))
        {
            painter.drawLine(
                x_offset + 2, y,
                x_offset + int(size * 0.25) - 2, y);
        }
    }

    QRadialGradient wheel_gradient;
    wheel_gradient.setColorAt(0, QColor("#9E9E9E"));
    wheel_gradient.setColorAt(1, QColor("#424242"));
    for (int i = 0; i < 4; ++i)
    {
        int x_offset = int(size * (0.2 + i * 0.2));
        wheel_gradient.setCenter(x_offset + wheel_radius,
                                 track_y + wheel_radius);
        wheel_gradient.setRadius(wheel_radius);
        painter.setBrush(wheel_gradient);
        painter.setPen(
            QPen(Qt::black, std::max(1, int(size * 0.02))));
        painter.drawEllipse(x_offset, track_y,
                            wheel_radius * 2,
                            wheel_radius * 2);
        painter.setBrush(QBrush(QColor("#757575")));
        painter.drawEllipse(
            x_offset + int(wheel_radius * 0.6),
            track_y + int(wheel_radius * 0.6),
            int(wheel_radius * 0.8),
            int(wheel_radius * 0.8));
    }

    QColor track_color("#E0E0E0");
    QColor tie_color("#8D6E63");
    painter.setPen(
        QPen(track_color, std::max(2, int(size * 0.02))));
    int y_offset = track_y + wheel_radius;
    painter.drawLine(int(size * 0.05), y_offset,
                     int(size * 0.95), y_offset);
    painter.drawLine(int(size * 0.05), y_offset + 4,
                     int(size * 0.95), y_offset + 4);
    QLinearGradient tie_gradient(0, y_offset, 0,
                                 y_offset + size * 0.04);
    tie_gradient.setColorAt(0, tie_color);
    tie_gradient.setColorAt(1, tie_color.darker(120));
    for (int i = 0; i < 8; ++i)
    {
        int x_tie = int(size * (0.1 + i * 0.1));
        painter.setBrush(tie_gradient);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawRect(x_tie, y_offset - int(size * 0.01),
                         int(size * 0.08),
                         int(size * 0.04));
    }

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Freight Truck Icon
//------------------------------------------------------------------------------
QPixmap createFreightTruckIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createFreightTruckIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    int truck_body_height = int(size * 0.32);
    int truck_body_width  = int(size * 0.65);
    int cab_width         = int(size * 0.22);
    int cab_height        = int(size * 0.38);
    int wheel_radius      = int(size * 0.08);
    int road_y            = int(size * 0.85);

    QLinearGradient trailer_gradient(
        size * 0.2, size * 0.35, size * 0.2,
        size * 0.35 + truck_body_height);
    trailer_gradient.setColorAt(0, QColor("#1E88E5"));
    trailer_gradient.setColorAt(1, QColor("#1565C0"));
    painter.setBrush(trailer_gradient);
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.drawRoundedRect(
        int(size * 0.2), int(size * 0.35), truck_body_width,
        truck_body_height, 6, 6);

    painter.setPen(QPen(QColor("#0D47A1"),
                        std::max(1, int(size * 0.01))));
    for (int y = int(size * 0.38); y < int(size * 0.62);
         y += int(size * 0.06))
    {
        painter.drawLine(int(size * 0.22), y,
                         int(size * 0.83), y);
    }

    QLinearGradient cab_gradient(size * 0.1, size * 0.4,
                                 size * 0.1,
                                 size * 0.4 + cab_height);
    cab_gradient.setColorAt(0, QColor("#F44336"));
    cab_gradient.setColorAt(1, QColor("#C62828"));
    painter.setBrush(cab_gradient);
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.drawRoundedRect(int(size * 0.1),
                            int(size * 0.4), cab_width,
                            cab_height, 8, 8);

    QLinearGradient window_gradient(
        size * 0.12, size * 0.42, size * 0.12, size * 0.5);
    window_gradient.setColorAt(0, QColor("#FFFFFF"));
    window_gradient.setColorAt(1, QColor("#B0BEC5"));
    painter.setBrush(window_gradient);
    painter.drawRoundedRect(
        int(size * 0.12), int(size * 0.42),
        int(size * 0.12), int(size * 0.1), 4, 4);

    QRadialGradient radial_gradient(
        size * 0.15, size * 0.53, size * 0.04);
    radial_gradient.setColorAt(0, QColor("#FFF176"));
    radial_gradient.setColorAt(1, QColor("#FDD835"));
    painter.setBrush(radial_gradient);
    painter.drawEllipse(int(size * 0.12), int(size * 0.51),
                        int(size * 0.06), int(size * 0.06));

    QRadialGradient wheel_gradient;
    wheel_gradient.setColorAt(0, QColor("#9E9E9E"));
    wheel_gradient.setColorAt(1, QColor("#424242"));
    QList<double> wheel_positions = {0.2, 0.4, 0.6, 0.8};
    for (double pos : wheel_positions)
    {
        int x_offset = int(size * pos);
        wheel_gradient.setCenter(x_offset + wheel_radius,
                                 road_y + wheel_radius);
        wheel_gradient.setRadius(wheel_radius);
        painter.setBrush(wheel_gradient);
        painter.setPen(
            QPen(Qt::black, std::max(1, int(size * 0.02))));
        painter.drawEllipse(x_offset, road_y,
                            wheel_radius * 2,
                            wheel_radius * 2);
        painter.setBrush(QBrush(QColor("#757575")));
        painter.drawEllipse(
            x_offset + int(wheel_radius * 0.6),
            road_y + int(wheel_radius * 0.6),
            int(wheel_radius * 0.8),
            int(wheel_radius * 0.8));
    }

    QLinearGradient road_gradient(
        0, road_y + wheel_radius, 0,
        road_y + wheel_radius + size * 0.05);
    road_gradient.setColorAt(0, QColor("#757575"));
    road_gradient.setColorAt(1, QColor("#424242"));
    painter.setBrush(road_gradient);
    painter.setPen(Qt::NoPen);
    painter.drawRect(int(size * 0.05),
                     int(road_y + wheel_radius),
                     int(size * 0.9), int(size * 0.05));

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Network Manager Icon
//------------------------------------------------------------------------------
QPixmap createNetworkManagerIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createNetworkManagerIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor hub_color("#A5D6A7");
    QColor node_color("#90CAF9");
    QColor link_color("#64B5F6");

    painter.setBrush(QBrush(hub_color));
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.03))));
    int hub_x      = int(size * 0.45);
    int hub_y      = int(size * 0.45);
    int hub_radius = int(size * 0.12);
    painter.drawEllipse(hub_x, hub_y, hub_radius,
                        hub_radius);

    QVector<QPoint> nodes;
    nodes.append(QPoint(int(size * 0.15), int(size * 0.2)));
    nodes.append(QPoint(int(size * 0.8), int(size * 0.25)));
    nodes.append(
        QPoint(int(size * 0.75), int(size * 0.75)));
    nodes.append(QPoint(int(size * 0.2), int(size * 0.8)));

    QPen pen(link_color, std::max(1, int(size * 0.02)));
    painter.setPen(pen);
    for (const QPoint &pt : nodes)
    {
        QPainterPath path;
        path.moveTo(hub_x + hub_radius / 2,
                    hub_y + hub_radius / 2);
        int control_x = (hub_x + pt.x()) / 2;
        int control_y = (hub_y + pt.y()) / 2;
        path.quadTo(control_x, control_y,
                    pt.x() + hub_radius / 2,
                    pt.y() + hub_radius / 2);
        painter.drawPath(path);
    }
    for (const QPoint &pt : nodes)
    {
        painter.setBrush(QBrush(node_color));
        painter.setPen(
            QPen(Qt::black, std::max(1, int(size * 0.02))));
        painter.drawEllipse(pt, hub_radius, hub_radius);
    }
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Link Terminal Icon
//------------------------------------------------------------------------------
QPixmap createLinkTerminalIcon()
{
    qCDebug(lcGuiUtil) << "IconFactory::createLinkTerminalIcon()";
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(QBrush(Qt::white));
    painter.drawEllipse(4, 4, 12, 12);
    painter.drawRect(16, 16, 12, 12);
    painter.drawLine(12, 12, 20, 20);
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Unlink Terminal Icon
//------------------------------------------------------------------------------
QPixmap createUnlinkTerminalIcon()
{
    qCDebug(lcGuiUtil) << "IconFactory::createUnlinkTerminalIcon()";
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(QBrush(Qt::white));
    painter.drawEllipse(4, 4, 12, 12);
    painter.drawRect(16, 16, 12, 12);
    QPen pen(QColor("#E74C3C"), 3);
    painter.setPen(pen);
    int margin = 6;
    painter.drawLine(margin, margin, 26, 26);
    painter.drawLine(26, margin, margin, 26);
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Link Terminals to Network Icon
//------------------------------------------------------------------------------
QPixmap createLinkTerminalsToNetworkIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createLinkTerminalsToNetworkIcon() size=" << size;
    // Start with the base link terminal icon
    QPixmap pixmap = createLinkTerminalIcon();

    // Add network node representation
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw a small "AL" for Network
    QFont font("Arial", size / 3, QFont::Bold);
    painter.setPen(QPen(QColor(30, 144, 255), 2));
    painter.setFont(font);
    painter.drawText(pixmap.rect(),
                     Qt::AlignTop | Qt::AlignHCenter, "AL");

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Unlink Terminals to Network Icon
//------------------------------------------------------------------------------
QPixmap createUnlinkTerminalsToNetworkIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createUnlinkTerminalsToNetworkIcon() size=" << size;
    // Start with the base link terminal icon
    QPixmap pixmap = createUnlinkTerminalIcon();

    // Add network node representation
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw a small "AL" for Network
    QFont font("Arial", size / 3, QFont::Bold);
    painter.setPen(QPen(QColor(30, 144, 255), 2));
    painter.setFont(font);
    painter.drawText(pixmap.rect(),
                     Qt::AlignTop | Qt::AlignHCenter, "AL");

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Auto Connect Terminals Icon
//------------------------------------------------------------------------------
QPixmap createAutoConnectTerminalsIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createAutoConnectTerminalsIcon() size=" << size;
    QPixmap  pixmap = createConnectTerminalsPixmap(size);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont font("Arial", size / 3, QFont::Bold);
    painter.setFont(font);
    painter.setPen(QPen(QColor("#E74C3C")));
    painter.drawText(pixmap.rect(),
                     Qt::AlignTop | Qt::AlignHCenter, "AN");
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Connect by Interface Icon
//------------------------------------------------------------------------------
QPixmap createConnectByInterfaceIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createConnectByInterfaceIcon() size=" << size;
    QPixmap  pixmap = createConnectTerminalsPixmap(size);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont font("Arial", size / 3, QFont::Bold);
    painter.setFont(font);
    painter.setPen(QPen(QColor("#E74C3C")));
    painter.drawText(pixmap.rect(),
                     Qt::AlignTop | Qt::AlignHCenter, "AI");

    int    indicator_size = size / 8;
    QColor indicator_color("#4CAF50");
    painter.setBrush(QBrush(indicator_color));
    painter.setPen(Qt::NoPen);
    // Left terminal indicator
    painter.drawRect(size / 4 - indicator_size / 2,
                     size / 2 + indicator_size,
                     indicator_size, indicator_size);
    // Right terminal indicator
    painter.drawRect(3 * size / 4 - indicator_size / 2,
                     size / 2 + indicator_size,
                     indicator_size, indicator_size);
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Check Network Icon
//------------------------------------------------------------------------------
QPixmap createCheckNetworkIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createCheckNetworkIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor node_color("#3498DB");
    QColor connection_color("#2ECC71");
    QColor checkmark_color("#27AE60");

    QVector<QPointF> nodes;
    nodes.append(QPointF(size * 0.5, size * 0.2));
    nodes.append(QPointF(size * 0.2, size * 0.4));
    nodes.append(QPointF(size * 0.8, size * 0.4));
    nodes.append(QPointF(size * 0.3, size * 0.75));
    nodes.append(QPointF(size * 0.7, size * 0.75));

    QPen pen(connection_color,
             std::max(2, int(size * 0.03)));
    painter.setPen(pen);
    QList<QPair<int, int>> connections = {
        {0, 1}, {0, 2}, {1, 3}, {2, 4},
        {1, 2}, {3, 4}, {3, 0}, {4, 0}};
    for (const auto &conn : connections)
    {
        painter.drawLine(nodes[conn.first],
                         nodes[conn.second]);
    }

    int radius = std::max(4, int(size * 0.05));
    painter.setBrush(QBrush(node_color));
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    for (const QPointF &pt : nodes)
    {
        painter.drawEllipse(pt, radius, radius);
    }

    pen = QPen(checkmark_color,
               std::max(3, int(size * 0.04)));
    painter.setPen(pen);
    QPoint start_point(int(size * 0.55), int(size * 0.75));
    QPoint mid_point(int(size * 0.65), int(size * 0.85));
    QPoint end_point(int(size * 0.85), int(size * 0.65));
    painter.drawLine(start_point, mid_point);
    painter.drawLine(mid_point, end_point);

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Move Network Icon
//------------------------------------------------------------------------------
QPixmap createMoveNetworkIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createMoveNetworkIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor node_color("#3498DB");
    QColor connection_color("#2ECC71");
    QColor arrow_color("#E74C3C");

    // Define network nodes
    QVector<QPointF> nodes;
    nodes.append(QPointF(size * 0.5, size * 0.2));
    nodes.append(QPointF(size * 0.2, size * 0.4));
    nodes.append(QPointF(size * 0.8, size * 0.4));
    nodes.append(QPointF(size * 0.3, size * 0.75));
    nodes.append(QPointF(size * 0.7, size * 0.75));

    // Draw connections between nodes
    QPen pen(connection_color,
             std::max(2, int(size * 0.03)));
    painter.setPen(pen);
    QList<QPair<int, int>> connections = {
        {0, 1}, {0, 2}, {1, 3}, {2, 4},
        {1, 2}, {3, 4}, {3, 0}, {4, 0}};
    for (const auto &conn : connections)
    {
        painter.drawLine(nodes[conn.first],
                         nodes[conn.second]);
    }

    // Draw nodes
    int radius = std::max(4, int(size * 0.05));
    painter.setBrush(QBrush(node_color));
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    for (const QPointF &pt : nodes)
    {
        painter.drawEllipse(pt, radius, radius);
    }

    // Draw arrow to indicate movement
    pen = QPen(arrow_color, std::max(3, int(size * 0.04)));
    painter.setPen(pen);

    // Arrow body
    QPointF arrow_start(size * 0.25, size * 0.65);
    QPointF arrow_end(size * 0.75, size * 0.65);
    painter.drawLine(arrow_start, arrow_end);

    // Arrow head
    QPolygonF arrowHead;
    arrowHead << arrow_end
              << QPointF(arrow_end.x() - size * 0.1,
                         arrow_end.y() - size * 0.05)
              << QPointF(arrow_end.x() - size * 0.1,
                         arrow_end.y() + size * 0.05);
    painter.setBrush(QBrush(arrow_color));
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(arrowHead);

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Unconnect Terminals Icon
//------------------------------------------------------------------------------
QPixmap createUnconnectTerminalsIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createUnconnectTerminalsIcon() size=" << size;
    QPixmap  pixmap = createConnectTerminalsPixmap(size);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor("#E74C3C"), 3);
    painter.setPen(pen);
    int margin = size / 6;
    painter.drawLine(margin, margin, size - margin,
                     size - margin);
    painter.drawLine(size - margin, margin, margin,
                     size - margin);
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Settings Icon (Gear)
//------------------------------------------------------------------------------
QPixmap createSettingsIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createSettingsIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor gear_color("#546E7A");
    QColor center_color("#B0BEC5");

    painter.setBrush(QBrush(gear_color));
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));

    int          num_teeth    = 6;
    double       outer_radius = size * 0.45;
    double       inner_radius = size * 0.3;
    double       tooth_height = size * 0.15;
    QPoint       center(size / 2, size / 2);
    QPainterPath path;
    for (int i = 0; i < num_teeth; ++i)
    {
        double angle = i * 360.0 / num_teeth * M_PI / 180.0;
        double x1 =
            center.x() + outer_radius * std::cos(angle);
        double y1 =
            center.y() + outer_radius * std::sin(angle);
        double x2 = center.x()
                    + (outer_radius + tooth_height)
                          * std::cos(angle);
        double y2 = center.y()
                    + (outer_radius + tooth_height)
                          * std::sin(angle);
        double x3 =
            center.x()
            + (outer_radius + tooth_height)
                  * std::cos(angle + M_PI / num_teeth);
        double y3 =
            center.y()
            + (outer_radius + tooth_height)
                  * std::sin(angle + M_PI / num_teeth);
        double x4 =
            center.x()
            + outer_radius
                  * std::cos(angle + M_PI / num_teeth);
        double y4 =
            center.y()
            + outer_radius
                  * std::sin(angle + M_PI / num_teeth);
        path.moveTo(x1, y1);
        path.lineTo(x2, y2);
        path.lineTo(x3, y3);
        path.lineTo(x4, y4);
    }
    painter.drawPath(path);

    painter.setBrush(QBrush(center_color));
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.drawEllipse(center, int(inner_radius),
                        int(inner_radius));
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// New Project Icon
//------------------------------------------------------------------------------
QPixmap createNewProjectIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createNewProjectIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(Qt::black, std::max(1, int(size * 0.02)));
    painter.setPen(pen);
    painter.setBrush(QBrush(Qt::white));
    QPainterPath path;
    path.moveTo(size * 0.2, size * 0.1);
    path.lineTo(size * 0.7, size * 0.1);
    path.lineTo(size * 0.8, size * 0.2);
    path.lineTo(size * 0.8, size * 0.9);
    path.lineTo(size * 0.2, size * 0.9);
    path.closeSubpath();
    QPainterPath fold_path;
    fold_path.moveTo(size * 0.7, size * 0.1);
    fold_path.lineTo(size * 0.7, size * 0.2);
    fold_path.lineTo(size * 0.8, size * 0.2);
    painter.drawPath(path);
    painter.drawPath(fold_path);
    painter.setPen(QPen(QColor("#4CAF50"),
                        std::max(1, int(size * 0.06))));
    painter.drawLine(QPointF(size * 0.4, size * 0.5),
                     QPointF(size * 0.6, size * 0.5));
    painter.drawLine(QPointF(size * 0.5, size * 0.4),
                     QPointF(size * 0.5, size * 0.6));
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Open Project Icon
//------------------------------------------------------------------------------
QPixmap createOpenProjectIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createOpenProjectIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(Qt::black, std::max(1, int(size * 0.02)));
    painter.setPen(pen);
    painter.setBrush(QBrush(QColor("#FFA000")));
    QPainterPath folder_path;
    folder_path.moveTo(size * 0.2, size * 0.3);
    folder_path.lineTo(size * 0.4, size * 0.3);
    folder_path.lineTo(size * 0.45, size * 0.4);
    folder_path.lineTo(size * 0.8, size * 0.4);
    folder_path.lineTo(size * 0.75, size * 0.8);
    folder_path.lineTo(size * 0.2, size * 0.8);
    folder_path.closeSubpath();
    painter.drawPath(folder_path);
    painter.setPen(QPen(QColor("#FFFFFF"),
                        std::max(1, int(size * 0.04))));
    painter.drawLine(QPointF(size * 0.4, size * 0.6),
                     QPointF(size * 0.6, size * 0.6));
    painter.drawLine(QPointF(size * 0.5, size * 0.5),
                     QPointF(size * 0.6, size * 0.6));
    painter.drawLine(QPointF(size * 0.5, size * 0.7),
                     QPointF(size * 0.6, size * 0.6));
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Save Project Icon
//------------------------------------------------------------------------------
QPixmap createSaveProjectIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createSaveProjectIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(Qt::black, std::max(1, int(size * 0.02)));
    painter.setPen(pen);
    painter.setBrush(QBrush(QColor("#2196F3")));
    painter.drawRect(int(size * 0.2), int(size * 0.2),
                     int(size * 0.6), int(size * 0.6));
    painter.setBrush(QBrush(QColor("#BDBDBD")));
    painter.drawRect(int(size * 0.3), int(size * 0.2),
                     int(size * 0.4), int(size * 0.1));
    painter.setBrush(QBrush(QColor("#FFFFFF")));
    painter.drawRect(int(size * 0.3), int(size * 0.4),
                     int(size * 0.4), int(size * 0.3));
    painter.setPen(QPen(QColor("#FFFFFF"),
                        std::max(1, int(size * 0.04))));
    painter.drawLine(QPointF(size * 0.5, size * 0.45),
                     QPointF(size * 0.5, size * 0.65));
    painter.drawLine(QPointF(size * 0.4, size * 0.55),
                     QPointF(size * 0.5, size * 0.65));
    painter.drawLine(QPointF(size * 0.6, size * 0.55),
                     QPointF(size * 0.5, size * 0.65));
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Shortest Paths Icon
//------------------------------------------------------------------------------
QPixmap createShortestPathsIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createShortestPathsIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor node_color("#90CAF9");
    QColor path_color("#4CAF50");
    QColor alt_path_color("#BDBDBD");

    QVector<QPointF> nodes;
    nodes.append(QPointF(size * 0.2, size * 0.2));
    nodes.append(QPointF(size * 0.8, size * 0.2));
    nodes.append(QPointF(size * 0.5, size * 0.5));
    nodes.append(QPointF(size * 0.2, size * 0.8));
    nodes.append(QPointF(size * 0.8, size * 0.8));

    QPen pen(alt_path_color, std::max(2, int(size * 0.02)));
    painter.setPen(pen);
    painter.drawLine(nodes[0], nodes[2]);
    painter.drawLine(nodes[2], nodes[4]);
    painter.drawLine(nodes[1], nodes[2]);
    painter.drawLine(nodes[2], nodes[3]);

    pen = QPen(path_color, std::max(3, int(size * 0.04)));
    painter.setPen(pen);
    painter.drawLine(nodes[0], nodes[2]);
    painter.drawLine(nodes[2], nodes[4]);

    int radius = std::max(4, int(size * 0.06));
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.setBrush(QBrush(node_color));
    for (const QPointF &pt : nodes)
    {
        painter.drawEllipse(pt, radius, radius);
    }

    int     arrow_size = int(size * 0.08);
    QPointF arrow_pos(size * 0.65, size * 0.65);
    painter.setBrush(QBrush(path_color));
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.01))));
    QPainterPath arrow;
    arrow.moveTo(arrow_pos.x(), arrow_pos.y());
    arrow.lineTo(arrow_pos.x() - arrow_size,
                 arrow_pos.y() - arrow_size / 2);
    arrow.lineTo(arrow_pos.x() - arrow_size,
                 arrow_pos.y() + arrow_size / 2);
    arrow.closeSubpath();
    painter.drawPath(arrow);

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Verify Simulation Icon
//------------------------------------------------------------------------------
QPixmap createVerifySimulationIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createVerifySimulationIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor sim_color("#FF7043");
    QColor check_color("#4CAF50");
    QColor arrow_color("#2196F3");

    QPen pen(sim_color, std::max(2, int(size * 0.04)));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    QPointF center(size / 2.0, size / 2.0);
    double  radius = size * 0.35;
    for (int i = 0; i < 3; ++i)
    {
        int start_angle = i * 120 * 16;
        int span_angle  = 100 * 16;
        painter.drawArc(
            QRect(int(size * 0.15), int(size * 0.15),
                  int(size * 0.7), int(size * 0.7)),
            start_angle, span_angle);
    }

    painter.setPen(
        QPen(arrow_color, std::max(2, int(size * 0.03))));
    int        arrow_size = int(size * 0.1);
    QList<int> angles     = {30, 150, 270};
    for (int angle : angles)
    {
        double  rad = angle * M_PI / 180.0;
        QPointF arrow_center(
            center.x()
                + (radius + arrow_size) * std::cos(rad),
            center.y()
                + (radius + arrow_size) * std::sin(rad));
        painter.drawLine(
            QPointF(arrow_center.x() - arrow_size / 2,
                    arrow_center.y()),
            QPointF(arrow_center.x() + arrow_size / 2,
                    arrow_center.y()));
        painter.drawLine(
            QPointF(arrow_center.x() + arrow_size / 2,
                    arrow_center.y()),
            QPointF(arrow_center.x(),
                    arrow_center.y() - arrow_size / 3));
        painter.drawLine(
            QPointF(arrow_center.x() + arrow_size / 2,
                    arrow_center.y()),
            QPointF(arrow_center.x(),
                    arrow_center.y() + arrow_size / 3));
    }

    pen = QPen(check_color, std::max(3, int(size * 0.06)));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    QPointF check_start(size * 0.35, size * 0.5);
    QPointF check_middle(size * 0.45, size * 0.6);
    QPointF check_end(size * 0.65, size * 0.4);
    painter.drawLine(check_start, check_middle);
    painter.drawLine(check_middle, check_end);

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Pan Mode Icon
//------------------------------------------------------------------------------
QPixmap createPanModeIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createPanModeIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor mouse_color("#78909C");
    QColor highlight_color("#2196F3");
    QColor key_color("#90A4AE");
    QColor arrow_color("#4CAF50");

    QPen pen(Qt::black, std::max(1, int(size * 0.02)));
    painter.setPen(pen);
    painter.setBrush(QBrush(mouse_color));

    QPainterPath mouse_path;
    mouse_path.moveTo(size * 0.3, size * 0.2);
    mouse_path.lineTo(size * 0.7, size * 0.2);
    mouse_path.arcTo(QRectF(size * 0.3, size * 0.2,
                            size * 0.4, size * 0.6),
                     0, 180);
    mouse_path.closeSubpath();
    painter.drawPath(mouse_path);

    painter.setBrush(QBrush(highlight_color));
    painter.drawRect(int(size * 0.45), int(size * 0.25),
                     int(size * 0.1), int(size * 0.15));

    painter.setBrush(QBrush(key_color));
    QRect key_rect(int(size * 0.15), int(size * 0.7),
                   int(size * 0.25), int(size * 0.25));
    painter.drawRoundedRect(key_rect, size * 0.05,
                            size * 0.05);

    painter.setPen(Qt::black);
    QFont font("Arial", int(size * 0.08));
    painter.setFont(font);
    painter.drawText(key_rect, Qt::AlignCenter, "Ctrl");

    pen = QPen(arrow_color, std::max(2, int(size * 0.03)));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    double arrow_size   = size * 0.15;
    double arrow_offset = size * 0.1;

    // Top arrow
    painter.drawLine(
        QPointF(size * 0.5, size * 0.1),
        QPointF(size * 0.5, size * 0.1 + arrow_size));
    painter.drawLine(QPointF(size * 0.5, size * 0.1),
                     QPointF(size * 0.5 - arrow_offset,
                             size * 0.1 + arrow_offset));
    painter.drawLine(QPointF(size * 0.5, size * 0.1),
                     QPointF(size * 0.5 + arrow_offset,
                             size * 0.1 + arrow_offset));
    // Bottom arrow
    painter.drawLine(
        QPointF(size * 0.5, size * 0.9),
        QPointF(size * 0.5, size * 0.9 - arrow_size));
    painter.drawLine(QPointF(size * 0.5, size * 0.9),
                     QPointF(size * 0.5 - arrow_offset,
                             size * 0.9 - arrow_offset));
    painter.drawLine(QPointF(size * 0.5, size * 0.9),
                     QPointF(size * 0.5 + arrow_offset,
                             size * 0.9 - arrow_offset));
    // Left arrow
    painter.drawLine(
        QPointF(size * 0.1, size * 0.5),
        QPointF(size * 0.1 + arrow_size, size * 0.5));
    painter.drawLine(QPointF(size * 0.1, size * 0.5),
                     QPointF(size * 0.1 + arrow_offset,
                             size * 0.5 - arrow_offset));
    painter.drawLine(QPointF(size * 0.1, size * 0.5),
                     QPointF(size * 0.1 + arrow_offset,
                             size * 0.5 + arrow_offset));
    // Right arrow
    painter.drawLine(
        QPointF(size * 0.9, size * 0.5),
        QPointF(size * 0.9 - arrow_size, size * 0.5));
    painter.drawLine(QPointF(size * 0.9, size * 0.5),
                     QPointF(size * 0.9 - arrow_offset,
                             size * 0.5 - arrow_offset));
    painter.drawLine(QPointF(size * 0.9, size * 0.5),
                     QPointF(size * 0.9 - arrow_offset,
                             size * 0.5 + arrow_offset));

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Show/Hide Terminals Icon
//------------------------------------------------------------------------------
QPixmap createShowHideTerminalsIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createShowHideTerminalsIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor terminal_color("#1976D2");
    QColor eye_color("#4CAF50");
    QColor line_color("#E0E0E0");

    QLinearGradient terminal_gradient(
        size * 0.2, size * 0.2, size * 0.2, size * 0.6);
    terminal_gradient.setColorAt(0, terminal_color);
    terminal_gradient.setColorAt(
        1, terminal_color.darker(120));
    painter.setBrush(terminal_gradient);
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.drawRoundedRect(
        int(size * 0.2), int(size * 0.2), int(size * 0.4),
        int(size * 0.4), 8, 8);

    painter.drawRect(int(size * 0.35), int(size * 0.4),
                     int(size * 0.1), int(size * 0.2));

    int             eye_x      = int(size * 0.65);
    int             eye_y      = int(size * 0.65);
    int             eye_width  = int(size * 0.25);
    int             eye_height = int(size * 0.15);
    QLinearGradient eye_gradient(eye_x, eye_y, eye_x,
                                 eye_y + eye_height);
    eye_gradient.setColorAt(0, eye_color);
    eye_gradient.setColorAt(1, eye_color.darker(120));
    painter.setBrush(eye_gradient);
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.drawEllipse(eye_x, eye_y, eye_width,
                        eye_height);

    QRadialGradient pupil_gradient(eye_x + eye_width * 0.5,
                                   eye_y + eye_height * 0.5,
                                   eye_height * 0.3);
    pupil_gradient.setColorAt(0, QColor("#212121"));
    pupil_gradient.setColorAt(1, QColor("#000000"));
    painter.setBrush(pupil_gradient);
    painter.drawEllipse(int(eye_x + eye_width * 0.35),
                        int(eye_y + eye_height * 0.2),
                        int(eye_width * 0.3),
                        int(eye_height * 0.6));

    painter.setPen(QPen(QColor("#FFFFFF"),
                        std::max(1, int(size * 0.02))));
    painter.drawArc(
        eye_x + int(eye_width * 0.1),
        eye_y + int(eye_height * 0.2), int(eye_width * 0.2),
        int(eye_height * 0.3), 30 * 16, 120 * 16);

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Show/Hide Connections Icon
//------------------------------------------------------------------------------
QPixmap createShowHideConnectionsIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createShowHideConnectionsIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor terminal_color("#1976D2");
    QColor connection_color("#4CAF50");
    QColor eye_color("#FF7043");

    QLinearGradient terminal_gradient;
    terminal_gradient.setColorAt(0, terminal_color);
    terminal_gradient.setColorAt(
        1, terminal_color.darker(120));

    QVector<QPointF> terminal_positions;
    terminal_positions.append(
        QPointF(size * 0.2, size * 0.2));
    terminal_positions.append(
        QPointF(size * 0.6, size * 0.2));
    terminal_positions.append(
        QPointF(size * 0.4, size * 0.5));

    painter.setPen(QPen(connection_color,
                        std::max(2, int(size * 0.03))));
    painter.drawLine(
        int(terminal_positions[0].x() + size * 0.1),
        int(terminal_positions[0].y() + size * 0.1),
        int(terminal_positions[1].x() + size * 0.1),
        int(terminal_positions[1].y() + size * 0.1));
    painter.drawLine(
        int(terminal_positions[0].x() + size * 0.1),
        int(terminal_positions[0].y() + size * 0.1),
        int(terminal_positions[2].x() + size * 0.1),
        int(terminal_positions[2].y() + size * 0.1));
    painter.drawLine(
        int(terminal_positions[1].x() + size * 0.1),
        int(terminal_positions[1].y() + size * 0.1),
        int(terminal_positions[2].x() + size * 0.1),
        int(terminal_positions[2].y() + size * 0.1));

    int terminal_size = size * 0.2;
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    for (const QPointF &pt : terminal_positions)
    {
        terminal_gradient.setStart(pt.x(), pt.y());
        terminal_gradient.setFinalStop(
            pt.x(), pt.y() + terminal_size);
        painter.setBrush(terminal_gradient);
        painter.drawRoundedRect(int(pt.x()), int(pt.y()),
                                terminal_size,
                                terminal_size, 8, 8);
        painter.setBrush(QColor("#FFFFFF"));
        painter.drawRect(int(pt.x() + terminal_size * 0.25),
                         int(pt.y() + terminal_size * 0.25),
                         int(terminal_size * 0.5),
                         int(terminal_size * 0.2));
    }

    int             eye_x      = int(size * 0.65);
    int             eye_y      = int(size * 0.65);
    int             eye_width  = int(size * 0.25);
    int             eye_height = int(size * 0.15);
    QLinearGradient eye_gradient(eye_x, eye_y, eye_x,
                                 eye_y + eye_height);
    eye_gradient.setColorAt(0, eye_color);
    eye_gradient.setColorAt(1, eye_color.darker(120));
    painter.setBrush(eye_gradient);
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.drawEllipse(eye_x, eye_y, eye_width,
                        eye_height);
    QRadialGradient pupil_gradient(eye_x + eye_width * 0.5,
                                   eye_y + eye_height * 0.5,
                                   eye_height * 0.3);
    pupil_gradient.setColorAt(0, QColor("#212121"));
    pupil_gradient.setColorAt(1, QColor("#000000"));
    painter.setBrush(pupil_gradient);
    painter.drawEllipse(int(eye_x + eye_width * 0.35),
                        int(eye_y + eye_height * 0.2),
                        int(eye_width * 0.3),
                        int(eye_height * 0.6));
    painter.setPen(QPen(QColor("#FFFFFF"),
                        std::max(1, int(size * 0.02))));
    painter.drawArc(
        eye_x + int(eye_width * 0.1),
        eye_y + int(eye_height * 0.2), int(eye_width * 0.2),
        int(eye_height * 0.3), 30 * 16, 120 * 16);

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Show Eye Icon
//------------------------------------------------------------------------------
QPixmap createShowEyeIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createShowEyeIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor eye_color("#4CAF50");
    int    eye_width  = int(size * 0.8);
    int    eye_height = int(size * 0.4);
    int    eye_x      = int((size - eye_width) / 2);
    int    eye_y      = int((size - eye_height) / 2);

    QLinearGradient eye_gradient(0, eye_y, 0,
                                 eye_y + eye_height);
    eye_gradient.setColorAt(0, eye_color);
    eye_gradient.setColorAt(1, eye_color.darker(120));
    QPainterPath eye_path;
    eye_path.moveTo(eye_x, eye_y + eye_height / 2);
    eye_path.quadTo(eye_x + eye_width / 2, eye_y,
                    eye_x + eye_width,
                    eye_y + eye_height / 2);
    eye_path.quadTo(eye_x + eye_width / 2,
                    eye_y + eye_height, eye_x,
                    eye_y + eye_height / 2);
    eye_path.closeSubpath();

    painter.setBrush(eye_gradient);
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.drawPath(eye_path);

    int pupil_size =
        int(std::min(eye_width, eye_height) * 0.5);
    int pupil_x = int(size / 2 - pupil_size / 2);
    int pupil_y = int(size / 2 - pupil_size / 2);
    QRadialGradient pupil_gradient(size / 2.0, size / 2.0,
                                   pupil_size / 2.0);
    pupil_gradient.setColorAt(0, QColor("#212121"));
    pupil_gradient.setColorAt(1, QColor("#000000"));
    painter.setBrush(pupil_gradient);
    painter.drawEllipse(pupil_x, pupil_y, pupil_size,
                        pupil_size);

    painter.setPen(QPen(QColor("#FFFFFF"),
                        std::max(1, int(size * 0.02))));
    painter.drawArc(int(size / 2 - pupil_size / 4),
                    int(size / 2 - pupil_size / 4),
                    int(pupil_size / 2),
                    int(pupil_size / 2), 30 * 16, 120 * 16);

    painter.setBrush(QColor("#FFFFFF"));
    int highlight_size = int(pupil_size * 0.15);
    painter.drawEllipse(int(size / 2 + pupil_size / 6),
                        int(size / 2 - pupil_size / 6),
                        highlight_size, highlight_size);

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Show/Hide Paths Table Icon
//------------------------------------------------------------------------------
QPixmap createShowHidePathsTableIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createShowHidePathsTableIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor table_color("#1976D2");
    QColor path_color("#4CAF50");
    QColor eye_color("#FF7043");

    QLinearGradient table_gradient(0, size * 0.2, 0,
                                   size * 0.5);
    table_gradient.setColorAt(0, table_color);
    table_gradient.setColorAt(1, table_color.darker(110));
    painter.setBrush(table_gradient);
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    QRect table_rect(int(size * 0.15), int(size * 0.2),
                     int(size * 0.5), int(size * 0.3));
    painter.drawRect(table_rect);

    painter.setPen(QPen(QColor("#FFFFFF"),
                        std::max(1, int(size * 0.01))));
    int x1 = table_rect.left() + table_rect.width() / 3;
    int x2 =
        table_rect.left() + (table_rect.width() * 2) / 3;
    painter.drawLine(x1, table_rect.top(), x1,
                     table_rect.bottom());
    painter.drawLine(x2, table_rect.top(), x2,
                     table_rect.bottom());
    int y = table_rect.top() + table_rect.height() / 2;
    painter.drawLine(table_rect.left(), y,
                     table_rect.right(), y);

    QLinearGradient path_gradient(0, 0, size * 0.1,
                                  size * 0.1);
    path_gradient.setColorAt(0, path_color);
    path_gradient.setColorAt(1, path_color.darker(110));
    painter.setBrush(path_gradient);

    double arrow_size = size * 0.06;
    for (int col = 0; col < 3; ++col)
    {
        int x = table_rect.left()
                + (col * table_rect.width() / 3)
                + (table_rect.width() / 6);
        for (int row = 0; row < 2; ++row)
        {
            int y_cell = table_rect.top()
                         + (row * table_rect.height() / 2)
                         + (table_rect.height() / 4);
            QPainterPath path;
            path.moveTo(x - arrow_size, y_cell);
            path.lineTo(x + arrow_size, y_cell);
            path.lineTo(x + arrow_size / 2,
                        y_cell - arrow_size / 2);
            path.moveTo(x + arrow_size, y_cell);
            path.lineTo(x + arrow_size / 2,
                        y_cell + arrow_size / 2);
            painter.setPen(QPen(
                path_color, std::max(1, int(size * 0.02))));
            painter.drawPath(path);
        }
    }

    int             eye_x      = int(size * 0.65);
    int             eye_y      = int(size * 0.65);
    int             eye_width  = int(size * 0.25);
    int             eye_height = int(size * 0.15);
    QLinearGradient eye_gradient(eye_x, eye_y, eye_x,
                                 eye_y + eye_height);
    eye_gradient.setColorAt(0, eye_color);
    eye_gradient.setColorAt(1, eye_color.darker(120));
    painter.setBrush(eye_gradient);
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.drawEllipse(eye_x, eye_y, eye_width,
                        eye_height);
    QRadialGradient pupil_gradient(eye_x + eye_width * 0.5,
                                   eye_y + eye_height * 0.5,
                                   eye_height * 0.3);
    pupil_gradient.setColorAt(0, QColor("#212121"));
    pupil_gradient.setColorAt(1, QColor("#000000"));
    painter.setBrush(pupil_gradient);
    painter.drawEllipse(int(eye_x + eye_width * 0.35),
                        int(eye_y + eye_height * 0.2),
                        int(eye_width * 0.3),
                        int(eye_height * 0.6));
    painter.setPen(QPen(QColor("#FFFFFF"),
                        std::max(1, int(size * 0.02))));
    painter.drawArc(
        eye_x + int(eye_width * 0.1),
        eye_y + int(eye_height * 0.2), int(eye_width * 0.2),
        int(eye_height * 0.3), 30 * 16, 120 * 16);

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Thick White Arrow Pixmap
//------------------------------------------------------------------------------
QPixmap createThickWhiteArrowPixmap(int size, int width)
{
    qCDebug(lcGuiUtil) << "IconFactory::createThickWhiteArrowPixmap() size=" << size;
    QPixmap pixmap(width, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    int    arrow_height     = size;
    double stem_height      = size * 0.5;
    int    x_start          = int(size * 0.1);
    int    y_center         = int(size * 0.4);
    int    arrow_head_width = int(size * 1.5);

    QPainterPath path;
    path.addRect(x_start, y_center - stem_height / 2,
                 width - x_start - arrow_head_width,
                 stem_height);
    path.moveTo(width - arrow_head_width,
                y_center - arrow_height / 2);
    path.lineTo(width - x_start, y_center);
    path.lineTo(width - arrow_head_width,
                y_center + arrow_height / 2);
    path.closeSubpath();

    painter.setBrush(QBrush(Qt::white));
    painter.setPen(Qt::NoPen);
    painter.drawPath(path);
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Thick White Line Pixmap
//------------------------------------------------------------------------------
QPixmap createThickWhiteLinePixmap(int size, int width)
{
    qCDebug(lcGuiUtil) << "IconFactory::createThickWhiteLinePixmap() size=" << size;
    QPixmap pixmap(width, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    double line_height = size * 0.2;
    int    x_start     = int(size * 0.1);
    int    y_center    = int(size * 0.5);
    painter.setBrush(QBrush(Qt::white));
    painter.setPen(Qt::NoPen);
    painter.drawRect(x_start, y_center - line_height / 2,
                     width - x_start * 2, line_height);
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Import Trains Icon
//------------------------------------------------------------------------------
QPixmap createImportTrainsIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createImportTrainsIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient file_gradient(0, 0, 0, size);
    file_gradient.setColorAt(0, QColor("#90CAF9"));
    file_gradient.setColorAt(1, QColor("#1976D2"));

    QPainterPath file_path;
    file_path.moveTo(size * 0.25, size * 0.15);
    file_path.lineTo(size * 0.65, size * 0.15);
    file_path.lineTo(size * 0.75, size * 0.25);
    file_path.lineTo(size * 0.75, size * 0.85);
    file_path.lineTo(size * 0.25, size * 0.85);
    file_path.closeSubpath();

    painter.setBrush(file_gradient);
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.drawPath(file_path);

    painter.drawLine(int(size * 0.65), int(size * 0.15),
                     int(size * 0.65), int(size * 0.25));
    painter.drawLine(int(size * 0.65), int(size * 0.25),
                     int(size * 0.75), int(size * 0.25));

    painter.setBrush(QBrush(QColor("#424242")));
    painter.drawRoundedRect(
        int(size * 0.35), int(size * 0.4), int(size * 0.3),
        int(size * 0.2), 4, 4);

    QColor arrow_color("#4CAF50");
    painter.setBrush(QBrush(arrow_color));
    QPainterPath arrow_path;
    arrow_path.moveTo(size * 0.85, size * 0.5);
    arrow_path.lineTo(size * 0.95, size * 0.6);
    arrow_path.lineTo(size * 0.75, size * 0.6);
    arrow_path.closeSubpath();
    painter.drawPath(arrow_path);
    painter.drawRect(int(size * 0.83), int(size * 0.35),
                     int(size * 0.04), int(size * 0.25));
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Delete Train Icon
//------------------------------------------------------------------------------
QPixmap createDeleteTrainIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createDeleteTrainIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient train_gradient(0, 0, 0, size);
    train_gradient.setColorAt(0, QColor("#78909C"));
    train_gradient.setColorAt(1, QColor("#455A64"));
    painter.setBrush(train_gradient);
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.drawRoundedRect(
        int(size * 0.2), int(size * 0.3), int(size * 0.6),
        int(size * 0.4), 8, 8);

    painter.setPen(QPen(QColor("#F44336"),
                        std::max(3, int(size * 0.06))));
    painter.drawLine(int(size * 0.65), int(size * 0.25),
                     int(size * 0.85), int(size * 0.45));
    painter.drawLine(int(size * 0.65), int(size * 0.45),
                     int(size * 0.85), int(size * 0.25));

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Import Ships Icon
//------------------------------------------------------------------------------
QPixmap createImportShipsIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createImportShipsIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient file_gradient(0, 0, 0, size);
    file_gradient.setColorAt(0, QColor("#90CAF9"));
    file_gradient.setColorAt(1, QColor("#1976D2"));

    QPainterPath file_path;
    file_path.moveTo(size * 0.25, size * 0.15);
    file_path.lineTo(size * 0.65, size * 0.15);
    file_path.lineTo(size * 0.75, size * 0.25);
    file_path.lineTo(size * 0.75, size * 0.85);
    file_path.lineTo(size * 0.25, size * 0.85);
    file_path.closeSubpath();

    painter.setBrush(file_gradient);
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.drawPath(file_path);

    painter.drawLine(int(size * 0.65), int(size * 0.15),
                     int(size * 0.65), int(size * 0.25));
    painter.drawLine(int(size * 0.65), int(size * 0.25),
                     int(size * 0.75), int(size * 0.25));

    QColor ship_color("#1565C0");
    painter.setBrush(QBrush(ship_color));
    QPainterPath ship_path;
    ship_path.moveTo(size * 0.35, size * 0.5);
    ship_path.lineTo(size * 0.65, size * 0.5);
    ship_path.lineTo(size * 0.6, size * 0.6);
    ship_path.lineTo(size * 0.4, size * 0.6);
    ship_path.closeSubpath();
    painter.drawPath(ship_path);

    QColor arrow_color("#4CAF50");
    painter.setBrush(QBrush(arrow_color));
    QPainterPath arrow_path;
    arrow_path.moveTo(size * 0.85, size * 0.5);
    arrow_path.lineTo(size * 0.95, size * 0.6);
    arrow_path.lineTo(size * 0.75, size * 0.6);
    arrow_path.closeSubpath();
    painter.drawPath(arrow_path);
    painter.drawRect(int(size * 0.83), int(size * 0.35),
                     int(size * 0.04), int(size * 0.25));
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Delete Ship Icon
//------------------------------------------------------------------------------
QPixmap createDeleteShipIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createDeleteShipIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QLinearGradient ship_gradient(0, 0, 0, size);
    ship_gradient.setColorAt(0, QColor("#1E88E5"));
    ship_gradient.setColorAt(1, QColor("#0D47A1"));
    painter.setBrush(ship_gradient);
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    QPainterPath ship_path;
    ship_path.moveTo(size * 0.2, size * 0.4);
    ship_path.lineTo(size * 0.8, size * 0.4);
    ship_path.lineTo(size * 0.7, size * 0.6);
    ship_path.lineTo(size * 0.3, size * 0.6);
    ship_path.closeSubpath();
    painter.drawPath(ship_path);

    painter.setPen(QPen(QColor("#F44336"),
                        std::max(3, int(size * 0.06))));
    painter.drawLine(int(size * 0.65), int(size * 0.25),
                     int(size * 0.85), int(size * 0.45));
    painter.drawLine(int(size * 0.65), int(size * 0.45),
                     int(size * 0.85), int(size * 0.25));

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Train Manager Icon
//------------------------------------------------------------------------------
QPixmap createTrainManagerIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createTrainManagerIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor train_color("#1976D2");
    QColor gear_color("#78909C");
    QColor rail_color("#BDBDBD");

    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.setBrush(QBrush(gear_color));
    painter.drawEllipse(int(size * 0.1), int(size * 0.1),
                        int(size * 0.8), int(size * 0.8));

    QLinearGradient train_gradient(0, size * 0.3, 0,
                                   size * 0.7);
    train_gradient.setColorAt(0, train_color);
    train_gradient.setColorAt(1, train_color.darker(120));
    painter.setBrush(train_gradient);
    painter.drawRoundedRect(
        int(size * 0.25), int(size * 0.35), int(size * 0.5),
        int(size * 0.25), 8, 8);

    painter.setBrush(QBrush(QColor("#424242")));
    for (double x : {0.3, 0.5, 0.7})
    {
        painter.drawEllipse(int(size * x), int(size * 0.55),
                            int(size * 0.1),
                            int(size * 0.1));
    }

    painter.setPen(
        QPen(rail_color, std::max(2, int(size * 0.03))));
    painter.drawLine(int(size * 0.2), int(size * 0.65),
                     int(size * 0.8), int(size * 0.65));
    painter.drawLine(int(size * 0.2), int(size * 0.7),
                     int(size * 0.8), int(size * 0.7));

    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.setBrush(QBrush(gear_color));
    for (int i = 0; i < 8; ++i)
    {
        double angle = i * 45 * M_PI / 180.0;
        double x =
            size * 0.5 + size * 0.45 * std::cos(angle);
        double y =
            size * 0.5 + size * 0.45 * std::sin(angle);
        painter.drawEllipse(
            int(x - size * 0.08), int(y - size * 0.08),
            int(size * 0.16), int(size * 0.16));
    }

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Ship Manager Icon
//------------------------------------------------------------------------------
QPixmap createShipManagerIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createShipManagerIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor ship_color("#0D47A1");
    QColor gear_color("#78909C");
    QColor water_color("#BBDEFB");

    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.setBrush(QBrush(gear_color));
    painter.drawEllipse(int(size * 0.1), int(size * 0.1),
                        int(size * 0.8), int(size * 0.8));

    QLinearGradient water_gradient(0, size * 0.6, 0,
                                   size * 0.8);
    water_gradient.setColorAt(0, water_color);
    water_gradient.setColorAt(1, water_color.darker(110));
    painter.setBrush(water_gradient);
    painter.drawRect(int(size * 0.2), int(size * 0.6),
                     int(size * 0.6), int(size * 0.2));

    QLinearGradient ship_gradient(0, size * 0.3, 0,
                                  size * 0.6);
    ship_gradient.setColorAt(0, ship_color);
    ship_gradient.setColorAt(1, ship_color.darker(120));
    painter.setBrush(ship_gradient);
    QPainterPath ship_path;
    ship_path.moveTo(size * 0.3, size * 0.45);
    ship_path.lineTo(size * 0.7, size * 0.45);
    ship_path.lineTo(size * 0.65, size * 0.6);
    ship_path.lineTo(size * 0.35, size * 0.6);
    ship_path.closeSubpath();
    painter.drawPath(ship_path);

    painter.drawRect(int(size * 0.45), int(size * 0.35),
                     int(size * 0.1), int(size * 0.1));

    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.setBrush(QBrush(gear_color));
    for (int i = 0; i < 8; ++i)
    {
        double angle = i * 45 * M_PI / 180.0;
        double x =
            size * 0.5 + size * 0.45 * std::cos(angle);
        double y =
            size * 0.5 + size * 0.45 * std::sin(angle);
        painter.drawEllipse(
            int(x - size * 0.08), int(y - size * 0.08),
            int(size * 0.16), int(size * 0.16));
    }

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Set Global Position Icon
//------------------------------------------------------------------------------
QPixmap createSetGlobalPositionIcon()
{
    qCDebug(lcGuiUtil) << "IconFactory::createSetGlobalPositionIcon()";
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(40, 110, 180), 2));
    painter.setBrush(QBrush(QColor(140, 200, 255, 200)));
    painter.drawEllipse(4, 4, 24, 24);
    painter.setPen(QPen(QColor(60, 60, 60), 1));
    painter.drawLine(4, 16, 28, 16);
    painter.drawLine(16, 4, 16, 28);
    painter.setPen(QPen(QColor(200, 30, 30), 2));
    painter.setBrush(QBrush(QColor(255, 80, 80)));
    painter.drawEllipse(13, 13, 6, 6);
    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Set Global Position Icon
//------------------------------------------------------------------------------
QPixmap createTransportationModePixmap(const QString &mode,
                                       int size, int width)
{
    qCDebug(lcGuiUtil) << "IconFactory::createTransportationModePixmap() mode=" << mode;
    // Create a pixmap for the transportation mode with text
    // and arrow
    QPixmap pixmap(width, 40);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Set color based on transportation mode
    QColor modeColor = Qt::black;
    if (mode.contains("Truck", Qt::CaseInsensitive))
    {
        modeColor =
            QColor(Qt::magenta); // Magenta for truck
    }
    else if (mode.contains("Rail", Qt::CaseInsensitive)
             || mode.contains("Train", Qt::CaseInsensitive))
    {
        modeColor =
            QColor(Qt::darkGray); // Dark gray for rail
    }
    else if (mode.contains("Ship", Qt::CaseInsensitive))
    {
        modeColor = QColor(Qt::blue); // Blue for ship
    }

    // Draw the text
    painter.setPen(modeColor);
    QFont font = painter.font();
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRect(0, 0, pixmap.width(), 15),
                     Qt::AlignCenter, mode);

    // Draw the arrow
    QPen pen(modeColor, 2);
    painter.setPen(pen);

    // Arrow shaft
    painter.drawLine(10, 25, width - 10, 25);

    // Arrow head
    QPolygon arrowHead;
    arrowHead << QPoint(width - 16, 20)
              << QPoint(width - 10, 25)
              << QPoint(width - 16, 30);
    painter.setBrush(modeColor);
    painter.drawPolygon(arrowHead);

    return pixmap;
}

//------------------------------------------------------------------------------
// Calculator Icon
//------------------------------------------------------------------------------
QPixmap createCalculatorIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createCalculatorIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor calc_body_color("#546E7A"); // Slate gray
    QColor screen_color("#E0F7FA");    // Light cyan
    QColor button_color("#B0BEC5");    // Light blue gray
    QColor function_button_color(
        "#FF9800"); // Orange for function buttons

    // Draw calculator body with a gradient
    QLinearGradient body_gradient(0, 0, 0, size);
    body_gradient.setColorAt(0, calc_body_color);
    body_gradient.setColorAt(1,
                             calc_body_color.darker(120));

    painter.setBrush(body_gradient);
    painter.setPen(
        QPen(Qt::black, std::max(1, int(size * 0.02))));
    painter.drawRoundedRect(
        int(size * 0.15), int(size * 0.1), int(size * 0.7),
        int(size * 0.8), int(size * 0.05),
        int(size * 0.05));

    // Draw calculator screen
    QLinearGradient screen_gradient(0, 0, 0, size * 0.2);
    screen_gradient.setColorAt(0, screen_color.darker(105));
    screen_gradient.setColorAt(1, screen_color);

    painter.setBrush(screen_gradient);
    painter.drawRoundedRect(
        int(size * 0.25), int(size * 0.15), int(size * 0.5),
        int(size * 0.15), int(size * 0.02),
        int(size * 0.02));

    // Draw some sample text/digits on screen
    painter.setPen(QColor("#263238"));
    QFont font("Monospace", int(size * 0.08));
    painter.setFont(font);
    painter.drawText(
        QRect(int(size * 0.27), int(size * 0.17),
              int(size * 0.46), int(size * 0.12)),
        Qt::AlignRight | Qt::AlignVCenter, "123");

    // Draw calculator buttons (4x4 grid)
    const int    rows          = 4;
    const int    cols          = 4;
    const double button_size   = size * 0.12;
    const double button_margin = size * 0.04;
    const double start_x       = size * 0.25;
    const double start_y       = size * 0.35;

    QStringList button_labels = {
        "7", "8", "9", "+", "4", "5", "6", "-",
        "1", "2", "3", "×", "0", ".", "=", "÷"};

    int label_index = 0;
    for (int row = 0; row < rows; row++)
    {
        for (int col = 0; col < cols; col++)
        {
            double x =
                start_x
                + col * (button_size + button_margin);
            double y =
                start_y
                + row * (button_size + button_margin);

            // Use function button color for operators
            if (col == 3 || (row == 3 && col == 2))
            {
                painter.setBrush(function_button_color);
            }
            else
            {
                painter.setBrush(button_color);
            }

            painter.drawRoundedRect(
                int(x), int(y), int(button_size),
                int(button_size), int(size * 0.02),
                int(size * 0.02));

            // Draw button labels
            painter.setPen(Qt::black);
            QFont button_font("Arial", int(size * 0.06),
                              QFont::Bold);
            painter.setFont(button_font);
            painter.drawText(
                QRectF(x, y, button_size, button_size),
                Qt::AlignCenter,
                button_labels[label_index++]);
        }
    }

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Filter Connections Icon
//------------------------------------------------------------------------------
QPixmap createFilterConnectionsIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createFilterConnectionsIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Base connection line and terminals
    QPen connectionPen(QColor("#2E86C1"), 2);
    painter.setPen(connectionPen);
    int    radius = 4;
    QPoint left_center(8, size / 2);
    QPoint right_center(size - 8, size / 2);

    // Draw terminals
    painter.drawEllipse(left_center, radius, radius);
    painter.drawEllipse(right_center, radius, radius);

    // Draw dashed connection line to represent filtering
    QPen dashedPen(QColor("#2E86C1"), 2, Qt::DashLine);
    painter.setPen(dashedPen);
    painter.drawLine(left_center, right_center);

    // Draw filter symbol (funnel shape)
    QPen filterPen(QColor("#E67E22"),
                   2); // Orange color for filter
    painter.setPen(filterPen);
    painter.setBrush(QBrush(QColor(
        230, 126, 34, 80))); // Semi-transparent orange

    QPolygon filterShape;
    int      centerX      = size / 2;
    int      topWidth     = size / 3;
    int      bottomWidth  = size / 6;
    int      filterHeight = size / 3;

    filterShape << QPoint(centerX - topWidth / 2, size / 3)
                << QPoint(centerX + topWidth / 2, size / 3)
                << QPoint(centerX + bottomWidth / 2,
                          size / 3 + filterHeight)
                << QPoint(centerX - bottomWidth / 2,
                          size / 3 + filterHeight);

    painter.drawPolygon(filterShape);

    // Draw small filter indicator at bottom of funnel
    int indicatorSize = 3;
    painter.setBrush(QBrush(QColor("#E67E22")));
    painter.drawEllipse(centerX - indicatorSize / 2,
                        size / 3 + filterHeight
                            + indicatorSize,
                        indicatorSize, indicatorSize);

    painter.end();
    return pixmap;
}

//------------------------------------------------------------------------------
// Path Status Icons
//------------------------------------------------------------------------------
QPixmap createStatusReadyIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createStatusReadyIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF bounds(1.5, 1.5, size - 3.0, size - 3.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#1F7A4D"));
    painter.drawEllipse(bounds);

    QPen checkPen(Qt::white, std::max(2, size / 7),
                  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(checkPen);
    QPainterPath check;
    check.moveTo(size * 0.28, size * 0.54);
    check.lineTo(size * 0.45, size * 0.70);
    check.lineTo(size * 0.74, size * 0.34);
    painter.drawPath(check);

    painter.end();
    return pixmap;
}

QPixmap createStatusWarningIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createStatusWarningIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QPolygonF triangle;
    triangle << QPointF(size * 0.50, 1.5)
             << QPointF(size - 1.5, size - 2.0)
             << QPointF(1.5, size - 2.0);

    painter.setPen(QPen(QColor("#A35F00"),
                        std::max(1.0, size / 14.0)));
    painter.setBrush(QColor("#F4B740"));
    painter.drawPolygon(triangle);

    QPen markPen(QColor("#4A3200"), std::max(2, size / 7),
                 Qt::SolidLine, Qt::RoundCap);
    painter.setPen(markPen);
    painter.drawLine(QPointF(size * 0.50, size * 0.30),
                     QPointF(size * 0.50, size * 0.60));
    painter.drawPoint(QPointF(size * 0.50, size * 0.77));

    painter.end();
    return pixmap;
}

QPixmap createStatusUnavailableIcon(int size)
{
    qCDebug(lcGuiUtil) << "IconFactory::createStatusUnavailableIcon() size=" << size;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF bounds(1.5, 1.5, size - 3.0, size - 3.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#B73A54"));
    painter.drawEllipse(bounds);

    QPen slashPen(Qt::white, std::max(2, size / 7),
                  Qt::SolidLine, Qt::RoundCap);
    painter.setPen(slashPen);
    painter.drawLine(QPointF(size * 0.30, size * 0.30),
                     QPointF(size * 0.70, size * 0.70));
    painter.drawLine(QPointF(size * 0.70, size * 0.30),
                     QPointF(size * 0.30, size * 0.70));

    painter.end();
    return pixmap;
}

} // namespace IconFactory
} // namespace GUI
} // namespace CargoNetSim
