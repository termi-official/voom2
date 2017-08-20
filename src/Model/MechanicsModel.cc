#include "MechanicsModel.h"

namespace voom {

  // Constructor
  MechanicsModel::MechanicsModel(Mesh* aMesh, vector<MechanicsMaterial * > Materials,
    const uint NodeDoF,
    int PressureFlag, Mesh* SurfaceMesh,
    int NodalForcesFlag,
    int ResetFlag,
    int SpringBCflag):
    Model(aMesh, NodeDoF), _materials(Materials),
    _pressureFlag(PressureFlag), _pressure(0.0), _surfaceMesh(SurfaceMesh),
    _nodalForcesFlag(NodalForcesFlag), _forcesID(NULL), _forces(NULL), _resetFlag(ResetFlag), _springBCflag(SpringBCflag)
    {
      // THERE IS ONE MATERIAL PER ELEMENT - CAN BE CHANGED - DIFFERENT THAN BEFORE
      // Resize and initialize (default function) _field vector
      _field.resize(  (_myMesh->getNumberOfNodes() )*_nodeDoF );
      this->initializeField();

      // Initializing Flags, SpringBCflag should also be done here.
      _torsionalSpringBCflag = 0;
      _lennardJonesBCFlag = 0;
      _makeFlexible = false;

      // if (_pressureFlag == 1) {
      _prevField.resize( _field.size() );
      this->setPrevField();
      // }
    }



  // Compute deformation gradient
  void MechanicsModel::computeDeformationGradient(vector<Matrix3d > & Flist, GeomElement* geomEl)
  {
    // Compute F at all quadrature points
    const uint numQP = geomEl->getNumberOfQuadPoints();
    const uint dim   = _myMesh->getDimension();

    const vector<int > & NodesID = geomEl->getNodesID();
    const uint nodeNum = NodesID.size();

    // cout << "Nodes: " << _field[NodesID[0]*3 + 0] << "\t" << _field[NodesID[1]*3 + 0]  << "\t" << _field[NodesID[2]*3 + 0] << "\t" << _field[NodesID[3]*3 + 0]  << endl;
    // cout << "Nodes: " << _field[NodesID[0]*3 + 1] << "\t" << _field[NodesID[1]*3 + 1]  << "\t" << _field[NodesID[2]*3 + 1] << "\t" << _field[NodesID[3]*3 + 1]  << endl;
    // cout << "Nodes: " << _field[NodesID[0]*3 + 2] << "\t" << _field[NodesID[1]*3 + 2]  << "\t" << _field[NodesID[2]*3 + 2] << "\t" << _field[NodesID[3]*3 + 2]  << endl;
    // cout << "Nodes: " << NodesID[0] << "\t" << NodesID[1] << "\t" << NodesID[2] << "\t" << NodesID[3] << endl;

    for(uint q = 0; q < numQP; q++) {
      // Initialize F to zero
      Flist[q] = Matrix3d::Zero();
      for(uint i = 0; i < dim; i++)
      for(uint J = 0; J < dim; J++)
      for(uint a = 0; a < nodeNum; a++)
      Flist[q](i,J) +=
      _field[NodesID[a]*dim + i] * geomEl->getDN(q, a, J);
    } // loop over quadrature points
  }


  void MechanicsModel::computeGreenLagrangianStrainTensor(vector<Matrix3d> & Elist, GeomElement* geomEl) {
    const int sizeofE = Elist.size();
    vector<Matrix3d> Flist(sizeofE, Matrix3d::Zero());
    this->computeDeformationGradient(Flist, geomEl);

    for (int i = 0; i < sizeofE; i++)
      Elist[i] = 0.5 * (Flist[i].transpose() * Flist[i] - Matrix3d::Identity());
  }





  // Compute Function - Compute Energy, Force, Stiffness
  void MechanicsModel::compute(Result * R)
  {
    const vector<GeomElement* > elements = _myMesh->getElements();
    const int AvgNodePerEl = ((elements[0])->getNodesID()).size();
    const int NumEl = elements.size();
    const int dim = _myMesh->getDimension();
    vector<Triplet<Real > > KtripletList, HgtripletList;

    int PbDoF = R->getPbDoF();
    int TotNumMatProp = R->getNumMatProp();
    vector<VectorXd > dRdalpha;
    int NumPropPerMat = (_materials[0]->getMaterialParameters()).size(); // Assume all materials have the same number of material properties


    // Reset values in result struct
    if ( R->getRequest() & ENERGY ) {
      R->setEnergy(0.0);
    }
    if ( R->getRequest() & FORCE || R->getRequest() & DMATPROP )  {
      R->resetResidualToZero();
    }
    if ( R->getRequest() & STIFFNESS ) {
      R->resetStiffnessToZero();
      KtripletList.reserve(dim*dim*NumEl*AvgNodePerEl*AvgNodePerEl);
    }

    if ( R->getRequest() & DMATPROP ) {
      dRdalpha.assign( TotNumMatProp, VectorXd::Zero(PbDoF) );
      if ( _resetFlag == 1 ) {
        R->resetGradgToZero();
        R->resetHgToZero();
        HgtripletList.reserve(TotNumMatProp*TotNumMatProp);
      }
    }



    // Loop through elements, also through material points array, which is unrolled
    // uint index = 0;
    MechanicsMaterial::FKresults FKres;
    FKres.request = R->getRequest();
    for(int e = 0; e < NumEl; e++)
    {
      GeomElement* geomEl = elements[e];
      const vector<int  >& NodesID = geomEl->getNodesID();
      const int numQP    = geomEl->getNumberOfQuadPoints();
      const int numNodes = NodesID.size();
      MatrixXd Kele = MatrixXd::Zero(numNodes*dim, numNodes*dim);

      // F at each quadrature point are computed at the same time in one element
      vector<Matrix3d > Flist(numQP, Matrix3d::Zero());
      // Compute deformation gradients for current element
      this->computeDeformationGradient(Flist, geomEl);

      // Loop over quadrature points
      for(int q = 0; q < numQP; q++) {
	// cout << "Element: " << e << "\tQuadrature: " << q << endl;
        _materials[e*numQP + q]->compute(FKres, Flist[q]);

        // Volume associated with QP q
        Real Vol = geomEl->getQPweights(q);

        // Compute energy
        if (R->getRequest() & ENERGY) {
          R->addEnergy(FKres.W*Vol);
        }

        // Compute Residual
        if ( (R->getRequest() & FORCE) || (R->getRequest() & DMATPROP) ) {
          for(uint a = 0; a < numNodes; a++) {
            for(uint i = 0; i < dim; i++) {
              Real tempResidual = 0.0;
              for (uint J = 0; J < dim; J++) {
                tempResidual += FKres.P(i,J) * geomEl->getDN(q, a, J);
              } // J loop
              tempResidual *= Vol;
              R->addResidual(NodesID[a]*dim+i, tempResidual);
            } // i loop
          } // a loop
        } // Internal force loop

        // Compute Stiffness
        if (R->getRequest() & STIFFNESS) {
          for(uint a = 0; a < numNodes; a++) {
            for(uint i = 0; i < dim; i++) {
              for(uint b = 0; b < numNodes; b++) {
                for(uint j = 0; j < dim; j++) {
                  Real tempStiffness = 0.0;
                  for(uint M = 0; M < dim; M++) {
                    for(uint N = 0; N < dim; N++) {
                      tempStiffness += FKres.K.get(i, M, j, N)*elements[e]->getDN(q, a, M)*
                      elements[e]->getDN(q, b, N);
                    } // N loop
                  } // M loop
                  tempStiffness *= Vol;
                  // R->addStiffness(NodesID[a]*dim + i, NodesID[b]*dim + j, tempStiffness);
                  Kele(a*dim + i, b*dim + j) += tempStiffness;
                } // j loop
              } // b loop
            } // i loop
          } // a loop
        } // Compute stiffness matrix

        if ( R->getRequest() & DMATPROP ) {
          // Assemble dRdalpha
          for (uint alpha = 0; alpha < NumPropPerMat; alpha++) {
            for(uint a = 0; a < numNodes; a++) {
              for(uint i = 0; i < dim; i++) {
                Real tempdRdalpha = 0.0;
                for (uint J = 0; J < dim; J++) {
                  tempdRdalpha += FKres.Dmat.get(alpha,i,J) * geomEl->getDN(q, a, J);
                } // J loop
                tempdRdalpha *= Vol;
                (dRdalpha[ (_materials[e*numQP + q]->getMatID())*NumPropPerMat + alpha])( NodesID[a]*dim + i) += tempdRdalpha;
              } // i loop
            } // a loop
          } // alpha loop

        } // Compute DMATPROP

      } // QP loop

      if ( R->getRequest() & STIFFNESS ) {
        // Transform in triplets Kele
        for(uint a = 0; a < numNodes; a++) {
          for(uint i = 0; i < dim; i++) {
            for(uint b = 0; b < numNodes; b++) {
              for(uint j = 0; j < dim; j++) {
                KtripletList.push_back( Triplet<Real >( NodesID[a]*dim + i, NodesID[b]*dim + j, Kele(a*dim + i, b*dim + j) ) );
              }
            }
          }
        }
      }

    } // Element loop

    // Insert BC terms from spring
    if (_springBCflag == 1) {
      vector<Triplet<Real > >  KtripletList_FromSpring = this->applySpringBC(*R);
      KtripletList.insert( KtripletList.end(), KtripletList_FromSpring.begin(), KtripletList_FromSpring.end() );
    }

    if (_torsionalSpringBCflag == 1) {
      vector<Triplet<Real> > KtripletList_FromTorsionalSpring = this->applyTorsionalSpringBC(*R);
      KtripletList.insert(KtripletList.end(), KtripletList_FromTorsionalSpring.begin(), KtripletList_FromTorsionalSpring.end());
    }

    if (_lennardJonesBCFlag) {
      if (_makeFlexible) {
        R->resizeDoFDataStructures(_field.size());
        vector<Triplet<Real> > KtripletList_FromFlexibleMembrane = this->imposeFlexibleMembrane(*R);
        KtripletList.insert(KtripletList.end(), KtripletList_FromFlexibleMembrane.begin(), KtripletList_FromFlexibleMembrane.end());
      }
      vector<Triplet<Real> > KtripletList_FromLJ = this->imposeLennardJones(*R);
      KtripletList.insert(KtripletList.end(), KtripletList_FromLJ.begin(), KtripletList_FromLJ.end());
    }

    // Sum up all stiffness entries with the same indices
    if ( R->getRequest() & STIFFNESS ) {
      R->setStiffnessFromTriplets(KtripletList);
      R->FinalizeGlobalStiffnessAssembly();
      // cout << "Stiffness assembled" << endl;
    }

    // Add pressure/external load if any
    if (_pressureFlag == 1) {
      this->applyPressure(R);
    }

    // Add external load if any
    if (_nodalForcesFlag == 1 && (R->getRequest() & FORCE || R->getRequest() & DMATPROP) ) {
      for (int i = 0; i < _forcesID->size(); i++) {
        R->addResidual( (*_forcesID)[i], (*_forces)[i] );
      }
    }

    // Compute Gradg and Hg
    if ( R->getRequest() & DMATPROP ) {

      // First extract residual
      VectorXd Residual = VectorXd::Zero(PbDoF);
      for (int i = 0; i < PbDoF; i++) {
        Residual(i) = R->getResidual(i); // local copy
      }

      if (_resetFlag == 1) {
        for (uint alpha = 0; alpha < TotNumMatProp; alpha++) {
          R->addGradg(alpha, 2.0*dRdalpha[alpha].dot(Residual) );
          for (uint beta = 0; beta < TotNumMatProp; beta++) {
            // !!!
            // WARNING : assume W is linear in alpha!!!!!
            // !!!
            HgtripletList.push_back( Triplet<Real >( alpha, beta,  2.0*dRdalpha[alpha].dot(dRdalpha[beta]) ) );
          } // alpha loop
        } // beta loop
        R->setHgFromTriplets(HgtripletList);
      }
      else {
        for (uint alpha = 0; alpha < TotNumMatProp; alpha++) {
          R->addGradg(alpha, 2.0*dRdalpha[alpha].dot(Residual) );
          for (uint beta = 0; beta < TotNumMatProp; beta++) {
            // !!!
            // WARNING : assume W is linear in alpha!!!!!
            // !!!
            R->addHg(alpha, beta,  2.0*dRdalpha[alpha].dot(dRdalpha[beta]) );
          } // alpha loop
        } // beta loop
      }




    } // Compute Gradg and Hg

  } // Compute Mechanics Model

  void MechanicsModel::finalizeCompute() {
    // The following code keeps track of \bar{x} which is used as the anchor point
    // for the linear springs
    if (_springBCflag) {
      // Compute new normals
      computeNormals();
      computeAnchorPoints(); 
    }

    if (_lennardJonesBCFlag) {
      if (_makeFlexible)
        computeLJNormals();
      findNearestRigidNeighbors();
    }
  } // finalizeCompute Mechanics Model



  void MechanicsModel::applyPressure(Result * R) {

    const vector<GeomElement* > elements = _surfaceMesh->getElements();

    // Loop through elements
    for(int e = 0; e < elements.size(); e++)
    {
      GeomElement* geomEl = elements[e];
      const vector<int  >& NodesID = geomEl->getNodesID();
      const int numQP    = geomEl->getNumberOfQuadPoints();
      const int numNodes = NodesID.size();

      // Loop over quadrature points
      for(int q = 0; q < numQP; q++) {

        // Compute normal based on _prevField and displacement
        Vector3d a1 = Vector3d::Zero(), a2 = Vector3d::Zero(), a3 = Vector3d::Zero(), u = Vector3d::Zero();
        for (int a = 0; a < NodesID.size(); a++) {
          int nodeID = NodesID[a];
          Vector3d xa_prev, xa_curr;
          xa_prev << _prevField[nodeID*3], _prevField[nodeID*3+1], _prevField[nodeID*3+2];
          xa_curr << _field[nodeID*3], _field[nodeID*3+1], _field[nodeID*3+2];
          a1 += xa_prev*geomEl->getDN(q, a, 0);
          a2 += xa_prev*geomEl->getDN(q, a, 1);

          u += (xa_curr - _surfaceMesh->getX(nodeID))*geomEl->getN(q, a);
        }
        a3 = a1.cross(a2);

        // Surface associated with QP q
        Real Area = a3.norm();
        a3 /= Area;
        Area *= geomEl->getQPweights(q);
        // cout << "Area = " << Area << endl;

        // Compute energy
        if (R->getRequest() & ENERGY) {
          R->addEnergy( _pressure*Area*a3.dot(u) );
        }

        // Compute Residual
        if ( (R->getRequest() & FORCE) || (R->getRequest() & DMATPROP) ) {
          for(uint a = 0; a < NodesID.size(); a++) {
            for(uint i = 0; i < 3; i++) {
              R->addResidual(NodesID[a]*3+i, _pressure * Area * a3(i) * geomEl->getN(q, a));
              // cout << NodesID[a] << " " << geomEl->getN(q, a) << endl;
            } // i loop
          } // a loop
        } // Internal force loop

      } // loop over QP
    } // loop over elements

  } // apply pressure




