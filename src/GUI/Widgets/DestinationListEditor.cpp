// src/GUI/Widgets/DestinationListEditor.cpp
#include "DestinationListEditor.h"
#include "GraphicsScene.h"
#include "../Input/PickCoordinator.h"
#include "../Input/PickSession.h"
#include "Backend/Commons/LogCategories.h"

#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cmath>

namespace CargoNetSim {
namespace GUI {

namespace {
constexpr int kColTerminal = 0;
constexpr int kColFraction = 1;
} // namespace

DestinationListEditor::DestinationListEditor(QWidget *parent)
    : QWidget(parent)
{
    qCDebug(lcGuiUtil) << "DestinationListEditor::DestinationListEditor: creating editor";
    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({ tr("Destination"), tr("Fraction") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // Interactive editing happens through the per-cell widgets
    // (pick button + double spin), not via inline item editing.

    m_addButton    = new QPushButton(tr("Add"), this);
    m_removeButton = new QPushButton(tr("Remove"), this);
    m_sumLabel     = new QLabel(this);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_addButton);
    buttons->addWidget(m_removeButton);
    buttons->addStretch(1);
    buttons->addWidget(m_sumLabel);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_table);
    layout->addLayout(buttons);
    layout->setContentsMargins(0, 0, 0, 0);

    connect(m_addButton, &QPushButton::clicked, this,
            &DestinationListEditor::addRow);
    connect(m_removeButton, &QPushButton::clicked, this,
            &DestinationListEditor::removeCurrentRow);

    refreshSumLabel();
}

void DestinationListEditor::setRoutes(
    const QList<Backend::Scenario::DestinationRoute> &routes)
{
    qCDebug(lcGuiUtil)
        << "DestinationListEditor::setRoutes:"
        << routes.size() << "routes";
    m_table->setRowCount(0);
    m_rowTerminalIds.clear();
    for (const auto &r : routes)
    {
        addRow();
        const int row = m_table->rowCount() - 1;
        m_rowTerminalIds[row] = r.terminal;
        if (auto *btn = qobject_cast<QPushButton *>(
                m_table->cellWidget(row, kColTerminal)))
        {
            btn->setText(QStringLiteral("Picked \u2713"));
            btn->setToolTip(r.terminal);
        }
        if (auto *spin = qobject_cast<QDoubleSpinBox *>(
                m_table->cellWidget(row, kColFraction)))
        {
            spin->setValue(r.fraction);
        }
    }
    refreshSumLabel();
    emit changed();
}

QList<Backend::Scenario::DestinationRoute>
DestinationListEditor::routes() const
{
    QList<Backend::Scenario::DestinationRoute> out;
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        const QString id = m_rowTerminalIds.value(row);
        if (id.isEmpty()) continue;
        auto *spin = qobject_cast<QDoubleSpinBox *>(
            m_table->cellWidget(row, kColFraction));
        if (!spin) continue;
        out.append({ id, spin->value() });
    }
    return out;
}

bool DestinationListEditor::isValid() const
{
    const auto rt = routes();
    if (rt.isEmpty())
    {
        qCWarning(lcGuiUtil) << "DestinationListEditor::isValid: no routes";
        return false;
    }

    QSet<QString> seen;
    double        sum = 0.0;
    for (const auto &r : rt)
    {
        if (r.terminal.isEmpty())
        {
            qCWarning(lcGuiUtil) << "DestinationListEditor::isValid: empty terminal ID";
            return false;
        }
        if (seen.contains(r.terminal))
        {
            qCWarning(lcGuiUtil) << "DestinationListEditor::isValid: duplicate terminal"
                                 << r.terminal;
            return false;
        }
        seen.insert(r.terminal);
        sum += r.fraction;
    }
    bool ok = std::abs(sum - 1.0) <= 1e-6;
    if (!ok)
        qCWarning(lcGuiUtil) << "DestinationListEditor::isValid: fraction sum" << sum
                             << "!= 1.0";
    return ok;
}

void DestinationListEditor::addRow()
{
    const int row = m_table->rowCount();
    qCDebug(lcGuiUtil)
        << "DestinationListEditor::addRow:" << row;
    m_table->insertRow(row);
    m_rowTerminalIds.append(QString());

    auto *pickBtn = new QPushButton("Pick");
    pickBtn->setToolTip(tr("Click terminal on canvas"));
    connect(pickBtn, &QPushButton::clicked, this,
        [this, row]() {
            if (!m_coordinator) {
                qCWarning(lcGuiUtil)
                    << "DestinationListEditor: Pick clicked without coordinator";
                return;
            }
            // Toggle: if this row is already the active pick target, cancel.
            const auto &s = m_coordinator->activeSession();
            if (s && s->requesterId == m_originTerminalId &&
                s->targetsRow(static_cast<std::size_t>(row)))
            {
                m_coordinator->cancel();
                return;
            }
            m_activePickRow = row;
            m_coordinator->begin(Input::PickSession{
                m_originTerminalId,
                m_originTerminalId,
                Input::MultiDestinationRowSlot{ static_cast<std::size_t>(row) }
            });
        });
    m_table->setCellWidget(row, kColTerminal, pickBtn);

    auto *spin = new QDoubleSpinBox();
    spin->setRange(0.0, 1.0);
    spin->setSingleStep(0.05);
    spin->setDecimals(3);
    spin->setValue(0.0);
    connect(spin,
            QOverload<double>::of(
                &QDoubleSpinBox::valueChanged),
            this,
            &DestinationListEditor::onCellChanged);
    m_table->setCellWidget(row, kColFraction, spin);

    refreshSumLabel();
    emit changed();
}

void DestinationListEditor::removeCurrentRow()
{
    const int row = m_table->currentRow();
    if (row < 0) return;
    qCDebug(lcGuiUtil)
        << "DestinationListEditor::removeCurrentRow:"
        << row;
    m_table->removeRow(row);
    if (row < m_rowTerminalIds.size())
        m_rowTerminalIds.removeAt(row);
    refreshSumLabel();
    emit changed();
}

void DestinationListEditor::onCellChanged()
{
    refreshSumLabel();
    emit changed();
}

void DestinationListEditor::refreshSumLabel()
{
    double sum = 0.0;
    for (const auto &r : routes()) sum += r.fraction;
    const bool ok = std::abs(sum - 1.0) <= 1e-6;
    m_sumLabel->setText(
        tr("Sum: %1 %2")
            .arg(QString::number(sum, 'f', 3))
            .arg(ok ? QStringLiteral("✓") : QStringLiteral("(must = 1.000)")));
    QPalette p = m_sumLabel->palette();
    p.setColor(QPalette::WindowText, ok ? QColor("green") : QColor("red"));
    m_sumLabel->setPalette(p);
}

void DestinationListEditor::setCoordinator(Input::PickCoordinator *coord)
{
    if (m_coordinator == coord) return;
    if (m_coordinator) disconnect(m_coordinator, nullptr, this, nullptr);
    m_coordinator = coord;
    if (!m_coordinator) return;
    connect(m_coordinator, &Input::PickCoordinator::sessionChanged,
            this, &DestinationListEditor::onSessionChanged);
}

void DestinationListEditor::applyPickedTerminalToActiveRow(
    const QString &id, const QString &name)
{
    if (m_activePickRow < 0) {
        qCWarning(lcGuiUtil)
            << "DestinationListEditor::applyPickedTerminalToActiveRow:"
            << "no active pick row";
        return;
    }
    if (m_activePickRow < m_rowTerminalIds.size())
        m_rowTerminalIds[m_activePickRow] = id;
    if (auto *btn = qobject_cast<QPushButton *>(
            m_table->cellWidget(m_activePickRow, kColTerminal))) {
        btn->setText(QStringLiteral("Picked ✓"));
        btn->setToolTip(name.isEmpty() ? id : name);
    }
    m_activePickRow = -1;
    refreshSumLabel();
    emit changed();
}

void DestinationListEditor::onSessionChanged()
{
    if (!m_coordinator || !m_coordinator->isActive()) {
        if (m_activePickRow >= 0) {
            const int row = m_activePickRow;
            m_activePickRow = -1;
            refreshRowButtonLabel(row);
        }
        return;
    }
    const auto &s = m_coordinator->activeSession();
    if (s->requesterId != m_originTerminalId) return;
    if (auto *r = std::get_if<Input::MultiDestinationRowSlot>(&s->slot)) {
        m_activePickRow = static_cast<int>(r->rowIndex);
        refreshRowButtonLabel(m_activePickRow);
    }
}

void DestinationListEditor::refreshRowButtonLabel(int row)
{
    if (row < 0 || row >= m_table->rowCount()) return;
    auto *btn = qobject_cast<QPushButton *>(
        m_table->cellWidget(row, kColTerminal));
    if (!btn) return;
    const bool isActive = m_coordinator && m_coordinator->isActive() &&
                          m_coordinator->activeSession()->requesterId == m_originTerminalId &&
                          m_coordinator->activeSession()->targetsRow(static_cast<std::size_t>(row));
    const QString currentId = (row < m_rowTerminalIds.size())
                                  ? m_rowTerminalIds[row] : QString();
    if (isActive) {
        btn->setText(tr("Picking..."));
    } else if (!currentId.isEmpty()) {
        btn->setText(QStringLiteral("Picked ✓"));
    } else {
        btn->setText(tr("Pick"));
    }
}

void DestinationListEditor::setOriginTerminalId(
    const QString &id)
{
    m_originTerminalId = id;
}

} // namespace GUI
} // namespace CargoNetSim
