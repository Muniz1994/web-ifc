#include <vector>
#include <optional>
#include <string>
#include <unordered_map>
#include <any>
#include <variant>
#include <memory>

#include "../modelmanager/ModelManager.h"


constexpr bool MT_ENABLED = false;

/**
 * @brief A type alias for the variant holding all possible C++ values from an IFC token.
 *
 * - std::monostate: Represents a null, empty, or undefined value (like $ in IFC, 'U' ENUM, or a default case).
 * - std::string:    For STRING tokens or unhandled ENUM values.
 * - bool:           For ENUM 'T' (true) and 'F' (false).
 * - long:           For INTEGER tokens.
 * - uint32_t:       For REF (entity reference) tokens.
 * - double:         For REAL tokens.
 */
using IfcSimpleValueVariant = std::variant<
    std::monostate, // Explicitly for null/undefined/empty states
    std::string,
    bool,
    long,
    uint32_t,
    double        // For REAL values
>;

// Forward-declare our recursive type
struct IfcArgument;

// An "Argument List" is just a vector of Arguments
// This is for nested sets: (arg1, (arg2, arg3))
using IfcArgumentList = std::vector<IfcArgument>;

// An "Argument Object" is a map.
// This is for your LABEL case: { "typecode": 123, "value": [...] }
// Note: The value of the map is also an IfcArgument.
using IfcArgumentObject = std::unordered_map<std::string, IfcArgument>;

struct IfcArgument {
    // We use a pointer-wrapper (like std::unique_ptr) for direct recursion
    // if the type were defined as std::variant<IfcSimpleValue, std.vector<IfcArgument>, ...>
    // But since std::vector and std::unordered_map are already
    // heap-allocated, indirect types, this explicit wrapping is often not needed.
    // For safety and to be explicit, let's use a wrapper.
    // UPDATE: A simpler way is to just use the aliased heap-allocated types.

    std::variant<
        IfcSimpleValueVariant,
        IfcArgumentList,
        IfcArgumentObject
    > value;

    // --- Constructors to make life easier ---
    IfcArgument(IfcSimpleValueVariant v) : value(std::move(v)) {}
    IfcArgument(IfcArgumentList v) : value(std::move(v)) {}
    IfcArgument(IfcArgumentObject v) : value(std::move(v)) {}

    // Default constructor for EMPTY ($)
    IfcArgument() : value(std::monostate{}) {}
};

struct IfcRawLine;

using IfcRawLineObject = std::unordered_map<std::string, IfcRawLine>;

struct IfcRawLine {

    std::optional<uint32_t> ID;
    std::optional<uint32_t> type;
    IfcArgument arguments;

    // Default constructor for EMPTY ($)
    IfcRawLine() : arguments(std::monostate{}), ID(std::nullopt), type(std::nullopt) {}
};


template <typename T>
struct RawLineData
{
int ID;
int type;
std::vector<T> arguments;
};


IfcRawLine GetRawLineData(webifc::parsing::IfcLoader* loader, webifc::manager::ModelManager* manager, uint32_t expressID);

std::vector<IfcRawLine> GetRawLinesData(webifc::parsing::IfcLoader* loader, webifc::manager::ModelManager* manager, std::vector<uint32_t> expressIDs);

void GetLine(int modelID, std::vector<int> expressIDs, bool flatten=false, bool inverse=false, std::optional<std::string> inversePropKey=std::nullopt);

void GetLines(int modelID, std::vector<int> expressIDs, bool flatten=false, bool inverse=false, std::optional<std::string> inversePropKey=std::nullopt);