  vector<Triplet<Real > > MechanicsModel::applySpringBC(Result & R) {

    vector<Triplet<Real > > KtripletList_FromSpring;

    // Set previous field for every solution step
    // this->setPrevField();

    // Recompute normals - no change if _prevField has not changed.
    // this->computeNormals();

    // Loop through _spNodes
    for(int n = 0; n < _spNodes.size(); n++)
    {
      int NodeID = _spNodes[n];
      Vector3d xa_prev, xa_curr;
      xa_prev << _prevField[NodeID*3], _prevField[NodeID*3+1], _prevField[NodeID*3+2];
      xa_curr << _field[NodeID*3], _field[NodeID*3+1], _field[NodeID*3+2];

      // Compute energy
      if (R.getRequest() & ENERGY) {
        R.addEnergy( 0.5* _springK*pow( (xa_curr - xa_prev).dot(_spNormals[n]), 2.0) );
      }

      // Compute Residual
      if ( (R.getRequest() & FORCE) || (R.getRequest() & DMATPROP) ) {
        for(uint i = 0; i < 3; i++) {
          R.addResidual(NodeID*3+i,  _springK*_spNormals[n](i)*(xa_curr - xa_prev).dot(_spNormals[n]) );
        } // i loop
	// cout << "Spring Force:\t" << _springK*_spNormals[n](0)*(xa_curr - xa_prev).dot(_spNormals[n]) << "\t" << _springK*_spNormals[n](1)*(xa_curr - xa_prev).dot(_spNormals[n]) << "\t" << _springK*_spNormals[n](2)*(xa_curr - xa_prev).dot(_spNormals[n]) << endl;
	// cout << "Disp:\t" << xa_curr(0) - xa_prev(0) << "\t" << xa_curr(1) - xa_prev(1) << "\t" << xa_curr(2) - xa_prev(2) << endl;
      } // Internal force loop

      // Compute stiffness matrix
      if ( R.getRequest() & STIFFNESS ) {
        for(uint i = 0; i < 3; i++) {
          for(uint j = 0; j < 3; j++) {
            KtripletList_FromSpring.push_back(Triplet<Real >( NodeID*3+i, NodeID*3+j, _springK*_spNormals[n](i)*_spNormals[n](j) ));
          } // j loop
        } // i loop
      } // Stiffness loop

    } // Spring nodes loop

    return  KtripletList_FromSpring;
  } // apply SpringBC



  void MechanicsModel::computeNormals() {

    // First compute normal of any element in _spMesh
    const vector<GeomElement* > elements = _spMesh->getElements();

    vector<Vector3d > ElNormals(elements.size(), Vector3d::Zero());

    // Loop over elements
    for(int e = 0; e < elements.size(); e++)
    {
      GeomElement* geomEl = elements[e];
      const vector<int  >& NodesID = geomEl->getNodesID();
      const int numQP    = geomEl->getNumberOfQuadPoints();
      const int numNodes = NodesID.size();

      // Loop over quadrature points
      for(int q = 0; q < numQP; q++) {

        // Compute normal based on _prevField
        Vector3d a1 = Vector3d::Zero(), a2 = Vector3d::Zero(), a3 = Vector3d::Zero();
        for (int a = 0; a < NodesID.size(); a++) {
          int nodeID = NodesID[a];
          Vector3d xa_prev;
          xa_prev << _prevField[nodeID*3], _prevField[nodeID*3+1], _prevField[nodeID*3+2];
          a1 += xa_prev*geomEl->getDN(q, a, 0);
          a2 += xa_prev*geomEl->getDN(q, a, 1);
        }
        ElNormals[e] += a1.cross(a2); // Not normalized with respect to area (elements with larger area count more)
      } // loop over QP

    } // loop over elements

    // loop over _spNodes
    for (int n = 0; n < _spNodes.size(); n++) {
      // Reset normal to zero
      _spNormals[n] = Vector3d::Zero();
      // Loop over all elements sharing that node
      for (int m = 0; m < _spNodesToEle[n].size(); m++) {
        _spNormals[n] += ElNormals[_spNodesToEle[n][m]];
      }
      Real normFactor = 1.0/_spNormals[n].norm();
      _spNormals[n] *= normFactor;

      // For testing only - to be commented out
      // cout << _spNormals[n](0) << " " << _spNormals[n](1) << " " << _spNormals[n](2) << endl;
    }

  } // compute Normals



  void MechanicsModel::initSpringBC(const string SpNodes, Mesh* SpMesh, Real SpringK) {

    // Activate flag
    _springBCflag = 1;

    // Store mesh information
    _spMesh = SpMesh;

    // Store spring constant
    _springK = SpringK;

    // Store node number on the outer surface
    ifstream inp(SpNodes.c_str());
    if (!inp.is_open()) {
        cout << "** Unable to open spring file " << SpNodes << ".\n ** Exiting..." << endl;
        exit(EXIT_FAILURE);
    }

    int numSpringNodes = 0;
    inp >> numSpringNodes;

    int nodeNum = 0;
    while (inp >> nodeNum)
      _spNodes.push_back(nodeNum);

    // Checking that the number at the top of the file corresponds to the number of nodes
    assert(numSpringNodes == _spNodes.size());
    cout << "** Applying the Spring BC to " << numSpringNodes << " nodes." << endl;

    // Collect elements that share a node in _spNodes
    const vector<GeomElement* > elements = _spMesh->getElements();

    for (int n = 0; n < _spNodes.size(); n++) {
      vector<int > connected;
      for(int e = 0; e < elements.size(); e++) {
        const vector<int >& NodesID = elements[e]->getNodesID();
        for (int m = 0; m < NodesID.size(); m++) {
          if (NodesID[m] == _spNodes[n]) {
            connected.push_back(e);
            break;
          } //  check if node belong to element
        } // loop over nodes of the element
      } // loop over elements
      _spNodesToEle.push_back(connected);

      // For testing only - to be commented out
      // cout << _spNodes[n] << " ";
      // for (int i = 0; i < connected.size(); i++) {
      // 	cout << connected[i] << " ";
      // }
      // cout << endl;

    } // loop over _spNodes

    _spNormals.resize(_spNodes.size(), Vector3d::Zero());
    // Compute initial node normals
    this->computeNormals();
    this->computeAnchorPoints();

  } // InitSpringBC

  void MechanicsModel::computeAnchorPoints() {
    // Compute \bar{u}_{k+1} = x_{k+1} - \bar{x}_k
    vector <Real> ubar = _field;
    vector <Real> xbar;
    xbar = _field;
    for (int i = 0; i < _field.size(); i++)
      ubar[i] = _field[i] - _prevField[i];
    
    // Compute the tangent vector at every node      
    for(int n = 0; n < _spNodes.size(); n++) {
      Vector3d nodeTangent;
      Vector3d ubarNode;
      Vector3d xbarkNode;
      Vector3d xNode;
      ubarNode << ubar[_spNodes[n] * 3 + 0], ubar[_spNodes[n] * 3 + 1], ubar[_spNodes[n] * 3 + 2];
      xbarkNode << _prevField[_spNodes[n] * 3 + 0], _prevField[_spNodes[n] * 3 + 1], _prevField[_spNodes[n] * 3 + 2];
      xNode << _field[_spNodes[n] * 3 + 0], _field[_spNodes[n] * 3 + 1], _field[_spNodes[n] * 3 + 2];
      nodeTangent = ubarNode - (ubarNode.dot(_spNormals[n])) * _spNormals[n];
      
      Vector3d xbarkp1Node = xbarkNode + (nodeTangent.dot(xNode - xbarkNode)) * nodeTangent;
      xbar[_spNodes[n] * 3 + 0] = xbarkp1Node[0];
      xbar[_spNodes[n] * 3 + 1] = xbarkp1Node[1];
      xbar[_spNodes[n] * 3 + 2] = xbarkp1Node[2];
    }
    setPrevField(xbar);
  } // computeAnchorPoints



  // *** BEGIN: MATERIAL PARAMETER ID FUNCTIONS *** //
  void MechanicsModel::checkDmat(EigenResult * R, Real perturbationFactor, Real hM, Real tol)
  {

    // Perturb initial config - gradg and Hg are zero at F = I
    const uint nodeNum   = _myMesh->getNumberOfNodes();
    const uint nLocalDoF = nodeNum*_nodeDoF;

    // Perturb field randomly to change from reference configuration
    // Save perturbed field to set the configuration back to reference
    // at the end of the test
    vector<Real > perturb(nLocalDoF, 0.0);
    srand( time(NULL) );
    for(uint a = 0; a < nodeNum; a++) {
      for(uint i = 0; i < _nodeDoF; i++) {
        Real randomNum =  perturbationFactor*(Real(rand())/RAND_MAX - 0.5);
        perturb[a*_nodeDoF + i] = randomNum;
        this->linearizedUpdate(a, i, randomNum);
      }
    }

    Real error = 0.0, norm = 0.0;

    set<MechanicsMaterial *> UNIQUEmaterials;
    for (uint i = 0; i < _materials.size(); i++)
    UNIQUEmaterials.insert(_materials[i]);

    cout << "Number of unique materials = " <<  UNIQUEmaterials.size() << endl;
    R->setRequest(8); // First compute gradg and Hg numerically
    this->compute(R);

    // Test gradg //
    R->setRequest(2); // Reset result request so that only forces are computed
    for ( set<MechanicsMaterial *>::iterator itMat =  UNIQUEmaterials.begin();
    itMat != UNIQUEmaterials.end(); itMat++ )
    {
      vector<Real > MatProp = (*itMat)->getMaterialParameters();
      int MatID = (*itMat)->getMatID();
      for(int m = 0; m < MatProp.size(); m++) {
        // Perturb +hM the material property alpha
        MatProp[m] += hM;
        // Reset matProp in the materials with MatProp[m]
        (*itMat)->setMaterialParameters(MatProp);
        // Compute R
        this->compute(R);
        Real RTRplus = (R->_residual)->dot(*R->_residual);
        // Perturb -2hM the material property alpha
        MatProp[m] -= 2.0*hM;
        // Reset matProp in the materials with MatProp[m]
        (*itMat)->setMaterialParameters(MatProp);
        // Compute R
        this->compute(R);
        Real RTRminus = (R->_residual)->dot(*R->_residual);

        // Bring back to original value of alpha
        MatProp[m] += hM;
        // Reset matProp in all the materials with MatProp[m]
        (*itMat)->setMaterialParameters(MatProp);

        error += pow( (RTRplus-RTRminus)/(2.0*hM) -
        (*R->_Gradg)( MatID*MatProp.size() + m), 2.0 );
        norm += pow( (*R->_Gradg)(  MatID*MatProp.size() + m), 2.0 );

        // cout << (*R->_Gradg)( MatID*MatProp.size() + m) << " " << (RTRplus-RTRminus)/(2.0*hM) << endl;
      } // Loop over m
    } // Loop over unique materials
    error = sqrt(error);
    norm  = sqrt(norm);

    if ( abs(error) < norm * tol) {
      cout << "** Gradg consistency check PASSED" << endl;
      cout << "** Error: " << error << " Norm: " << norm  << " Norm*tol: " << norm*tol << endl;
    }
    else {
      cout << "** Gradg consistency check FAILED" << endl;
      cout << "** Error: " << error << " Norm: " << norm << " Norm*tol: " << norm*tol << endl;
    }



    // Test Hg //
    error = 0.0; norm = 0.0;
    R->setRequest(8);
    this->compute(R);
    SparseMatrix<Real > HgAn = *R->_Hg;
    for ( set<MechanicsMaterial *>::iterator itMatA =  UNIQUEmaterials.begin(); itMatA != UNIQUEmaterials.end(); itMatA++ )
    {
      vector<Real > MatPropA = (*itMatA)->getMaterialParameters();
      int MatIDA = (*itMatA)->getMatID();

      for(int mA = 0; mA < MatPropA.size(); mA++) {

        for ( set<MechanicsMaterial *>::iterator itMatB =  UNIQUEmaterials.begin(); itMatB != UNIQUEmaterials.end(); itMatB++ )
        {
          vector<Real > MatPropB = (*itMatB)->getMaterialParameters();
          int MatIDB = (*itMatB)->getMatID();

          for(int mB = 0; mB < MatPropB.size(); mB++) {

            // Perturb +hM the material property alpha
            MatPropB[mB] += hM;
            // Reset matProp in the materials with MatProp[m]
            (*itMatB)->setMaterialParameters(MatPropB);
            // Compute R
            this->compute(R);
            Real GradPlus = R->getGradg( MatIDA*MatPropA.size() + mA);

            // Perturb -2hM the material property alpha
            MatPropB[mB] -= 2.0*hM;
            // Reset matProp in the materials with MatProp[m]
            (*itMatB)->setMaterialParameters(MatPropB);
            // Compute R
            this->compute(R);
            Real GradMinus = R->getGradg( MatIDA*MatPropA.size() + mA);

            // Bring back to original value of alpha
            MatPropB[mB] += hM;
            // Reset matProp in all the materials with MatPropB[m]
            (*itMatB)->setMaterialParameters(MatPropB);

            error += pow( (GradPlus - GradMinus)/(2.0*hM) -
            HgAn.coeff( MatIDA*MatPropA.size() + mA, MatIDB*MatPropB.size() + mB ), 2.0);
            norm += pow( HgAn.coeff( MatIDA*MatPropA.size() + mA, MatIDB*MatPropB.size() + mB ) , 2.0 );

            // cout << (*R->_Gradg)( MatID*MatProp.size() + m) << " " << (RTRplus-RTRminus)/(2.0*hM) << endl;

          } // Loop over mB
        } // Loop over unique materials B
      } // Loop over mA
    } // Loop over unique materials A
    error = sqrt(error);
    norm  = sqrt(norm);

    if ( abs(error) < norm * tol) {
      cout << "** Hg consistency check PASSED" << endl;
      cout << "** Error: " << error << " Norm: " << norm  << " Norm*tol: " << norm*tol << endl;
    }
    else {
      cout << "** Hg consistency check FAILED" << endl;
      cout << "** Error: " << error << " Norm: " << norm << " Norm*tol: " << norm*tol << endl;
    }



    // Reset field to initial values
    for(uint a = 0; a < nodeNum; a++) {
      for(uint i = 0; i < _nodeDoF; i++) {
        this->linearizedUpdate(a, i, -perturb[a*_nodeDoF + i]);
      }
    }

  } // Check consistency of gradg and Hg - checkDmat

