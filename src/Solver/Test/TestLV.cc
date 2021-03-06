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
  FEMesh LVmesh("../../Mesh/Test/CoarseLV.node", "../../Mesh/Test/CoarseLV.ele");
  FEMesh LVsurf("../../Mesh/Test/CoarseLV.node", "../../Mesh/Test/CoarseLV.surf");
  string BCfile = "CoarseLV.BaseSurfBC";
  string DispFile = "CoarseLV.disp";
 
  cout << endl;
  cout << "Number Of Nodes   : " << LVmesh.getNumberOfNodes() << endl;
  cout << "Number Of Element : " << LVmesh.getNumberOfElements() << endl;
  cout << "Mesh Dimension    : " << LVmesh.getDimension() << endl << endl;
  
  // Initialize Material
  uint NumMat =  LVmesh.getNumberOfElements();
  vector<MechanicsMaterial * > materials;
  materials.reserve(NumMat);

  string FiberFile = "CoarseLV.fiber";
  ifstream FiberInp(FiberFile.c_str());


  for (int k = 0; k < NumMat; k++) {
    PassMyoA* Mat = new PassMyoA(k, 35.19, 7.06, 100.0, 0.025, 2.87, 2.82);
    Vector3d N = Vector3d::Zero();
    FiberInp >> N(0);
    FiberInp >> N(1);
    FiberInp >> N(2);
    Mat->setN(N);
    materials.push_back(Mat);
    // materials.push_back(new CompNeoHookean(k, 10.0, 10.0) );
  }

  // cout << N(0) << " " << N(1) << " " << N(2) << endl;

  // Initialize Model
  int NodeDoF = 3;
  int PressureFlag = 1;
  Real Pressure = 0.05;
  MechanicsModel myModel(&LVmesh, materials, NodeDoF, PressureFlag, Pressure, &LVsurf);
 
  // Initialize Result
  uint PbDoF = (LVmesh.getNumberOfNodes())*myModel.getDoFperNode();
  EigenEllipticResult myResults(PbDoF, NumMat);
 
  // Print initial configuration
  myModel.writeOutputVTK("LVpassiveFillingFiber_", 0);

  // Check on applied pressure
  int myRequest = 2;
  myResults.setRequest(myRequest);
  myModel.compute(myResults);
  VectorXd F = *(myResults._residual);
  Real pX = 0.0, pY = 0.0, pZ = 0.0;
  for (int i = 0; i < F.size(); i += 3) {
    pX += F(i);
    pY += F(i+1);
    pZ += F(i+2);
  }
  cout << endl << "Pressure = " << pX << " " << pY << " " << pZ << endl << endl;


  // EBC
  int NumBC = 0, node = 0, ind = 0;;
  vector<int > DoFid;
  vector<Real > DoFvalues;
  ifstream BCinp(BCfile.c_str());

  BCinp >> NumBC;
  DoFid.reserve(NumBC*3);
  DoFvalues.reserve(NumBC*3);
  for(int i = 0; i < NumBC; i++) {
    BCinp >> node;
    for (int j = 0; j < 3; j++) {
      DoFid.push_back(node*3 + j);
      DoFvalues.push_back(LVmesh.getX(node, j));
      // cout << DoFid[ind] << " " <<  DoFvalues[ind] << endl;
      ind++;
    }
  }
  
  // Solver
  Real NRtol = 1.0e-10;
  uint NRmaxIter = 100;
  EigenNRsolver mySolver(&myModel, DoFid, DoFvalues, CHOL, NRtol, NRmaxIter);
  mySolver.solve(DISP); 

  // int m = 5; 
  // double factr = 1.0e+1;
  // double pgtol = 1.0e-5;
  // int iprint = 50;
  // int maxIterations = 10000;
  
  // vector<int > nbd(PbDoF, 0);
  // vector<double > lowerBound(PbDoF, 0.0);
  // vector<double > upperBound(PbDoF, 0.0);
  // for(int i = 0; i < DoFid.size(); i++) {
  //   nbd[DoFid[i]] = 2;
  //   lowerBound[DoFid[i]] = DoFvalues[i];
  //   upperBound[DoFid[i]] = DoFvalues[i];
  // }
  // // cout << "PbDoF = " << PbDoF << endl;
  // // for (int j = 0; j < PbDoF; j++) {
  // //   cout << nbd[j] << " " << lowerBound[j] << " " << upperBound[j] << endl;
  // // }
  
  // LBFGSB mySolver(& myModel, & myResults, m, factr, pgtol, iprint, maxIterations);
  // mySolver.setBounds(nbd, lowerBound, upperBound);
  // mySolver.solve();
  
  // Print final configuration
  myModel.writeOutputVTK("LVpassiveFillingFiber_", 1);
  myModel.writeField(DispFile);

  // Next recompute material properties using EMFO
  {
    ifstream BCinp(DispFile.c_str());
    int DoF;

    BCinp >> DoF;
    vector<int > DoFid(DoF, 0);
    vector<Real > DoFvalues(DoF, 0);
    for(int i = 0; i < DoF; i++) {
      DoFid[i] = i;
      BCinp >> DoFvalues[i];
      // cout <<  DoFid[i] << " " <<  DoFvalues[i] << endl;
    }
   
    // Change Material properties
    // Find unique material parameters
    set<MechanicsMaterial *> UNIQUEmaterials;
    for (uint i = 0; i < materials.size(); i++) 
      UNIQUEmaterials.insert(materials[i]);

    srand (time(NULL));
    for (set<MechanicsMaterial *>::iterator MatIt = UNIQUEmaterials.begin(); MatIt != UNIQUEmaterials.end(); MatIt++) {
      int MatID = (*MatIt)->getMatID();
      vector<Real > MatProp = (*MatIt)->getMaterialParameters();
      int NumPropPerMat = MatProp.size();
      for (int m = 0; m < NumPropPerMat; m++) {
    	MatProp[m] *= double(rand())/double(RAND_MAX);
	cout << MatProp[m] << " ";
      }
      cout << endl;
      (*MatIt)->setMaterialParameters(MatProp);
    }

    // Solve for correct material properties
    Real NRtol = 1.0e-12;
    uint NRmaxIter = 10;
    EigenNRsolver mySolver(&myModel, DoFid, DoFvalues, CHOL, NRtol, NRmaxIter);
    mySolver.solve(MAT); 
 
    // Print found material properties
    for (set<MechanicsMaterial *>::iterator MatIt = UNIQUEmaterials.begin(); MatIt != UNIQUEmaterials.end(); MatIt++) {
      int MatID = (*MatIt)->getMatID();
      vector<Real > MatProp = (*MatIt)->getMaterialParameters();
      int NumPropPerMat = MatProp.size();
      for (int m = 0; m < NumPropPerMat; m++) {
	cout << MatProp[m] << " ";
      }
      cout << endl;
    }

  } // End of material properties solution


  // Timing
  time (&end);
  cout << endl << "BVP solved in " << difftime(end,start) << " s" << endl;



  return 0;
}
