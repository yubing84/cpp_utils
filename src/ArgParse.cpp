#include "ArgParse.hpp"


#include <iostream>
#include <sstream>

#include <algorithm>
#include <string>
#include <set>

#include <stdexcept>

#include <cstdio> // sscan
#include <cctype>


ArgParse::ArgParse(const std::string description,
                   const std::string epilog,
                   const bool addHelp)
  : m_description(description)
  , m_epilog(epilog)
  , m_programName()
  , m_params()
{
  if (addHelp)
    addArgument("-h, --help", "Prints this help message");
}


void ArgParse::addArgument(const std::string arg,
                           const std::string help,
                           const ValueType type,
                           const ValueRequired valueRequired,
                           const std::string valueName,
                           const std::string choices)
{
  if ( arg.at(0) != '-' )
    throw std::logic_error(std::string(arg).
                               append(" shall start with a dash."));

  if ( findKeyinArgMap(arg) != m_params.end() )
    throw std::logic_error(std::string(arg).
                               append(" has been given before."));

  int i;
  if ( type == INT &&
       !choices.empty() &&
       sscanf( choices.c_str(), "%d..%d", &i, &i ) != 2 )
           throw std::logic_error(std::string( arg ).
                           append(" has syntax error. ").
                           append("Range expected in a INT..INT format" ));

  float f;
  if ( type == FLOAT &&
       !choices.empty() &&
       sscanf( choices.c_str(), "%f..%f", &f, &f ) != 2 )
           throw std::logic_error(std::string( arg ).
                           append(" has syntax error. ").
                           append("Range expected in a FLOAT..FLOAT format" ));

  Argument argument(help,
                    type,
                    valueRequired,
                    typeToString(type, valueName),
                    choices,
                    "");
  m_params.insert(std::pair<std::string, Argument>(arg, argument));
}


void ArgParse::parseArgs(int argc,
                         char* argv[])
{
  std::list<std::string> argList;
  for (int i = 0; i < argc; ++i )
    argList.push_back(argv[i]);

  parseArgs(argList);
}


void ArgParse::parseArgs(const std::list<std::string> argList)
{
  m_programName = argList.front();

  std::list<std::string>::const_iterator it = argList.begin();
  for (++it; it != argList.end(); ++it ) {

    ArgMap::iterator argMapIt = findKeyinArgMap(*it);
    if ( argMapIt == m_params.end() )
      throw std::runtime_error(std::string(*it).append(" is not known."));

    (*argMapIt).second.m_found = true;

    if ( (*argMapIt).second.m_type == NONE )
      continue;

    std::list<std::string>::const_iterator next = it;
    next++;

    if ( next == argList.end() ) {
      if ( (*argMapIt).second.m_valueRequired == REQUIRED )
        throw std::runtime_error(std::string(*it).
                                 append(" requires a parameter."));

      if ( (*argMapIt).second.m_valueRequired == OPTIONAL )
        continue;
    }

    if ( (*argMapIt).second.m_valueRequired == OPTIONAL &&
          findKeyinArgMap( *next ) != m_params.end() )
      continue;

    validateValue( (*argMapIt).second.m_type,
                   (*argMapIt).first,
                   (*argMapIt).second.m_choices,
                   *next );

    (*argMapIt).second.m_value = *next;
    (*argMapIt).second.m_valueHasBeenSet = true;
    ++it;
  }
}


void ArgParse::validateValue(const ArgParse::ValueType type,
                             const std::string name,
                             const std::string choices,
                             const std::string value) const
{
  switch ( type ) {
     case INT :
       validateInt(name, choices, value);
       break;
     case FLOAT :
       validateFloat(name, choices, value);
       break;
     case BOOL :
       validateBool(name, choices, value);
       break;
     case STRING :
       validateString(name, choices, value);
       break;
     default:
       break;
   }
}


void ArgParse::validateString( const std::string name,
                               const std::string choices,
                               const std::string value) const
{
  if ( !choices.empty() ) {

    std::set<std::string> choicesSet = choicesStringToSet( choices );

    if ( choicesSet.find( value ) == choicesSet.end() )
      throw std::runtime_error(std::string( value ).
            append(" is not in the expected list of choices: {").
            append( choices ).
            append("}, required by ").
            append( name ));
  }
}


void ArgParse::validateInt( const std::string name,
                            const std::string choices,
                            const std::string value) const
{
  int temp;
  if ( sscanf( value.c_str(), "%d", &temp ) == 0 )
    throw std::runtime_error(std::string( value ).
            append(" is not an integer, required by ").append( name ));

  if ( !choices.empty() ) {
    int lowerBound;
    int upperBound;
    sscanf( choices.c_str(), "%d..%d", &lowerBound, &upperBound );

    if ( temp < lowerBound || temp > upperBound )
      throw std::runtime_error(std::string( name ).
                    append( " expects an integer in the range of {" ).
                    append( choices).
                    append("}") );
  }
}


void ArgParse::validateFloat( const std::string name,
                              const std::string choices,
                              const std::string value) const
{
  float temp;
  if ( sscanf( value.c_str(), "%f", &temp ) == 0 )
    throw std::runtime_error(std::string( value ).
            append(" is not a float, required by ").append( name ));

  if ( !choices.empty() ) {
    float lowerBound;
    float upperBound;
    sscanf( choices.c_str(), "%f..%f", &lowerBound, &upperBound );

    if ( temp < lowerBound || temp > upperBound )
      throw std::runtime_error(std::string( name ).
                     append( " expects a float in the range of [" ).
                     append( choices).
                     append("}") );
  }
}


