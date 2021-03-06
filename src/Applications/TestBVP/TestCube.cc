#include "PassMyoA.h"
#include "CompNeoHookean.h"
#include "FEMesh.h"
#include "EigenEllipticResult.h"
#include "MechanicsModel.h"
#include "EigenNRsolver.h"
#include "LBFGSB.h"

using namespace voom;

int main(int argc, char** argv)
{

  // Timing
  time_t start, end;
  time(&start);



  // Initialize Mesh
  // Assumptions to use this main as is: strip has a face at z=0; tetrahedral mesh
  // FEMesh Cube("Cube6.node", "Cube6.ele");
  // FEMesh surfMesh("Cube6.node", "Cube6Surf.ele");
  FEMesh Cube("Strip36.node", "Strip36.ele");
  FEMesh surfMesh("Strip36.node", "Strip36Surf.ele");
  // FEMesh Cube("Strip144.node", "Strip144.ele");
  // FEMesh surfMesh("Strip144.node", "Strip144Surf.ele");

 
  cout << endl;
  cout << "Number Of Nodes   : " << Cube.getNumberOfNodes() << endl;
  cout << "Number Of Element : " << Cube.getNumberOfElements() << endl;
  cout << "Mesh Dimension    : " << Cube.getDimension() << endl << endl;
  
    
  // Initialize Material
  uint NumMat =  Cube.getNumberOfElements();
  vector<MechanicsMaterial * > materials;
  materials.reserve(NumMat);
  vector<Vector3d > Fibers; 
  Fibers.reserve(NumMat);

  for (int k = 0; k < NumMat; k++) {
    CompNeoHookean* Mat = new CompNeoHookean(k, 1.0, 1.0);
    materials.push_back(Mat);

    Vector3d N; N << 0.0, 0.0, 1.0;
    Fibers.push_back(N);
  }

  // Load fibers into elements
  Cube.setFibers(Fibers);

  // Initialize Model
  int NodeDoF = 3;
  int PressureFlag = 1;
  Real Pressure = 1.54;
  int NodalForcesFlag = 1;
  vector<int > ForcesID;
  vector<Real > Forces;
  MechanicsModel myModel(&Cube, materials, NodeDoF, PressureFlag, &surfMesh,
			 NodalForcesFlag);
  myModel.updatePressure(Pressure);
  myModel.updateNodalForces(&ForcesID, &Forces);
 
  // Initialize Result
  uint myRequest;
  uint PbDoF = (Cube.getNumberOfNodes())*myModel.getDoFperNode();
  EigenEllipticResult myResults(PbDoF, NumMat*2);

  // Run Consistency check
  /*
  Real perturbationFactor = 0.1;
  myRequest = 7; // Check both Forces and Stiffness
  Real myH = 1e-6;
  Real myTol = 1e-7;
  myModel.checkConsistency(myResults, perturbationFactor, myRequest, myH, myTol);
  myModel.checkDmat(myResults, perturbationFactor, myH, myTol);
  */




    // Print initial configuration
    // myModel.writeOutputVTK("CubeSmall_", 0);
    myModel.writeOutputVTK("Strip36_", 0);

    // Check on applied pressure
    myRequest = 2;
    myResults.setRequest(myRequest);
    myModel.compute(myResults);
    VectorXd Fstart = *(myResults._residual);
    Real pX = 0.0, pY = 0.0, pZ = 0.0;
    for (int i = 0; i < Fstart.size(); i += 3) {
      pX += Fstart(i);
      pY += Fstart(i+1);
      pZ += Fstart(i+2);
    }
    cout << endl << "Pressure = " << pX << " " << pY << " " << pZ << endl << endl;
     
    // EBC
    vector<int > DoFid;
    vector<Real > DoFvalues;
    for(int i = 0; i < Cube.getNumberOfNodes(); i++) {
      // All nodes at x = 0
      if ( Cube.getX(i, 0) < 1.0e-12 ) {
	DoFid.push_back(i*3);
	// All nodes at y =0
	if ( Cube.getX(i, 1) < 1.0e-12 ) {
	  DoFid.push_back(i*3 + 1);
	}
	// All nodes at x = z
	if ( Cube.getX(i, 2) < 1.0e-12 ) {
	  DoFid.push_back(i*3 + 2);
	}
      }
    }
 
    for(int i = 0; i < DoFid.size(); i++) {
      DoFvalues.push_back( Cube.getX(floor(double(DoFid[i])/3.0) ,DoFid[i]%3) );
      // cout << DoFid[i] << " " <<  DoFvalues[i] << endl;
    }

    // Lin tet cube - applied displacements //
    // for (uint i = 18; i < 27; i++) { 
    //   DoFid[i-5] = i*3 + 2; // Lower top in z direction by 0.1
    //   DoFvalues[i-5] = 1.95;
    // }
    
    // Solver
    Real NRtol = 1.0e-12;
    uint NRmaxIter = 100;
    EigenNRsolver mySolver(&myModel, DoFid, DoFvalues, CHOL, NRtol, NRmaxIter);
    mySolver.solve(DISP); 

    // myModel.writeOutputVTK("CubeSmall_", 1);
    myModel.writeOutputVTK("Strip36_", 1);
    

    // int m = 5; 
    // double factr = 1.0e+1;
    // double pgtol = 1.0e-5;
    // int iprint = 0;
    // int maxIterations = -1;
    
    // vector<int > nbd(PbDoF, 0);
    // vector<double > lowerBound(PbDoF, 0.0);
    // vector<double > upperBound(PbDoF, 0.0);
    // for(int i = 0; i < DoFid.size(); i++) {
    //   nbd[DoFid[i]] = 2;
    //   lowerBound[DoFid[i]] = DoFvalues[i];
    //   upperBound[DoFid[i]] = DoFvalues[i];
    // }
    // cout << "PbDoF = " << PbDoF << endl;
    // for (int j = 0; j < PbDoF; j++) {
    //   cout << nbd[j] << " " << lowerBound[j] << " " << upperBound[j] << endl;
    // }
    
    // LBFGSB mySolver(& myModel, & myResults, m, factr, pgtol, iprint, maxIterations);
    // mySolver.setBounds(nbd, lowerBound, upperBound);
    // mySolver.solve();
    
    // // Print final configuration
    // myModel.writeOutputVTK("CubeQuad_", 1);
    
    // // Print _field
    // cout << endl;
    // myModel.printField();
    // cout << endl;
    
    // Compute Residual
    myRequest = 2;
    myResults.setRequest(myRequest);
    myModel.compute(myResults);
    VectorXd F = *(myResults._residual);
    for (int i = 0; i < DoFid.size(); i ++) {
      ForcesID.push_back( DoFid[i] );
      Forces.push_back( -F(DoFid[i]) );
    }

  // Timing
  time (&end);
  cout << endl << "BVP solved in " << difftime(end,start) << " s" << endl;



  return 0;
}
