#include "ifc-api.h"

webifc::manager::ModelManager manager = new webifc::manager::ModelManager(MT_ENABLED);

static IfcSimpleValueVariant ReadValue(webifc::parsing::IfcLoader* loader, webifc::parsing::IfcTokenType t)
{
    // The function should not be responsible for getting the loader.
    // It should be passed in, making the function pure and testable.
    if (!loader) {
        return std::monostate{}; // Handle null loader gracefully
    }

    switch (t)
    {
    case webifc::parsing::IfcTokenType::STRING:
        return loader->GetDecodedStringArgument();

    case webifc::parsing::IfcTokenType::ENUM:
    {
        std::string_view s = loader->GetStringArgument();
        if (s == "T") {
            return true;
        }
        if (s == "F") {
            return false;
        }
        if (s == "U") {
            // Use std::monostate for "Undefined" or "Unknown"
            return std::monostate{};
        }
        // Store other enum values as their string representation
        return std::string(s);
    }

    case webifc::parsing::IfcTokenType::REAL:
        // **Crucial Change:** Store REAL as a double, not a string.
        // Assuming a method like GetDoubleArgument() exists.
        return loader->GetDoubleArgument();
        // If only GetDoubleArgumentAsString() exists, you'd have to parse it:
        // return std::stod(std::string(loader->GetDoubleArgumentAsString()));

    case webifc::parsing::IfcTokenType::INTEGER:
        return loader->GetIntArgument();

    case webifc::parsing::IfcTokenType::REF:
        return loader->GetRefArgument();

    default:
        // Use std::monostate to signal an unhandled or undefined value
        return std::monostate{};
    }
}


/**
 * @brief Recursively parses a list of IFC arguments.
 *
 * This function parses tokens from the loader until it hits a SET_END
 * or a LINE_END. It builds a list of IfcArgument items.
 *
 * @param loader  A pointer to the active IfcLoader.
 * @param manager A pointer to your manager (for 'LABEL' typecode lookups).
 * @return        An IfcArgumentList (std::vector<IfcArgument>)
 * containing all parsed arguments.
 */
IfcArgumentList GetArgs(webifc::parsing::IfcLoader* loader, webifc::manager::ModelManager& manager)
{
    IfcArgumentList arguments;
    bool endOfList = false;

    while (!loader->IsAtEnd() && !endOfList)
    {
        webifc::parsing::IfcTokenType t = loader->GetTokenType();

        switch (t)
        {
            // --- List Terminators ---
        case webifc::parsing::IfcTokenType::LINE_END:
        case webifc::parsing::IfcTokenType::SET_END:
        {
            endOfList = true;
            break;
        }

        // --- Simple Empty Value ($) ---
        case webifc::parsing::IfcTokenType::EMPTY:
        {
            // Push back an IfcArgument holding a std::monostate
            arguments.emplace_back(std::monostate{});
            break;
        }

        // --- Recursive List ---
        case webifc::parsing::IfcTokenType::SET_BEGIN:
        {
            // Recursively call GetArgs and wrap the resulting list
            // in an IfcArgument.
            arguments.emplace_back(GetArgs(loader, manager));
            break;
        }

        // --- Recursive Object (Label) ---
        case webifc::parsing::IfcTokenType::LABEL:
        {
            IfcArgumentObject obj;

            // Insert the token type
            obj.insert({ "type", IfcSimpleValueVariant(static_cast<long>(t)) }); // Store as 'long'

            loader->StepBack();
            auto s = loader->GetStringArgument();
            auto typeCode = manager.GetSchemaManager().IfcTypeToTypeCode(s);

            // Insert the typecode
            obj.insert({ "typecode", IfcSimpleValueVariant(typeCode) }); // 'typeCode' is uint32_t

            // Read the set open token '('
            loader->GetTokenType();

            // Recursively parse the arguments for this object
            // and store them as an IfcArgumentList under the "value" key
            obj.insert({ "value", IfcArgument{GetArgs(loader, manager)} });

            // Wrap the entire map in an IfcArgument and add to our list
            arguments.emplace_back(std::move(obj));
            break;
        }

        // --- Simple Values (Leaf Nodes) ---
        case webifc::parsing::IfcTokenType::STRING:
        case webifc::parsing::IfcTokenType::ENUM:
        case webifc::parsing::IfcTokenType::REAL:
        case webifc::parsing::IfcTokenType::INTEGER:
        case webifc::parsing::IfcTokenType::REF:
        {
            loader->StepBack();
            // Call our ReadValue function to get the simple value
            IfcSimpleValueVariant simpleVal = ReadValue(loader, t);

            // Wrap the simple value in an IfcArgument and add to our list
            arguments.emplace_back(simpleVal);
            break;
        }

        default:
            // Ignore other tokens (or handle as error)
            break;
        }
    }

    // Always return the list of arguments we've built.
    // It will be empty if nothing was parsed.
    return arguments;
}

// Original GetLine from wasm api
IfcRawLine GetRawLineData(webifc::parsing::IfcLoader* loader, int modelID, webifc::manager::ModelManager& manager, uint32_t expressID)
{

    if (!manager.IsModelOpen(modelID))
        return IfcRawLine();
    if (!loader->IsValidExpressID(expressID))
        return IfcRawLine();
    uint32_t lineType = loader->GetLineType(expressID);
    if (lineType == 0)
        return IfcRawLine();

    loader->MoveToArgumentOffset(expressID, 0);

    auto arguments = GetArgs(loader,manager);

    IfcRawLine retVal;
    retVal.ID = expressID;
    retVal.type = lineType;
    retVal.arguments = arguments;
    return retVal;
};

// Original GetLines from wasm api
std::vector<IfcRawLine> GetRawLinesData(webifc::parsing::IfcLoader* loader, int modelID, webifc::manager::ModelManager& manager, std::vector<uint32_t> expressIDs)
{
    std::vector<IfcRawLine> result;

    for (auto i : expressIDs)
    {
        result.push_back(GetRawLineData(loader, modelID, manager, i));
    }

    return result;
}

