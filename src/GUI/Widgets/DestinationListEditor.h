// src/GUI/Widgets/DestinationListEditor.h
#pragma once

#include <QList>
#include <QWidget>

#include "Backend/GuiApi/ScenarioDocumentApi.h"  // for DestinationRoute

class QTableWidget;
class QPushButton;
class QLabel;

namespace CargoNetSim { namespace GUI {
class GraphicsScene;
namespace Input { class PickCoordinator; }
} }

namespace CargoNetSim {
namespace GUI {

/// A table-based editor for the `destinations: [{terminal, fraction}]`
/// multi-destination form. Emits `changed()` whenever the edited list
/// is self-consistent enough to be written through
/// `ScenarioEditService::setTerminalProperty`.
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

    /// Wire up to the app-scope PickCoordinator for cross-scene pick flow.
    /// Must be called before the user can click any per-row Pick button.
    void setCoordinator(Input::PickCoordinator *coord);

    /// Store the origin terminal id so self-picks are rejected.
    void setOriginTerminalId(const QString &id);

    /// Called by the hosting PropertiesPanel when the coordinator resolves
    /// a multi-row pick targeted at this editor. Writes the id into the
    /// active row's buffer, updates the button label, and emits changed().
    /// No-op if m_activePickRow == -1.
    void applyPickedTerminalToActiveRow(const QString &id, const QString &name);

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

private slots:
    /// Re-renders per-row Pick button labels when the coordinator's session
    /// starts/ends/resolves. Pure projection of coordinator state — no edits.
    void onSessionChanged();

private:
    void refreshSumLabel();
    void refreshRowButtonLabel(int row);

    QTableWidget *m_table;
    QPushButton  *m_addButton;
    QPushButton  *m_removeButton;
    QLabel       *m_sumLabel;

    Input::PickCoordinator *m_coordinator = nullptr;
    QString        m_originTerminalId;
    int            m_activePickRow = -1;
    QStringList    m_rowTerminalIds;
};

} // namespace GUI
} // namespace CargoNetSim
