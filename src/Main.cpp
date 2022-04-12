
#include <cxxopts.hpp>
#include <fstream>
#include <iostream>

#include <sofa/helper/logging/Messaging.h>
#include <sofa/helper/system/FileSystem.h>

#include <sofa/simulation/config.h>
#include <sofa/simulation/Node.h>
#include <SofaSimulationCommon/SceneLoaderXML.h>

#include <sofa/core/logging/PerComponentLoggingMessageHandler.h>
#include <sofa/helper/logging/LoggingMessageHandler.h>

#include <sofa/core/ObjectFactory.h>
#include <sofa/helper/BackTrace.h>
#include <sofa/helper/system/FileRepository.h>
#include <sofa/helper/system/PluginManager.h>
#include <SofaBase/initSofaBase.h>
#include <SofaGraphComponent/SceneCheckerListener.h>
#include <SofaGraphComponent/SceneCheckerVisitor.h>
#include <SofaGraphComponent/SceneCheckMissingRequiredPlugin.h>
#include <SofaSimulationGraph/DAGSimulation.h>
#include <SofaSimulationGraph/init.h>

void loadPlugins(const char* const appName);
void getAllInputFiles(const char* const appName, const std::vector<std::string>& input, std::vector<std::string>& allFiles);
void findPluginsFromNode(sofa::simulation::NodeSPtr root, std::map<std::string, std::set<std::string> >& allRequiredPlugins);
void writeRequiredPlugins(const char* const appName, const std::string& inputFile, std::map<std::string, std::set<std::string> >& allRequiredPlugins);



