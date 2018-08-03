//
// Created by oiluz on 9/4/2017.
//

#include <guacservice/GuacService.h>
#include <guacservice/GuacConfigParser.h>

int main(int argc, char ** argv)
{
   GuacConfig config;

   if(argc == 3)
   {
      if(std::string(argv[1]) == "--config" || std::string(argv[1]) == "-c")
      {
         GuacConfigParser parser;
         parser.FileToConfig(std::string(argv[2]), config);
      }
   }
   else
   {
      std::cout << "No Config File Path Supplied" << std::endl;
      std::cout << "Usage: " << std::endl;
      std::cout << "guacservice.exe --config path/to/config.json" << std::endl;
      std::cout << "Or" << std::endl;
      std::cout << "guacservice.exe -c path/to/config.json" << std::endl;
      std::cout << "Change the path to config to the actual path" << std::endl;
      return 1;
   }

   GuacService service;
   service.StartGuacamoleService(config);

   return 0;
}