//
// Created by oiluz on 9/3/2017.
//

#include <guacservice/GuacConfig.h>

GuacConfig::GuacConfig()
{
   ResetToDefault();
}

void GuacConfig::ResetToDefault()
{
   m_eMaxLogLevel = boost::log::trivial::debug;
   m_sBindPort = 4822;
   m_stBindHost = "";
   m_stGuacServiceClientProcessPath = "guacservice_client.exe";
   m_stGuacProtocolsLibrariesFolder = "./";
   m_stGuacLogOutputFolder = "./";
   m_bWithSSL = false;
   m_bWithConsoleLog = true;
   m_bWithFileLog = true;
   m_stSSLPEMFilePath = "./key.pem";
   m_stSSLCertFilePath = "./cert.crt";
   m_stSSLDiffieHellmanPEMFilePath = "./dh512.pem";
   m_sFPS = 30;
}

void GuacConfig::SetWithSSL(bool bWithSSL)
{
   m_bWithSSL = bWithSSL;
}

void GuacConfig::SetWithConsoleLog(bool bWithConsoleLog)
{
   m_bWithConsoleLog = bWithConsoleLog;
}

void GuacConfig::SetWithFileLog(bool bWithFileLog)
{
   m_bWithFileLog = bWithFileLog;
}

void GuacConfig::SetBindHost(const std::string & stHost)
{
   m_stBindHost = stHost;
}

void GuacConfig::SetBindPort(short sPort)
{
   m_sBindPort = sPort;
}

void GuacConfig::SetMaxLogLevel(boost::log::trivial::severity_level eLogLevel)
{
   m_eMaxLogLevel = eLogLevel;
}

void GuacConfig::SetMaxLogLevel(const std::string stLogLevel)
{
   std::string level = boost::algorithm::to_lower_copy(stLogLevel);
   if(level == "trace")
   {
      SetMaxLogLevel(boost::log::trivial::severity_level::trace);
   }
   else if(level == "debug")
   {
      SetMaxLogLevel(boost::log::trivial::severity_level::debug);
   }
   else if(level == "info")
   {
      SetMaxLogLevel(boost::log::trivial::severity_level::info);
   }
   else if(level == "warning")
   {
      SetMaxLogLevel(boost::log::trivial::severity_level::warning);
   }
   else if(level == "error")
   {
      SetMaxLogLevel(boost::log::trivial::severity_level::error);
   }
   else if(level == "fatal")
   {
      SetMaxLogLevel(boost::log::trivial::severity_level::fatal);
   }
}

void GuacConfig::SetGuacServiceClientProcessPath(const std::string & stPath)
{
   m_stGuacServiceClientProcessPath = stPath;
}

void GuacConfig::SetGuacProtocolsLibrariesFolder(const std::string & stPath)
{
   m_stGuacProtocolsLibrariesFolder = stPath;
}

void GuacConfig::SetGuacLogOutputFolder(const std::string & stPath)
{
   m_stGuacLogOutputFolder = stPath;
}

void GuacConfig::SetSSLPEMFilePath(const std::string & stPath)
{
   m_stSSLPEMFilePath = stPath;
}

void GuacConfig::SetSSLCertFilePath(const std::string & stPath)
{
   m_stSSLCertFilePath = stPath;
}

void GuacConfig::SetSSLDiffieHellmanPEMFilePath(const std::string & stPath)
{
   m_stSSLDiffieHellmanPEMFilePath = stPath;
}

void GuacConfig::SetFPS(short sFPS)
{
   m_sFPS = sFPS;
}

bool GuacConfig::IsWithSSL() const
{
   return m_bWithSSL;
}

bool GuacConfig::IsWithConsoleLog() const
{
   return m_bWithConsoleLog;
}

bool GuacConfig::IsWithFileLog() const
{
   return m_bWithFileLog;
}

std::string GuacConfig::GetBindHost() const
{
   return m_stBindHost;
}

short GuacConfig::GetBindPort() const
{
   return m_sBindPort;
}

std::string GuacConfig::GetGuacServiceClientProcessPath() const
{
   return m_stGuacServiceClientProcessPath;
}

std::string GuacConfig::GetGuacProtocolsLibrariesFolder() const
{
   return m_stGuacProtocolsLibrariesFolder;
}

std::string GuacConfig::GetGuacLogOutputFolder() const
{
   return m_stGuacLogOutputFolder;
}

std::string GuacConfig::GetSSLPEMFilePath() const
{
   return m_stSSLPEMFilePath;
}

std::string GuacConfig::GetSSLCertFilePath() const
{
   return m_stSSLCertFilePath;
}

std::string GuacConfig::GetSSLDiffieHellmanPEMFilePath() const
{
   return m_stSSLDiffieHellmanPEMFilePath;
}

boost::log::trivial::severity_level GuacConfig::GetMaxLogLevel() const
{
   return m_eMaxLogLevel;
}

short GuacConfig::GetFPS() const
{
   return m_sFPS;
}
