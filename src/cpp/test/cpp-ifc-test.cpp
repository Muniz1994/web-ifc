/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <iostream>
#include <fstream>
#include <cstdint>
#include <filesystem>
#include <print>
#include "io_helpers.h"

#include "../web-ifc/parsing/IfcLoader.h"
#include "../web-ifc/schema/IfcSchemaManager.h"
#include "../web-ifc/geometry/IfcGeometryProcessor.h"
#include "../web-ifc/schema/ifc-schema.h"
#include "../web-ifc/parsing/ifc-api.h"

using namespace webifc::io;


// ---------------------------------------------------------------------- Process values 

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

// Visitor for the innermost IfcSimpleValueVariant
// This is where you decide what to do with the data.
auto printIfcSimpleValue = overloaded{
    [](const std::string& val) { std::cout << "String: " << val << "\n"; },
    [](bool val) { std::cout << "Bool: " << val << "\n"; },
    [](long val) { std::cout << "Long: " << val << "\n"; },
    [](uint32_t val) { std::cout << "uint32_t: " << val << "\n"; },
    [](double val) { std::cout << "Double: " << val << "\n"; },
    [](std::monostate) { /* Do nothing for null/empty */ }
};

auto getIfcSimpleValue = overloaded{
    [](const std::string& val) { return val; },
    [](bool val) { return val; },
    [](long val) { return val; },
    [](uint32_t val) { return val; },
    [](double val) { return val; },
    [](std::monostate) { /* Do nothing for null/empty */ }
};

// We must forward-declare the function so it can call itself in the lambda
void processArgument(const IfcArgument& arg);

// The main recursive visitor
auto processArgumentVisitor = overloaded{
    // Base Case: We found a simple value.
    [&](const IfcSimpleValueVariant& simpleVal) {
        // Use the other visitor to handle the simple types
        std::visit(getIfcSimpleValue, simpleVal);
    },

    // Recursive Case 1: We found a list.
    [&](const IfcArgumentList& list) {
        // Loop through the list and call the main function recursively
        for (const IfcArgument& childArg : list) {
            processArgument(childArg);
        }
    },

    // Recursive Case 2: We found an object (map).
    [&](const IfcArgumentObject& obj) {
        // Loop through the map and call the main function recursively
        for (const auto& [key, childArg] : obj) {
            // You could also print the key: std::cout << "Key: " << key << "\n";
            processArgument(childArg);
        }
    }
};

// Definition of the function that uses the visitor
void processArgument(const IfcArgument& arg)
{
    std::visit(processArgumentVisitor, arg.value);
}

// --------------------------------------------------------------------- END process values

long long ms()
{
    using namespace std::chrono;
    milliseconds millis = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch());

    return millis.count();
}

double RandomDouble(double lo, double hi)
{
    return lo + static_cast<double>(rand()) / (static_cast<double>(RAND_MAX / (hi - lo)));
}

std::string ReadFile(std::string filename)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open file");
    }

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string buffer(size, '\0');
    
    if (!file.read(buffer.data(), size)) {
        throw std::runtime_error("Error reading file");
    }

    return buffer;
}

void SpecificLoadTest(webifc::parsing::IfcLoader &loader, webifc::geometry::IfcGeometryProcessor &geometryLoader, uint64_t num)
{
    auto walls = loader.GetExpressIDsWithType(webifc::schema::IFCSLAB);

    bool writeFiles = true;

    auto mesh = geometryLoader.GetMesh(num);

    if (writeFiles)
    {
        DumpMesh(mesh, geometryLoader, "TEST.obj");
    }
}

