#include "vtkX3DImporter.h"
#include "vtkObjectFactory.h"

#include "X3DLoader.h"
#include "vtkX3DNodeHandler.h"
#include "X3DAttributes.h"
#include "X3DParseException.h"

vtkCxxRevisionMacro(vtkX3DImporter, "$Revision: 1.19 $");
vtkStandardNewMacro(vtkX3DImporter);

vtkX3DImporter::vtkX3DImporter()
{
	this->FileName = NULL;
	this->Verbose = 0;
}

vtkX3DImporter::~vtkX3DImporter()
{
	if (this->FileName)
    {
    delete [] this->FileName;
    }
}

int vtkX3DImporter::ImportBegin()
{
	X3D::X3DLoader loader;
	X3D::X3DTypes::initMaps();
	vtkX3DNodeHandler handler(this->Renderer);
	loader.setNodeHandler(&handler);
	handler.setVerbose(this->Verbose == 1);
	try {
		if (!loader.load(this->FileName))
			return 0;
	} 
	catch (X3D::X3DParseException& e)
	{	
	  vtkErrorMacro( << "Error while parsing file " << this->FileName << ":" << e.getMessage().c_str()
					  << " (Line: " << e.getLineNumber() << ", Column: " << e.getColumnNumber() << ")");
      return 0;
	}
	
	return 1;

}

void vtkX3DImporter::ImportEnd ()
{
}

void vtkX3DImporter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  os << indent << "File Name: " 
     << (this->FileName ? this->FileName : "(none)") << "\n";
}

