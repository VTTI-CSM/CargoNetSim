#ifndef INTERFACESELECTIONDIALOG_H
#define INTERFACESELECTIONDIALOG_H
#include <QCheckBox>
#include <QDialog>
#include <QGridLayout>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>

namespace CargoNetSim
{
namespace GUI
{
class InterfaceSelectionDialog : public QDialog
{
    Q_OBJECT
public:
    // Define an enum for dialog types
    enum DialogType
    {
        InterfaceSelection, // For
                            // connectVisibleTerminalsByInterfaces
        NetworkSelection // For
                         // connectVisibleTerminalsByNetworks
    };

    explicit InterfaceSelectionDialog(
        const QSet<QString> &availableOptions,
        const QSet<QString> &visibleTerminalTypes,
        DialogType dialogType = InterfaceSelection,
        QWidget   *parent     = nullptr);
    ~InterfaceSelectionDialog();

    QList<QString>      getSelectedInterfaces() const;
    QList<QString>      getSelectedNetworkTypes() const;
    QMap<QString, bool> getIncludedTerminalTypes() const;

private slots:
    void selectAllInterfaces();
    void deselectAllInterfaces();
    void selectAllNetworkTypes();
    void deselectAllNetworkTypes();
    void selectAllTerminalTypes();
    void deselectAllTerminalTypes();

private:
    DialogType                 m_dialogType;
    QMap<QString, QCheckBox *> m_interfaceCheckboxes;
    QMap<QString, QCheckBox *> m_networkTypeCheckboxes;
    QMap<QString, QCheckBox *> m_terminalTypeCheckboxes;
};
} // namespace GUI
} // namespace CargoNetSim
#endif // INTERFACESELECTIONDIALOG_H