  // *** END: MATERIAL PARAMETER ID FUNCTIONS *** //



  // Compute volume functions - current and reference volumes
  Real MechanicsModel::computeRefVolume()
  {
    Real RefVol = 0.0;
    const vector<GeomElement* > elements = _myMesh->getElements();
    const int NumEl = elements.size();

    // Loop through elements, also through material points array, which is unrolled
    for(int e = 0; e < NumEl; e++)
    {
      GeomElement* geomEl = elements[e];
      const int numQP    = geomEl->getNumberOfQuadPoints();
      // Loop over quadrature points
      for(int q = 0; q < numQP; q++) {
        // Volume associated with QP q
        RefVol += geomEl->getQPweights(q);
      } // Loop over QP
    } // Loop over elements

    return RefVol;
  }

  Real MechanicsModel::computeCurrentVolume()
  {
    Real CurrVol = 0.0;
    const vector<GeomElement* > elements = _myMesh->getElements();
    const int NumEl = elements.size();

    // Loop through elements, also through material points array, which is unrolled
    for(int e = 0; e < NumEl; e++)
    {
      GeomElement* geomEl = elements[e];
      const int numQP    = geomEl->getNumberOfQuadPoints();

      // F at each quadrature point are computed at the same time in one element
      vector<Matrix3d > Flist(numQP, Matrix3d::Zero());
      // Compute deformation gradients for current element
      this->computeDeformationGradient(Flist, geomEl);

      // Loop over quadrature points
      for(int q = 0; q < numQP; q++) {
        // Volume associated with QP q
        if (isnan(Flist[q].determinant()))
        {
          // this->computeDeformationGradient(Flist, geomEl);
          // cout << Flist[q] << endl;
          cout << "Element with nan: " << e << endl;
        }
        CurrVol += ( geomEl->getQPweights(q) * Flist[q].determinant() );
      } // Loop over QP
    } // Loop over elements

    return CurrVol;
  }

  // Functions for applying Torsional Spring BC
  void MechanicsModel::initTorsionalSpringBC(const string torsionalSpringNodes, Real torsionalSpringK) {
    // Activate flag
     _torsionalSpringBCflag = 1;

    // Store node number on the outer surface
    ifstream inp(torsionalSpringNodes.c_str());
    if (!inp.is_open()) {
	cout << "** Unable to open Torsional spring file " << torsionalSpringNodes << ".\n ** Exiting..." << endl;
	exit(EXIT_FAILURE);
    }
    int numTorsionalSpringNodes = 0;
    inp >> numTorsionalSpringNodes;

    int nodeNum = 0;
    while (inp >> nodeNum)
      _torsionalSpringNodes.push_back(nodeNum);

    // Checking that the number at the top of the file corresponds to the number of nodes
    assert(numTorsionalSpringNodes == _torsionalSpringNodes.size());
    cout << "** Applying the Torsional Spring BC to " << numTorsionalSpringNodes << " nodes." << endl;

    _torsionalSpringK = torsionalSpringK;
    
    // Compute Centroid to compute the tangential vector
    computeCentroid();

    // Compute the tangential vectors
    computeTangents();
  }

  void MechanicsModel::computeCentroid() {
    double xavg = 0.0;
    double yavg = 0.0;
    double zavg = 0.0;
    for (int n = 0; n < _myMesh->getNumberOfNodes(); n++) {
      xavg += _myMesh->getX(n)(0);
      yavg += _myMesh->getX(n)(1);
      zavg += _myMesh->getX(n)(2);
    }
    xavg = xavg/_myMesh->getNumberOfNodes();
    yavg = yavg/_myMesh->getNumberOfNodes();
    zavg = zavg/_myMesh->getNumberOfNodes();
    _centroidLocation(0) = xavg; _centroidLocation(1) = yavg; _centroidLocation(2) = zavg;
  }

  void MechanicsModel::computeTangents() {
    for(int n = 0; n < _torsionalSpringNodes.size(); n++)
    {
      int NodeID = _torsionalSpringNodes[n];
      Vector3d xa_reference(_myMesh->getX(NodeID)(0), _myMesh->getX(NodeID)(1), _myMesh->getX(NodeID)(2));

      Vector2d normal = Vector2d::Zero();
      Vector3d tempNormal = xa_reference -_centroidLocation;
      normal(0) = tempNormal(0); normal(1) = tempNormal(1);
      normal = normal/normal.norm();

      Vector3d tangent(-1.0 * normal(1), normal(0), 0.0);
      _spTangents.push_back(tangent);
    }
  }

  vector<Triplet<Real> > MechanicsModel::applyTorsionalSpringBC(Result & R) {
    vector<Triplet<Real > > KtripletList_FromSpring;

    // Loop through _spNodes
    for(int n = 0; n < _torsionalSpringNodes.size(); n++)
    {
      int NodeID = _torsionalSpringNodes[n];
      Vector3d xa_reference, xa_curr;
      xa_reference << _myMesh->getX(NodeID)(0), _myMesh->getX(NodeID)(1), _myMesh->getX(NodeID)(2);
      xa_curr << _field[NodeID*3], _field[NodeID*3+1], _field[NodeID*3+2];

      // Compute energy
      if (R.getRequest() & ENERGY) {
        R.addEnergy( 0.5* _torsionalSpringK*pow( (xa_curr - xa_reference).dot(_spTangents[n]), 2.0) );
      }

      // Compute Residual
      if ( (R.getRequest() & FORCE) || (R.getRequest() & DMATPROP) ) {
        for(uint i = 0; i < 3; i++) {
          R.addResidual(NodeID*3+i,  _torsionalSpringK*_spTangents[n](i)*(xa_curr - xa_reference).dot(_spTangents[n]) );
        } // i loop
      } // Internal force loop

      // Compute stiffness matrix
      if ( R.getRequest() & STIFFNESS ) {
        for(uint i = 0; i < 3; i++) {
          for(uint j = 0; j < 3; j++) {
            KtripletList_FromSpring.push_back(Triplet<Real >( NodeID*3+i, NodeID*3+j, _torsionalSpringK*_spTangents[n](i)*_spTangents[n](j) ));
          } // j loop
        } // i loop
      } // Stiffness loop
    } // Spring nodes loop

    return  KtripletList_FromSpring;
  }

  void MechanicsModel::initializeLennardJonesBC(BCPotentialType BCType, const string bodyPotentialBoundaryNodesFile, Mesh* rigidPotentialBoundaryMesh, Mesh* bodyPotentialBoundaryMesh, Real searchRadius, Real depthPotentialWell, Real minDistance) {
    _lennardJonesBCFlag = 1;
    _potentialType = BCType;
    _rigidPotentialBoundaryMesh = rigidPotentialBoundaryMesh;
    _bodyPotentialBoundaryMesh = bodyPotentialBoundaryMesh;
    _searchRadius = searchRadius;
    _depthPotentialWell = depthPotentialWell;
    _minDistance = minDistance;
    _useConstantMinimumDistance = true;
    _useNumberNeighborFlag = false;

    for (int i = 0; i < _rigidPotentialBoundaryMesh->getNumberOfNodes(); i++) 
      _minDistancePerRigidNode.push_back(_minDistance);

    // Read in the surface Nodes
    ifstream inp(bodyPotentialBoundaryNodesFile.c_str());
    if (!inp.is_open()) {
        cout << "** Unable to open LJ Potential Boundary Nodes file " << bodyPotentialBoundaryNodesFile << ".\n ** Exiting..." << endl;
        exit(EXIT_FAILURE);
    }

    int numLJNodes = 0;
    inp >> numLJNodes;

    int nodeNum = 0;
    while (inp >> nodeNum)
      _bodyPotentialBoundaryNodes.push_back(nodeNum);

    // Checking that the number at the top of the file corresponds to the number of nodes
    assert(numLJNodes == _bodyPotentialBoundaryNodes.size());
    cout << "** Applying the Lennard-Jones Potential BC to " << numLJNodes << " nodes." << endl;

    // Compute normals - For now we can use the computeNormals function from the linear spring methods
    const vector<GeomElement* > elements = _rigidPotentialBoundaryMesh->getElements();

    for (int n = 0; n < _rigidPotentialBoundaryMesh->getNumberOfNodes(); n++) {
      vector<int > connected;
      for(int e = 0; e < elements.size(); e++) {
        const vector<int >& NodesID = elements[e]->getNodesID();
        for (int m = 0; m < NodesID.size(); m++) {
          if (NodesID[m] == n) {
            connected.push_back(e);
            break;
          } //  check if node belong to element
        } // loop over nodes of the element
      } // loop over elements
      _LJNodesToEle.push_back(connected);
    }
    _rigidSurfaceNormals.resize(_rigidPotentialBoundaryMesh->getNumberOfNodes(), Vector3d::Zero());
    // Compute initial node normals
    this->computeLJNormals();

    // Initialize Rigid Neighbors vector:
    _rigidNeighbors.resize(_bodyPotentialBoundaryNodes.size());
    this->findNearestRigidNeighbors();

  }

  
  void MechanicsModel::initializeMembraneStiffness(double membraneStiffness) {
    _makeFlexible = true;
    _membraneStiffnessCoefficient = membraneStiffness;

    // Steps:
    // - Get number of nodes from mesh
    // - Resize the field vector
    int numMembraneMeshNodes = _rigidPotentialBoundaryMesh->getNumberOfNodes();
    for (int node_iter = 0; node_iter < numMembraneMeshNodes; node_iter++) {
      for (int dim_iter = 0; dim_iter < _nodeDoF; dim_iter++) {
	_field.push_back(_rigidPotentialBoundaryMesh->getX(node_iter, dim_iter));
      }
    }
    this->computeLJNormals();
  } // End initializeMembraneStiffness 

