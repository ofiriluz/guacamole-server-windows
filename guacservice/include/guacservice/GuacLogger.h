//
// Created by oiluz on 9/5/2017.
//

#ifndef GUACAMOLE_GUACLOGGER_H
#define GUACAMOLE_GUACLOGGER_H

#pragma warning(push)
#pragma warning(disable:4819)

#include <boost/filesystem.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/support/date_time.hpp>
#include <windows.h>

#pragma warning(pop)
#pragma warning(disable:4503)

class GuacLog
{
private:
   std::stringstream m_SS;
   boost::log::trivial::severity_level m_eLevel;

public:
   /**
    * Constructor
    * @param eLevel
    */
   GuacLog(boost::log::trivial::severity_level eLevel);
   /**
    * Copy Constructor
    * @param rOther
    */
   GuacLog(const GuacLog & rOther);
   /**
    * Destructor, writes the stream to the logger
    */
   virtual ~GuacLog();

   /**
    * Stream operator for a template, writes to the stringstream
    * @tparam T
    * @param stMessage
    * @return
    */
   template<class T>
   GuacLog & operator<<(const T & stMessage)
   {
      m_SS << stMessage;
      return *this;
   }
};

class GuacLogger
{
private:
   std::string m_stLogFile;
   bool m_Initialized;
   bool m_IgnoreLogs;
   bool m_WithConsole;
#ifdef WIN32
   CONSOLE_SCREEN_BUFFER_INFO m_ConsoleInfo;
#endif

private:
   /**
    * Private constructor for singleton
    */
   GuacLogger();
   /**
    * Deleted for singleton
    * @param other
    */
   GuacLogger(const GuacLogger & rOther) = delete;
   /**
    * Deleted for singleton
    * @param other
    */
   GuacLogger(GuacLogger && rOther) = delete;
   /**
    * Deleted for singleton
    * @param other
    * @return
    */
   GuacLogger operator=(const GuacLogger & rOther) = delete;
   /**
    * Deleted for singleton
    * @param other
    * @return
    */
   GuacLogger operator=(GuacLogger && rOther) = delete;
   /**
    * Sets the consoloe color by the given severity level
	* @param eLevel
    */
   void SetConsoleColorBySeverity(boost::log::trivial::severity_level eLevel);
   /**
    * Clears the console color back to normal
    */
   void ClearConsoleColor();

public:
   /**
    * Default Destructor
    */
   virtual ~GuacLogger() = default;
   /**
    * Singleton getter
    * @return
    */
   static boost::shared_ptr<GuacLogger> GetInstance();
   /**
    * Initializes the log and all the relevent sinks with its format
    * @param stLogFolder
    * @param stLogFilePrefix
    * @param eMaxSeverityLevel
    * @param bWithConsoleOutput
    */
   void InitializeLog(const std::string & stLogFolder, const std::string & stLogFilePrefix,
                      boost::log::trivial::severity_level eMaxSeverityLevel,
                      bool bWithConsoleOutput = false, bool bWithFileOutput = true);
   /**
    * Logs a given message with the severity
    * @param stMessage
    * @param eLevel
    */
   void LogMessage(const std::string & stMessage, boost::log::trivial::severity_level eLevel);
   /**
    * Getter for a Trace GuacLog
    * @return
    */
   GuacLog Trace() const;
   /**
    * Getter for a Debug GuacLog
    * @return
    */
   GuacLog Debug() const;
   /**
    * Getter for a Info GuacLog
    * @return
    */
   GuacLog Info() const;
   /**
    * Getter for a Warning GuacLog
    * @return
    */
   GuacLog Warning() const;
   /**
    * Getter for a Error GuacLog
    * @return
    */
   GuacLog Error() const;
   /**
    * Getter for a Fatal GuacLog
    * @return
    */
   GuacLog Fatal() const;
};

#endif //GUACAMOLE_GUACLOGGER_H
