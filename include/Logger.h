#ifndef LOGGER_H
#define LOGGER_H

#ifndef MODULE_EXPORT
   #if defined(_WIN32) || defined(_WIN_64)
      #ifdef SpikeNN_EXPORTS
         #define MODULE_EXPORT  __declspec(dllexport)   // export DLL information
      #else
         #define MODULE_EXPORT  __declspec(dllimport)   // import DLL information
      #endif
   #else
      #define MODULE_EXPORT
   #endif
#endif

#include <fstream>
#include <string>

class MODULE_EXPORT Logger
{
public:
   Logger(){}
   Logger(std::string filename) {set(filename);}
   ~Logger() {mStream.close();}

   void set(std::string filename);
   void writeLine(std::string line) {if (!mStream.is_open()) mStream.open(mFileName.c_str()); mStream << line << std::endl; mStream.flush();}
   void write(std::string str) {if (!mStream.is_open()) mStream.open(mFileName.c_str()); mStream << str; mStream.flush();}
   
   static std::string toString(float input);

private:
   std::string   mFileName;
   std::ofstream mStream;
};

#endif