  vector<Triplet<Real> > MechanicsModel::imposeLennardJones(Result& R) {
    // 1. Loop through every surface node
    // 2. Find neighbors within search radius
    // 3. Compute the force associated with that neighbor
    // 4. Fill in Results
    
    vector<Triplet<Real > > KtripletList_FromLJ;

    // Loop through every surface node
    int numSurfaceNodes = _bodyPotentialBoundaryNodes.size();
    int dim = _rigidPotentialBoundaryMesh->getDimension();
    int numMainNodes = _myMesh->getNumberOfNodes();
    for(int n = 0; n < numSurfaceNodes; n++)
    {
      int NodeID = _bodyPotentialBoundaryNodes[n];
      Vector3d xa_body, xa_rigid;
      xa_body << _field[NodeID*3 + 0],  _field[NodeID*3 + 1],  _field[NodeID*3 + 2];
      for (int node_rigid_iter = 0; node_rigid_iter < _rigidNeighbors[n].size(); node_rigid_iter++) {
	int node_rigid = _rigidNeighbors[n][node_rigid_iter];
	if (_makeFlexible)
          xa_rigid << _field[numMainNodes * dim + node_rigid * dim + 0], _field[numMainNodes * dim + node_rigid * dim + 1], _field[numMainNodes * dim + node_rigid * dim + 2];
        else
          xa_rigid << _rigidPotentialBoundaryMesh->getX(node_rigid)(0), _rigidPotentialBoundaryMesh->getX(node_rigid)(1), _rigidPotentialBoundaryMesh->getX(node_rigid)(2);
        
        // Check if within search radius
        Vector3d gapVector = xa_rigid - xa_body;
        double gapDistance = gapVector.norm();
        Real r_e = gapVector.dot(_rigidSurfaceNormals[node_rigid]);
	// cout << r_e << endl;

	// Compute energy
	if (R.getRequest() & ENERGY) {
	  // Real r_e = gapVectorNormal.norm();
	  // minDistance is the distance where the energy is a minimum
	  // r_e is the current distance between rigid and body surfaces
	  // IF YOU USE THIS MAKE SURE YOU CHECK THE SIGNS
	  
	  {
	    // LENNARD-JONES needs work
	    // double tempW = _depthPotentialWell * (pow(_minDistancePerRigidNode[node_rigid_iter]/r_e, 12.0) - 2.0 * pow(_minDistancePerRigidNode[node_rigid_iter]/r_e, 6.0));
	    // Multiplying by -1 because this is really acting like an external energy
	    // R.addEnergy(-1.0 * tempW);
	  }  

	  if (_potentialType == QUADRATIC) {
	    // Quadratic Spring
	    R.addEnergy(  (1.0/_rigidNeighbors[n].size()) * 0.5 * _depthPotentialWell * pow( gapVector.dot(_rigidSurfaceNormals[node_rigid]) - _minDistancePerRigidNode[node_rigid_iter], 2.0) );
	  }
	  else if(_potentialType == QUARTIC) {	
	    // Quartic Spring
	    R.addEnergy(  (1.0/_rigidNeighbors[n].size()) * 0.5 * _depthPotentialWell * pow( gapVector.dot(_rigidSurfaceNormals[node_rigid]) - _minDistancePerRigidNode[node_rigid_iter], 4.0) );
	  }
	  else {
	    cout << "** Error: BCPotentialType not implemented." << endl;
	    exit(EXIT_FAILURE);
	  }
	} // end ENERGY
  
	// Compute Residual
	if (R.getRequest() & FORCE) {
	  for(uint i = 0; i < 3; i++) {
	    {
	      // IF YOU USE THIS MAKE SURE YOU CHECK THE SIGNS - AND FLEXIBLE PART ISN'T COMPLETE
	      // double tempFactor = -12 * _depthPotentialWell/r_e * (pow(_minDistancePerRigidNode[node_rigid_iter]/r_e, 12.0) - pow(_minDistancePerRigidNode[node_rigid_iter]/r_e, 6.0));
	      // Multiplying by -1 because this is an external force
	      // R.addResidual(NodeID*3+i, -1.0 * tempFactor * _rigidSurfaceNormals[node_rigid](i));
	    }

	    if (_potentialType == QUADRATIC) {
	      // Quadratic Spring
	      R.addResidual(NodeID*3+i, (-1.0/_rigidNeighbors[n].size()) * _depthPotentialWell * _rigidSurfaceNormals[node_rigid](i) * (gapVector.dot(_rigidSurfaceNormals[node_rigid]) - _minDistancePerRigidNode[node_rigid_iter]));
	      if (_makeFlexible)
	        R.addResidual(numMainNodes * dim + node_rigid * dim + i, (1.0/_rigidNeighbors[n].size()) * _depthPotentialWell * _rigidSurfaceNormals[node_rigid](i) * (gapVector.dot(_rigidSurfaceNormals[node_rigid]) - _minDistancePerRigidNode[node_rigid_iter]));
	    }
	    else if (_potentialType == QUARTIC) {
	      // Quartic Spring
	      R.addResidual(NodeID*3+i, (-1.0/_rigidNeighbors[n].size()) * 2.0 * _depthPotentialWell * _rigidSurfaceNormals[node_rigid](i) * pow(gapVector.dot(_rigidSurfaceNormals[node_rigid]) - _minDistancePerRigidNode[node_rigid_iter], 3.0));
	      if (_makeFlexible)
	        R.addResidual(numMainNodes * dim + node_rigid * dim + i, (1.0/_rigidNeighbors[n].size()) * 2.0 * _depthPotentialWell * _rigidSurfaceNormals[node_rigid](i) * pow(gapVector.dot(_rigidSurfaceNormals[node_rigid]) - _minDistancePerRigidNode[node_rigid_iter], 3.0));
             } 
	     else {
               cout << "** Error: BCPotentialType not implemented." << endl;
               exit(EXIT_FAILURE);
             }
	  } // i loop
	} // Internal force loop

          // Compute stiffness matrix
	if ( R.getRequest() & STIFFNESS ) {
	  for(uint i = 0; i < 3; i++) {
	    for(uint j = 0; j < 3; j++) {

	      {
	        // IF YOU USE THIS MAKE SURE YOU CHECK THE SIGNS - AND FLEXIBLE PART ISN'T COMPLETE
	        // double tempFactor = 12 * _depthPotentialWell/pow(r_e, 2.0) * (13.0 * pow(_minDistancePerRigidNode[node_rigid_iter]/r_e, 12.0) - 7.0 * pow(_minDistancePerRigidNode[node_rigid_iter]/r_e, 6.0));
	        // KtripletList_FromLJ.push_back(Triplet<Real >( NodeID*3+i, NodeID*3+j,-1.0 * tempFactor * _rigidSurfaceNormals[node_rigid](i) * _rigidSurfaceNormals[node_rigid](j)));
	
	      }

	      if (_potentialType == QUADRATIC) {
	        // Quadratic Spring  
	        KtripletList_FromLJ.push_back(Triplet<Real >( NodeID*3+i, NodeID*3+j, (1.0/_rigidNeighbors[n].size()) * _depthPotentialWell*_rigidSurfaceNormals[node_rigid](i)*_rigidSurfaceNormals[node_rigid](j) ));
	        if (_makeFlexible) {
                  KtripletList_FromLJ.push_back(Triplet<Real >( numMainNodes * dim + node_rigid * dim + i, numMainNodes * dim + node_rigid * dim + j,  (1.0/_rigidNeighbors[n].size()) * _depthPotentialWell*_rigidSurfaceNormals[node_rigid](i)*_rigidSurfaceNormals[node_rigid](j) ));
                  KtripletList_FromLJ.push_back(Triplet<Real >( NodeID*3+i, numMainNodes * dim + node_rigid * dim + j, (-1.0/_rigidNeighbors[n].size()) * _depthPotentialWell*_rigidSurfaceNormals[node_rigid](i)*_rigidSurfaceNormals[node_rigid](j) ));
                  KtripletList_FromLJ.push_back(Triplet<Real >( numMainNodes * dim + node_rigid * dim + i, NodeID*3+j, (-1.0/_rigidNeighbors[n].size()) * _depthPotentialWell*_rigidSurfaceNormals[node_rigid](i)*_rigidSurfaceNormals[node_rigid](j) ));
                }
	      }
	      else if (_potentialType == QUARTIC) {
	        // Quartic Spring
	        KtripletList_FromLJ.push_back(Triplet<Real >( NodeID*3+i, NodeID*3+j, (1.0/_rigidNeighbors[n].size()) * 6.0 * _depthPotentialWell*_rigidSurfaceNormals[node_rigid](i)*_rigidSurfaceNormals[node_rigid](j) * pow(gapVector.dot(_rigidSurfaceNormals[node_rigid]) - _minDistancePerRigidNode[node_rigid_iter], 2.0) ));
	        if (_makeFlexible) {
	          KtripletList_FromLJ.push_back(Triplet<Real >( numMainNodes * dim + node_rigid * dim + i, numMainNodes * dim + node_rigid * dim + j, (1.0/_rigidNeighbors[n].size()) * 6.0 * _depthPotentialWell*_rigidSurfaceNormals[node_rigid](i)*_rigidSurfaceNormals[node_rigid](j) * pow(gapVector.dot(_rigidSurfaceNormals[node_rigid]) - _minDistancePerRigidNode[node_rigid_iter], 2.0) ));
	          KtripletList_FromLJ.push_back(Triplet<Real >( NodeID*3+i, numMainNodes * dim + node_rigid * dim + j, (-1.0/_rigidNeighbors[n].size()) * 6.0 * _depthPotentialWell*_rigidSurfaceNormals[node_rigid](i)*_rigidSurfaceNormals[node_rigid](j) * pow(gapVector.dot(_rigidSurfaceNormals[node_rigid]) - _minDistancePerRigidNode[node_rigid_iter], 2.0) ));
	          KtripletList_FromLJ.push_back(Triplet<Real >( numMainNodes * dim + node_rigid * dim + i, NodeID*3+j, (-1.0/_rigidNeighbors[n].size()) * 6.0 * _depthPotentialWell*_rigidSurfaceNormals[node_rigid](i)*_rigidSurfaceNormals[node_rigid](j) * pow(gapVector.dot(_rigidSurfaceNormals[node_rigid]) - _minDistancePerRigidNode[node_rigid_iter], 2.0) ));
 	        }
	      } // End Quartic
	      else {
               cout << "** Error: BCPotentialType not implemented." << endl;
               exit(EXIT_FAILURE);
             }
	    } // j loop
	  } // i loop
	} // Stiffness loop
      } // Loop over rigid nodes
    } // Spring nodes loop

    return  KtripletList_FromLJ;
    
  }

