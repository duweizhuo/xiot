#include "Argument_helper.h"
#include <iostream>
#include <string>
#include <fstream>
#include "X3DLoader.h"
#include "X3DDefaultNodeHandler.h"
#include "X3DAttributes.h"
#include "X3DParseException.h"


using namespace std;

string input_filename;
string output_filename;

class MyShapeCounter : public X3D::X3DDefaultNodeHandler {
	
  public:
    MyShapeCounter() { };

    virtual void startDocument() {
      _count = 0;
    }

    virtual void endDocument() {
      std::cout << "Found " << _count << " shape nodes in file";
      std::cout << std::endl;
    }

    virtual int startShape(const X3D::X3DAttributes &attr)
    {
      _count++;
	  return X3D::CONTINUE;
    }

  protected:
    int _count;  
};

int start(const string &input_filename, const string &output_filename)
{
	X3D::X3DLoader loader;
	MyShapeCounter* handler = new MyShapeCounter();
	loader.setNodeHandler(handler);
	try {
		loader.load(input_filename);
	} catch (X3D::X3DParseException& e)
	{	
	  cerr << "Error while parsing file " << input_filename << ":" << endl;
      cerr << e.getMessage() << " (Line: " << e.getLineNumber() << ", Column: " << e.getColumnNumber() << ")" << endl;
      return 1;
	}
	delete handler;
	return 0;
}

bool fileExists(const std::string& fileName)
{
  std::fstream fin;
  fin.open(fileName.c_str(),std::ios::in);
  if( fin.is_open() )
  {
    fin.close();
    return true;
  }
  fin.close();
  return false;
}

int main(int argc, char *argv[])
{
  dsr::Argument_helper ah;

  ah.new_string("input_filename.x3d", "The name of the input file", input_filename);
  //ah.new_string("output_filename", "The name of the output file", output_filename);
  
  //ARGUMENT_HELPER_BASICS(ah);
  ah.set_description("A simple test application for the X3DLoader");
  ah.set_author("Kristian Sons, kristian.sons@actor3d.com");
  ah.set_version(0.9f);
  ah.set_build_date(__DATE__);

  ah.process(argc, argv);


  // Check output string
  if (fileExists(input_filename))
  {
	  return start(input_filename, output_filename);
  }
  
  cerr << "Input file not found or not readable: " << input_filename << endl;
  return 1;
}
