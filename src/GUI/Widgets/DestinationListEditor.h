// src/GUI/Widgets/DestinationListEditor.h
#pragma once

#include <QList>
#include <QWidget>

#include "Backend/Scenario/ScenarioDocument.h"  // for DestinationRoute

class QTableWidget;
class QPushButton;
class QLabel;

namespace CargoNetSim { namespace GUI {
class GraphicsScene;
namespace Input { class InteractionController; }
} }

namespace CargoNetSim {
namespace GUI {

/// A table-based editor for the `destinations: [{terminal, fraction}]`
/// multi-destination form. Emits `changed()` whenever the edited list
/// is self-consistent enough to be written through
/// `ScenarioMutator::setTerminalProperty`.
///
/// Non-owning: does NOT write to any document. The hosting
/// PropertiesPanel reads `routes()` on every `changed()` signal and
/// invokes the mutator itself. This keeps the widget free of
/// document-lifecycle knowledge and lets the same widget be unit-
/// tested without a MainWindow.
///
/// Validation:
///   * live fraction sum is displayed at the bottom of the table
///   * `isValid()` is true iff the list is non-empty, every terminal
///     is a distinct non-empty id, and the sum is within 1e-6 of 1.0
///   * `changed()` fires on every user edit; isValid() may be false
///     transiently while the user is mid-edit — the caller decides
///     whether to write-through on every change or only on valid
class DestinationListEditor : public QWidget
{
    Q_OBJECT
public:
    /// Construct empty. The hosting panel supplies the scene via
    /// setScene() so the widget can enter pick-destination mode.
    explicit DestinationListEditor(QWidget *parent = nullptr);

    /// Wire up to the interaction controller for pick-on-canvas flow.
    void setController(Input::InteractionController *ctrl);

    /// Store the origin terminal id so self-picks are rejected.
    void setOriginTerminalId(const QString &id);

    /// Replace the current list (read-back on every load).
    void setRoutes(
        const QList<Backend::Scenario::DestinationRoute> &routes);

    /// Current list, in row order. Rows with empty terminal id are
    /// filtered out — the widget hides that interstitial state from
    /// the caller.
    QList<Backend::Scenario::DestinationRoute> routes() const;

    /// True iff the list is non-empty, every terminal id is distinct
    /// and non-empty, and the fraction sum is within 1e-6 of 1.0.
    bool isValid() const;

signals:
    void changed();

private slots:
    void addRow();
    void removeCurrentRow();
    void onCellChanged();

private:
    void refreshSumLabel();

    QTableWidget *m_table;
    QPushButton  *m_addButton;
    QPushButton  *m_removeButton;
    QLabel       *m_sumLabel;

    Input::InteractionController *m_controller = nullptr;
    QString        m_originTerminalId;
    int            m_activePickRow = -1;
    QMetaObject::Connection m_pickConnection;
    QStringList    m_rowTerminalIds;
};

} // namespace GUI
} // namespace CargoNetSim