  vector<Triplet<Real> > MechanicsModel::imposeFlexibleMembrane(Result& R) {
    vector<Triplet<Real> > KtripletList_FromFlexibleBoundary;
    Matrix3d identityMat = Matrix3d::Identity();
    int numMembraneMeshNodes = _rigidPotentialBoundaryMesh->getNumberOfNodes();
    int numMembraneMeshElements = _rigidPotentialBoundaryMesh->getNumberOfElements();
    int numMainNodes = _myMesh->getNumberOfNodes();
    const vector<GeomElement*> BCelements = _rigidPotentialBoundaryMesh->getElements();

    for (int el_iter = 0; el_iter < numMembraneMeshElements; el_iter++) {
      vector<int> elNodeIds = BCelements[el_iter]->getNodesID();
      int numNodesPerEl = elNodeIds.size();

      vector <Vector3d> elCurrentNodePositions;
      vector <Vector3d> elReferenceNodePositions;
      vector<Vector3d> currentEdgeVectors;
      vector<Vector3d> referenceEdgeVectors;

      // Store the nodal positions of each vertex
      for (int i = 0; i < elNodeIds.size(); i++) {
        Vector3d tempCurr, tempRef;
        tempCurr << _field[numMainNodes * _nodeDoF + elNodeIds[i] * _nodeDoF + 0], _field[numMainNodes * _nodeDoF + elNodeIds[i] * _nodeDoF + 1], _field[numMainNodes * _nodeDoF + elNodeIds[i] * _nodeDoF + 2];
        tempRef  << _rigidPotentialBoundaryMesh->getX(elNodeIds[i])(0), _rigidPotentialBoundaryMesh->getX(elNodeIds[i])(1), _rigidPotentialBoundaryMesh->getX(elNodeIds[i])(2);
        elCurrentNodePositions.push_back(tempCurr);
        elReferenceNodePositions.push_back(tempRef);
      }

      if (numNodesPerEl == 3) { // Linear Tri
        vector <pair <int,int> > vertexes;
        pair<int,int> temp;
        temp.first = 0; temp.second = 1; vertexes.push_back(temp);
        temp.first = 1; temp.second = 2; vertexes.push_back(temp);
        temp.first = 2; temp.second = 0; vertexes.push_back(temp);

        for (int i = 0; i < vertexes.size(); i++) {
          Vector3d tempCurr = elCurrentNodePositions[vertexes[i].first] - elCurrentNodePositions[vertexes[i].second];
          Vector3d tempRef  = elReferenceNodePositions[vertexes[i].first] - elReferenceNodePositions[vertexes[i].second];
          currentEdgeVectors.push_back(tempCurr);
          referenceEdgeVectors.push_back(tempRef);
        }

        if (R.getRequest() & ENERGY) {
          double tempEnergy = 0.0;
	
	  for (int edge_iter = 0; edge_iter < currentEdgeVectors.size(); edge_iter++)
	    tempEnergy += _membraneStiffnessCoefficient/2.0 * pow(currentEdgeVectors[edge_iter].norm() - referenceEdgeVectors[edge_iter].norm(), 2.0);
          R.addEnergy(tempEnergy);
        } // End ENERGY
	if (R.getRequest() & FORCE) {
	  for (int i = 0; i < _nodeDoF; i++) {
	    for (int edge_iter = 0; edge_iter < currentEdgeVectors.size(); edge_iter++) {
	      int nodeID1 = elNodeIds[vertexes[edge_iter].first];
              int nodeID2 = elNodeIds[vertexes[edge_iter].second];

	      R.addResidual(numMainNodes * _nodeDoF + nodeID1 * _nodeDoF + i,  1.0 * _membraneStiffnessCoefficient * (currentEdgeVectors[edge_iter](i) - referenceEdgeVectors[edge_iter].norm() * currentEdgeVectors[edge_iter](i) / currentEdgeVectors[edge_iter].norm()));
              R.addResidual(numMainNodes * _nodeDoF + nodeID2 * _nodeDoF + i, -1.0 * _membraneStiffnessCoefficient * (currentEdgeVectors[edge_iter](i) - referenceEdgeVectors[edge_iter].norm() * currentEdgeVectors[edge_iter](i) / currentEdgeVectors[edge_iter].norm()));

	    }
	  }
	}
	if (R.getRequest() & STIFFNESS) {
	  for (int i = 0; i < _nodeDoF; i++) {
	    for (int j = 0; j < _nodeDoF; j++) {
	      for (int edge_iter = 0; edge_iter < currentEdgeVectors.size(); edge_iter++) {
		int nodeID1 = elNodeIds[vertexes[edge_iter].first];
                int nodeID2 = elNodeIds[vertexes[edge_iter].second];

	        KtripletList_FromFlexibleBoundary.push_back(Triplet<Real >( numMainNodes * _nodeDoF + nodeID1 * _nodeDoF + i, numMainNodes * _nodeDoF + nodeID1 * _nodeDoF + j, _membraneStiffnessCoefficient * (identityMat(i,j) - referenceEdgeVectors[edge_iter].norm() * (identityMat(i,j) / currentEdgeVectors[edge_iter].norm() - currentEdgeVectors[edge_iter](i) * currentEdgeVectors[edge_iter](j)/ pow(currentEdgeVectors[edge_iter].norm(),3)))));

	        KtripletList_FromFlexibleBoundary.push_back(Triplet<Real >( numMainNodes * _nodeDoF + nodeID2 * _nodeDoF + i, numMainNodes * _nodeDoF + nodeID2 * _nodeDoF + j, _membraneStiffnessCoefficient * (identityMat(i,j) - referenceEdgeVectors[edge_iter].norm() * (identityMat(i,j) / currentEdgeVectors[edge_iter].norm() - currentEdgeVectors[edge_iter](i) * currentEdgeVectors[edge_iter](j)/ pow(currentEdgeVectors[edge_iter].norm(),3)))));

		KtripletList_FromFlexibleBoundary.push_back(Triplet<Real >( numMainNodes * _nodeDoF + nodeID1 * _nodeDoF + i, numMainNodes * _nodeDoF + nodeID2 * _nodeDoF + j, -1.0 * _membraneStiffnessCoefficient * (identityMat(i,j) - referenceEdgeVectors[edge_iter].norm() * (identityMat(i,j) / currentEdgeVectors[edge_iter].norm() - currentEdgeVectors[edge_iter](i) * currentEdgeVectors[edge_iter](j)/ pow(currentEdgeVectors[edge_iter].norm(),3)))));

		KtripletList_FromFlexibleBoundary.push_back(Triplet<Real >( numMainNodes * _nodeDoF + nodeID2 * _nodeDoF + i, numMainNodes * _nodeDoF + nodeID1 * _nodeDoF + j, -1.0 * _membraneStiffnessCoefficient * (identityMat(i,j) - referenceEdgeVectors[edge_iter].norm() * (identityMat(i,j) / currentEdgeVectors[edge_iter].norm() - currentEdgeVectors[edge_iter](i) * currentEdgeVectors[edge_iter](j)/ pow(currentEdgeVectors[edge_iter].norm(),3)))));

	      } // End edgeLoop Iter
	    } // End j loop
	  } // End i loop
        } // End Stiffness
      } // 3 Nodes Per El
      else if (numNodesPerEl == 6) { // Quadratic Tri
	vector <pair <int,int> > vertexes;
	pair<int,int> temp;
	temp.first = 0; temp.second = 3; vertexes.push_back(temp);
	temp.first = 3; temp.second = 1; vertexes.push_back(temp);
	temp.first = 1; temp.second = 4; vertexes.push_back(temp);
	temp.first = 4; temp.second = 2; vertexes.push_back(temp);
	temp.first = 2; temp.second = 5; vertexes.push_back(temp);
	temp.first = 5; temp.second = 0; vertexes.push_back(temp);

	temp.first = 3; temp.second = 4; vertexes.push_back(temp);
	temp.first = 4; temp.second = 5; vertexes.push_back(temp);
	temp.first = 5; temp.second = 3; vertexes.push_back(temp);

	for (int i = 0; i < vertexes.size(); i++) {
	  Vector3d tempCurr = elCurrentNodePositions[vertexes[i].first] - elCurrentNodePositions[vertexes[i].second];
	  Vector3d tempRef  = elReferenceNodePositions[vertexes[i].first] - elReferenceNodePositions[vertexes[i].second];
	  currentEdgeVectors.push_back(tempCurr);
	  referenceEdgeVectors.push_back(tempRef);
 	}


        if (R.getRequest() & ENERGY) {
          double tempEnergy = 0.0;

          for (int edge_iter = 0; edge_iter < currentEdgeVectors.size(); edge_iter++) {
	    // Because edges get counted twice, we need to count midside nodes twice by multiplying x2
	    double tempMembraneStiffnessCoefficient = (edge_iter < 6) ? _membraneStiffnessCoefficient : _membraneStiffnessCoefficient * 2.0;
 	    // cout << "Energy: " << tempMembraneStiffnessCoefficient/2.0 * pow(currentEdgeVectors[edge_iter].norm() - referenceEdgeVectors[edge_iter].norm(), 2.0) << "\t" << currentEdgeVectors[edge_iter].norm() << "\t" << referenceEdgeVectors[edge_iter].norm() << endl;
            tempEnergy += tempMembraneStiffnessCoefficient/2.0 * pow(currentEdgeVectors[edge_iter].norm() - referenceEdgeVectors[edge_iter].norm(), 2.0);
	  }
          R.addEnergy(tempEnergy);
        } // End ENERGY

        if (R.getRequest() & FORCE) {
          for (int i = 0; i < _nodeDoF; i++) {
            for (int edge_iter = 0; edge_iter < currentEdgeVectors.size(); edge_iter++) {
	      // Because edges get counted twice, we need to count midside nodes twice by multiplying x2
	      double tempMembraneStiffnessCoefficient = (edge_iter < 6) ? _membraneStiffnessCoefficient : _membraneStiffnessCoefficient * 2.0;
              int nodeID1 = elNodeIds[vertexes[edge_iter].first];
              int nodeID2 = elNodeIds[vertexes[edge_iter].second];
              R.addResidual(numMainNodes * _nodeDoF + nodeID1 * _nodeDoF + i,  1.0 * tempMembraneStiffnessCoefficient * (currentEdgeVectors[edge_iter](i) - referenceEdgeVectors[edge_iter].norm() * currentEdgeVectors[edge_iter](i) / currentEdgeVectors[edge_iter].norm()));
              R.addResidual(numMainNodes * _nodeDoF + nodeID2 * _nodeDoF + i, -1.0 * tempMembraneStiffnessCoefficient * (currentEdgeVectors[edge_iter](i) - referenceEdgeVectors[edge_iter].norm() * currentEdgeVectors[edge_iter](i) / currentEdgeVectors[edge_iter].norm()));
            }
          }
        }
        if (R.getRequest() & STIFFNESS) {
          for (int i = 0; i < _nodeDoF; i++) {
            for (int j = 0; j < _nodeDoF; j++) {
              for (int edge_iter = 0; edge_iter < currentEdgeVectors.size(); edge_iter++) {
		// Because edges get counted twice, we need to count midside nodes twice by multiplying x2
		double tempMembraneStiffnessCoefficient = (edge_iter < 6) ? _membraneStiffnessCoefficient : _membraneStiffnessCoefficient * 2.0;
                int nodeID1 = elNodeIds[vertexes[edge_iter].first];
                int nodeID2 = elNodeIds[vertexes[edge_iter].second];

                KtripletList_FromFlexibleBoundary.push_back(Triplet<Real >( numMainNodes * _nodeDoF + nodeID1 * _nodeDoF + i, numMainNodes * _nodeDoF + nodeID1 * _nodeDoF + j, tempMembraneStiffnessCoefficient * (identityMat(i,j) - referenceEdgeVectors[edge_iter].norm() * (identityMat(i,j) / currentEdgeVectors[edge_iter].norm() - currentEdgeVectors[edge_iter](i) * currentEdgeVectors[edge_iter](j)/ pow(currentEdgeVectors[edge_iter].norm(),3)))));

                KtripletList_FromFlexibleBoundary.push_back(Triplet<Real >( numMainNodes * _nodeDoF + nodeID2 * _nodeDoF + i, numMainNodes * _nodeDoF + nodeID2 * _nodeDoF + j, tempMembraneStiffnessCoefficient * (identityMat(i,j) - referenceEdgeVectors[edge_iter].norm() * (identityMat(i,j) / currentEdgeVectors[edge_iter].norm() - currentEdgeVectors[edge_iter](i) * currentEdgeVectors[edge_iter](j)/ pow(currentEdgeVectors[edge_iter].norm(),3)))));

                KtripletList_FromFlexibleBoundary.push_back(Triplet<Real >( numMainNodes * _nodeDoF + nodeID1 * _nodeDoF + i, numMainNodes * _nodeDoF + nodeID2 * _nodeDoF + j, -1.0 * tempMembraneStiffnessCoefficient * (identityMat(i,j) - referenceEdgeVectors[edge_iter].norm() * (identityMat(i,j) / currentEdgeVectors[edge_iter].norm() - currentEdgeVectors[edge_iter](i) * currentEdgeVectors[edge_iter](j)/ pow(currentEdgeVectors[edge_iter].norm(),3)))));

                KtripletList_FromFlexibleBoundary.push_back(Triplet<Real >( numMainNodes * _nodeDoF + nodeID2 * _nodeDoF + i, numMainNodes * _nodeDoF + nodeID1 * _nodeDoF + j, -1.0 * tempMembraneStiffnessCoefficient * (identityMat(i,j) - referenceEdgeVectors[edge_iter].norm() * (identityMat(i,j) / currentEdgeVectors[edge_iter].norm() - currentEdgeVectors[edge_iter](i) * currentEdgeVectors[edge_iter](j)/ pow(currentEdgeVectors[edge_iter].norm(),3)))));

              }
            }
          }
        }
      }
      else { // Neither Linear or Quadratic Tri
	cout << "ERROR: Not sure what element to impose for the Flexible Membrane." << endl;
	exit(EXIT_FAILURE);
      }
    }
    return KtripletList_FromFlexibleBoundary;
  } // End imposeFlexibleMembrane

  void MechanicsModel::computeLJNormals() {
    const vector<GeomElement* > elements = _rigidPotentialBoundaryMesh->getElements();
    vector<Vector3d > ElNormals(elements.size(), Vector3d::Zero()); 
    int dim = _rigidPotentialBoundaryMesh->getDimension();
    int numMainNodes = _myMesh->getNumberOfNodes();
    
    // Loop over elements
    for(int e = 0; e < elements.size(); e++)
    {
      GeomElement* geomEl = elements[e];
      const vector<int  >& NodesID = geomEl->getNodesID();
      const int numQP    = geomEl->getNumberOfQuadPoints();
      const int numNodes = NodesID.size();

      // Loop over quadrature points
      for(int q = 0; q < numQP; q++) {

        // Compute normal based on _prevField
        Vector3d a1 = Vector3d::Zero(), a2 = Vector3d::Zero(), a3 = Vector3d::Zero();
        for (int a = 0; a < NodesID.size(); a++) {
          int nodeID = NodesID[a];
          Vector3d nodalPos;
	  if (_makeFlexible)
	    nodalPos << _field[numMainNodes * dim + nodeID * dim + 0], _field[numMainNodes * dim + nodeID * dim + 1], _field[numMainNodes * dim + nodeID * dim + 2];
	  else
            nodalPos << _rigidPotentialBoundaryMesh->getX(nodeID)(0), _rigidPotentialBoundaryMesh->getX(nodeID)(1), _rigidPotentialBoundaryMesh->getX(nodeID)(2);
          a1 += nodalPos*geomEl->getDN(q, a, 0);
          a2 += nodalPos*geomEl->getDN(q, a, 1);
        }
        ElNormals[e] += a1.cross(a2); // Not normalized with respect to area (elements with larger area count more)
      } // loop over QP

    } // loop over elements

    // Loop over rigidBoundary nodes
    for (int n = 0; n < _rigidPotentialBoundaryMesh->getNumberOfNodes(); n++) {
      // Reset normal to zero
      _rigidSurfaceNormals[n] = Vector3d::Zero();
      // Loop over all elements sharing that node
      for (int m = 0; m < _LJNodesToEle[n].size(); m++) {
        _rigidSurfaceNormals[n] += ElNormals[_LJNodesToEle[n][m]];
      }
      Real normFactor = 1.0/_rigidSurfaceNormals[n].norm();
      _rigidSurfaceNormals[n] *= normFactor;

      // For testing only - to be commented out
      // cout << _spNormals[n](0) << " " << _spNormals[n](1) << " " << _spNormals[n](2) << endl;
    }
  }

  void MechanicsModel::findNearestRigidNeighbors() {
    // Loop through every surface node
    int numSurfaceNodes = _bodyPotentialBoundaryNodes.size();
    int dim = _rigidPotentialBoundaryMesh->getDimension();
    int numMainNodes = _myMesh->getNumberOfNodes();
    for(int n = 0; n < numSurfaceNodes; n++)
    {
      _rigidNeighbors[n].clear();
      int NodeID = _bodyPotentialBoundaryNodes[n];
      Vector3d xa_body, xa_rigid;
      xa_body << _field[NodeID*3 + 0],  _field[NodeID*3 + 1],  _field[NodeID*3 + 2];

      vector <pair<double, int> > distanceAndIndex;
      for (int node_rigid = 0; node_rigid < _rigidPotentialBoundaryMesh->getNumberOfNodes(); node_rigid++) {
	if (_makeFlexible)
	  xa_rigid << _field[numMainNodes * dim + node_rigid * dim + 0], _field[numMainNodes * dim + node_rigid * dim + 1], _field[numMainNodes * dim + node_rigid * dim + 2];
 	else
          xa_rigid << _rigidPotentialBoundaryMesh->getX(node_rigid)(0), _rigidPotentialBoundaryMesh->getX(node_rigid)(1), _rigidPotentialBoundaryMesh->getX(node_rigid)(2);
        
        // Check if within search radius
        Vector3d gapVector = xa_rigid - xa_body;
        double gapDistance = gapVector.norm();
        Real r_e = gapVector.dot(_rigidSurfaceNormals[node_rigid]);

	if (gapDistance <= _searchRadius) {
	  _rigidNeighbors[n].push_back(node_rigid);
	  if (_useNumberNeighborFlag) distanceAndIndex.push_back(make_pair(gapDistance, node_rigid));
	}
      } // loop through all rigid nodes

      // If we only want a n number of neighbors
      if (_useNumberNeighborFlag) {
	sort(distanceAndIndex.begin(), distanceAndIndex.end());
	_rigidNeighbors[n].clear();
	for (int addNumNeighbors = 0; addNumNeighbors < _numberNearestNeighborsToUse; addNumNeighbors++) {
	  if (addNumNeighbors < distanceAndIndex.size())
	    _rigidNeighbors[n].push_back(distanceAndIndex[addNumNeighbors].second);
	}
      }
    } // loop through all surface nodes
    cout << "Found Nearest Neighbors" << endl;
  } // end findNearestRigidNeighbors