std::vector<webifc::geometry::IfcAlignment> GetAlignments(webifc::parsing::IfcLoader &loader, webifc::geometry::IfcGeometryProcessor &geometryLoader)
{
    std::vector<webifc::geometry::IfcAlignment> alignments;

    auto type = webifc::schema::IFCALIGNMENT;

    auto elements = loader.GetExpressIDsWithType(type);

    for (unsigned int i = 0; i < elements.size(); i++)
    {
        auto alignment = geometryLoader.GetLoader().GetAlignment(elements[i]);
        alignment.transform(geometryLoader.GetCoordinationMatrix());
        alignments.push_back(alignment);
    }

    bool writeFiles = true;

    if (writeFiles)
    {
        DumpAlignment(alignments, "V_ALIGN.obj", "H_ALIGN.obj");
    }

    for (size_t i = 0; i < alignments.size(); i++)
    {
        webifc::geometry::IfcAlignment alignment = alignments[i];
        std::vector<glm::dvec3> pointsH;
        std::vector<glm::dvec3> pointsV;
        for (size_t j = 0; j < alignment.Horizontal.curves.size(); j++)
        {
            for (size_t k = 0; k < alignment.Horizontal.curves[j].points.size(); k++)
            {
                pointsH.push_back(alignment.Horizontal.curves[j].points[k]);
            }
        }
        for (size_t j = 0; j < alignment.Vertical.curves.size(); j++)
        {
            for (size_t k = 0; k < alignment.Vertical.curves[j].points.size(); k++)
            {
                pointsV.push_back(alignment.Vertical.curves[j].points[k]);
            }
        }
        webifc::geometry::IfcCurve curve;
        curve.points = bimGeometry::Convert2DAlignmentsTo3D(pointsH, pointsV);
        alignments[i].Absolute.curves.push_back(curve);
    }

    return alignments;
}

std::vector<webifc::geometry::IfcCrossSections> GetCrossSections3D(webifc::parsing::IfcLoader &loader, webifc::geometry::IfcGeometryProcessor &geometryLoader)
{
    std::vector<webifc::geometry::IfcCrossSections> crossSections;

    std::vector<uint32_t> typeList;
    typeList.push_back(webifc::schema::IFCSECTIONEDSOLID);
    typeList.push_back(webifc::schema::IFCSECTIONEDSURFACE);
    typeList.push_back(webifc::schema::IFCSECTIONEDSOLIDHORIZONTAL);

    for (auto &type : typeList)
    {

        auto elements = loader.GetExpressIDsWithType(type);

        for (unsigned int i = 0; i < elements.size(); i++)
        {
            auto crossSection = geometryLoader.GetLoader().GetCrossSections3D(elements[i]);
            crossSections.push_back(crossSection);
        }
    }

    bool writeFiles = true;

    if (writeFiles)
    {
        DumpCrossSections(crossSections, "CrossSection.obj");
    }

    return crossSections;
}

std::string ReadValue(webifc::parsing::IfcLoader &loader, webifc::parsing::IfcTokenType t)
{
    switch (t)
    {
    case webifc::parsing::IfcTokenType::STRING:
    {
        return loader.GetDecodedStringArgument();
    }
    case webifc::parsing::IfcTokenType::ENUM:
    {
        std::string_view s = loader.GetStringArgument();
        return std::string(s);
    }
    case webifc::parsing::IfcTokenType::REAL:
    {
        std::string_view s = loader.GetDoubleArgumentAsString();
        return std::string(s);
    }
    case webifc::parsing::IfcTokenType::INTEGER:
    {
        long d = loader.GetIntArgument();
        return std::to_string(d);
    }
    case webifc::parsing::IfcTokenType::REF:
    {
        uint32_t ref = loader.GetRefArgument();
        return std::to_string(ref);
    }
    default:
        // use undefined to signal val parse issue
        return "";
    }
}

std::string GetArgs(webifc::parsing::IfcLoader &loader, bool inObject = false, bool inList = false)
{
    std::string arguments;
    size_t size = 0;
    bool endOfLine = false;
    while (!loader.IsAtEnd() && !endOfLine)
    {
        webifc::parsing::IfcTokenType t = loader.GetTokenType();

        switch (t)
        {
        case webifc::parsing::IfcTokenType::LINE_END:
        {
            endOfLine = true;
            break;
        }
        case webifc::parsing::IfcTokenType::EMPTY:
        {
            arguments += " Empty ";
            break;
        }
        case webifc::parsing::IfcTokenType::SET_BEGIN:
        {
            arguments += GetArgs(loader, false, true);
            break;
        }
        case webifc::parsing::IfcTokenType::SET_END:
        {
            endOfLine = true;
            break;
        }
        case webifc::parsing::IfcTokenType::LABEL:
        {
            // read label
            std::string obj;
            obj = " type: LABEL ";
            loader.StepBack();
            auto s = loader.GetStringArgument();
            // read set open
            loader.GetTokenType();
            obj += " value " + GetArgs(loader, true) + " ";
            arguments += obj;
            break;
        }
        case webifc::parsing::IfcTokenType::STRING:
        case webifc::parsing::IfcTokenType::ENUM:
        case webifc::parsing::IfcTokenType::REAL:
        case webifc::parsing::IfcTokenType::INTEGER:
        case webifc::parsing::IfcTokenType::REF:
        {
            loader.StepBack();
            std::string obj;
            if (inObject)
                obj = ReadValue(loader, t);
            else
            {
                std::string obj;
                obj += " type REF ";
                obj += ReadValue(loader, t) + " ";
            }
            arguments += obj;
            break;
        }
        default:
            break;
        }
    }
    return arguments;
}