int main(int argc, char** argv)
{
    constexpr const char* appName = "PluginFinder";
    cxxopts::Options options(appName, "Analyses SOFA scenes and set the optimal RequiredPlugin components");
    options.add_options()
        ("verbose", "Verbose")
        ("h,help", "print usage")
        ("input", "Input file(s) or directory",
                cxxopts::value<std::vector<std::string>>());

    options.parse_positional("input");
    const auto result = options.parse(argc, argv);


    if (result.count("help"))
    {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    if (result.count("input") == 0)
    {
        std::cout << options.help() << std::endl;
        exit(0);
    }

    sofa::helper::logging::MessageDispatcher::addHandler(&sofa::helper::logging::MainLoggingMessageHandler::getInstance());
    sofa::helper::logging::MainLoggingMessageHandler::getInstance().activate();

    sofa::helper::BackTrace::autodump();
    sofa::simulation::graph::init();

    sofa::simulation::setSimulation(new sofa::simulation::graph::DAGSimulation());

    loadPlugins(appName);

    const auto input = result["input"].as<std::vector<std::string>>();
    const auto verbose = result["verbose"].as<bool>();

    std::vector<std::string> allFiles;

    getAllInputFiles(appName, input, allFiles);

    for (auto& in : allFiles)
    {
        in = sofa::helper::system::FileSystem::convertSlashesToBackSlashes(in);
        msg_info_when(verbose, appName) << "Processing " << in;

        sofa::simulation::SceneLoaderXML loader;

        if (!loader.canLoadFileName(in.c_str()))
        {
            msg_error(appName) << "Cannot load file " << in;
            continue;
        }
        sofa::simulation::NodeSPtr root;
        try
        {
            root = loader.load(in);
        }
        catch(...)
        {
            msg_info(appName) << "Error during loading of file " << in;
            continue;
        }


        if (!root)
        {
            msg_error(appName) << "Could not load file " << in;
        }

        std::map<std::string, std::set<std::string> > allRequiredPlugins;

        findPluginsFromNode(root, allRequiredPlugins);
        writeRequiredPlugins(appName, in, allRequiredPlugins);
    }

    sofa::simulation::graph::cleanup();
    return 0;
}

void loadPlugins(const char* const appName)
{
    std::string configPluginPath = "plugin_list.conf";
    std::string defaultConfigPluginPath = "plugin_list.conf.default";
    if (sofa::helper::system::PluginRepository.findFile(configPluginPath, "", nullptr))
    {
        msg_info(appName) << "Loading automatically plugin list in " << configPluginPath;
        sofa::helper::system::PluginManager::getInstance().readFromIniFile(configPluginPath);
    }
    else if (sofa::helper::system::PluginRepository.findFile(defaultConfigPluginPath, "", nullptr))
    {
        msg_info(appName) << "Loading automatically plugin list in " << defaultConfigPluginPath;
        sofa::helper::system::PluginManager::getInstance().readFromIniFile(defaultConfigPluginPath);
    }
    else
    {
        msg_info(appName) << "No plugin list found. No plugin will be automatically loaded.";
    }
}

void getAllInputFiles(const char* const appName, const std::vector<std::string>& input, std::vector<std::string>& allFiles)
{
    for (const auto& in : input)
    {
        const auto exist = sofa::helper::system::FileSystem::exists(in);
        if (!exist)
        {
            msg_error(appName) << "Cannot find " << in;
            continue;
        }
        const auto isDirectory = sofa::helper::system::FileSystem::isDirectory(in);
        const auto isFile = sofa::helper::system::FileSystem::isFile(in);

        if (isFile)
        {
            allFiles.push_back(in);
        }
        else if (isDirectory)
        {
            sofa::helper::system::FileSystem::findFiles(in, allFiles, ".scn", 1000);
        }
    }
}

void findPluginsFromNode(sofa::simulation::NodeSPtr root, std::map<std::string, std::set<std::string> >& allRequiredPlugins)
{
    const auto objects = root->getTreeObjects<sofa::core::objectmodel::BaseObject>();
    for (const auto& object : objects)
    {
        const sofa::core::ObjectFactory::ClassEntry entry =sofa::core::ObjectFactory::getInstance()->getEntry(object->getClassName());
        if (!entry.creatorMap.empty())
        {
            auto it = entry.creatorMap.find(object->getTemplateName());
            if (entry.creatorMap.end() != it)
            {
                const std::string pluginName = it->second->getTarget();
                if (!pluginName.empty())
                {
                    allRequiredPlugins[pluginName].insert(object->getClassName());
                }
            }
        }
    }
}

void writeRequiredPlugins(const char* const appName, const std::string& inputFile, std::map<std::string, std::set<std::string> >& allRequiredPlugins)
{
    std::ifstream ifs;
    std::string line;
    ifs.open( inputFile, std::ios::in );  // input file stream

    std::vector<std::string> newFileLines;

    if(ifs)
    {
        std::string indentation;
        bool arePluginsInserted = false;
        while ( !ifs.eof() )
        {
            std::getline ( ifs, line);
            const auto pos = line.find("<RequiredPlugin");
            if (pos != std::string::npos && line.find("/>") != std::string::npos)
            {
                if (!arePluginsInserted)
                {
                    indentation = line.substr(0, pos);

                    for (const auto& [plugin, components] : allRequiredPlugins)
                    {
                        if (components.size() == 1 && *components.begin() != "RequiredPlugin")
                        {
                            std::stringstream ss;
                            ss << indentation << "<RequiredPlugin name=\"" << plugin << "\"/> <!-- Needed to use components [" << components << "] -->\n";
                            newFileLines.push_back(ss.str());
                        }
                    }

                    arePluginsInserted = true;
                }
            }
            else
            {
                newFileLines.push_back(line + "\n");
            }
        }
        ifs.close();
    }
    else msg_error(appName) << "Unable to open file to read " << inputFile;

    if (!newFileLines.empty())
    {
        newFileLines.back() = newFileLines.back().substr(0, newFileLines.back().rfind("\n"));
    }

    std::ofstream ofs;

    ofs.open( inputFile);
    if (ofs)
    {
        for (const auto& l : newFileLines)
        {
            ofs << l;
        }
    }
    else msg_error(appName) << "Unable to open file to write " << inputFile;

    ofs.close();
}