void ArgParse::validateBool( const std::string name,
                             const std::string choices,
                             const std::string value) const
{
  std::string temp = value;
  std::transform(temp.begin(), temp.end(),temp.begin(), ::toupper);
  if ( temp != "TRUE" && temp != "FALSE" )
    throw std::runtime_error(std::string( value ).
            append(" is not a boolean, required by ").append( name ));

  if ( !choices.empty() )
    throw std::logic_error(std::string( value ).
            append(" expects a boolean not choices."));
}


bool ArgParse::isArg(const std::string arg) const
{
  ArgMap::const_iterator it = m_params.find(arg);
  return it != m_params.end();
}


bool ArgParse::foundArg(const std::string arg) const
{
  ArgMap::const_iterator it = m_params.find(arg);
  return it != m_params.end() && (*it).second.m_found == true;
}

bool ArgParse::argHasValue(const std::string arg) const
{
  ArgMap::const_iterator it = m_params.find(arg);
  return it != m_params.end() &&
         (*it).second.m_found == true &&
         (*it).second.m_valueHasBeenSet;
}


bool ArgParse::argAsString(const std::string arg, std::string &value) const
{
  if ( !argHasValue(arg) )
    return false;

  ArgMap::const_iterator it = m_params.find(arg);
  value = (*it).second.m_value;
  return true;
}


bool ArgParse::argAsInt(const std::string arg, int &value) const
{
  if ( !argHasValue(arg) )
    return false;

  ArgMap::const_iterator it = m_params.find(arg);
  value = atoi((*it).second.m_value.c_str());
  return true;
}


bool ArgParse::argAsFloat(const std::string arg, float &value) const
{
  if ( !argHasValue(arg) )
    return false;

  ArgMap::const_iterator it = m_params.find(arg);
  value = atof((*it).second.m_value.c_str());
  return true;
}


bool ArgParse::argAsBool(const std::string arg, bool &value) const
{
  if ( !argHasValue(arg) )
    return false;

  ArgMap::const_iterator it = m_params.find(arg);

  std::string temp = (*it).second.m_value;
  std::transform(temp.begin(), temp.end(),temp.begin(), ::toupper);

  value = temp == "TRUE";
  return true;
}


std::string ArgParse::usage() const
{
  std::stringstream ss;

  ss << m_description << std::endl << std::endl;

  ss << "usage: " << m_programName;
  if (!m_params.empty()) {
    ss << " [OPTION]" << std::endl << std::endl;
    ss << "Options:" << std::endl;
    int length;
    ArgMap::const_iterator it;
    for ( it = m_params.begin(); it != m_params.end(); it++ ) {
      ss << (*it).first;
      length = (*it).first.length();
      if ( (*it).second.m_type != NONE ) {

        ss << " ";
        length++;

        if ( (*it).second.m_valueRequired == OPTIONAL ) {
          ss << "[";
          length ++;
        }

        if ( !(*it).second.m_choices.empty() ) {
          ss << "{" << (*it).second.m_choices << "}";
          length += (*it).second.m_choices.length() + 2;
        } else {
          ss << (*it).second.m_valueName;
          length += (*it).second.m_valueName.length();
        }

        if ( (*it).second.m_valueRequired == OPTIONAL ) {
          ss << "]";
          length++;
        }
      }
      ss << std::string(30-length, ' ');
      ss << (*it).second.m_help << std::endl;
    }
  }
  ss << std::endl;
  ss << m_epilog << std::endl;

  return ss.str();;
}


ArgParse::Argument::Argument (const std::string help,
                              const enum ValueType type,
                              const enum ValueRequired valueRequired,
                              const std::string valueName,
                              const std::string choices,
                              const std::string value)
  : m_help(help)
  , m_type(type)
  , m_valueRequired(valueRequired)
  , m_valueName(valueName)
  , m_choices(choices)
  , m_value(value)
  , m_found(false)
  , m_valueHasBeenSet(false)
{

}


bool
ArgParse::argCompare::operator()(const std::string a,const std::string b) const
{
  if ( a.at(1) == '-' && b.at(1) != '-' )
    return a.substr(1) < b;

  if ( b.at(1) == '-' && a.at(1) != '-' )
    return a < b.substr(1);

  return a<b;
}


std::map<std::string, ArgParse::Argument>::iterator
ArgParse::findKeyinArgMap(const std::string param)
{
  ArgMap::iterator it;
  for( it = m_params.begin(); it != m_params.end(); ++it) {

    // if it's the short param at the beginning
    if ( (*it).first.find(param) == 0 )
      return it;

    // or is it the long after the comma?
    size_t commaPos = (*it).first.find(",");
    if ( commaPos != std::string::npos &&
         (*it).first.find( param, commaPos+1 ) != std::string::npos )
      return it;
  }

  return m_params.end();
}

std::set<std::string>
ArgParse::choicesStringToSet(const std::string s) const
{
  std::string tmp(s);
  std::set<std::string> stringSet;

  size_t pos;
  std::string element;

  while ( !tmp.empty() ) {
    pos = tmp.find(',');
    if (pos == std::string::npos) {
      element = tmp;
      tmp = "";
    } else {
      element = tmp.substr(0,pos);
      tmp = tmp.substr(pos+1);
    }

    // if ( element.empty() ) ... ?

    if ( stringSet.find(element) != stringSet.end() ) {
      throw std::logic_error( std::string( element ).
                              append(" listed twice in ").append(s) );
    }

    stringSet.insert(element);
  }

  return stringSet;
}


std::string
ArgParse::typeToString(const ValueType type, std::string valueName) const
{
  if ( !valueName.empty() )
    return valueName;

  switch ( type ) {
    case NONE :
      return "NONE";
    case STRING :
      return "STRING";
    case INT :
      return "INT";
    case FLOAT :
      return "DOUBLE";
    case BOOL :
       return "BOOL";
    default:
      return "";
  }
}