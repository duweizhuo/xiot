#include <cassert>
#include <map>

#include "vtkX3DNodeHandler.h"
#include "X3DAttributes.h"
#include "X3DTypes.h"

#include "vtkActor.h"
#include "vtkByteSwap.h"
#include "vtkCamera.h"
#include "vtkCellArray.h"
#include "vtkConeSource.h"
#include "vtkCubeSource.h"
#include "vtkCylinderSource.h"
#include "vtkFloatArray.h"
#include "vtkIdTypeArray.h"
#include "vtkLight.h"
#include "vtkLookupTable.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"
#include "vtkPolyDataMapper.h"
#include "vtkPolyDataNormals.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkSphereSource.h"
#include "vtkStripper.h"
#include "vtkSystemIncludes.h"
#include "vtkTransform.h"
#include "vtkX3DIndexedFaceSetSource.h"
#include "vtkPolyDataNormals.h"

using namespace std;

// Hardcoded conversion from radians to degrees: necessary as XML uses radians while VTK expects degrees
#define RAD_TO_DEG 57.29577951f

vtkX3DNodeHandler::vtkX3DNodeHandler(vtkRenderer* Renderer)
{
	this->Renderer = Renderer;

	this->CurrentActor = NULL;
	this->CurrentLight = NULL;
	this->CurrentProperty = NULL;
	this->CurrentCamera = NULL;
	this->CurrentSource = NULL;
	this->CurrentPoints = NULL;
	this->CurrentScalars = NULL;
	this->CurrentNormals = NULL;
	this->CurrentTCoords = NULL;
	this->CurrentTCoordCells = NULL;
	this->CurrentMapper = NULL;
	this->CurrentLut = NULL;
	this->CurrentIndexedFaceSet = NULL;
	this->CurrentColors = NULL;
	this->CurrentTransform = vtkTransform::New();
	
	_ignoreNodes.insert(X3D::X3D);
	_ignoreNodes.insert(X3D::Scene);
	_ignoreNodes.insert(X3D::StaticGroup);
	_ignoreNodes.insert(X3D::Group);

	bDEFCoordinate = false;
	bDEFTextureCoordinate = false;
	bDEFNormal = false;
	bDEFColor = false;
	_verbose = false;
}

vtkX3DNodeHandler::~vtkX3DNodeHandler()
{
	if (this->CurrentActor)
    {
    this->CurrentActor->Delete();
    }
  if (this->CurrentLight)
    {
    this->CurrentLight->Delete();
    }
  if (this->CurrentProperty)
    {
    this->CurrentProperty->Delete();
    }
  if (this->CurrentCamera)
    {
    this->CurrentCamera->Delete();
    }
  if (this->CurrentSource)
    {
    this->CurrentSource->Delete();
    }
  if (this->CurrentPoints)
    {
    this->CurrentPoints->Delete();
    }
  if (this->CurrentNormals)
    {
    this->CurrentNormals->Delete();
    }
  if (this->CurrentTCoords)
    {
    this->CurrentTCoords->Delete();
    }
  if (this->CurrentTCoordCells)
    {
    this->CurrentTCoordCells->Delete();
    }
  if (this->CurrentScalars)
    {
    this->CurrentScalars->Delete();
    }
  if (this->CurrentMapper)
    {
    this->CurrentMapper->Delete();
    }
  if (this->CurrentLut)
    {
    this->CurrentLut->Delete();
    }
  if (this->CurrentIndexedFaceSet)
    {
    this->CurrentIndexedFaceSet->Delete();
    }
  if (this->CurrentColors)
    {
    this->CurrentColors->Delete();
    }
  this->CurrentTransform->Delete();

  // clean up saved coordinates
  for(std::map<std::string, vtkPoints*>::iterator I = defCoordinate.begin(); I != defCoordinate.end(); I++)
  {
	I->second->Delete();
  }

  // clean up saved colors
  for(std::map<std::string, vtkUnsignedCharArray*>::iterator I = defColor.begin(); I != defColor.end(); I++)
  {
	I->second->Delete();
  }

  // clean up saved normals
  for(std::map<std::string, vtkFloatArray*>::iterator I = defNormals.begin(); I != defNormals.end(); I++)
  {
	I->second->Delete();
  }

  // clean up saved texture coordinates
  for(std::map<std::string, vtkFloatArray*>::iterator I = defTextureCoordinates.begin(); I != defTextureCoordinates.end(); I++)
  {
	I->second->Delete();
  }
}

