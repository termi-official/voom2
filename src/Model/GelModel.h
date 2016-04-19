//-*-C++-*-
#ifndef __GelModel_h__
#define __GelModel_h__

#include "Model.h"
#include "FilamentMaterial.h"
#include "EigenResult.h"
#include "GelMesh.h"
#include "Filament.h"

namespace voom{

  // Model Results
  class GelModel: public Model {

  public:

    //! Basic Constructor
    /*! Construct from basic data structures defining the mesh, materials, BCs. 
     */
    GelModel(GelMesh* aGelMesh, vector<FilamentMaterial * > _springs,
	     vector<FilamentMaterial * > _angleSprings,
	     const uint NodeDoF,
	     int NodalForcesFlag = 0,
	     int _resetFlag = 1);
    
		     
    //! Input-file-based Constructor
    // GelModel(Mesh* myMesh, const string inputFile, const uint NodeDoF);

    //! Destructor
    
    ~GelModel() {
      set<FilamentMaterial *> UNIQUEmaterials;
      for (uint i = 0; i < _springs.size(); i++) 
	UNIQUEmaterials.insert(_springs[i]);
      for (uint i = 0; i < _angleSprings.size(); i++)
        UNIQUEmaterials.insert(_angleSprings[i]);

      for (set<FilamentMaterial *>::iterator it = UNIQUEmaterials.begin();
	   it != UNIQUEmaterials.end(); it++) 
		delete (*it);
    };
    
    // Get number of Nodes:
    int getNumberOfNodes(){return _myGelMesh->getNumberOfNodes();}

    //! Initialize field
    // From constant value
    void initializeField(const Real value = 1.0) {
      const uint numNodes = _myGelMesh->getNumberOfNodes();
      const uint dim = _myGelMesh->getDimension();

      for (uint i = 0; i < numNodes; i++) 
	for (uint j = 0; j < dim; j++)
	  _field[i*dim+j] = _myGelMesh->getX(i,j)*value; // value = isotropic expansion/shrinking
    };

    //! From array
    void initializeField(const Real* value) {
      _field.assign(value, value+_field.size());
    };

    //! Linearized update
    void linearizedUpdate(const Real* localValues, Real fact) {
      const int nLocalDof = (_myGelMesh->getNumberOfNodes())*_nodeDoF;
      for(uint i = 0; i < nLocalDof; i++)
	_field[i] += fact*localValues[i];
    };

    // One value at the time (Node ID, dof index, value)
    void linearizedUpdate(const int id, const int dof, const Real value) {
      // const uint dim = _myGelMesh->getDimension();
      // assert( id < _field.size() && dof < dim );
      _field[id*_nodeDoF + dof] += value;
    }

    // One value at the time (Node ID, dof index, value)
    void linearizedUpdate(const int dof, const Real value) {
      _field[dof] += value;
    }
    
    void setField(uint dof, Real value) {
      _field[dof] = value;
    }
    void setField(const Real* value) {
      _field.assign(value, value+_field.size());
    };
    void getField(vector<double> & x) {
      assert(x.size() == _field.size());
      x = _field;
    }

    VectorXd getX()
    {
      VectorXd X =_myGelMesh->getX();
      return X;
    }
    
    Real getX(const uint nodeId, const uint dof){
      return _myGelMesh->getX(nodeId,dof);
    }
    
    void setPrevField() {
      _prevField = _field;
    };

    void printField() {
      int i = 0;
      while (i < _field.size()) {
	for (uint j = 0; j < _nodeDoF; j++) {
	  cout << _field[i] << " ";
	  i++;
	}
	cout << endl;
      }
    }

    void writeField(string OutputFile, int step) {
      // Create outputFile name
      stringstream FileNameStream;
      FileNameStream << OutputFile << step << ".dat";
      ofstream out;
      out.open( (FileNameStream.str()).c_str() );
  
      out << _field.size() << endl;
      for (uint i = 0; i < _field.size(); i++) {
	out << setprecision(15) << _field[i] << endl;
      }
      out.close();
    }

    uint getNumMat() {
      set<FilamentMaterial *> UNIQUEmaterials;
      for (uint i = 0; i < _springs.size(); i++) 
	UNIQUEmaterials.insert(_springs[i]);
      for (uint i = 0; i < _angleSprings.size(); i++)
        UNIQUEmaterials.insert(_angleSprings[i]);

      return UNIQUEmaterials.size();
    }

    uint getTotNumMatProp() {
      set<FilamentMaterial *> UNIQUEmaterials;
      for (uint i = 0; i < _springs.size(); i++)
        UNIQUEmaterials.insert(_springs[i]);
      for (uint i = 0; i < _angleSprings.size(); i++)
	UNIQUEmaterials.insert(_angleSprings[i]);

      return UNIQUEmaterials.size();

    }

    vector<FilamentMaterial * > getSprings() {
      return _springs;
    }

    void setResetFlag(int ResetFlag) {
      _resetFlag = ResetFlag;
    }

    
    void setNodalForcesFlag(int NodalForcesFlag) {
      _nodalForcesFlag = NodalForcesFlag;
    }
    
    //! Write output
    void writeOutputVTK(const string OutputFile, int step); 

    //! Solve the system
    void compute(Result & R);

    
    // Update nodal forces
    void updateNodalForces(vector<int > * ForcesID, vector<Real > * Forces) {
      _forcesID = ForcesID; _forces = Forces;
    }

    //void computeDeformation(vector<Vector3d > & dlist, GeomFilament* geomEl);

    void getFilamentx(vector<Vector3d > & xlist, GeomFilament* geomEl);

  protected:
    
    //void computeDeformation(vector<Vector3d > & dlist, GeomElement* geomEl);
    //! List of Material data at each element in the model
    vector<FilamentMaterial * > _springs;
    vector<FilamentMaterial * > _angleSprings;
    //! Solution value at all nodes, local and ghost
    vector<Real > _field;
    vector<VectorXd> _X0;
    // It should not be done here - maye we should have bodies and forms models from bodies
    
    int _nodalForcesFlag;
    vector<int > * _forcesID;
    vector<Real > * _forces;
    GelMesh* _myGelMesh;
    vector<Real > _prevField;

    int _resetFlag;
  };

} // namespace voom

#endif