std::string GetLine(webifc::parsing::IfcLoader &loader, uint32_t expressID)
{
    if (!loader.IsValidExpressID(expressID))
        return "";
    uint32_t lineType = loader.GetLineType(expressID);
    if (lineType == 0)
        return "";

    loader.MoveToArgumentOffset(expressID, 0);

    auto arguments = GetArgs(loader);

    std::string retVal;
    retVal += "\"ID\": " + std::to_string(expressID) + ", ";
    retVal += "\"type\": " + std::to_string(lineType) + ", ";
    retVal += "\"arguments\": " + arguments;
    retVal += "}";

    return retVal;
}

std::vector<webifc::geometry::IfcFlatMesh> LoadAllTest(webifc::parsing::IfcLoader &loader, webifc::geometry::IfcGeometryProcessor &geometryLoader, uint32_t IdToExport)
{
    std::vector<webifc::geometry::IfcFlatMesh> meshes;
    webifc::schema::IfcSchemaManager schema;

    bool writeFiles = true;

    for (auto type : schema.GetIfcElementList())
    {
        auto elements = loader.GetExpressIDsWithType(type);

        for (unsigned int i = 0; i < elements.size(); i++)
        {
            auto mesh = geometryLoader.GetFlatMesh(elements[i]);

            if (mesh.expressID == IdToExport)
            {
                DumpFlatMesh(mesh, geometryLoader, "TEST_GEOM.obj");
            }

            for (auto &geom : mesh.geometries)
            {
                auto flatGeom = geometryLoader.GetGeometry(geom.geometryExpressID);
            }

            meshes.push_back(mesh);
        }
    }

    return meshes;
}

std::vector<webifc::geometry::SweptDiskSolid> GetAllRebars(webifc::parsing::IfcLoader &loader, webifc::geometry::IfcGeometryProcessor &geometryLoader)
{
    std::vector<webifc::geometry::SweptDiskSolid> reinforcingBars;
    std::vector<glm::dmat4> reinforcingBarsTransform;

    auto type = webifc::schema::IFCREINFORCINGBAR;

    auto elements = loader.GetExpressIDsWithType(type);

    for (size_t i = 0; i < elements.size(); i++)
    {
        auto mesh = geometryLoader.GetFlatMesh(elements[i]);

        for (auto &geom : mesh.geometries)
        {
            auto flatGeom = geometryLoader.GetGeometry(geom.geometryExpressID);
            reinforcingBars.push_back(flatGeom.sweptDiskSolid);
            reinforcingBarsTransform.push_back(geom.transformation);
        }
    }

    return reinforcingBars;
}

void DumpRefs(std::unordered_map<uint32_t, std::vector<uint32_t>> &refs)
{
    std::ofstream of("refs.txt");

    int32_t prev = 0;
    for (auto &it : refs)
    {
        if (!it.second.empty())
        {
            for (auto &i : it.second)
            {
                of << (((int32_t)i) - (prev));
                prev = i;
            }
        }
    }
}

struct BenchMarkResult
{
    std::string file;
    long long timeMS;
    long long sizeBytes;
};

void Benchmark()
{
    std::vector<BenchMarkResult> results;
    std::string path = "../../../benchmark/ifcfiles";
    for (const auto &entry : std::filesystem::directory_iterator(path))
    {
        if (entry.path().extension().string() != ".ifc")
        {
            continue;
        }

        std::string filePath = entry.path().string();
        std::string filename = entry.path().filename().string();

        std::string content = ReadFile(filePath);

        auto start = ms();
        {
            // loader.LoadFile(content);
        }
        auto time = ms() - start;

        BenchMarkResult result;
        result.file = filename;
        result.timeMS = time;
        result.sizeBytes = entry.file_size();
        results.push_back(result);

        std::cout << "Reading " << result.file << " took " << time << "ms" << std::endl;
    }

    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << "Results:" << std::endl;

    double avgMBsec = 0;
    for (auto &result : results)
    {
        double MBsec = result.sizeBytes / 1000.0 / result.timeMS;
        avgMBsec += MBsec;
        std::cout << result.file << ": " << MBsec << " MB/sec" << std::endl;
    }

    avgMBsec /= results.size();

    std::cout << std::endl;
    std::cout << "Average: " << avgMBsec << " MB/sec" << std::endl;

    std::cout << std::endl;
    std::cout << std::endl;
}