  void MechanicsModel::recomputeAverageMinDistance() {
    int dim = _rigidPotentialBoundaryMesh->getDimension();
    int numMainNodes = _myMesh->getNumberOfNodes();
    // If all nodes should have the same minimum distance, then loop through each epicardium node and calculate the normal distance to the rigid surface and average
    if (_useConstantMinimumDistance) {
      double averageDistance = 0.0;
      int counter = 0;
      Vector3d xa_body, xa_rigid;
      for (int n = 0; n < _rigidNeighbors.size(); n++) {
        int bodyNodeID = _bodyPotentialBoundaryNodes[n];
        xa_body << _field[bodyNodeID * 3 + 0], _field[bodyNodeID * 3 + 1], _field[bodyNodeID * 3 + 2];
        for (int r = 0; r < _rigidNeighbors[n].size(); r++) {
	  if (_makeFlexible)
            xa_rigid << _field[numMainNodes * dim + _rigidNeighbors[n][r] * dim + 0], _field[numMainNodes * dim + _rigidNeighbors[n][r] * dim + 1], _field[numMainNodes * dim + _rigidNeighbors[n][r] * dim + 2];
	  else
	    xa_rigid << _rigidPotentialBoundaryMesh->getX(_rigidNeighbors[n][r])(0), _rigidPotentialBoundaryMesh->getX(_rigidNeighbors[n][r])(1), _rigidPotentialBoundaryMesh->getX(_rigidNeighbors[n][r])(2);
	  Vector3d gapVector = xa_rigid - xa_body;
          Real r_e = gapVector.dot(_rigidSurfaceNormals[_rigidNeighbors[n][r]]);
	  averageDistance += abs(r_e);
	  counter++;
        }
      }
      averageDistance /= counter;
      cout << "Resetting every rigid node BC Minimum Distance to " << averageDistance << endl;
      _minDistance = averageDistance;
      _minDistancePerRigidNode.clear();
      _minDistancePerRigidNode.resize(_rigidPotentialBoundaryMesh->getNumberOfNodes(), _minDistance);
    }
    else { // Else - if every rigid node has a different minimum distance
      double totalAverage = 0.0;
      Vector3d xa_body, xa_rigid;
      int numSurfaceNodes = _bodyPotentialBoundaryNodes.size();
      // cout << "Number of Rigid body nodes: " << _rigidPotentialBoundaryMesh->getNumberOfNodes() << endl;
      // cout << "Vec 1st element " << _minDistancePerRigidNode[0] << endl;
      // cout << "Size of minDistance Vector: " << _minDistancePerRigidNode.size() << endl; 
      for (int rigid_node_iter = 0; rigid_node_iter < _rigidPotentialBoundaryMesh->getNumberOfNodes(); rigid_node_iter++) {
	if (_makeFlexible)
          xa_rigid << _field[numMainNodes * dim + rigid_node_iter * dim + 0], _field[numMainNodes * dim + rigid_node_iter * dim + 1], _field[numMainNodes * dim + rigid_node_iter * dim + 2];
	else
	  xa_rigid << _rigidPotentialBoundaryMesh->getX(rigid_node_iter)(0), _rigidPotentialBoundaryMesh->getX(rigid_node_iter)(1), _rigidPotentialBoundaryMesh->getX(rigid_node_iter)(2);
        double averageDistance = 0.0;
        int counter = 0;
        for (int body_node_iter = 0; body_node_iter < numSurfaceNodes; body_node_iter++) {
	  int bodyNodeID = _bodyPotentialBoundaryNodes[body_node_iter];
	  xa_body << _field[bodyNodeID * 3 + 0], _field[bodyNodeID * 3 + 1], _field[bodyNodeID * 3 + 2];

          // Check if within search radius
          Vector3d gapVector = xa_rigid - xa_body;
          double gapDistance = gapVector.norm();
          
          if (gapDistance <= _searchRadius) {
	    averageDistance += gapVector.dot(_rigidSurfaceNormals[rigid_node_iter]);
	    counter++;
          }
        }
	if (counter > 0) {
	  averageDistance /= counter;
          _minDistancePerRigidNode[rigid_node_iter] = averageDistance;
        }
	totalAverage += _minDistancePerRigidNode[rigid_node_iter];
      }
      totalAverage /= _rigidPotentialBoundaryMesh->getNumberOfNodes();
      cout << "Resetting each rigid node BC's Minimum Distance. Average turns out to be " << totalAverage << endl;
    }
  } // end recomputeAverageMinDistance


  // Writing output
  void MechanicsModel::writeOutputVTK(const string OutputFile, int step)
  {
    /////
    // Todo: NEED TO BE REWRITTEN TAKING INTO ACCOUNT MULTIPLE QUADRATURE POINTS PER ELEMENT !!!
    /////

    // Rewrite it with VTK Libraries
    // Create outputFile name
    string outputFileName = OutputFile + boost::lexical_cast<string>(step) + ".vtu";
    vtkSmartPointer<vtkUnstructuredGrid> newUnstructuredGrid = vtkSmartPointer<vtkUnstructuredGrid>::New();

    // Insert Points:
    int NumNodes = _myMesh->getNumberOfNodes();
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    // points->SetNumberOfPoints(NumNodes);
    for (int i = 0; i < NumNodes; i++) {
      float x_point = 0.0; float y_point = 0.0; float z_point = 0.0;
      x_point = _myMesh->getX(i)(0);
      if (_myMesh->getDimension() > 1) y_point = _myMesh->getX(i)(1);
      if (_myMesh->getDimension() > 2) z_point = _myMesh->getX(i)(2);
      points->InsertNextPoint(x_point, y_point, z_point);
      // points->InsertPoint(i, x_point, y_point, z_point);
    }
    newUnstructuredGrid->SetPoints(points);
    
    // Element Connectivity:
    // To-do: Figure out how to handle mixed meshes
    // To-do: It would be better to select based on Abaqus element names
    vector <GeomElement*> elements = _myMesh->getElements();
    int NumEl = elements.size();
    int NodePerEl = (elements[0])->getNodesPerElement();
    int dim = _myMesh->getDimension();

    VTKCellType cellType;
    // Set Cell Type: http://www.vtk.org/doc/nightly/html/vtkCellType_8h.html
    switch (dim) {
      case 3: // 3D
	switch (NodePerEl) {
	  case 4: // Linear Tetrahedron
	    cellType = VTK_TETRA;
	    break;
	  case 10: // Quadratic Tetrahedron
	    cellType = VTK_QUADRATIC_TETRA;
	    break;
	  default:
	    cout << "3D Element type not implemented in MechanicsModel writeOutput." << endl;
	    exit(EXIT_FAILURE);
	}
	break;
      default:
        cout << "This element has not been implemented in MechanicsModel writeOutput." << endl;
	exit(EXIT_FAILURE);
    }

    for (int el_iter = 0; el_iter < NumEl; el_iter++) {
      vtkSmartPointer<vtkIdList> elConnectivity = vtkSmartPointer<vtkIdList>::New();

      const vector<int > & NodesID = (elements[el_iter])->getNodesID();
      for (int n = 0; n < NodePerEl; n++) {
        elConnectivity->InsertNextId(NodesID[n]);
      }
      newUnstructuredGrid->InsertNextCell(cellType, elConnectivity);
    }

    // ** BEGIN: POINT DATA ** //
    // ~~ BEGIN: DISPLACEMENTS ~~ //
    vtkSmartPointer<vtkDoubleArray> displacements = vtkSmartPointer<vtkDoubleArray>::New();
    displacements->SetNumberOfComponents(dim);
    displacements->SetName("displacement");
    displacements->SetComponentName(0, "X");
    displacements->SetComponentName(1, "Y");
    if (dim > 2) displacements->SetComponentName(2, "Z");

    for (int i = 0; i < NumNodes; i++ ) {
      double x[dim];
      VectorXd X = _myMesh->getX(i);
      for (int j = 0; j < dim; j++) {
        x[j] = _field[i*dim + j] - X(j);
      }
      displacements->InsertNextTuple(x);
    }
    newUnstructuredGrid->GetPointData()->AddArray(displacements);
    // ~~ END: DISPLACEMENTS ~~ //
    
    // ~~ BEGIN: RESIDUALS ~~ //
    vtkSmartPointer<vtkDoubleArray> residuals = vtkSmartPointer<vtkDoubleArray>::New();
    residuals->SetNumberOfComponents(dim);
    residuals->SetName("residual");
    residuals->SetComponentName(0, "X"); residuals->SetComponentName(1, "Y");
    if (dim > 2) residuals->SetComponentName(2, "Z");

    // Compute Residual
    uint PbDoF = ( _myMesh->getNumberOfNodes())*this->getDoFperNode();
    if (_makeFlexible) PbDoF += _rigidPotentialBoundaryMesh->getNumberOfNodes() * this->getDoFperNode();
    EigenResult myResults(PbDoF, 2);
    int myRequest = 2;
    myResults.setRequest(myRequest);
    this->compute(&myResults);
    VectorXd R = *(myResults._residual);

    for (int i = 0; i < NumNodes; i++ ) {
      double res[dim];
      for (int j = 0; j < dim; j++) {
        res[j] = R(i*dim + j);
      }
      residuals->InsertNextTuple(res);
    }
    newUnstructuredGrid->GetPointData()->AddArray(residuals);
    // ~~ END: RESIDUALS ~~ //
    // ** END: POINT DATA ** //
    
    // ** BEGIN: CELL DATA ** //
    // ~~ BEGIN: \alpha MATERIAL PROPERTY (MAT_PARAM_ID) ~~ //
    vtkSmartPointer<vtkDoubleArray> alpha = vtkSmartPointer<vtkDoubleArray>::New();
    alpha->SetNumberOfComponents(2);
    alpha->SetName("alpha");
    alpha->SetComponentName(0, "Alpha_1"); alpha->SetComponentName(1, "Alpha_2");
    for (int e = 0; e < NumEl; e++) {
      GeomElement* geomEl = elements[e];
      const int numQP = geomEl->getNumberOfQuadPoints();
      double alpha_arr[2];
      Real AvgMatProp_alpha1 = 0.0;
      Real AvgMatProp_alpha2 = 0.0;
      for (int q = 0; q < numQP; q++) {
        vector <Real> MatProp = _materials[e*numQP + q]->getMaterialParameters();
	if (!MatProp.empty()) {
	  if (MatProp.size() > 0) AvgMatProp_alpha1 += MatProp[0];
	  if (MatProp.size() > 1) AvgMatProp_alpha2 += MatProp[1];
        }
      }
      AvgMatProp_alpha1 /= double(numQP); AvgMatProp_alpha2 /= double(numQP);
      alpha_arr[0] = AvgMatProp_alpha1; alpha_arr[1] = AvgMatProp_alpha2;
      alpha->InsertNextTuple(alpha_arr);
    }
    newUnstructuredGrid->GetCellData()->AddArray(alpha);
    // ~~ END: \alpha MATERIAL PROPERTY (MAT_PARAM_ID) ~~ //
    
    // ~~ BEGIN: INTERNAL VARIABLES ~~ //
    // TODO: This method assumes the same material throughout the entire body
    int numInternalVariables = (_materials[0]->getInternalParameters()).size();
    if (numInternalVariables > 0) {
      vtkSmartPointer <vtkDoubleArray> internalVariables = vtkSmartPointer<vtkDoubleArray>::New();
      internalVariables->SetName("Material_Internal_Variables");
      internalVariables->SetNumberOfComponents(numInternalVariables);
      for (int i = 0; i < numInternalVariables; i++) {
        string tempName = "Internal_Variable_" +  boost::lexical_cast<string>(i);
        internalVariables->SetComponentName(i, tempName.c_str());
      }
      for (int e = 0; e < NumEl; e++) {
        GeomElement* geomEl = elements[e];
        const int numQP = geomEl->getNumberOfQuadPoints();
        vector<Real> IntProp = _materials[e*numQP]->getInternalParameters();
        if (!IntProp.size() == numInternalVariables) {
	  cout << "Internal Variables output for multi-materials not supported yet." << endl;
          // double* tempIntProp = new double[numInternalVariables]();
          double tempIntProp[numInternalVariables];
          for (int p = 0; p < numInternalVariables; p++) tempIntProp[p] = numeric_limits<double>::quiet_NaN(); // Some error value. NaN is better.
	  internalVariables->InsertNextTuple(tempIntProp);
	  // delete tempIntProp;
	  continue;
        }

        fill(IntProp.begin(), IntProp.end(), 0.0);

        // Get Internal Properties from each quad point.
        for (int q = 0; q < numQP; q++) {
          vector <Real> IntPropQuad = _materials[e*numQP + q]->getInternalParameters();
          for (int p = 0; p < numInternalVariables; p++) IntProp[p] = IntProp[p] + IntPropQuad[p];
        }
        // Average over quad points for cell data.
        // double* intPropArr = new double(numInternalVariables);
        double intPropArr[numInternalVariables];
        for (int i = 0; i < numInternalVariables; i++) 
  	  intPropArr[i] = IntProp[i]/numQP;
        internalVariables->InsertNextTuple(intPropArr);
	// delete [] intPropArr;
      }
      newUnstructuredGrid->GetCellData()->AddArray(internalVariables);
    }
    // ~~ END: INTERNAL VARIABLES ~~ //
  
    // ~~ BEGIN: 1ST PIOLA-KIRCHHOFF STRESS ~~ //
    // Loop through elements, also through material points array, which is unrolled
    // uint index = 0;
    
    vtkSmartPointer <vtkDoubleArray> FirstPKStress = vtkSmartPointer<vtkDoubleArray>::New();
    FirstPKStress->SetName("First_PK_Stress");
    FirstPKStress->SetNumberOfComponents(dim*dim);
    for (int i = 0; i < dim; i++) { // row
      for (int j = 0; j < dim; j++) {
        string tempCompName = "P" + boost::lexical_cast<string>(i+1) + boost::lexical_cast<string>(j+1);
        FirstPKStress->SetComponentName(i*3 + j, tempCompName.c_str());
      }
    }

    MechanicsMaterial::FKresults FKres;
    FKres.request = 2;
    for(int e = 0; e < NumEl; e++)
    {
      // The dynamic allocation results in a memory leak + Seg fault. Not great.
      // double* eleFirstPKStress = new (nothrow) double(dim*dim);
      double eleFirstPKStress[dim * dim];
      for (int i = 0; i < dim; i++)
        for (int j = 0; j < dim; j++)
          eleFirstPKStress[i*dim + j] = 0.0;

      // double eleFirstPKStress[9] = {0.0};;
      GeomElement* geomEl = elements[e];
      const int numQP = geomEl->getNumberOfQuadPoints();
      
      // F at each quadrature point are computed at the same time in one element
      vector<Matrix3d > Flist(numQP, Matrix3d::Zero());
      // Compute deformation gradients for current element
      this->computeDeformationGradient(Flist, geomEl);

      // Loop over quadrature Points
      // for (int q = 0; q < numQP; q++) {
      //   _materials[e]->compute(FKres, Flist[q], &Fiber);
      /*
      WARNING
      THIS ONLY WORKS WITH 1QP PER ELEMENT - BAD - NEEDS TO BE CHANGED!!
      */
      // }
      // Read through this on how to visualize data at integration points:
      // http://www.vtk.org/Wiki/VTK/VTK_integration_point_support

      _materials[e*numQP + 0]->compute(FKres, Flist[0]);
      
      for (int i = 0; i < dim; i++)
	for (int j = 0; j < dim; j++) 
	  eleFirstPKStress[i*3 + j] = FKres.P(i,j);
      
      FirstPKStress->InsertNextTuple(eleFirstPKStress);
      // delete eleFirstPKStress;
    }
    newUnstructuredGrid->GetCellData()->AddArray(FirstPKStress);
    // ~~ END: 1ST PIOLA-KIRCHHOFF STRESS ~~ //
    
    
    // ~~ START: COMPUTE FIBER AND CIRCUMFERENTIAL STRAINS ~~ //
    vtkSmartPointer <vtkDoubleArray> GreenStrains = vtkSmartPointer<vtkDoubleArray>::New();
    GreenStrains->SetName("GreenStrains");
    GreenStrains->SetNumberOfComponents(4);
    GreenStrains->SetComponentName(0, "FiberDirection");
    GreenStrains->SetComponentName(1, "RadialDirection");
    GreenStrains->SetComponentName(2, "CircumferentialDirection");
    GreenStrains->SetComponentName(3, "LongitudinalDirection");

    // Compute Radial Vector
    vtkSmartPointer<vtkDoubleArray> radialVecs = vtkSmartPointer<vtkDoubleArray>::New();
    radialVecs->SetNumberOfComponents(3);
    radialVecs->SetName("RadialVector");

    // Compute Centroid
    Vector3d centroidLoc(3); centroidLoc << 0.0, 0.0, 0.0;
    for (int i = 0; i < NumNodes; i++ ) {
      centroidLoc = centroidLoc + _myMesh->getX(i);
    }
    centroidLoc[0] = centroidLoc[0]/NumNodes; centroidLoc[1] = centroidLoc[1]/NumNodes; centroidLoc[2] = centroidLoc[2]/NumNodes;

    for (int e = 0; e < NumEl; e++) {
      // FIX THIS: ONLY WORKS FOR ONE QUADRATURE POINT
      // FOR NOW: q = 0 (first quadrature point
      int q = 0;
      // Setup an empty array to hold all Green strains
      double eleGreenStrains[4] = {0.0, 0.0, 0.0, 0.0};

      GeomElement* geomEl = elements[e];
      const int numQP = geomEl->getNumberOfQuadPoints();
      vector<Matrix3d> Elist(numQP, Matrix3d::Zero());
      this->computeGreenLagrangianStrainTensor(Elist, geomEl);
      
      vector<Vector3d> dirVecs = _materials[e * numQP + q]->getDirectionVectors();

      // Fiber Strain:
      eleGreenStrains[0] = dirVecs[0].transpose() * Elist[q] * dirVecs[0];

      // Compute the Radial/Tangential/Longitudinal vectors
      vector<Vector3d> cylindricalVectors(3, Vector3d::Zero());

      // Compute location of quadrature point:
      Vector3d quadPointLocation(3); quadPointLocation << 0.0, 0.0, 0.0;
      const vector<int>& NodesID = geomEl->getNodesID();
      const uint numNodesOfEl = NodesID.size();

      for (int n = 0; n < numNodesOfEl; n++) {
        for (int d = 0; d < dim; d++) {
          quadPointLocation[d] += _myMesh->getX(NodesID[n])(d) * geomEl->getN(q,n);
        }
      }
      // Radial Vec:
      cylindricalVectors[0] = quadPointLocation - centroidLoc;
      cylindricalVectors[0](2) = 0.0;
      // Normalize:
      cylindricalVectors[0] = cylindricalVectors[0]/cylindricalVectors[0].norm();
      eleGreenStrains[1] = cylindricalVectors[0].transpose() * Elist[q] * cylindricalVectors[0];
     
      double tempRadialVec[3] = {cylindricalVectors[0](0), cylindricalVectors[0](1), cylindricalVectors[0](2)};
      radialVecs->InsertNextTuple(tempRadialVec);


      // Circumferential Vec:
      cylindricalVectors[1](0) = -1 * cylindricalVectors[0](1);
      cylindricalVectors[1](1) = cylindricalVectors[0](0);
      cylindricalVectors[1](2) = 0.0;
      cylindricalVectors[1] = cylindricalVectors[1]/cylindricalVectors[1].norm();
      eleGreenStrains[2] = cylindricalVectors[1].transpose() * Elist[q] * cylindricalVectors[1];
      
      // Longitudinal Vec:
      cylindricalVectors[2] = cylindricalVectors[0].cross(cylindricalVectors[1]);
      cylindricalVectors[2] = cylindricalVectors[2]/cylindricalVectors[2].norm();
      eleGreenStrains[3] = cylindricalVectors[2].transpose() * Elist[q] * cylindricalVectors[2];

      GreenStrains->InsertNextTuple(eleGreenStrains);
    }
    newUnstructuredGrid->GetCellData()->AddArray(GreenStrains);
    newUnstructuredGrid->GetCellData()->AddArray(radialVecs);
    // ~~ END  : COMPUTE FIBER AND CIRCUMFERENTIAL STRAINS ~~ //
    // ** END: CELL DATA ** //

    // Write file
    vtkSmartPointer<vtkXMLUnstructuredGridWriter> writer = vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
    writer->SetFileName(outputFileName.c_str());
    writer->SetInput(newUnstructuredGrid);
    writer->Write();
    
    // ** BEGIN: SUPPORT FOR INTEGRATION POINTS ** //
    // ~~ BEGIN: PLOT PRESSURE NORMALS ~~ //
    if (_pressureFlag) 
      writePressurePolyData(OutputFile, step);
    // ~~ END: PLOT PRESSURE NORMALS ~~ //
    if (_springBCflag)
      writeLinearSpringPolyData(OutputFile, step);
    if (_torsionalSpringBCflag)
      writeTorsionalSpringPolyData(OutputFile, step);
    if (_lennardJonesBCFlag) {
       writeLJBCPolyData(OutputFile, step);
       if (_makeFlexible)
	  writeFlexibleMembraneBCPolyData(OutputFile, step);
    }
  } // writeOutput

