#include "X3DXMLLoader.h"
#include "X3DNodeHandler.h"
#include "X3DTypes.h"
#include "X3DSwitch.h"
#include "X3DXMLAttributes.h"
#include "X3DParseException.h"

#include <xercesc/util/XMLString.hpp>
#include <xercesc/sax2/DefaultHandler.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/util/PlatformUtils.hpp>

#include <iostream>
#include <cassert>

using namespace std;
XERCES_CPP_NAMESPACE_USE

namespace X3D {

class X3DXMLContentHandler;

class XMLParserImpl 
{     
public:
	XMLParserImpl() : _handler(NULL), _parser(NULL) {};
  
  XERCES_CPP_NAMESPACE_QUALIFIER SAX2XMLReader* _parser;
  X3DXMLContentHandler *_handler;
};

class X3DXMLContentHandler : public DefaultHandler
{
public: 
  X3DXMLContentHandler(X3DNodeHandler* nodeHandler);
  ~X3DXMLContentHandler();
  
  void startDocument();
  void startElement(const XMLCh* const uri, const XMLCh* const localname, const XMLCh* const qname, const XERCES_CPP_NAMESPACE_QUALIFIER Attributes &attrs);
  void endElement(const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname);  
  void endDocument();
  void fatalError(const XERCES_CPP_NAMESPACE_QUALIFIER SAXParseException& exception);
  void error(const XERCES_CPP_NAMESPACE_QUALIFIER SAXParseException& exception);
  
private:
  X3DNodeHandler* _nodeHandler;
  X3DSwitch _switch;
  int _skipCount;
};

X3DXMLContentHandler::X3DXMLContentHandler(X3DNodeHandler* nodeHandler) : _nodeHandler(nodeHandler), _skipCount(0)
{
	_switch.setNodeHandler(nodeHandler);
}

X3DXMLContentHandler::~X3DXMLContentHandler()
{
}

void X3DXMLContentHandler::startDocument()
{
	_nodeHandler->startDocument();
}

void X3DXMLContentHandler::startElement(const XMLCh* const, const XMLCh* const, const XMLCh* const qname, const Attributes &attrs)
{
	if (_skipCount != 0)
	{
		_skipCount++;
		return;
	}
	char* nodeName = XMLString::transcode(qname);
	int id = X3D::X3DTypes::getElementID(nodeName);
	X3DXMLAttributes xmlAttributes(&attrs);
	int state = _switch.doStartElement(id, xmlAttributes);
	if (state == X3D::SKIP_CHILDREN)
		_skipCount = 1;
	XMLString::release(&nodeName);

}

void X3DXMLContentHandler::endElement(const XMLCh *const, const XMLCh *const, const XMLCh *const qname)
{
	if (_skipCount != 0)
	{
		_skipCount--;
		return;
	}
	char* nodeName = XMLString::transcode(qname);
	int id = X3D::X3DTypes::getElementID(nodeName);
	_switch.doEndElement(id, nodeName);
	XMLString::release(&nodeName);

}

void X3DXMLContentHandler::endDocument()
{
	_nodeHandler->endDocument();
}

void X3DXMLContentHandler::error(const SAXParseException& exception)
{
  char* message = XMLString::transcode(exception.getMessage());
  cerr << "XercesLoader::error: " << message << " at line: " << exception.getLineNumber() << endl;
}

void X3DXMLContentHandler::fatalError(const SAXParseException& exception)
{
  char* message = XMLString::transcode(exception.getMessage());
  throw X3DParseException(message, static_cast<int>(exception.getLineNumber()), static_cast<int>(exception.getColumnNumber()));
}

X3DXMLLoader::X3DXMLLoader()
{
  _impl = new XMLParserImpl();
  try 
  {
    XMLPlatformUtils::Initialize();
  }
  catch (const XMLException& toCatch) 
  {
    char* message = XMLString::transcode(toCatch.getMessage());
    cerr << "XercesLoader::Error during initialization: " << message << endl;
    XMLString::release(&message);
  }
  
  _impl->_parser = XMLReaderFactory::createXMLReader();
  _impl->_parser->setFeature(XMLUni::fgSAX2CoreNameSpaces, true);
  _impl->_parser->setFeature(XMLUni::fgXercesSchemaFullChecking, false);
  _impl->_parser->setFeature(XMLUni::fgSAX2CoreNameSpacePrefixes, false);
  

}

X3DXMLLoader::~X3DXMLLoader()
{
  delete _impl->_parser;
  if (_impl->_handler)
	  delete _impl->_handler;
  delete _impl;
  XMLPlatformUtils::Terminate();

}

bool X3DXMLLoader::load(std::string fileName, bool fileValidation) const
{
	assert(_handler);


  _impl->_handler = new X3DXMLContentHandler(_handler);
  _impl->_parser->setContentHandler(_impl->_handler);
  _impl->_parser->setErrorHandler(_impl->_handler);
  _impl->_parser->setDTDHandler(_impl->_handler);

  if (fileValidation)
  {
    _impl->_parser->setFeature(XMLUni::fgXercesLoadExternalDTD, true);
    _impl->_parser->setFeature(XMLUni::fgXercesSchema, true);
    _impl->_parser->setFeature(XMLUni::fgSAX2CoreValidation, true);
  }
  else
  {
    _impl->_parser->setFeature(XMLUni::fgXercesLoadExternalDTD, false);
    _impl->_parser->setFeature(XMLUni::fgXercesSchema, false);
    _impl->_parser->setFeature(XMLUni::fgSAX2CoreValidation, false);
  }  
      
  try 
  {
	  _impl->_parser->parse(fileName.c_str());
  }
  catch (SAXException& e) 
  {
	  cerr << "X3DXMLLoader::load: internal error: " << e.getMessage() << endl;
	  return false;
  }
  return true;
  
  
}



}
