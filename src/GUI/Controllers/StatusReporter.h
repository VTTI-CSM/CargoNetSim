#pragma once

#include <QString>

namespace CargoNetSim
{
namespace GUI
{

class StatusReporter
{
public:
    virtual ~StatusReporter() = default;
    virtual void showMessage(const QString &msg,
                             int ms = 3000) = 0;
    virtual void showError(const QString &msg,
                           int ms = 5000) = 0;
    virtual void startProgress() = 0;
    virtual void stopProgress() = 0;
    virtual void storeButtons() = 0;
    virtual void disableButtons() = 0;
    virtual void restoreButtons() = 0;
};

} // namespace GUI
} // namespace CargoNetSim
