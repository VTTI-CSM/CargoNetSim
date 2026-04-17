#include "LogCategories.h"

// Backend -- Messaging
Q_LOGGING_CATEGORY(lcRabbitMQ,
                   "cargonetsim.rabbitmq")

// Backend -- Client base + per-mode clients
Q_LOGGING_CATEGORY(lcClient,
                   "cargonetsim.client")
Q_LOGGING_CATEGORY(lcClientShip,
                   "cargonetsim.client.ship")
Q_LOGGING_CATEGORY(lcClientTrain,
                   "cargonetsim.client.train")
Q_LOGGING_CATEGORY(lcClientTruck,
                   "cargonetsim.client.truck")
Q_LOGGING_CATEGORY(lcClientTerminal,
                   "cargonetsim.client.terminal")

// Backend -- Rail network loading
Q_LOGGING_CATEGORY(lcRail,
                   "cargonetsim.rail")

// Backend -- Controllers
Q_LOGGING_CATEGORY(lcController,
                   "cargonetsim.controller")
Q_LOGGING_CATEGORY(lcControllerNetwork,
                   "cargonetsim.controller.network")

// Backend -- Configuration
Q_LOGGING_CATEGORY(lcConfig,
                   "cargonetsim.config")

// Backend -- Domain models
Q_LOGGING_CATEGORY(lcModel,
                   "cargonetsim.model")

// Backend -- Scenario pipeline
Q_LOGGING_CATEGORY(lcScenario,
                   "cargonetsim.scenario")

// Backend -- Threading utilities
Q_LOGGING_CATEGORY(lcThreading,
                   "cargonetsim.threading")

// Application initialization
Q_LOGGING_CATEGORY(lcInit,
                   "cargonetsim.init")

// CLI commands
Q_LOGGING_CATEGORY(lcCli,
                   "cargonetsim.cli")

// GUI
Q_LOGGING_CATEGORY(lcGui,
                   "cargonetsim.gui")
Q_LOGGING_CATEGORY(lcGuiButton,
                   "cargonetsim.gui.button")
Q_LOGGING_CATEGORY(lcGuiView,
                   "cargonetsim.gui.view")
Q_LOGGING_CATEGORY(lcGuiNetwork,
                   "cargonetsim.gui.network")
Q_LOGGING_CATEGORY(lcGuiScene,
                   "cargonetsim.gui.scene")
Q_LOGGING_CATEGORY(lcGuiPathTable,
                   "cargonetsim.gui.pathtable")
Q_LOGGING_CATEGORY(lcGuiHeartbeat,
                   "cargonetsim.gui.heartbeat")
Q_LOGGING_CATEGORY(lcGuiUtil,
                   "cargonetsim.gui.util")
