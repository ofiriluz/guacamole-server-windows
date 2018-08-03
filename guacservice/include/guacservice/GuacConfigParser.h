//
// Created by oiluz on 9/3/2017.
//

#ifndef GUACAMOLE_SERVER_SRC_GUACCONFIGPARSER_H
#define GUACAMOLE_SERVER_SRC_GUACCONFIGPARSER_H

#include <guacservice/GuacConfig.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>

typedef boost::property_tree::ptree JSONTree;

class GuacConfigParser
{
private:
   /**
    * For a given tree, parsers the tree into the config file with default values
    * @param rTree
    * @param rOutConfig
    * @return
    */
   bool JSONTreeToConfig(const JSONTree & rTree, GuacConfig & rOutConfig);
   /**
    * For a given config, parsers it into the tree
    * @param rConfig
    * @param rOutTree
    * @return
    */
   bool ConfigToJSONTree(const GuacConfig & rConfig, JSONTree & rOutTree);

public:
   /**
    * Default Constructor
    */
   GuacConfigParser() = default;
   /**
    * Default Destructor
    */
   virtual ~GuacConfigParser() = default;
   /**
    * For a given config, writes it into the output config string
    * @param rConfig
    * @param stOutConfig
    * @return
    */
   bool ConfigToString(const GuacConfig & rConfig, std::string & stOutConfig);
   /**
    * For a given config, writes it into a file by the given path
    * @param rConfig
    * @param stOutConfigFilePath
    * @return
    */
   bool ConfigToFile(const GuacConfig & rConfig, const std::string & stOutConfigFilePath);
   /**
    * For a given config string, parsers it into the out config
    * @param stConfig
    * @param rOutConfig
    * @return
    */
   bool StringToConfig(const std::string & stConfig, GuacConfig & rOutConfig);
   /**
    * For a given file path, parsers it into the out config
    * @param stConfigFile
    * @param rOutConfig
    * @return
    */
   bool FileToConfig(const std::string & stConfigFile, GuacConfig & rOutConfig);
};

#endif /* GUACAMOLE_SERVER_SRC_GUACCONFIGPARSER_H */