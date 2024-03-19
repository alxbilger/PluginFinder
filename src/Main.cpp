
#include <cxxopts.hpp>
#include <fstream>
#include <iostream>

#include <sofa/helper/logging/Messaging.h>
#include <sofa/helper/system/FileSystem.h>

#include <sofa/simulation/config.h>
#include <sofa/simulation/Node.h>
#include <sofa/simulation/common/SceneLoaderXML.h>

#include <sofa/helper/logging/LoggingMessageHandler.h>

#include <sofa/core/ObjectFactory.h>
#include <sofa/helper/BackTrace.h>
#include <sofa/helper/system/FileRepository.h>
#include <sofa/helper/system/PluginManager.h>
#include <sofa/simulation/graph/DAGSimulation.h>
#include <sofa/simulation/graph/init.h>

#include "ErrorCountingMessageHandler.h"

void loadPlugins(const char* const appName, const std::vector<std::string>& pluginsToLoad);
void getAllInputFiles(const char* const appName, const std::vector<std::string>& input, std::vector<std::string>& allFiles);
void findPluginsFromNode(sofa::simulation::NodeSPtr root, std::map<std::string, std::set<std::string> >& allRequiredPlugins);
void writeRequiredPlugins(const char* const appName, const std::string& inputFile, std::map<std::string, std::set<std::string> >& allRequiredPlugins);
void replaceInFile(const std::string& in, const std::map<std::string, std::string>& map);

int main(int argc, char** argv)
{
    constexpr const char* appName = "PluginFinder";
    cxxopts::Options options(appName, "Analyses SOFA scenes and set the optimal RequiredPlugin components");
    options.add_options()
        ("verbose", "Verbose")
        ("h,help", "print usage")
        ("l,load", "load given plugins", cxxopts::value<std::vector<std::string>>())
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


    std::vector<std::string> pluginsToLoad;
    if (result.count("load"))
    {
        pluginsToLoad = result["load"].as<std::vector<std::string>>();

    }
    loadPlugins(appName, pluginsToLoad);

    std::map<std::string, std::string > aliases; //key: alias, value: original name
    sofa::core::ObjectFactory::getInstance()->setCallback([&aliases](sofa::core::Base* o, sofa::core::objectmodel::BaseObjectDescription *arg)
    {
        const std::string typeNameInScene = arg->getAttribute("type", "");
        if ( typeNameInScene != o->getClassName() )
        {
            aliases[typeNameInScene] = o->getClassName();
        }
    });

    const auto input = result["input"].as<std::vector<std::string>>();
    const auto verbose = result["verbose"].as<bool>();

    std::vector<std::string> allFiles;

    getAllInputFiles(appName, input, allFiles);

    std::map<sofa::helper::logging::Message::Type, std::vector<std::string> > filesWithMessages;

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

        ErrorCountingMessageHandler countingMessageHandler;

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

        if (countingMessageHandler.getCount(sofa::helper::logging::Message::Type::Error) > 0)
        {
            msg_info(appName) << "Error during loading of file " << in << ": skip";
            filesWithMessages[sofa::helper::logging::Message::Type::Error].push_back(in);
            continue;
        }
        if (countingMessageHandler.getCount(sofa::helper::logging::Message::Type::Fatal) > 0)
        {
            msg_info(appName) << "Error during loading of file " << in << ": skip";
            filesWithMessages[sofa::helper::logging::Message::Type::Fatal].push_back(in);
            continue;
        }
        for (const auto type : {sofa::helper::logging::Message::Type::Advice, sofa::helper::logging::Message::Type::Deprecated, sofa::helper::logging::Message::Type::Info, sofa::helper::logging::Message::Type::Warning})
        {
            if (countingMessageHandler.getCount(type))
            {
                filesWithMessages[type].push_back(in);
            }
        }

        std::map<std::string, std::set<std::string> > allRequiredPlugins;

        findPluginsFromNode(root, allRequiredPlugins);
        writeRequiredPlugins(appName, in, allRequiredPlugins);

        if (!aliases.empty())
        {
            replaceInFile(in, aliases);
        }

        aliases.clear();
    }

    if (!filesWithMessages.empty())
    {

        for (const auto& [type, files] : filesWithMessages)
        {
            std::stringstream ss;
            for (const auto& file : files)
            {
                ss << '\t' << file << '\n';
            }
            static std::map<sofa::helper::logging::Message::Type, std::string> messageType{
                {sofa::helper::logging::Message::Type::Advice, "advice"},
                {sofa::helper::logging::Message::Type::Deprecated, "deprecated"},
                {sofa::helper::logging::Message::Type::Error, "error"},
                {sofa::helper::logging::Message::Type::Fatal, "fatal"},
                {sofa::helper::logging::Message::Type::Info, "info"},
                {sofa::helper::logging::Message::Type::Warning, "warning"},
            };
            msg_info(appName) << "Found " << files.size() << " files with " << messageType[type] << ":\n" << ss.str();
        }

    }

    sofa::simulation::graph::cleanup();
    return 0;
}

void loadPlugins(const char* const appName, const std::vector<std::string>& pluginsToLoad)
{
    for (const auto& plugin : pluginsToLoad)
    {
        sofa::helper::system::PluginManager::getInstance().loadPlugin(plugin);
    }

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
                        if (!components.empty() && plugin != "Sofa.Simulation.Core")
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

void replaceInFile(const std::string& in, const std::map<std::string, std::string>& aliases)
{
    std::ifstream ifs;
    ifs.open( in, std::ios::in );  // input file stream

    std::vector<std::string> newFileLines;

    if(ifs)
    {
        std::string line;
        while ( !ifs.eof() )
        {
            std::getline ( ifs, line);

            for (const auto& [alias, componentName] : aliases)
            {
                const auto pos = line.find("<" + alias + " ");
                if (pos != std::string::npos)
                {
                    line.replace(pos, alias.size() + 1, "<" + componentName + " ");
                }
            }

            newFileLines.push_back(line + '\n');
        }

        if (!newFileLines.empty())
        {
            newFileLines.back() = newFileLines.back().substr(0, newFileLines.back().rfind("\n"));
        }

        ifs.close();
    }

    std::cout << *newFileLines.begin() << std::endl;

    std::ofstream ofs;
    ofs.open( in);
    if (ofs)
    {
        for (const auto& l : newFileLines)
        {
            ofs << l;
        }
        ofs.close();
    }
}
