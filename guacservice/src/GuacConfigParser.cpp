#include <guacservice/GuacConfigParser.h>

bool GuacConfigParser::JSONTreeToConfig(const JSONTree & rTree, GuacConfig & rOutConfig)
{
   // Deserialize everything from the json tree
   rOutConfig.SetBindHost(rTree.get<std::string>("BindHost", ""));
   rOutConfig.SetBindPort(rTree.get<short>("BindPort", 4822));
   rOutConfig.SetWithSSL(rTree.get<bool>("WithSSL", false));
   rOutConfig.SetWithConsoleLog(rTree.get<bool>("WithConsoleLog", true));
   rOutConfig.SetWithFileLog(rTree.get<bool>("WithFileLog", true));
   rOutConfig.SetMaxLogLevel(rTree.get<std::string>("MaxLogLevel", "debug"));
   rOutConfig.SetGuacServiceClientProcessPath(
           rTree.get<std::string>("GuacServiceClientProcessPath", "./guacservice_client.exe"));
   rOutConfig.SetGuacProtocolsLibrariesFolder(rTree.get<std::string>("GuacProtocolsLibrariesFolder", "./"));
   rOutConfig.SetGuacLogOutputFolder(rTree.get<std::string>("GuacLogOutputFolder", "./"));
   rOutConfig.SetSSLPEMFilePath(rTree.get<std::string>("SSLPEMFilePath", "./key.pem"));
   rOutConfig.SetSSLCertFilePath(rTree.get<std::string>("SSLCertFilePath", "./cert.crt"));
   rOutConfig.SetSSLDiffieHellmanPEMFilePath(rTree.get<std::string>("SSLDiffieHellmanPEMFilePath", "./dh512.pem"));
   rOutConfig.SetFPS(rTree.get<short>("FPS", 30));

   return true;
}

bool GuacConfigParser::ConfigToJSONTree(const GuacConfig & rConfig, JSONTree & rOutTree)
{
   // Serialize everything to the json tree
   rOutTree.put("BindHost", rConfig.GetBindHost());
   rOutTree.put("BindPort", rConfig.GetBindPort());
   rOutTree.put("WithSSL", rConfig.IsWithSSL());
   rOutTree.put("WithConsoleLog", rConfig.IsWithConsoleLog());
   rOutTree.put("WithFileLog", rConfig.IsWithFileLog());
   rOutTree.put("MaxLogLevel", boost::log::trivial::to_string(rConfig.GetMaxLogLevel()));
   rOutTree.put("GuacServiceClientProcessPath", rConfig.GetGuacServiceClientProcessPath());
   rOutTree.put("GuacProtocolsLibrariesFolder", rConfig.GetGuacProtocolsLibrariesFolder());
   rOutTree.put("GuacLogOutputFolder", rConfig.GetGuacLogOutputFolder());
   rOutTree.put("SSLPEMFilePath", rConfig.GetSSLPEMFilePath());
   rOutTree.put("SSLCertFilePath", rConfig.GetSSLCertFilePath());
   rOutTree.put("SSLDiffieHellmanPEMFilePath", rConfig.GetSSLDiffieHellmanPEMFilePath());
   rOutTree.put("FPS", rConfig.GetFPS());

   return true;
}

bool GuacConfigParser::ConfigToString(const GuacConfig & rConfig, std::string & stOutConfig)
{
   try
   {
      JSONTree tree;
      if(ConfigToJSONTree(rConfig, tree))
      {
         std::stringstream ss;
         boost::property_tree::write_json(ss, tree);
         stOutConfig = ss.str();
         return true;
      }
   }
   catch(...)
   {

   }
   return false;
}

bool GuacConfigParser::ConfigToFile(const GuacConfig & rConfig, const std::string & stOutConfigFilePath)
{
   try
   {
      JSONTree tree;
      if(ConfigToJSONTree(rConfig, tree))
      {
         boost::property_tree::write_json(stOutConfigFilePath, tree);
         return true;
      }
   }
   catch(...)
   {

   }
   return false;
}

bool GuacConfigParser::StringToConfig(const std::string & stConfig, GuacConfig & rOutConfig)
{
   try
   {
      std::stringstream ss;
      ss << stConfig;
      JSONTree tree;
      boost::property_tree::read_json(ss, tree);
      if(JSONTreeToConfig(tree, rOutConfig))
      {
         return true;
      }
   }
   catch(...)
   {

   }
   return false;
}

bool GuacConfigParser::FileToConfig(const std::string & stConfigFile, GuacConfig & rOutConfig)
{
   try
   {
      JSONTree tree;
      boost::property_tree::read_json(stConfigFile, tree);
      if(JSONTreeToConfig(tree, rOutConfig))
      {
         return true;
      }
   }
   catch(...)
   {

   }
   return false;
}