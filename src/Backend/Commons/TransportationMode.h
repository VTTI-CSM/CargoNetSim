/**
 * @file TransportationTypes.h
 * @brief Defines transportation modes and conversion
 * utilities for the CargoNetSim system.
 * @author Ahmed Aredah
 */

#pragma once
#include <QMetaEnum>
#include <QObject>
#include <QString>
#include <containerLib/container.h>

namespace CargoNetSim
{
namespace Backend
{

/**
 * @class TransportationTypes
 * @brief Class that defines and manages transportation mode
 * types in the cargo network system.
 *
 * This class provides an enumeration of different
 * transportation modes and utility functions to convert
 * between various representations including ContainerCore
 * hauler types, integer values, and string representations.
 */
class TransportationTypes : public QObject
{
    Q_OBJECT
public:
    /**
     * @enum TransportationMode
     * @brief Enumeration of available transportation modes
     * in the cargo network.
     *
     * Defines the different modes of transportation that
     * can be used for moving cargo within the simulation
     * system.
     */
    enum class TransportationMode
    {
        Any   = -1, /**< Any transportation mode */
        Ship  = 0,  /**< Maritime vessel transportation */
        Truck = 1,  /**< Road-based truck transportation */
        Train = 2   /**< Rail-based train transportation */
    };
    Q_ENUM(TransportationMode)

    /**
     * @brief Converts a TransportationMode to the
     * corresponding ContainerCore hauler type.
     * @param mode The TransportationMode to convert.
     * @return The equivalent
     * ContainerCore::Container::HaulerType.
     */
    static ContainerCore::Container::HaulerType
    toContainerHauler(TransportationMode mode);

    /**
     * @brief Converts a ContainerCore hauler type to the
     * corresponding TransportationMode.
     * @param hauler The
     * ContainerCore::Container::HaulerType to convert.
     * @return The equivalent TransportationMode.
     */
    static TransportationMode fromContainerHauler(
        ContainerCore::Container::HaulerType hauler);

    /**
     * @brief Converts a TransportationMode to its integer
     * representation.
     * @param mode The TransportationMode to convert.
     * @return The integer value corresponding to the given
     * TransportationMode.
     */
    static int toInt(TransportationMode mode);

    /**
     * @brief Converts an integer value to the corresponding
     * TransportationMode.
     * @param value The integer value to convert.
     * @return The TransportationMode corresponding to the
     * given integer value.
     */
    static TransportationMode fromInt(int value);

    /**
     * @brief Converts a TransportationMode to a
     * human-readable string.
     * @param mode The TransportationMode to convert.
     * @return A string representation of the transportation
     * mode.
     */
    static QString toString(TransportationMode mode);

    /**
     * @brief Converts a string to the corresponding
     * TransportationMode.
     *
     * This function is case-insensitive and will match
     * partial strings that reasonably identify a
     * transportation mode.
     *
     * @param str The string to convert.
     * @return The TransportationMode corresponding to the
     * given string.
     * @throws std::invalid_argument if the string doesn't
     * match any transportation mode.
     */
    static TransportationMode
    fromString(const QString &str);
};

/**
 * @brief YAML-schema canonical mode string for a `TransportationMode`.
 *
 * Produces the lowercase `"truck"` / `"rail"` / `"ship"` convention used
 * by `Connection::mode`, `GlobalLink::mode`, the ScenarioValidator, and
 * `SegmentCostMath`'s transportModes lookup. Distinct from
 * `TransportationTypes::toString` which produces the C++ enum-key form
 * (`"Truck"` / `"Train"` / `"Ship"`) intended for user-visible display.
 *
 * Matches the naming convention of `networkKindToString`,
 * `linkageStrategyToString`, and `positionModeToString` — free function
 * prefixed with the enum name, co-located with the enum.
 *
 * `Any` → empty QString (no canonical YAML form).
 */
QString transportationModeToString(
    TransportationTypes::TransportationMode m);

/**
 * @brief Inverse of `transportationModeToString`. Case-insensitive;
 *        accepts `"rail"` and `"train"` interchangeably (documented
 *        schema form is `"rail"` but the enum value is `Train`).
 *        Sets @p ok to false and returns `Any` on unknown input.
 */
TransportationTypes::TransportationMode
transportationModeFromString(const QString &s, bool *ok = nullptr);

/**
 * @brief Interface-canonical string for a `TransportationMode`.
 *
 * Produces the capitalized `"Truck"` / `"Rail"` / `"Ship"` vocabulary used
 * by the terminal interface schema (YAML `terminals[].interfaces.land_side`
 * / `.sea_side`). This is a DIFFERENT vocabulary from the lowercase
 * `transportationModeToString` used by `Connection`/`GlobalLink` — both are
 * load-bearing and must not be conflated.
 *
 * `Any` → empty QString (invalid for interface modes).
 */
QString interfaceModeCanonicalString(
    TransportationTypes::TransportationMode m);

/**
 * @brief Inverse of `interfaceModeCanonicalString`. Case-insensitive;
 *        accepts `"rail"` and `"train"` interchangeably. Sets @p ok to
 *        false and returns `Any` on unknown input. Shares its parse
 *        logic with `transportationModeFromString` (the canonical-string
 *        vocabulary is a case/formatting difference only).
 */
TransportationTypes::TransportationMode
interfaceModeFromCanonicalString(const QString &s, bool *ok = nullptr);

} // namespace Backend
} // namespace CargoNetSim

// Register custom types with Qt's meta-object system
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::TransportationTypes::
        TransportationMode)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::TransportationTypes::
        TransportationMode *)