  void MechanicsModel::writePressurePolyData(string OutputFile, int step) {

    int dim = _myMesh->getDimension();

    string outputIntegrationPointDataFileName = OutputFile + "_IntPointData" + boost::lexical_cast<string>(step) + ".vtp";
    vtkSmartPointer<vtkPolyData> IntegrationPointGrid = vtkSmartPointer<vtkPolyData>::New();
    vtkSmartPointer<vtkPoints> IntegrationPoints = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkDoubleArray> IntegrationPointsDisplacements = vtkSmartPointer<vtkDoubleArray>::New();
    IntegrationPointsDisplacements->SetNumberOfComponents(3);
    IntegrationPointsDisplacements->SetName("Displacements");

    vector <GeomElement*> elements2D = _surfaceMesh->getElements();
    for (int e = 0; e < elements2D.size(); e++) {
      GeomElement* geomEl = elements2D[e];
      const int numQP = geomEl->getNumberOfQuadPoints();
      const vector<int>& NodesID = geomEl->getNodesID();
      const uint numNodesOfEl = NodesID.size();

      for (int q = 0; q < numQP; q++) {
	float tempDisplacement[3] = {0.0}; float tempPoint[3] = {0.0};
	for (int d = 0; d < dim; d++) {
	  for (int n = 0; n < numNodesOfEl; n++) {
	    tempPoint[d] += _surfaceMesh->getX(NodesID[n])(d) * geomEl->getN(q,n);
	    tempDisplacement[d] += (_field[NodesID[n]*dim + d] - _surfaceMesh->getX(n)(d)) * geomEl->getN(q, n);
	  }
	}
	IntegrationPoints->InsertNextPoint(tempPoint);
	IntegrationPointsDisplacements->InsertNextTuple(tempDisplacement);
      }
    }
    IntegrationPointGrid->SetPoints(IntegrationPoints);
    IntegrationPointGrid->GetPointData()->AddArray(IntegrationPointsDisplacements);


    // Compute Normals:
    vtkSmartPointer<vtkDoubleArray> pressureNormal = vtkSmartPointer<vtkDoubleArray>::New();
    pressureNormal->SetNumberOfComponents(3);
    pressureNormal->SetName("Pressure_Normals");
    for(int e = 0; e < elements2D.size(); e++) {
      GeomElement* geomEl = elements2D[e];
      const vector<int  >& NodesID = geomEl->getNodesID();
      const int numQP    = geomEl->getNumberOfQuadPoints();
      const int numNodes = NodesID.size();
	
      // Loop over quadrature points
      for(int q = 0; q < numQP; q++) {
	// Compute normal based on _prevField and displacement
	Vector3d a1 = Vector3d::Zero(), a2 = Vector3d::Zero(), a3 = Vector3d::Zero();
	for (int a = 0; a < NodesID.size(); a++) {
	  int nodeID = NodesID[a];
	  Vector3d xa_prev, xa_curr;
	  xa_prev << _prevField[nodeID*3], _prevField[nodeID*3+1], _prevField[nodeID*3+2];
	  xa_curr << _field[nodeID*3], _field[nodeID*3+1], _field[nodeID*3+2];
	  a1 += xa_prev*geomEl->getDN(q, a, 0);
	  a2 += xa_prev*geomEl->getDN(q, a, 1);
	}
	a3 = a1.cross(a2);
	  
	// Surface associated with QP q
	Real Area = a3.norm();
	a3 /= Area;
	double tempNormal[3] = {a3[0], a3[1], a3[2]};
	pressureNormal->InsertNextTuple(tempNormal);
      }
    }
    IntegrationPointGrid->GetPointData()->AddArray(pressureNormal);

    // Write File
    vtkSmartPointer<vtkXMLPolyDataWriter> IntegrationPointWriter = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
    IntegrationPointWriter->SetFileName(outputIntegrationPointDataFileName.c_str());
    IntegrationPointWriter->SetInput(IntegrationPointGrid);
    IntegrationPointWriter->Write();
    // ** END: SUPPORT FOR INTEGRATION POINTS ** //
  }

  void MechanicsModel::writeLinearSpringPolyData(string OutputFile, int step) {
    int dim = _myMesh->getDimension();

    string outputIntegrationPointDataFileName = OutputFile + "_LinearSpring" + boost::lexical_cast<string>(step) + ".vtp";
    vtkSmartPointer<vtkPolyData> IntegrationPointGrid = vtkSmartPointer<vtkPolyData>::New();
    vtkSmartPointer<vtkPoints> IntegrationPoints = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkDoubleArray> IntegrationPointsDisplacements = vtkSmartPointer<vtkDoubleArray>::New();
    IntegrationPointsDisplacements->SetNumberOfComponents(3);
    IntegrationPointsDisplacements->SetName("Displacements");

    vtkSmartPointer<vtkDoubleArray> LinearSpringForces = vtkSmartPointer<vtkDoubleArray>::New();
    LinearSpringForces->SetNumberOfComponents(3);
    LinearSpringForces->SetName("LinearSpring_Force");
  
    uint PbDoF = ( _myMesh->getNumberOfNodes())*this->getDoFperNode();
    if (_makeFlexible) PbDoF += _rigidPotentialBoundaryMesh->getNumberOfNodes() * this->getDoFperNode();
    EigenResult myResults(PbDoF, 0);
    myResults.resetResidualToZero();
    ComputeRequest myRequest = FORCE;

    myResults.setRequest(myRequest);
    this->applySpringBC(myResults);
    VectorXd R = *(myResults._residual);

    vector <GeomElement*> elements2D = _spMesh->getElements();
    double springForceMagnitude = 0.0;
    for (int n = 0; n < _spNodes.size(); n++) {
      float tempDisplacement[3] = {0.0}; float tempPoint[3] = {0.0};

      // Compute Spring Force also
      float tempSpringForce[3] = {0.0};  double tempSpringForceMagnitude = 0.0;

      for (int d = 0; d < dim; d++) {
        tempPoint[d] = _spMesh->getX(n)(d);
	tempDisplacement[d] = (_field[_spNodes[n]*dim + d] - _spMesh->getX(n)(d));
        tempSpringForce[d] =  R(_spNodes[n]*dim + d);
        tempSpringForceMagnitude = tempSpringForceMagnitude + pow(tempSpringForce[d],2);
      }
      springForceMagnitude = springForceMagnitude + sqrt(tempSpringForceMagnitude);
      // cout << "Spring Force:\t" << tempSpringForce[0] << "\t" << tempSpringForce[1] << "\t" << tempSpringForce[2] << endl;
      IntegrationPoints->InsertNextPoint(tempPoint);
      IntegrationPointsDisplacements->InsertNextTuple(tempDisplacement);
      LinearSpringForces->InsertNextTuple(tempSpringForce);
    }

    cout << "Spring Reaction Force: " << springForceMagnitude << endl;
    
    IntegrationPointGrid->SetPoints(IntegrationPoints);
    IntegrationPointGrid->GetPointData()->AddArray(IntegrationPointsDisplacements);
    IntegrationPointGrid->GetPointData()->AddArray(LinearSpringForces);

    // Compute Normals:
    vtkSmartPointer<vtkDoubleArray> spNormal = vtkSmartPointer<vtkDoubleArray>::New();
    spNormal->SetNumberOfComponents(3);
    spNormal->SetName("LinearSpring_Normals");

    for (int n = 0; n < _spNormals.size(); n++) {
      double tempSpNormal[3] = {_spNormals[n](0), _spNormals[n](1), _spNormals[n](2)};
      spNormal->InsertNextTuple(tempSpNormal);
    }

    IntegrationPointGrid->GetPointData()->AddArray(spNormal);

    // Write File
    vtkSmartPointer<vtkXMLPolyDataWriter> IntegrationPointWriter = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
    IntegrationPointWriter->SetFileName(outputIntegrationPointDataFileName.c_str());
    IntegrationPointWriter->SetInput(IntegrationPointGrid);
    IntegrationPointWriter->Write();
  }

