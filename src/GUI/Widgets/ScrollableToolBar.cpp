/**
 * @file ScrollableToolBar.cpp
 * @author Ahmed Aredah
 * @brief Implementation of the ScrollableToolBar class
 * @details This file contains the implementation of the
 * ScrollableToolBar class methods.
 */

#include "ScrollableToolBar.h"
#include "Backend/Commons/LogCategories.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSpinBox>
#include <QStyle>
#include <QStyleOption>
#include <QToolButton>

namespace CargoNetSim
{
namespace GUI
{

ScrollableToolBar::ScrollableToolBar(const QString &title,
                                     QWidget       *parent)
    : QToolBar(title, parent)
{
    qCDebug(lcGuiButton) << "ScrollableToolBar::ScrollableToolBar: creating" << title;
    setSizePolicy(QSizePolicy::Preferred,
                  QSizePolicy::Fixed);

    // Remove default margins and spacing
    setContentsMargins(0, 0, 0, 0);

    // Create widget to hold the toolbar content
    containerWidget = new QWidget(this);
    containerLayout = new QHBoxLayout(containerWidget);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(1);
    containerWidget->setSizePolicy(QSizePolicy::Preferred,
                                   QSizePolicy::Preferred);

    // Create the ribbon as part of the toolbar
    ribbon_ = new QTabWidget(containerWidget);

    // Apply ribbon styles
    ribbon_->setStyleSheet(
        "QGroupBox {"
        "   margin-top: 0px;    /* Remove space above the "
        "GroupBox */"
        "   margin-bottom: 15px; /* Add space below the "
        "GroupBox */"
        "   padding-top: 0px;"
        "   padding-right: 2px;"
        "   padding-bottom: 10px;"
        "   padding-left: 2px;"
        "}"
        "QGroupBox::title {"
        "   subcontrol-origin: margin;"
        "   subcontrol-position: bottom center;"
        "   padding: 0 5px;"
        "   bottom: 7px;"
        "}"
        "QToolButton {"
        "   icon-size: 32px;"
        "}");

    // Add ribbon to container layout
    containerLayout->addWidget(ribbon_);

    // Create scroll area
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(containerWidget);
    scrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        "QScrollArea {"
        "   background-color: transparent;"
        "   border: none;"
        "}"
        "QScrollBar:horizontal {"
        "   height: 10px;"
        "}"
        "QScrollBar::handle:horizontal {"
        "   background: #AAAAAA;"
        "   min-width: 20px;"
        "   border-radius: 5px;"
        "}"
        "QScrollBar::add-line:horizontal, "
        "QScrollBar::sub-line:horizontal {"
        "   width: 0px;"
        "}"
        "QScrollBar::add-page:horizontal, "
        "QScrollBar::sub-page:horizontal {"
        "   background: none;"
        "}");

    // Add scroll area to toolbar
    QToolBar::addWidget(scrollArea);

    // Apply the same style as your standard toolbar
    setStyleSheet("QToolBar {"
                  "   spacing: 1px;"
                  "   border: none;"
                  "   background-color: transparent;"
                  "}"
                  "QToolButton {"
                  "   icon-size: 32px;"
                  "}");

