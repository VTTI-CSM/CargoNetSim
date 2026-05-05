#pragma once

#include <QLoggingCategory>

// Backend -- Messaging
Q_DECLARE_LOGGING_CATEGORY(lcRabbitMQ)

// Backend -- Client base + per-mode clients
Q_DECLARE_LOGGING_CATEGORY(lcClient)
Q_DECLARE_LOGGING_CATEGORY(lcClientShip)
Q_DECLARE_LOGGING_CATEGORY(lcClientTrain)
Q_DECLARE_LOGGING_CATEGORY(lcClientTruck)
Q_DECLARE_LOGGING_CATEGORY(lcClientTerminal)

// Backend -- Rail network loading (consolidated from
// anonymous-namespace declarations in 3 files)
Q_DECLARE_LOGGING_CATEGORY(lcRail)

// Backend -- Controllers
Q_DECLARE_LOGGING_CATEGORY(lcController)
Q_DECLARE_LOGGING_CATEGORY(lcControllerNetwork)

// Backend -- Configuration
Q_DECLARE_LOGGING_CATEGORY(lcConfig)

// Backend -- Domain models
Q_DECLARE_LOGGING_CATEGORY(lcModel)

// Backend -- Scenario pipeline
Q_DECLARE_LOGGING_CATEGORY(lcScenario)

// Backend -- Threading utilities
Q_DECLARE_LOGGING_CATEGORY(lcThreading)

// Application initialization
Q_DECLARE_LOGGING_CATEGORY(lcInit)

// CLI commands
Q_DECLARE_LOGGING_CATEGORY(lcCli)

// GUI
Q_DECLARE_LOGGING_CATEGORY(lcGui)
Q_DECLARE_LOGGING_CATEGORY(lcGuiButton)
Q_DECLARE_LOGGING_CATEGORY(lcGuiView)
Q_DECLARE_LOGGING_CATEGORY(lcGuiNetwork)
Q_DECLARE_LOGGING_CATEGORY(lcGuiScene)
Q_DECLARE_LOGGING_CATEGORY(lcGuiPathTable)
Q_DECLARE_LOGGING_CATEGORY(lcGuiHeartbeat)
Q_DECLARE_LOGGING_CATEGORY(lcGuiUtil)

// GUI -- Input dispatch pipeline
Q_DECLARE_LOGGING_CATEGORY(lcGuiInput)
Q_DECLARE_LOGGING_CATEGORY(lcGuiInputMode)
Q_DECLARE_LOGGING_CATEGORY(lcGuiInputCmd)
Q_DECLARE_LOGGING_CATEGORY(lcGuiInputItem)