int vtkX3DNodeHandler::startTransform(const X3D::X3DAttributes &attr)
{
	this->CurrentTransform->Push();

	int index;

	// No defaults needed, as vtk uses the same defaults for the translation, rotation, scale

	// Check for translation
	index = attr.getAttributeIndex(X3D::translation);
	if(index != -1)
	{
		X3D::SFVec3f vec = attr.getSFVec3f(index);
		this->CurrentTransform->Translate(vec.x, vec.y, vec.z);
	}

	// Check for rotation
	index = attr.getAttributeIndex(X3D::rotation);
	if(index != -1)
	{
		X3D::SFRotation rot = attr.getSFRotation(index);
		this->CurrentTransform->RotateWXYZ(rot.angle * RAD_TO_DEG, &rot.x);
	}

	// Check for scale
	index = attr.getAttributeIndex(X3D::scale);
	if(index != -1)
	{
		X3D::SFVec3f vec = attr.getSFVec3f(index);
		this->CurrentTransform->Scale(vec.x, vec.y, vec.z);
	}
	return X3D::CONTINUE;
}

int vtkX3DNodeHandler::endTransform()
{
	this->CurrentTransform->Pop();
	return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startMaterial(const X3D::X3DAttributes &attr)
{
	bool ambientSet = false;

	// There was no Appearance Node
	if(!this->CurrentProperty)
		return X3D::CONTINUE;

	int index;
	
	// Check for ambientIntensity
	index = attr.getAttributeIndex(X3D::ambientIntensity);
	if(index != -1)
	{
		float ambientIntensity = attr.getSFFloat(index);
		this->CurrentProperty->SetAmbient(ambientIntensity);
		ambientSet = true;
	}
	else
		this->CurrentProperty->SetAmbient(0.2f);
	
	// Check for diffuse color
	index = attr.getAttributeIndex(X3D::diffuseColor);
	if(index != -1)
	{
		X3D::SFColor col = attr.getSFColor(index);
		// Set both, ambient and diffuse Color to this value
		// The ambient Color will be multiplied with the ambientIntensity
		// above
		this->CurrentProperty->SetDiffuseColor(col.r, col.g, col.b);
		this->CurrentProperty->SetAmbientColor(col.r, col.g, col.b);
	}
	else
	{
		this->CurrentProperty->SetDiffuseColor(0.8f, 0.8f, 0.8f);
		this->CurrentProperty->SetAmbientColor(0.8f, 0.8f, 0.8f);
	}

	// Check for emissiveColor
	// VTK does not support emissive color. It will be mapped to ambient
	// color only if ambientIntensity is not set.
	index = attr.getAttributeIndex(X3D::emissiveColor);
	if(index != -1 && !ambientSet)
	{
		X3D::SFColor col = attr.getSFColor(index);
		this->CurrentProperty->SetAmbientColor(col.r, col.g, col.b);
		this->CurrentProperty->SetAmbient(1.0);
	}
	
	// Check for shininess
	index = attr.getAttributeIndex(X3D::shininess);
	if(index != -1)
	{
		float shininess = attr.getSFFloat(index);
		// The X3D range for shininess is [0,1] while for OpenGL is [0,128]
		this->CurrentProperty->SetSpecularPower(shininess * 128.0);
	}
	else
		this->CurrentProperty->SetSpecularPower(0.2 * 128.0);

	// Check for specularColor
	index = attr.getAttributeIndex(X3D::specularColor);
	if(index != -1)
	{
		X3D::SFColor col = attr.getSFColor(index);
		this->CurrentProperty->SetSpecularColor(col.r, col.g, col.b);
	}
	else
		this->CurrentProperty->SetSpecularColor(0.0f, 0.0f, 0.0f);

	// Check for transparency
	index = attr.getAttributeIndex(X3D::transparency);
	if(index != -1)
	{
		float transparency = attr.getSFFloat(index);
		this->CurrentProperty->SetOpacity(1.0f - transparency);
	}
	else
		this->CurrentProperty->SetOpacity(1.0f);

	return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startAppearance(const X3D::X3DAttributes &attr)
{
	if(this->CurrentProperty)
		this->CurrentProperty->Delete();

	this->CurrentProperty = vtkProperty::New();
	this->CurrentProperty->SetInterpolationToPhong();
	return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startShape(const X3D::X3DAttributes &attr)
{
	actor = vtkActor::New();

	// Check for DEF
	if(attr.isDEF())
	{
		defString = attr.getDEF();
	}
	else
		defString.erase();

	// Check for USE
	if(attr.isUSE())
	{
		vtkActor* copy = defShape[attr.getUSE()];
		if (copy != NULL)
			actor->ShallowCopy(copy);
	}
	else if (this->CurrentProperty)
    {
      actor->SetProperty(this->CurrentProperty);
    }
	
	// This overrides the transformation derived by the "ShallowCopy(..)"
	actor->SetOrientation(this->CurrentTransform->GetOrientation());
    actor->SetPosition(this->CurrentTransform->GetPosition());
    actor->SetScale(this->CurrentTransform->GetScale());
    
	if(this->CurrentActor)
    {
      this->CurrentActor->Delete();
    }
    
	this->CurrentActor = actor;
    // Add actor to renderer
    this->Renderer->AddActor(actor);
	return X3D::CONTINUE;
}

int vtkX3DNodeHandler::endShape()
{
	if(bDEFCoordinate) 
	{
		this->CurrentPoints = NULL;
		bDEFCoordinate = false;
	}

	if(bDEFColor) 
	{
		this->CurrentColors = NULL;
		bDEFColor = false;
	}

	if(bDEFNormal) 
	{
		this->CurrentNormals = NULL;
		bDEFNormal = false;
	}

	if(bDEFTextureCoordinate) 
	{
		this->CurrentTCoords = NULL;
		bDEFTextureCoordinate = false;
	}

	// Nothing to do in case there's no defString
	if(defString.empty())
		return X3D::CONTINUE;

	// Create a copy of the CurrentActor
	vtkActor* DEFactor = vtkActor::New();
	DEFactor->ShallowCopy(this->CurrentActor);

	// Save the copy with defString as key
	defShape.insert(pair<string,vtkActor*>(defString, DEFactor));
	defString.erase();
	
	return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startSphere(const X3D::X3DAttributes &attr)
{
	if(!this->CurrentActor)
	{
		return X3D::CONTINUE;
	}

	int index;

	pmap = vtkPolyDataMapper::New();
	vtkSphereSource *sphere = vtkSphereSource::New();
    
	// Check for radius
	index = attr.getAttributeIndex(X3D::radius);
	if(index != -1)
	{
		float fRadius = attr.getSFFloat(index);
		sphere->SetRadius(fRadius);
	}
	else
		sphere->SetRadius(1.0f);
	
	pmap->SetInput(sphere->GetOutput());
	sphere->Delete();

    this->CurrentActor->SetMapper(pmap);
    
	pmap->Delete();

    if (this->CurrentProperty)
    {
      this->CurrentActor->SetProperty(this->CurrentProperty);
    }

	return X3D::CONTINUE;
}



int vtkX3DNodeHandler::startUnhandled(const char* nodeName, const X3D::X3DAttributes &attr)
{
	int elementID = X3D::X3DTypes::getElementID(nodeName);
	
	if (_ignoreNodes.find(elementID) == _ignoreNodes.end())
	{
		if (this->_verbose)
			cout << "Ignoring node " << nodeName << " and all of it's children." << endl;
		return X3D::SKIP_CHILDREN;
	}
	if (this->_verbose)
		cout << "Ignoring node " << nodeName << endl;
	return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startIndexedFaceSet(const X3D::X3DAttributes &attr) {
	
	int index;

	if(!this->CurrentActor)
	{
		return X3D::CONTINUE;
	}

	pmap = vtkPolyDataMapper::New();
    this->CurrentIndexedFaceSet = vtkX3DIndexedFaceSetSource::New();
	
	pmap->SetInput(this->CurrentIndexedFaceSet->GetOutput());
	// normal per vertex
	index = attr.getAttributeIndex(X3D::normalPerVertex);
	if(index != -1 && !attr.getSFBool(index))
	{
		this->CurrentIndexedFaceSet->NormalPerVertexOff();
	}
	else
		this->CurrentIndexedFaceSet->NormalPerVertexOn();

	// color per vertex
	index = attr.getAttributeIndex(X3D::colorPerVertex);
	if(index != -1 && !attr.getSFBool(index))
	{
		this->CurrentIndexedFaceSet->ColorPerVertexOff();
	}
	else
		this->CurrentIndexedFaceSet->ColorPerVertexOn();

	// coord index
	index = attr.getAttributeIndex(X3D::coordIndex);
	if(index != -1)
	{
		std::vector<int> coords = attr.getMFInt32(index);
		vtkIdTypeArray* idArray = vtkIdTypeArray::New();
		for(std::vector<int>::iterator I = coords.begin(); I != coords.end(); I++)
		{
			idArray->InsertNextValue(*I);
		}
		this->CurrentIndexedFaceSet->SetCoordIndex(idArray);
		idArray->Delete();
	}

	// color index
	index = attr.getAttributeIndex(X3D::colorIndex);
	if(index != -1)
	{
		std::vector<int> coords = attr.getMFInt32(index);
		vtkIdTypeArray* idArray = vtkIdTypeArray::New();
		for(std::vector<int>::iterator I = coords.begin(); I != coords.end(); I++)
		{
			idArray->InsertNextValue(*I);
		}
		this->CurrentIndexedFaceSet->SetColorIndex(idArray);
		idArray->Delete();
	}

	// normal index
	index = attr.getAttributeIndex(X3D::normalIndex);
	if(index != -1)
	{
		std::vector<int> coords = attr.getMFInt32(index);
		vtkIdTypeArray* idArray = vtkIdTypeArray::New();
		for(std::vector<int>::iterator I = coords.begin(); I != coords.end(); I++)
		{
			idArray->InsertNextValue(*I);
		}
		this->CurrentIndexedFaceSet->SetNormalIndex(idArray);
		idArray->Delete();
	}

	// texCoord index
	index = attr.getAttributeIndex(X3D::texCoordIndex);
	if(index != -1)
	{
		std::vector<int> coords = attr.getMFInt32(index);
		vtkIdTypeArray* idArray = vtkIdTypeArray::New();
		for(std::vector<int>::iterator I = coords.begin(); I != coords.end(); I++)
		{
			idArray->InsertNextValue(*I);
		}
		this->CurrentIndexedFaceSet->SetTexCoordIndex(idArray);
		idArray->Delete();
	}
	


    this->CurrentActor->SetMapper(pmap);
    pmap->Delete();
	return X3D::CONTINUE;
}

int vtkX3DNodeHandler::endIndexedFaceSet() {

  if(this->CurrentPoints)
	this->CurrentIndexedFaceSet->SetCoords(this->CurrentPoints);
  
  if(this->CurrentColors)
	this->CurrentIndexedFaceSet->SetColors(this->CurrentColors);
  
  if(this->CurrentTCoords)
	this->CurrentIndexedFaceSet->SetTexCoords(this->CurrentTCoords);

  if(this->CurrentNormals)
	this->CurrentIndexedFaceSet->SetNormals(this->CurrentNormals);

  this->CurrentIndexedFaceSet->Update();
  if (this->_verbose)
  {
	  vtkPolyData* p = this->CurrentIndexedFaceSet->GetOutput();
		cout << "Generated IndexedFaceSet with " << p->GetPolys()->GetNumberOfCells() << " faces, ";
		cout << p->GetNumberOfPoints() << " points and ";
		cout << (p->GetPointData()->GetNormals() ? p->GetPointData()->GetNormals()->GetNumberOfTuples() :0 ) << " normals." << endl;
  }
  this->CurrentIndexedFaceSet->Delete();
  this->CurrentIndexedFaceSet = NULL;

  return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startCoordinate(const X3D::X3DAttributes &attr) {
 
	int index = attr.getAttributeIndex(X3D::point);
    vtkPoints* points = NULL;

	if(index != -1)
	{
	  std::vector<X3D::SFVec3f> coords = attr.getMFVec3f(index);
	  points = vtkPoints::New();
	  for(std::vector<X3D::SFVec3f>::iterator I = coords.begin(); I != coords.end(); I++)
	  {
		  points->InsertNextPoint(&(*I).x);
	  }
	  if(this->CurrentPoints)
		  this->CurrentPoints->Delete();
	  this->CurrentPoints = points;
	}
	else if(attr.isUSE())
	{
		std::string sUse = attr.getUSE();
		this->CurrentPoints = defCoordinate[sUse];
		bDEFCoordinate = true; // don't delete saved points after usage
	}

	// Check for DEF
	if(attr.isDEF())
	{
		defCoordinate.insert(pair<std::string,vtkPoints*>(attr.getDEF(), points));
		bDEFCoordinate = true;
	}
	
	if(this->CurrentScalars) {
		  this->CurrentScalars->Reset();
		  for (int i=0;i < this->CurrentPoints->GetNumberOfPoints();i++) 
			{
			this->CurrentScalars->InsertNextValue(i);
			}
	}
  return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startNormal(const X3D::X3DAttributes &attr) {
  int index = attr.getAttributeIndex(X3D::vector);
  
  if(index != -1)
  {
	  std::vector<X3D::SFVec3f> normals = attr.getMFVec3f(index);
	  vtkFloatArray* normalArray = vtkFloatArray::New();
	  normalArray->SetNumberOfComponents(3);
	  for(std::vector<X3D::SFVec3f>::iterator I = normals.begin(); I != normals.end(); I++)
	  {
		  normalArray->InsertNextTupleValue(&(*I).x);
	  }
	  if(this->CurrentNormals)
		  this->CurrentNormals->Delete();
	  this->CurrentNormals = normalArray;
  }
  else if(attr.isUSE())
	{
		std::string sUse = attr.getUSE();
		this->CurrentNormals = defNormals[sUse];
		bDEFNormal = true; // don't delete saved points after usage
	}

	// Check for DEF
	if(attr.isDEF())
	{
		defNormals.insert(pair<std::string,vtkFloatArray*>(attr.getDEF(), this->CurrentNormals));
		bDEFNormal = true;
	}

  return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startColor(const X3D::X3DAttributes &attr) {
	  
	int index = attr.getAttributeIndex(X3D::color);

	if(index != -1)
	{
		std::vector<X3D::SFColor> colors = attr.getMFColor(index);
	
		if(this->CurrentColors)
			this->CurrentColors->Delete();

	 	this->CurrentColors = vtkUnsignedCharArray::New();
		this->CurrentColors->SetNumberOfComponents(3);
		
		for(std::vector<X3D::SFColor>::iterator I = colors.begin(); I != colors.end(); I++)
		{
			this->CurrentColors->InsertNextTuple3((*I).r * 255, (*I).g * 255, (*I).b * 255);
		}
	}
	else if(attr.isUSE())
	{
		std::string sUse = attr.getUSE();
		this->CurrentColors = defColor[sUse];
		bDEFColor = true; // don't delete saved colors after usage
	}

	// Check for DEF
	if(attr.isDEF())
	{
		defColor.insert(pair<std::string,vtkUnsignedCharArray*>(attr.getDEF(), this->CurrentColors));
		bDEFColor = true;
	}
    
    return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startTextureCoordinate(const X3D::X3DAttributes &attr) {
  int index = attr.getAttributeIndex(X3D::point);
  assert(this->CurrentIndexedFaceSet);
  if(index != -1)
  {
	  std::vector<X3D::SFVec2f> normals = attr.getMFVec2f(index);
	  vtkFloatArray* texCoordArray = vtkFloatArray::New();
	  texCoordArray->SetNumberOfComponents(2);
	  for(std::vector<X3D::SFVec2f>::iterator I = normals.begin(); I != normals.end(); I++)
	  {
		  texCoordArray->InsertNextTuple2((*I).x, (*I).y);
	  }
	  if(this->CurrentTCoords)
		  this->CurrentTCoords->Delete();
	  this->CurrentTCoords = texCoordArray;
  }
  else if(attr.isUSE())
	{
		std::string sUse = attr.getUSE();
		this->CurrentTCoords = defTextureCoordinates[sUse];
		bDEFTextureCoordinate = true; // don't delete saved TCoords after usage
	}

	// Check for DEF
	if(attr.isDEF())
	{
		defTextureCoordinates.insert(pair<std::string,vtkFloatArray*>(attr.getDEF(), this->CurrentTCoords));
		bDEFTextureCoordinate = true;
	}
  return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startPointSet(const X3D::X3DAttributes &attr) {
	pmap = vtkPolyDataMapper::New();
	pmap->SetScalarVisibility(0);

	this->CurrentActor->SetMapper(pmap);
    
	if (this->CurrentProperty)
      {
      this->CurrentActor->SetProperty(this->CurrentProperty);
      }
    if (this->CurrentMapper)
      {
      this->CurrentMapper->Delete();
      }
    
	this->CurrentMapper = pmap;
    
	if (this->CurrentScalars)
      {
      this->CurrentScalars->Delete();
      }
    
	this->CurrentScalars = vtkFloatArray::New();


	return X3D::CONTINUE;
}

int vtkX3DNodeHandler::endPointSet() {
  
	vtkCellArray *cells;
    vtkIdType i;
    vtkPolyData *pd;

    pd = vtkPolyData::New();
    cells = vtkCellArray::New();
    for (i=0;i < this->CurrentPoints->GetNumberOfPoints();i++) 
    {
		cells->InsertNextCell(1, &i);
    }

    pd->SetVerts(cells);
    this->CurrentMapper->SetInput(pd);
    pd->Delete();
    cells->Delete();

    ((vtkPolyData *)this->CurrentMapper->GetInput())->SetPoints(this->CurrentPoints);

	if(this->CurrentColors) 
	{
		((vtkPolyData *)this->CurrentMapper->GetInput())->GetPointData()->SetScalars(CurrentColors);
		pmap->SetScalarVisibility(1);
	}

	return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startBox(const X3D::X3DAttributes &attr) {
	pmap = vtkPolyDataMapper::New();
    vtkCubeSource *cube= vtkCubeSource::New();
 
	int index;

    // Check for size
	index = attr.getAttributeIndex(X3D::size);
	if(index != -1)
	{
		X3D::SFVec3f size = attr.getSFVec3f(index);
		cube->SetXLength(size.x);
		cube->SetYLength(size.y);
		cube->SetZLength(size.z);
	}
	else
	{
		cube->SetXLength(2.0f);
		cube->SetYLength(2.0f);
		cube->SetZLength(2.0f);
	}

	pmap->SetInput(cube->GetOutput());
    this->CurrentActor->SetMapper(pmap);
    pmap->Delete();
    cube->Delete();
	

	if(this->CurrentProperty)
    {
      this->CurrentActor->SetProperty(this->CurrentProperty);
    }
  return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startCone(const X3D::X3DAttributes &attr) {
	pmap = vtkPolyDataMapper::New();
    vtkConeSource *cone= vtkConeSource::New();

    int index;

    // Check for height
	index = attr.getAttributeIndex(X3D::height);
	if(index != -1)
	{
		float height = attr.getSFFloat(index);
		cone->SetHeight(height);
	}
	else
	{
		cone->SetHeight(2.0f);
	}

	// Check for bottomRadius
	index = attr.getAttributeIndex(X3D::bottomRadius);
	if(index != -1)
	{
		float bottomRadius = attr.getSFFloat(index);
		cone->SetRadius(bottomRadius);
	}
	else
	{
		cone->SetRadius(1.0f);
	}

	pmap->SetInput(cone->GetOutput());
    this->CurrentActor->SetMapper(pmap);
    pmap->Delete();

	if(this->CurrentProperty)
    {
      this->CurrentActor->SetProperty(this->CurrentProperty);
    }
  return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startCylinder(const X3D::X3DAttributes &attr) {
	pmap = vtkPolyDataMapper::New();
	vtkCylinderSource *cylinder = vtkCylinderSource::New();

    int index;

    // Check for height
	index = attr.getAttributeIndex(X3D::height);
	if(index != -1)
	{
		float height = attr.getSFFloat(index);
		cylinder->SetHeight(height);
	}
	else
	{
		cylinder->SetHeight(2.0f);
	}

	// Check for radius
	index = attr.getAttributeIndex(X3D::radius);
	if(index != -1)
	{
		float radius = attr.getSFFloat(index);
		cylinder->SetRadius(radius);
	}
	else
	{
		cylinder->SetRadius(1.0f);
	}

	pmap->SetInput(cylinder->GetOutput());
    this->CurrentActor->SetMapper(pmap);
    pmap->Delete();

	if(this->CurrentProperty)
    {
      this->CurrentActor->SetProperty(this->CurrentProperty);
    }
  return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startIndexedLineSet(const X3D::X3DAttributes &attr)
{
	if(!this->CurrentActor)
	{
		return X3D::CONTINUE;
	}

	int index;
	
	vtkCellArray* cells;
    
	// Check for coordinate Index
	index = attr.getAttributeIndex(X3D::coordIndex);
	if(index != -1)
	{
		vector<int> coords = attr.getMFInt32(index);
		cells = vtkCellArray::New();
		int indexCount = 0;
		for(vector<int>::const_iterator I = coords.begin(), J = coords.begin(); I != coords.end(); I++)
		{
			if (*I == -1)
			{
				cells->InsertNextCell(indexCount);
				while(J != I)
				{
					cells->InsertCellPoint(*J);
					J++;
				}
				indexCount = 0;
				// Skip -1
				if (J != coords.end())
					J++;
				
			}
			else
				indexCount++;
		}

	}
	else
		return X3D::CONTINUE;
	
	vtkPolyData* pd = vtkPolyData::New();
	pd->SetLines(cells);
	cells->Delete();


	pmap = vtkPolyDataMapper::New();
	pmap->SetInput(pd);
	pd->Delete();

    this->CurrentActor->SetMapper(pmap);
	pmap->Delete();

    if (this->CurrentProperty)
    {
      this->CurrentActor->SetProperty(this->CurrentProperty);
    }

	return X3D::CONTINUE;
}

int vtkX3DNodeHandler::endIndexedLineSet()
{
	if (!this->CurrentPoints)
		return X3D::CONTINUE;
	
	vtkPolyDataMapper* mapper = ((vtkPolyDataMapper*)this->CurrentActor->GetMapper());
	mapper->GetInput()->SetPoints(this->CurrentPoints);
}

int vtkX3DNodeHandler::startDirectionalLight(const X3D::X3DAttributes &attr) {
	
	if (this->CurrentLight)
    {
      this->CurrentLight->Delete();
    }

    this->CurrentLight = vtkLight::New();
    this->Renderer->AddLight(this->CurrentLight);
    
	int index;

	// Check for ambientIntensity (SetIntensity does not really suit here)
	index = attr.getAttributeIndex(X3D::ambientIntensity);
	if(index != -1)
	{
		float ambientIntensity = attr.getSFFloat(index);
		this->CurrentLight->SetIntensity(ambientIntensity);
	}
	// Default value for ambientIntensity is '0', but then you won't see anything, as SetIntensity doesn't seem to be the same. 
	else
		this->CurrentLight->SetIntensity(1.0f);

	// Check for color
	index = attr.getAttributeIndex(X3D::color);
	if(index != -1)
	{
		X3D::SFColor color = attr.getSFColor(index);
		this->CurrentLight->SetColor(color.r, color.g, color.b);
	}
	else
		this->CurrentLight->SetColor(1.0f, 1.0f, 1.0f);

	// Check for direction
	index = attr.getAttributeIndex(X3D::direction);
	if(index != -1)
	{
		X3D::SFVec3f direction = attr.getSFVec3f(index);
		this->CurrentLight->SetFocalPoint(direction.x, direction.y, direction.z);
	}
	else
		this->CurrentLight->SetFocalPoint(0.0f, 0.0f, -1.0f);


	// Check for intensity
	index = attr.getAttributeIndex(X3D::intensity);
	if(index != -1)
	{
		float intensity = attr.getSFFloat(index);
		this->CurrentLight->SetIntensity(intensity);
	}
	else
		this->CurrentLight->SetIntensity(1.0f);

	// Check for on
	index = attr.getAttributeIndex(X3D::on);
	if(index != -1)
	{
		bool on = attr.getSFBool(index);
		this->CurrentLight->SetSwitch(on ? 1 : 0);
	}
	else
		this->CurrentLight->SetSwitch(1);

	return X3D::CONTINUE;
}

int vtkX3DNodeHandler::startBackground(const X3D::X3DAttributes &attr) {
	int index = attr.getAttributeIndex(X3D::skyColor);
	if(index != -1)
	{
		std::vector<X3D::SFColor> skyColor = attr.getMFColor(index);
		if (skyColor.size())
		{
			X3D::SFColor mainColor = skyColor[0];
			this->Renderer->SetBackground(mainColor.r, mainColor.g, mainColor.b);
		}
	}
	return X3D::CONTINUE;
}

void vtkX3DNodeHandler::setVerbose(bool verbose)
{
	_verbose = verbose;
}