    setMovable(false);
    setFloatable(true);
    setAllowedAreas(Qt::TopToolBarArea);
}

QTabWidget *ScrollableToolBar::getRibbon()
{
    return ribbon_;
}

int ScrollableToolBar::addTab(QWidget       *widget,
                              const QString &label)
{
    qCDebug(lcGuiButton) << "ScrollableToolBar::addTab:" << label;
    return ribbon_->addTab(widget, label);
}

void ScrollableToolBar::setTabVisible(int  index,
                                      bool visible)
{
    qCDebug(lcGuiButton) << "ScrollableToolBar::setTabVisible: index" << index
                         << "visible:" << visible;
    ribbon_->setTabVisible(index, visible);
}

QAction *ScrollableToolBar::addWidget(QWidget *widget)
{
    // If this is the scroll area itself, use the original
    // method
    if (widget == scrollArea)
    {
        return QToolBar::addWidget(widget);
    }

    qCDebug(lcGuiButton) << "ScrollableToolBar::addWidget:"
                         << widget->metaObject()->className();
    // Add widget to container layout
    containerLayout->addWidget(widget);
    return new QAction(this); // Return a dummy action
}

QAction *ScrollableToolBar::addAction(const QString &text)
{
    qCDebug(lcGuiButton) << "ScrollableToolBar::addAction:" << text;
    QAction *action = new QAction(text, this);
    containerLayout->addWidget(
        createWidgetForAction(action));
    return action;
}

QAction *ScrollableToolBar::addAction(const QIcon   &icon,
                                      const QString &text)
{
    qCDebug(lcGuiButton) << "ScrollableToolBar::addAction(icon):" << text;
    QAction *action = new QAction(icon, text, this);
    containerLayout->addWidget(
        createWidgetForAction(action));
    return action;
}

QAction *ScrollableToolBar::addSeparator()
{
    qCDebug(lcGuiButton) << "ScrollableToolBar::addSeparator";
    QAction *action = new QAction(this);
    action->setSeparator(true);
    QWidget *sep = createWidgetForAction(action);
    containerLayout->addWidget(sep);
    return action;
}

/**
 * @brief Find all interactive widgets in the toolbar and
 * its containers
 * @return QList<QWidget*> List of all interactive widgets
 * found
 */
QList<QWidget *>
ScrollableToolBar::findAllInteractiveWidgets()
{
    qCDebug(lcGuiButton) << "ScrollableToolBar::findAllInteractiveWidgets";
    QList<QWidget *> allWidgets;

    // Function to check if a widget is interactive and
    // should be included
    auto isInteractiveWidget = [](QWidget *widget) -> bool {
        return (qobject_cast<QToolButton *>(widget)
                || qobject_cast<QComboBox *>(widget)
                || qobject_cast<QLineEdit *>(widget)
                || qobject_cast<QCheckBox *>(widget)
                || qobject_cast<QSpinBox *>(widget)
                || qobject_cast<QDoubleSpinBox *>(widget)
                || qobject_cast<QPushButton *>(widget)
                || qobject_cast<QSlider *>(widget));
    };

    // Process the ribbon
    if (ribbon_)
    {
        for (int i = 0; i < ribbon_->count(); i++)
        {
            QWidget *tabWidget = ribbon_->widget(i);
            if (!tabWidget)
                continue;

            // Get all widgets recursively
            QList<QWidget *> tabWidgets =
                tabWidget->findChildren<QWidget *>();

            // Add interactive widgets to our list
            for (QWidget *widget : tabWidgets)
            {
                if (isInteractiveWidget(widget)
                    && !allWidgets.contains(widget))
                {
                    allWidgets.append(widget);
                }
            }
        }
    }

    // Process the container widget
    if (containerWidget)
    {
        // Get all widgets recursively
        QList<QWidget *> containerWidgets =
            containerWidget->findChildren<QWidget *>();

        // Add interactive widgets to our list
        for (QWidget *widget : containerWidgets)
        {
            if (isInteractiveWidget(widget)
                && !allWidgets.contains(widget))
            {
                allWidgets.append(widget);
            }
        }
    }

    // Process direct children
    QList<QWidget *> directWidgets =
        findChildren<QWidget *>(QString(),
                                Qt::FindDirectChildrenOnly);
    for (QWidget *widget : directWidgets)
    {
        if (widget != ribbon_ && widget != containerWidget
            && widget != scrollArea)
        {
            if (isInteractiveWidget(widget)
                && !allWidgets.contains(widget))
            {
                allWidgets.append(widget);
            }
        }
    }

    qCDebug(lcGuiButton) << "ScrollableToolBar::findAllInteractiveWidgets: found"
                         << allWidgets.size();
    return allWidgets;
}

void ScrollableToolBar::resizeEvent(QResizeEvent *event)
{
    // Don't set fixed width, allow the scroll area to
    // resize naturally
    scrollArea->setMinimumWidth(
        0); // Remove any minimum width constraints
    scrollArea->setMaximumWidth(
        event->size().width()); // Set maximum width to
                                // current width
    scrollArea->setMinimumHeight(
        containerWidget->sizeHint().height() + 10.0);
    QToolBar::resizeEvent(event);
}

QWidget *
ScrollableToolBar::createWidgetForAction(QAction *action)
{
    QToolButton *button = new QToolButton();
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    return button;
}

} // namespace GUI
} // namespace CargoNetSim