void TestTriangleDecompose()
{
    const int NUM_TESTS = 100;
    const int PTS_PER_TEST = 100;
    const int EDGE_PTS_PER_TEST = 10;

    const double scaleX = 650;
    const double scaleY = 1;

    glm::dvec2 a(0, 0);
    glm::dvec2 b(scaleX, 0);
    glm::dvec2 c(0, scaleY);

    for (int i = 0; i < NUM_TESTS; i++)
    {
        srand(i);

        std::vector<glm::dvec2> points;

        // random points
        for (unsigned int j = 0; j < PTS_PER_TEST; j++)
        {
            points.push_back({RandomDouble(0, scaleX),
                              RandomDouble(0, scaleY)});
        }

        // points along the edges
        for (unsigned int j = 0; j < EDGE_PTS_PER_TEST; j++)
        {
            glm::dvec2 e1 = b - a;
            glm::dvec2 e2 = c - a;
            glm::dvec2 e3 = b - c;

            points.push_back(a + e1 * RandomDouble(0, 1));
            points.push_back(a + e2 * RandomDouble(0, 1));
            points.push_back(c + e3 * RandomDouble(0, 1));
        }

        std::cout << "Start test " << i << std::endl;

        bool swapped = false;

        // webifc::IsValidTriangulation(triangles, points);

        std::vector<webifc::io::Point> pts;

        for (auto &pt : points)
        {
            webifc::io::Point p;
            p.x = pt.x;
            p.y = pt.y;
            pts.push_back(p);
        }
    }
}

int main()
{
    std::cout << "Hello web IFC test!" << std::endl;

    std::string path= "C:/Users/engbr/Documents/GitHub/IFcFiles/Example_Georeferenced.ifc";

    struct LoaderSettings
    {
        bool COORDINATE_TO_ORIGIN = false;
        uint16_t CIRCLE_SEGMENTS = 12;
        uint32_t TAPE_SIZE = 67108864; // probably no need for anyone other than web-ifc devs to change this
        uint32_t MEMORY_LIMIT = 2147483648;
        uint16_t LINEWRITER_BUFFER = 10000;
        double TOLERANCE_PLANE_INTERSECTION = 1.0E-04;
        double TOLERANCE_PLANE_DEVIATION = 1.0E-04;
        double TOLERANCE_BACK_DEVIATION_DISTANCE = 1.0E-04;
        double TOLERANCE_INSIDE_OUTSIDE_PERIMETER = 1.0E-10;
        double TOLERANCE_SCALAR_EQUALITY = 1.0E-04;
        uint16_t PLANE_REFIT_ITERATIONS = 1;
        uint16_t BOOLEAN_UNION_THRESHOLD = 150;
    };

    LoaderSettings set;

    set.COORDINATE_TO_ORIGIN = true;

    webifc::schema::IfcSchemaManager schemaManager;

    webifc::manager::ModelManager manager(true);

    webifc::parsing::IfcLoader loader(set.TAPE_SIZE, set.MEMORY_LIMIT, set.LINEWRITER_BUFFER, schemaManager);

    auto start = ms();

    std::ifstream file_stream(path);

    if (file_stream.is_open()) {
        // 3. Pass the stream object (by reference) to loadfile
        //    'file_stream' is an object of a class derived from std::istream.
        loader.LoadFile(file_stream);

        file_stream.close();
    }
    else {
        std::cout << "Error: Could not read ifc file";
    }

    auto walls = loader.GetExpressIDsWithType(schemaManager.IfcTypeToTypeCode("IFCELEMENTQUANTITY"));

    for (auto i : walls)
    {
        // Assuming GetRawLineData returns an IfcRawLine or similar
        // and .arguments IS the IfcArgument struct.
        const IfcArgument& root_argument = GetRawLineData(&loader, &manager, i).arguments;

        // Just call the recursive function.
        // It will handle everything, no matter how nested.
        processArgument(root_argument);
    }

    auto time = ms() - start;

    std::cout << "Process took " << time << "ms" << std::endl;

    std::cout << "Done" << std::endl;
}