  void MechanicsModel::writeTorsionalSpringPolyData(string OutputFile, int step) {
    int dim = _myMesh->getDimension();

    string outputIntegrationPointDataFileName = OutputFile + "_TorsionalSpring" + boost::lexical_cast<string>(step) + ".vtp";
    vtkSmartPointer<vtkPolyData> IntegrationPointGrid = vtkSmartPointer<vtkPolyData>::New();
    vtkSmartPointer<vtkPoints> IntegrationPoints = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkDoubleArray> IntegrationPointsDisplacements = vtkSmartPointer<vtkDoubleArray>::New();
    IntegrationPointsDisplacements->SetNumberOfComponents(3);
    IntegrationPointsDisplacements->SetName("Displacements");

    for (int n = 0; n < _torsionalSpringNodes.size(); n++) {
      float tempDisplacement[3] = {0.0}; float tempPoint[3] = {0.0};
      for (int d = 0; d < dim; d++) {
        tempPoint[d] = _myMesh->getX(_torsionalSpringNodes[n])(d);
        tempDisplacement[d] = (_field[_torsionalSpringNodes[n]*dim + d] - _myMesh->getX(_torsionalSpringNodes[n])(d));
      }
      IntegrationPoints->InsertNextPoint(tempPoint);
      IntegrationPointsDisplacements->InsertNextTuple(tempDisplacement);
    }

    IntegrationPointGrid->SetPoints(IntegrationPoints);
    IntegrationPointGrid->GetPointData()->AddArray(IntegrationPointsDisplacements);

    // Compute Normals:
    vtkSmartPointer<vtkDoubleArray> spNormal = vtkSmartPointer<vtkDoubleArray>::New();
    spNormal->SetNumberOfComponents(3);
    spNormal->SetName("TorsionalSpring_Normals");

    for (int n = 0; n < _spTangents.size(); n++) {
      double tempSpNormal[3] = {_spTangents[n](0), _spTangents[n](1), _spTangents[n](2)};
      spNormal->InsertNextTuple(tempSpNormal);
    }

    IntegrationPointGrid->GetPointData()->AddArray(spNormal);

    // Write File
    vtkSmartPointer<vtkXMLPolyDataWriter> IntegrationPointWriter = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
    IntegrationPointWriter->SetFileName(outputIntegrationPointDataFileName.c_str());
    IntegrationPointWriter->SetInput(IntegrationPointGrid);
    IntegrationPointWriter->Write();
  }

  void MechanicsModel::writeLJBCPolyData(string OutputFile, int step) {
    int dim = _myMesh->getDimension();

    string LJBCPointDataFileName = OutputFile + "_LJBC" + boost::lexical_cast<string>(step) + ".vtu";

    vtkSmartPointer<vtkUnstructuredGrid> NodalPointGrid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    vtkSmartPointer<vtkPoints> NodalPoints = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkDoubleArray> NodalPointsDisplacements = vtkSmartPointer<vtkDoubleArray>::New();
    NodalPointsDisplacements->SetNumberOfComponents(3);
    NodalPointsDisplacements->SetName("Displacements");

    vtkSmartPointer<vtkDoubleArray> LJForces = vtkSmartPointer<vtkDoubleArray>::New();
    LJForces->SetNumberOfComponents(3);
    LJForces->SetName("LJ_Force");

    uint PbDoF = ( _myMesh->getNumberOfNodes())*this->getDoFperNode();
    if (_makeFlexible) PbDoF += _rigidPotentialBoundaryMesh->getNumberOfNodes() * this->getDoFperNode();
    EigenResult myResults(PbDoF, 0);
    myResults.resetResidualToZero();
    ComputeRequest myRequest = FORCE;

    myResults.setRequest(myRequest);
    this->imposeLennardJones(myResults);
    VectorXd R = *(myResults._residual);

    vector <GeomElement*> elements2D = _bodyPotentialBoundaryMesh->getElements();
    for (int n = 0; n < _myMesh->getNumberOfNodes(); n++) {
      float tempDisplacement[3] = {0.0}; float tempPoint[3] = {0.0};
      float tempForce[3] = {0.0};

      for (int d = 0; d < dim; d++) {
          tempPoint[d] = _myMesh->getX(n)(d);
	  tempDisplacement[d] = (_field[n * dim + d] - _myMesh->getX(n)(d));
          tempForce[d] = R(n*dim + d);
	  // cout << n * dim + d << "\t" << tempForce[d] << endl;
       }
       NodalPoints->InsertNextPoint(tempPoint);
       NodalPointsDisplacements->InsertNextTuple(tempDisplacement);
       LJForces->InsertNextTuple(tempForce);
    }
    
    NodalPointGrid->SetPoints(NodalPoints);
    NodalPointGrid->GetPointData()->AddArray(NodalPointsDisplacements);
    NodalPointGrid->GetPointData()->AddArray(LJForces);

    for (int el_iter = 0; el_iter < elements2D.size(); el_iter++) {
      vtkSmartPointer<vtkIdList> elConnectivity = vtkSmartPointer<vtkIdList>::New();

      const vector<int > & NodesID = (elements2D[el_iter])->getNodesID();
      for (int n = 0; n < NodesID.size(); n++) {
        elConnectivity->InsertNextId(NodesID[n]);
      }
      NodalPointGrid->InsertNextCell(determineVTKCellType(2, NodesID.size()), elConnectivity);
    }

    // Write File
    vtkSmartPointer<vtkXMLUnstructuredGridWriter> NodalPointWriter = vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
    NodalPointWriter->SetFileName(LJBCPointDataFileName.c_str());
    NodalPointWriter->SetInput(NodalPointGrid);
    NodalPointWriter->Write();

    {
      string neighborsFileName = OutputFile + "_PlotRigidNeighbors" + boost::lexical_cast<string>(step) + ".vtp";
      // Create a vtkPoints object and store the points in it
      vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
      int numMainNodes = _myMesh->getNumberOfNodes();
      // Write points from the main mesh
      for (int i = 0; i < numMainNodes; i++) {
	double p[3] = {_field[i * dim + 0], _field[i * dim + 1], _field[i * dim + 2]};
	points->InsertNextPoint(p);
      }
      // Write points from the boundary condition mesh
      for (int i = 0; i < _rigidPotentialBoundaryMesh->getNumberOfNodes(); i++) {
	double p[3] = {_rigidPotentialBoundaryMesh->getX(i, 0), _rigidPotentialBoundaryMesh->getX(i,1), _rigidPotentialBoundaryMesh->getX(i,2)};
        if (_makeFlexible) {
	  p[0] = _field[numMainNodes * dim + i * dim + 0]; p[1] = _field[numMainNodes * dim + i * dim + 1]; p[2] = _field[numMainNodes * dim + i * dim + 2];
	}
	points->InsertNextPoint(p);
      }

      // Create a cell array to store the lines in and add the lines to it
      vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New();

      for (int i = 0; i < _rigidNeighbors.size(); i++) {
	for (int j = 0; j < _rigidNeighbors[i].size(); j++) {
          vtkSmartPointer<vtkPolyLine> polyLine = vtkSmartPointer<vtkPolyLine>::New();
	  polyLine->GetPointIds()->SetNumberOfIds(2);
	  polyLine->GetPointIds()->SetId(0, _bodyPotentialBoundaryNodes[i]);
	  polyLine->GetPointIds()->SetId(1, numMainNodes + _rigidNeighbors[i][j]);
	  cells->InsertNextCell(polyLine);
	}
      }
      
      // Create a polydata to store everything in
      vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
 
      // Add the points to the dataset
      polyData->SetPoints(points);
 
      // Add the lines to the dataset
      polyData->SetLines(cells);
      
      vtkSmartPointer<vtkXMLPolyDataWriter> polyDataWriter = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
      polyDataWriter->SetFileName(neighborsFileName.c_str());
      polyDataWriter->SetInput(polyData);
      polyDataWriter->Write();
    }

  }

  void MechanicsModel::writeFlexibleMembraneBCPolyData(string OutputFile, int step) {
    int dim = _rigidPotentialBoundaryMesh->getDimension();
    int numMainNodes = _myMesh->getNumberOfNodes();
    int numMembraneMeshElements = _rigidPotentialBoundaryMesh->getNumberOfElements();

    string FlexibleBCPointDataFileName = OutputFile + "_FlexibleBC" + boost::lexical_cast<string>(step) + ".vtu";

    vtkSmartPointer<vtkUnstructuredGrid> NodalPointGrid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    vtkSmartPointer<vtkPoints> NodalPoints = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkDoubleArray> NodalPointsDisplacements = vtkSmartPointer<vtkDoubleArray>::New();
    NodalPointsDisplacements->SetNumberOfComponents(3);
    NodalPointsDisplacements->SetName("Displacements");

    vector <GeomElement*> elementsBC = _rigidPotentialBoundaryMesh->getElements();
    for (int n = 0; n < _rigidPotentialBoundaryMesh->getNumberOfNodes(); n++) {
      float tempDisplacement[3] = {0.0}; float tempPoint[3] = {0.0};

      for (int d = 0; d < dim; d++) {
        tempPoint[d] = _rigidPotentialBoundaryMesh->getX(n)(d);
        tempDisplacement[d] = _field[numMainNodes * dim + n * dim + d] - tempPoint[d];
      }
      NodalPoints->InsertNextPoint(tempPoint);
      NodalPointsDisplacements->InsertNextTuple(tempDisplacement);
    }
          
    NodalPointGrid->SetPoints(NodalPoints);
    NodalPointGrid->GetPointData()->AddArray(NodalPointsDisplacements);
          
    for (int el_iter = 0; el_iter < numMembraneMeshElements; el_iter++) {
      vtkSmartPointer<vtkIdList> elConnectivity = vtkSmartPointer<vtkIdList>::New();
          
      const vector<int > & NodesID = (elementsBC[el_iter])->getNodesID();
      for (int n = 0; n < NodesID.size(); n++) {
        elConnectivity->InsertNextId(NodesID[n]);
      }
      NodalPointGrid->InsertNextCell(determineVTKCellType(2, NodesID.size()), elConnectivity);
    }

    // Compute Node Normals:
    vtkSmartPointer<vtkDoubleArray> membraneNormals = vtkSmartPointer<vtkDoubleArray>::New();
    membraneNormals->SetNumberOfComponents(3);
    membraneNormals->SetName("membraneAverageNodeNormals");
    
    for (int n = 0; n < _rigidSurfaceNormals.size(); n++) {
      double tempNormal[3] = {_rigidSurfaceNormals[n](0), _rigidSurfaceNormals[n](1), _rigidSurfaceNormals[n](2)};
      membraneNormals->InsertNextTuple(tempNormal);
    }
    NodalPointGrid->GetPointData()->AddArray(membraneNormals);
    

    // *** DELETE FROM HERE *** //
    // Computes element normals
    vtkSmartPointer<vtkDoubleArray> membraneElementNormals = vtkSmartPointer<vtkDoubleArray>::New();
    membraneElementNormals->SetNumberOfComponents(3);
    membraneElementNormals->SetName("membraneElementNormals");

    const vector<GeomElement* > elements = _rigidPotentialBoundaryMesh->getElements();
    vector<Vector3d > ElNormals(elements.size(), Vector3d::Zero());

    // Loop over elements
    for(int e = 0; e < elements.size(); e++)
    {
      GeomElement* geomEl = elements[e];
      const vector<int  >& NodesID = geomEl->getNodesID();
      const int numQP    = geomEl->getNumberOfQuadPoints();
      const int numNodes = NodesID.size();

      // Loop over quadrature points
      for(int q = 0; q < numQP; q++) {

        // Compute normal based on _prevField
        Vector3d a1 = Vector3d::Zero(), a2 = Vector3d::Zero(), a3 = Vector3d::Zero();
        for (int a = 0; a < NodesID.size(); a++) {
          int nodeID = NodesID[a];
          Vector3d nodalPos;
          if (_makeFlexible)
            nodalPos << _field[numMainNodes * dim + nodeID * dim + 0], _field[numMainNodes * dim + nodeID * dim + 1], _field[numMainNodes * dim + nodeID * dim + 2];
          else
            nodalPos << _rigidPotentialBoundaryMesh->getX(nodeID)(0), _rigidPotentialBoundaryMesh->getX(nodeID)(1), _rigidPotentialBoundaryMesh->getX(nodeID)(2);
          a1 += nodalPos*geomEl->getDN(q, a, 0);
          a2 += nodalPos*geomEl->getDN(q, a, 1);
        }
        ElNormals[e] += a1.cross(a2); // Not normalized with respect to area (elements with larger area count more)
      } // loop over QP
      double tempNormal[3] = {ElNormals[e](0), ElNormals[e](1),ElNormals[e](2)};

      membraneElementNormals->InsertNextTuple(tempNormal);


    } // loop over elements
    NodalPointGrid->GetCellData()->AddArray(membraneElementNormals);

    // *** DELETE TILL HERE *** //


    // Write File
    vtkSmartPointer<vtkXMLUnstructuredGridWriter> NodalPointWriter = vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
    NodalPointWriter->SetFileName(FlexibleBCPointDataFileName.c_str());
    NodalPointWriter->SetInput(NodalPointGrid);
    NodalPointWriter->Write();

  }

  // Helper function to determine vtkCellType
  VTKCellType MechanicsModel::determineVTKCellType(int dim, int NodePerEl) {
     VTKCellType cellType;
     // Set Cell Type: http://www.vtk.org/doc/nightly/html/vtkCellType_8h.html
     
     switch (dim) {
      case 2: // 2D
        switch (NodePerEl) {
          case 3: // Linear Triangle
            cellType = VTK_TRIANGLE;
	    break;
	  case 6: // Quadratic Triangle
	    cellType = VTK_QUADRATIC_TRIANGLE;
	    break;
          default:
	    cout << "2D Element type not implemented in MechanicsModel writeOutput." << endl;
            exit(EXIT_FAILURE);
        }
        break;
      case 3: // 3D
	switch (NodePerEl) {
	  case 4: // Linear Tetrahedron
	    cellType = VTK_TETRA;
	    break;
	  case 10: // Quadratic Tetrahedron
	    cellType = VTK_QUADRATIC_TETRA;
	    break;
	  default:
	    cout << "3D Element type not implemented in MechanicsModel writeOutput." << endl;
	    exit(EXIT_FAILURE);
	}
	break;
      default:
        cout << "This element has not been implemented in MechanicsModel writeOutput." << endl;
	exit(EXIT_FAILURE);
    }
    return cellType;
  }

} // namespace voom
