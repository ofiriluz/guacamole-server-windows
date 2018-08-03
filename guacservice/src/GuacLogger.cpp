//
// Created by oiluz on 9/5/2017.
//

#include <guacservice/GuacLogger.h>

GuacLogger::GuacLogger() : m_Initialized(false), m_IgnoreLogs(false), m_WithConsole(false)
{

}

boost::shared_ptr<GuacLogger> GuacLogger::GetInstance()
{
   static boost::shared_ptr<GuacLogger> logger(new GuacLogger());
   return logger;
}

void GuacLogger::SetConsoleColorBySeverity(boost::log::trivial::severity_level eLevel)
{
#ifdef WIN32
	WORD color;
	switch (eLevel)
	{
		case boost::log::trivial::trace: 
			color = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
			break;
		case boost::log::trivial::debug: 
			color = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
			break;
		case boost::log::trivial::info: 
			color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
			break;
		case boost::log::trivial::warning: 
			color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
			break;
		case boost::log::trivial::error: 
			color = FOREGROUND_RED | FOREGROUND_INTENSITY;
			break;
		case boost::log::trivial::fatal: 
			color = FOREGROUND_RED | FOREGROUND_INTENSITY;
			break;
		default: 
			color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
			break;
	}
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleScreenBufferInfo(hConsole, &m_ConsoleInfo);
	SetConsoleTextAttribute(hConsole, color);
#elif defined __linux__
	switch (eLevel)
	{
		case boost::log::trivial::trace:
		case boost::log::trivial::debug:
			std::cout << "\e[39m";
		case boost::log::trivial::info:
			std::cout << "\e[34m";
			break;
		case boost::log::trivial::warning:
			std::cout << "\e[33m";
			break;
		case boost::log::trivial::error:
		case boost::log::trivial::fatal:
			std::cout << "\e[31m";
			break;
		default:
			break;
	}
#endif
}

void GuacLogger::ClearConsoleColor()
{
#ifdef WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, m_ConsoleInfo.wAttributes);
#elif defined __linux__
	std::cout << "\e[39m";
#endif
}

void GuacLogger::InitializeLog(const std::string & stLogFolder, const std::string & stLogFilePrefix,
                               boost::log::trivial::severity_level eMaxSeverityLevel, bool bWithConsoleOutput,
                               bool bWithFileOutput)
{
	try
	{
		// Create log folder if does not exist and with file output
		if (!boost::filesystem::exists(boost::filesystem::path(stLogFolder)) && bWithFileOutput)
		{
			boost::system::error_code ec;
			boost::filesystem::create_directories(boost::filesystem::path(stLogFolder), ec);
			if (ec)
			{
				bWithFileOutput = false;
			}
		}

		// Add common attributes to the log and set its max log level
		boost::log::add_common_attributes();
		boost::log::core::get()->set_filter(boost::log::trivial::severity >= eMaxSeverityLevel);

		// Create the formatters
		auto fmtTimeStamp = boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp",
			"%Y-%m-%d %H:%M:%S.%f");
		auto fmtProcessId = boost::log::expressions::attr<boost::log::attributes::current_process_id::value_type>(
			"ProcessID");
		auto fmtThreadId = boost::log::expressions::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID");
		auto fmtSeverity = boost::log::expressions::attr<boost::log::trivial::severity_level>("Severity");

		boost::log::formatter logFmt =
			boost::log::expressions::format("[%1%] [%2%(%3%)] [%4%] => %5%")
			% fmtTimeStamp % fmtProcessId % fmtThreadId % fmtSeverity
			% boost::log::expressions::smessage;

		// Add console sink if needed
		if (bWithConsoleOutput)
		{
			auto consoleSink = boost::log::add_console_log(std::clog);
			consoleSink->set_formatter(logFmt);
		}

		// Add File sink
		if (bWithFileOutput)
		{
			auto fsSink = boost::log::add_file_log(
				boost::log::keywords::file_name = stLogFolder + "/" + stLogFilePrefix + "_%Y-%m-%d_%H-%M-%S.%N.log",
				boost::log::keywords::rotation_size = 10 * 1024 * 1024,
				boost::log::keywords::min_free_space = 30 * 1024 * 1024,
				boost::log::keywords::open_mode = std::ios_base::app,
				boost::log::keywords::auto_flush = true);
			fsSink->set_formatter(logFmt);
		}

		m_Initialized = true;
		m_WithConsole = bWithConsoleOutput;
		m_IgnoreLogs = !(bWithConsoleOutput || bWithFileOutput);
	}
	catch (...)
	{
		m_Initialized = false;
	}
}

void GuacLogger::LogMessage(const std::string & stMessage, boost::log::trivial::severity_level eLevel)
{
   if(!m_Initialized || m_IgnoreLogs)
   {
      return;
   }

   try
   {
	  // Set the console color if with console
	  if (m_WithConsole)
	  {
	     SetConsoleColorBySeverity(eLevel);
	  }

      // Write log depending on the severity type
      switch(eLevel)
      {
         case boost::log::trivial::trace:
            BOOST_LOG_TRIVIAL(trace) << stMessage;
            break;
         case boost::log::trivial::debug:
            BOOST_LOG_TRIVIAL(debug) << stMessage;
            break;
         case boost::log::trivial::info:
            BOOST_LOG_TRIVIAL(info) << stMessage;
            break;
         case boost::log::trivial::warning:
            BOOST_LOG_TRIVIAL(warning) << stMessage;
            break;
         case boost::log::trivial::error:
            BOOST_LOG_TRIVIAL(error) << stMessage;
            break;
         case boost::log::trivial::fatal:
            BOOST_LOG_TRIVIAL(fatal) << stMessage;
            break;
      }

	  // Clear the console color
	  if (m_WithConsole)
	  {
		 ClearConsoleColor();
	  }
   }
   catch(...)
   {

   }
}

GuacLog GuacLogger::Trace() const
{
   return GuacLog(boost::log::trivial::trace);
}

GuacLog GuacLogger::Debug() const
{
   return GuacLog(boost::log::trivial::debug);
}

GuacLog GuacLogger::Info() const
{
   return GuacLog(boost::log::trivial::info);
}

GuacLog GuacLogger::Warning() const
{
   return GuacLog(boost::log::trivial::warning);
}

GuacLog GuacLogger::Error() const
{
   return GuacLog(boost::log::trivial::error);
}

GuacLog GuacLogger::Fatal() const
{
   return GuacLog(boost::log::trivial::fatal);
}

GuacLog::GuacLog(boost::log::trivial::severity_level eLevel)
{
   m_eLevel = eLevel;
}

GuacLog::GuacLog(const GuacLog & rOther)
{
   m_eLevel = rOther.m_eLevel;
   m_SS << rOther.m_SS.str();
}

GuacLog::~GuacLog()
{
   GuacLogger::GetInstance()->LogMessage(m_SS.str(), m_eLevel);
}
