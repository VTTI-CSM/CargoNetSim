# GUI/Scenario — thin bridge between GUI items and Backend/Scenario

- Factories produce QGraphicsObject views from scenario data.
- ScenarioMutator is the ONLY write path from GUI to ScenarioDocument.
- SceneRepopulator rebuilds the scene when documentReset fires.
- ItemEventBinder holds the signal-wiring lambdas shared across factories.

All classes in this module are stateless (static methods). They are
constructed on demand at the call site; no singletons, no globals.
