//
// Created by oiluz on 9/3/2017.
//

#ifndef GUACAMOLE_GUACCONFIG_H
#define GUACAMOLE_GUACCONFIG_H

#include <string>
#include <boost/log/trivial.hpp>
#include <boost/algorithm/string.hpp>

class GuacConfig
{
private:
   std::string m_stBindHost;
   short m_sBindPort;
   bool m_bWithSSL;
   bool m_bWithConsoleLog, m_bWithFileLog;
   boost::log::trivial::severity_level m_eMaxLogLevel;
   std::string m_stGuacServiceClientProcessPath;
   std::string m_stGuacProtocolsLibrariesFolder;
   std::string m_stGuacLogOutputFolder;
   std::string m_stSSLPEMFilePath;
   std::string m_stSSLCertFilePath;
   std::string m_stSSLDiffieHellmanPEMFilePath;
   short m_sFPS;

public:
   /**
    * Constructor
    */
   GuacConfig();
   /**
    * Default Destructor
    */
   virtual ~GuacConfig() = default;
   /**
    * Resets the config to default
    */
   void ResetToDefault();
   /**
    * Setter for SSL
    * @param bWithSSL
    */
   void SetWithSSL(bool bWithSSL);
   /**
    * Setter for if with console log output
    * @param bWithConsoleLog
    */
   void SetWithConsoleLog(bool bWithConsoleLog);
   /**
    * Setter for if with file log output
    * @param bWithFileLog
    */
   void SetWithFileLog(bool bWithFileLog);
   /**
    * Setter for bind host, if empty will bind to 0.0.0.0
    * @param stHost
    */
   void SetBindHost(const std::string & stHost);
   /**
    * Setter for port
    * @param sPort
    */
   void SetBindPort(short sPort);
   /**
    * Setter for the max log level shown
    * @param eLogLevel
    */
   void SetMaxLogLevel(boost::log::trivial::severity_level eLogLevel);
   /**
    * Setter for the max log level by string
    */
   void SetMaxLogLevel(const std::string stLogLevel);
   /**
    * Settr for the client child process path
    * @param stPath
    */
   void SetGuacServiceClientProcessPath(const std::string & stPath);
   /**
    * Setter for the guac protocols libraries path to find the protocols
    * @param stPath
    */
   void SetGuacProtocolsLibrariesFolder(const std::string & stPath);
   /**
    * Setter for output log folder
    * @param stPath
    */
   void SetGuacLogOutputFolder(const std::string & stPath);
   /**
    * Setter for SSL PEM file
    * @param stPath
    */
   void SetSSLPEMFilePath(const std::string & stPath);
   /**
   * Setter for SSL Cert file
   * @param stPath
   */
   void SetSSLCertFilePath(const std::string & stPath);
   /**
    * Setter for SSL DH PEM file which will be used to generate keys on communication
    * @param stPath
    */
   void SetSSLDiffieHellmanPEMFilePath(const std::string & stPath);
   /**
    * Setter for the FPS of the RDP Frames
    * @param sFPS
    */
   void SetFPS(short sFPS);
   /**
    * Getter for SSL
    * @return
    */
   bool IsWithSSL() const;
   /**
    * Getter for if with console log output
    * @return
    */
   bool IsWithConsoleLog() const;
   /**
    * Getter for if with file log output
    * @return
    */
   bool IsWithFileLog() const;
   /**
    * Getter for host name
    * @return
    */
   std::string GetBindHost() const;
   /**
    * Getter for bind port
    * @return
    */
   short GetBindPort() const;
   /**
    * Getter for the child process exec path
    * @return
    */
   std::string GetGuacServiceClientProcessPath() const;
   /**
    * Getter for the guac protocols libraries folder path
    * @return
    */
   std::string GetGuacProtocolsLibrariesFolder() const;
   /**
    * Getter for the log output folder
    * @return
    */
   std::string GetGuacLogOutputFolder() const;
   /**
    * Getter for the SSL PEM file path
    * @return
    */
   std::string GetSSLPEMFilePath() const;
   /**
   * Getter for the SSL Cert file path
   * @return
   */
   std::string GetSSLCertFilePath() const;
   /**
    * Getter for the SSL DH PEM File path which will be used to generate keys on SSL communication
    * @return
    */
   std::string GetSSLDiffieHellmanPEMFilePath() const;
   /**
    * Getter for the max log level
    * @return
    */
   boost::log::trivial::severity_level GetMaxLogLevel() const;
   /**
    * Getter for the RDP FPS
    * @return
    */
   short GetFPS() const;
};

#endif //GUACAMOLE_GUACCONFIG_H
