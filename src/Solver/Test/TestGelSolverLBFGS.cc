// Test for Model
  
#include "GelModel.h"
#include "GelMesh.h"
#include "EigenResult.h"
#include "Spring.h"
#include "AngleSpring.h"
#include "LBFGSB.h"

using namespace voom;

int main(int argc, char** argv) {

  
  
  //Test Mechanics model
  {
    cout << " ------------------------------- " << endl;
    cout << " TEST OF GEL MODEL " << endl << endl;
            
    GelMesh TestGelMesh("../../Mesh/Test/filaments.node", "../../Mesh/Test/filaments.ele");
    
    // Initialize Model
    uint NodeDoF = TestGelMesh.getDimension();
    uint NumFil = TestGelMesh.getNumberOfFilaments();
    uint NumNod = TestGelMesh.getNumberOfNodes();
    uint PbDoF = NumNod*NodeDoF;

    cout << "Number of elements is " << NumFil << endl;
    cout << "Number of degrees of freedom is " << PbDoF << endl;

    // Initlize filament materials
    vector<FilamentMaterial * > springs;
    springs.reserve(NumFil);
    
    vector<FilamentMaterial * > angleSprings;
    angleSprings.reserve(NumFil);
    
    Real k = 1.0;
    Real kappa = 1.0;
    for (int nFil = 0; nFil < NumFil; nFil++) {        
      Spring* Spr = new Spring(nFil,k); 
      springs.push_back(Spr);
      AngleSpring* Ang = new AngleSpring(nFil,kappa);
      angleSprings.push_back(Ang);
    }

    // Construct gel model
    GelModel myModel(&TestGelMesh, springs, angleSprings, NodeDoF,0,1);
     
    EigenResult R(PbDoF, 1);
    R.setRequest(3);  
    
    //myModel.checkConsistency(R,0.1,3,1e-8,1e-5);

    myModel.initializeField(1.5);

    myModel.compute(R);

    cout <<"energy is "<<  R.getEnergy() << endl;
 
    double factr=1.0e+1;
    double pgtol=1.0e-5;
    int iprint= 80;  
    //int maxIterations=10000;

    // LBFGSB Solver

    LBFGSB mySolver(&myModel,PbDoF, &R,5,factr,pgtol,iprint);
    
    mySolver.solve();
    
    myModel.writeOutput("filaments",1);
    
  
    cout << endl << " END OF TEST OF GEL MODEL " << endl;
    cout << " ------------------------------ " << endl << endl;
    // myModel.
  }


}
