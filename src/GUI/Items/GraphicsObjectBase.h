#pragma once

#include "GUI/Items/AnimationObject.h"
#include <QColor>
#include <QGraphicsObject>
#include <QGraphicsRectItem>
#include <QPropertyAnimation>

namespace CargoNetSim::GUI
{

class GraphicsObjectBase : public QGraphicsObject
{
    Q_OBJECT
public:
    explicit GraphicsObjectBase(
        QGraphicsItem *parent = nullptr);
    ~GraphicsObjectBase() override;

    /// QObject instance identity. UUID assigned at construction, stable
    /// for the lifetime of the item. Non-virtual: "which object is this?"
    /// has exactly one answer. Callers that want a document-addressable
    /// id should use the domain accessor (e.g. TerminalItem::getTerminalId)
    /// or sceneRegistryKey() — not getID().
    QString getID() const;

    /// Scene-registration contract. Returns the item's domain key when the
    /// item is bound to a model entity; otherwise falls back to `getID()`
    /// (UUID). This is the single rule every `addItemWithId` /
    /// `removeItemWithId` call site should honour — the fallback lives here
    /// so it isn't duplicated at every site.
    QString sceneRegistryKey() const
    {
        const QString d = domainKey();
        return d.isEmpty() ? getID() : d;
    }

    /** Pulsing highlight effect */
    void flash(bool          evenIfHidden = false,
               const QColor &color = QColor(255, 0, 0,
                                            180));

protected:
    /// Hook for derived classes that view a document entity. Return the
    /// model-field identity (e.g., TerminalPlacement::id, RegionSpec::name)
    /// when bound; return an empty QString when unbound. Base returns
    /// empty — classes that don't represent a document entity inherit
    /// "no domain key".
    virtual QString domainKey() const { return QString(); }

    /** Clear any overlay visuals */
    virtual void clearAnimationVisuals();
    /** Create the overlay */
    virtual void createAnimationVisual(const QColor &color);
    void         onAnimationFinished();
    AnimationObject    *m_animObject = nullptr;
    QPropertyAnimation *m_animation  = nullptr;

private:
    QString m_id;
};

} // namespace CargoNetSim::GUI
